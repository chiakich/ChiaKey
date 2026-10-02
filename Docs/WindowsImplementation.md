# Windows 實作指南

最後更新：2026-10-02

這份文件說明 Windows TSF 輸入法要如何接 `ChiaKeyCore`，也是第二階段（接 TSF
前端）的交接文件。第一階段已完成：核心能用 MSVC 編譯並執行。

## 目前狀態

### 共同發版

自 `v1.2.7` 起兩平台共用 `vX.Y.Z`／`vX.Y.Z-beta.N`，同一 GitHub release
包含 macOS `.pkg`、Windows `Setup.exe`、兩者的 `SHA256SUMS.txt` 及平台 notes。
Windows 仍標示為預覽版，GitHub release 的 stable／Beta 標記由共同版號決定。
兩邊成功才公開 release，draft 期間先上傳完整產物，避免舊 mac 更新器看到缺少
`.pkg` 的版本。`.github/workflows/release-windows.yml` 只供共同流程建置，不再
透過推送 `win-v*` 單獨發布。

macOS 沿用 `/chiakey/appcast.json` 的 schema 1、頂層 stable／beta 與 `.pkg` URL。
平台 feed 在 `/chiakey/updates/macos/appcast.json` 與
`/chiakey/updates/windows/appcast.json`，同樣使用 schema 1，加上 platform 與
平台專屬 notes_url。詞庫維持既有共用介面。
Windows 先讀自己的 CDN feed，並與 GitHub SHA256SUMS.txt 交叉核對；失敗時
回到 GitHub，辨識共同 `v*` 及舊 `win-v*`，依實際 Windows installer 篩選。
同名重發的 `win-v0.1.0-beta.1` 測試預覽可直接更新至共同版本；重發前的原建置需手動安裝一次新版。

使用者可見變更寫在 `ReleaseNotes/*.json`，明確列出 macos／windows 平台；
共用核心改動不會自動被當成雙平台新功能。scope 可繼續使用 mac／win／core，
但 release notes 由平台變更檔產生，詳見 `ReleaseNotes/README.md`。

`.github/workflows/core-msvc.yml` 在 `windows-latest` 上編譯核心並跑 smoke test，
另一個 job 編譯 TSF 前端（DLL、圖示、設定程式）並跑 `chiakey_tsf_engine_test`。
詞庫不在 git 裡，所以由一個 `macos-26` job 用 `install-lexicon-release.sh
--dry-run --keep-downloads` 下載並驗證，再以 artifact 交給 Windows job。

已在 Windows 上實際執行過的：注音組字、選字、標點、多 context 隔離、設定即時
重載、學習寫入落到磁碟，以及引擎一結束就存檔。

TSF 前端在 `ChiaKey-Source/Loaders/Windows-TSF`：inline 組字、仿 KeyKey 的候選窗、
語言列與工作列狀態圖示（中文模式依輸入法顯示「注／倉／簡／中」，保留英文與全半形狀態）、`ChiaKeySettings.exe` 設定程式，
以及倉頡、簡易與使用者 `.cin` 字表（`%APPDATA%\ChiaKey\Tables\Generic\*.cin`）。

設定程式與安裝器使用 Mac 的 `ChiaKey.icns` 角色圖示；建置時從該檔的 PNG
representation 產生 `app.ico`，同時嵌入設定程式的 Win32 圖示與 managed resource。
偏好設定視窗直接讀取內嵌的橘色角色圖示；詞彙編輯器使用 Mac 的
`PhraseEditor.icns` 紫色角色圖示，轉成內嵌的 `phrase-editor.ico`。
TIP profile 的 `badge.ico` 則由 `qian.svg` 產生，
兩者不共用圖像，也不在「千」字背後繪製底色。

「一般設定」與語言列右鍵選單提供「簡體輸出」，預設關閉，選項保存於
`Preferences/Windows.plist` 的 `SimplifiedOutput`。送出文字時沿用 Mac
`OVOFHanConvert-TC2SC` 的字元表；組字、候選與學習資料維持原始繁體。
英文、符號、emoji 與 UTF-16 surrogate pair 原樣保留；切換中英、焦點移動
和宿主結束組字時也會套用輸出轉換。

AppContainer（開始選單搜尋、Store app）與桌面程式共用 `%APPDATA%\ChiaKey`：桌面程式
第一次建立它時會開放給 `ALL APPLICATION PACKAGES` 並標成 Low integrity。這代表任何
Store app 都讀得到學習資料庫，是刻意接受的取捨。共用目錄出現之前，AppContainer
先用自己的暫存目錄，之後每分鐘重試一次，成功就讓新的 `Engine` 搬過去。

## 架構決定：引擎放在 TSF DLL 裡

評估過三個前例後決定 in-process：

| 專案 | 引擎位置 |
|---|---|
| Yahoo KeyKey（2008，Windows） | server process，`BaseIMEServer` 經 MS RPC |
| RIME / Weasel | server process，`WeaselServerApp` |
| 新酷音 windows-chewing-tsf | in-process |

決定的理由是要做新注音那種 inline 虛線組字。`ITfContext`、edit session、
`ITfComposition` 與 `GUID_PROP_ATTRIBUTE` 都是應用程式 process 內的 COM 物件，
不能交給另一個 process 驅動。所以組字、display attribute 與候選窗無論如何都得在
DLL 裡。server 剩下的工作只有引擎與資料庫，卻得在每個按鍵上同步等一次 IPC。

新酷音是需求最接近的前例：注音、繁體、TSF、inline 組字，引擎在 process 內。
他們曾把 UI 拆到 `chewing_tip_host.exe`，四個月後以權限層級問題退回。

`Runtime` 就是 process 邊界的接縫。Yahoo 的 `BIServerRPCInterface.idl` 與它幾乎
一對一，所以日後若要拆成 server，不必重畫邊界。

## 第二階段：接 TSF 前端

從 [polobread/KeyKey](https://github.com/polobread/KeyKey) 的
`Source/Loaders/Windows-TSF` 開始。那是 MIT 授權，請保留 Chui-Ping Cheng 的版權
聲明。

TSF 程式碼只透過 `KeyKeyEngine.cpp` 這一層接觸引擎。把它換成包 `ChiaKeyCore` 的
實作即可，`TextService.cpp` 的 COM 與組字處理可以沿用。

**要編在我們的 framework tree 上，不要合併他們的。** 他們的 OpenVanilla、
PlainVanilla、Formosa 已經與我們分歧很多，`Mandarin.cpp` 差了一千多行。

## 移植時必須遵守的事項

每一條都是實測踩到的，不是推測。

1. **`keyCode` 要填移位後的字元。** 引擎從 keyCode 判斷注音鍵，修飾鍵不影響這一
   步。送 `keyCode=','` 加 shift 會組出 ㄝ；要送 `keyCode='<'` 才會得到 `，`。
   polobread 的 `PrintableAsciiFromVirtualKey` 就是在做這件事，照用。
2. **必須定義 `WIN32`。** 引擎全部以 `WIN32` 判斷平台，MSVC 只預先定義 `_WIN32`。
   沒定義會靜默走進 POSIX 分支，然後炸在 `dirent.h`。`ChiaKeyCore/CMakeLists.txt`
   已處理，自己寫的 build 檔也要照做。
3. **在 TSF `Deactivate` 時銷毀 `Engine`。** 學習是在 context 結束時存檔的，app 關閉
   時 TSF 送的 `Deactivate` 就是唯一的存檔時機。polobread 在 `Deactivate` 裡做
   `engine_.reset()`，保留它。若為了避開 loader lock 而在 process 結束時刻意不銷毀
   `Runtime`（polobread 就是這樣），沒關係，存檔靠的是 `Engine` 而不是 `Runtime`。
4. **學習資料庫的 connection 絕不能用 `BEGIN IMMEDIATE`。** 詞庫通常裝在
   `Program Files` 底下，一般使用者對那裡是唯讀的。bundled SQLite 3.6.11 在主
   database 唯讀時會拒絕 `BEGIN IMMEDIATE`，即使要寫的是 ATTACH 上去的學習資料庫。
   現代 SQLite 不會這樣，所以 macOS 上測不出來。
5. **詞庫要以讀寫開啟。** 學習資料庫是 `ATTACH` 在詞庫 connection 上的，而被 ATTACH
   的資料庫會沿用唯讀旗標。`Runtime` 先用唯讀 connection 驗證詞庫，驗證通過才以
   讀寫開啟，這樣被拒絕的詞庫不會被寫入，學習也寫得進去。
6. **存檔跑在 app 的 UI thread 上。** `busy_timeout` 因此只設 200 毫秒。不要改成
   PhraseEditor 用的三秒，那會讓 app 在切換焦點時凍住。
7. **一個 process 一個 `Runtime`，一個文字欄位一個 `Engine`。** `Runtime` 有一把
   recursive mutex，所有 `Engine` 呼叫都經過它，TSF 多 thread 呼叫是安全的。

## Inline 組字：新注音的外觀

polobread 已經註冊了 `ITfDisplayAttributeProvider`，也會把 `GUID_PROP_ATTRIBUTE`
套到組字 range 上，但只有一個 `TF_LS_SOLID` 屬性。改成新酷音的雙屬性：

| 範圍 | 線型 | `bAttr` |
|---|---|---|
| 整段組字 | `TF_LS_DOT` | `TF_ATTR_INPUT` |
| 游標所在的詞段 | `TF_LS_SOLID` | `TF_ATTR_INPUT` |

切 range 需要的資料 `EngineState` 已經有了：`wordSegments` 與 `highlight`。

## 建置與測試

在 x64 Native Tools 命令列裡執行，需要 CMake 3.21 以上。`ChiaKeySource.db` 不在
git 裡，要另外複製過去。

```powershell
cmake -S ChiaKey-Source\Frameworks\ChiaKeyCore -B build\core-cmake -DCHIAKEY_LEXICON_DATABASE=C:\path\to\ChiaKeySource.db
cmake --build build\core-cmake --config Release
ctest --test-dir build\core-cmake -C Release --output-on-failure
```

TSF 前端換成 `ChiaKey-Source\Loaders\Windows-TSF` 當 `-S`，產出的 `ChiaKeyTsf.dll` 用
`Register-Tip.ps1 -DllPath ...` 註冊。

## 需要實機的項目

以下都是 GUI 行為，CI 做不到：

1. 註冊 TIP，需要系統管理員權限。
2. 在記事本、Word、Edge 裡實際打字。
3. 目視確認虛線與實線底線。
4. 在 Store app（AppContainer）與以系統管理員身分執行的記事本裡打字。這一項連
   RIME 都沒有看得到的處理，值得先確認。

## 尚未解決

- `Scripts/test-learning-store.sh` 裡的學習並發測試只在 macOS 跑，沒有進 Windows CI。
- 加詞寫入失敗時會顯示「該詞已經存在於資料庫中」。不再謊稱成功，但把寫入失敗
  說成已存在仍不對，需要讓 `addUserUnigram` 的回傳值多一種狀態。

## Windows 更新機制

偏好設定的「更新」頁開啟時即檢查本體與詞庫，也可分別手動檢查、下載與安裝。
兩者有獨立的自動更新開關，預設皆開啟；已儲存的關閉選項仍保留。
「接受 Beta 版」獨立勾選且預設關閉，套用後手動與背景檢查都使用該選項。
手動檢查與安裝不受三天等待期限制。開啟後，由同一個設定 EXE 的
`/update-background` 桌面 helper 在登入時執行；每分鐘查看設定與每日節流標記，
每天最多連網檢查一次，發布滿三天才自動安裝。下載與 DB 完整性驗證在 helper／
設定程式的背景執行緒處理，TSF DLL 不連網。失敗原因與最後檢查結果可在更新頁看到。

本體接受 `chiakich/ChiaKey` 的共同 `vX.Y.Z`／`vX.Y.Z-beta.N`，
也相容舊 `win-vX.Y.Z`／`win-vX.Y.Z-beta.N` release（排除 draft），尋找版本相符的 `ChiaKey-Windows-X.Y.Z-Setup.exe`，核對該 release 的
`SHA256SUMS.txt`。安裝前再核對下載內容，透過 Windows `runas` 啟動 Inno 安裝器，
使用者仍須回應 UAC 並完成安裝流程；取消不更動現有安裝。舊應用程式仍保留原 DLL，
重新開啟才載入新版。新安裝器以原始桌面使用者執行 `/update-register`，把登入啟動
路徑改到新版；舊 helper 注意到路徑改變後退出，新 helper 接手。解除安裝移除啟動項。

詞庫採用與 Mac 相同的 CDN manifest，網路失敗時回到 GitHub。只接受詞庫 repo
release／CDN 的 HTTPS artifact URL，schema 1；DB 的 manifest SHA-256 必須與
GitHub release 校驗清單一致。WinSQLite 唯讀驗證 integrity、必要 tables／metadata、
最小筆數、Shift+, 標點、符號 plist 與禁止的 OneKey 資料，再由實際 bundled core
建立隔離 Runtime 並驗證「你好」組字。optional metadata 也核對 SHA-256。

外部詞庫與更新狀態存於 `%APPDATA%\ChiaKeyUpdates`，與 low-integrity 的學習資料
目錄分開。AppContainer 只有讀取權限，不能修改下載的 EXE 或詞庫啟用指標。
每次下載建立新版本目錄，再以 `File.Replace` 原子更新 `Lexicons/active.txt`；第一行
為目前目錄名、第二行為前一版。TSF 只接受安全目錄名，依序嘗試目前／前一版／內建 DB。
每個 Engine 在沒有組字時才接上新 Runtime，詞彙編輯器亦使用目前載入的 DB。
helper 若發現外部 DB 被移除或損壞，會驗證前一版並修正指標，讓下次檢查能重試。
保留舊版本檔案以供仍持有 SQLite handle 的應用程式繼續使用。

`chiakey_windows_updates` 是離線 CTest：以 fixture 取代網路、不寫入登入啟動項、不
啟動安裝器，涵蓋合法安裝、hash／SQLite／core 拒絕、回退、重新嘗試、Windows
頻道、快取竄改與三天／每日節流；原有 TSF engine test 另測指標解析。
正式發布前仍須實測網路下載、UAC／取消、登入排程、更新中組字與 AppContainer 載入。
