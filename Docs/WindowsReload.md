# Windows 更新後重新載入

更新後，已開啟的程式會在輸入閒置時**自動載入新版**，不必關閉程式，
也不必切換輸入法。組字、候選字、符號窗或按鍵操作中會延後。
首次由沒有自動載入層的版本升級，已開啟的程式仍需重開一次。
手動以 Win+空白鍵切到另一個輸入法再切回，仍可作為備用載入方式。

## 新酷音參考

查閱新酷音 Windows TSF 的 `0100c1b57a185ac6c9655af798c4aa763b94f91d`
（2026-10-05；GitHub 與 Codeberg HEAD 相同）：

- [TIP 啟用／停用](https://github.com/chewing/windows-chewing-tsf/blob/0100c1b57a185ac6c9655af798c4aa763b94f91d/tip/src/text_service/mod.rs)
  在 Activate 建立服務，在 Deactivate 解除事件訂閱並釋放服務。
- [更新設計](https://github.com/chewing/windows-chewing-tsf/blob/0100c1b57a185ac6c9655af798c4aa763b94f91d/docs/development/self_update_design.md)
  將更新檢查交給 `chewing_tip_host`，透過 registry 通知 TIP。
- [變更紀錄](https://github.com/chewing/windows-chewing-tsf/blob/0100c1b57a185ac6c9655af798c4aa763b94f91d/CHANGELOG.md)
  記載 26.5.1 將 UI 與更新檢查移至獨立程序。

這些程式碼並沒有提供整個 TIP DLL 的熱替換保證。千秋參考其生命週期邊界，
另行實作可重新選擇 backend 的 COM 載入層，沒有複製新酷音程式碼。

## 實作

`DllGetClassObject` 建立 `ReloadableTextService`，維持 Windows 持有的 COM identity。
它自己實作 TIP、display attribute、function provider 與 configure 介面；
backend 自行訂閱 TSF key/edit/focus sinks。外部快取的 provider 仍透過載入層
轉送至目前 backend，不會固定在第一個版本。

每次新的 ActivateEx，以及啟用期間每秒一次的低優先序 WM_TIMER，讀取程序位元架構對應的
`HKLM\Software\ChiaKey\Tsf\BackendPathV1`。安裝器於 `[Files]` 完成後的
`[Registry]` 階段發布完整 DLL 路徑，x64 與 x86 registry view 各有自己的值。
開發用 `Register-Tip.ps1` 也會在成功註冊後發布對應架構的路徑。
讀取 HKLM 而非 HKCU；只接受磁碟機絕對路徑，以 DLL 所在目錄與 System32
作為相依 DLL 搜尋範圍。

載入新版後呼叫固定 ABI `ChiaKeyCreateTextServiceV1`，直接取得 backend，
避免再次進入 COM factory 而無限建立載入層。初次建立時無可用更新則使用
本 DLL。已啟用時重複呼叫 Activate 不會切換版本。

自動切換由建立在 TSF 原執行緒上的 message-only window 驅動，沒有背景執行緒
操作 TSF 或強制終止宿主。版本路徑未改變時不建立服務、不重新載入 DLL。
訊息迴圈暫停或宿主忙碌時，偵測自然延後，並非即時更新保證。

切換前透過私有 `IChiaKeyReloadControl` 詢問安全狀態。TextService 的入口及
可能重入的處理有活動計數；所有 key／symbol／commit／terminate edit session
由建立到釋放均計數，包含排隊中及正在執行的請求。尚有組字、候選字、模式提交、
符號／標點鍵盤、未成對的 TSF key test、實際按住的按鍵，或最後按鍵後未滿
500ms，皆回傳 S_FALSE 等下一次 timer。語言列 popup 的巢狀訊息迴圈也會延後換版。

確認新版具備控制介面且舊版仍閒置後，先停用舊 backend，再啟用新版，
沿用同一個 thread manager、client id、ActivateEx flags，並恢復中英文／全半形。
新引擎必須 ready 才接受交接；啟用或狀態恢復失敗時，先清理新版，再重新
啟用舊版及恢復狀態，每五秒重試。停用請求若在切換的巢狀呼叫中到達，
交接結束後會停用目前 backend、取消 timer，避免使用者已切離千秋卻又被啟用。
若舊版復原也失敗，保留 backend 並在相同 STA 重試復原。

自動更新要求新舊 backend 都實作此控制介面；缺少介面時不強制停用舊版。
只提供 V1 factory ABI 的版本仍可手動切換載入；不提供 V1 ABI 的版本需重開程式。

backend 保留在程序位址空間至程序結束，避免尚未釋放的 COM 物件、視窗類別
或延後的 edit session 指向已卸載的程式碼。這與原有引擎 pin module 的策略一致。
舊 backend session 會走 Deactivate，但原有全域 runtime／資料庫仍可能佔用記憶體
及檔案；多次更新後重開程式才能完全釋放各版 runtime。
因此不保證更新當下能刪除所有舊版本檔案，也不強制關閉宿主。

不同版本使用不同安裝目錄。同一路徑重裝／原地重編譯不在重新載入保證範圍，
應使用新版本目錄或重開宿主。初次安裝、宿主不派送 timer、視窗建立失敗、
AppContainer 拒絕存取新版 DLL 等情況仍可能需要手動切換或排查。
舊程式碼的記憶體不會因更新立即回收，這不影響新版接手輸入。

## 驗證

`chiakey_windows_reload` 使用正式 `ChiaKeyTsf.dll` 的 COM factory，並在同一
程序內載入兩個真正的測試 DLL。透過 `RegOverridePredefKey` 將 HKLM 僅在
測試程序中導向暫存 registry key，不改動已安裝輸入法，也不啟用桌面 TSF。
涵蓋同一 TIP／已快取 provider 的 v1 → v2、COM identity、ActivateEx flags、
重複停用、缺檔／相對路徑／缺 ABI 回退，並由真實訊息迴圈驗證不呼叫手動
Deactivate／Activate 的自動換版、busy 延後、中英文／全半形保留、啟用／
狀態恢復失敗回退、再次嘗試，以及切換中的重入停用與 timer 清理。
兩種位元架構均已加入 CTest；測試 DLL 使用相同檔名、不同版本目錄。

2026-10-06 本機驗證：x64／Win32 Release 編譯及上述自動換版測試皆通過。
原 Win32 測試 EXE 被檔案鎖定，改用乾淨的 `build/Win32-auto` 建置後通過。
同分支先前的 x64／Win32 更新測試、x64 UILess、Win32 引擎測試亦通過；
x64 歷史 helper 與原 Win32 UILess EXE 則被本機應用程式控制政策阻擋，
CodeIntegrity 事件 3077／3033 已確認。未修改安全政策。

發佈前仍需桌面驗收：保留 Notepad、瀏覽器、32-bit 程式及 Store app，
從支援自動載入的 A 版升級至不同目錄的 B 版。不切換輸入法，完成或取消
組字、關閉符號窗，確認自動出現 B 版行為及原有模式、候選窗、設定與使用者詞彙。
另需確認正在組字、按住鍵、待執行 edit session 及語言列選單中維持 A，
直到操作結束才換版，且新版不可用時仍可輸入。自動化 fixture 不取代這些桌面驗收。
