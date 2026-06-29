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
2. 補齊 DataTables、從 GitHub 下載最新版 ChiaKey-Lexicon release 詞庫、以及詞庫 installer script
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

release packaging 預設會使用 GitHub 上最新的 ChiaKey-Lexicon release，
下載 `lexicon-manifest.json`、資料庫與 metadata，驗證 SHA-256 與資料庫健康狀態後
包進 app。它不會從 raw source 重建 DB，也不會默默使用本機 CookedDatabase 作為
正式 release 詞庫。

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

正式 release 不應使用本機詞庫；預設流程會抓 GitHub release。只有在做本機測試時，
才建議把目前 active local lexicon 放進 app bundle 作為 fallback DB：

```sh
Scripts/build-release-package.sh --bundle-local-lexicon
```

或指定另一份 DB：

```sh
Scripts/build-release-package.sh --local-lexicon /path/to/ChiaKeySource.db
```

正式 release 應使用已驗證、版本化的詞庫 release artifact，而不是臨時本機 DB。

## 手動發佈 Release(GitHub Action)

`.github/workflows/release.yml` 提供手動觸發的發佈流程。
在 GitHub 的 **Actions → Release → Run workflow** 執行。

流程：

1. 從現有 tag 算出下一個版本 tag(若已存在則中止)。
2. 在 macOS runner 執行 `Scripts/build-release-package.sh` 產生 `.pkg`。
3. push tag。
4. 建立 GitHub Release,用 `generate_release_notes` 自動 summary 這版改動,並附上 `.pkg`。

`generate_release_notes` 由 `softprops/action-gh-release` 觸發,等同呼叫 GitHub
API 依上一個 tag 至今的 commit / PR 產生 release notes,不需額外設定。

### 輸入參數

| 參數 | 說明 |
| --- | --- |
| `release_type` | 依現有 tag 遞增下一版,預設 `beta`。`beta`→`vX.Y.Z-beta.N`(標為 prerelease);`patch`/`minor`/`major`→遞增對應位;`stable`→去掉 `-beta` 後綴。 |
| `dry_run` | 只算 tag 與建置,不 push、不發佈。先驗證用。 |

### 簽章 / notarization(可選)

未設定 secrets 時產出 **unsigned(ad-hoc)** package，設定以下 repository secrets 後會自動啟用
正式簽章與公證:

| Secret | 用途 |
| --- | --- |
| `APP_CERT_P12` / `APP_CERT_PASSWORD` | Developer ID Application 憑證(base64 `.p12`)與密碼 |
| `INSTALLER_CERT_P12` / `INSTALLER_CERT_PASSWORD` | Developer ID Installer 憑證與密碼 |
| `APP_SIGN_IDENTITY` / `INSTALLER_SIGN_IDENTITY` | 簽章 identity 名稱 |
| `NOTARY_API_KEY_P8` / `NOTARY_API_KEY_ID` / `NOTARY_API_ISSUER_ID` | App Store Connect API key,供 notarytool 公證 |

workflow 會把憑證匯入臨時 keychain、用 API key 建立 notarytool profile,再呼叫
同一支 `build-release-package.sh`。
