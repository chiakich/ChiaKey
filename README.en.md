[繁體中文](README.MD) · English

# ChiaKey (千秋輸入法)

<img width="256" height="256" alt="ChiaKey (千秋輸入法)" src="Docs/images/chiakey-icon.png" />

<img height="400" alt="Image" src="https://github.com/user-attachments/assets/c43697e2-e22e-4c08-95eb-1cfe36028774" />

ChiaKey is a Traditional Chinese Bopomofo input method for macOS. It is built on the open-sourced code of Yahoo! KeyKey (奇摩輸入法) / OpenVanilla, with the goal of preserving KeyKey's familiar typing feel and keeping this input engine usable and maintained on modern macOS. A Windows version is available as a preview — see [Windows (Preview)](#windows-preview) below.

Official website: [ChiaKey](https://chiaki.ch/works/chiakey)
Related article: [It's 2026 — why is anyone still writing an input method?](https://chiaki.ch/blog/writing-an-input-method-in-2026)
Media coverage: [free.com.tw — "Yahoo! KeyKey can't run on macOS 27? ChiaKey takes over for free" (in Traditional Chinese)](https://free.com.tw/chiakey/)

This project focuses on fixing and updating the input method itself; the lexicon data comes from the [ChiaKey Lexicon](https://github.com/chiakich/ChiaKey-Lexicon). Please report missing phrases or wrong candidates in that repository.

This is not an official Yahoo product. The source code is released under a BSD-style license; see [LICENSE](LICENSE). The Yahoo! name and the names of its contributors may not be used to endorse derivative products without prior written consent.

> If you enjoy ChiaKey and can spare it, sponsorship is welcome! Donations go toward the Apple Developer Program annual fee.

#### For Taiwan credit cards / Apple Pay / ATM / convenience-store payment codes:

[ECPay sponsorship link](https://p.ecpay.com.tw/2A3B186)

#### For everywhere else:

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/A0A21UAIV9)

## Download

- [Download the latest macOS installer (.pkg)](https://cdn.chiaki.ch/chiakey/ChiaKey.pkg)
- [Browse all releases](https://github.com/chiakich/ChiaKey/releases/)

The installer installs into the current user's `~/Library/Input Methods`.
If the input method isn't active after the first installation, add "ChiaKey" under "System Settings > Keyboard > Text Input". If the system still shows an old version or can't find the input method after installing, signing out and back in usually resolves it.

Note: for a global installation (usable by every account on this Mac) or batch deployment with MDM, use the command line:

```sh
sudo installer -pkg ChiaKey.pkg -target /
```

This installs into `/Library/Input Methods` and requires administrator privileges.

### Migrating from Yahoo! KeyKey

If this Mac still has Yahoo! KeyKey installed, open ChiaKey Preferences, go to the Vocabulary Management (詞彙管理) tab and click "Import Yahoo! KeyKey data…" to bring over your old user phrases, learned selection records, canned messages and .cin tables.

The old lexicon file is encrypted and only the old input method itself can read it, so ChiaKey asks the old program to export the data itself. This means the old program must still be able to run: it has no Apple silicon build, so on M-series Macs it needs Rosetta 2. The export also depends on the running old input method responding — if the button appears to do nothing, switch to Yahoo! KeyKey, type any character to wake it up, then come back and press it again.

Importing never overwrites existing canned messages or table files; duplicate user phrases are skipped.

### Uninstalling

In ChiaKey Preferences, open the Update (更新) tab and click "Remove ChiaKey…". By default your user phrases and settings are kept (so a later reinstall picks up where you left off); tick "Also delete user phrases and settings" to remove everything.

You can also run the uninstall script directly:

```sh
Scripts/uninstall.sh
```

Add `--purge` to delete user phrases, the lexicon and settings as well. A global installation (`/Library/Input Methods`) additionally needs `sudo rm -rf '/Library/Input Methods/ChiaKey.app'`.

## Windows (Preview)

The Windows version is currently a preview, versioned with a `win-v` prefix and published as a Pre-release on [GitHub Releases](https://github.com/chiakich/ChiaKey/releases). Download `ChiaKey-Windows-<version>-Setup.exe` and run it.

- Supports Windows 10 / 11 on x64; both 64-bit and 32-bit apps can type with it. ARM64 is not supported yet.
- Apps that are already open need to be restarted to load the input method. If ChiaKey doesn't appear in your input method list, add "Chinese (Traditional, Taiwan)" under "Settings > Time & language > Language & region".
- The current installer is **unsigned** (see the [Code signing policy](#code-signing-policy) below). SmartScreen will show a warning on first run — choose "More info > Run anyway". Machines with Smart App Control enabled will block it outright for now.
- To uninstall, use "Settings > Apps > Installed apps" → "ChiaKey". User phrases, learning data and settings live in `%APPDATA%\ChiaKey` and are kept when you uninstall.
- "Preferences > Update" can check for and install app and lexicon updates, each with its own opt-in auto-update (off by default). When enabled it checks daily and only auto-updates releases that have been out for three days; the app update needs a Windows privilege confirmation. The lexicon is switched at composition end after validation, keeping the old one if validation fails. Update settings and external lexicons live in `%APPDATA%\ChiaKeyUpdates`.

## Code signing policy

Free code signing provided by [SignPath.io](https://signpath.io/), certificate by
[SignPath Foundation](https://signpath.org/) — applied for, not yet granted. Until it is,
the Windows release files are unsigned.

- **Roles.** Committers and reviewers: [@chiakich](https://github.com/chiakich).
  Approvers: [@chiakich](https://github.com/chiakich).
- **What is signed.** The Windows text service (`ChiaKeyTsf.dll`, 64-bit and 32-bit), the
  settings and phrase editor app (`ChiaKeySettings.exe`) and the installer, all built by
  [GitHub Actions](.github/workflows/release-windows.yml) from the source in this repository.
  No third-party binaries are redistributed.
- **Privacy.** Typing remains offline. The separate update helper connects to ChiaKey's
  GitHub releases and lexicon CDN only when checking or downloading updates; automatic
  updates are opt-in. It never sends typed text, phrases, or learning data. Your phrases
  and learning stay in `%APPDATA%\ChiaKey`; update state stays in `%APPDATA%\ChiaKeyUpdates`.

## Who is this for

ChiaKey was not created to compete with every modern phonetic input method on feature completeness. The project's main motivation is to commemorate and continue the Yahoo! KeyKey / OpenVanilla lineage, to preserve KeyKey's macOS typing rhythm and candidate window experience as much as possible, and to reconstruct the bigram language model that was never released back then.

It is a good fit for these users:

1. You miss Yahoo! KeyKey's candidate window, composition and typing rhythm.
2. You want to preserve, study or continue the KeyKey / OpenVanilla open source input engine.
3. You prefer a conservative, small-steps revival project rather than a feature-packed modern IME.
4. You would like lexicon releases, the SQLite schema, fallback and validation rules to keep evolving around KeyKey's language model architecture.

If you are looking for an everyday, more feature-complete modern Bopomofo input method, also consider:

1. The built-in macOS Zhuyin input source: no extra installation, the deepest system integration, good for simple needs and minimal maintenance.
2. [McBopomofo (小麥注音)](https://github.com/openvanilla/McBopomofo): an open source Bopomofo input method long maintained by the OpenVanilla community, sharing ancestry with Yahoo! KeyKey, modern in architecture and clear in focus.
3. [vChewing (唯音輸入法)](https://vchewing.github.io/) and its [macOS repo](https://github.com/vChewing/vChewing-macOS): feature-complete, with support for multiple keyboard layouts, Pinyin, Sandbox, security hardening, output conversion and many advanced preferences.

## The lexicon and the bigram model

Much of Yahoo! KeyKey's famed conversion quality came from a bigram language model — it decided which character makes sense next after the previous word, among homophones. That bigram model was never open-sourced with the code, so it could not be reused.

ChiaKey's bigram is rebuilt from scratch: starting from existing open lexicons, layering Mozilla Common Voice sentence data and Taiwan colloquial corpus generated by large language models — seeded on the topic distribution of the Academia Sinica Balanced Corpus — then reviewed and refined, produced through a reproducible pipeline with traceable sources.

This is the part of the project with the deepest investment. The full data layers, source review and integration pipeline are documented in the [ChiaKey Lexicon](https://github.com/chiakich/ChiaKey-Lexicon) repository.

## Importing custom input methods (.cin tables)

Beyond Bopomofo, Cangjie and Simplex, ChiaKey keeps OpenVanilla's generic table engine and can load `.cin` mapping tables. Code-driven lookup input methods — Cangjie variants, Array, Dayi, Cantonese romanisation, Hakka, Taiwanese Hokkien and more — all come in this format.

In "ChiaKey Preferences > Custom Input Methods", press **+** and choose a `.cin` file to import. Before importing, it shows the table name, entry count and selection keys for confirmation; legacy Big5-encoded tables are converted to UTF-8 automatically. Once imported, the input method appears in the input menu, and you can untick it in the "General" tab to hide it from the menu.

Select a table in the list and press **−** to remove it. Only tables you imported are listed here; the four built-in input methods are unaffected.

Table files are stored in:

```
~/Library/Application Support/ChiaKey/DataTables/Generic/
```

You can also drop `.cin` files into that folder directly (alphanumeric filenames only), then sign out and back in for them to take effect.
Note: do not put files inside the app bundle — that invalidates the app's code signature, and the files would be overwritten on update.

Tables can be obtained from [chinese-opendesktop/cin-tables](https://github.com/chinese-opendesktop/cin-tables). The repository as a whole is CC0, but individual files may carry their own licensing notices — for example, Cantonese `cantonhk.cin` states GPL in its header, while `jyutping.cin` (Jyutping), `ile.cin` (Cantonese romanisation) and `zyujam.cin` (Cantonese Bopomofo) are marked Public Domain. Check each file's own license statement before use.

## Current status

This fork's upstream lineage comes from the official `YahooArchive/KeyKey`; the current modernization work started from the `vChewing/KeyKey-Boneyard` snapshot.

- Official Yahoo archive: <https://github.com/YahooArchive/KeyKey>
- Earliest GitHub fork root: <https://github.com/Yi-Kai/KeyKey>
- Boneyard snapshot: <https://github.com/vChewing/KeyKey-Boneyard>

Note:

- `ChiaKey` is a homophone pun combining `Chiaki` and `KeyKey`.
- The current release mainline maintains the modern macOS InputMethodKit version.
- bundle id / TIS id: `com.chiakey.inputmethod.ChiaKey`.
- User data path: `~/Library/Application Support/ChiaKey`.
- The lexicon is published from the separate `ChiaKey-Lexicon` repository.
- An experimental `ChiaKeyCore` host-neutral engine facade serves as the shared input-core foundation for a standalone iOS host repository.

Requires a modern Xcode with Apple silicon support; currently verified on Xcode 26.5.

## Documentation

- [CONTRIBUTING.md](CONTRIBUTING.md): building, testing, release packaging, lexicon update testing and maintenance boundaries.
- [Architecture](Docs/Architecture.md): product scope, runtime routes, legacy cleanup principles and the test baseline.
- [Project structure](Docs/ProjectStructure.md): source tree layering, host boundaries and the standalone iOS host repo relationship.
- [iOS implementation guide](Docs/iOSImplementation.md): how the iOS keyboard extension wires up the core, repo boundaries and verification entry points.
- [Windows implementation guide](Docs/WindowsImplementation.md): TSF front-end architecture decisions, porting constraints and build instructions.
- [Lexicon contract](Docs/LexiconContract.md): GitHub release assets, required SQLite data, validation rules and fallback behaviour.
- [Release packaging](Docs/ReleasePackaging.md): the official `.pkg` installer, signing and notarization flow.
- [Modernization roadmap](Docs/ModernizationPlan.md): completed baseline, next steps and deferred items.
- [OneKey removal notes](Docs/LexiconOneKeyRemoval.md): compatibility notes for the lexicon repository.
