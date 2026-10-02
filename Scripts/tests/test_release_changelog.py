#!/usr/bin/env python3
"""Exercise platform filtering against a temporary Git history."""
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class ReleaseChangelogTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.repo = Path(self.temp.name)
        self.git("init", "-q")
        self.git("config", "user.name", "Test")
        self.git("config", "user.email", "test@example.invalid")
        self.commit("fix(mac): initial version", "mac.m")
        self.git("tag", "v1.2.6")

    def git(self, *args):
        return subprocess.check_output(["git", *args], cwd=self.repo, text=True).strip()

    def commit(self, subject, *paths):
        for name in paths:
            path = self.repo / name
            path.parent.mkdir(parents=True, exist_ok=True)
            with path.open("a") as out:
                out.write(subject + "\n")
        self.git("add", ".")
        self.git("commit", "-qm", subject)

    def notes(self, *args):
        return subprocess.check_output(
            ["python3", str(ROOT / "Scripts/generate-macos-changelog.py"), *args],
            cwd=self.repo, text=True,
        )

    def test_scopes_legacy_paths_and_shared_changes(self):
        self.commit("feat(win): scoped Windows update", "shared.cpp")
        self.commit("feat(ios): iOS keyboard", "ios.cpp")
        self.commit("fix: legacy Windows icon", "ChiaKey-Source/Loaders/Windows-TSF/icon.cpp", "README.MD")
        self.commit("feat: legacy Windows installer", "Packaging/Windows/installer.iss")
        self.commit("fix(mac): focus bug", "mac.m")
        self.commit("fix(core): shared learning", "core.cpp", "ChiaKey-Source/Loaders/Windows-TSF/engine.cpp")
        self.commit("docs: manual", "README.MD")
        self.commit("ci: workflow", ".github/workflows/example.yml")
        self.commit("chore: bump version to 1.2.7", "version.txt")
        notes = self.notes("--since", "v1.2.6")
        self.assertIn("focus bug", notes)
        self.assertIn("shared learning", notes)
        for excluded in ["initial version", "Windows", "iOS keyboard", "manual", "workflow", "bump version"]:
            self.assertNotIn(excluded, notes)

    def test_first_release_and_empty_platform_range(self):
        self.commit("feat(win): Windows only", "Packaging/Windows/file.iss")
        self.assertEqual(self.notes("--since", "v1.2.6"), "")
        self.assertIn("initial version", self.notes())

    def test_workflow_ignores_higher_windows_and_unmerged_mac_tags(self):
        self.git("tag", "win-v99.0.0-beta.1")
        original = self.git("rev-parse", "HEAD")
        self.git("checkout", "-qb", "future")
        self.commit("fix(mac): future work", "mac.m")
        self.git("tag", "v99.0.0")
        self.git("checkout", "-q", original)
        workflow = (ROOT / ".github/workflows/release.yml").read_text()
        selection = next(line.strip() for line in workflow.splitlines() if line.strip().startswith('latest="'))
        latest = subprocess.check_output(
            ["bash", "-c", selection + '\n printf "%s" "$latest"'],
            cwd=self.repo, text=True,
        )
        self.assertEqual(latest, "v1.2.6")


if __name__ == "__main__":
    unittest.main()
