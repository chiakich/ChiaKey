#!/usr/bin/env python3
"""Collect explicit user-facing release fragments for one platform."""
import argparse
import json
import subprocess
from pathlib import Path


def git(*args):
    return subprocess.check_output(["git", *args], text=True).strip()


def generate(platform, since=None):
    if since:
        base = git("rev-parse", "--verify", since + "^{commit}")
        paths = git("diff", "--name-only", base, "HEAD", "--", "ReleaseNotes").splitlines()
    else:
        paths = git("ls-files", "ReleaseNotes").splitlines()
    lines = []
    for name in sorted(paths):
        path = Path(name)
        if path.suffix != ".json" or not path.is_file():
            continue
        fragment = json.loads(path.read_text())
        for change in fragment["changes"]:
            platforms = change["platforms"]
            if not platforms or not set(platforms) <= {"macos", "windows"}:
                raise ValueError("invalid platforms in " + name)
            description = change["description"]
            if not isinstance(description, str) or not description.strip() or "\n" in description:
                raise ValueError("invalid description in " + name)
            if platform in platforms:
                line = "- " + description
                if line not in lines:
                    lines.append(line)
    return "\n".join(lines) + ("\n" if lines else "")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--platform", choices=["macos", "windows"], required=True)
    parser.add_argument("--since")
    args = parser.parse_args()
    print(generate(args.platform, args.since), end="")
