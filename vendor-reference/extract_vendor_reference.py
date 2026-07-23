#!/usr/bin/env python3
"""Validate and extract the ZTE SR1010 V1.0.0.2B5 vendor OTA reference."""

from __future__ import annotations

import argparse
import gzip
import hashlib
import json
import shutil
import struct
import subprocess
import sys
import zlib
from dataclasses import dataclass, field
from pathlib import Path

from Crypto.Cipher import AES


OUTER_MAGIC = bytes.fromhex("999999994444444455555555aaaaaaaa")
INNER_MAGIC = bytes.fromhex("333333336666666699999999cccccccc")
KERNEL_MAGIC = bytes.fromhex("33333333cccccccc88888888dddddddd")
KERNEL_KEY = b"SR101038c09ffb98"
ROOTFS_KEY = b"SR1010V102020030"
UBOOT_SIZE = 0x180000
KERNEL_MAGIC_REL = 0x180000
KERNEL_DATA_REL = 0x180020
ROOTFS_REL = 0x6E0000
KERNEL_VIRTUAL_BASE = 0xFFFFFFC010080000
START_KERNEL_ADDRESS = 0xFFFFFFC010AB0994


class ExtractError(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ExtractError(message)


def le32(blob: bytes, offset: int) -> int:
    return struct.unpack_from("<I", blob, offset)[0]


def be32(blob: bytes, offset: int) -> int:
    return struct.unpack_from(">I", blob, offset)[0]


def sha256(blob: bytes) -> str:
    return hashlib.sha256(blob).hexdigest()


def csp_crc32(blob: bytes) -> int:
    """ZTE's CSP CRC is the standard reflected IEEE CRC-32."""
    return zlib.crc32(blob) & 0xFFFFFFFF


def decrypt_ecb(ciphertext: bytes, key: bytes, name: str) -> bytes:
    require(len(ciphertext) % AES.block_size == 0, f"{name} length is not AES-block aligned")
    return AES.new(key, AES.MODE_ECB).decrypt(ciphertext)


@dataclass
class FdtNode:
    name: str
    properties: dict[str, bytes] = field(default_factory=dict)
    children: list["FdtNode"] = field(default_factory=list)

    def property(self, name: str) -> bytes:
        try:
            return self.properties[name]
        except KeyError as error:
            raise ExtractError(f"FIT property missing: {self.name}/{name}") from error

    def string(self, name: str) -> str:
        value = self.property(name)
        require(value.endswith(b"\0"), f"FIT string is not NUL-terminated: {self.name}/{name}")
        try:
            return value[:-1].decode("ascii")
        except UnicodeDecodeError as error:
            raise ExtractError(f"FIT string is not ASCII: {self.name}/{name}") from error

    def child(self, name: str) -> "FdtNode":
        for child in self.children:
            if child.name == name:
                return child
        raise ExtractError(f"FIT node missing: {self.name}/{name}")


def parse_fdt(blob: bytes) -> tuple[FdtNode, int]:
    require(len(blob) >= 40 and be32(blob, 0) == 0xD00DFEED, "FIT FDT magic mismatch")
    total_size, struct_off, strings_off, _reserve_off, version, last_comp, _boot_cpu, strings_size, struct_size = struct.unpack_from(
        ">9I", blob, 4
    )
    require(40 <= total_size <= len(blob), "invalid FIT total size")
    require(version >= 16 and last_comp <= version, "unsupported FIT version")
    require(
        40 <= struct_off <= struct_off + struct_size <= total_size,
        "invalid FIT structure bounds",
    )
    require(
        40 <= strings_off <= strings_off + strings_size <= total_size,
        "invalid FIT strings bounds",
    )

    structure = blob[struct_off : struct_off + struct_size]
    strings = blob[strings_off : strings_off + strings_size]
    cursor = 0
    root: FdtNode | None = None
    stack: list[FdtNode] = []

    while cursor + 4 <= len(structure):
        token = be32(structure, cursor)
        cursor += 4
        if token == 1:  # FDT_BEGIN_NODE
            end = structure.find(b"\0", cursor)
            require(end >= 0, "unterminated FIT node name")
            try:
                node = FdtNode(structure[cursor:end].decode("ascii"))
            except UnicodeDecodeError as error:
                raise ExtractError("FIT node name is not ASCII") from error
            cursor = (end + 4) & ~3
            require(cursor <= len(structure), "FIT node name exceeds structure block")
            if stack:
                stack[-1].children.append(node)
            else:
                require(root is None, "multiple FIT roots")
                root = node
            stack.append(node)
        elif token == 2:  # FDT_END_NODE
            require(stack, "FIT END_NODE underflow")
            stack.pop()
        elif token == 3:  # FDT_PROP
            require(stack and cursor + 8 <= len(structure), "malformed FIT property")
            value_size, name_offset = struct.unpack_from(">II", structure, cursor)
            cursor += 8
            value_end = cursor + value_size
            require(value_end <= len(structure) and name_offset < len(strings), "FIT property out of bounds")
            name_end = strings.find(b"\0", name_offset)
            require(name_end >= 0, "unterminated FIT property name")
            try:
                name = strings[name_offset:name_end].decode("ascii")
            except UnicodeDecodeError as error:
                raise ExtractError("FIT property name is not ASCII") from error
            stack[-1].properties[name] = structure[cursor:value_end]
            cursor = (value_end + 3) & ~3
            require(cursor <= len(structure), "FIT property alignment exceeds structure block")
        elif token == 4:  # FDT_NOP
            continue
        elif token == 9:  # FDT_END
            require(root is not None and not stack, "unterminated FIT tree")
            return root, total_size
        else:
            raise ExtractError(f"unknown FIT structure token: 0x{token:x}")

    raise ExtractError("FIT END token missing")


def verify_image_hashes(node: FdtNode, data: bytes) -> dict[str, str]:
    verified: dict[str, str] = {}
    for child in node.children:
        if not child.name.startswith("hash"):
            continue
        algorithm = child.string("algo")
        expected = child.property("value")
        if algorithm == "sha1":
            actual = hashlib.sha1(data).digest()
        elif algorithm == "sha256":
            actual = hashlib.sha256(data).digest()
        elif algorithm == "crc32":
            actual = struct.pack(">I", zlib.crc32(data) & 0xFFFFFFFF)
        else:
            raise ExtractError(f"unsupported FIT hash algorithm: {algorithm}")
        require(actual == expected, f"FIT {node.name} {algorithm} hash mismatch")
        verified[algorithm] = actual.hex()
    return verified


def extract_fit_image(node: FdtNode) -> tuple[bytes, bytes, dict[str, object]]:
    stored = node.property("data")
    compression = node.string("compression") if "compression" in node.properties else "none"
    hashes = verify_image_hashes(node, stored)
    if compression == "none":
        decoded = stored
    elif compression == "gzip":
        decoded = gzip.decompress(stored)
    else:
        raise ExtractError(f"unsupported FIT compression: {compression}")
    return stored, decoded, {
        "name": node.name,
        "compression": compression,
        "stored_size": len(stored),
        "decoded_size": len(decoded),
        "hashes": hashes,
    }


def parse_ota(blob: bytes) -> tuple[int, dict[str, int]]:
    require(len(blob) >= 0x214 and blob[:16] == OUTER_MAGIC, "outer OTA magic mismatch")
    signature_size = le32(blob, 0x10)
    version_header_off = 0x14 + signature_size
    body_off = version_header_off + 0x200
    require(body_off <= len(blob), "OTA version header is truncated")
    header = blob[version_header_off:body_off]
    require(header[0xF4:0x104] == INNER_MAGIC, "inner OTA magic mismatch")

    fields = {
        "signature_size": signature_size,
        "version_header_offset": version_header_off,
        "body_offset": body_off,
        "body_size": le32(header, 0x30),
        "kernel_size": le32(header, 0x34),
        "kernel_offset": le32(header, 0x38),
        "kernel_crc32": le32(header, 0x3C),
        "rootfs_size": le32(header, 0x40),
        "rootfs_offset": le32(header, 0x44),
        "rootfs_crc32": le32(header, 0x48),
        "header_crc32": le32(header, 0xA4),
        "header_crc32_2": le32(header, 0x1FC),
    }
    require(fields["body_size"] <= len(blob) - body_off, "OTA body is truncated")
    require(
        csp_crc32(header[:0xA4]) == fields["header_crc32"],
        "OTA first header CRC mismatch",
    )
    require(
        csp_crc32(header[:0x1FC]) == fields["header_crc32_2"],
        "OTA second header CRC mismatch",
    )

    for name in ("kernel", "rootfs"):
        offset = fields[f"{name}_offset"]
        size = fields[f"{name}_size"]
        require(offset >= body_off and size <= len(blob) - offset, f"{name} payload out of bounds")
        actual = csp_crc32(blob[offset : offset + size])
        require(actual == fields[f"{name}_crc32"], f"{name} payload CRC mismatch")

    require(body_off + UBOOT_SIZE <= body_off + fields["body_size"], "OTA U-Boot is truncated")
    require(
        fields["kernel_offset"] == body_off + KERNEL_DATA_REL,
        "unexpected kernel offset relative to OTA body",
    )
    require(
        fields["rootfs_offset"] == body_off + ROOTFS_REL,
        "unexpected rootfs offset relative to OTA body",
    )
    require(
        blob[body_off + KERNEL_MAGIC_REL : body_off + KERNEL_MAGIC_REL + len(KERNEL_MAGIC)]
        == KERNEL_MAGIC,
        "kernel magic mismatch",
    )
    return body_off, fields


def fit_address(node: FdtNode, property_name: str) -> int:
    value = node.property(property_name)
    require(len(value) in (4, 8), f"invalid FIT {property_name} length")
    return int.from_bytes(value, "big")


def resolve_tool(requested: Path | None, name: str, fallbacks: tuple[Path, ...] = ()) -> Path:
    candidates: list[Path] = []
    if requested is not None:
        candidates.append(requested)
    else:
        discovered = shutil.which(name)
        if discovered:
            candidates.append(Path(discovered))
        candidates.extend(fallbacks)
    for candidate in candidates:
        if candidate.is_file():
            return candidate.resolve()
        discovered = shutil.which(str(candidate))
        if discovered:
            return Path(discovered).resolve()
    raise ExtractError(f"required tool not found: {name}")


def run_tool(arguments: list[str], name: str) -> subprocess.CompletedProcess[str]:
    try:
        return subprocess.run(arguments, check=True, capture_output=True, text=True)
    except OSError as error:
        raise ExtractError(f"failed to execute {name}: {error}") from error
    except subprocess.CalledProcessError as error:
        detail = (error.stderr or error.stdout or "").strip()
        raise ExtractError(f"{name} failed: {detail}") from error


def dtc_version(dtc: Path) -> str:
    result = run_tool([str(dtc), "--version"], "dtc --version")
    return (result.stdout or result.stderr).strip()


def python_distribution_version(executable: Path, distribution: str) -> str:
    try:
        first_line = executable.read_bytes().splitlines()[0].decode("ascii")
        require(first_line.startswith("#!"), f"{executable.name} has no Python shebang")
        interpreter = first_line[2:]
        result = run_tool(
            [
                interpreter,
                "-c",
                "import importlib.metadata as m,sys; print(m.version(sys.argv[1]))",
                distribution,
            ],
            f"query {distribution} version",
        )
        return result.stdout.strip()
    except (ExtractError, IndexError, UnicodeDecodeError):
        return "unknown"


def inspect_symbolized_elf(blob: bytes, image_size: int) -> dict[str, object]:
    require(
        len(blob) >= 64
        and blob[:6] == b"\x7fELF\x02\x01"
        and struct.unpack_from("<H", blob, 16)[0] == 2
        and struct.unpack_from("<H", blob, 18)[0] == 183,
        "rebuilt kernel is not an AArch64 ELF64 ET_EXEC",
    )
    entry = struct.unpack_from("<Q", blob, 24)[0]
    section_offset = struct.unpack_from("<Q", blob, 40)[0]
    section_entry_size, section_count, string_section_index = struct.unpack_from("<HHH", blob, 58)
    require(section_entry_size == 64 and section_count > 0, "rebuilt ELF has no section table")
    require(string_section_index < section_count, "rebuilt ELF section-name table is invalid")
    require(
        section_offset <= len(blob) - section_entry_size * section_count,
        "rebuilt ELF section table is truncated",
    )

    sections: list[tuple[int, int, int, int, int, int, int]] = []
    for index in range(section_count):
        offset = section_offset + index * section_entry_size
        name_offset, section_type, _flags, address, file_offset, size, link = struct.unpack_from(
            "<IIQQQQI", blob, offset
        )
        require(file_offset <= len(blob), "rebuilt ELF section offset is invalid")
        if section_type != 8:  # SHT_NOBITS occupies memory but has no file payload.
            require(size <= len(blob) - file_offset, "rebuilt ELF section is truncated")
        sections.append((name_offset, section_type, address, file_offset, size, link, offset))

    string_section = sections[string_section_index]
    section_names = blob[string_section[3] : string_section[3] + string_section[4]]

    def string_at(table: bytes, offset: int, description: str) -> str:
        require(offset < len(table), f"invalid {description} string offset")
        end = table.find(b"\0", offset)
        require(end >= 0, f"unterminated {description} string")
        try:
            return table[offset:end].decode("ascii")
        except UnicodeDecodeError as error:
            raise ExtractError(f"non-ASCII {description} string") from error

    named_sections = {string_at(section_names, section[0], "section name"): section for section in sections}
    require(".kernel" in named_sections, "rebuilt ELF has no .kernel section")
    require(".symtab" in named_sections, "rebuilt ELF has no symbol table")
    kernel_section = named_sections[".kernel"]
    require(kernel_section[2] == KERNEL_VIRTUAL_BASE, "rebuilt ELF virtual base mismatch")
    require(kernel_section[4] == image_size, "rebuilt ELF .kernel size mismatch")

    symbol_section = named_sections[".symtab"]
    require(symbol_section[1] == 2 and symbol_section[5] < len(sections), "invalid rebuilt ELF symbol table")
    symbol_strings_section = sections[symbol_section[5]]
    symbol_strings = blob[
        symbol_strings_section[3] : symbol_strings_section[3] + symbol_strings_section[4]
    ]
    symbol_entry_size = struct.unpack_from("<Q", blob, symbol_section[6] + 56)[0]
    require(symbol_entry_size == 24 and symbol_section[4] % 24 == 0, "unsupported ELF symbol format")
    symbol_count = symbol_section[4] // symbol_entry_size
    require(symbol_count > 1000, "rebuilt ELF symbol table is unexpectedly small")

    wanted = {"_text", "start_kernel", "__init_begin", "__init_end"}
    symbols: dict[str, int] = {}
    for offset in range(symbol_section[3], symbol_section[3] + symbol_section[4], symbol_entry_size):
        name_offset = struct.unpack_from("<I", blob, offset)[0]
        if name_offset >= len(symbol_strings):
            continue
        name = string_at(symbol_strings, name_offset, "symbol")
        if name in wanted:
            symbols[name] = struct.unpack_from("<Q", blob, offset + 8)[0]
    require(symbols.get("_text") == KERNEL_VIRTUAL_BASE, "rebuilt ELF _text address mismatch")
    require(symbols.get("start_kernel") == START_KERNEL_ADDRESS, "rebuilt ELF start_kernel address mismatch")
    require(entry == START_KERNEL_ADDRESS, "rebuilt ELF entry is not start_kernel")
    return {
        "entry": entry,
        "entry_hex": f"0x{entry:016x}",
        "kernel_virtual_base": kernel_section[2],
        "kernel_virtual_base_hex": f"0x{kernel_section[2]:016x}",
        "kernel_section_size": kernel_section[4],
        "symbol_count": symbol_count,
        "symbols": symbols,
        "symbols_hex": {name: f"0x{address:016x}" for name, address in symbols.items()},
    }


def extract(
    input_path: Path,
    output_dir: Path,
    force: bool,
    dtc_requested: Path | None,
    vmlinux_to_elf_requested: Path | None,
) -> None:
    blob = input_path.read_bytes()
    body_off, ota = parse_ota(blob)
    kernel_encrypted = blob[ota["kernel_offset"] : ota["kernel_offset"] + ota["kernel_size"]]
    fit_padded = decrypt_ecb(kernel_encrypted, KERNEL_KEY, "encrypted kernel FIT")
    require(be32(fit_padded, 0) == 0xD00DFEED, "decrypted kernel is not a FIT")
    fit_size = be32(fit_padded, 4)
    require(40 <= fit_size <= len(fit_padded), "FIT total size exceeds encrypted kernel payload")
    fit = fit_padded[:fit_size]
    root, parsed_fit_size = parse_fdt(fit)
    require(parsed_fit_size == fit_size, "FIT parser size disagreement")

    images = root.child("images")
    configurations = root.child("configurations")
    if "default" in configurations.properties:
        configuration_name = configurations.string("default")
    else:
        require(configurations.children, "FIT has no configuration")
        configuration_name = configurations.children[0].name
    configuration = configurations.child(configuration_name)
    kernel_node = images.child(configuration.string("kernel"))
    dtb_node = images.child(configuration.string("fdt"))
    kernel_gzip, image, kernel_info = extract_fit_image(kernel_node)
    _dtb_stored, dtb, dtb_info = extract_fit_image(dtb_node)
    _dtb_root, dtb_size = parse_fdt(dtb)
    require(dtb_size == len(dtb), "board DTB contains trailing or truncated data")

    require(len(image) >= 0x40 and image[0x38:0x3C] == b"ARMd", "ARM64 Image magic mismatch")
    text_offset, image_size = struct.unpack_from("<QQ", image, 8)
    load = fit_address(kernel_node, "load")
    entry = fit_address(kernel_node, "entry")
    require(image_size >= len(image), "ARM64 Image size is smaller than extracted data")
    require(load <= entry < load + image_size, "FIT entry is outside the ARM64 Image mapping")

    rootfs_encrypted = blob[ota["rootfs_offset"] : ota["rootfs_offset"] + ota["rootfs_size"]]
    rootfs = decrypt_ecb(rootfs_encrypted, ROOTFS_KEY, "encrypted rootfs")
    require(rootfs[:2] == b"\x85\x19", "decrypted rootfs is not JFFS2")

    files: dict[str, bytes] = {
        "uboot.bin": blob[body_off : body_off + UBOOT_SIZE],
        "kernel.encrypted.bin": kernel_encrypted,
        "vendor-2b5.itb": fit,
        "kernel-image.gz": kernel_gzip,
        "Image": image,
        "zx279133-sr1010.dtb": dtb,
        "rootfs.encrypted.bin": rootfs_encrypted,
        "rootfs.jffs2": rootfs,
    }

    if output_dir.exists():
        require(force, f"output directory exists: {output_dir} (pass --force to replace it)")
    temporary_dir = output_dir.with_name(output_dir.name + ".tmp")
    require(not temporary_dir.exists(), f"temporary output directory exists: {temporary_dir}")
    repository_root = Path(__file__).resolve().parents[2]
    dtc = resolve_tool(dtc_requested, "dtc")
    vmlinux_to_elf = resolve_tool(
        vmlinux_to_elf_requested,
        "vmlinux-to-elf",
        (repository_root / "raw/.venv/bin/vmlinux-to-elf",),
    )

    try:
        temporary_dir.mkdir(parents=True)
        for name, data in files.items():
            (temporary_dir / name).write_bytes(data)
        run_tool(
            [
                str(dtc),
                "-I",
                "dtb",
                "-O",
                "dts",
                "-o",
                str(temporary_dir / "zx279133-sr1010.dts"),
                str(temporary_dir / "zx279133-sr1010.dtb"),
            ],
            "dtc decompile",
        )
        run_tool(
            [
                str(vmlinux_to_elf),
                str(temporary_dir / "Image"),
                str(temporary_dir / "kernel-2b5.elf"),
            ],
            "vmlinux-to-elf",
        )
        files["zx279133-sr1010.dts"] = (temporary_dir / "zx279133-sr1010.dts").read_bytes()
        files["kernel-2b5.elf"] = (temporary_dir / "kernel-2b5.elf").read_bytes()
        elf_info = inspect_symbolized_elf(files["kernel-2b5.elf"], len(image))
        manifest = {
            "input": str(input_path),
            "input_sha256": sha256(blob),
            "ota": ota,
            "fit": {
                "configuration": configuration_name,
                "sha256": sha256(fit),
                "kernel": kernel_info,
                "dtb": dtb_info,
            },
            "image": {
                "sha256": sha256(image),
                "physical_load": load,
                "physical_load_hex": f"0x{load:x}",
                "physical_entry": entry,
                "physical_entry_hex": f"0x{entry:x}",
                "text_offset": text_offset,
                "text_offset_hex": f"0x{text_offset:x}",
                "image_size": image_size,
                "image_size_hex": f"0x{image_size:x}",
            },
            "elf": elf_info,
            "tools": {
                "dtc": dtc_version(dtc),
                "vmlinux_to_elf": python_distribution_version(vmlinux_to_elf, "vmlinux-to-elf"),
            },
            "files": {
                name: {"size": len(data), "sha256": sha256(data)} for name, data in files.items()
            },
        }
        (temporary_dir / "manifest.json").write_text(
            json.dumps(manifest, indent=2, sort_keys=True) + "\n"
        )
        (temporary_dir / "SHA256SUMS").write_text(
            "".join(f"{sha256(data)}  {name}\n" for name, data in files.items())
        )
        if output_dir.exists():
            shutil.rmtree(output_dir)
        temporary_dir.rename(output_dir)
    except Exception:
        shutil.rmtree(temporary_dir, ignore_errors=True)
        raise

    print(
        json.dumps(
            {
                "output_dir": str(output_dir),
                "configuration": configuration_name,
                "fit_size": fit_size,
                "image_size": len(image),
                "dtb_size": len(dtb),
            },
            sort_keys=True,
        )
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path, help="vendor OTA .bin")
    parser.add_argument("--output-dir", required=True, type=Path, help="new extraction directory")
    parser.add_argument("--force", action="store_true", help="replace an existing output directory")
    parser.add_argument("--dtc", type=Path, help="path to dtc (default: PATH)")
    parser.add_argument(
        "--vmlinux-to-elf",
        type=Path,
        help="path to vmlinux-to-elf (default: PATH or raw/.venv)",
    )
    arguments = parser.parse_args()
    try:
        extract(
            arguments.input,
            arguments.output_dir,
            arguments.force,
            arguments.dtc,
            arguments.vmlinux_to_elf,
        )
    except (ExtractError, OSError, ValueError, struct.error, gzip.BadGzipFile) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
