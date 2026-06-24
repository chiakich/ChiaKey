# iOS 實作指南

最後更新：2026-06-23

這份文件保留在 ChiaKey 主 repo，說明 iOS custom keyboard 要如何接
`ChiaKeyCore`。實際 iOS app 與 keyboard extension 可放在獨立 repo，並把
主 repo 作為共用輸入核心來源。

建議 repo layout：

```text
ChiaKey-iOS/
├── App/
├── KeyboardExtension/
├── Shared/
├── Resources/
└── Support/HeaderShims/
```

未來若要正式化依賴，可以把 `ChiaKeyCore` 轉成 submodule、Swift Package 或
XCFramework。主 repo 仍保留 `Loaders/iOS-Keyboard/Resources/.gitkeep`，用來
保留平台 host 的目錄位置與分層語意；不要把 iOS app/extension source 放回主 repo。

## 目標架構

```text
iOS KeyboardExtension / UIKit
  -> ObjC++ / Swift bridge in the iOS host repo
  -> ChiaKeyCore
  -> OVIMMandarin
  -> Formosa + Manjusri + SQLite ChiaKeySource.db
```

`ChiaKeyCore` 的公開 header 不暴露 OpenVanilla 型別。iOS host 只需要：

1. 將鍵盤按鍵轉成 `ChiaKey::KeyEvent` 或 C ABI key event。
2. 呼叫 engine handle key API。
3. 讀取 engine snapshot。
4. 用 `textDocumentProxy` commit `committedTextSegments`。
5. commit 成功後呼叫 `acknowledgeCommit()`。
6. 用 snapshot render reading text、composition 與 candidate bar。

## Core API 邊界

`ChiaKeyCore.h` 提供 C++ facade：

1. `EnginePaths`：`loadedPath`、`resourcePath`、`writablePath`、
   `lexiconDatabasePath`。
2. `EngineConfig`：locale、keyboard layout、candidate keys、composition 行為。
3. `KeyEvent` / `KeyModifiers`：host key input。
4. `EngineState`：reading、composing、committed segments、candidate state、
   cursor、highlight、word segments、tooltip、beep、notifications。
5. `selectCandidate(index)`：以 absolute candidate index 選字。

`ChiaKeyCoreC.h` 提供 C ABI bridge：

1. `CKC_Engine` opaque handle。
2. `CKC_EngineConfigDefault` / `CKC_KeyModifiersNone`。
3. `CKC_EngineCreate` / `CKC_EngineDestroy`。
4. `CKC_EngineHandleKey` / `CKC_EngineHandleAsciiKey`。
5. `CKC_EngineCopySnapshot` / `CKC_EngineSnapshotDestroy`。
6. `CKC_EngineAcknowledgeCommit`。

iOS Swift host 應優先透過 C ABI 或 ObjC++ wrapper 接入，不直接 include
`OVIMSmartMandarin` internals，也不依賴 PlainVanilla 或 OpenVanilla
implementation details。

## iOS Extension 行為原則

1. 不依賴 macOS inline marked text；composition 顯示在 keyboard UI。
2. 沒有 Full Access 時仍需可用 bundled lexicon 與 extension writable path。
3. App Group / Full Access 可用時才啟用共享詞庫匯入與跨 app 設定同步。
4. secure text field、phone pad、host app 拒絕 third-party keyboard 時，要接受
   iOS 系統 fallback。
5. 必須提供 next-keyboard 切換入口。

## 驗證

主 repo 改動 core 或 iOS 邊界後請跑：

```sh
Scripts/test-core-smoke.sh
Scripts/test-ios-core-syntax.sh
```

`test-core-smoke.sh` 會在 macOS 上編譯臨時 command-line binary，打開 bundled
`ChiaKeySource.db`，餵入 `你好` 的標準注音鍵序，並驗證 composing 與 commit
snapshot。

`test-ios-core-syntax.sh` 會用 iPhoneOS SDK syntax-check `ChiaKeyCore` facade、
C ABI bridge、OVIMMandarin、Formosa 與 Manjusri source，確保主 repo 的共用核心
仍能被 iOS toolchain 解析。

iOS app/extension build 請在對應的 iOS host repo 執行：

```sh
Scripts/test-ios-xcode-build.sh
```
