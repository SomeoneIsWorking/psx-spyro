#!/usr/bin/env python3
"""Which globals a guest function touches: lui/lo pairs decoded numerically from the shipping executable.

A `lui` never paired with a use is reported as `unpaired`.
"""

import argparse
import pathlib
import sys

import title_identity  # noqa: F401  (puts the framework tools on sys.path)
from formats import psx_exe  # noqa: E402

# The instructions that pair with a lui to form an address, by primary opcode.
LO_OPS = {
    0x09: "addiu", 0x0D: "ori",
    0x20: "lb", 0x21: "lh", 0x23: "lw", 0x24: "lbu", 0x25: "lhu",
    0x28: "sb", 0x29: "sh", 0x2B: "sw",
    0x32: "lwc2", 0x3A: "swc2",
}


def signed16(value: int) -> int:
    return value - 0x10000 if value & 0x8000 else value


def scan(image: psx_exe.PsxExe, start: int, end: int | None):
    """Decode [start, end), or to the function's own `jr $ra` and its delay slot when end is None."""
    hi: dict[int, tuple[int, int]] = {}  # register -> (high half, the pc that set it)
    pairs: list[tuple[int, int, str, int]] = []  # (pc, address, mnemonic, register)
    unpaired: list[tuple[int, int]] = []
    address = start
    instructions = 0
    returning = False
    while end is None or address < end:
        word = image.word(address)
        instructions += 1
        op = word >> 26
        if op == 0x0F:  # lui
            rt = (word >> 16) & 0x1F
            if rt in hi:
                unpaired.append((rt, hi[rt][1]))
            hi[rt] = (word & 0xFFFF, address)
        elif op in LO_OPS:
            rs = (word >> 21) & 0x1F
            if rs in hi:
                high, _ = hi.pop(rs)
                pairs.append(((high << 16) + signed16(word & 0xFFFF), address, LO_OPS[op], rs))
        elif op == 0 and (word & 0x3F) == 0x08 and ((word >> 21) & 0x1F) == 31:
            returning = True  # jr $ra — one delay slot still executes
        address += 4
        if end is None and returning and address > start:
            # consume the delay slot, then stop
            word = image.word(address)
            instructions += 1
            op = word >> 26
            if op in LO_OPS:
                rs = (word >> 21) & 0x1F
                if rs in hi:
                    high, _ = hi.pop(rs)
                    pairs.append(((high << 16) + signed16(word & 0xFFFF), address, LO_OPS[op], rs))
            break
    for register, (_, pc) in hi.items():
        unpaired.append((register, pc))
    unpaired.sort(key=lambda item: item[1])
    return instructions, pairs, unpaired


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("start", help="guest address of the function, e.g. 0x8001973C")
    parser.add_argument("--end", default=None, help="stop here instead of at the function's jr $ra")
    parser.add_argument("--img", default="scratch/assets/spyro1/SCUS_942.28",
                        help="the provisioned executable to decode")
    args = parser.parse_args()

    path = pathlib.Path(args.img)
    if not path.is_file():
        print(f"re_globals: REFUSED — no image at {path}. Provision the title first; this tool "
              f"reads the user's own executable and never ships one.", file=sys.stderr)
        return 2
    start = int(args.start, 0)
    end = int(args.end, 0) if args.end else None
    try:
        instructions, pairs, unpaired = scan(psx_exe.load(path), start, end)
    except (IndexError, ValueError) as error:
        print(f"re_globals: REFUSED — {error}", file=sys.stderr)
        return 2

    print(f"0x{start:08X}: decoded {instructions} instruction(s)")
    if not pairs:
        print("  no lui/lo pairs — this function touches no global through a hi/lo pair")
    seen: dict[int, list[str]] = {}
    for address, pc, mnemonic, _ in pairs:
        seen.setdefault(address, []).append(f"{mnemonic}@0x{pc:08X}")
    for address in sorted(seen):
        print(f"  0x{address:08X}  {' '.join(seen[address])}")
    if unpaired:
        print(f"  {len(unpaired)} lui(s) never paired with a low half — an address this tool "
              f"could NOT resolve, not an absence of one:")
        for register, pc in sorted(unpaired, key=lambda item: item[1]):
            print(f"    $r{register} set at 0x{pc:08X}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
