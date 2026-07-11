# 千秋輸入法 ChiaKey

<img height="600" alt="Image" src="https://github.com/user-attachments/assets/c43697e2-e22e-4c08-95eb-1cfe36028774" />  

千秋輸入法（ChiaKey）是一套給 macOS 使用的繁體中文注音輸入法。它以 Yahoo! 奇摩輸入法 / KeyKey / OpenVanilla 的開源程式碼為基礎，目標是保留 KeyKey 熟悉的輸入手感，並讓這套輸入法引擎能在現代 macOS 上繼續使用與維護。

本專案專注於修復與更新輸入法本體，詞庫資料則使用 [千秋輸入法綜合詞庫](https://github.com/chiakich/ChiaKey-Lexicon)，遇到缺詞、選字錯誤問題，請至該專案反應。

這不是 Yahoo 官方產品。原始碼依 BSD-style 授權釋出；詳見 [LICENSE](LICENSE)。Yahoo! 名稱與貢獻者姓名不得在未取得書面同意的情況下用於背書衍生產品。

> 如果喜歡的話歡迎贊助，費用將用於支付年度蘋果開發者帳號費用。

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/A0A21UAIV9)

## 下載

- [下載最新版 macOS 安裝包（.pkg）](https://github.com/chiakich/ChiaKey/releases/latest/)
- [查看所有歷史版本](https://github.com/chiakich/ChiaKey/releases/)

下載後開啟 `.pkg`，依照安裝程式指示完成安裝。第一次安裝後若未自動啟動，請到「系統設定 > 鍵盤 > 文字輸入」新增「千秋輸入法」。若安裝後系統仍顯示舊版本或找不到輸入法，登出再登入一次通常可以解決。

## 適合誰使用

千秋輸入法不是為了和所有現代注音輸入法競爭功能完整度而建立。這個專案的主要動機是紀念並延續 Yahoo! 奇摩輸入法 / KeyKey / OpenVanilla 這條歷史脈絡，盡可能保留 KeyKey 在 macOS 上的輸入節奏、候選窗體驗與 bigram 語言模型。

它比較適合下列使用者：

1. 懷念 Yahoo! 奇摩輸入法 的候選窗、組字與輸入節奏。
2. 想保存、研究或延續 KeyKey / OpenVanilla 這套開源輸入法引擎。
3. 偏好保守、小步維護的復興專案，而不是功能面非常寬的現代輸入法。
4. 希望詞庫 release、SQLite schema、fallback 與驗證規則圍繞 KeyKey 原有語言模型逐步整理。

如果你想找一套日常可用、功能更完整的現代注音輸入法，也可以參考：

1. macOS 內建注音輸入方式：不需要額外安裝，和系統整合度最高，適合需求單純、希望維持最少維護成本的使用者。
2. [小麥注音輸入法](https://github.com/openvanilla/McBopomofo)：長期維護的開源注音輸入法，架構較現代、專案焦點清楚。
3. [vChewing 唯音輸入法](https://vchewing.github.io/) 與其 [macOS repo](https://github.com/vChewing/vChewing-macOS)：功能面完整，支援多種鍵盤、拼音輸入、Sandbox、安全防護、輸出轉換與許多進階偏好設定。

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
