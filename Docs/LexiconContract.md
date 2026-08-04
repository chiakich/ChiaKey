# 千秋輸入法詞庫 Contract

狀態：已採納

最後更新：2026-06-23

這份文件定義千秋輸入法 app repo 與 `ChiaKey-Lexicon` release repo 之間的 contract。

App 必須能下載、驗證、安裝、拒絕與 fallback 詞庫 release，且不能破壞 user data。詞庫 repo 必須發佈符合這份 contract 的 release assets。

## Release 模式

詞庫 release 發佈位置：

```text
https://github.com/chiakich/ChiaKey-Lexicon/releases
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
3. `filename`: 資料庫 release asset 的檔名；必須是安全的單一路徑元件並以 `.db` 結尾。可使用帶版本號的名稱，例如 `ChiaKeySource-2026.07.4.db`。
4. `sha256`

Installer 與 release packaging 只接受 `chiakey-source-db` artifact。release asset 可使用版本化 `.db` 檔名；安裝時一律重新命名為 `ChiaKeySource.db`。舊版 `keykey-source-db` 或 `KeyKeySource.db` 只保留為 runtime migration fallback，不再是合法的新 release artifact。

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
  pending-verification
```

Active DB 路徑：

```text
~/Library/Application Support/ChiaKey/Lexicons/active/ChiaKeySource.db
```

更新必須使用 atomic symlink swap。下載失敗、checksum mismatch、SQLite validation failure 或 install failure 都必須保留既有 active lexicon。

## 舊版本保留與回退

安裝本身不刪除任何舊版本，只寫下 `pending-verification` 標記。標記兩行：第一行是待驗證的版本，第二行是它取代的版本（首次安裝為空）。

Runtime 載入後結算，兩種結果只會發生一種：

1. **開起 active DB** → `--prune-superseded`：刪除 active 以外的所有版本並清掉標記。穩定狀態下 `versions/` 只有 active 一份。
2. **active 存在但開不起來** → `--rollback`：把 `active` 指回標記第二行的版本、刪掉失敗的版本、清掉標記，然後重新載入一次。沒有可退的版本時改為移除 `active` symlink。

回退是重指 symlink，而不是讓 runtime 去挑別的檔案。`active` 必須是唯一真相：偏好設定顯示的版本、`--skip-current` 比對的版本、實際使用的資料庫必須永遠是同一個。讓 runtime 靜默改用別的 DB 會使 `--skip-current` 讀到一個永遠載不起來的版本號，從此每次更新都跳過，使用者被永久卡在 bundled DB。

標記在回退時一併清除，因此每次安裝最多只回退一次；上一版也載不起來就照 fallback 順序停在 bundled DB。`active` 已經不是標記所指版本時（狀態已改變），只清標記、不刪任何東西。

Prune 與 rollback 都由 runtime 驅動，因此輸入法從未載入過該詞庫的機器（例如只用 CLI 安裝）不會觸發。偏好設定 app 安裝後發出的 reload 請求必須走同一條結算路徑，否則從 UI 更新的失敗詞庫要等到下一輪定時檢查才會被回退。

`metadata.json` 中 `version` 為 `dev` 的目錄由本機建置產生、不可重新下載，prune 不得刪除。

## Runtime fallback

Startup 或 reload 時 runtime 嘗試順序：

1. external active lexicon
2. legacy external active lexicon
3. app 內建 fallback lexicon
4. legacy app 內建 fallback lexicon

第 1 項存在卻載入失敗時，runtime 會先回退到上一版並重新載入（見上節）；bundled DB 是回退也救不回來時才停留的位置。

Bundled fallback DB 必須足以離線使用與救援。它不需要永遠最新，但必須符合 runtime-critical schema。

如果外部 DB 存在但驗證失敗，app 應記錄 rejection，並繼續使用 bundled DB。

Legacy 路徑只為遷移期保留：

```text
~/Library/Application Support/ChiaKey/Lexicons/active/KeyKeySource.db
ChiaKey.app/Contents/Resources/Databases/KeyKeySource.db
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
6. bundled fallback DB 的放置、驗證與使用流程
7. compatibility documentation

App repo 不負責保存 raw lexicon source，也不負責從 source build release DB。

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
10. 更新並成功載入後，`versions/` 只剩 active（dev 版本除外），標記已清除。
11. 新 DB 無法載入時，`active` 回退到上一版、失敗的版本被刪除、標記清除，且 runtime 跑在上一版而非 bundled DB。
12. 回退後 `--skip-current` 讀到的是上一版版本號，下一輪自動更新會重新嘗試安裝。
