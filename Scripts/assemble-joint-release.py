#!/usr/bin/env python3
"""Assemble platform notes and checksums after both builds have succeeded."""
import argparse
import hashlib
import re
from pathlib import Path


def assemble(directory, tag):
    if not re.fullmatch(r"v\d+\.\d+\.\d+(?:-beta\.[1-9]\d*)?", tag):
        raise ValueError("invalid joint release tag")
    root = Path(directory)
    version = tag[1:]
    packages = list(root.glob("ChiaKey-*.pkg"))
    windows = root / f"ChiaKey-Windows-{version}-Setup.exe"
    if len(packages) != 1 or not windows.is_file():
        raise ValueError("a joint release requires exactly one macOS package and the matching Windows installer")
    expected_pkg = f"ChiaKey-{version}.pkg"
    # macOS packaging uses its numeric CFBundleVersion for beta filenames.
    if packages[0].name not in {expected_pkg, f"ChiaKey-{version.split('-')[0]}.pkg"}:
        raise ValueError("macOS package does not match the release version")
    checksums = "".join(f"{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\n"
                        for p in sorted([packages[0], windows]))
    (root / "SHA256SUMS.txt").write_text(checksums)
    sections = []
    if "-beta." in version:
        sections.append("> 此版本為 Beta 預覽版。\n")
    for platform, label in [("macos", "macOS"), ("windows", "Windows（預覽版）")]:
        raw = (root / f"notes-{platform}.md").read_text().strip()
        summary_path = root / f"summary-{platform}.md"
        summary = summary_path.read_text().strip() if summary_path.exists() else ""
        notes = summary or raw or "- 此平台沒有額外的使用者可見變更。"
        if summary and raw:
            notes += f"\n\n<details>\n<summary>完整變更</summary>\n\n{raw}\n\n</details>"
        extra_path = root / f"extra-{platform}.md"
        extra = extra_path.read_text().strip() if extra_path.exists() else ""
        if extra:
            notes += "\n\n" + extra
        if platform == "windows":
            notes += "\n\nWindows 安裝檔目前尚未簽章；ARM64 尚未支援。同名重發的 win-v0.1.0-beta.1 測試預覽可直接追蹤 v* 更新；重發前的原建置需手動安裝一次新版。"
        (root / f"release-notes-{platform}.md").write_text(notes + "\n")
        sections.append(f"## {label}\n\n{notes}\n")
    sections.append("macOS 請下載 `.pkg`；Windows 請下載 `ChiaKey-Windows-" + version + "-Setup.exe`。\n")
    (root / "RELEASE_NOTES.md").write_text("\n".join(sections))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", required=True)
    parser.add_argument("--tag", required=True)
    args = parser.parse_args()
    assemble(args.directory, args.tag)
