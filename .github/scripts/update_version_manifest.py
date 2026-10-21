#!/usr/bin/env python3
"""Merge a release's server downloads into the shared version.json."""

import argparse
import datetime
import json
from pathlib import Path
import re
import hashlib
import os
import subprocess
import tempfile


DOWNLOAD_BASE = "https://downloads.crossdesk.cn/"
ARTIFACTS = {
    "windows-x64": ("win-x64", "exe"),
    "windows-x64-portable": ("win-x64-portable", "zip"),
    "macos-x64": ("macos-x64", "pkg"),
    "macos-arm64": ("macos-arm64", "pkg"),
    "linux-amd64": ("linux-amd64", "deb"),
    "linux-arm64": ("linux-arm64", "deb"),
    "linux-amd64-virtual-desktop-xfce": ("virtual-desktop-xfce-linux-amd64", "deb"),
    "linux-arm64-virtual-desktop-xfce": ("virtual-desktop-xfce-linux-arm64", "deb"),
    "ios-arm64": ("ios-arm64-unsigned", "zip"),
    "android-arm64": ("android-arm64(?:-unsigned)?", "apk"),
}

def sign_update(payload, private_key, expected_public_key):
    """Sign exact UTF-8 JSON bytes using a PEM Ed25519 key, never shell text."""
    if not re.fullmatch(r"[0-9a-f]{64}", expected_public_key):
        raise ValueError("CROSSDESK_UPDATE_PUBLIC_KEY must contain 64 lowercase hex characters")
    with tempfile.TemporaryDirectory(prefix="crossdesk-sign-") as directory:
        root = Path(directory)
        key = root / "key.pem"
        key.write_text(private_key, encoding="utf-8")
        key.chmod(0o600)
        public = subprocess.run(["openssl", "pkey", "-in", str(key), "-pubout", "-outform", "DER"],
                                check=True, capture_output=True).stdout
        # RFC 8410 SubjectPublicKeyInfo prefix for an Ed25519 public key.
        if public != bytes.fromhex("302a300506032b6570032100" + expected_public_key):
            raise ValueError("Update signing key does not match the public key built into clients")
        data = root / "payload.json"
        data.write_bytes(payload.encode("utf-8"))
        signature = subprocess.run(
            ["openssl", "pkeyutl", "-sign", "-rawin", "-inkey", str(key), "-in", str(data)],
            check=True, capture_output=True).stdout
        if len(signature) != 64:
            raise ValueError("Expected an Ed25519 signature")
        return {"payload": payload, "signature": signature.hex()}


def version_fields(version):
    version = version.removeprefix("v")
    base = r"[0-9]+(?:\.[0-9]+){1,3}"
    for pattern in (
        rf"(?P<base>{base})-(?P<patch>[0-9]+)-(?P<date>[0-9]{{8}})",
        rf"(?P<base>{base})-(?P<date>[0-9]{{8}})(?:-(?P<patch>[0-9]+))?",
        rf"(?P<base>{base})(?:-(?P<patch>[0-9]+))?",
    ):
        match = re.fullmatch(pattern, version)
        if match:
            return match.groupdict()
    raise ValueError(f"Invalid release version: {version}")


def version_info(version, fallback_date=None):
    fields = version_fields(version)
    version = version.removeprefix("v")
    date = fields.get("date")
    release_date = (datetime.datetime.strptime(date, "%Y%m%d").date().isoformat()
                    if date else fallback_date)
    if not release_date:
        raise ValueError(f"Release date is missing for {version}")
    datetime.date.fromisoformat(release_date)
    return {"latest_version": version, "version": version,
            "releaseDate": release_date, "patch": int(fields.get("patch") or 0)}


def release_order(metadata):
    """Match client ordering: zero-padded numeric components, then patch, never date."""
    version = metadata.get("latest_version") or metadata.get("version")
    if not version:
        return None
    fields = version_fields(version)
    numbers = tuple(int(part) for part in fields["base"].split("."))
    inline_patch = fields.get("patch") or "0"
    patch = int(inline_patch) if len(inline_patch) <= 4 else 0
    # Legacy manifests may store the effective patch outside the version string.
    explicit_patch = metadata.get("patch")
    if isinstance(explicit_patch, str) and re.fullmatch(r"[0-9]+", explicit_patch):
        explicit_patch = int(explicit_patch)
    if type(explicit_patch) is int and 0 <= explicit_patch <= 2147483647:
        patch = explicit_patch
    return numbers + (0,) * (4 - len(numbers)) + (patch,)


def artifact_downloads(directory):
    downloads = {}
    private_key = os.environ.get("CROSSDESK_UPDATE_PRIVATE_KEY", "")
    public_key = os.environ.get("CROSSDESK_UPDATE_PUBLIC_KEY", "")
    for path in sorted(Path(directory).iterdir()):
        if not path.is_file():
            continue
        for key, (prefix, extension) in ARTIFACTS.items():
            match = re.fullmatch(rf"crossdesk-{prefix}-v([0-9][0-9.\-]*)\.{extension}", path.name)
            if not match:
                continue
            metadata = version_info(match[1])
            if key in downloads:
                previous = downloads[key]
                if (key != "android-arm64" or previous["version"] != metadata["version"]
                        or ("-unsigned-" in previous["filename"]) == ("-unsigned-" in path.name)):
                    raise ValueError(f"Multiple release artifacts for {key}")
                # Older releases may retain their unsigned APK after adding a signed one.
                if "-unsigned-" in path.name:
                    continue
            downloads[key] = {"url": DOWNLOAD_BASE + path.name, "filename": path.name,
                              "version": metadata["version"], "releaseDate": metadata["releaseDate"]}
            with path.open("rb") as artifact:
                hasher = hashlib.sha256()
                for chunk in iter(lambda: artifact.read(65536), b""):
                    hasher.update(chunk)
                digest = hasher.hexdigest()
            downloads[key].update(size=path.stat().st_size, sha256=digest)
            if private_key and key.startswith(("windows-", "macos-", "linux-")) and "portable" not in key:
                payload = json.dumps({
                    "schema": 1, "platform": key, "version": metadata["version"],
                    "url": DOWNLOAD_BASE + path.name, "filename": path.name,
                    "size": path.stat().st_size, "sha256": digest,
                }, sort_keys=True, separators=(",", ":"), ensure_ascii=False)
                downloads[key]["update"] = sign_update(payload, private_key, public_key)
            break
    if not downloads:
        raise ValueError("No supported release artifacts found")
    return downloads


def merge_release(current, tag, name, notes, downloads, published_date=None):
    if not isinstance(current, dict) or not isinstance(current.get("downloads"), dict):
        raise ValueError("Current version.json must contain a downloads object")
    mobile = re.fullmatch(r"(ios|android)-(v.+)", tag)
    if mobile:
        platform, version = mobile.groups()
        expected_key = f"{platform}-arm64"
        if set(downloads) != {expected_key}:
            raise ValueError(f"{tag} must contain only the {expected_key} release artifact")
        metadata = version_info(version, published_date)
        if downloads[expected_key]["version"] != metadata["version"]:
            raise ValueError("Mobile source tag does not match the artifact version")
    else:
        if not tag.startswith("v"):
            raise ValueError(f"Expected a version tag, got {tag!r}")
        metadata = version_info(tag, published_date)
        # Clients require the full artifact version to match the advertised
        # desktop version, including its date and patch spelling.
        for platform, download in downloads.items():
            if (platform.startswith(("windows-", "macos-", "linux-"))
                    and download["version"] != metadata["version"]):
                raise ValueError(
                    f"Desktop release {tag} expects version {metadata['version']}, "
                    f"but {platform} artifact {download['filename']} "
                    f"has version {download['version']}")
        current_order = release_order(current)
        if current_order is not None and release_order(metadata) < current_order:
            current_version = current.get("latest_version") or current.get("version")
            print(f"Skipping {tag}: older than current desktop release {current_version}")
            return dict(current)

    accepted_downloads = {}
    for platform, download in downloads.items():
        previous_order = release_order(current["downloads"].get(platform, {}))
        if previous_order is not None and release_order(download) < previous_order:
            print(f"Skipping {tag} download for {platform}: a newer version is already published")
            continue
        accepted_downloads[platform] = download
    if mobile and not accepted_downloads:
        return dict(current)

    result = dict(current)
    result.pop("platforms", None)
    result["downloads"] = {**current["downloads"], **accepted_downloads}
    if not mobile:
        result.update({**metadata, "releaseName": name, "releaseNotes": notes, "tagName": tag})
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--current", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--artifacts", required=True, type=Path)
    parser.add_argument("--tag")
    parser.add_argument("--name")
    parser.add_argument("--notes", type=Path)
    parser.add_argument("--release-json", type=Path)
    args = parser.parse_args()
    if args.release_json:
        release = json.loads(args.release_json.read_text())
        tag = release["tagName"]
        name = release.get("name") or tag
        notes = release.get("body") or ""
        published_date = (release.get("publishedAt") or "").split("T")[0] or None
    else:
        if not args.tag or not args.name or not args.notes:
            parser.error("Supply --release-json or --tag, --name and --notes")
        tag, name, notes = args.tag, args.name, args.notes.read_text()
        published_date = None
    current_text = args.current.read_text()
    current = json.loads(current_text)
    downloads = artifact_downloads(args.artifacts)
    result = merge_release(current, tag, name, notes, downloads, published_date)
    if result == current:
        args.output.write_text(current_text)
        print(f"No changes to version.json for {tag}")
    else:
        args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
        print(f"Updated version.json for {tag}")


if __name__ == "__main__":
    main()
