# 專案目錄結構

最後更新：2026-06-23

這份文件固定 ChiaKey repo 的資料夾分工。整理原則是：現有 macOS
InputMethodKit target 先不做破壞性搬移；新的跨平台核心與 iOS repo
放在清楚的邊界裡，讓後續 Xcode target、Swift Package 或 submodule 可以逐步接上。

## Top Level

```text
.
├── ChiaKey-Source/      # 主要 source tree
├── Docs/                # 架構、iOS 實作、詞庫 contract、roadmap、目錄結構
├── Scripts/             # 本機 build、install、驗證與維護 scripts
├── KeyKey.xcworkspace   # workspace 入口
├── README.MD
└── LICENSE
```

## Source Tree

```text
ChiaKey-Source/
├── Frameworks/          # 可被 host 或 module 共用的 library/framework source
├── ModulePackages/      # OpenVanilla input methods、filters、工具 modules
├── Loaders/             # 平台 host / loader integration
├── PreferenceApplications/
├── Utilities/
├── Distributions/
├── DataTables/
└── ExternalLibraries/
```

## Runtime Layers

目前主線 runtime 分成四層：

```text
Platform host
  -> ChiaKeyCore / OpenVanilla loader boundary
  -> OVIMMandarin module
  -> Formosa + Manjusri + SQLite lexicon
```

### `Frameworks/ChiaKeyCore`

`ChiaKeyCore` 是新的 host-neutral engine facade。它不應依賴 AppKit、
InputMethodKit、UIKit 或 SwiftUI。

```text
ChiaKey-Source/Frameworks/ChiaKeyCore/
├── Headers/ChiaKeyCore/ # 公開 C++ facade 與 C ABI bridge
├── Source/              # facade implementation
└── Tests/               # core-level smoke tests
```

`Frameworks/HeaderShims/` 提供 framework-style include symlink，讓
`ChiaKeyCore` 與 iOS syntax checks 可以解析 `<OpenVanilla/...>`、
`<PlainVanilla/...>`、`<Formosa/...>`、`<Manjusri/...>` 與
`<ChiaKeyCore/...>`。

可以放在這裡：

1. 跨平台 engine facade。
2. host-neutral state snapshot。
3. Swift/ObjC++ 可接的 C ABI bridge。
4. macOS / iOS 都能使用的 C++ tests。

不要放在這裡：

1. AppKit / UIKit UI。
2. `UIInputViewController` 或 InputMethodKit controller。
3. app-specific settings screen。
4. package/install/release scripts。

### `ModulePackages/OVIMMandarin`

智慧注音 module 仍維持在 OpenVanilla module package 位置。短期內不要搬動；
`ChiaKeyCore` 應包住它，而不是把 module source 併進 core。

### `Loaders`

`Loaders` 放平台 host，而不是 engine business logic。

```text
ChiaKey-Source/Loaders/
├── CrossPlatform/       # macOS host 目前仍使用的 shared loader helpers
├── OSX-IMK/             # 目前發佈中的 macOS InputMethodKit host
└── iOS-Keyboard/        # iOS host placeholder; implementation can live in a separate repo
```

`OSX-IMK` 已被 `Takao.xcodeproj` 大量引用。除非一起做 Xcode project
migration，否則不要直接更名或搬動這個資料夾。

`iOS-Keyboard` 目前只保留平台位置；實際 iOS app / keyboard extension 可放在
獨立 repo。iOS repo 應只透過 `ChiaKeyCore` 接主 repo，不直接把 host code
寫回 `OVIMSmartMandarin` internals。

## iOS Host Placeholder

```text
ChiaKey-Source/Loaders/iOS-Keyboard/
└── Resources/           # reserved for future neutral fixtures if needed
```

若 iOS host 放在獨立 repo，建議維持下列結構：

```text
ChiaKey-iOS/
├── App/
├── KeyboardExtension/
├── Shared/
├── Resources/
├── Support/HeaderShims/
└── ChiaKey-iOS.xcodeproj
```

正式維護時，優先保持 `App/`、`KeyboardExtension/`、`Shared/`、`Resources/`
這個 shape，再決定 `ChiaKeyCore` 要用 submodule、Swift Package 或 vendored
source。

iOS host 的資料流應是：

```text
Keyboard tap
  -> Swift/ObjC++ bridge
  -> ChiaKey::Engine::handleKey
  -> ChiaKey::EngineState snapshot
  -> textDocumentProxy + keyboard UI
```

iOS 實作細節集中在 `Docs/iOSImplementation.md`，不要在 scaffold 子目錄散放短
README。

## Scripts

目前 script 仍維持 flat layout，因為 README 與開發流程已引用這些路徑。
新 script 命名請用動詞和 scope：

```text
Scripts/test-core-smoke.sh
Scripts/test-ios-core-syntax.sh
Scripts/test-lexicon-smoke.sh
Scripts/dev-install-local.sh
```

如果 scripts 繼續增加，再一次性整理成 `Scripts/dev/`、`Scripts/test/`、
`Scripts/lexicon/`，並同步更新文件。

## Resources

目前 bundled fallback DB 仍在歷史 distribution path：

```text
ChiaKey-Source/Distributions/Takao/CookedDatabase/ChiaKeySource.db
```

這條路徑仍被既有 build 與 scripts 使用，但 DB 由 ChiaKey-Lexicon release
artifact 或本機 lexicon DB 提供；app repo 不再保留 historical `DataSource`
raw files，也不在打包流程中重建詞庫 DB。未來若 macOS 與 iOS 共用 bundled
runtime resources，可以新增中性的 resource 入口，再讓各平台 target copy 到
自己的 bundle。
