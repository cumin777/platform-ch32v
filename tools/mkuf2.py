#!/usr/bin/env python3
"""Pack a raw firmware.bin into an UF2 file for the XIAO CH32X035 TinyUF2 bootloader.

Usage:
    python tools/mkuf2.py <firmware.bin> [-b 0x4000] [-o <out.uf2>]

The bin must be linked above the bootloader region (base 0x4000 by default,
see bsp/ch32x035_uf2.ld and the *_uf2 env in the sample's platformio.ini).
A bin linked at 0x00000000 is rejected: the bootloader refuses writes below
the app region and the jump would crash anyway.

Family ID 0x699B62EC = TinyUF2 UF2_FAMILY_ID for the ch32x035 port.
"""
import argparse
import os
import struct
import sys

UF2_MAGIC_START0 = 0x0A324655  # "UF2\n"
UF2_MAGIC_START1 = 0x9E5D5157
UF2_MAGIC_END = 0x0AB16F30
UF2_FLAG_FAMILY_ID_PRESENT = 0x00002000
UF2_PAYLOAD_SIZE = 256

CH32X035_FAMILY_ID = 0x699B62EC
DEFAULT_BASE = 0x4000  # TinyUF2 bootloader occupies the first 16K


def check_image_base(data, base):
    """The image starts with a 'j handle_reset' word followed by vector words;
    vector addresses below `base` mean the image is linked at 0x00000000."""
    words = struct.unpack_from("<8I", data)
    nonzero = [w for w in words[1:] if w]
    if nonzero and min(nonzero) < base:
        sys.exit(
            "error: image looks linked at 0x00000000 "
            "(vector word %#x < base %#x); rebuild with the *_uf2 env first"
            % (min(nonzero), base)
        )
    if not data:
        sys.exit("error: empty input file")


def pack(bin_path, out_path, base, family):
    data = open(bin_path, "rb").read()
    check_image_base(data, base)

    total = (len(data) + UF2_PAYLOAD_SIZE - 1) // UF2_PAYLOAD_SIZE
    with open(out_path, "wb") as f:
        for i in range(total):
            chunk = data[i * UF2_PAYLOAD_SIZE:(i + 1) * UF2_PAYLOAD_SIZE]
            chunk = chunk.ljust(UF2_PAYLOAD_SIZE, b"\x00")
            header = struct.pack(
                "<IIIIIIII",
                UF2_MAGIC_START0, UF2_MAGIC_START1,
                UF2_FLAG_FAMILY_ID_PRESENT,
                base + i * UF2_PAYLOAD_SIZE,
                UF2_PAYLOAD_SIZE, i, total, family,
            )
            # 512-byte block: 32-byte header + 256-byte payload + pad + trailing magic
            f.write(header + chunk + b"\x00" * 220 + struct.pack("<I", UF2_MAGIC_END))
    return total


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("bin", help="raw firmware binary linked at --base")
    ap.add_argument("-b", "--base", type=lambda x: int(x, 0), default=DEFAULT_BASE,
                    help="app base address (default 0x4000)")
    ap.add_argument("-f", "--family", type=lambda x: int(x, 0), default=CH32X035_FAMILY_ID,
                    help="UF2 family id (default 0x699B62EC ch32x035)")
    ap.add_argument("-o", "--out", default=None, help="output .uf2 (default <bin>.uf2)")
    args = ap.parse_args()

    out = args.out or args.bin + ".uf2"
    n = pack(args.bin, out, args.base, args.family)
    print("wrote %s: %d blocks, base %#x, family %#x" % (out, n, args.base, args.family))


if __name__ == "__main__":
    main()
