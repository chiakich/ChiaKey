#!/usr/bin/env python3
"""Exercise platform filtering against a temporary Git history."""
import subprocess
import sys
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
            [sys.executable, str(ROOT / "Scripts/generate-release-notes.py"), "--platform", "macos", *args],
            cwd=self.repo, text=True,
        )

    def test_platforms_and_internal_commits(self):
        self.commit('feat(win): Windows update', 'shared.cpp')
        self.commit('fix(mac): Mac focus', 'shared.cpp')
        self.commit('fix: Shared fix', 'shared.cpp')
        self.commit('ci: build both', '.github/workflows/test.yml')
        self.commit('docs: explain updates', 'Docs/updates.md')
        self.commit('refactor: internals', 'shared.cpp')
        self.commit('feat: legacy Windows settings', 'shared.cpp')
        self.commit('fix: legacy composition focus', 'ChiaKey-Source/Loaders/Windows-TSF/TextService.cpp')
        self.commit('fix: legacy Mac symbols', 'ChiaKey-Source/Loaders/OSX-IMK/InputController.mm')
        notes = self.notes('--since', 'v1.2.6')
        self.assertIn('Mac focus', notes)
        self.assertIn('Shared fix', notes)
        self.assertIn('legacy Mac symbols', notes)
        for omitted in ['Windows update', 'legacy Windows settings', 'legacy composition focus',
                        'build both', 'explain updates', 'internals']:
            self.assertNotIn(omitted, notes)
        windows = subprocess.check_output(
            [sys.executable, str(ROOT / 'Scripts/generate-release-notes.py'), '--platform', 'windows', '--since', 'v1.2.6'],
            cwd=self.repo, text=True)
        self.assertIn('Windows update', windows)
        self.assertIn('Shared fix', windows)
        self.assertIn('legacy composition focus', windows)
        self.assertNotIn('Mac focus', windows)
        self.assertNotIn('legacy Mac symbols', windows)

    def test_shared_paths_override_keywords_and_ios_only_is_excluded(self):
        self.commit('fix: Windows and macOS shared input', 'ChiaKey-Source/Frameworks/ChiaKeyCore/Source/ChiaKeyCore.cpp')
        self.commit('feat: iOS keyboard', 'ChiaKey-Source/Loaders/iOS/Keyboard.swift')
        self.commit('feat: Windows iOS bridge', 'ChiaKey-Source/Loaders/iOS/Bridge.swift')
        self.commit('fix: Unknown path fallback', 'unknown.txt')
        for platform in ['macos', 'windows']:
            notes = subprocess.check_output(
                [sys.executable, str(ROOT / 'Scripts/generate-release-notes.py'), '--platform', platform, '--since', 'v1.2.6'],
                cwd=self.repo, text=True)
            self.assertIn('Windows and macOS shared input', notes)
            self.assertIn('Unknown path fallback', notes)
            self.assertNotIn('iOS keyboard', notes)
            self.assertNotIn('iOS bridge', notes)

    def test_stable_notes_include_beta_series_and_no_repeat_after_release(self):
        self.commit('feat: Feature in beta', 'shared.cpp')
        self.git('tag', 'v1.2.7-beta.1')
        self.commit('fix: Later fix', 'shared.cpp')
        self.assertIn('Feature in beta', self.notes('--since', 'v1.2.6'))
        self.assertNotIn('Feature in beta', self.notes('--since', 'v1.2.7-beta.1'))
        self.assertIn('Later fix', self.notes('--since', 'v1.2.7-beta.1'))
        self.git('tag', 'v1.2.7')
        self.assertEqual(self.notes('--since', 'v1.2.7'), '')
        self.assertIn('Feature in beta', self.notes())

    def test_workflow_ignores_higher_windows_and_unmerged_mac_tags(self):
        self.git("tag", "win-v99.0.0-beta.1")
        original = self.git("rev-parse", "HEAD")
        self.git("checkout", "-qb", "future")
        self.commit("fix(mac): future work", "mac.m")
        self.git("tag", "v99.0.0")
        self.git("checkout", "-q", original)
        workflow = (ROOT / ".github/workflows/release.yml").read_text(encoding="utf-8")
        selection = next(line.strip() for line in workflow.splitlines() if line.strip().startswith('latest="'))
        latest = subprocess.check_output(
            ["bash", "-c", selection + '\n printf "%s" "$latest"'],
            cwd=self.repo, text=True,
        )
        self.assertEqual(latest, "v1.2.6")


if __name__ == "__main__":
    unittest.main()
