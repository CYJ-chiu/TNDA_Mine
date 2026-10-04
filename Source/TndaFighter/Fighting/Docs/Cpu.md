# CPU 對手模組

## 責任

`AFightingGameMode` 在玩家初始化完成後建立唯一 CPU 並完成一對一配對；`AFightingCpuCharacter` 繼承共同的 `AFightingCharacter`；`AFightingCpuController` 保留日後接回 StateTree AI 的 Controller 基底。

原始碼：`FightingCpuCharacter.h/.cpp`、`FightingCpuController.h/.cpp`

## 出生設定與生成

關卡中的 `CpuStart` 是 Player Start Actor，`PlayerStartTag` 為 `CpuStart`。GameMode 使用它的 Transform 生成 CPU，建立玩家／CPU 雙向弱參照，再依雙方實際位置設定水平朝向；定位完成不等待落地。配對拒絕無效、自身或覆寫既有對手，不會改選最近角色；失敗時移除本次生成的 CPU 並停止開場。

GameMode 的 `Class Defaults > Fighting Match > Cpu Class` 必須指定有效且非抽象的 `AFightingCpuCharacter` 子類，使用 `AdjustIfPossibleButAlwaysSpawn` 處理出生碰撞。

## 設定錯誤

CPU 設定、出生點與生成錯誤統一交由 GameMode 回報；PIE 提示後停止對局，打包版保留 Error Log。完整契約見 [對局與出生](Match.md)。

## 角色與 AI 狀態

`AFightingCpuCharacter` 將 `AFightingCpuController` 設為 AI Controller 類別，但 `AutoPossessAI` 預設為 `Disabled`。`BP_FightingCpuCharacter` 即使保留 `BP_FightingCpuController` 類別設定，生成後也不會自動建立或掛上 AI Controller，因此目前不會自主移動。

舊有的 `ST_CombatEnemy` 已移除；只供它使用的 `EnvQuery_Evade`、`EnvQuery_Fallback` 與 `EnvQuery_Flank` 也已刪除。`BP_FightingCpuController` 仍保留原有的 `StateTreeAIComponent`，但 `StateTreeRef` 與參數資料均為空。重新實作 AI 時，需要建立新的行為資產並重新指定。

### CPU 測試模式

`AFightingCpuCharacter` 的 Class Defaults 內有 `CPU Test Movement` 類別屬性，用於 PIE 與教學驗收，不是正式 AI：

| 屬性 | 預設值 | 用途 |
| --- | --- | --- |
| `Test Movement Mode` | `關閉測試流程` | 預設不提供測試移動輸入。開啟後可選相對 1P 的靠近／遠離，各有慢與快四種模式；慢速使用 CPU 原本最高移速的 25%，快速使用 100%。 |

測試流程關閉時，CPU 不提供主動移動輸入。啟用後，CPU 每幀讀取共同基底提供的穩定對手方向；1P 換位時，CPU 會繼續選定的靠近或遠離行為，不依賴搜尋、開局畫面或攝影機方向。CPU 以 `CharacterMovement` 消耗測試輸入，即使 AI Controller 仍為 Disabled 也會實際移動並產生主動移動動畫。

## 角色面向

CPU 可在無 Controller 時使用共同 `TryExecuteCommand("X")` 與 `CancelAttack()`。Class Defaults 的 `BasicAttackMontage` 指定攻擊呈現；攻擊包含完整淡出期間不提供測試移動輸入，收尾後沿用原 `TestMovementMode` 恢復。這不新增自主選招或連發 AI。

`BP_FightingCpuCharacter.BasicAttackMontage` 與玩家共用 `/Game/Fighting/Anims/AM_Attack_01`。AutoPossessAI 及測試移動模式仍預設 Disabled。

CPU 停用 `bUseControllerRotationYaw`、Character Movement 的 `bUseControllerDesiredRotation` 與移動方向轉身。它和玩家一樣由 `AFightingCharacter` 寫入 Actor Yaw，測試移動或日後重新接回 AI Controller 都不能覆蓋共同對手面向。`ABP_Manny_Combat` 只根據 Gameplay 與 Character Movement 的結果選擇 Pose，不負責世界面向。

正常遊戲的 CPU Capsule 對 Pawn 維持原本的 `Block`。非 Shipping PIE 若要展示真正換邊，可使用 `tnda.Debug.FighterOverlap on`（也可用 `1`）暫時讓已配對雙方穿越；完全分離後必須使用 `off`（也可用 `0`）還原。指令不會改寫 CPU Blueprint 或 Class Defaults。

## 目前限制

AI 停用設定會套用到所有 `AFightingCpuCharacter` 子類。CPU 測試模式只提供相對 1P 的直線靠近或遠離，不會避障、選擇攻擊或取代日後的 StateTree。GameMode 不監聽 CPU 死亡，也不會自動補生下一名對手。
