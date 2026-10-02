#!/usr/bin/env python3
"""Collect conventional commit messages for one platform's release notes."""
import argparse
import re
import subprocess


def git(*args):
    return subprocess.check_output(["git", *args], text=True).strip()


def platforms(scope, description, paths):
    # Explicit scopes are authoritative. Older unscoped history needs no rewrite.
    if scope in {"win", "windows"}:
        return {"windows"}
    if scope in {"mac", "macos"}:
        return {"macos"}
    if scope in {"ios"}:
        return set()
    if not scope and re.search(r"\b(windows|win32|tsf|appcontainer|msvc)\b", description, re.I):
        return {"windows"}
    found = set()
    for path in paths:
        if path.startswith(("Docs/", ".github/", "Scripts/tests/")) or path.lower().startswith("readme"):
            continue
        if path.startswith(("ChiaKey-Source/Loaders/Windows-TSF/", "Packaging/Windows/")):
            found.add("windows")
        elif path.startswith("ChiaKey-Source/Loaders/iOS/"):
            continue
        elif (path.startswith("ChiaKey-Source/Loaders/OSX-IMK/") or
              "/OSX/" in path or path.endswith(".xcodeproj/project.pbxproj") or
              path in {"Scripts/build-release-package.sh", "Scripts/build-dev.sh"}):
            found.add("macos")
        else:
            found.update({"macos", "windows"})
    return found or {"macos", "windows"}


def generate(platform, since=None):
    revision = "HEAD"
    if since:
        base = git("rev-parse", "--verify", since + "^{commit}")
        revision = base + "..HEAD"
    lines = []
    for record in git("log", "--no-merges", "--reverse", "--format=%H%x09%s", revision).splitlines():
        commit, subject = record.split("\t", 1)
        match = re.fullmatch(r"(feat|fix|perf|revert)(?:\(([^)]+)\))?(!)?:\s+(.+)", subject)
        if not match:
            continue
        kind, scope, breaking, description = match.groups()
        paths = git("diff-tree", "--root", "--no-commit-id", "--name-only", "-r", commit).splitlines()
        if platform in platforms((scope or "").lower(), description, paths):
            # Keep the original message as input to AI and as the offline fallback.
            line = "- " + subject
            if line not in lines:
                lines.append(line)
    return "\n".join(lines) + ("\n" if lines else "")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--platform", choices=["macos", "windows"], required=True)
    parser.add_argument("--since")
    args = parser.parse_args()
    print(generate(args.platform, args.since), end="")
