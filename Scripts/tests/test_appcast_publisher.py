#!/usr/bin/env python3
"""Run the real publishing shell script against local curl/gh fixtures."""
import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

CURL = '''#!/usr/bin/env python3
import os, sys
from pathlib import Path
args = sys.argv[1:]
root = Path(os.environ['PUBLISH_FIXTURE_ROOT'])
method = args[args.index('-X') + 1]
key = args[-1].split('/chiakey/')[1]
with (root / 'calls').open('a') as out: out.write(method + ' ' + key + '\\n')
path = root / 'objects' / key
if method == 'GET':
    destination = Path(args[args.index('-o') + 1])
    code = '503' if os.environ.get('PUBLISH_FIXTURE_FAIL') == key else ('200' if path.exists() else '404')
    destination.write_bytes(path.read_bytes() if code == '200' else b'')
    sys.stdout.write(code)
else:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(Path(args[args.index('--upload-file') + 1]).read_bytes())
'''


class PublisherTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        tools = self.root / 'bin'; tools.mkdir()
        (tools / 'curl').write_text(CURL); (tools / 'curl').chmod(0o755)
        (tools / 'gh').write_text('#!/bin/sh\nprintf "2026-10-02T00:00:00Z\\n"\n'); (tools / 'gh').chmod(0o755)
        self.release = self.root / 'release'; self.release.mkdir()
        (self.release / 'ChiaKey-1.2.7.pkg').write_bytes(b'mac')
        (self.release / 'ChiaKey-Windows-1.2.7-Setup.exe').write_bytes(b'MZwindows')
        for platform in ['macos', 'windows']:
            (self.release / f'release-notes-{platform}.md').write_text('- ' + platform)
        self.env = dict(os.environ, PATH=str(tools) + os.pathsep + os.environ['PATH'],
            PUBLISH_FIXTURE_ROOT=str(self.root), R2_S3_ENDPOINT='https://example.invalid',
            R2_BUCKET='fixture', R2_ACCESS_KEY_ID='fixture', R2_SECRET_ACCESS_KEY='fixture',
            R2_PUBLIC_BASE_URL='https://cdn.chiaki.ch', TAG='v1.2.7',
            GITHUB_REPOSITORY='chiakich/ChiaKey', RELEASE_DIR=str(self.release))

    def publish(self):
        return subprocess.run(['bash', str(ROOT / 'Scripts/publish-joint-appcast.sh')],
                              env=self.env, capture_output=True, text=True)

    def test_publish_separate_feeds_legacy_projection_and_retry(self):
        result = self.publish()
        self.assertEqual(result.returncode, 0, result.stderr)
        objects = self.root / 'objects'
        legacy = json.loads((objects / 'appcast.json').read_text())
        mac = json.loads((objects / 'updates/macos/appcast.json').read_text())
        win = json.loads((objects / 'updates/windows/appcast.json').read_text())
        self.assertEqual(legacy, mac)
        self.assertTrue(mac['stable']['package_url'].endswith('.pkg'))
        self.assertTrue(win['stable']['package_url'].endswith('-Setup.exe'))
        self.assertIn('/macos/', mac['stable']['notes_url'])
        self.assertIn('/windows/', win['stable']['notes_url'])
        result = self.publish()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(legacy, json.loads((objects / 'appcast.json').read_text()))

    def test_server_error_does_not_initialize_or_write_any_pointer(self):
        self.env['PUBLISH_FIXTURE_FAIL'] = 'appcast.json'
        result = self.publish()
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn('PUT ', (self.root / 'calls').read_text())

    def test_corrupt_existing_feed_is_preserved(self):
        objects = self.root / 'objects'; objects.mkdir()
        (objects / 'appcast.json').write_text('{corrupt')
        result = self.publish()
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual((objects / 'appcast.json').read_text(), '{corrupt')
        self.assertNotIn('PUT ', (self.root / 'calls').read_text())


if __name__ == '__main__':
    unittest.main()
