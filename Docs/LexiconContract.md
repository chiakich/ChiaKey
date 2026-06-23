# 千秋輸入法詞庫 Contract

決策狀態：已採納

最後更新：2026-06-23

這份文件定義千秋輸入法 app repo 與 `ChiaKey-Lexicon` release repo 之間的 contract。

App 必須能下載、驗證、安裝、拒絕與 fallback 詞庫 release，且不能破壞 user data。詞庫 repo 必須發佈符合這份 contract 的 release assets。

## Release 模式

詞庫 release 發佈位置：

```text
https://github.com/akira02/ChiaKey-Lexicon/releases
```

每個 release 必須提供：

1. `lexicon-manifest.json`
2. 一個 `ChiaKeySource.db` artifact
3. optional `metadata.json`

Generated DB 應上傳為 GitHub Release asset，不應 commit 到 app repo。

App-side installer：

```text
Scripts/install-lexicon-release.sh
```

偏好設定 app 應呼叫同一條 installer path，或等價的 shared validator。CLI 與 UI 不應分裂成兩套驗證規則。

## Manifest 必要欄位

`lexicon-manifest.json` 必須提供足夠資訊，讓 installer 找到並驗證 DB。

必要 top-level fields：

1. `version`
2. `database_schema_version`
3. `artifacts`

必要 database artifact fields：

1. `kind`: 必須是 `chiakey-source-db`
2. `url`
3. `filename`
4. `sha256`

過渡期相容：installer 會接受舊版 manifest 的 `keykey-source-db` kind，但新的 release 應全部改用 `chiakey-source-db`。

Optional metadata artifact fields：

1. `kind`: 建議是 `metadata`
2. `url`
3. `filename`
4. `sha256`

目前接受的 database schema version：

```text
1
```

App 必須在安裝前拒絕不支援的 schema version。

## 安裝 layout

外部詞庫安裝於：

```text
~/Library/Application Support/ChiaKey/Lexicons
```

Versioned layout：

```text
Lexicons/
  versions/
    2026.06.7/
      ChiaKeySource.db
      lexicon-manifest.json
      metadata.json
  active -> versions/2026.06.7
```

Active DB 路徑：

```text
~/Library/Application Support/ChiaKey/Lexicons/active/ChiaKeySource.db
```

更新必須使用 atomic symlink swap。下載失敗、checksum mismatch、SQLite validation failure 或 install failure 都必須保留既有 active lexicon。

## Runtime fallback

Startup 或 reload 時 runtime 嘗試順序：

1. external active lexicon
2. legacy external active lexicon
3. app 內建 fallback lexicon
4. legacy app 內建 fallback lexicon

Bundled fallback DB 必須足以離線使用與救援。它不需要永遠最新，但必須符合 runtime-critical schema。

如果外部 DB 存在但驗證失敗，app 應記錄 rejection，並繼續使用 bundled DB。

Legacy 路徑只為遷移期保留：

```text
~/Library/Application Support/ChiaKey/Lexicons/active/KeyKeySource.db
千秋輸入法.app/Contents/Resources/Databases/KeyKeySource.db
```

## 必要 SQLite tables

Release DB 必須包含：

1. `cooked_information`
2. `prepopulated_service_data`
3. `unigrams`
4. `bigrams`
5. `Mandarin-bpmf-cin`
6. `chiaki_db_metadata`
7. `chiaki_db_sources`

`chiaki_db_*` 是 schema v1 的歷史相容名稱。未來若要改成 `chiakey_db_*`，必須升級 schema version，並同時更新 app 與 lexicon repo。

目前 runtime 只需要 core tables 才能 open DB；release validation 必須檢查上列完整集合。

## 必要 metadata

`chiaki_db_metadata` 必須包含：

1. `schema_version` = `1`

`cooked_information` 必須包含：

1. 非空的 `version`

建議 metadata keys：

1. `lexicon_release_version`
2. `builder_version`
3. `build_time`
4. `source_count`
5. `unigram_count`
6. `bigram_count`
7. `candidate_count`

在 builder 穩定產生前，app 不應強制要求所有建議欄位。

## 必要輸入資料

DB 必須提供足夠資料，支援預設 Mandarin input method 與基本 fallback。

Release validation 目前要求：

1. `unigrams` 至少 1000 rows。
2. `Mandarin-bpmf-cin` 至少 1000 rows。
3. `unigrams` 至少 50 筆 `_punctuation_list`。
4. `Mandarin-bpmf-cin` 至少 50 筆 `_punctuation_list`。

Punctuation validation 目前要求：

1. `_punctuation_<` resolves to `，`
2. `_punctuation_Standard_<` resolves to `，`

這些檢查保護使用者可見的 `Shift+,` -> `，` 行為。

## 符號表資料

符號表使用 `prepopulated_service_data` 裡的 `canned_messages`。

必要 keys：

1. `canned_messages`
2. `canned_messages_timestamp`

`canned_messages` 必須是合法 plist，並包含至少一個 category dictionary 的 `CannedMessages` array。

當 release value 改變時，app 可以將資料複製到 user persistence DB。成功更新詞庫後，app 應 reload 或 merge，避免符號表仍然是空的。

## 禁止的 legacy data

OneKey service data 已不屬於千秋輸入法。

新詞庫 release 不應包含：

1. `onekey_services`
2. `onekey_services_timestamp`

舊 release 若仍包含這些 keys，現代 app 會忽略。新 release 應移除，CI 也可以把它視為失敗。

另見：

```text
Docs/LexiconOneKeyRemoval.md
```

## 驗證失敗政策

Installer 必須在以下狀況拒絕 release：

1. manifest 無法下載
2. manifest 缺少必要欄位
3. artifact 無法下載
4. SHA-256 驗證失敗
5. SQLite integrity check 失敗
6. 缺少必要 table
7. 必要 metadata 錯誤或缺失
8. minimum row-count checks 失敗
9. punctuation checks 失敗
10. `canned_messages` 缺失、太小、不是合法 plist，或沒有 categories

失敗時既有 active lexicon 必須保持不變。

## App repo 責任

App repo 負責：

1. download / install script
2. release validation rules
3. runtime fallback behavior
4. 偏好設定的 update UI
5. user-facing error messages
6. bundled fallback DB
7. compatibility documentation

驗證應保守。拒絕一個壞 release，比安裝後默默破壞輸入行為好。

## 詞庫 repo 責任

詞庫 repo 負責：

1. source attribution 與 license notes
2. normalized source data
3. deterministic DB builder
4. release manifest generation
5. checksum generation
6. publishing 前的 CI validation
7. changelog / release notes

詞庫 repo 應在發佈前執行等同 app installer 的檢查，包含 punctuation 與 symbol table checks。

## Schema evolution

Schema change 必須明確。

變更 DB schema 時：

1. 新增 `database_schema_version`
2. 更新這份文件
3. 更新 `Scripts/install-lexicon-release.sh`
4. 如有需要，更新 IMK loader 的 runtime validation
5. 為已安裝使用者保留 migration 或 fallback story
6. 發佈詞庫 release note 說明相容性

App 可以支援多個 schema versions，但不得默默接受未知 schema。

## User data separation

詞庫 release 是 shared release data，不得覆蓋：

1. user phrases
2. learned ranking state
3. user preferences
4. per-user keyboard layout choices
5. local opt-in personal corpus data

未來若要做個人學習，應實作為 release lexicon 上方的 overlay，不要直接修改下載來的 `ChiaKeySource.db`。

## Smoke test checklist

Release 在以下情境檢查前不算完整：

1. `ㄋㄧˇ ㄏㄠˇ` 可產生 `你好`。
2. 候選窗可開啟且選字可用。
3. `Shift+,` 產生 `，`。
4. 符號表可開啟且有分類。
5. 若產品包含倉頡與簡易，它們仍可載入。
6. 安裝 bad checksum 會保留舊 active lexicon。
7. 缺少 `canned_messages` 的 DB 會被拒絕。
8. 移除 active symlink 會 fallback 到 bundled DB。
9. 從偏好設定更新後可以乾淨 reload 或 relaunch input method。
