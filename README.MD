# 千秋輸入法 ChiaKey

千秋輸入法（ChiaKey）是一個以 Yahoo! 奇摩輸入法 / KeyKey / OpenVanilla 開源程式碼為基礎的現代 macOS 復興專案。

目標是保留 KeyKey 在繁體中文注音輸入上的手感，並讓專案能在現代 Apple Silicon Mac 上編譯、測試、維護與發佈。

這不是 Yahoo 官方產品。原始碼依 BSD-style 授權釋出；詳見 [LICENSE.txt](LICENSE.txt)。Yahoo! 名稱與貢獻者姓名不得在未取得書面同意的情況下用於背書衍生產品。

## 專案定位

千秋輸入法不是為了和所有現代注音輸入法競爭功能完整度而建立。這個專案的主要動機是紀念並延續 Yahoo! 奇摩輸入法 / KeyKey / OpenVanilla 這條歷史脈絡，盡可能保留 KeyKey 在 macOS 上的輸入手感，同時讓它能被現代工具鏈編譯、測試與維護。

如果你只是想找一套日常可用的現代注音輸入法，也可以先參考這些選擇：

1. macOS 內建注音輸入方式：不需要額外安裝，和系統整合度最高，適合需求單純、希望維持最少維護成本的使用者。
2. [小麥注音輸入法](https://github.com/openvanilla/McBopomofo)：長期維護的開源注音輸入法，架構較現代、專案焦點清楚，適合想要穩定第三方開源注音、但不特別追求 KeyKey 懷舊手感的使用者。
3. [vChewing 唯音輸入法](https://vchewing.github.io/) 與其 [macOS repo](https://github.com/vChewing/vChewing-macOS)：功能面非常完整，支援原生簡體與繁體注音、多種注音鍵盤與拼音輸入、Sandbox、安全防護、全字庫支援、繁簡分離詞庫、輸出轉換與許多進階偏好設定；適合希望有大量現代功能可用的 macOS 使用者。

千秋輸入法比較適合下列使用者：

1. 懷念 Yahoo! 奇摩輸入法 / KeyKey 的候選窗、組字與輸入節奏。
2. 想保存、研究或延續 KeyKey / OpenVanilla / Manjusri 這條開源輸入法 lineage。
3. 偏好保守、小步維護的復興專案，而不是功能面非常寬的現代輸入法。
4. 希望詞庫 release、SQLite schema、fallback 與驗證規則圍繞 KeyKey 原有語言模型逐步整理。

不同輸入法專案也會有不同的詞庫路線。千秋輸入法目前優先讓資料來源、授權邊界與 Yahoo KeyKey 相容性保持可追蹤，因此不直接把其他現代輸入法的詞庫當作 KeyKey 復刻的預設基準。

## 專案狀態

這個 fork 的上游脈絡來自官方封存的 `YahooArchive/KeyKey`，目前的現代化工作則起始於 `vChewing/KeyKey-Boneyard` snapshot。

- GitHub: <https://github.com/akira02/ChiaKey>
- 官方 Yahoo 封存: <https://github.com/YahooArchive/KeyKey>
- 最早 GitHub fork root: <https://github.com/Yi-Kai/KeyKey>
- Boneyard snapshot: <https://github.com/vChewing/KeyKey-Boneyard>

目前主線設定：

1. 專案中文名稱暫定為「千秋輸入法」。
2. `ChiaKey` 是 `Chiaki` 與 `KeyKey` 結合的諧音名稱。
3. 目前發佈主線維護現代 macOS InputMethodKit 版本。
4. bundle id / TIS id 使用 `com.chiakey.inputmethod.ChiaKey`。
5. 使用者資料路徑使用 `~/Library/Application Support/ChiaKey`。
6. 詞庫由獨立 repo `ChiaKey-Lexicon` 透過 GitHub Releases 發佈。
7. 新增實驗性的 `ChiaKeyCore` host-neutral engine facade，作為獨立 iOS host repo 可共用的輸入核心地基。

需使用支援 Apple Silicon 的現代 Xcode；目前已在 Xcode 26.5 驗證。

## 決策文件

- [架構決策](Docs/Architecture.md)：產品範圍、runtime 路線、legacy cleanup 原則與測試基線。
- [專案目錄結構](Docs/ProjectStructure.md)：source tree 分層、host 邊界與獨立 iOS host repo 關係。
- [iOS 實作指南](Docs/iOSImplementation.md)：iOS keyboard extension 的 core 接法、repo 邊界與驗證入口。
- [詞庫 contract](Docs/LexiconContract.md)：GitHub Release assets、SQLite 必要資料、驗證規則與 fallback 行為。
- [Release packaging](Docs/ReleasePackaging.md)：正式 `.pkg` installer、簽章與 notarization 流程。
- [現代化 roadmap](Docs/ModernizationPlan.md)：已完成基線、下一步與延後事項。
- [OneKey 移除說明](Docs/LexiconOneKeyRemoval.md)：給詞庫 repo 的相容性說明。

## 編譯

目前第一個現代化里程碑是讓歷史 target 能在 Xcode 26+ / Apple Silicon 上編譯。

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

目前 Release build 已可在現代 Xcode 完成。先前的 `LSMinimumSystemVersion`、legacy library search path、shell script phase output、舊 WebView、舊 TSM fallback 與 CommonPanels sheet API 警告已整理；這些調整多屬於建置設定與維護性清理。乾淨 rebuild 仍會看到部分舊 API / nib 警告，例如 `NSConnection`、舊 AppKit 常數與既有 XIB layout notices；這些要分批評估，尤其 IPC 與組字視窗相關程式不應一次大換。

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

## Release package

正式發佈使用 macOS Installer `.pkg`，安裝目的地是：

```text
/Library/Input Methods/ChiaKey.app
```

建立本機 unsigned package：

```sh
Scripts/build-release-package.sh
```

預設輸出在：

```text
artifacts/release/
```

公開 release 應提供 Developer ID Application / Installer signing identities，並跑 notarization。細節請見 [Release packaging](Docs/ReleasePackaging.md)。

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

驗證 host-neutral core facade 與 iOS SDK 語法相容性：

```sh
Scripts/test-core-smoke.sh
Scripts/test-ios-core-syntax.sh
```

iOS app + keyboard extension 可放在獨立 repo，並透過 `ChiaKeyCore` 接入共用輸入核心。若有對應的 iOS host project，請在該 repo 執行它自己的 Xcode build 驗證腳本。

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

更完整的分層原則請見 `Docs/ProjectStructure.md`。

## 詞庫維護邊界

千秋輸入法 app repo 不維護語料來源，也不決定詞庫產製策略；這些工作由 ChiaKey-Lexicon repo 負責。app repo 只負責定義 runtime 需要的 DB contract、驗證 release artifact、安裝 / 更新 active lexicon，以及提供 bundled DB fallback。

歷史 KeyKey `DataSource` raw files 不再保存在 app repo。其 provenance 與 bootstrap DB 由 ChiaKey-Lexicon repo 管理；需要 bundled fallback DB 時，請使用詞庫 repo 產出的 release artifact 或本機 DB。詞庫格式與更新契約請見 `Docs/LexiconContract.md`。
