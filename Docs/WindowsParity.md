# 原版 Windows KeyKey 功能與 UI 對照

更新：2026-10-04。目標是恢復原版的本機輸入行為與可辨認的操作介面；此表不是「已完全一致」的宣告。

## 比較基準與來源

原始版本為 repo 中 `d87c1696b8987fb490c5dfb67752b7f13db8727a`（2012-12-03 開源提交）
的 `YahooKeyKey-Source-1.1.2528`。Windows 移除提交為 `7bdb7bc5c0dc40b45b736cdb07ae405bccbd3eae`
（2026-06-23）；其前一版 `b5c291c` 已經刪除 OneKey，因此完整功能盤點使用最初開源版，
並對照刪除前的 tree。歷史有另一條移除提交 `41087ab`，不要把兩條歷史當成兩個產品版本。

主要證據都是版本庫內的原始碼與繁體中文資源：

- `PreferenceApplications/Windows/TakaoPreference.cs` 與各 `Panel*.cs`／`*.zh-TW.resx`。
- `Loaders/Windows-IMM/BaseIMEServer/RPCService.cpp`：真正的按鍵分派，不只 UI 標籤。
- `Loaders/Windows-IMM/BaseIMEUI/BICandidateForm.cs`、`BIStatusBarForm*.cs`、`BISymbolForm.cs`、
  `BIKeyboardForm.cs`、`BIDictionaryForm.cs`。
- `Utilities/PhraseEditor/Windows/EditorForm.h`、`ReadingForm.h` 與本地化資源。

所有上述路徑都相對於原版 source 目錄；新前端在 `ChiaKey-Source/Loaders/Windows-TSF`。
原版是 IMM client＋RPC server＋C++/CLI／WinForms UI；新前端沿用目前已決定的 TSF in-process
架構。沒有將舊 framework tree 覆蓋到千秋，也沒有執行舊版安裝器。

## 功能對照

「已有」表示在目前程式碼找到功能，不代表已完成與舊版的 GUI 對照。「本批」表示已補上程式碼
與適用的自動測試；宿主實測仍須另列。

| 功能 | 原版 Windows 證據／行為 | 新前端狀態 |
|---|---|---|
| 好打／傳統注音、倉頡、簡易、泛用字表 | PanelGeneral 與各輸入法 Panel | 已有，共用現行核心與正式詞庫 |
| 輸入法選單管理 | ModulesSuppressedFromUI | 已有，隱藏項目不顯示，使用中的保留 |
| Ctrl+\\ 切換下一輸入法 | RPCService 呼叫 UseNextInputMethod | 本批修正，依選單順序循環並略過隱藏項目 |
| 單擊 Shift 切中英 | RPCService Shift 分支與 IgnoreShiftAsAlphanumericModeToggleKey | 已有；本批在 Caps Lock 模式啟用時停用 Shift 切換，保留勾選偏好 |
| Caps Lock 英文模式 | EnablesCapsLockAsAlphanumericModeToggle、移除大小寫 latch | 本批補上；Caps Lock 開啟時英文小寫、Shift 英文大寫，仍待實機 |
| Ctrl+Space／Shift+Space | 原版 Ctrl+Space 交 Windows，Shift+Space 切全半形 | 現行 TSF 切中英／全半形保持；Windows 系統層行為待實機 |
| 簡繁切換快捷鍵 | ChineseConverterToggleKey，預設 Ctrl+Alt+S | 本批補上 a-z／無；普通 Ctrl+S 保留給宿主 |
| 重送最近文字 | RepeatLastCommitTextKey，預設 Ctrl+Alt+G | 本批補上，組字中不重送；保留實際已轉換文字。歷史目前每個 TIP instance 一份，跨應用程式共用尚未完成 |
| 字根反查 | ReverseLookup-Generic-cj-cin／Mandarin-bpmf-cin／HanyuPinyin | 本批載入現有反查 package，提供三種選擇與無；實際詞庫測試通過 |
| 提示視窗開關 | ShouldUseNotifyWindow | 本批補上，關閉通知不會關掉反查／候選 tooltip；獨立提示窗外觀與顯示時間仍待對照 |
| 注音 layout／選字鍵／buffer／空白／Esc／罕用字 | PanelPhonetic | 已有，原有核心與 TSF engine 測試保留 |
| 倉頡／簡易 auto-compose、clear-on-error、dynamic frequency／標點 | PanelCangjie／PanelSimplex | 已有；既有互斥規則與設定流程保留 |
| 泛用表設定／萬用字元／最大字根／空白選第一候選 | PanelGenericSettings | 已有；使用者 .cin 仍走現行 Generic 模組 |
| 候選窗版型與顏色 | BICandidateForm 的色彩／版型常數 | 已有原版主要版型；本批補上 ColorDialog 與 Color signed-ARGB 設定。DPI／字型／定位仍須目視對照 |
| 自訂提示音／測試 | SoundFilename＝Default 或 WAV；PanelMisc | 本批補上自訂 WAV、測試、停用狀態與非同步播放，無效檔案回到系統音；音訊仍待實機 |
| 符號表與罐頭訊息 | BISymbolForm／BISmileyPanel | 已有資料、分類、送出與編輯；原版完整分類排序／按鈕密度仍待視覺對照 |
| 標點螢幕鍵盤 | BIKeyboardForm，Ctrl+Alt+, 開啟後選一鍵送符號 | 尚缺；不能把現有 Ctrl+Alt+, 標點候選列表視為相同功能 |
| 螢幕鍵盤跟隨游標 | KeyboardFormShouldFollowCursor | 尚缺，依附上列螢幕鍵盤 |
| 詞彙增刪／詞文／讀音／匯入匯出 | EditorForm／ReadingForm／PanelPhrases | 已有，前批補上多音字選讀音與原子寫入；本批補上剪下／複製／貼上選單、依焦點操作與整列複製、說明／關於 |
| 設定的確定／取消／套用 | TakaoPreference | 已有；取消不寫未套用設定。本批新增欄位沿用同一儲存流程 |
| 關於頁與選單 | BIAboutPanel／BIStatusBarForm | 本批補上設定關於頁、語言列關於入口與詞彙編輯器關於；保留千秋名稱、版本與專案網址 |
| 獨立浮動狀態列、半透明、最小化到 system tray | BIStatusBarForm／PanelMisc | 尚缺完整原版浮動列，目前是 TSF 語言列／狀態圖示。不能把 tray 圖示視為原版完整狀態列 |
| 字典搜尋與歷史 | BIDictionaryForm 的 Yahoo 網路查詢／內嵌瀏覽器 | 尚缺；舊碼使用 HTTP 網路服務與外部 JS，不直接恢復失效服務或舊瀏覽器容器 |
| OneKey／Evaluator／其他 around filters | BIStatusBarForm 動態 modules 選單 | OneKey 已在本 repo 明確移除；其餘不是僅補一個選單即可使用，需各自核對模組與資料契約 |
| 字數統計、今天／本週／總計／清除 | BIAboutPanel 依 WordCount 套件啟用 | 尚未接上 Windows TSF runtime；原版也在套件不存在時停用 |
| signed plug-in 管理／移除 | PanelMisc 與 PVDLLLoadingSystem | 尚缺，現行核心使用 static packages；不可直接恢復舊 DLL 載入與簽章機制 |
| 自動更新 | PanelUpdate／FormAskDownload／FormDownload | 現行本體／詞庫雙管道已實作；產品發布與下載契約使用千秋，不復用 Yahoo 舊 endpoint |
| 繁中／簡中／英文 UI | zh-TW／zh-CN／default resx | 現行偏好設定主要繁中，完整三語 UI 尚缺 |

## 本批按鍵與設定實作

`FrontendBehavior` 共用同一份 shortcut 分派、選單循環、Caps Lock ASCII case 與舊 Color ARGB
解析，供 TSF callback 與 engine test 使用。快捷鍵同時接到 OnTestKeyDown／OnKeyDown 與
OnPreservedKey，避免只測引擎而漏掉 TSF preserved-key 宿主。

新增設定沿用原版 key 名稱（Windows.plist）：EnablesCapsLockAsAlphanumericModeToggle、
ChineseConverterToggleKey、RepeatLastCommitTextKey、ShouldUseNotifyWindow、SoundFilename。
反查以 ReverseLookupMethod 指定現行 static package 的 identifier。切換反查會改 loader around
filter；只接受已初始化的模組，停用反查保留聯想詞 filter。一般設定選單顯示倉頡、注音、漢語拼音。

自訂顏色沿用 `Color <signed Int32 ARGB>`，GDI 使用 RGB channel，壞值回退預設色。聲音使用
PlaySound 的非同步檔案播放，避免在宿主 UI thread 等待整段音檔。

## 驗證邊界與下一步

本批以原生 MSVC x64／Win32 建置；兩架構 CTest 各 5/5 通過，包括正式詞庫的注音／拼音反查、
反查停用與未知 identifier、保留其他 filter，以及快捷鍵設定／循環／Caps Lock case／ARGB／通知
與重送歷史測試。原有更新、core smoke、空白、你好切詞 Tab、候選、Enter／Esc 測試也通過。

桌面仍受 RDP disconnected／locked 限制，沒有把編譯成功當成 GUI 已一致。待桌面可用時應先驗證
新快捷鍵在記事本／Edge 的 preserved-key 分派、Caps Lock 中途組字提交、反查提示、剪貼簿焦點、
設定全部九個頁面的 DPI／捲動／字型與顏色選取，再繼續標點螢幕鍵盤與完整狀態列的 UI 對照。
Server 的成功不替代 Windows 11 Store app／非管理員／提升權限宿主與不同 DPI 的驗證。

本批沿用 windows-tsf／PR #16，以獨立 conventional commit 保留功能補齊與先前 review 修正的邊界。
依目前約定，push 前先提供驗證結果；未發表任何 GitHub comments。
