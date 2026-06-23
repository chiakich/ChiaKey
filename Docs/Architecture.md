# 千秋輸入法架構決策

決策狀態：已採納

最後更新：2026-06-23

這份文件記錄千秋輸入法（ChiaKey）的目前架構方向。它不是重寫計畫，而是用來固定主線：保留已能運作的 macOS 輸入法路徑，逐步替換 Yahoo 時代留下的資料流程與包裝假設。

## 產品範圍

千秋輸入法是 macOS-only 的繁體中文輸入法。

支援的產品路徑是：

```text
千秋輸入法.app
  -> macOS InputMethodKit host
  -> OpenVanilla loader bridge
  -> OpenVanilla / PlainVanilla modules
  -> OVIMMandarin
  -> Manjusri language model
  -> versioned KeyKeySource.db
```

短期內不把專案重新擴張成跨平台輸入法 framework。Windows、Carbon TSM、Yahoo web integrations 與 installer-era helpers 都視為歷史資料，除非它仍是現代 macOS build 的必要部分。

## 主要決策

### 保留現有 IMK runtime

千秋輸入法目前應保留 Objective-C++ InputMethodKit runtime。

理由：

1. 目前路徑已可在現代 macOS 編譯與執行。
2. Yahoo 時代的候選窗與組字行為在 Electron / Chromium app 裡相對穩定。
3. 完整重寫會在測試不足時冒著丟失既有輸入手感的風險。
4. 目前最高槓桿的工作是詞庫 pipeline、更新、驗證、包裝與 cleanup。

這不代表永遠不能替換。它代表替換應該等行為測試與邊界穩定後逐步發生。

### 保留老派組字策略

IMK host 應繼續使用 inline marked text，候選窗與工具窗則由千秋輸入法自行繪製。

在沒有可重現的特定 app bug 前，不應加入大型 per-client workaround matrix。目前行為雖然老派，但正因為保守，反而避開一些現代輸入法在 Electron app 裡遇到的浮動組字窗問題。

候選窗後續工作應先聚焦可靠性：

1. 穩定出現在 client insertion point 附近。
2. app 切換後不留下壞掉的 composition state。
3. client 回傳不可用 geometry 時能安全 fallback。
4. 視覺重做前先補 regression tests。

### 偏好設定作為維護中心

偏好設定 app 應成為使用者面向的維護介面。

預期責任：

1. 顯示 app 版本與 input source identity。
2. 顯示目前詞庫版本與安裝路徑。
3. 檢查 `ChiaKey-Lexicon` 的 GitHub Releases。
4. 透過與 CLI 相同的 validation path 安裝詞庫。
5. 可行時 reload input method process。
6. 以清楚文字顯示驗證失敗原因。
7. 提供開啟 Application Support 與複製診斷資訊的入口。

在 runtime 與詞庫更新 contract 穩定前，不應把偏好設定擴張成大型功能 playground。

### 倉頡與簡易先保留

倉頡與簡易目前看起來仍可透過 generic module 與 CIN table path 運作。除非測試證明它們壞掉或無法維護，否則先保留。

真實輸入法功能的移除門檻應高於 Yahoo 時代服務功能。可以移除的條件：

1. 無法測試。
2. 沒有維護需求。
3. 阻礙現代 build、signing、packaging 或 runtime。
4. 它不是輸入法功能。

OneKey 符合移除條件，因為它是 Yahoo web-service launcher，不是輸入法。

## Repo 邊界

### App repo

`akira02/ChiaKey` 負責：

1. macOS app 與 InputMethodKit runtime。
2. runtime 目前需要的 OpenVanilla / PlainVanilla source。
3. 偏好設定與詞彙編輯器。
4. 本機安裝與測試 scripts。
5. 詞庫 installer 與 validator。
6. app 內建 fallback DB。
7. 架構、更新與相容性文件。

App repo 不應累積每一版 generated lexicon release。

### 詞庫 repo

`akira02/ChiaKey-Lexicon` 負責：

1. source manifests 與 attribution。
2. normalized lexicon source data。
3. DB build scripts。
4. release metadata。
5. generated DB 的 GitHub Release assets。
6. lexicon CI checks。

Generated `KeyKeySource.db` 應放在 GitHub Release assets，不應進入一般 git history。

## Runtime 資料權責

輸入法使用三類資料：

1. App bundle resources：app 內建、可離線使用的 fallback data。
2. External release lexicon：下載後安裝到 Application Support 的 release data。
3. User data：使用者偏好、自訂詞、學習資料，以及複製到 user persistence DB 的符號表資料。

詞庫更新不得直接覆蓋 user data。Release data 可以刷新 `canned_messages` 這類 prepopulated service data，但個人詞與學習狀態必須留在使用者資料層。

外部詞庫路徑：

```text
~/Library/Application Support/ChiaKey/Lexicons/active/KeyKeySource.db
```

Fallback DB 路徑：

```text
千秋輸入法.app/Contents/Resources/Databases/KeyKeySource.db
```

如果外部 DB 缺失、損壞或不相容，runtime 必須 fallback 到 app 內建 DB。

## Legacy cleanup 原則

Legacy files 若不在現代 macOS 產品路徑上，且不幫助 build、test 或理解現代 app，就可以移除或封存。

高信心 cleanup candidates：

1. Windows loader、Windows installer、Visual Studio project。
2. OneKey module 與 Yahoo-era web service data。
3. 指向 Yahoo infrastructure 的 dead update/feed paths。
4. 依賴 obsolete Apple packaging tools 的 installer systems。

需要先檢查 build graph 的 candidates：

1. `Loaders/OSX-TSM`
2. old package-maker distributions
3. studies 與 internal documents
4. 目前產品沒有 exposes 的 extra modules
5. `Takao.xcodeproj` 仍引用的 helper apps

Cleanup 應以小 commit 進行，且保持 `Takao-All` 可編譯。

## 現代化優先順序

### 立即

1. 讓 Debug / Release build 持續可在 Apple Silicon 上成功。
2. 讓 local install 與 ad-hoc signing 持續可用。
3. 詞庫 release 安裝前必須驗證。
4. 詞庫更新後可 reload 或乾淨 relaunch。
5. 維護 runtime 與詞庫 contract 文件。

### 下一階段

1. 補 composition、candidate selection、punctuation、symbol table、lexicon fallback smoke tests。
2. 讓偏好設定成為維護中心。
3. 從 active Xcode project 移除未使用 targets 與 dead source trees。
4. 只有在阻礙 build、signing、packaging 或 runtime 穩定時才替換 deprecated APIs。

### 之後

1. 有測試後再重看候選窗 rendering。
2. 可考慮用 Swift 改寫偏好設定或小型 helper surface。
3. 可考慮用 Rust 或 Swift 做未來詞庫 builder / language model layer。
4. 更深的 engine rewrite 必須等行為被測試固定後再說。

## 測試基線

每個 cleanup 或 runtime change 都應維持：

1. 基本注音組字，例如 `ㄋㄧˇ ㄏㄠˇ` -> `你好`。
2. 候選窗顯示、選字、翻頁與取消。
3. 全型標點，例如 `Shift+,` -> `，`。
4. 符號表可從 `canned_messages` 顯示分類。
5. Caps Lock / native ASCII 行為符合目前 macOS 慣例。
6. 倉頡與簡易維持可用，除非有明確決策移除。
7. 外部詞庫驗證失敗時 fallback 到 bundled DB。
8. App 切換後不留下 stale marked text。

重要 app targets：

1. TextEdit
2. Safari
3. Chrome
4. VS Code、Discord、Slack、Notion、Obsidian 等 Electron apps
5. Terminal

## 非目標

短期內明確不做：

1. 完整 Swift rewrite。
2. 完整 Rust engine rewrite。
3. 恢復 Windows support。
4. 恢復 Carbon TSM support。
5. 重建 Yahoo web services。
6. 未經授權審查就匯入第三方詞庫資料。
7. 沒有可重現問題前加入大型 Electron-specific hacks。
