#!/usr/bin/env python3
"""Generate normalized SR1010 / ZX279133 hardware evidence inventories.

The extractor is intentionally read-only with respect to vendor artifacts.  It
combines captured .ko metadata, FIT/live device trees, runtime snapshots, and
ZX279133-specific mainline sources into CSV/JSON files beside this script.
"""

from __future__ import annotations

import csv
import hashlib
import json
import re
import shutil
import subprocess
from collections import defaultdict
from pathlib import Path
from typing import Iterable

HERE = Path(__file__).resolve().parent
VENDOR = HERE.parent
REPO = VENDOR.parent
RUNTIME = VENDOR / "sr1010-vendor-runtime"
MODULE_ROOT = RUNTIME / "modules"
KERNEL = REPO / "linux-6.18.38"

RESOURCE_PROPS = (
    "status", "compatible", "reg", "reg-names", "interrupts",
    "interrupt-names", "interrupt-parent", "clocks", "clock-names",
    "resets", "reset-names", "dmas", "dma-names", "power-domains",
    "iommus", "phys", "phy-names", "operating-points-v2",
    "interconnects", "interconnect-names", "thermal-zones", "memory-region",
    "memory-region-names", "nvmem-cells", "nvmem-cell-names",
)

API_PATTERNS = {
    "procfs": ("proc_create", "proc_mkdir", "proc_remove", "remove_proc_entry", "seq_"),
    "sysfs": ("sysfs_", "device_create_file", "device_remove_file", "class_create", "device_create"),
    "debugfs": ("debugfs_",),
    "ioctl": ("ioctl", "unlocked_ioctl", "compat_ioctl", "ndo_do_ioctl"),
    "irq": ("request_irq", "request_threaded_irq", "free_irq", "of_irq_get", "platform_get_irq", "irq_"),
    "dma": ("dma_", "virt_to_phys", "phys_to_virt", "skb_", "alloc_skb", "netdev_alloc_skb"),
    "clock": ("clk_", "clock"),
    "reset": ("reset_", "_reset", "rstctrl"),
    "regulator": ("regulator_",),
    "power-domain": ("power_domain", "genpd", "pm_domain"),
    "iommu": ("iommu_", "smmu"),
    "phy": ("phy_", "phylib", "mdio"),
    "opp": ("dev_pm_opp", "operating_point"),
    "interconnect": ("icc_", "interconnect"),
    "thermal": ("thermal_", "hwmon_", "temp"),
    "pm-hook": ("suspend", "resume", "shutdown", "pm_runtime", "dev_pm_ops", "wakeup"),
    "register-io": ("ioremap", "iounmap", "readl", "writel", "ioread", "iowrite", "regmap_"),
}


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def ascii_strings(blob: bytes, minimum: int = 3) -> list[str]:
    return [m.decode("latin1", "replace") for m in re.findall(rb"[ -~]{%d,}" % minimum, blob)]


def find_readelf() -> str | None:
    for name in ("aarch64-linux-gnu-readelf", "llvm-readelf", "readelf"):
        path = shutil.which(name)
        if path:
            return path
    return None


def elf_symbols(path: Path, readelf: str | None) -> tuple[list[str], list[str], list[str]]:
    if not readelf:
        return [], [], []
    proc = subprocess.run([readelf, "-Ws", str(path)], text=True,
                          stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    defined: set[str] = set()
    all_defined: set[str] = set()
    undefined: set[str] = set()
    for line in proc.stdout.splitlines():
        parts = line.split()
        if len(parts) < 8 or not parts[0].rstrip(":").isdigit():
            continue
        name = parts[7].split("@", 1)[0]
        if not name:
            continue
        if parts[6] == "UND":
            undefined.add(name)
        else:
            all_defined.add(name)
            if parts[4] in {"GLOBAL", "WEAK"}:
                defined.add(name)
    return sorted(defined), sorted(undefined), sorted(all_defined)


def parse_modinfo(strings: Iterable[str]) -> dict[str, list[str]]:
    result: dict[str, list[str]] = defaultdict(list)
    for value in strings:
        match = re.match(r"^(license|author|description|depends|alias|parm|parmtype|vermagic|name)=(.*)$", value)
        if match:
            result[match.group(1)].append(match.group(2))
    return dict(result)


def category_hits(symbols: Iterable[str]) -> dict[str, list[str]]:
    result: dict[str, list[str]] = {}
    for category, needles in API_PATTERNS.items():
        hits = sorted({s for s in symbols if any(n.lower() in s.lower() for n in needles)})
        if hits:
            result[category] = hits
    return result


def interface_hints(strings: Iterable[str]) -> list[str]:
    hints: set[str] = set()
    for value in strings:
        low = value.lower()
        if len(value) > 240:
            continue
        if re.search(r"/(proc|sys|debug)/|/proc/|/sys/|debugfs", low):
            hints.add(value)
        elif any(token in low for token in ("proc_mkdir", "proc_create", "sysfs group", "usage: echo", "ioctl")):
            hints.add(value)
    return sorted(hints)


def loaded_metadata(name: str) -> dict[str, object]:
    directory = MODULE_ROOT / "loaded" / name
    if not directory.is_dir():
        return {"loaded": False}
    data: dict[str, object] = {"loaded": True}
    for filename in ("coresize", "initsize", "refcnt", "taint", "state"):
        path = directory / filename
        if path.is_file():
            data[filename] = path.read_text(errors="replace").strip()
    params: dict[str, str] = {}
    param_dir = directory / "parameters"
    if param_dir.is_dir():
        for path in sorted(param_dir.iterdir()):
            if path.is_file():
                params[path.name] = path.read_text(errors="replace").strip()
    data["runtime_parameters"] = params
    modinfo = directory / "modinfo.txt"
    if modinfo.is_file():
        data["runtime_modinfo_path"] = str(modinfo.relative_to(VENDOR))
    return data


def module_inventory() -> list[dict[str, object]]:
    readelf = find_readelf()
    grouped: dict[str, list[Path]] = defaultdict(list)
    for path in sorted((MODULE_ROOT / "files").rglob("*.ko")):
        grouped[sha256(path)].append(path)

    rows: list[dict[str, object]] = []
    for digest, paths in sorted(grouped.items(), key=lambda item: (item[1][0].name.lower(), item[1][0].stat().st_size)):
        canonical = paths[0]
        strings = ascii_strings(canonical.read_bytes())
        modinfo = parse_modinfo(strings)
        defined, undefined, all_defined = elf_symbols(canonical, readelf)
        name = (modinfo.get("name") or [canonical.stem])[0]
        sysfs_symbols = sorted({symbol for symbol in all_defined if
                                symbol.startswith("dev_attr_") or
                                symbol.endswith("_attribute_group") or
                                symbol.endswith("_debug_info_group") or
                                symbol.endswith("_debug_group") or
                                symbol.endswith("_test_group")})
        procfs_symbols = sorted({symbol for symbol in all_defined if "proc" in symbol.lower()})
        ioctl_symbols = sorted({symbol for symbol in all_defined if "ioctl" in symbol.lower()})
        pm_symbols = sorted({symbol for symbol in all_defined if any(token in symbol.lower() for token in
                            ("suspend", "resume", "shutdown", "poweroff", "low_power", "wakeup"))})
        row: dict[str, object] = {
            "name": name,
            "filename": canonical.name,
            "sha256": digest,
            "size": canonical.stat().st_size,
            "paths": [str(path.relative_to(VENDOR)) for path in paths],
            "duplicate_path_count": len(paths),
            "modinfo": modinfo,
            "defined_symbols": defined,
            "undefined_symbols": undefined,
            "api_categories": category_hits(defined + undefined + all_defined),
            "interface_hints": interface_hints(strings),
            "sysfs_attribute_symbols": sysfs_symbols,
            "procfs_symbols": procfs_symbols,
            "ioctl_symbols": ioctl_symbols,
            "pm_hook_symbols": pm_symbols,
            "function_name_hints": sorted({
                symbol for symbol in defined
                if any(token in symbol.lower() for token in (
                    "probe", "remove", "ioctl", "proc", "debug", "sysfs", "irq",
                    "interrupt", "dma", "clk", "reset", "suspend", "resume",
                    "thermal", "iommu", "power", "ioremap", "init_module", "cleanup_module",
                ))
            }),
        }
        row.update(loaded_metadata(name))
        # Runtime directory spelling sometimes differs from modinfo name.
        if not row["loaded"] and (MODULE_ROOT / "loaded" / canonical.stem).is_dir():
            row.update(loaded_metadata(canonical.stem))
        rows.append(row)
    return rows


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//.*", "", text)


def parse_dts(text: str, source: str) -> list[dict[str, object]]:
    text = strip_comments(text)
    lines = text.splitlines()
    nodes: list[dict[str, object]] = []
    stack: list[dict[str, object]] = []
    pending = ""
    pending_line = 0

    for lineno, raw in enumerate(lines, 1):
        line = raw.strip()
        if not line or line.startswith("/dts-") or line.startswith("/memreserve/"):
            continue
        if pending:
            pending += " " + line
            if ";" not in line:
                continue
            line = pending
            lineno = pending_line
            pending = ""
        if line.endswith("{"):
            label = ""
            name = line[:-1].strip()
            if ":" in name:
                label, name = [part.strip() for part in name.split(":", 1)]
            if not stack and name == "/":
                path = "/"
            else:
                parent = stack[-1]["path"].rstrip("/") if stack else ""
                path = f"{parent}/{name}"
            node = {"source": source, "line": lineno, "label": label, "name": name,
                    "path": path, "properties": {}}
            nodes.append(node)
            stack.append(node)
            continue
        if line.startswith("}"):
            if stack:
                stack.pop()
            continue
        if not stack:
            continue
        if "=" in line:
            if ";" not in line:
                pending = line
                pending_line = lineno
                continue
            key, value = line.split("=", 1)
            stack[-1]["properties"][key.strip()] = value.rsplit(";", 1)[0].strip()
        elif line.endswith(";"):
            stack[-1]["properties"][line[:-1].strip()] = True

    normalized: list[dict[str, object]] = []
    for node in nodes:
        props = node["properties"]
        compatibles = re.findall(r'"([^"]+)"', str(props.get("compatible", "")))
        if not compatibles and not any(prop in props for prop in RESOURCE_PROPS):
            continue
        entry = {key: node[key] for key in ("source", "line", "label", "name", "path")}
        entry["compatibles"] = compatibles
        for prop in RESOURCE_PROPS:
            if prop in props:
                entry[prop] = props[prop]
        normalized.append(entry)
    return normalized


def dt_inventory() -> list[dict[str, object]]:
    rows: list[dict[str, object]] = []
    vendor_dts = VENDOR / "2b5" / "zx279133-sr1010.dts"
    rows.extend(parse_dts(vendor_dts.read_text(errors="replace"), str(vendor_dts.relative_to(VENDOR))))

    dtc = shutil.which("dtc")
    runtime_dtb = RUNTIME / "device-tree" / "runtime.dtb"
    if dtc and runtime_dtb.is_file():
        proc = subprocess.run([dtc, "-I", "dtb", "-O", "dts", str(runtime_dtb)],
                              text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if proc.returncode == 0:
            rows.extend(parse_dts(proc.stdout, str(runtime_dtb.relative_to(VENDOR)) + " (dtc)"))

    for path in (KERNEL / "arch/arm64/boot/dts/zte").glob("zx279133*.dts*"):
        rows.extend(parse_dts(path.read_text(errors="replace"), str(path.relative_to(REPO))))
    return rows


def source_inventory() -> list[dict[str, object]]:
    candidates: set[Path] = set()
    for pattern in (
        "drivers/**/*zx279133*.[ch]", "drivers/**/*sr1010*.[ch]",
        "Documentation/devicetree/bindings/**/*zx279133*.yaml",
        "Documentation/devicetree/bindings/**/*sr1010*.yaml",
        "include/dt-bindings/**/*zx279133*.h",
        "arch/arm64/boot/dts/zte/zx279133*.dts*",
    ):
        candidates.update(KERNEL.glob(pattern))

    rows: list[dict[str, object]] = []
    for path in sorted(candidates):
        if not path.is_file():
            continue
        text = path.read_text(errors="replace")
        compatibles = sorted(set(re.findall(r'compatible\s*=\s*"([^"]+)"', text) +
                                 re.findall(r'\.compatible\s*=\s*"([^"]+)"', text) +
                                 re.findall(r'const:\s*([A-Za-z0-9,._+-]+)', text)))
        module_params = sorted(set(re.findall(r'module_param(?:_named)?\s*\(([^;]+)\)', text)))
        APIs = category_hits(re.findall(r'\b[A-Za-z_][A-Za-z0-9_]*\b', text))
        reg_offsets = sorted(set(re.findall(r'\b0x[0-9a-fA-F]{2,8}\b', text)), key=lambda x: int(x, 16))
        rows.append({
            "path": str(path.relative_to(REPO)),
            "sha256": sha256(path),
            "compatibles": compatibles,
            "module_parameters": module_params,
            "api_categories": APIs,
            "register_constants": reg_offsets,
        })
    return rows


def runtime_inventory() -> dict[str, object]:
    loaded = []
    proc_modules = RUNTIME / "system/proc/modules"
    if proc_modules.is_file():
        for line in proc_modules.read_text(errors="replace").splitlines():
            fields = line.split()
            if fields:
                loaded.append({"name": fields[0], "size": fields[1] if len(fields) > 1 else "",
                               "refcount": fields[2] if len(fields) > 2 else "",
                               "users": fields[3] if len(fields) > 3 else ""})
    return {
        "loaded_modules": loaded,
        "captured_debugfs_files": [str(path.relative_to(RUNTIME)) for path in sorted((RUNTIME / "debugfs").rglob("*")) if path.is_file()],
        "captured_irq_files": [str(path.relative_to(RUNTIME)) for path in sorted((RUNTIME / "irq").rglob("*")) if path.is_file()],
        "platform_devices": _read_lines(RUNTIME / "buses/platform-devices.txt"),
        "platform_drivers": _read_lines(RUNTIME / "buses/platform-drivers.txt"),
        "amba_devices": _read_lines(RUNTIME / "buses/amba-devices.txt"),
        "amba_drivers": _read_lines(RUNTIME / "buses/amba-drivers.txt"),
        "mdio_devices": _read_lines(RUNTIME / "buses/mdio-devices.txt"),
        "proc_iomem": _read_lines(RUNTIME / "system/proc/iomem"),
        "proc_interrupts": _read_lines(RUNTIME / "system/proc/interrupts"),
    }


def _read_lines(path: Path) -> list[str]:
    if not path.is_file():
        return []
    return [line.rstrip() for line in path.read_text(errors="replace").splitlines()]


def write_csv(path: Path, rows: list[dict[str, object]], fields: list[str]) -> None:
    with path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields, extrasaction="ignore")
        writer.writeheader()
        for row in rows:
            cooked = {}
            for field in fields:
                value = row.get(field, "")
                if isinstance(value, (list, dict)):
                    value = json.dumps(value, ensure_ascii=False, sort_keys=True)
                cooked[field] = value
            writer.writerow(cooked)


def main() -> None:
    modules = module_inventory()
    dt_nodes = dt_inventory()
    sources = source_inventory()
    runtime = runtime_inventory()
    manifest = {
        "scope": "SR1010 V1.0.0.2B5 / ZX279133",
        "evidence_roots": {
            "vendor_reference": str(VENDOR),
            "runtime": str(RUNTIME),
            "mainline": str(KERNEL),
        },
        "counts": {
            "module_paths": sum(int(row["duplicate_path_count"]) for row in modules),
            "unique_modules": len(modules),
            "loaded_unique_modules": sum(bool(row.get("loaded")) for row in modules),
            "dt_resource_nodes": len(dt_nodes),
            "zx279133_mainline_sources": len(sources),
        },
        "modules": modules,
        "device_tree_nodes": dt_nodes,
        "mainline_sources": sources,
        "runtime": runtime,
    }
    (HERE / "inventory.json").write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n")
    write_csv(HERE / "vendor-modules.csv", modules, [
        "name", "filename", "sha256", "size", "duplicate_path_count", "loaded",
        "coresize", "initsize", "refcnt", "taint", "state", "paths", "modinfo",
        "runtime_parameters", "api_categories", "sysfs_attribute_symbols", "procfs_symbols",
        "ioctl_symbols", "pm_hook_symbols", "function_name_hints", "interface_hints",
    ])
    write_csv(HERE / "dt-nodes.csv", dt_nodes, [
        "source", "line", "path", "label", "name", "compatibles", *RESOURCE_PROPS,
    ])
    write_csv(HERE / "mainline-sources.csv", sources, [
        "path", "sha256", "compatibles", "module_parameters", "api_categories", "register_constants",
    ])
    checksum_lines = []
    for path in sorted(HERE.rglob("*")):
        if not path.is_file() or path.name == "SHA256SUMS" or "__pycache__" in path.parts:
            continue
        checksum_lines.append(f"{sha256(path)}  {path.relative_to(HERE)}")
    (HERE / "SHA256SUMS").write_text("\n".join(checksum_lines) + "\n")
    print(json.dumps(manifest["counts"], indent=2))


if __name__ == "__main__":
    main()
