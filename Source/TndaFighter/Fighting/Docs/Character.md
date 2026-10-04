# 共同角色模組

## 責任

`AFightingCharacter` 是玩家與 CPU 共用的抽象原生基底，保存當次對局唯一的弱對手參照，並提供戰鬥中點、穩定的水平對手方向、相對方向分類與共同自動面向。這個類別不搜尋、選擇或更換對手。

原始碼：`FightingCharacter.h/.cpp`

## 對手配對

`AFightingGameMode` 在標準玩家初始化後生成唯一 CPU，再把玩家與 CPU 設為彼此的對手。`Opponent` 是 `Transient` 的弱參照，可由 Blueprint 唯讀觀察；角色被銷毀或 World 結束時會自然失效。本階段不處理死亡補生、回合重設或重新配對。

`SetOpponent()` 拒絕無效角色、自己，以及把已有有效對手的角色改配給另一人；相同對手可安全重複指定。Gameplay 不會以距離、畫面左右或 Actor Tag 選擇替代角色。

## 共用位置資料

`TryGetCombatMidpoint()` 回傳雙方世界位置的中點。尚未配對或對手已失效時會輸出零向量並回傳 `false`。

共同基底每幀更新由自己指向對手的水平單位向量。雙方水平距離大於 `10 cm` 時更新 `LastValidOpponentDirection`；距離不大於門檻時沿用最後有效值。若配對後尚未出現有效方向，`TryGetOpponentDirection()` 會輸出零向量並回傳 `false`，呼叫端不得自行推測方向。

## 一般地面狀態

`IsInNormalGroundState()` 只在角色未受攻擊鎖定、落地、未播放 Montage 且未使用 Root Motion 時回傳 `true`。玩家推擠與共同自動面向都使用這個判定，不另外建立第二套生命週期規則。

## 基本攻擊生命週期

`TryExecuteCommand(FName)` 是 BlueprintCallable 的共用入口，本週只接受 `X`。需要有效對手、Mesh、AnimInstance、非零長度的 `BasicAttackMontage` 與一般地面資格；已有任何 Montage 或攻擊鎖定時直接拒絕，不預約、不停止別人的播放。CPU 不要求 Controller；玩家覆寫入口另檢查控制權。

Class Defaults 的 `Fighting Character > Attack > Basic Attack Montage` 由角色 Blueprint 指定單一資產，必須非循環且開啟 Auto Blend Out。沒有配置時會以包含角色、語意、Montage 與原因的 Warning 拒絕，既有移動仍可使用。未知語意與一般操作中的重複攻擊只回傳 false。

玩家與 CPU 均指定 `/Game/Fighting/Anims/AM_Attack_01`：使用 `MM_Attack_01` 完整 0～1 秒片段、Play Rate 1、Loop Count 1，Default Section 的 Next 為 None，Slot 為 `DefaultGroup.DefaultSlot`。Blend In 為 0.10 秒、Blend Out 為 0.15 秒、Auto Blend Out 開啟、Trigger Time 為 -1；可在 Montage Editor 編輯，生命週期不硬編碼這些秒數。

播放成功並取得 instance 後才提交 Transient、BlueprintReadOnly 的 `bIsAttacking` 與 `CurrentCommand`。開始時清除待消耗輸入、主動速度／加速度及衍生角色推擠意圖，保留方向語意。每次播放只對自己的 instance 執行 `PushDisableRootMotion()`；不改 Sequence 或 AnimBP 的全域 Root Motion 設定。

`CancelAttack()` 使用資產 Blend Out 請求中斷，已淡出時不延長時間。鎖定包含正常與中斷淡出，直到原 AnimInstance／instance ID 的 `OnMontageEnded` 才清除；同資產的舊事件不能清除新攻擊。收尾只在原 instance 仍存活時對應 Pop。沒有固定秒數或幀數計時器。

Tick 在更新面向前檢查離地／對手失效並請求中斷；播放者重建或 instance 消失時記錄原因並直接清理。EndPlay 先使回呼失效再釋放本次播放。收尾不改 Movement Mode、位置、Yaw、Controller 或對手；重力、碰撞與攝影機最大距離修正始終有效。

## 共同自動面向

共同基底將最後有效水平對手方向換算成 BlueprintReadOnly `DesiredFacingYaw`，並由 Character Gameplay 寫入 Actor Rotation。第一次取得有效方向時立即朝向對手；之後以 `FMath::RInterpTo` 平滑追向。Editor 可調的 `FacingInterpolationSpeed` 預設為 `10`、最小值為 `0`；設為 `0` 時，只要角色符合一般地面資格，就會在當幀直接套用目標 Yaw。

雙方水平距離不大於 `10 cm` 時，`DesiredFacingYaw` 沿用最後有效方向。若配對後從未取得有效方向，Gameplay 保留目前 Actor Rotation，不從零向量建立任意面向。對手參照失效時也會停止寫入旋轉。

自動面向只在一般地面待機或移動期間更新 Actor Rotation。角色離地、播放 Montage 或使用 Root Motion 時仍會更新有效的目標方向，但暫停寫入 Actor Rotation；恢復資格後從當下角度平滑追向，不突然補寫整段旋轉。

玩家與 CPU 都關閉 `bUseControllerRotationYaw`、`bUseControllerDesiredRotation` 與 `bOrientRotationToMovement`，因此 Controller、Character Movement 與共同面向不會同時競爭 Actor Yaw。

## PIE 換邊展示指令

非 Shipping 組態可在執行中的 PIE Console 使用 `tnda.Debug.FighterOverlap on|off`（也可用 `1|0`）：

- `on` 只把目前 World 中已完成雙向配對的 1P 與 CPU Capsule 對 Pawn 的回應暫時設為 `Overlap`，並保存兩者第一次啟用前的原始值。重複執行 `on` 不會覆蓋最初保存值。
- `off` 會先以兩個直立 Capsule 的實際尺寸檢查是否仍相交；仍重疊時拒絕恢復並要求先分離。完全分離後，兩者會精確恢復保存的 Pawn Response。沒有已保存狀態時重複 `off` 只回報目前已關閉。
- 無參數或參數不是 `on`／`1`／`off`／`0` 時會輸出用法；非 PIE／Game World、玩家無效、沒有 CPU 對手或配對不互為對手時會在 Output Log 說明原因。
- 指令狀態只存在於當前 World；停止 PIE 時由 `WorldCleanup` 清除。它不修改 Blueprint、Class Defaults 或關卡資產，下一次 PIE 會回到原本的 Pawn Block。

Overlap 展示期間，共同推擠解算不再施加被動位移，讓 A／D 能實際穿越 CPU；恢復 Block 後自動回到正常推擠與對頂規則。

## 相對方向分類

`ERelativeDirection` 定義 `Neutral`、`Backward4` 與 `Forward6`。`ClassifyRelativeDirection()` 是不依賴 World 的純計算入口：本次世界移動方向與對手方向都會先移除 Z 並正規化，Dot Product 嚴格大於 `0.1` 時為 `Forward6`，嚴格小於 `-0.1` 時為 `Backward4`，其餘為 `Neutral`。零向量不具方向，也會回傳 `Neutral`。

分類只解讀 Gameplay 已換算完成的世界移動，不會反過來修改 Character Movement。對手距離不大於 `10 cm` 時，分類使用共同基底保存的最後有效對手方向；尚無有效方向時安全回到 `Neutral`。

## 目前限制

共同基底目前提供配對、可觀察資料、純方向分類、自動面向、單一 X 攻擊生命週期與非 Shipping 換邊展示指令。自動化測試涵蓋中點、穩定對手方向及相對方向分類；攻擊、角色移動、面向與攝影機整合見 [Week 05 PIE 驗收](../../../../.scratch/week-05-basic-attack/verification.md)。本週沒有命中判定、扣血、連段、指令緩衝或多招式選擇。
