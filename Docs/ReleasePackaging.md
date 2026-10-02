# Release packaging

千秋輸入法正式發佈的主要 artifact 為 macOS Installer `.pkg`。

## 輸入模式相容性

`Takao-Info.plist` 以 `ComponentInputModeDict` 宣告單一可見模式
`com.chiakey.inputmethod.ChiaKey.Hant`，顯示名稱仍是「千秋輸入法」。注音、
倉頡等內部模組仍由原本選單切換。安裝時先啟用父輸入法，再啟用模式；
移除時停用兩者。Dev 安裝會把模式與本地化名稱一併改為 Dev 身分。

從沒有模式宣告的舊版升級後，請登出再登入，讓使用中的應用程式重新載入
輸入來源資料。`Scripts/verify-input-source-icon.sh` 會檢查建置產物的父項／
模式圖示、模式 ID 與本地化名稱，正式打包與 Dev 安裝都會執行。

這項相容性調整與圖示路徑修正用於處理 #6 的輸入來源切換問題；Safari 的
原始崩潰尚未在本機重現，仍需回報者在原環境驗證。


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

1. 從目前原始碼可達的 `v*` tag 計算共同版號（預設 patch；v1.2.6 的下一版是 v1.2.7）。
2. 同步調整 macOS 的兩份 plist、Windows CMake 版號，保存確切提交的 Git bundle。
3. 建置並驗證 macOS `.pkg`；Windows reusable workflow 從同一 bundle 建置 x64／x86
   DLL、設定程式、離線測試與 Inno Setup installer。任一失敗都不發布。
4. 從 `ReleaseNotes/*.json` 依平台與上次共同 tag 收集變更，分別產生 AI 摘要。
   stable 版的基準是上次 stable，包含整個 Beta 系列；AI 失敗使用平台原始清單。
5. 合併兩平台 SHA256SUMS.txt 與 notes，確認產物版號；dry_run 在此上傳完整
   artifacts 與 notes 預覽，不推送版號提交、tag，不建立 release，也不寫入 CDN。
6. 實際發布時推送兩平台確實建置的提交與共同 tag。原分支若已移動則中止，不 rebase。
7. 完整產物先上傳 GitHub draft，完成後才公開為共同 release。只有 stable 設為 Latest；
   Windows 目前仍是預覽版，在該平台說明中標示，並不把整個 macOS 正式版標為預覽。
8. 發布兩個平台 CDN feed 與專屬 notes，並維持既有 macOS `/chiakey/appcast.json`
   schema 1 的頂層 stable／beta 與 `/chiakey/ChiaKey.pkg` 人工下載連結。

新版 feed 是 `/chiakey/updates/macos/appcast.json` 與
`/chiakey/updates/windows/appcast.json`，使用相同 schema 1，加上 platform、version、
notes_url、release_url。安裝檔使用不可變版本 URL、SHA-256 與首次公開時間。
Windows 不再透過 `win-v*` tag 推送獨立發布。Windows build workflow 的手動執行
僅建置同名 `win-v0.1.0-beta.1` 朋友測試預覽，不自動發布；建置與測試成功後
替換原 GitHub release，不寫入 CDN。重發的預覽支援共同 `v*`、Windows CDN
及既有 `win-v*` fallback；重發前的原建置需手動安裝一次新版。

CDN 同時預檢兩平台 feed；只有讀取物件得到 404 才可初始化，403／5xx／逾時或
資料不合法都會停止，不可當成空檔。Beta 保留 stable，stable 不覆蓋更新的 Beta。
同版內容不可更換，發布流程序列化；重試保留 GitHub 原始 published_at。
所有 CDN 指標只會指向已上傳的完整產物，但各 feed 之間不是跨物件單一交易。
R2 設定為實際發布的必要條件；dry_run 無須這些密鑰。

### 輸入參數

| 參數 | 說明 |
| --- | --- |
| `release_type` | 依現有 tag 遞增下一版,預設 `patch`。`beta`→`vX.Y.Z-beta.N`(標為 prerelease);`patch`/`minor`/`major`→遞增對應位;`stable`→去掉 `-beta` 後綴。 |
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
