#!/usr/bin/env python3
import argparse
import struct
import sys
from pathlib import Path
import zlib # for crc
import hashlib # for sha256
from cryptography.hazmat.primitives.asymmetric import ec, utils
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric.utils import decode_dss_signature


APP_MAGIC = 0x0B00B1E5
HEADER_SIZE = 80

def load_private_key():
    with open("dont-touch/private_key.pem", "rb") as f:
        return serialization.load_pem_private_key(f.read(), password=None)

def sign_code(code, private_key):
    sig_der = private_key.sign(code, ec.ECDSA(hashes.SHA256()))
    r, s = decode_dss_signature(sig_der)
    return r.to_bytes(32, 'big') + s.to_bytes(32, 'big')

def custom_sha256(data):
    digest = hashlib.sha256(data).digest()
    print(f"digest: {digest}")
    print(f"digest hex: {digest.hex()}")

    return digest

def custom_crc(app_bin):
    with open(app_bin, "rb") as f:
        data = f.read()

    crc = zlib.crc32(data) & 0xFFFFFFFF

    print(f"CRC32 = 0x{crc:08X}")
    print(f"Size  = {len(data)} bytes")
    return crc


def version_to_int(s: str) -> int:
    major, minor = s.split(".")
    return (int(major) << 16) | int(minor)

def cmd_sign(args):
    code = Path(args.input).read_bytes()
    version = version_to_int(args.version)

    crc = custom_crc(args.input)
    custom_sha256(code)

    private_key = load_private_key()
    signature = sign_code(code, private_key)
    print(f"Signature: {signature.hex()}")

    header = struct.pack("<IIII", APP_MAGIC, version, len(code), crc)

    header += signature 

    header += bytes(HEADER_SIZE - len(header))
    
    assert len(header) == HEADER_SIZE

    out = Path(args.output) if args.output else Path(args.input).with_suffix("") \
          .parent / (Path(args.input).stem + "_signed.bin")
    out.write_bytes(header + code)
    
    print(f"Signed image: {out}  ({len(header) + len(code)} bytes)")
    print(f"  Header @ 0x08004000 ({HEADER_SIZE} bytes)")
    print(f"  Code   @ 0x08006000 ({len(code)} bytes)")


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    sub = p.add_subparsers(dest="cmd", required=True)

    sp = sub.add_parser("sign")
    sp.add_argument("--input",   required=True)
    sp.add_argument("--version", required=True, help="e.g. 1.3")
    sp.add_argument("--output",  default=None)

    args = p.parse_args()
    {"sign": cmd_sign}[args.cmd](args)
