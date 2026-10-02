# 共同發版與舊更新器相容性

已採用方案：從 v1.2.7 起，macOS 與 Windows 共用 vX.Y.Z／vX.Y.Z-beta.N。
同一 release 必須同時包含 macOS .pkg 與 Windows Setup.exe；Windows 目前仍為
預覽版，GitHub 的 stable／Beta 標記依共同版本決定。

## 舊 macOS 介面

`https://cdn.chiaki.ch/chiakey/appcast.json` 永久保留 schema 1 和頂層 stable／beta。
每個 entry 的 tag、published_at、package_name、package_url、sha256、prerelease
保留原語意，始終指向 macOS .pkg。新增欄位不改變舊欄位。

CDN 失敗時，舊版讀 GitHub 第一頁 30 筆。從現在起所有公開的新 release 都有
macOS .pkg，且以可辨識的 v* 排序；不再加入 Windows-only release，避免版號
反超及 Windows release 把最新 macOS 推出第一頁的問題。既有 win-v0.1.0-beta.1
保留，版號低於已發布的 macOS 版本。

四組已發布實作（v1.2.0、v1.2.1、v1.2.2、v1.2.6）以原版 source/header 回歸
測試 legacy manifest 欄位、共同 release 的 .pkg 選取，以及接受／不接受 Beta。
原本早期版本不能辨識同數字版號不同 Beta 的限制仍存在，不宣稱能回溯修正。

## CDN 格式

新平台 feed：

- `/chiakey/updates/macos/appcast.json`
- `/chiakey/updates/windows/appcast.json`

沿用 schema 1、頂層 stable／beta，以降低資料模型與舊介面的差異：

```json
{
  "schema": 1,
  "platform": "windows",
  "stable": {
    "tag": "v1.2.7",
    "version": "1.2.7",
    "published_at": "<GitHub first publication in UTC>",
    "package_name": "ChiaKey-Windows-1.2.7-Setup.exe",
    "package_url": "https://cdn.chiaki.ch/chiakey/updates/windows/releases/v1.2.7/ChiaKey-Windows-1.2.7-Setup.exe",
    "sha256": "<64 lowercase hex characters>",
    "prerelease": false,
    "notes_url": "https://cdn.chiaki.ch/chiakey/updates/windows/releases/v1.2.7/release-notes-windows.md",
    "release_url": "https://github.com/chiakich/ChiaKey/releases/tag/v1.2.7"
  },
  "beta": {
    "tag": "v1.2.7",
    "version": "1.2.7",
    "published_at": "<GitHub first publication in UTC>",
    "package_name": "ChiaKey-Windows-1.2.7-Setup.exe",
    "package_url": "https://cdn.chiaki.ch/chiakey/updates/windows/releases/v1.2.7/ChiaKey-Windows-1.2.7-Setup.exe",
    "sha256": "<64 lowercase hex characters>",
    "prerelease": false,
    "notes_url": "https://cdn.chiaki.ch/chiakey/updates/windows/releases/v1.2.7/release-notes-windows.md",
    "release_url": "https://github.com/chiakich/ChiaKey/releases/tag/v1.2.7"
  }
}
```

上例為結構說明；正式版較新時 beta 與 stable 指向同一版。
beta 代表接受 Beta 者應選的最新版本，可指向 stable。穩定版不得覆蓋較新的
Beta；Beta 不得抹掉 stable。macOS feed 相同欄位，artifact 改為 .pkg；舊 feed
由同一結果產生。macOS 保留既有 `/chiakey/releases/檔名.pkg` 下載位置。

Windows CDN 與 GitHub SHA256SUMS.txt 交叉核對；CDN 失敗回到 GitHub，僅接受
有匹配 Windows installer 的 v*／win-v*。健康 CDN 沒有新版时不查 GitHub 列表。
正式共同版本提供 Windows installer，因此關閉接受 Beta 者也能更新 Windows；
Windows 的預覽成熟度與版本的 Beta 頻道為不同概念。

## 發布與 notes

兩平台使用同一提交的 Git bundle。所有產物與測試完成後才推送 tag；完整產物
先進 GitHub draft，再公開。任一平台建置失敗不會出現可見的半套 release。
流程序列化，禁止 rebase 到未建置的新 source；分支移動時重新執行。

CDN 先上傳不可變產物與 notes，再更新平台指標及 legacy feed。GET 只有 404
可初始化，其他錯誤停止。各指標更新為完整 object，跨指標不具有單一交易；
失敗時保留可用的舊或新版本，支援對同一份 artifacts 重試。

變更以 `ReleaseNotes/*.json` 標明 macos／windows；GitHub body 分平台，CDN
各自提供 notes。AI 只摘要該平台的清單，失敗回到相同清單。stable 從前次
stable 累積，包含該 Beta 系列。內部重構不自動列入使用者變更。

## 遷移與驗證

舊 Windows win-v0.1.0-beta.1 僅認 win-v*；使用者需手動安裝一次共同版本。
此次不另發布 Windows-only 過渡 release。

需通過：舊版 macOS manifest 與 fallback fixture、平台 notes 與 Beta 基準測試、
feed 合併及故障處理測試、macOS 雙架構建置、Windows x64／x86 建置、更新器
離線測試、installer 打包，以及整個共同 workflow 的 dry_run。

修改不代表已發版。合併與正式 v1.2.7 發布仍待使用者決定。
