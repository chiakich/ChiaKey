# Windows 遊戲輸入相容性

千秋的 Windows 前端是 TSF 輸入法。遊戲必須啟用 Windows 的文字輸入服務，
或提供可與 TSF 配合的輸入框，輸入法才能建立組字與提交文字。

## 已補上的支援

- 註冊 `GUID_TFCAT_TIPCAP_UIELEMENTENABLED`，讓要求 UILess 輸入法的宿主可以啟用千秋。
- 透過 `ITfUIElementMgr` 的 Begin / Update / End 發布候選字。
  `ITfCandidateListUIElement` 提供完整候選清單、全域選取位置、分頁與文件管理器。
  宿主可以自行繪製候選字，或允許千秋顯示原有候選窗。
- 宿主呼叫 `Show(FALSE)` 後不會在下一次按鍵更新時重新彈出候選窗。
  候選資料不依賴 `GetTextExt` 成功，也不依賴本機視窗是否顯示。
- 一般桌面宿主的模式通知、符號面板與標點鍵盤也先與 UIElement manager 協商。
  UILess / secure 宿主不開啟這些額外視窗。
- 宿主沒有 TSF 文字位置時，一般視窗模式先嘗試系統游標位置，
  再使用宿主視窗左下方作為候選窗備援位置；這不是精確的聊天框定位。
- `KEYBOARD_DISABLED`、`EMPTYCONTEXT` 或唯讀文字區不吃輸入按鍵與保留快捷鍵。
  同步 edit session 被鎖定時，亦檢查內層 HRESULT 並重試非同步 session。

## 已知限制

模擬宿主測試不能證明英雄聯盟或其他特定遊戲可正常使用。
目前沒有實機驗證對局內聊天、獨佔全螢幕、遊戲自訂文字框或反作弊載入政策。
候選清單介面支援宿主讀取與繪製，尚未提供
`ITfCandidateListUIElementBehavior` 的滑鼠選字／Finalize 操作。
文字提交仍依賴宿主的 TSF 文字服務；本次沒有新增 IMM 輸入法、鍵盤注入或遊戲 hook。

## 回報與驗證步驟

1. 記錄 Windows 版本、千秋版本、遊戲版本與實際問題位置。
   遊戲啟動器／大廳與對局內聊天可能是不同程序，請分別測試。
2. 在同一輸入框比較微軟注音與千秋：中文組字、空白選字、上下／翻頁、Enter 提交、
   Escape 取消，以及英數輸入。區分完全無反應、只出現英數、看不到候選字和提交失敗。
3. 比較視窗、無邊框與獨佔全螢幕模式；切換後重新開啟聊天框再測。
4. 在關閉聊天框時確認遊戲快捷鍵正常；重新開啟聊天框、切換輸入法後確認能恢復輸入。
5. 遊戲程序若根本沒有載入 `ChiaKeyTsf.dll`，先區分輸入服務未啟用、
   DLL 載入失敗與系統／遊戲安全政策。僅憑沒有候選窗不能認定反作弊封鎖。
   讀取程序模組失敗（例如權限不足）也不能作為 DLL 未載入的證據。

`OutputDebugString` 日誌前綴為 `ChiaKeyTsf pid=... tid=...`，可依程序分辨宿主：

| 日誌 | 用途 |
| --- | --- |
| `Activate flags=... uiLess=... uiManager=...` | 是否啟用 TIP、是否為 UILess 宿主 |
| `Activate engineReady=...` | 引擎與詞庫是否初始化成功 |
| `KeyDown request=... edit=...` | TSF 是否拒絕文字編輯 |
| `CompositionStart ...` | 插入介面、組字範圍與建立組字的失敗階段 |
| `UpdateComposition hr=...` | 更新／提交組字失敗 |
| `CandidateGeometry ...` | 候選窗位置取得失敗 |
| `CandidateUI ...` / `AuxiliaryUI ...` | 宿主 UI 協商失敗 |

新增日誌記錄旗標、狀態與 HRESULT，不記錄輸入文字或候選字內容。
若懷疑 DLL 被封鎖，請一併提供相關的 Windows Code Integrity 或遊戲載入錯誤記錄，
再決定後續的簽章／發布處理。

遊戲執行中也可用 `Scripts/collect-windows-game-diagnostics.ps1` 收集本機報告：

```powershell
.\Scripts\collect-windows-game-diagnostics.ps1
# 其他遊戲可指定程序名稱的正規表示式
.\Scripts\collect-windows-game-diagnostics.ps1 -ProcessPattern 'GameProcessName'
```

報告包含 Windows 版本、輸入法登錄路徑與簽章狀態、遊戲程序的千秋模組，
以及最近 30 分鐘包含 ChiaKey 的 Code Integrity 事件。
此工具只讀取資料並儲存 JSON，不注入遊戲、不附加 debugger、不修改安全設定或上傳。
分享前請檢查報告中的檔案路徑；路徑可能含 Windows 使用者名稱。

## 參考

- [Microsoft：UILess Mode Overview](https://learn.microsoft.com/en-us/windows/win32/tsf/uiless-mode-overview)
- [Microsoft：Using an Input Method Editor in a Game](https://learn.microsoft.com/en-us/windows/win32/dxtecharts/using-an-input-method-editor-in-a-game)
  （此篇為舊版 DirectX / IMM 文件，供理解獨佔全螢幕的 UI 限制。）
