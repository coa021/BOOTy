#!/usr/bin/env python3
import argparse
import struct
import sys
from pathlib import Path

# Must match shared/app_header.h
APP_MAGIC    = 0x0B00B1E5
# Jebemu mater de znam. idk why im giving 16KB now, seems too much. but i was thinking something regarding the sector so fuck it, will change later
HEADER_SIZE  = 16384

# Source - https://stackoverflow.com/a/75572017
# Posted by Satake, modified by community. See post 'Timeline' for change history
# Retrieved 2026-05-04, License - CC BY-SA 4.0
def crc32mpeg2(buf, crc=0xffffffff):
    for val in buf:
        crc ^= val << 24
        for _ in range(8):
            crc = crc << 1 if (crc & 0x80000000) == 0 else (crc << 1) ^ 0x104c11db7
    return crc


def version_to_int(s: str) -> int:
    major, minor = s.split(".")
    return (int(major) << 16) | int(minor)

def cmd_sign(args):
    code = Path(args.input).read_bytes()
    version = version_to_int(args.version)

    header = struct.pack("<III", APP_MAGIC, version, len(code))
    header += bytes(HEADER_SIZE - len(header))
    
    assert len(header) == HEADER_SIZE

    out = Path(args.output) if args.output else Path(args.input).with_suffix("") \
          .parent / (Path(args.input).stem + "_signed.bin")
    out.write_bytes(header + code)
    
    print(f"Signed image: {out}  ({len(header) + len(code)} bytes)")
    print(f"  Header @ 0x08004000 ({HEADER_SIZE} bytes)")
    print(f"  Code   @ 0x08008000 ({len(code)} bytes)")


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    sub = p.add_subparsers(dest="cmd", required=True)

    sp = sub.add_parser("sign")
    sp.add_argument("--input",   required=True)
    sp.add_argument("--version", required=True, help="e.g. 1.3")
    sp.add_argument("--output",  default=None)

    args = p.parse_args()
    {"sign": cmd_sign}[args.cmd](args)
