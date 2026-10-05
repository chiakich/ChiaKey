# Windows upgrade cleanup and settings startup

Manual and automatic app updates both run `Packaging/Windows/ChiaKey.iss`.
Each release has its own directory because applications may still have the
previous TSF DLL loaded. Previously, `DelTree` left locked files behind without
registering their removal on reboot.

After registration of the new release, obsolete installation directories are
renamed to `obsolete-<version>-<timestamp>`. Cleanup deletes unlocked files and
uses Inno Setup `RestartReplace(path, '')` for locked files, followed by their
parent directories. Renaming prevents pending deletions from affecting a
subsequent reinstall of the same version before reboot. If Windows refuses the
directory rename, the installer logs it and retries on a later upgrade.
Directory junctions are not traversed. The current release is retained.

Settings initialization keeps the content panel, toolbar and nested preference
layouts suspended until stored values are populated. The update pane reads the
lexicon version on a worker thread when that pane is opened, instead of reading
it synchronously in the constructor.
Word-count exports resolve the desktop data directory directly instead of
initializing the entire input engine just to read or clear counters.

The update page requests the latest published version for the selected channel
even when it is equal to or older than the installed release. Only a newer
version enables installation; background checks keep their existing newer-only
behavior. A channel with no release displays "No release available" instead of
substituting the installed version. Lexicon recovery completes before reading
the current version, so the page reflects the recovered activation pointer.
Installed local build labels such as `0.1.0-beta.2.pr18.20261005` keep their full
display name but compare using `0.1.0-beta.2`. Remote release tags remain strictly
validated. This fixes the "invalid update version format" error for local builds.

The phrase editor's Start menu shortcut explicitly uses the purple
`phrase-editor.ico`, which is now copied into the release payload and installed.
Settings, phrase editor and dictionary processes set separate AppUserModelIDs
matching their shortcuts, so Windows does not group these modes under the same
settings executable identity. Existing pinned shortcuts may need to be unpinned
and pinned again after upgrading.

Validation on Windows:

- Full x64 Release compilation passed; all nine CTest checks passed.
- Existing layout checks passed in zh-TW, zh-CN and en across 100/150/200% scaling
  and 600/720/900px widths (576 text checks per locale).
- Initial constructor observations: existing executable 5.18 s; modified
  standalone executable 2.43–3.22 s. Full builds in parallel tests varied between
  3.49 and 5.76 s. These observations use different payloads and CPU loads and
  do not establish an end-to-end startup speedup ratio.
- Inno Setup compilation passed using existing local x64/x86 payloads.
- All 117 offline update regression checks passed, including actual native
  lexicon validation and local build version labels.
- Actual upgrade with locked DLL/database files and reboot cleanup still needs
  an installed-system integration check. No test installer was installed.
- Start menu/taskbar icon behavior still needs visual validation after installing
  the updated package; compilation verifies the icon payload and shortcut syntax.
