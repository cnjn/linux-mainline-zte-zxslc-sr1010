#!/usr/bin/env python3
"""Join captured CPU single steps to the measured Intel firmware.

This is a bounded trace annotation tool, not a complete PPU disassembler.
Successors come from observations; an absent next stop is not DMA completion.
"""

import argparse
import hashlib
import json
import re
import struct
from pathlib import Path


FIRMWARE_SHA256 = "056da8ecb66d8aa13248eb974a87b81ad97d96507fc5c526edbe2b98cc992b40"
READ_FIELDS = (31 << 46) | (127 << 19)
READ_BASE = 0x0700000018000523


def audit(firmware, capture):
    digest = hashlib.sha256(firmware).hexdigest()
    if digest != FIRMWARE_SHA256:
        raise ValueError("firmware differs from the calibrated Intel image")
    trace = capture["cpu_trace"]
    if not trace:
        raise ValueError("capture contains no CPU steps")
    states = []
    state = {}
    for stop in trace:
        state = {**state, **stop}
        states.append(state)
    control = re.search(r"(?:^|\s)cpu_control=(\d+)(?:\s|$)", capture["args"])
    port = re.search(r"(?:^|\s)cpu_wan_port=1(?:\s|$)", capture["args"])
    rows = []
    for index, stop in enumerate(states):
        pc = stop["pc"]
        if not 0 <= pc < 12288:
            raise ValueError(f"PC outside calibrated instruction RAM: {pc}")
        instruction = struct.unpack_from(">Q", firmware, 16 + 8 * pc)[0]
        following = states[index + 1] if index + 1 < len(states) else None
        row = {
            "pc": pc,
            "firmware_instruction": f"{instruction:016x}",
            "observed_next_pc": following["pc"] if following else None,
        }
        replay = ((pc == 1926 and control and int(control[1])) or
                  (pc == 1928 and port))
        if replay:
            row["instruction_replayed"] = True
        elif instruction & ~READ_FIELDS == READ_BASE:
            register = (instruction >> 46) & 31
            word = (instruction >> 19) & 127
            read = {"packet_word": word, "destination_spr": register}
            source = stop.get(f"p{word}")
            result = following.get(f"r{register}") if following else None
            read["sampled_source"] = source
            read["sampled_result_at_next_stop"] = result
            read["check"] = ("match" if source == result else "mismatch") \
                if source is not None and result is not None else "not_sampled"
            row["packet_word_read_candidate"] = read
        rows.append(row)
    return {
        "firmware_sha256": digest,
        "capture_args": capture["args"],
        "capture_verdict": capture["verdict"],
        "exact_wire_frames": capture["exact_wire_frames"],
        "rows": rows,
        "context_end_log": [line for line in capture.get("board", "").splitlines()
                            if "cpu-trace end " in line],
        "limits": [
            "Read field layout is inferred from calibrated local packet-word loads.",
            "Not-sampled loads require further runtime validation.",
            "Branch successors are observed, not statically decoded predicates.",
            "Final stop/context end is not proof of frame end or DMA release.",
            "No external payload DMA or continuation opcode is decoded.",
        ],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("firmware", type=Path)
    parser.add_argument("captures", type=Path, nargs="+")
    args = parser.parse_args()
    firmware = args.firmware.read_bytes()
    result = []
    for path in args.captures:
        report = audit(firmware, json.loads(path.read_text()))
        report["capture"] = str(path)
        report["capture_sha256"] = hashlib.sha256(path.read_bytes()).hexdigest()
        result.append(report)
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
