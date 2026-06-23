# 千秋輸入法 ChiaKey

千秋輸入法（ChiaKey）是一個以 Yahoo! 奇摩輸入法 / KeyKey / OpenVanilla 開源程式碼為基礎的現代 macOS 復興專案。

目標是保留 KeyKey 在繁體中文注音輸入上的手感，並讓專案能在現代 Apple Silicon Mac 上編譯、測試、維護與發佈。

這不是 Yahoo 官方產品。原始碼依 BSD-style 授權釋出；詳見 [LICENSE.txt](LICENSE.txt)。Yahoo! 名稱與貢獻者姓名不得在未取得書面同意的情況下用於背書衍生產品。

## 專案狀態

這個 fork 的上游脈絡來自官方封存的 `YahooArchive/KeyKey`，目前的現代化工作則起始於 `vChewing/KeyKey-Boneyard` snapshot。

- GitHub: <https://github.com/akira02/ChiaKey>
- 官方 Yahoo 封存: <https://github.com/YahooArchive/KeyKey>
- 最早 GitHub fork root: <https://github.com/Yi-Kai/KeyKey>
- Boneyard snapshot: <https://github.com/vChewing/KeyKey-Boneyard>

目前主線定位：

1. 產品中文名稱為「千秋輸入法」。
2. 英文與技術名稱為 `ChiaKey`。
3. 只維護現代 macOS InputMethodKit 版本。
4. bundle id / TIS id 使用 `com.chiakey.inputmethod.ChiaKey`。
5. 使用者資料路徑使用 `~/Library/Application Support/ChiaKey`。
6. 詞庫由獨立 repo `ChiaKey-Lexicon` 透過 GitHub Releases 發佈。

本機開發環境註記：目前開發機有 Xcode 26.5。

## 決策文件

- [架構決策](Docs/Architecture.md)：產品範圍、runtime 路線、legacy cleanup 原則與測試基線。
- [詞庫 contract](Docs/LexiconContract.md)：GitHub Release assets、SQLite 必要資料、驗證規則與 fallback 行為。
- [現代化 roadmap](Docs/ModernizationPlan.md)：已完成基線、下一步與延後事項。
- [OneKey 移除說明](Docs/LexiconOneKeyRemoval.md)：給詞庫 repo 的相容性說明。

## 編譯

目前第一個現代化里程碑是讓歷史 target 能在 Xcode 26+ / Apple Silicon 上編譯。

本機曾於 2026-06-22 驗證：

```sh
xcodebuild -project ChiaKey-Source/Takao.xcodeproj \
  -scheme Takao-All \
  -configuration Debug \
  -derivedDataPath /private/tmp/ChiaKeyDerivedAll \
  CODE_SIGNING_ALLOWED=NO \
  ONLY_ACTIVE_ARCH=YES \
  build

xcodebuild -project ChiaKey-Source/Takao.xcodeproj \
  -scheme Takao-All \
  -configuration Release \
  -derivedDataPath /private/tmp/ChiaKeyDerivedRelease \
  CODE_SIGNING_ALLOWED=NO \
  ONLY_ACTIVE_ARCH=YES \
  build
```

目前 Release build 已可在現代 Xcode 完成。先前的 `LSMinimumSystemVersion`、legacy library search path、shell script phase output、舊 WebView、舊 TSM fallback 與 CommonPanels sheet API 警告已整理；這些調整多屬於 build hygiene 或維護性清理。乾淨 rebuild 仍會看到部分舊 API / nib 警告，例如 `NSConnection`、舊 AppKit 常數與既有 XIB layout notices；這些要分批評估，尤其 IPC 與組字視窗相關程式不應一次大換。

## 本機測試

日常開發請使用 local install helper，不需要每次重跑 installer：

```sh
Scripts/dev-install-local.sh
```

常用選項：

```sh
Scripts/dev-install-local.sh --configuration Release
Scripts/dev-install-local.sh --skip-build
Scripts/dev-install-local.sh --update-lexicon
Scripts/dev-install-local.sh --bundle-local-lexicon
Scripts/dev-install-local.sh --local-lexicon "/path/to/ChiaKeySource.db"
Scripts/dev-install-local.sh --dry-run
Scripts/dev-install-local.sh --open-settings
```

這個 script 會 build `Takao-All`，把 `ChiaKey.app` 複製到 `~/Library/Input Methods/`，進行 ad-hoc signing，並結束目前執行中的 `ChiaKey` process，讓 macOS 下次切回輸入法時載入新版。

`--update-lexicon` 會額外執行 `Scripts/install-lexicon-release.sh --skip-current`，只在外部詞庫落後時下載並安裝最新 release。若搭配 `--bundle-local-lexicon`，會先更新 active 詞庫再包進 app；其他情境會在本機安裝後更新外部詞庫。預設 build / install 不會連網。

`--bundle-local-lexicon` 會先驗證目前 active 的本機詞庫，再把 `~/Library/Application Support/ChiaKey/Lexicons/active/ChiaKeySource.db` 複製進 dev build 的 `ChiaKey.app/Contents/Resources/Databases/ChiaKeySource.db`。若要測另一份本機 DB，使用 `--local-lexicon` 指定路徑。這適合在詞庫 repo 本機產出 DB 後，直接測 bundled DB fallback；正式執行時外部 active 詞庫仍會優先於 app 內建詞庫。

第一次安裝後，請到「系統設定 > 鍵盤 > 文字輸入」新增千秋輸入法。之後多數開發循環只需要跑 helper script，再切離與切回輸入法。

若 macOS 持續使用舊的 input source cache，登出再登入一次通常可以清掉。

## 圖示

重新從 SVG 產生 app 與 input source `.icns`：

```sh
Scripts/generate-icon-assets.sh
```

目前圖示來源是：

```text
ChiaKey-Source/Loaders/OSX-IMK/Images/ChiaKey.svg
```

## 詞庫更新測試

千秋輸入法可以從下列路徑載入外部 Smart Mandarin DB：

```text
~/Library/Application Support/ChiaKey/Lexicons/active/ChiaKeySource.db
```

若外部 DB 缺失或不合法，runtime 會 fallback 到 app bundle 內建的 `ChiaKeySource.db`。

安裝目前預設詞庫 release：

```sh
Scripts/install-lexicon-release.sh
```

常用選項：

```sh
Scripts/install-lexicon-release.sh --tag 2026.06.7
Scripts/install-lexicon-release.sh --skip-current
Scripts/install-lexicon-release.sh --min-release-age-days 7 --skip-current
Scripts/install-lexicon-release.sh --dry-run
Scripts/install-lexicon-release.sh --keep-downloads
```

installer 會下載 `lexicon-manifest.json`，下載 manifest 宣告的 DB 與 metadata，驗證 SHA-256、SQLite table、符號表、全型標點與必要 metadata，通過後安裝到 versioned directory，最後以 atomic symlink swap 更新 `active`。

詞庫安裝後，切離再切回千秋輸入法，或重新執行 `Scripts/dev-install-local.sh`，讓 runtime 重新開啟 DB。

偏好設定 app 也提供詞庫更新頁面。輸入法 runtime 啟動後會每天最多檢查一次詞庫；若 GitHub latest release 比目前安裝版本新，且 release 已發佈超過 7 天，會在背景靜默安裝並 reload runtime。失敗時會保留既有 active lexicon。

不下載、不安裝，只跑本機詞庫 smoke test：

```sh
Scripts/test-lexicon-smoke.sh
Scripts/test-lexicon-smoke.sh path/to/ChiaKeySource.db
```

## 目錄概覽

- `ChiaKey-Source/`：原 Yahoo KeyKey source package。
- `ChiaKey-Source/Loaders/OSX-IMK/`：目前主要 macOS InputMethodKit runtime。
- `ChiaKey-Source/Frameworks/OpenVanilla/`：OpenVanilla framework source。
- `ChiaKey-Source/Frameworks/PlainVanilla/`：PlainVanilla bridge / loader policy。
- `ChiaKey-Source/Distributions/Takao/DataSource/`：歷史資料來源與目前 seed data。
- `Scripts/`：本機 build、install、icon、lexicon helper。
- `Docs/`：現代化決策文件。

## 詞庫來源方向

原始 Yahoo KeyKey 詞庫建置依賴已不適合現代 redistributable fork 的資料：

1. Sinica Corpus 需要另外取得授權。
2. Yahoo search terms 不是穩定或可重散佈的開放資料來源。
3. `sqlite-cerod-see` / CEROD 是商業 SQLite read-only compression extension。

千秋輸入法的詞庫應拆成四層：

1. 讀音 mapping。
2. 詞彙與短語。
3. 頻率 / language model。
4. 繁簡與區域用字轉換。

目前較適合研究的公開來源：

| 來源 | 用途 | 注意事項 |
| --- | --- | --- |
| libchewing | 注音行為、鍵盤配置、測試對照 | 適合作為行為 benchmark；直接引用資料前需審授權。 |
| Rime bopomofo / terra-pinyin / essay | 注音 schema、拼音 mapping、語料參考 | 多為 LGPL；shipping 前需確認相容性。 |
| CC-CEDICT | 詞條與拼音 | CC BY-SA 4.0；有讀音但缺使用頻率。 |
| moedict-data | 教育部辭典資料、讀音、釋義 | 原文字授權較敏感，適合先當參考。 |
| Wikimedia / Wiktionary dumps | 語料、標題、詞頻 | CC BY-SA / GFDL 相關義務需文件化。 |
| Tatoeba | 句子語料補充 | 規模較小，不適合單獨當主 language model。 |
| Unicode Unihan | 字級讀音、異體、部首筆畫 | 很適合做字級 metadata。 |
| CNS11643 全字庫 | 台灣字級資料、注音、倉頡、筆畫 | 台灣資料很有價值。 |
| OpenCC | 繁簡與區域用字轉換 | Apache-2.0；不是 ranking dictionary。 |

預設方向是：先避開私有或授權不明資料，以 CC-CEDICT、Unicode Unihan、CNS11643、Wikimedia/Tatoeba opt-in corpus、OpenCC 與本專案人工補詞建立可重現 pipeline。
