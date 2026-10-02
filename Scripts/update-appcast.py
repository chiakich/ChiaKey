#!/usr/bin/env python3
"""Merge a platform release without losing or downgrading either channel."""
import argparse
import copy
import json
import re
from pathlib import Path


def version(tag):
    match = re.fullmatch(r"v(\d+)\.(\d+)\.(\d+)(?:-beta\.([1-9]\d*))?", tag)
    if not match:
        raise ValueError("invalid joint tag: " + str(tag))
    major, minor, patch, beta = match.groups()
    return (int(major), int(minor), int(patch), beta is None, int(beta or 0))


def validate_entry(entry, platform):
    version(entry["tag"])
    if entry["prerelease"] != ("-beta." in entry["tag"]):
        raise ValueError("prerelease flag disagrees with tag")
    if not re.fullmatch(r"[0-9a-f]{64}", entry["sha256"]):
        raise ValueError("invalid digest")
    extension = ".pkg" if platform == "macos" else "-Setup.exe"
    if not entry["package_name"].endswith(extension) or not entry["package_url"].startswith("https://"):
        raise ValueError("wrong platform artifact")


def merge(current, entry, platform):
    if not isinstance(current, dict) or current.get("schema") != 1:
        raise ValueError("unsupported or corrupt existing appcast")
    if current.get("platform", platform) != platform:
        raise ValueError("wrong appcast platform")
    result = copy.deepcopy(current)
    result["platform"] = platform
    validate_entry(entry, platform)
    for channel in ("stable", "beta"):
        existing = result.get(channel)
        if existing is not None:
            validate_entry(existing, platform)
            if channel == "stable" and existing["prerelease"]:
                raise ValueError("beta version in stable channel")
    channel = "beta" if entry["prerelease"] else "stable"
    existing = result.get(channel)
    if existing and version(entry["tag"]) < version(existing["tag"]):
        raise ValueError("refusing to downgrade " + channel)
    if existing and existing["tag"] == entry["tag"] and existing["sha256"] != entry["sha256"]:
        raise ValueError("refusing to replace an existing release's contents")
    result[channel] = entry
    # Legacy macOS beta followers expect their field to include stable releases.
    candidates = [result.get("stable"), result.get("beta")]
    result["beta"] = max((e for e in candidates if e), key=lambda e: version(e["tag"]))
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--current", required=True)
    parser.add_argument("--entry", required=True)
    parser.add_argument("--platform", choices=["macos", "windows"], required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    result = merge(json.loads(Path(args.current).read_text()), json.loads(Path(args.entry).read_text()), args.platform)
    Path(args.output).write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
