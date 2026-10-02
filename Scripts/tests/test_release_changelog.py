#!/usr/bin/env python3
"""Exercise platform filtering against a temporary Git history."""
import json
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
            ["python3", str(ROOT / "Scripts/generate-release-notes.py"), "--platform", "macos", *args],
            cwd=self.repo, text=True,
        )

    def fragment(self, name, changes):
        path = self.repo / 'ReleaseNotes' / name
        path.parent.mkdir(exist_ok=True)
        path.write_text(json.dumps({'changes': changes}))
        self.git('add', '.')
        self.git('commit', '-qm', 'docs: add platform notes')

    def test_platforms_and_internal_commits(self):
        self.commit('feat(win): Windows update', 'windows.cpp')
        self.fragment('one.json', [
            {'platforms': ['macos'], 'type': 'fix', 'description': 'Mac focus'},
            {'platforms': ['windows'], 'type': 'feat', 'description': 'Windows output'},
            {'platforms': ['macos', 'windows'], 'type': 'fix', 'description': 'Shared fix'},
        ])
        notes = self.notes('--since', 'v1.2.6')
        self.assertIn('Mac focus', notes)
        self.assertIn('Shared fix', notes)
        self.assertNotIn('Windows output', notes)
        self.assertNotIn('Windows update', notes)

    def test_stable_notes_include_beta_series_and_no_repeat_after_release(self):
        self.fragment('one.json', [{'platforms': ['macos'], 'type': 'feat', 'description': 'Feature in beta'}])
        self.git('tag', 'v1.2.7-beta.1')
        self.fragment('two.json', [{'platforms': ['macos'], 'type': 'fix', 'description': 'Later fix'}])
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
        workflow = (ROOT / ".github/workflows/release.yml").read_text()
        selection = next(line.strip() for line in workflow.splitlines() if line.strip().startswith('latest="'))
        latest = subprocess.check_output(
            ["bash", "-c", selection + '\n printf "%s" "$latest"'],
            cwd=self.repo, text=True,
        )
        self.assertEqual(latest, "v1.2.6")


if __name__ == "__main__":
    unittest.main()
