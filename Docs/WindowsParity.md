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
| 重送最近文字 | RepeatLastCommitTextKey，預設 Ctrl+Alt+G | 本批補上，組字中不重送；保留實際已轉換文字。第三批改為同一登入工作階段的共享記憶體；跨程序測試通過，宿主實測仍待完成 |
| 字根反查 | ReverseLookup-Generic-cj-cin／Mandarin-bpmf-cin／HanyuPinyin | 本批載入現有反查 package，提供三種選擇與無；實際詞庫測試通過 |
| 提示視窗開關 | ShouldUseNotifyWindow | 第三批補上獨立通知窗，第六批補上原版滑入、右上角堆疊、漸層與模式通知；關閉通知保留反查 tooltip。外觀與宿主仍待 GUI |
| 注音 layout／選字鍵／buffer／空白／Esc／罕用字 | PanelPhonetic | 已有，原有核心與 TSF engine 測試保留 |
| 倉頡／簡易 auto-compose、clear-on-error、dynamic frequency／標點 | PanelCangjie／PanelSimplex | 已有；既有互斥規則與設定流程保留 |
| 泛用表設定／萬用字元／最大字根／空白選第一候選 | PanelGenericSettings | 已有；使用者 .cin 仍走現行 Generic 模組 |
| 候選窗版型與顏色 | BICandidateForm 的色彩／版型常數 | 已有原版主要版型；本批補上 ColorDialog 與 Color signed-ARGB 設定。DPI／字型／定位仍須目視對照 |
| 自訂提示音／測試 | SoundFilename＝Default 或 WAV；PanelMisc | 本批補上自訂 WAV、測試、停用狀態與非同步播放，無效檔案回到系統音；音訊仍待實機 |
| 符號表與罐頭訊息 | BISymbolForm／BISmileyPanel | 已有資料、分類、送出與編輯；原版完整分類排序／按鈕密度仍待視覺對照 |
| 標點螢幕鍵盤 | BIKeyboardForm，Ctrl+Alt+, 開啟後選一鍵送符號 | 第二批補上 46 鍵、原版 420×127 版面／停用鍵／綠色鍵名、一次選鍵與滑鼠點選；物理按鍵走 DirectText，保留句子組字。焦點／外觀仍待實機 |
| 螢幕鍵盤跟隨游標 | KeyboardFormShouldFollowCursor | 第二批補上；預設停用，開啟時由 TSF selection 取得游標位置，跨螢幕邊界限制與 DPI scaling 尚待實機 |
| 詞彙增刪／詞文／讀音／匯入匯出 | EditorForm／ReadingForm／PanelPhrases | 已有，前批補上多音字選讀音與原子寫入；本批補上剪下／複製／貼上選單、依焦點操作與整列複製、說明／關於 |
| 設定的確定／取消／套用 | TakaoPreference | 已有；取消不寫未套用設定。本批新增欄位沿用同一儲存流程 |
| 關於頁與選單 | BIAboutPanel／BIStatusBarForm | 本批補上設定關於頁、語言列關於入口與詞彙編輯器關於；保留千秋名稱、版本與專案網址 |
| 獨立浮動狀態列、半透明、最小化到 system tray | BIStatusBarForm／PanelMisc | 第三批補上六操作浮動列、拖曳位置、50% 半透明、雙擊 mini／tray 和還原；使用千秋圖示，原版 bitmap 皮膚與宿主切換／tray 尚待視覺實測 |
| 字典搜尋與歷史 | BIDictionaryForm 的 Yahoo 網路查詢／內嵌瀏覽器 | 第五批補上獨立字典、HTTPS 搜尋、八筆歷史、複製／全選與瀏覽器開啟；內嵌頁面改用 WebView2，舊 XML API／Flash 與將字典文字直接送回宿主的橋接未恢復 |
| OneKey／Evaluator／其他 around filters | BIStatusBarForm 動態 modules 選單 | OneKey 已在本 repo 明確移除；其餘不是僅補一個選單即可使用，需各自核對模組與資料契約 |
| 字數統計、今天／本週／總計／清除 | BIAboutPanel 依 WordCount 套件啟用 | 第三批補上可停用的 TSF 成功提交計數與關於頁今日／最近七天／累計／清除；SQLite 跨宿主原子更新，不儲存文字。原 DLL 套件設定尚未遷移 |
| signed plug-in 管理／移除 | PanelMisc 與 PVDLLLoadingSystem | 尚缺，現行核心使用 static packages；不可直接恢復舊 DLL 載入與簽章機制 |
| 自動更新 | PanelUpdate／FormAskDownload／FormDownload | 現行本體／詞庫雙管道已實作；產品發布與下載契約使用千秋，不復用 Yahoo 舊 endpoint |
| 繁中／簡中／英文 UI | zh-TW／zh-CN／default resx | 第四批補上繁中／簡中／英文偏好設定、詞彙編輯器、更新頁、原生選單與核心 locale；字典頁尚未加入，字串裁切與字型仍待 GUI |

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

## 第二批標點螢幕鍵盤

原版 `BaseIMEServer.cpp` 的 PPK 表與 `BIKeyboardForm.cs` 含相同 46 個符號；
`BIKeyboardForm.Designer.cs` 的按鈕矩形、停用的 Tab／Enter／Shift 等鍵與 420×127 client size
作為版面基準。`CustomizedControls/BIKeyboardButton.cs` 的 Arial 6pt 綠色鍵名與 PMingLiU 11pt
黑色符號也沿用。現代系統的字型 fallback／DPI 與 GDI rendering 仍需視覺核對。

Ctrl+Alt+, 透過既有 preserved-key 註冊進入 TSF edit session，候選顯示期間不開啟。
鍵盤視窗不取得宿主焦點；物理選鍵使用原版 DirectText path，可將標點保留在好打句子組字中；
滑鼠點選沿用符號表的送出流程。一次選鍵後隱藏，Esc 關閉且不提交、不送出控制字元；
無效鍵關閉並依提示音設定發聲。切換輸入欄位／應用程式也關閉等待狀態。
新增 KeyboardFormShouldFollowCursor 設定，沿用原版預設 false。

第二批 x64／Win32 完整 CTest 各 5/5 通過（34.35／36.04 秒）。包括以正式詞庫組出「你好」
再送鍵盤逗號，確認留下「你好，」組字且 committedText 為空。這不是 TSF callback 或 GUI 的實測。

## 第三批浮動列、提示與共享狀態

浮動列依原版六操作順序接到現有 TSF 功能：輸入法、中英、簡繁、全半形、符號、設定。
ShouldUseTransparentStatusBar 使用 50% alpha；ShouldUseMiniMode 與 ShouldUseSystemTray
支援雙擊收合／還原，StatusWindow.plist 保留跨螢幕位置。每個前景 TIP 取得共享 owner，
其他宿主撤下浮動列／tray；Explorer tray flyout 不應使自己的 icon 消失。實際切換、DPI 與外觀尚待 GUI。

通知與反查 tooltip 分開，不再用候選窗代替通知。一秒後每 50ms 減少 20% opacity；
第六批改為多視窗堆疊與滑入動畫，詳見下節。安全模式不顯示新浮動 UI、不記錄字數或重送文字。

重送歷史使用按使用者 SID 與登入工作階段隔離的共享記憶體，明確 ACL／medium integrity
可供同一使用者的提升／一般宿主使用；x64 與 Win32 使用固定 layout，不落地文字，也不經系統剪貼簿。
只有實際成功提交才更新歷史；組字轉換的待提交內容延後至 EndComposition 成功。
共享最多 65,536 UTF-16 units，超長內容僅原 instance 能重送，其他 instance 不重送截斷／舊文字。
當最後一個持有 mapping 的 TIP 退出時，歷史消失；尚不具原版常駐 RPC server 的持續生命週期。

字數統計沿用原版按 Unicode code point 計數（包括標點與英文），由 TSF 成功提交時更新，
不是在核心尚未成功送進宿主時先加總。使用 WindowsWordCount.db 的 SQLite transaction，
資料只含日期／計數。預設關閉；關於頁提供開關、刷新與確認後清除。
最近七天按本機日曆日期計算，不用固定秒數跨 DST；舊版 YKAFWordCount.plist 未自動匯入。
SQLite 忙碌超過 50ms 會回報診斷而不阻塞打字更久，這種失敗會少計，尚需 GUI／壓力測試評估。

原生兩架構建置成功；完整 CTest 各 5/5（34.37／36.86 秒），最後共享 owner 變更後
兩架構 engine fixture 各 2/2 通過（1.00／1.05 秒）。新增真正子程序重送、Unicode code point、
午夜／七天窗口、並行計數與清除、超長歷史與通知時間測試。這些不等於 GUI 已驗證。

## 驗證邊界與下一步

本批以原生 MSVC x64／Win32 建置；兩架構 CTest 各 5/5 通過，包括正式詞庫的注音／拼音反查、
反查停用與未知 identifier、保留其他 filter，以及快捷鍵設定／循環／Caps Lock case／ARGB／通知
與重送歷史測試。原有更新、core smoke、空白、你好切詞 Tab、候選、Enter／Esc 測試也通過。

桌面仍受 RDP disconnected／locked 限制，沒有把編譯成功當成 GUI 已一致。待桌面可用時應先驗證
新快捷鍵在記事本／Edge 的 preserved-key 分派、Caps Lock 中途組字提交、反查提示、剪貼簿焦點、
設定全部九個頁面的 DPI／捲動／字型與顏色選取，以及標點螢幕鍵盤的滑鼠／一次選鍵／拖曳與跟隨游標。第三批浮動列／tray 已補程式碼，仍待實際 UI 對照。
Server 的成功不替代 Windows 11 Store app／非管理員／提升權限宿主與不同 DPI 的驗證。

本批沿用 windows-tsf／PR #16，以獨立 conventional commit 保留功能補齊與先前 review 修正的邊界。
依目前約定，push 前先提供驗證結果；未發表任何 GitHub comments。

## 第四批三語介面

Windows.plist 的 UiLanguage 可選 zh-TW、zh-CN 或 en；無效值回到繁中。
偏好設定、詞彙編輯器與更新訊息共用翻譯資源，簡中使用現有 HanConvert 轉換表。
原生選單、浮動列、符號表與通知標題也使用此設定；使用者詞彙、罐頭訊息與自訂表名稱不翻譯。
新建核心 runtime 使用相同 locale，讓反查與核心訊息一致。核心 locale 在建立後固定，
因此變更語言後需重新開啟設定與使用中的打字應用程式。

x64／Win32 完整 CTest 各 5/5 通過（33.76／36.12 秒）。包含 DLL 的簡中轉換、
英文資源、無效 locale 回退與核心 locale 設定；三語畫面的裁切、DPI 與字型尚未 GUI 驗證。

## 第五批字典與搜尋歷史

原版 BIDictionaryForm 保留八筆不重複的 FIFO 搜尋歷史，並提供複製／全選。
新字典由語言列／浮動列設定選單的「字典…」或 ChiaKeySettings.exe /dictionary 開啟，
沿用 SizableToolWindow，可輸入中文／英文、Enter 查詢、重選歷史、複製／全選與瀏覽器開啟。
歷史只在視窗程序記憶體，不另外落地；WebView2 的網頁 profile 則在 LocalAppData/ChiaKey。
查詢由使用者操作才送出；使用目前 Yahoo HTTPS 搜尋頁，而非舊 HTTP XML API／Flash。

WebView2 SDK 1.0.3537.50 自 Microsoft 的 NuGet 下載，CMake 固定 SHA-256 驗證。
SDK 為 net462，沿用系統 C# 5／.NET Framework；組件、對應架構 loader 與授權檔一起打包。
Evergreen Runtime 為另外維護的系統元件；缺少或初始化失敗仍可用「瀏覽器開啟」。
不把 host object、WebMessage 或 SendString callback 暴露給外部網頁；舊版直接送回宿主的
字典功能尚未恢復。沒有擷取／重製第三方詞典內容，網頁內容與版型由 Yahoo 提供。

兩架構完整 CTest 各 5/5（33.23／36.69 秒），包含 Unicode／&／#／? 查詢編碼、
空／過長輸入拒絕、八筆唯一歷史、重查順序與 HTTPS 導覽限制。
本機 headless probe 成功載入兩架構 SDK／loader，均找到 Runtime 151.0.4129.78；
這不代表已建立或目視驗證 WebView2 視窗。安裝 Inno Setup 的指令遭工具政策拒絕，
尚未編譯修改後的安裝包；桌面顯示、網頁載入、複製與原版外觀仍待 GUI。

## 第六批模式通知與通知堆疊

對照 BINotifyForm／BIStatusBarForm.Notify：通知從目前螢幕右上角堆疊、10px 間隔，
以 10ms timer 滑入 10px／淡入，100ms 後等待一秒，再每 50ms 減少 20% opacity。
恢復黑底、兩段紫色漸層標題、白字與矩形邊框。各則訊息保留自己的 timer，最多八則；
小螢幕放不下時回到頂端並移除遮擋的舊通知。這是每個 TIP 內的堆疊，跨宿主集中佇列尚缺。

中英、全半形、簡繁及輸入法切換重新提供原版通知，沿用三語與通知開關。
相同狀態不重複通知，初始化與無 thread focus 不產生通知，安全模式也不顯示。
同批修正已成功 InsertTextAtSelection、後續 caret move 失敗時漏記字數／重送歷史；
組字 SetText 成功後保留待提交內容，只有真正 EndComposition 成功或宿主終止才記錄。

兩架構完整 CTest 各 5/5（37.95／30.41 秒）。新增滑入／等待／淡出邊界、
負座標／小 work area 堆疊定位與三語通知 fixture；真正宿主 caret failure、動畫與焦點需 GUI 驗證。
