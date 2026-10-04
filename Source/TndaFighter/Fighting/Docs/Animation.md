# 動畫資料模組

## 責任

`UFightingAnimInstance` 是玩家與 CPU 共用的原生動畫資料來源。它在 `NativeInitializeAnimation()` 快取擁有角色與 `CharacterMovement`，再由 `NativeUpdateAnimation()` 更新 locomotion 資料。`ABP_Manny_Combat` 只保留 AnimGraph、狀態機、BlendSpace、Montage Slot 與動畫資產編排。

原始碼：`FightingAnimInstance.h/.cpp`

動畫藍圖：`/Game/Fighting/Anims/ABP_Manny_Combat`

## Locomotion 輸出

AnimGraph 只能讀取這些原生欄位：

- `Velocity`：動畫使用的速度。一般情況直接採用 `CharacterMovement::Velocity`。
- `Ground Speed`：`Velocity` 的水平長度，不包含垂直速度。
- `Direction`：`Velocity` 相對角色面向的 Unreal 標準動畫方向。
- `Should Move`：`Ground Speed > 3 cm/s` 且移動加速度非零。
- `Is Falling`：直接取自 `CharacterMovement::IsFalling()`。

`CharacterMovement::bOrientRotationToMovement` 開啟時，`Direction` 會限制在 `[-45, 45]`；關閉時保留完整方向。

## 受阻移動

角色仍有移動輸入，而且實際三維速度小於等於 `Blocked Movement Animation Speed` 時，動畫速度改用正規化加速度方向乘上門檻值。預設值是 `300 cm/s`，可在 `ABP_Manny_Combat` 的 Class Defaults 調整。

這段補償只改動畫資料，不會改角色的實際速度、碰撞或輸入。放開輸入後，`Should Move` 會變回 `false`，狀態機照原本規則回到 Idle。

## 生命週期與限制

預覽器、重新初始化或角色結束生命週期時，擁有者可能暫時不存在。原生類別會清除參照與輸出，等有效角色重新出現後再快取。

目前更新仍在遊戲執行緒的 `NativeUpdateAnimation()` 執行，沒有自訂 `FAnimInstanceProxy` 或 thread-safe update。若日後要把大量計算搬到動畫執行緒，必須先設計資料快照，不能直接從背景執行緒讀取 `ACharacter` 或 `UCharacterMovementComponent`。
