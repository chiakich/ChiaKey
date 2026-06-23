# Chiaki KeyKey

Chiaki KeyKey is a modern macOS revival experiment based on the open-sourced Yahoo! KeyKey / OpenVanilla codebase.

The goal is to preserve the excellent Traditional Chinese input experience of KeyKey while making the project buildable, auditable, and maintainable on current Apple Silicon Macs.

This is not an official Yahoo product. The upstream source was released under a BSD-style license; see [LICENSE.txt](LICENSE.txt). The Yahoo! name and contributor names may not be used to endorse derived products without written permission.

## Current Status

This fork traces its upstream lineage to the official Yahoo archive at `YahooArchive/KeyKey`, with the current modernization work starting from the `vChewing/KeyKey-Boneyard` snapshot. It is now hosted as:

- GitHub: <https://github.com/akira02/Chiaki-KeyKey>
- Official Yahoo archive: <https://github.com/YahooArchive/KeyKey>
- Original GitHub fork root: <https://github.com/Yi-Kai/KeyKey>
- Boneyard snapshot: <https://github.com/vChewing/KeyKey-Boneyard>

The repository currently contains the original Yahoo! KeyKey source tree plus previously repaired macOS utility targets:

- Preferences app target: `Takao -> Preferences`
- Phrase editor target: `Takao -> PhraseEditor`

The full input method target still needs modernization before it can be treated as a current macOS product. The important work is expected to be:

- Replace legacy build and packaging assumptions with an Xcode 26+ friendly project.
- Make the InputMethodKit host build cleanly for Apple Silicon.
- Remove old x86 / PPC / 10.5-era assumptions from `Info.plist`, event handling, helper code, and packaging.
- Rebuild the dictionary pipeline without Yahoo search-term data, private Sinica Corpus inputs, or the commercial CEROD SQLite extension.
- Add a reproducible data pipeline so generated dictionaries can be rebuilt from documented sources.

Local environment note: Xcode 26.5 is available on the current development machine.

## Building on Modern macOS

The first modern build milestone is to keep the historical targets compiling with Xcode 26+ on Apple Silicon.

Verified locally on 2026-06-22:

```sh
xcodebuild -project YahooKeyKey-Source-1.1.2528/Takao.xcodeproj \
  -scheme Takao-All \
  -configuration Debug \
  -derivedDataPath /private/tmp/ChiakiKeyKeyDerivedAll \
  CODE_SIGNING_ALLOWED=NO \
  ONLY_ACTIVE_ARCH=YES \
  build

xcodebuild -project YahooKeyKey-Source-1.1.2528/Takao.xcodeproj \
  -scheme Takao-All \
  -configuration Release \
  -derivedDataPath /private/tmp/ChiakiKeyKeyDerivedRelease \
  CODE_SIGNING_ALLOWED=NO \
  ONLY_ACTIVE_ARCH=YES \
  build
```

Both commands currently produce `Chiaki KeyKey.app`, `Preferences.app`, `PhraseEditor.app`, installer helper apps, and the OpenVanilla / PlainVanilla frameworks under the selected DerivedData products directory.

Regenerate the app and input-source `.icns` assets from the scripted vector icon drawing with:

```sh
Scripts/generate-icon-assets.sh
```

This build still emits many deprecation warnings from legacy AppKit, WebKit `WebView`, `NSConnection`, old nib loading, and installer-era file manager APIs. Those warnings are expected for now; the current milestone is successful compilation, not runtime certification or notarized distribution.

### Local Input Method Testing

For day-to-day development, use the local install helper instead of rebuilding a distribution installer:

```sh
Scripts/dev-install-local.sh
```

The script builds `Takao-All`, copies `Chiaki KeyKey.app` into `~/Library/Input Methods/`, ad-hoc signs the copied app, and kills any currently running `Chiaki KeyKey` process so macOS can launch the new build.

Useful variants:

```sh
Scripts/dev-install-local.sh --configuration Release
Scripts/dev-install-local.sh --skip-build
Scripts/dev-install-local.sh --dry-run
Scripts/dev-install-local.sh --open-settings
```

On the first local install, add the input method from System Settings > Keyboard > Text Input. After that, most edit/build/test cycles only need this helper script plus switching away from and back to the input method. If macOS keeps an old cached input source, log out and back in once.

The development bundle uses `Chiaki KeyKey.app` and `com.chiaki.inputmethod.ChiakiKeyKey`, so it can be installed next to the historical `Yahoo! KeyKey.app` / `com.yahoo.inputmethod.KeyKey` input source.

### Lexicon Release Testing

Chiaki KeyKey can load an external Smart Mandarin database from:

```text
~/Library/Application Support/Chiaki KeyKey/Lexicons/active/KeyKeySource.db
```

If that database is missing or invalid, the input method falls back to the bundled `KeyKeySource.db` inside the app.

Install the current seed lexicon release with:

```sh
Scripts/install-lexicon-release.sh
```

Useful variants:

```sh
Scripts/install-lexicon-release.sh --tag 2026.06.7
Scripts/install-lexicon-release.sh --dry-run
Scripts/install-lexicon-release.sh --keep-downloads
```

The installer reads `lexicon-manifest.json` from the GitHub Release, downloads the declared DB and metadata artifacts, verifies their SHA-256 checksums, validates the required SQLite tables, installs into a versioned directory, and atomically updates the `active` symlink.

After installing a lexicon, switch away from and back to Chiaki KeyKey, or rerun `Scripts/dev-install-local.sh`, so the input method process reopens the database.

The Preferences app also exposes a Lexicon update page. It checks the latest `Chiaki-KeyKey-Lexicon` GitHub Release and runs the bundled installer script, so the same checksum and SQLite schema validation path is used by both the UI and CLI.

OneKey service data is no longer part of the lexicon contract. See [LexiconOneKeyRemoval.md](Docs/LexiconOneKeyRemoval.md) before updating lexicon release checks.

## Source Layout

- `YahooKeyKey-Source-1.1.2528/` - original Yahoo! KeyKey source package.
- `YahooKeyKey-Source-1.1.2528/Loaders/OSX-IMK/` - legacy macOS InputMethodKit loader.
- `YahooKeyKey-Source-1.1.2528/Frameworks/OpenVanilla/` - OpenVanilla framework sources.
- `YahooKeyKey-Source-1.1.2528/Distributions/Takao/DatabaseCooker/` - original dictionary cooking scripts.

## Dictionary Replacement Survey

Last reviewed: 2026-06-22.

The original dictionary build expected data that is no longer a good fit for a redistributable modern fork:

- Sinica Corpus files require a separate license.
- Yahoo search terms are not a stable or redistributable open data source.
- `sqlite-cerod-see` / CEROD was a commercial SQLite read-only compression extension.

For Chiaki KeyKey, dictionary data should be split into four layers: pronunciation mappings, phrase vocabulary, frequency / language model data, and Traditional / Simplified / regional conversion data.

| Source | Useful For | License / Access Notes | Fit |
| --- | --- | --- | --- |
| [libchewing](https://github.com/chewing/libchewing) | Bopomofo behavior, keyboard layouts, engine comparison, possible data reference | GitHub mirror lists LGPL-2.1; the project has moved to Codeberg. Audit before vendoring data or code. | Strong reference implementation for Traditional Chinese phonetic input. Best used first as a behavior benchmark. |
| [Rime bopomofo](https://github.com/rime/rime-bopomofo), [terra-pinyin](https://github.com/rime/rime-terra-pinyin), [essay](https://github.com/rime/rime-essay) | Bopomofo schemas, pinyin mappings, shared vocabulary / language model | Repositories list LGPL-3.0. Bundling converted data may impose license obligations. | Very useful seed material, but should be kept behind a license audit before shipping. |
| [CC-CEDICT](https://www.mdbg.net/chinese/dictionary?page=cedict) | Traditional / Simplified word entries and pinyin | CC BY-SA 4.0. MDBG listed 125,045 entries in the 2026-06-21 release. | Good base word dictionary. It has readings, not usage frequency. |
| [moedict-data](https://github.com/g0v/moedict-data) | Taiwan Ministry of Education dictionary data, readings, definitions | README says original text remains MOE copyright under CC BY-ND 3.0 Taiwan, while the JSON conversion work is CC0. | Useful as a reference source. Be cautious about distributing modified derived dictionary text. |
| [Wikimedia dumps](https://dumps.wikimedia.org/zhwiki/latest/) and [Chinese Wiktionary dumps](https://dumps.wikimedia.org/zhwiktionary/latest/) | Corpus text, article titles, term frequency, Wiktionary lexical hints | Wikimedia text is generally CC BY-SA 4.0 / GFDL under the Terms of Use. Dumps are updated regularly. | Good for frequency and phrase mining after cleanup. Attribution/share-alike implications must be documented. |
| [Tatoeba downloads](https://tatoeba.org/en/downloads) | Mandarin sentences, sentence-level examples, small corpus supplement | Downloads page says files are CC BY 2.0 FR, with some sentences also available as CC0. | Useful supplemental corpus, not enough as the primary language model. |
| [Unicode Unihan](https://www.unicode.org/reports/tr38/) | Per-character readings, variants, radical/stroke metadata | Unicode License v3 permits use, copy, modification, publication, distribution, and sale with notice. | Excellent low-level character metadata; not a phrase dictionary. |
| [CNS11643 全字庫](https://data.gov.tw/dataset/5961) | Taiwan character metadata: Zhuyin, Cangjie, strokes, radicals, pinyin, components, stroke order | Government Open Data License v1 and OFL-1.1 for font assets. Dataset updated 2026-05-05. | Strong Taiwan-specific character metadata source. |
| [OpenCC](https://github.com/BYVoid/OpenCC) | Traditional / Simplified conversion, Taiwan / Hong Kong regional variants | Apache-2.0. | Useful conversion layer; not a ranking dictionary. |

### Recommended Data Direction

The safest first build should avoid private or hard-to-redistribute data. A practical starting combination is:

1. Use CC-CEDICT for base words and pinyin readings.
2. Use Unicode Unihan and CNS11643 for character-level readings and metadata.
3. Use Wikimedia and Tatoeba as opt-in corpus inputs for phrase frequency and language model training.
4. Use OpenCC for conversion and regional variant normalization.
5. Compare behavior against libchewing and Rime before deciding whether to reuse any LGPL-covered data directly.

This gives us a reproducible, public-data path without depending on the original Yahoo search terms, Sinica Corpus files, or CEROD.

## Modernization Roadmap

The likely path is not a full rewrite of the Chinese input algorithm on day one. A more realistic sequence is:

1. Keep the OpenVanilla / KeyKey core readable and buildable.
2. Modernize the macOS InputMethodKit shell for Apple Silicon.
3. Replace deprecated or removed macOS APIs only where they block building, signing, packaging, or runtime behavior.
4. Build a new dictionary cooker with explicit source licenses and deterministic outputs.
5. Add smoke tests for composition, candidate selection, punctuation, preference loading, and user phrase editing.
6. Package as a signed modern `.app` / input method bundle once the loader is stable.

## Original Upstream Notes

The original README described Yahoo! KeyKey as a customized Chinese input method based on OpenVanilla, released as open source under a New BSD license.

It also listed the historical external dependencies:

1. Sinica Corpus - separate license from Academia Sinica.
2. BPMF mappings created from Sinica Corpus.
3. Yahoo search terms from `tw.yahoo.com`.
4. Commercial SQLite CEROD extension.
5. Apple development kit tools such as DotMacKit.framework and PackageMaker.app.
6. CTGradient under Creative Commons BY 2.5.

Those notes are preserved here for provenance, but they should not be treated as the data plan for Chiaki KeyKey.
