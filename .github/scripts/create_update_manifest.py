#!/usr/bin/env python3
import argparse
import hashlib
import json
from pathlib import Path


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as file:
        for chunk in iter(lambda: file.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


parser = argparse.ArgumentParser()
parser.add_argument("--channel", required=True, choices=("stable", "nightly"))
parser.add_argument("--version", required=True)
parser.add_argument("--build-id", required=True)
parser.add_argument("--platform", required=True, choices=("linux", "windows"))
parser.add_argument("--minimum-version", required=True)
parser.add_argument("--release-url", required=True)
parser.add_argument("--linux-archive", required=True)
parser.add_argument("--windows-archive", required=True)
args = parser.parse_args()

archive_name = args.linux_archive if args.platform == "linux" else args.windows_archive
archive = Path(archive_name)
if not archive.is_file():
    raise SystemExit(f"Archive not found: {archive}")

tag = "nightly" if args.channel == "nightly" else f"v{args.minimum_version}"
manifest = {
    "channel": args.channel,
    "version": args.version,
    "build_id": args.build_id,
    "platform": args.platform,
    "archive": archive_name,
    "archive_url": f"https://github.com/HenrySck075/VirtualClub/releases/download/{tag}/{archive_name}",
    "archive_size": archive.stat().st_size,
    "sha256": sha256(archive),
    "release_url": f"https://github.com/HenrySck075/VirtualClub/releases/tag/{tag}",
    "minimum_launcher_version": args.minimum_version,
    "signatures": [],
}

Path(f"update-manifest-{args.platform}.json").write_text(
    json.dumps(manifest, indent=2) + "\n",
    encoding="utf-8",
)
Path(f"{archive_name}.sha256").write_text(
    f"{manifest['sha256']}  {archive_name}\n",
    encoding="utf-8",
)
