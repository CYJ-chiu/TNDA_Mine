# Fighting 原始碼協作規則

本文件套用於 `Fighting/` 下的原始碼與文件，並與上層 `Source/AGENTS.md` 一併遵守。

## 文件對應

修改 Fighting 功能前，先依下表讀取所屬功能文件。新增、修改或刪除行為時，在同一批改動內更新該文件。

| 原始碼 | 功能文件 |
|---|---|
| `FightingGameMode.h/.cpp` | `Docs/Match.md` |
| `FightingPlayerController.h/.cpp`、`FightingPlayerCharacter.h/.cpp` | `Docs/Player.md` |
| `FightingCameraActor.h/.cpp` | `Docs/Camera.md` |
| `FightingCpuCharacter.h/.cpp`、`FightingCpuController.h/.cpp` | `Docs/Cpu.md` |

下列變更必須同步更新功能文件：

- 類別責任、公開介面、Unreal 生命週期或跨類別協作。
- Blueprint／Editor 設定位置、預設值、單位或有效範圍。
- 錯誤提示、失敗行為、備援路徑或目前未實作的限制。

新增不屬於現有四個模組的 Fighting 功能時，建立對應的 `Docs/*.md`，並在 `README.md` 加入索引。刪除模組時，一併移除過期文件與索引。變更跨越多個模組時，更新每一份受影響的文件；只有模組關係或啟動順序改變時，才需要修改 `README.md` 的總覽。

## 完成條件

- 將本次每個 Fighting 原始碼變更對照上表，確認所屬功能文件已更新，或確認既有說明仍完全正確。
- 新增或刪除模組後，確認 `README.md` 的索引與實際 `Docs/` 內容一致。
- 檢查 `README.md` 與 `Docs/*.md` 的相對連結均可解析。
