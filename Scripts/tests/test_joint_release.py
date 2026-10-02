#!/usr/bin/env python3
import importlib.util
import json
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def module(name, filename):
    spec = importlib.util.spec_from_file_location(name, ROOT / 'Scripts' / filename)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


feeds = module('feeds', 'update-appcast.py')
assembly = module('assembly', 'assemble-joint-release.py')


def entry(tag, platform='macos'):
    suffix = '.pkg' if platform == 'macos' else '-Setup.exe'
    return {'tag': tag, 'prerelease': '-beta.' in tag, 'sha256': 'a' * 64,
            'package_name': 'ChiaKey-' + tag[1:] + suffix,
            'package_url': 'https://example.invalid/' + tag + suffix,
            'published_at': '2026-10-02T00:00:00Z'}


class JointReleaseTests(unittest.TestCase):
    def test_beta_preserves_stable_and_late_stable_preserves_newer_beta(self):
        current = {'schema': 1, 'stable': entry('v1.2.6'), 'beta': entry('v1.2.6')}
        beta = feeds.merge(current, entry('v1.2.8-beta.1'), 'macos')
        result = feeds.merge(beta, entry('v1.2.7'), 'macos')
        self.assertEqual(result['stable']['tag'], 'v1.2.7')
        self.assertEqual(result['beta']['tag'], 'v1.2.8-beta.1')
        self.assertEqual(current['stable']['tag'], 'v1.2.6')

    def test_bad_feed_platform_downgrade_and_replaced_binary_rejected(self):
        for bad in [{}, {'schema': 2}, {'schema': 1, 'platform': 'windows'},
                    {'schema': 1, 'stable': entry('v1.2.7-beta.1')}]:
            with self.assertRaises(ValueError):
                feeds.merge(bad, entry('v1.2.7'), 'macos')
        current = feeds.merge({'schema': 1}, entry('v1.2.7'), 'macos')
        with self.assertRaises(ValueError):
            feeds.merge(current, entry('v1.2.6'), 'macos')
        changed = entry('v1.2.7'); changed['sha256'] = 'b' * 64
        with self.assertRaises(ValueError):
            feeds.merge(current, changed, 'macos')

    def test_legacy_fields_remain_flat_and_windows_is_separate(self):
        mac = feeds.merge({'schema': 1}, entry('v1.2.7'), 'macos')
        win = feeds.merge({'schema': 1}, entry('v1.2.7', 'windows'), 'windows')
        for field in ['tag', 'package_name', 'package_url', 'sha256', 'published_at', 'prerelease']:
            self.assertIn(field, mac['stable'])
        self.assertTrue(mac['beta']['package_name'].endswith('.pkg'))
        self.assertTrue(win['stable']['package_name'].endswith('-Setup.exe'))

    def test_no_release_body_until_both_matching_artifacts_exist(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'ChiaKey-1.2.7.pkg').write_bytes(b'mac')
            for platform in ['macos', 'windows']:
                (root / f'notes-{platform}.md').write_text('- ' + platform)
            with self.assertRaises(ValueError):
                assembly.assemble(root, 'v1.2.7')
            (root / 'ChiaKey-Windows-1.2.7-Setup.exe').write_bytes(b'MZwin')
            (root / 'extra-macos.md').write_text('- Manual Mac reminder')
            (root / 'summary-macos.md').write_text('- AI summary')
            assembly.assemble(root, 'v1.2.7')
            self.assertIn('Manual Mac reminder', (root / 'release-notes-macos.md').read_text())
            self.assertNotIn('Manual Mac reminder', (root / 'release-notes-windows.md').read_text())
            self.assertIn('macOS', (root / 'RELEASE_NOTES.md').read_text())
            self.assertIn('Windows', (root / 'RELEASE_NOTES.md').read_text())
            self.assertNotIn('- windows', (root / 'release-notes-macos.md').read_text())
            self.assertEqual(len((root / 'SHA256SUMS.txt').read_text().splitlines()), 2)


if __name__ == '__main__':
    unittest.main()
