#!/usr/bin/env python3
"""Filter platform-specific commits before constructing macOS release notes."""
import argparse
import re
import subprocess


WINDOWS_PATHS = (
    "ChiaKey-Source/Loaders/Windows-TSF/",
    "Packaging/Windows/",
)
WINDOWS_FILES = {".github/workflows/release-windows.yml", "Docs/WindowsImplementation.md"}
TITLES = {
    "feat": "Features", "fix": "Bug Fixes", "perf": "Performance",
    "refactor": "Refactoring", "test": "Tests", "build": "Build",
    "style": "Styling", "chore": "Chores", "other": "Other Changes",
}


def git(*args):
    return subprocess.check_output(["git", *args]).decode("utf-8", errors="replace")


def windows_only(paths):
    # Shared release documentation often accompanies old Windows commits.
    # Keep any commit touching shared code, scripts, or macOS code.
    windows = [p for p in paths if p.startswith(WINDOWS_PATHS) or p in WINDOWS_FILES]
    return bool(windows) and all(
        p in windows or p.lower().endswith(".md") for p in paths
    )


def generate(since=None):
    revisions = []
    if since:
        base = git("rev-parse", "--verify", since + "^{commit}").strip()
        revisions = [base + "..HEAD"]
    commits = git("log", "--no-merges", "--format=%H", *revisions).splitlines()
    buckets = {kind: [] for kind in TITLES}
    for commit in commits:
        short, subject = git("show", "-s", "--format=%h%x00%s", commit).rstrip("\n").split("\0", 1)
        match = re.match(r"^(\w+)(?:\(([^)]+)\))?!?:\s+(.+)$", subject)
        kind, scope, description = match.groups() if match else ("other", None, subject)
        kind = kind.lower()
        if kind in {"docs", "ci"} or (kind == "chore" and "bump version" in description):
            continue
        if scope and scope.lower() in {"win", "windows", "ios"}:
            continue
        paths = git("diff-tree", "--root", "--no-commit-id", "--name-only", "-r", "-z", commit).split("\0")
        if windows_only([p for p in paths if p]):
            continue
        buckets[kind if kind in TITLES else "other"].append(f"- {description} (`{short}`)\n")
    sections = []
    for kind, title in TITLES.items():
        if not buckets[kind]:
            continue
        body = "".join(buckets[kind])
        if kind == "chore":
            sections.append(f"<details>\n<summary>Chores</summary>\n\n{body}</details>\n")
        else:
            sections.append(f"### {title}\n\n{body}")
    return "\n".join(sections)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--since", help="Previous macOS release tag; omit for the entire history")
    args = parser.parse_args()
    print(generate(args.since), end="")
