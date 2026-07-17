# 千秋輸入法架構

狀態：已採納

最後更新：2026-07-17

這份文件記錄目前的架構方向與理由。主線很簡單：保留已經能動的 macOS 輸入法路徑，逐步把 Yahoo 時代留下的資料流程與包裝假設換掉，不做整體重寫。

## 關於維護範圍

發佈主線是 macOS 繁體中文輸入法：

```text
ChiaKey.app
  -> macOS InputMethodKit host
  -> ChiaKeyCore / OpenVanilla loader boundary
  -> OpenVanilla loader bridge
  -> OpenVanilla / PlainVanilla modules
  -> OVIMMandarin
  -> Manjusri language model
  -> versioned ChiaKeySource.db
```

暫時先不管 Windows、舊 TSM loader、Yahoo web integrations 或 installer-era helpers。

## 主要決策

### 保留現有 IMK runtime

Objective-C++ InputMethodKit runtime 先留著，實測現在的狀態已經能在 Apple Silicon 上編譯執行，且 Yahoo 時代的候選窗與組字行為目前表現還算穩定，無需貿然重寫。而且目前真正值得投入的地方是詞庫 pipeline、更新、驗證、包裝跟 cleanup，大範圍的重構或 porting 到其他語言/架構並非我們的首要目標。

這不代表永遠不換，只是首要任務是先把行為和邊界穩定下來。

現代 IMK runtime 與 Preferences 仍會 link `Carbon.framework`，但只剩 TIS keyboard layout API 在用。舊 TSM loader 與 TSM installer 已經移除。

### 分離可攜核心邊界

加了一個小而明確的 host-neutral core boundary（`ChiaKeyCore`），讓未來其他移植可以共用智慧注音引擎。目前暫時未有其他平台的移植計畫，只是先把引擎分開，保留未來要移植的方便性。

`ChiaKeyCore` 是 host-neutral facade，包住 `OVIMSmartMandarin`，對外只暴露 key event、engine config、state snapshot、commit acknowledgement。它不依賴 AppKit、InputMethodKit、UIKit、SwiftUI，也不處理 macOS 的 inline marked text 政策、iOS 的 `UIInputViewController`，或安裝簽章打包這類事。

責任：接 host key event、開 SQLite-backed `ChiaKeySource.db`、回傳 reading/composing/candidate/committed text、保持能過 iPhoneOS SDK syntax-check、用 smoke test 固定基本注音行為。

iOS host 有在考慮實作，移至獨立實驗 repo；主 repo 只保留 `ChiaKeyCore` 公開 facade 跟一個最小 platform placeholder。細節在 [iOSImplementation.md](iOSImplementation.md)。

### 保留老派組字策略

IMK host 繼續用 inline marked text，候選窗跟工具窗自己畫。沒有可重現的特定 app bug 前，暫時不管。目前這套雖然老派，但也因為保守而避開了不少現代輸入法在 Electron app 裡常踩的浮動組字窗問題。

候選窗後續要先顧可靠性：穩定出現在組字區附近、app 切換後不留壞掉的 composition state、client 給不出可用 geometry 時能安全 fallback。

### 偏好設定作為維護中心

偏好設定 app 的目標角色是使用者面向的維護介面：顯示 app 版本與 input source identity、顯示目前詞庫版本與安裝路徑、檢查 `ChiaKey-Lexicon` 的 GitHub Releases、透過跟 CLI 相同的 validation path 安裝詞庫、可行時重整 process、清楚顯示驗證失敗原因、提供開啟 Application Support 與複製診斷資訊的入口。

這套維護 UI 目前還沒做（見 [ModernizationPlan.md](ModernizationPlan.md)）。偏好設定與 CLI 已經改用檔案 + distributed notification 跟執行中的 IME 溝通，不再依賴 XPC。細節見 [PhraseEditorRewrite.md](PhraseEditorRewrite.md)。在 runtime 穩定前，把偏好設定擴張並非首要目標。

### 倉頡與簡易先保留

雖然現在 mac 已不配備簡易與倉頡鍵盤印刷，我也不會這兩種輸入法，但簡單確認後，目前仍可透過 generic module 與 CIN table path 運作。除非測試證明壞掉或難以維護，我認為可以先留著。

## Repo 邊界

### App repo (`chiakich/ChiaKey`)

負責 macOS app 與 InputMethodKit runtime、runtime 目前需要的 OpenVanilla/PlainVanilla source、偏好設定與詞彙編輯器、本機安裝與測試 scripts、詞庫 installer 與 validator、app 內建 fallback DB，以及架構/更新/相容性文件。

### 詞庫 repo (`chiakich/ChiaKey-Lexicon`)

負責 source manifests 與 attribution、normalized lexicon source data、DB build scripts、release metadata、generated DB 的 GitHub Release assets，以及 lexicon CI checks。Generated `ChiaKeySource.db` 放在 GitHub Release assets，不進一般 git history。App repo 只把詞庫 repo 或 release artifact 產出的 DB 當本機 bundled fallback，不保存 raw lexicon source，也不執行 DB build pipeline。

## Runtime 資料權責

輸入法用三類資料：App bundle resources（app 內建、可離線用的 fallback data）、External release lexicon（下載後安裝到 Application Support 的 release data）、User data（使用者偏好、自訂詞、學習資料，以及複製到 user persistence DB 的符號表資料）。

詞庫更新不能直接覆蓋 user data。Release data 可以刷新 `canned_messages` 這類 prepopulated service data，但個人詞跟學習狀態要確認留在使用者資料層。

```text
外部詞庫：~/Library/Application Support/ChiaKey/Lexicons/active/ChiaKeySource.db
Fallback：ChiaKey.app/Contents/Resources/Databases/ChiaKeySource.db
```

外部 DB 缺失、損壞或不相容時，runtime 必須 fallback 到 app 內建 DB。

## Legacy cleanup 原則

無法測試、沒有維護需求、對於 build、test 沒有幫助，或者它根本不是輸入法功能，就可以移除或封存。舉例像 OneKey 服務等 Yahoo web-service，非輸入法核心功能，可先移除。

高信心 candidates：Windows loader、Windows installer、Visual Studio project、historical studies 與 internal documents、confirmed-unused helper utilities、產品沒有 exposes 的資料檔與模組。

需要先查 build graph 的 candidates：目前產品沒有 exposes 的 extra modules、`Takao.xcodeproj` 仍引用的 helper apps、仍由 runtime 或偏好設定間接載入的資料檔、仍影響 signing/packaging/local install scripts 的 legacy helper。

Cleanup 用小 commit 進行，且保持 `Takao-All` 可編譯。

## 現代化優先順序

進度追蹤看 [ModernizationPlan.md](ModernizationPlan.md)；這裡只列長期方向。

立即：Debug/Release build 在 Apple Silicon 上持續可成功、local install 與 ad-hoc signing 持續可用、詞庫 release 安裝前必須驗證、詞庫更新後可 reload 或乾淨 relaunch、維護 runtime 與詞庫 contract 文件。

下一階段：補 composition/candidate selection/punctuation/symbol table/lexicon fallback smoke tests、讓偏好設定成為維護中心、從 active Xcode project 移除未使用 targets 與 dead source trees、只在阻礙 build/signing/packaging/runtime 穩定時才替換 deprecated API。

之後：有測試後再重看候選窗 rendering、可考慮用 Swift 改寫偏好設定或小型 helper surface、可考慮用 Rust 或 Swift 做未來詞庫 builder/language model layer、更深的 engine rewrite 等行為被測試固定後再說。

## 測試基線

每個 cleanup 或 runtime change 都要維持：基本注音組字（`ㄋㄧˇ ㄏㄠˇ` -> `你好`）、候選窗顯示/選字/翻頁/取消、全型標點（`Shift+,` -> `，`）、符號表可從 `canned_messages` 顯示分類、Caps Lock/native ASCII 行為符合現代 macOS 慣例、倉頡與簡易維持可用（除非有明確決策移除）、外部詞庫驗證失敗時 fallback 到 bundled DB、App 切換後不留下 stale marked text。

重要 app targets：TextEdit、Safari、Chrome、VS Code/Discord/Slack/Notion/Obsidian 等 Electron apps、Terminal。

## 非目標

短期內先不做：Swift rewrite、engine rewrite、Windows support、舊 TSM loader support、Yahoo web services相關功能、第三方詞庫資料。

## AI coding agent 資訊揭露

2026-07 的詞彙編輯器（PhraseEditor）改寫由 AI coding agent（Fable 5）主導完成，動機與細節見 [PhraseEditorRewrite.md](PhraseEditorRewrite.md)。這次改動也順手把偏好設定與 CLI 的溝通機制從失效的 XPC 換成檔案 + distributed notification，並移除了 `ChiaKeyServiceClient`。

評估認為 PhraseEditor 是邊緣組件（使用者詞庫管理工具，不在核心輸入路徑上），不影響輸入法主線行為，所以用 AI 改寫的風險可控。改動範圍內我已盡可能逐項 review 過，包含 rowid 穩定性假設、WAL 並行讀寫、以及 MJSR 匯入匯出格式相容性。
