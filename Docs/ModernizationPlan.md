# 千秋輸入法現代化 Roadmap

最後更新：2026-06-24

這份文件只追蹤工作順序，不是正式架構決策文件。

正式決策請看：

- [Architecture.md](Architecture.md)
- [LexiconContract.md](LexiconContract.md)

## 目前方向

千秋輸入法目前聚焦一個實際目標：讓 Yahoo KeyKey 的 macOS 輸入體驗能在現代 Apple Silicon Mac 上持續運作，並逐步替換脆弱的資料與 release pipeline。

目前策略：

1. 保留既有 InputMethodKit runtime。
2. 保留舊候選窗與組字行為，除非有可重現的 app bug 需要 workaround。
3. 詞庫資料由 `ChiaKey-Lexicon` 維護。
4. Generated lexicon DB 透過 GitHub Releases 發佈。
5. 每次詞庫更新都必須先驗證，再切換 active DB。
6. 偏好設定成為維護與診斷介面。
7. 清 legacy source tree 前先確認 active Xcode build graph。

## 已完成基線

目前已有可用的現代化基線：

1. `Takao-All` scheme 的 Debug / Release build 可在現代 Xcode 編譯。
2. Local install helper 會將 `ChiaKey.app` 安裝到 `~/Library/Input Methods`。
3. Bundle id 與 input source identity 已獨立於歷史 Yahoo KeyKey。
4. App 與 input-source icons 由 scripted vector assets 產生。
5. External lexicon release 可安裝到 Application Support。
6. Runtime 可從 external lexicon fallback 到 bundled DB。
7. 偏好設定可透過與 CLI 相同的 validation path 觸發詞庫更新。
8. OneKey 與 legacy Windows code 已從現代 app path 移除。
9. Locale tags 已針對現代 macOS 正規化。
10. GitHub fork 已 credit 官方 Yahoo archive upstream。
11. `ChiaKeyCore` host-neutral facade 已建立，並有 macOS smoke test 與 iPhoneOS syntax probe。
12. 舊 Yahoo runtime integrations、legacy dictionary panel、tracker、legacy installer pipeline、dead update/feed endpoints 與 standalone legacy helper projects 已移除。

## 下一步

### 1. 偏好設定維護介面

偏好設定應提供使用者真正需要的維護功能：

1. 目前 app version
2. 目前 lexicon version
3. 目前 lexicon install path
4. 檢查詞庫更新
5. 安裝更新
6. 重新載入輸入法
7. 開啟 Application Support folder
8. 複製診斷資訊

介面應安靜、工具導向。維護流程穩定前，不需要建立新的視覺系統。

### 2. 強化詞庫驗證

App 已能在安裝前驗證 release DB。下一步是讓這些檢查更容易與詞庫 repo 共用。

可加入：

1. 將 DB validation 抽成獨立 script。
2. 加入 CI 可用的 fixture 或 dry-run mode。
3. 驗證 symbol categories、punctuation keys、minimum table sizes。
4. 新 release 若含 forbidden OneKey data 則拒絕。
5. 新 metadata 在強制前先文件化。

### 3. Runtime smoke tests

改候選窗或 engine behavior 前，先補小型 manual / scripted smoke-test checklist。

必要行為：

1. 注音組字可產生 `你好`。
2. 候選字選取與取消可用。
3. `Shift+,` 產生 `，`。
4. 符號表可開啟且有分類。
5. Caps Lock 符合現代 native ASCII 行為。
6. 倉頡與簡易仍可載入。
7. App 切換後 composition state 乾淨。
8. invalid external lexicon 會 fallback 到 bundled DB。

重要 app targets：

1. TextEdit
2. Safari
3. Chrome
4. VS Code
5. Discord
6. Slack
7. Notion
8. Obsidian
9. Terminal

### 4. Legacy cleanup

Cleanup 應用小 commit 進行，且保持 `Takao-All` 可編譯。

好的下一批 candidates：

1. historical studies 與 internal documents
2. 產品沒有 exposes 的 unused extra modules
3. confirmed-unused helper utilities
4. 仍由 runtime 或偏好設定間接載入、但產品不再 exposes 的資料檔

Active input method path、倉頡、簡易不應只因為老就移除。

### 5. Release packaging

已建立 `Scripts/build-release-package.sh` 作為本機與 CI 共用的 `.pkg`
打包入口。正式 release 前仍需要補齊公開發佈環節：

1. Developer ID Application / Installer signing identities
2. notarization credential 與 CI keychain setup
3. tag push 後自動上傳 `.pkg` 到 GitHub Release
4. 視需要把 notarized `.pkg` 包進 `.dmg`
5. install / rollback 文件與 release smoke test

### 6. iOS-ready core boundary

先把 engine 與平台 host 的邊界固定住，未來保留實作空間。
實作細節集中在 [iOSImplementation.md](iOSImplementation.md)。

下一步：

1. 將 `ChiaKeyCore` 接成正式 Xcode library target 或 Swift Package wrapper。
2. 加 ObjC++ / Swift bridge，讓 Swift host 不直接 include OpenVanilla internals。
3. 以 XCTest 固定 `你好`、候選選字、退格、標點、commit acknowledgement。
4. 在獨立 iOS host repo 維護最小 keyboard extension shell。
5. 保持沒有 Full Access 時仍可使用 bundled DB 與 extension writable path。

## 延後事項

目前刻意不優先做：

1. 完整 Swift runtime rewrite
2. 完整 Rust engine rewrite
3. 大型 Electron-specific mitigation
4. 候選窗視覺重設計
5. 新 language model architecture
6. personal learning overhaul
7. iOS UI polish 與完整上架流程

這些之後可能值得做，但現在更需要測試、release 流程整理與穩定詞庫 pipeline。
