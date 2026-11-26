#!/usr/bin/env python3
"""Install an individually supplied Jaguar JERRY wave ROM from Virtual Jaguar source.

The official Virtual Jaguar 3.12.0 source carries waveTableROM in its
src/jerry/wavetable.c. This extracts that *user-supplied* data, verifies it
against independent ROM hashes, and writes only the requested file. No
hardware ROM is embedded in this script or in the distributable port.
"""
from __future__ import annotations

import argparse
import hashlib
import pathlib
import re
import sys
import zipfile
import zlib

EXPECTED_SIZE = 4096
EXPECTED_CRC32 = 0x7A25EE5B
EXPECTED_SHA1 = "58117e11fd6478c521fbd3fdbe157f39567552f0"


def read_source(path: pathlib.Path) -> str:
    if path.is_dir():
        choices = list(path.glob("**/src/jerry/wavetable.c"))
        if len(choices) != 1:
            raise ValueError(f"Expected one src/jerry/wavetable.c, found {len(choices)}")
        return choices[0].read_text(encoding="utf-8")
    if path.suffix.lower() == ".zip":
        with zipfile.ZipFile(path) as archive:
            matches = [n for n in archive.namelist()
                       if n.endswith("/src/jerry/wavetable.c") and not n.startswith("__MACOSX/")]
            if len(matches) != 1:
                raise ValueError(f"Expected one src/jerry/wavetable.c in archive, found {len(matches)}")
            return archive.read(matches[0]).decode("utf-8")
    if path.name == "wavetable.c":
        return path.read_text(encoding="utf-8")
    raise ValueError("Input must be Virtual Jaguar source ZIP, source directory, or wavetable.c")


def extract(source: str) -> bytes:
    match = re.search(r"\bwaveTableROM\s*\[\s*4096\s*\]\s*=\s*\{([^}]*)\}", source, re.S)
    if not match:
        raise ValueError("Virtual Jaguar waveTableROM[4096] declaration not found")
    body = re.sub(r"/\*.*?\*/|//[^\n]*", "", match.group(1), flags=re.S)
    tokens = [token.strip() for token in body.split(",") if token.strip()]
    if len(tokens) != EXPECTED_SIZE or any(not re.fullmatch(r"0[xX][0-9a-fA-F]{1,2}", t) for t in tokens):
        raise ValueError("Invalid or unexpected 4 KiB wavetable initializer")
    data = bytes(int(t, 16) for t in tokens)
    crc = zlib.crc32(data)
    sha1 = hashlib.sha1(data).hexdigest()
    if crc != EXPECTED_CRC32 or sha1 != EXPECTED_SHA1:
        raise ValueError(f"JERRY waveform ROM hash mismatch: CRC32={crc:08X}, SHA1={sha1}")
    return data


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("source", type=pathlib.Path,
                    help="User-owned Virtual Jaguar source ZIP, directory, or wavetable.c")
    ap.add_argument("--output", type=pathlib.Path, default=pathlib.Path("jagwave.rom"),
                    help="Output file next to the avp_pc executable (default: ./jagwave.rom)")
    ap.add_argument("--verify-only", action="store_true", help="Check the bytes without writing a ROM")
    args = ap.parse_args()
    try:
        data = extract(read_source(args.source))
        if not args.verify_only:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            if args.output.exists() and args.output.read_bytes() != data:
                raise ValueError(f"Refusing to overwrite nonmatching file: {args.output}")
            args.output.write_bytes(data)
            if args.output.read_bytes() != data:
                raise IOError("Output verification failed")
        print(f"PASS: authentic 4096-byte JERRY waveform ROM, CRC32={EXPECTED_CRC32:08X}, SHA1={EXPECTED_SHA1}")
        if not args.verify_only:
            print(f"Installed: {args.output}")
        return 0
    except (OSError, UnicodeError, ValueError, zipfile.BadZipFile) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
