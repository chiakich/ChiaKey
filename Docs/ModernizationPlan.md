# Chiaki KeyKey 現代化方向

更新日期：2026-06-22

這份文件整理 Chiaki KeyKey 目前的現代化方向。目標不是把 Yahoo KeyKey 全部推翻，而是保留它在 macOS 上已經證明穩定的輸入體驗，再逐步替換掉不適合長期維護的資料流程與工程結構。

## 目標

Chiaki KeyKey 的短中期目標是：

1. 讓現有 macOS IMK 版本能穩定編譯、安裝、測試。
2. 保留 Yahoo KeyKey 在 Electron / Chromium app 裡相對穩定的老派 UI 路線。
3. 重建詞庫與語言模型資料流程，支援未來自動更新。
4. 逐步分離 macOS host、輸入引擎、詞庫資料、偏好設定，讓之後要用 Swift 或 Rust 替換部分模組時有清楚邊界。
5. 將長期維護的詞庫資料移到獨立 repo，避免與輸入法本體耦合。

非目標：

1. 短期內不做完整 Swift/Rust 重寫。
2. 短期內不恢復 Windows loader 或舊 Carbon TSM loader。
3. 短期內不追求 vChewing 的所有功能與設定量。
4. 不在輸入法 runtime 內直接抓未知遠端資料並覆蓋主詞庫。

## 目前專案定位

原始 Yahoo KeyKey 不是純 mac 專案，repo 內仍保留：

- `Loaders/OSX-IMK`：目前 Chiaki KeyKey 的主要 macOS runtime。
- `Loaders/OSX-TSM`：舊式 Carbon/TSM 路線，現代 macOS 不應再投入。
- `Loaders/Windows-IMM`：舊 Windows IMM loader，與 Chiaki KeyKey 目前目標無關。
- `Frameworks/*`、`ModulePackages/*`：跨平台 C++ core 與輸入法模組。
- `Distributions/Takao/DataSource`：目前 mac 版本實際使用的資料。

Chiaki KeyKey 應該把 active path 收斂成：

```text
macOS IMK host
  -> OpenVanilla / PlainVanilla bridge
  -> OVIMMandarin
  -> Manjusri language model
  -> versioned lexicon databases
```

Windows、OSX-TSM、舊 installer 與研究性資料可以先視為歷史資料，不納入現代化主線。

## Repo 邊界

詞庫應該獨立成另一個 repo，例如：

```text
Chiaki-KeyKey-Lexicon
```

主 repo `Chiaki-KeyKey` 負責：

1. macOS IMK runtime。
2. 輸入引擎與資料庫讀取邏輯。
3. DB schema 與 builder scripts。
4. 本機安裝、編譯、測試工具。
5. 一份可離線使用的內建 fallback DB。

詞庫 repo `Chiaki-KeyKey-Lexicon` 負責：

1. 詞庫來源 manifest。
2. 來源授權與 attribution。
3. normalized 中介詞庫資料。
4. 建好的 release DB。
5. checksum / signature。
6. 詞庫版本 changelog。

這樣分開有幾個好處：

1. 詞庫更新頻率通常比 app 高，不會讓主程式 repo 被資料更新洗版。
2. 詞庫授權比程式授權複雜，獨立 repo 比較容易追蹤每個來源。
3. 建好的 DB 可能很大，適合放 GitHub Releases，不適合反覆 commit 到主 repo。
4. 自動更新可以只指向詞庫 repo 的 manifest 與 release assets。
5. 主 repo 可以只保留穩定 fallback DB，確保離線與更新失敗時仍可使用。

主 repo 仍可保留少量 seed data 與測試資料，但不應長期承擔完整公開詞庫的版本歷史。

## 組字窗與 Electron 策略

目前觀察到一個重要事實：Yahoo KeyKey 的老派 macOS 實作在 Electron app 裡反而不差。

原因大致是：

1. 組字文字仍透過 `setMarkedText` 交給 client。
2. 候選窗、提示窗、搜尋窗等 UI 是輸入法自己畫的 `NSWindow` / `NSPanel`。
3. 候選窗位置由輸入法根據 client 回傳的 line height rect 自行修正。
4. marked text 樣式相對保守，沒有把太多複雜視覺狀態交給 client 畫。

這代表我們不需要一開始就做完整的 per-client mitigation。更好的策略是：

1. 保留現在「自畫候選窗」的路線。
2. 不主動把所有組字內容改成浮動組字窗。
3. 暫時不加入龐大的 Electron app 偵測清單。
4. 只在實測發現某個 app 明確有問題時，再加入小範圍 workaround。

vChewing 值得參考的不是「全部照做」，而是它把 IMK 相容問題拆得很清楚：

- Electron / Chromium client 的 IMKTextInput 行為可能不穩。
- 浮動組字窗可以作為 fallback。
- 太複雜的 inline marked text 樣式要能降級。
- Shift / Caps Lock 這類 modifier event 在 Electron 裡可能需要去彈跳。
- per-client 設定比全域硬切換更安全。

Chiaki KeyKey 目前的判斷是：先不要重做這整套 mitigation；保留 Yahoo 的穩定基線，之後只補必要的相容層。

## 詞庫自動更新設計

詞庫更新應該被設計成可追蹤、可驗證、可回復，而不是讓輸入法 runtime 任意覆蓋資料。

建議分成五層：

```text
lexicon repo release manifest
  -> fetch and verify
  -> normalize
  -> build database
  -> install versioned database
```

### 1. Source Manifest

在 `Chiaki-KeyKey-Lexicon` 新增版本化 manifest，描述每個詞庫來源與 release DB：

```json
{
  "schema": 1,
  "version": "2026.06.dev",
  "sources": [
    {
      "id": "chiaki-modern-phrases",
      "url": "https://example.invalid/phrases.tsv",
      "format": "tsv",
      "license": "TBD",
      "sha256": "TBD",
      "priority": 100,
      "enabled": true
    }
  ],
  "artifacts": [
    {
      "id": "smart-mandarin-db",
      "url": "https://github.com/akira02/Chiaki-KeyKey-Lexicon/releases/download/2026.06/KeyKeySource-2026.06.db",
      "sha256": "TBD",
      "schema_version": 1
    }
  ]
}
```

每個來源至少要有：

- `id`
- `url`
- `format`
- `license`
- `sha256` 或簽章
- `priority`
- `enabled`

每個 release artifact 至少要有：

- `id`
- `url`
- `sha256` 或簽章
- `schema_version`

沒有授權資訊的資料只能放進本地實驗，不應成為預設下載來源。

### 2. Fetch And Verify

新增下載工具，例如：

```text
Scripts/update-lexicon-sources.rb
```

責任：

1. 讀取 manifest。
2. 下載詞庫原始檔。
3. 驗證 `sha256` 或簽章。
4. 記錄實際下載時間、來源 URL、checksum。
5. 若驗證失敗，保留現有詞庫，不產生新 DB。

這個工具可以先給開發者手動跑，之後再接到 `Chiaki-KeyKey-Lexicon` 的 GitHub Actions 或偏好設定 app。

### 3. Normalize

不同來源的格式先轉成 Chiaki KeyKey 自己的中介格式，例如：

```text
reading<TAB>phrase<TAB>weight<TAB>source_id
```

原則：

1. 所有注音讀音要正規化成同一套內部表示。
2. 同詞同音可以合併權重，但要保留來源 metadata。
3. 明顯不適合預設詞庫的內容要能 filter。
4. 簡繁資料不要混在同一條路徑裡硬轉。
5. 人工補詞與自動語料要能分層。

### 4. Build Database

現有 `Scripts/build-dev-smart-mandarin-db.rb` 已經能建出現代化的 `KeyKeySource.db`。下一步應該把它改成正式 builder：

```text
Scripts/build-smart-mandarin-db.rb
```

建議輸出：

```text
Build/Lexicons/KeyKeySource-2026.06.22.db
Build/Lexicons/KeyKeySource-2026.06.22.json
```

JSON sidecar 記錄：

- DB schema version
- 詞庫版本
- 來源清單
- checksum
- unigram / bigram / candidate row counts
- builder 版本
- build time

建好的 DB 不應直接 commit 回主 repo。開發期可以留在 `Build/Lexicons`，正式發佈時由 `Chiaki-KeyKey-Lexicon` 的 GitHub Releases 提供下載。

### 5. Install Versioned Database

不要直接覆蓋 app bundle 裡的 DB。建議 runtime 搜尋順序：

1. `~/Library/Application Support/Chiaki KeyKey/Lexicons/active/KeyKeySource.db`
2. app bundle 內建 `KeyKeySource.db`

更新時採用 atomic swap：

```text
Lexicons/
  versions/
    2026.06.22/
      KeyKeySource.db
      metadata.json
  active -> versions/2026.06.22
```

如果 active DB 損壞或 schema 不相容，runtime 應 fallback 到 app bundle 內建 DB。

## 更新模式

可以分三階段做：

### Phase 1: Developer Update

只提供開發者指令：

```text
Scripts/update-lexicon-sources.rb
Scripts/build-smart-mandarin-db.rb
Scripts/install-dev-input-method.sh
```

這階段先把資料流程固定下來，不碰 UI。

### Phase 2: Release-time Update

透過 `Chiaki-KeyKey-Lexicon` 的 GitHub Releases 發佈已建好的 DB，並在 release manifest 中記錄版本、schema、checksum 與來源。

使用者安裝新版本 app 時會拿到新的內建 DB。這是最安全的自動更新形式。

### Phase 3: Runtime Optional Update

偏好設定 app 可以提供「檢查詞庫更新」：

1. 從 `Chiaki-KeyKey-Lexicon` 下載 signed manifest。
2. 顯示版本與來源。
3. 下載 DB。
4. 驗證 checksum / signature。
5. 安裝到 Application Support。
6. 下次切換輸入法或重新載入 engine 時啟用。

輸入法 process 本身不應在按鍵處理途中做網路、解壓縮或 DB 寫入。

## 候選詞庫來源原則

可研究的來源：

1. 目前 Chiaki KeyKey 自己整理的 modern phrase list。
2. libchewing / 新酷音相關公開資料。
3. vChewing / 先鋒語料庫作為架構參考。
4. 政府或學術公開資料，如教育部資料、開放授權語料。
5. 使用者自訂詞與本地學習資料。

注意事項：

1. vChewing 可以參考工程設計，但不應在未確認授權與資料來源前直接匯入詞庫。
2. GPL / LGPL / MIT / CC BY / CC0 的相容性要逐一確認。
3. Yahoo 原始智慧詞庫沒有完整公開，不能假設可以取回。
4. 任何個人語料都只能做本機 opt-in，不應進入公開預設詞庫。
5. 主 repo 不應直接 vendoring 第三方大詞庫；若需要收錄，應先進詞庫 repo，並附上授權、來源與轉換流程。

## 建議實作順序

1. 寫 `Docs/Architecture.md`，正式標出 active mac path 與 legacy path。
2. 建立 `Chiaki-KeyKey-Lexicon` repo。
3. 將 `build-dev-smart-mandarin-db.rb` 拆成可重用 builder。
4. 在詞庫 repo 新增 manifest 與 source cache 結構。
5. 新增 normalized TSV 中介格式。
6. 讓 app 支援 Application Support 內的 active DB fallback。
7. 加入 DB metadata 檢查與版本 log。
8. 做 Electron 測試清單，但不先做全域 mitigation。
9. 等詞庫更新流程穩定後，再做偏好設定 UI。

## 測試矩陣

輸入行為：

- `ㄋㄧˇ ㄏㄠˇ` 應可組成 `你好`
- `Shift+,` 應輸出全形逗號
- 候選窗叫出、翻頁、選字、取消
- 中英切換
- app 切換後 composition state 清理

App 類型：

- TextEdit
- Safari
- Chrome
- VS Code
- Discord
- Slack
- Notion
- Obsidian
- Terminal

詞庫更新：

- checksum 錯誤時不安裝
- schema 不相容時 fallback
- active DB 損壞時 fallback
- 使用者自訂詞不被詞庫更新覆蓋
- 更新後可回復上一版

## 開放問題

1. 詞庫 repo 是否命名為 `Chiaki-KeyKey-Lexicon`，或改用 `Chiaki-KeyKey-Dictionaries`？
2. runtime 是否需要熱重載 DB，還是重新啟用輸入法才切換？
3. 使用者自訂詞要沿用 Manjusri 既有 user cache，還是搬到新的 overlay DB？
4. 偏好設定 app 要繼續沿用舊 Cocoa UI，還是另開 SwiftUI app？
5. 是否要為 Electron app 提供手動「使用浮動組字窗」選項？

目前答案傾向保守：

- 詞庫先由獨立 repo 的 GitHub Releases 發佈，app release 內建一版穩定 DB。
- runtime 不做熱重載。
- 使用者詞庫先不大改，只加保護與匯出。
- Electron mitigation 先不全域自動化，等實測需要再加。
