#!/usr/bin/env python3
"""Check a sealed Sonsei Neuron image (Wireless_neuron.bin) before flashing.

Runs the same checks Bazecor runs before it flashes a Sonsei Neuron .bin
(seal version, seal CRC, size, program CRC). With --reference, it also
requires the seal fields that should never differ from an official image
(device id, program start, firmware version) to match that image.

Usage: check_seal.py Wireless_neuron.bin [--reference official/Wireless_neuron.bin]
"""
import argparse
import struct
import sys
import zlib

FIELDS = ("seal_version", "seal_size", "seal_crc", "device_id",
          "program_start", "program_size", "program_crc", "program_version")
FIRST_SECTOR_SIZE = 4096
MUST_MATCH_REFERENCE = ("seal_version", "seal_size", "device_id",
                        "program_start", "program_version")


def parse(data):
    return dict(zip(FIELDS, struct.unpack("<8I", data[:32])))


def check(path):
    data = open(path, "rb").read()
    if len(data) < 32:
        return None, [f"{path}: too small to contain a seal"]
    seal = parse(data)
    errors = []
    if seal["seal_version"] != 2:
        errors.append(f"seal version {seal['seal_version']}, expected 2")
    unsigned = struct.pack("<8I", seal["seal_version"], seal["seal_size"], 0,
                           seal["device_id"], seal["program_start"],
                           seal["program_size"], seal["program_crc"],
                           seal["program_version"])
    if zlib.crc32(unsigned) != seal["seal_crc"]:
        errors.append("seal CRC mismatch")
    if len(data) - FIRST_SECTOR_SIZE < seal["program_size"]:
        errors.append("file shorter than the program size in the seal")
    program = data[FIRST_SECTOR_SIZE:FIRST_SECTOR_SIZE + seal["program_size"]]
    if zlib.crc32(program) != seal["program_crc"]:
        errors.append("program CRC mismatch")
    return seal, errors


def show(label, seal):
    print(f"{label}:")
    for k in FIELDS:
        print(f"  {k:16} 0x{seal[k]:08x} ({seal[k]})")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("image")
    ap.add_argument("--reference")
    args = ap.parse_args()

    seal, errors = check(args.image)
    if seal:
        show(args.image, seal)
    if args.reference and seal:
        ref, ref_errors = check(args.reference)
        if ref_errors:
            errors += [f"reference: {e}" for e in ref_errors]
        else:
            show(args.reference, ref)
            for k in MUST_MATCH_REFERENCE:
                if seal[k] != ref[k]:
                    errors.append(f"{k} 0x{seal[k]:x} differs from reference 0x{ref[k]:x}")
    if errors:
        for e in errors:
            print(f"FAIL: {e}")
        return 1
    print("OK: seal valid" + (" and consistent with reference" if args.reference else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
