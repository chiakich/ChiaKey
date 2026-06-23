# 詞庫 release note：移除 OneKey service data

千秋輸入法已從現代 macOS app 移除 Yahoo KeyKey 時代的 OneKey 功能。

OneKey 不是輸入詞庫。它是一個 Yahoo-era URL launcher：app 會載入 Yahoo Search、台股查詢、無名搜尋、Yahoo Auction、Yahoo Maps 等 web service plist，然後用使用者輸入開啟 URL。

歷史 DB key：

- `prepopulated_service_data.key = 'onekey_services'`
- 歷史 build 也可能提到 `onekey_services_timestamp`

現代千秋輸入法不再載入 OneKey module，不再 fetch 或 merge OneKey plist data，不再顯示 OneKey 偏好設定，也不再使用 backtick key (`) 作為 OneKey shortcut。

## 詞庫 repo 應做的事

未來 `ChiaKey-Lexicon` release 應省略 OneKey data。

必要動作：

1. 不產生、不發佈 `onekey_services`。
2. 不產生、不發佈 `onekey_services_timestamp`。
3. 移除任何要求這些 keys 存在的 CI assertion。
4. 保留既有 `prepopulated_service_data` table。
5. 保留 `canned_messages` 與 `canned_messages_timestamp`；它們仍被符號表使用。
6. 保留 punctuation tables 與 symbol data validations。

可選 cleanup：

CI 可以在 release 含有 `onekey_services` 時失敗，避免新的詞庫不小心保留過時 Yahoo web-service data。

相容性註記：

舊詞庫 release 若仍包含 `onekey_services`，新版千秋輸入法會忽略這個 key。新的 release 應移除它，讓 DB contract 聚焦在輸入資料。
