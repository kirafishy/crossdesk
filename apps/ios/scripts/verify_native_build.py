#!/usr/bin/env python3
"""Verify iOS native build inputs and toolchain without consulting license data."""

import argparse
import hashlib
from pathlib import Path
import re
import subprocess
import sys

XMAKE_MIN_VERSION = "3.1.1"
XMAKE_REPOSITORY_REVISION = "121109352da272cd1a3d4ed52f2dcda34f839955"


def verify(inputs=None, xmake_repository=None, xmake_version=None):
    if xmake_version is not None:
        version = re.search(r"xmake v(\d+)\.(\d+)\.(\d+)(?:\+|[,\s]|$)", xmake_version.read_text())
        if not version or tuple(map(int, version.groups())) < tuple(map(int, XMAKE_MIN_VERSION.split("."))):
            raise ValueError(f"Xmake {XMAKE_MIN_VERSION} or newer is required; see apps/ios/README.md")
    if xmake_repository is not None:
        revision = subprocess.check_output(
            ["git", "-C", str(xmake_repository), "rev-parse", "HEAD"], text=True).strip()
        if revision != XMAKE_REPOSITORY_REVISION:
            raise ValueError("Xmake package repository differs from the pinned iOS toolchain")
        changes = subprocess.check_output(
            ["git", "-C", str(xmake_repository), "status", "--porcelain", "--untracked-files=normal"],
            text=True).strip()
        if changes:
            raise ValueError("Xmake package repository has local changes; use the unmodified pinned revision")
    if inputs is not None:
        for line in inputs.read_text().splitlines():
            sha, name = line.split(maxsplit=1)
            if hashlib.sha256(Path(name).read_bytes()).hexdigest() != sha:
                raise ValueError(f"Native input checksum mismatch: {name}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, help="native archive .inputs.sha256 manifest")
    parser.add_argument("--xmake-repository", type=Path, help="active pinned Xmake package repository")
    parser.add_argument("--xmake-version", type=Path, help="Xmake version output recorded by the native build")
    args = parser.parse_args()
    if not any((args.inputs, args.xmake_repository, args.xmake_version)):
        parser.error("provide --inputs, --xmake-repository or --xmake-version")
    verify(args.inputs, args.xmake_repository, args.xmake_version)
    print("Verified iOS native build inputs and toolchain")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"iOS native build: {error}", file=sys.stderr)
        sys.exit(1)
