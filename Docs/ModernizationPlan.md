# 千秋輸入法現代化 Roadmap

最後更新：2026-07-17

這份文件只追蹤工作順序，正式決策請看 [Architecture.md](Architecture.md) 與 [LexiconContract.md](LexiconContract.md)。

## 目前方向

讓 Yahoo KeyKey 的 macOS 輸入體驗能在現代 Apple Silicon Mac 上持續運作，並逐步替換脆弱的資料與 release pipeline。策略：保留既有 InputMethodKit runtime；保留舊候選窗與組字行為，除非有可重現的 app bug 需要 workaround；詞庫資料由 `ChiaKey-Lexicon` 維護，透過 GitHub Releases 發佈，每次更新先驗證再切換 active DB；偏好設定成為維護與診斷介面；清 legacy source tree 前先確認 active Xcode build graph。

## 已完成基線

- `Takao-All` scheme 的 Debug/Release build 可在現代 Xcode 編譯。
- Local install helper 會把 `ChiaKey.app` 安裝到 `~/Library/Input Methods`。
- Bundle id 與 input source identity 已獨立於歷史 Yahoo KeyKey。
- App 與 input-source icons 由 scripted vector assets 產生。
- External lexicon release 可安裝到 Application Support，runtime 可 fallback 到 bundled DB。
- 偏好設定可透過與 CLI 相同的 validation path 觸發詞庫更新。
- OneKey 與 legacy Windows code 已從現代 app path 移除；locale tags 已針對現代 macOS 正規化。
- GitHub fork 已 credit 官方 Yahoo archive upstream。
- `ChiaKeyCore` host-neutral facade 已建立，有 macOS smoke test 與 iPhoneOS syntax probe。
- 舊 Yahoo runtime integrations、legacy dictionary panel、tracker、legacy installer pipeline、dead update/feed endpoints、standalone legacy helper projects 已移除。
- 2026-07：詞彙編輯器（PhraseEditor）改寫為直連 SQLite，砍掉整條失效的 XPC 通道；偏好設定與 CLI 子命令也一併改用狀態檔 + distributed notification 跟 IME 溝通，`ChiaKeyServiceClient` 已刪除。細節見 [PhraseEditorRewrite.md](PhraseEditorRewrite.md)。
- Release packaging 已有本機/CI 共用入口 `Scripts/build-release-package.sh`，以及可手動觸發的 `.github/workflows/release.yml`。Developer ID Application/Installer 憑證與 notarization API key 已放進 repo secrets，release workflow 現在會自動簽章並 notarize `.pkg`。細節見 [ReleasePackaging.md](ReleasePackaging.md)。

## 下一步

### 1. 偏好設定維護介面

目前偏好設定還沒有這些使用者真正需要的功能：目前 app version、目前 lexicon version、目前 lexicon install path、檢查詞庫更新、安裝更新、重新載入輸入法、開啟 Application Support folder、複製診斷資訊。

介面應安靜、工具導向。維護流程穩定前，不需要建立新的視覺系統。IME 端已經有 plist 狀態發佈機制（見 [PhraseEditorRewrite.md](PhraseEditorRewrite.md)），這塊可以直接接上，不用重新設計溝通層。

### 2. 強化詞庫驗證與自動更新

App 已能在安裝前驗證 release DB。下一步是讓這些檢查更容易與詞庫 repo 共用：把 DB validation 抽成獨立 script、加入 CI 可用的 fixture 或 dry-run mode、驗證 symbol categories/punctuation keys/minimum table sizes、新 release 若含 forbidden OneKey data 則拒絕、新 metadata 在強制前先文件化。

### 3. Runtime smoke tests

改候選窗或 engine behavior 前，先補小型 manual/scripted smoke-test checklist。必要行為：注音組字可產生 `你好`；候選字選取與取消可用；`Shift+,` 產生 `，`；符號表可開啟且有分類；Caps Lock 符合現代 native ASCII 行為；倉頡與簡易仍可載入；App 切換後 composition state 乾淨；invalid external lexicon 會 fallback 到 bundled DB。

重要 app targets：TextEdit、Safari、Chrome、VS Code、Discord、Slack、Notion、Obsidian、Terminal。

### 4. Legacy cleanup

Cleanup 用小 commit 進行，且保持 `Takao-All` 可編譯。好的下一批 candidates：historical studies 與 internal documents、產品沒有 exposes 的 unused extra modules、confirmed-unused helper utilities、仍由 runtime 或偏好設定間接載入但產品不再 exposes 的資料檔。

Active input method path、倉頡、簡易不應只因為老就移除。

### 5. Release packaging 收尾

打包、簽章、notarization 都已經自動化。剩下比較次要：視需要把 notarized `.pkg` 包進 `.dmg`、補 install/rollback 文件與 release smoke test checklist。pkg 多語言不知為何沒能成功生效，有空再行研究。

### 6. iOS-ready core boundary

先把 engine 與平台 host 的邊界固定住，未來保留實作空間，細節在 [iOSImplementation.md](iOSImplementation.md)。下一步：把 `ChiaKeyCore` 接成正式 Xcode library target 或 Swift Package wrapper；加 ObjC++/Swift bridge，讓 Swift host 不直接 include OpenVanilla internals；以 XCTest 固定 `你好`、候選選字、退格、標點、commit acknowledgement；在獨立 iOS host repo 維護最小 keyboard extension shell；保持沒有 Full Access 時仍可使用 bundled DB 與 extension writable path。

## 延後事項

有在考慮的想法：Swift runtime 或 UI rewrite、Rust engine rewrite、大型 Electron-specific mitigation、候選窗視覺重設計、新 language model architecture、personal learning overhaul、iOS UI polish 與上架。

這些之後可能值得做，但現在先專注在本體的修復與整理。
