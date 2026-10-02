# SignPath Foundation 申請

Windows 版的程式碼簽章打算申請 [SignPath Foundation](https://signpath.org/) 的免費開源方案。這份文件整理申請要做的事和表單答案；送出、接受條款與保管 API token 都只能由 repo 擁有者本人做。

## 順序

1. **先發一個未簽章的 Windows release。** SignPath 只簽已經發佈過的東西（"the project must already be released in the form that should be signed"）。推 `windows-v<版本>` tag，[`release-windows.yml`](../.github/workflows/release-windows.yml) 就會建置並發佈 Pre-release。
2. **確認首頁有 Code signing policy。** README 的 [Code signing policy](../README.MD#code-signing-policy) 段落，以及 release notes 裡的同名段落（[`Packaging/Windows/release-notes.md`](../Packaging/Windows/release-notes.md)）。
3. **送出申請**：[signpath.org/apply](https://signpath.org/apply)。
4. **通過後**：在 SignPath 建立專案與簽章政策，把 API token 存成 repo secret，在 `release-windows.yml` 標好的位置加上簽章步驟；README 與 release notes 刪掉「applied for, not yet granted」那句。

## 條件對照

| 條件 | 現況 |
|---|---|
| OSI 授權、無商業雙授權 | BSD 3-Clause |
| 無專有元件 | SQLite、expat 以原始碼編入 |
| 已發佈 | 步驟 1 |
| 首頁與下載頁有 Code signing policy | README 與 release notes |
| 所有有 commit 權限的帳號啟用兩步驟驗證（GitHub 與 SignPath） | 申請前請確認 |
| 被簽檔案的產品名稱與版本一致 | `ChiaKeyTsf.dll`、`ChiaKeySettings.exe`、安裝檔都是 ProductName `ChiaKey`、ProductVersion 取自 CMake `project()` |
| 提供解除安裝、不默默改系統設定 | Inno Setup 安裝檔，「已安裝的應用程式」可移除 |
| 每次發佈都要人工核准簽章 | 計畫規定；每次 release 要到 SignPath 網頁核准 |

憑證發給 SignPath Foundation，所以 Windows 顯示的發行者會是 SignPath Foundation。

## 表單答案

| 欄位 | 內容 |
|---|---|
| Name | `ChiaKey` |
| Handle | `chiakey` |
| Type | `Program` |
| License | `BSD 3-Clause "New" or "Revised" License` — `https://opensource.org/license/bsd-3-clause` |
| Repository URL | `https://github.com/chiakich/ChiaKey` |
| Homepage URL | `https://chiaki.ch/works/chiakey` |
| Download URL | `https://github.com/chiakich/ChiaKey/releases` |
| Privacy Policy URL | `https://github.com/chiakich/ChiaKey#code-signing-policy` |
| Wikipedia URL | （留空） |
| Tagline | `A Traditional Chinese input method for macOS and Windows, reviving Yahoo! KeyKey` |
| User Full Name / Email | 本人姓名與信箱 |
| Build System | `GitHub Actions` |

**Description**

```text
ChiaKey is a Traditional Chinese input method that continues Yahoo! KeyKey, which Yahoo
released as open source under the BSD license in 2012, together with the OpenVanilla framework
it was built on. It offers Smart Phonetic (Zhuyin with sentence-level conversion and learning),
Traditional Phonetic, Cangjie, Simplex and user-supplied .cin tables, plus a symbol table and a
phrase editor. On Windows it is a Text Services Framework input processor with a settings and
phrase editor app; on macOS it is an InputMethodKit input method. The lexicon is maintained in a
companion open source repository.
```

**Reputation**（數字送出前更新）

```text
ChiaKey has been developed in the open at https://github.com/chiakich/ChiaKey since June 2026
and has shipped 14 macOS releases so far (current: v1.2.6), each built by a GitHub Actions
workflow and distributed as an Apple-notarized, Developer ID-signed installer. The repository
has 75 stars and 3 forks. The codebase descends from Yahoo! KeyKey, a widely used Traditional
Chinese input method in Taiwan, which Yahoo open-sourced in 2012. The Windows version is built
by the same CI from the same source tree, and nothing is uploaded from a developer machine.
```

最後勾選「I hereby accept the terms of use」。

## 隱私說明要照實寫

Windows 輸入引擎維持離線；獨立更新 helper 在手動檢查／下載或開啟自動更新時，會連到
千秋輸入法的 GitHub Releases 與詞庫 CDN，不傳送輸入內容、詞彙或學習資料。
自動更新預設關閉，可在偏好設定「更新」頁分別開關本體與詞庫更新。
README 的 Privacy 已同步說明這些行為；申請表亦應使用相同說明。
