# Contributing

感謝你願意協助千秋輸入法。這個 repo 的維護目標偏保守：優先保留 Yahoo! 奇摩輸入法 / KeyKey 的輸入手感與歷史脈絡，同時讓專案能在現代 macOS、Xcode 與 Apple Silicon 上穩定編譯、測試、打包與發佈。

## 開發環境

需使用支援 Apple Silicon 的現代 Xcode；目前已在 Xcode 26.5 驗證。

這個 fork 的上游脈絡來自官方封存的 `YahooArchive/KeyKey`，目前的現代化工作則起始於 `vChewing/KeyKey-Boneyard` snapshot。

主要主線設定：

1. 目前發佈主線維護現代 macOS InputMethodKit 版本。
2. bundle id / TIS id 使用 `com.chiakey.inputmethod.ChiaKey`。
3. 使用者資料路徑使用 `~/Library/Application Support/ChiaKey`。
4. 詞庫由獨立 repo `ChiaKey-Lexicon` 透過 GitHub Releases 發佈。
5. 實驗性的 `ChiaKeyCore` host-neutral engine facade 作為 macOS / iOS 可共用的輸入核心地基。

## 編譯

歷史 target 目前可在 Xcode 26+ / Apple Silicon 上編譯。

已於 2026-06-22 使用 Xcode 26.5 驗證：

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

Release build 已可在現代 Xcode 完成。先前的 `LSMinimumSystemVersion`、legacy library search path、shell script phase output、舊 WebView、舊 TSM fallback 與 CommonPanels sheet API 警告已整理；這些調整多屬於建置設定與維護性清理。

乾淨 rebuild 仍會看到部分舊 API / nib 警告，例如 `NSConnection`、舊 AppKit 常數與既有 XIB layout notices；這些要分批評估，尤其 IPC 與組字視窗相關程式不應一次大換。

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

若本機沒有 `ChiaKey-Source/Distributions/Takao/CookedDatabase/ChiaKeySource.db`，dev install 不會從 raw source 重建 DB。請先使用詞庫 repo 產出的 release/local DB，或以 `--bundle-local-lexicon` / `--local-lexicon` 明確指定要包進 app 的 DB。

第一次安裝後，請到「系統設定 > 鍵盤 > 文字輸入」新增千秋輸入法。之後多數開發循環只需要跑 helper script，再切離與切回輸入法。

若 macOS 持續使用舊的 input source cache，登出再登入一次通常可以清掉。

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
Scripts/install-lexicon-release.sh --min-release-age-days 3 --skip-current
Scripts/install-lexicon-release.sh --dry-run
Scripts/install-lexicon-release.sh --keep-downloads
```

installer 會下載 `lexicon-manifest.json`，下載 manifest 宣告的 DB 與 metadata，驗證 SHA-256、SQLite table、符號表、全型標點與必要 metadata，通過後安裝到 versioned directory，最後以 atomic symlink swap 更新 `active`。

詞庫安裝後，切離再切回千秋輸入法，或重新執行 `Scripts/dev-install-local.sh`，讓 runtime 重新開啟 DB。

偏好設定 app 也提供詞庫更新頁面，並可開關自動更新詞庫。輸入法 runtime 啟動後會每天最多檢查一次詞庫；若 GitHub latest release 比目前安裝版本新，且 release 已發佈超過 3 天，會在背景靜默安裝並 reload runtime。失敗時會保留既有 active lexicon。

不下載、不安裝，只跑本機詞庫 smoke test：

```sh
Scripts/test-lexicon-smoke.sh
Scripts/test-lexicon-smoke.sh path/to/ChiaKeySource.db
```

驗證 host-neutral core facade 與 iOS SDK 語法相容性：

```sh
Scripts/test-core-smoke.sh
Scripts/test-ios-core-syntax.sh
```

驗證個人學習（LearningStore 淘汰策略、使用者詞庫 schema 遷移與舊版相容性、學過的候選能不能在 walker 存活）。自帶 SQLite fixture，不需要詞庫：

```sh
Scripts/test-learning-store.sh
```

驗證使用者詞庫匯入（`MJSR version 1.0.0` 匯出檔）。重點是 `<database>` 區塊：舊版 Yahoo! 奇摩輸入法用 SQLite SEE 加密，ChiaKey 自己匯出則是明文，Import 兩種都要吃。測試裡有兩組取自真實 KeyKey 匯出檔的 golden vector（只有 SQLite 檔頭與亂數 nonce，不含任何詞彙資料），改動金鑰推導、模式、IV 位置或 counter 規則都會被擋下來。自帶 fixture，不需要詞庫：

```sh
Scripts/test-user-phrase-import.sh
```

驗證偏好設定「匯入 Yahoo! 奇摩輸入法資料…」走的那條路（`PEUserPhraseStore`）：匯入的 unigram 機率會被正規化成跟手動新增的自訂詞同一條線（舊版的數值是對著另一套詞庫算的，而 `user_unigrams` 的機率是直接被讀的），以及學習快取中現行詞庫產不出來的項目會被丟掉。測試透過 `CFFIXED_USER_HOME` 導到暫存 home，**不會碰到你真正的使用者資料**；萬一導向失敗，測試會拒絕執行而不是寫進真實 profile：

```sh
Scripts/test-legacy-import.sh
```

驗證 Phrase Editor 匯入的檔案格式處理：export → import round trip（含四張 learning table）、缺 header、註解與空行、CRLF、legacy 機率正規化，以及匯入檔與內嵌 learning DB 的大小上限。跑兩份 binary，第二份把上限調低（`-DPE_MAX_IMPORT_FILE_SIZE` / `-DPE_MAX_LEARNING_BLOB_SIZE`），才不用為了測上限寫幾百 MB 到磁碟。同樣透過 `CFFIXED_USER_HOME` 導到暫存 home：

```sh
Scripts/test-phrase-editor-import.sh
```

量測智慧注音 walker 的 top-1 準確率。這不是 pass/fail 測試，是給「會動到排序的改動」用的比較工具（詞長加成、個人學習權重等），所以命名為 `eval-`：

```sh
Scripts/eval-walker-goldset.sh --corpus path/to/sentences.txt
Scripts/eval-walker-goldset.sh --gold goldset.tsv --length-prior 1.0
Scripts/eval-walker-goldset.sh --gold goldset.tsv --user-db ~/Library/Application\ Support/ChiaKey/SmartMandarinUserData.db
```

預設模式只讀 `--user-db`，所以上面第三行指向自己的真實學習資料庫是安全的。但 `--replay` 會**寫入** `--user-db`（它就是要量測學習行為），所以那個模式請指向副本；不給 `--user-db` 時它會用 TMPDIR 下的暫存 DB。

中文句子只給得出輸出，輸入的讀音序列必須反推，而每個多音字都是一次反推錯的機會 —— 讀音餵錯，walker 就不可能答對，那個誤差會被算在 walker 頭上。`--dominance` 控制這個取捨：預設 `0` 只收詞庫裡唯一讀音的字（完全無噪音，但句子少且偏短）；正值會額外接受「最高機率讀音領先次高 N 個 log10」的多音字（句子多很多，但部分讀音是推測的）。

實測兩者角色不同：嚴格集（1,182 句）偵測不到 ranking 改動的**傷害面** —— 詞長加成從 1.2 加到 2.5 準確率完全不動。`--dominance 1.0`（約 10,400 句）才有靈敏度，能重現詞長加成在 1.0 附近的最佳點。所以**絕對準確率只從 `--dominance 0` 報，調參用寬鬆集**，工具會同時印出無噪音子集的數字當對照。

gold set 是從本機語料現算的，不要 commit 進 repo：語料可能包含個人對話內容。

驗證 Manjusri 的 graph/node 查找（NodeSet 定位、前驅、重疊）與 Bopomofo 音節／鍵盤佈局（標準、倚天、倚天 26、許氏）往返轉換。自帶測資，不需要詞庫：

```sh
Scripts/test-manjusri-core.sh
```

iOS app + keyboard extension 可放在獨立 repo，並透過 `ChiaKeyCore` 接入共用輸入核心。若有對應的 iOS host project，請在該 repo 執行它自己的 Xcode build 驗證腳本。

## Release package

正式發佈使用 macOS Installer `.pkg`，預設走 per-user domain，安裝目的地是：

```text
~/Library/Input Methods/ChiaKey.app
```

若需系統全域安裝，請使用 CLI 命令：`sudo installer -pkg ChiaKey.pkg -target /`

建立本機 unsigned package：

```sh
Scripts/build-release-package.sh
```

預設輸出在：

```text
artifacts/release/
```

未提供 Installer signing identity 時，輸出檔名會加上 `-unsigned`，表示只適合本機測試，不應上傳為公開 release。

公開 release 應提供 Developer ID Application / Installer signing identities，並跑 notarization。細節請見 [Release packaging](Docs/ReleasePackaging.md)。

## 目錄概覽

- `ChiaKey-Source/Frameworks/ChiaKeyCore/`：host-neutral engine facade，未來 macOS / iOS 共用輸入核心邊界。
- `ChiaKey-Source/Frameworks/OpenVanilla/`：OpenVanilla framework source。
- `ChiaKey-Source/Frameworks/PlainVanilla/`：PlainVanilla bridge / loader policy。
- `ChiaKey-Source/Frameworks/Formosa/`：注音 syllable、鍵盤 layout 與 reading buffer。
- `ChiaKey-Source/Frameworks/Manjusri/`：SQLite-backed language model。
- `ChiaKey-Source/ModulePackages/OVIMMandarin/`：智慧注音 OpenVanilla module。
- `ChiaKey-Source/Loaders/OSX-IMK/`：目前發佈中的 macOS InputMethodKit host。
- `ChiaKey-Source/Loaders/iOS-Keyboard/`：iOS host placeholder；實際 iOS host 可放在獨立 repo。
- `ChiaKey-Source/Distributions/Takao/CookedDatabase/`：本機 bundled fallback DB 位置；DB 由詞庫 repo 或 release artifact 提供。
- `Scripts/`：本機 build、install、icon、lexicon helper。
- `Docs/`：現代化決策文件。

更完整的分層原則請見 [Docs/ProjectStructure.md](Docs/ProjectStructure.md)。

## 詞庫維護邊界

千秋輸入法 app repo 不維護語料來源，也不決定詞庫產製策略；這些工作由 ChiaKey-Lexicon repo 負責。app repo 只負責定義 runtime 需要的 DB contract、驗證 release artifact、安裝 / 更新 active lexicon，以及提供 bundled DB fallback。

歷史 KeyKey `DataSource` raw files 不再保存在 app repo。其 provenance 與 bootstrap DB 由 ChiaKey-Lexicon repo 管理；需要 bundled fallback DB 時，請使用詞庫 repo 產出的 release artifact 或本機 DB。詞庫格式與更新契約請見 [Docs/LexiconContract.md](Docs/LexiconContract.md)。
