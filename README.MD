# 千秋輸入法 ChiaKey
<img width="256" height="256" alt="千秋輸入法 ChiaKey" src="Docs/images/chiakey-icon.png" />

<img height="400" alt="Image" src="https://github.com/user-attachments/assets/c43697e2-e22e-4c08-95eb-1cfe36028774" />  

千秋輸入法（ChiaKey）是一套給 macOS 使用的繁體中文注音輸入法。它以 Yahoo! 奇摩輸入法 / KeyKey / OpenVanilla 的開源程式碼為基礎，目標是保留 KeyKey 熟悉的輸入手感，並讓這套輸入法引擎能在現代 macOS 上繼續使用與維護。

本專案專注於修復與更新輸入法本體，詞庫資料則使用 [千秋輸入法綜合詞庫](https://github.com/chiakich/ChiaKey-Lexicon)，遇到缺詞、選字錯誤問題，請至該專案反應。

這不是 Yahoo 官方產品。原始碼依 BSD-style 授權釋出；詳見 [LICENSE](LICENSE)。Yahoo! 名稱與貢獻者姓名不得在未取得書面同意的情況下用於背書衍生產品。

## 下載

- [下載最新版 macOS 安裝包（.pkg）](https://cdn.chiaki.ch/chiakey/ChiaKey.pkg)
- [查看所有歷史版本](https://github.com/chiakich/ChiaKey/releases/)

安裝包會裝到目前使用者的 `~/Library/Input Methods`。
第一次安裝後若未自動啟動，請到「系統設定 > 鍵盤 > 文字輸入」新增「千秋輸入法」。若安裝後系統仍顯示舊版本或找不到輸入法，登出再登入一次通常可以解決。

備註：若需要全域安裝（讓同一台 Mac 上所有帳號都能使用），或用 MDM 批次部署，請使用命令列：

```sh
sudo installer -pkg ChiaKey.pkg -target /
```

此命令將會安裝到 `/Library/Input Methods`，需要管理員權限。

### 從 Yahoo! 奇摩輸入法搬家

如果這台 Mac 上還留著 Yahoo! 奇摩輸入法，可以在「千秋輸入法偏好設定」的「自訂詞」分頁按「匯入 Yahoo! 奇摩輸入法資料…」，把當年的自訂詞、學習過的選字記錄，以及自訂訊息與 .cin 表格一併帶過來。

舊版的詞庫檔是加密的，只有那套輸入法本身讀得開，所以千秋輸入法是請它自己匯出。這代表舊版程式必須還能執行：它沒有 Apple Silicon 版本，在 M 系列晶片上需要安裝 Rosetta 2；而且匯出指令要靠執行中的舊輸入法回應，若按下按鈕後顯示沒有回應，請先切換到 Yahoo! 奇摩輸入法隨便打一個字把它喚醒，再回來按一次。

匯入不會覆蓋現有的自訂訊息或表格檔，重複的自訂詞也會略過。

### 解除安裝

在「千秋輸入法偏好設定」的「更新」分頁按「移除 ChiaKey…」即可。預設會保留使用者自訂詞與設定（之後重新安裝可以直接接續使用）；勾選「同時刪除使用者自訂詞與設定」則會徹底移除。

也可以直接執行解除安裝 script：

```sh
Scripts/uninstall.sh
```

加上 `--purge` 會一併刪除自訂詞、詞庫與設定。全域安裝（`/Library/Input Methods`）需要另外以 `sudo rm -rf '/Library/Input Methods/ChiaKey.app'` 移除。

## 適合誰使用

千秋輸入法不是為了和所有現代注音輸入法競爭功能完整度而建立。這個專案的主要動機是紀念並延續 Yahoo! 奇摩輸入法 / KeyKey / OpenVanilla 這條歷史脈絡，盡可能保留 KeyKey 在 macOS 上的輸入節奏與候選窗體驗，並重建當年未曾釋出的 bigram 語言模型。

它比較適合下列使用者：

1. 懷念 Yahoo! 奇摩輸入法 的候選窗、組字與輸入節奏。
2. 想保存、研究或延續 KeyKey / OpenVanilla 這套開源輸入法引擎。
3. 偏好保守、小步維護的復興專案，而不是功能面非常寬的現代輸入法。
4. 希望詞庫 release、SQLite schema、fallback 與驗證規則圍繞 KeyKey 的語言模型架構逐步整理。

如果你想找一套日常可用、功能更完整的現代注音輸入法，也可以參考：

1. macOS 內建注音輸入方式：不需要額外安裝，和系統整合度最高，適合需求單純、希望維持最少維護成本的使用者。
2. [小麥注音輸入法](https://github.com/openvanilla/McBopomofo)：由 OpenVanilla 社群長期維護的開源注音輸入法，與 Yahoo 輸入法有共同的淵源，架構現代、專案焦點清楚。
3. [vChewing 唯音輸入法](https://vchewing.github.io/) 與其 [macOS repo](https://github.com/vChewing/vChewing-macOS)：功能面完整，支援多種鍵盤、拼音輸入、Sandbox、安全防護、輸出轉換與許多進階偏好設定。

## 關於詞庫與 bigram

Yahoo! 奇摩輸入法當年為人稱道的選字品質，很大一部分來自一套 bigram 語言模型。它決定了同音時「打完前一個詞，接哪個字才合理」。但這套 bigram 模型當年並未隨程式碼一起開源，因此無法沿用。

千秋輸入法的 bigram 是重新建構的：以既有開源詞庫為基底，疊加 Mozilla Common Voice 句料，以及參照中央研究院平衡語料庫主題分佈、由大型語言模型生成再經審核提煉的台灣口語語料，透過一條可重現、來源可追蹤的 pipeline 產生。

這是本專案投入最深的部分。完整的資料層、來源審查與整合流程記錄在 [千秋輸入法綜合詞庫](https://github.com/chiakich/ChiaKey-Lexicon)。

## 匯入自訂輸入法（cin 表格）

除了注音、倉頡與速成之外，千秋輸入法也保留了 OpenVanilla 的通用表格引擎，可以載入 `.cin` 對照表。倉頡、行列、大易、粵語拼音、客語、台語等以「打碼查字」為主的輸入法，都是這種格式。

在「千秋輸入法偏好設定 > 自訂輸入法」按 **+** 選擇 `.cin` 檔即可匯入。匯入前會先顯示表格名稱、字詞數與選字鍵讓你確認；Big5 編碼的舊表格會自動轉成 UTF-8。匯入完成後，該輸入法就會出現在輸入選單中，也可以在「一般」分頁取消勾選，讓它不顯示在選單上。

選取清單中的表格再按 **−** 即可移除。只有你自己匯入的表格會列在這裡，內建的四個輸入法不受影響。

表格檔會存放在：

```
~/Library/Application Support/ChiaKey/DataTables/Generic/
```

也可以直接把 `.cin` 檔放進這個資料夾（檔名請使用英數字），登出再登入即可生效。
注意：請不要把檔案放進 app bundle 內部，那會讓程式安全性簽章失效，並且更新時會被覆蓋。

表格檔可以從 [chinese-opendesktop/cin-tables](https://github.com/chinese-opendesktop/cin-tables) 取得。該倉庫整體採用 CC0，但個別檔案可能另外標註授權，例如廣東話的 `cantonhk.cin` 在檔頭寫明是 GPL，而 `jyutping.cin`（粵拼）、`ile.cin`（廣東拼音）、`zyujam.cin`（粵語注音）則標為 Public Domain。使用前請自行確認檔案本身的授權說明。


## 目前狀態

這個 fork 的上游脈絡來自官方封存的 `YahooArchive/KeyKey`，目前的現代化工作則起始於 `vChewing/KeyKey-Boneyard` snapshot。

- 官方 Yahoo 封存: <https://github.com/YahooArchive/KeyKey>
- 最早 GitHub fork root: <https://github.com/Yi-Kai/KeyKey>
- Boneyard snapshot: <https://github.com/vChewing/KeyKey-Boneyard>

Note:

- `ChiaKey` 是 `Chiaki` 與 `KeyKey` 結合的諧音哏。
- 目前發佈主線維護現代 macOS InputMethodKit 版本。
- bundle id / TIS id 使用 `com.chiakey.inputmethod.ChiaKey`。
- 使用者資料路徑使用 `~/Library/Application Support/ChiaKey`。
- 詞庫由獨立 repo `ChiaKey-Lexicon` 發佈。
- 新增實驗性的 `ChiaKeyCore` host-neutral engine facade，作為獨立 iOS host repo 可共用的輸入核心地基。

需使用支援 Apple Silicon 的現代 Xcode；目前已在 Xcode 26.5 驗證。

## 文件

- [CONTRIBUTING.md](CONTRIBUTING.md)：編譯、測試、release packaging、詞庫更新測試與維護邊界。
- [架構](Docs/Architecture.md)：產品範圍、runtime 路線、legacy cleanup 原則與測試基線。
- [專案目錄結構](Docs/ProjectStructure.md)：source tree 分層、host 邊界與獨立 iOS host repo 關係。
- [iOS 實作指南](Docs/iOSImplementation.md)：iOS keyboard extension 的 core 接法、repo 邊界與驗證入口。
- [詞庫 contract](Docs/LexiconContract.md)：GitHub Release assets、SQLite 必要資料、驗證規則與 fallback 行為。
- [Release packaging](Docs/ReleasePackaging.md)：正式 `.pkg` installer、簽章與 notarization 流程。
- [現代化 roadmap](Docs/ModernizationPlan.md)：已完成基線、下一步與延後事項。
- [OneKey 移除說明](Docs/LexiconOneKeyRemoval.md)：給詞庫 repo 的相容性說明。
