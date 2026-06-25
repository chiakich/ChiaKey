# Release packaging

千秋輸入法正式發佈的主要 artifact 應是 macOS Installer `.pkg`。

## 為什麼先做 `.pkg`

輸入法不是一般 drag-and-drop app。正式安裝需要把 app bundle 放到 macOS 會掃描的 input method 位置：

```text
/Library/Input Methods/ChiaKey.app
```

`.pkg` 可以直接描述這個安裝目的地，並透過 Installer 取得必要權限。`.dmg` 比較適合作為外層下載容器，或放一個 `.pkg` 與 README；它不適合當唯一安裝流程，因為使用者必須手動拖到 `Input Methods`，這比拖到 `/Applications` 更不直覺。

目前策略：

1. dev build：`Scripts/dev-install-local.sh`，安裝到 `~/Library/Input Methods`。
2. release build：`Scripts/build-release-package.sh`，產生安裝到 `/Library/Input Methods` 的 `.pkg`。
3. future polish：需要更完整的下載體驗時，再把 notarized `.pkg` 包進 `.dmg`。

## 建立本機測試 package

```sh
Scripts/build-release-package.sh
```

預設輸出：

```text
artifacts/release/ChiaKey-<CFBundleVersion>-unsigned.pkg
```

沒有提供 signing identity 時，script 會：

1. build Release `Takao-All`
2. 補齊 DataTables、bundled DB 與詞庫 installer script
3. ad-hoc sign `ChiaKey.app`
4. 建立 unsigned `.pkg`

腳本會在 `pkgbuild` 產生 component package 後重建 clean payload，避免新版
macOS 將 `com.apple.provenance` extended attribute 轉成 `._*` AppleDouble
條目包進 installer。

這適合本機測試 package payload，不適合公開 release。

打包流程會將授權與 acknowledgement 文件放進 app bundle：

```text
ChiaKey.app/Contents/Resources/Legal/
```

其中包含主專案 `LICENSE`、`COPYING`、`ACKNOWLEDGEMENTS` 與 vendored libraries 的必要 notices，讓 binary redistribution 也保留授權聲明。

如果 `ChiaKey-Source/Distributions/Takao/CookedDatabase/ChiaKeySource.db`
不存在，release packaging 會停止，不會從 raw source 重建 DB。請先放入
ChiaKey-Lexicon 產出的 release/local DB，或用 `--bundle-local-lexicon` /
`--local-lexicon` 明確指定要包進 app 的 DB。

## 正式簽章與 notarization

公開 release 應使用：

1. Developer ID Application certificate 簽 app bundle。
2. Developer ID Installer certificate 簽 package。
3. Apple notary service notarize package。
4. staple notarization ticket。

範例：

```sh
APP_SIGN_IDENTITY="Developer ID Application: Example Developer (TEAMID)" \
INSTALLER_SIGN_IDENTITY="Developer ID Installer: Example Developer (TEAMID)" \
NOTARY_PROFILE="chiakey-notary" \
  Scripts/build-release-package.sh --notarize
```

提供 Installer signing identity 時，預設輸出為：

```text
artifacts/release/ChiaKey-<CFBundleVersion>.pkg
```

`NOTARY_PROFILE` 是 `xcrun notarytool store-credentials` 建立在 keychain 裡的 profile 名稱。CI 不能直接使用本機 keychain profile 時，應在 CI job 裡建立 temporary keychain 與 profile，再呼叫同一支 script。

## 可選：bundle 本機詞庫

如果要把目前 active local lexicon 放進 app bundle 作為 fallback DB：

```sh
Scripts/build-release-package.sh --bundle-local-lexicon
```

或指定另一份 DB：

```sh
Scripts/build-release-package.sh --local-lexicon /path/to/ChiaKeySource.db
```

正式 release 通常應使用已驗證、版本化的詞庫 release artifact，而不是臨時本機 DB。

## CI / GitHub Release 方向

之後的 release automation 可以在 tag push 時執行：

```sh
Scripts/build-release-package.sh --notarize
```

然後把 `artifacts/release/*.pkg` 上傳到 GitHub Release。

必要 secrets / credentials：

1. Developer ID Application certificate。
2. Developer ID Installer certificate。
3. Certificate import password。
4. Notarytool credentials 或 App Store Connect API key。

在這些 secret 準備好以前，可以先讓 CI 產生 unsigned package artifact 作為 smoke test，但不要把 unsigned package 當公開 release。
