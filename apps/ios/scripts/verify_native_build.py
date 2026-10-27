#!/usr/bin/env python3
"""Verify iOS native build inputs and toolchain without consulting license data."""

import argparse
import hashlib
from pathlib import Path
import re
import sys

XMAKE_MIN_VERSION = "3.1.1"


def verify(inputs=None, xmake_version=None):
    if xmake_version is not None:
        version = re.search(r"xmake v(\d+)\.(\d+)\.(\d+)(?:\+|[,\s]|$)", xmake_version.read_text())
        if not version or tuple(map(int, version.groups())) < tuple(map(int, XMAKE_MIN_VERSION.split("."))):
            raise ValueError(f"Xmake {XMAKE_MIN_VERSION} or newer is required; see apps/ios/README.md")
    if inputs is not None:
        for line in inputs.read_text().splitlines():
            sha, name = line.split(maxsplit=1)
            if hashlib.sha256(Path(name).read_bytes()).hexdigest() != sha:
                raise ValueError(f"Native input checksum mismatch: {name}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, help="native archive .inputs.sha256 manifest")
    parser.add_argument("--xmake-version", type=Path, help="Xmake version output recorded by the native build")
    args = parser.parse_args()
    if not any((args.inputs, args.xmake_version)):
        parser.error("provide --inputs or --xmake-version")
    verify(args.inputs, args.xmake_version)
    print("Verified iOS native build inputs and toolchain")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError) as error:
        print(f"iOS native build: {error}", file=sys.stderr)
        sys.exit(1)
