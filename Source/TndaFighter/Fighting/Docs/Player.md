# 玩家模組

## 責任

`AFightingPlayerController` 負責本機玩家的 Enhanced Input 與實際格鬥視角查詢；`AFightingPlayerCharacter` 繼承共同的 `AFightingCharacter`，接收移動輸入，並把輸入方向換算到目前畫面座標。

原始碼：`FightingPlayerController.h/.cpp`、`FightingPlayerCharacter.h/.cpp`

## Controller 設定

`DefaultMappingContexts` 內的 Enhanced Input Mapping Context 會以優先權 `0` 加入本機 `LocalPlayer`。遠端 Controller 不會掛載本機輸入。

`AFightingPlayerController` 在建構時停用 `bAutoManageActiveCameraTarget`，避免標準玩家初始化覆寫 GameMode 指定的視角。相機建立及類別設定由 [GameMode](Match.md) 負責；Controller 不保存另一份相機參照，`IsFightingCameraActive()` 與 `GetFightingCameraRotation()` 直接查詢實際 View Target。

## 移動輸入

`AttackAction` 在 Class Defaults 指定 Boolean 攻擊動作，以 `Started` 呼叫共用 `TryExecuteCommand("X")`；未指定時不建立無效綁定。每次按下只嘗試一次，長按不連發、攻擊中按下不預約。`BasicAttackMontage` 由共同基底提供。

`BP_FightingPlayerCharacter.AttackAction` 指向 `/Game/Fighting/Input/Actions/IA_Attack1`。既有 `/Game/Fighting/Input/IMC_Combat` 同時將鍵盤 U 與搖桿 X（`Gamepad_FaceButton_Left`）綁到這個 Boolean Input Action；兩種按鍵都送出攻擊語意 X，沒有 Hold／Pulse 或其他 Trigger。

`IA_Move` 支援 WASD、鍵盤方向鍵、左類比搖桿（`Gamepad_Left2D`）及十字方向鍵。十字方向鍵沿用鍵盤四方向的軸向設定：

| 搖桿按鍵 | UE Key | `IA_Move` 輸出 | Mapping Modifier |
|---|---|---|---|
| 十字上 | `Gamepad_DPad_Up` | `(0, 1)` | `SwizzleAxis`（YXZ） |
| 十字下 | `Gamepad_DPad_Down` | `(0, -1)` | `SwizzleAxis`（YXZ）、`Negate` |
| 十字左 | `Gamepad_DPad_Left` | `(-1, 0)` | `Negate` |
| 十字右 | `Gamepad_DPad_Right` | `(1, 0)` | 無 |

同一個 IMC 共 15 筆 mapping：13 筆移動、2 筆攻擊。原有鍵盤與左類比搖桿的 mapping、modifier 均保留，沒有新增第二個 IMC。

攻擊中 `DoMove()` 仍依當下輸入更新 4／6／Neutral，再阻擋 `AddMovementInput()` 並清除推擠意圖。持續按方向時，解鎖後下一次正常輸入更新恢復移動；已鬆鍵則保持 Idle。`UnPossessed()` 清除待消耗輸入、推擠意圖與方向，並請求原攻擊淡出；玩家沒有 Controller 時不能出招。

`MoveAction` 輸出 `Vector2D`，X 代表左右，Y 代表前後。`AFightingPlayerCharacter::DoMove()` 讓硬體輸入與 Blueprint／UI 共用同一套方向換算。

共用攝影機啟用後，移動方向以實際攝影機的水平旋轉為準；View Target 不是有效格鬥相機時，角色改用 Controller Rotation。兩種路徑都只使用 Yaw，避免攝影機俯角產生垂直移動分量。

目前原生類別綁定 `MoveAction` 與 `AttackAction`，Blueprint EventGraph 為空；未綁定 Look 或跳躍輸入。跳躍構圖驗證使用既有 `ACharacter::Jump()`。

## 4／6 相對方向快照

`DoMove()` 保留原本的攝影機相對世界移動；它只在完成世界方向換算後，將本次移動方向交給共同基底分類。玩家保存 BlueprintReadOnly 的 `CurrentRelativeDirection`，並只在語意改變時透過 `OnRelativeDirectionChanged` 發布新值。換邊不會主動重算或改寫先前通知；仍按住或再次觸發輸入時，才依當下配對位置產生新值。`MoveAction` 的 `Completed` 或 `Canceled` 會把目前值設回 `Neutral`。

| 玩家畫面位置 | A／畫面左 | D／畫面右 |
| --- | --- | --- |
| CPU 左側 | `Backward4` | `Forward6` |
| CPU 右側 | `Forward6` | `Backward4` |

純 W／S 在正常構圖下與對手方向垂直，因此為 `Neutral`。斜向輸入不依按鍵名稱硬編碼，而是沿用世界移動方向對對手方向的 `±0.1` Dot Product 門檻。對手方向尚未有效時也會回到 `Neutral`，不以角色旋轉或固定世界軸猜測。

## 角色面向與動畫責任

玩家停用 `bUseControllerRotationYaw` 與 Character Movement 的移動方向轉身。Actor Yaw 由 `AFightingCharacter` 依固定對手配對統一控制；第一次有效方向立即對正，後續換邊平滑轉向。離地、播放 Montage 或使用 Root Motion 時不覆寫 Actor Rotation，恢復一般地面狀態後再追向。

`ABP_Manny_Combat` 只讀取 Character、Movement Component、Velocity、Ground Speed、Is Falling 與 Direction 等 Gameplay 結果來選擇 Pose。它不寫入 Actor Rotation，也沒有另一套依世界位置判斷左右面向的規則；Mesh 的相對旋轉仍只負責 Manny 資產軸向，不代表 Gameplay 面向。

## Combat Debug Draw

玩家 Class Defaults 的 `Fighting Debug > Show Combat Debug` 預設為關閉。可在 PIE instance 勾選，或在執行中的非 Shipping PIE／遊戲 Console 輸入 `tnda.Debug.ShowCombatDebug on`（也可用 `1`）開啟、輸入 `tnda.Debug.ShowCombatDebug off`（也可用 `0`）關閉，即可用一幀一刷的 Debug Draw 觀察共同資料，不需要 Debug UI 資產。Console 指令直接修改 Player 0 instance 既有的 `bShowCombatDebug`，不改寫 Blueprint Class Defaults；參數不是 `on`／`1`／`off`／`0`、目前不是遊戲 World，或 Player 0 不是 `AFightingPlayerCharacter` 時，會在 Output Log 說明原因。

- 黃線：玩家目前保存的對手參照。
- 青色球：雙方戰鬥中點。
- 洋紅箭頭：目前攝影機的螢幕 X 軸。
- 綠箭頭：玩家最後有效對手方向。
- 藍箭頭：`DesiredFacingYaw`。
- 玩家上方的小球：灰色為 `Neutral`、紅色為 `Backward4`、綠色為 `Forward6`。
- 若目前 viewport 允許 Unreal on-screen messages，左上角也會顯示對手名稱、相對方向與目標 Yaw 數值。

Shipping 組態不執行繪製。這個開關只觀察 Gameplay 結果，不建立自己的方向、配對或旋轉判定。

## 受阻時的移動動畫

`UFightingAnimInstance` 負責 `ABP_Manny_Combat` 的 locomotion 資料。原生類別優先使用角色真實 Velocity；角色仍有移動輸入，但因 CPU 碰撞或格鬥攝影機邊界使真實速度低於 `Blocked Movement Animation Speed` 時，會改用輸入加速度方向與此速度產生動畫用 Velocity。AnimBP 只讀取結果，不再用 EventGraph 重複計算。

`Ground Speed`、`Should Move` 與 `Direction` 都由動畫用 Velocity 計算。`Blocked Movement Animation Speed` 預設為 `300 cm/s`，對應目前 BlendSpace 的 Walk sample，可在 `ABP_Manny_Combat` 的 Class Defaults 調整；玩家頂住 CPU 或攝影機邊界時仍會維持移動動作，放開輸入後會回到 Idle。欄位與生命週期細節見 [動畫資料模組](Animation.md)。

## 地面推擠與對頂

玩家角色在 `PrePhysics` 以一對一互動的單一解算點處理自己與共同對手的接觸，不再搜尋最近 CPU。這讓被推者的原生移動與速度差補償在同一幀完成，避免同向接觸短暫拉開。只有兩者的 `IsInNormalGroundState()` 都成立時才會進入這個規則；任一方離地、播放 Montage 或使用 Root Motion 時，沿用原本移動與碰撞，直到雙方恢復資格。

解算讀取 CPU 的本幀主動加速度，以及 1P 由 `MoveAction` 的 `Triggered` 保存的畫面空間意圖，而不把被動位移寫回移動輸入。`Completed` 或 `Canceled` 會立刻清除 1P 意圖，避免鬆鍵後持續推動。這使待機角色被推動時仍維持 Idle，主動頂住對手的角色則沿用既有受阻 locomotion。角色接觸且只有一方朝對方移動時，解算會先以 Capsule Sweep 取得被推者當幀實際可達的水平位移，再讓推動者以相同 Sweep 跟上；若任一方受阻，便收斂到雙方都能到達的位移，避免單幀推距把兩人拉開而形成隔幀啟停。推動者放開輸入時不再加入被動位移，也不額外產生慣性。

雙方都有朝向彼此的地面意圖時，解算不施加被動位移，保留 Pawn Blocking Collision 形成對頂；因為兩方仍有主動加速度，兩方都持續播放移動動作。斜向接觸只使用使角色接近的水平分量，讓原生 CharacterMovement 保留沿對手側邊繞行的分量。同向追上時，只補足後方沿接觸線速度超過前方的差額，讓前方最終同步而不被額外加速；前方較快時自然拉開距離。

本專案目前只有一名 1P 與一名 CPU。推擠不處理多角色選擇、連鎖推動、空中推擠、特殊動作推擠或依速度決定誰推贏。現有角色與共用 AnimBP 尚無可辨識非 Montage、非 Root Motion 的攻擊／受擊／挑釁狀態；這些動作必須在未來提供自身生命週期訊號後，才能納入資格判定。

## 目前限制

`AFightingPlayerController` 不監聽 Pawn 銷毀，也不會在 Pawn 被移除後自動重生。關卡再戰、回合重置或 KO 後重建角色，日後由 GameMode 或關卡流程另行設計。
