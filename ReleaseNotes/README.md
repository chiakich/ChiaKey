# 平台更新說明

每項使用者可見變更新增一個 JSON 檔；已發布的片段保留，不再覆寫。

```json
{
  "changes": [
    {"platforms": ["macos"], "type": "fix", "description": "修正切換程式後輸入模式不一致的問題。"},
    {"platforms": ["windows"], "type": "feat", "description": "新增某項 Windows 功能。"},
    {"platforms": ["macos", "windows"], "type": "fix", "description": "修正兩平台都會遇到的使用者問題。"}
  ]
}
```

platforms 依實際交付行為填寫；core 改動不一定影響 macOS 前端。
內部重構、測試、CI 無須片段。description 為單行繁體中文，不含 Markdown 標題。

Beta 版從上個共同版本累積變更；正式版從上個正式共同版本累積，包含 Beta 系列。
兩平台分別生成 AI 摘要與原始 fallback；GitHub release 以平台分節，CDN notes
只包含該平台內容。首次共同發版的人工整理放在 first-joint-release.json。
