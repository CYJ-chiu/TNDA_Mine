# Fighting

`Fighting` 實作一名本機玩家對一名 CPU 的格鬥關卡流程。功能依責任分成七個文件模組；各文件記錄
介面、Editor 設定、失敗行為與目前限制。這些模組是文件與職責分組，原始碼仍屬於同一個
`TndaFighter` Unreal Module。

## 模組索引

| 模組 | 主要類別 | 文件 |
|---|---|---|
| 共同角色資料 | `AFightingCharacter` | [Character](Docs/Character.md) |
| 對局與出生 | `AFightingGameMode` | [Match](Docs/Match.md) |
| 玩家 | `AFightingPlayerController`、`AFightingPlayerCharacter` | [Player](Docs/Player.md) |
| 攝影機 | `AFightingCameraActor` | [Camera](Docs/Camera.md) |
| CPU 對手 | `AFightingCpuCharacter`、`AFightingCpuController` | [CPU](Docs/Cpu.md) |
| 動畫資料 | `UFightingAnimInstance` | [Animation](Docs/Animation.md) |
| 戰鬥區域 | `/Game/Fighting/stage1` 場地 Actor | [Arena](Docs/Arena.md) |

## 啟動流程

1. `stage1` 載入既有 `SM_Cylinder` 戰鬥區域與 24 段 WorldStatic 可見場地邊界；邊界是關卡靜態內容，不由執行期生成。
2. `AFightingGameMode` 檢查 `PlayerStart`、`CpuStart` 兩個 Tag 的唯一出生點，由 Unreal 標準流程建立並 Possess 玩家。
3. `AFightingPlayerController` 掛載 Enhanced Input，並停用自動選鏡。
4. `AFightingGameMode` 在標準玩家初始化完成後，於 `CpuStart` 生成 CPU，建立 `AFightingCharacter` 的玩家／CPU 雙向弱參照，再依雙方實際位置設定初始朝向。
5. `AFightingGameMode` 建立相機並傳入同一組角色；相機同步完成初始構圖與 SpringArm 插槽更新後，GameMode 才切換 View Target，直接呈現完整格鬥畫面。
6. 每個角色 Tick 由 `AFightingCharacter` 更新最後有效對手方向，並在一般地面狀態控制 Actor Yaw；Controller、Character Movement 與 AnimBP 不另寫世界面向。
7. 每個 `PrePhysics` 幀，`AFightingPlayerCharacter` 先以共同對手解算雙方地面推擠或對頂；格鬥攝影機在 `PostPhysics` 套用既有最大距離限制。各種角色位移仍受 WorldStatic 牆阻擋。

## 規格對應

- [Week 04 場地邊界規格](../../../.scratch/week-04-arena-boundary/spec.md) 定義 24 段可見牆、角色碰撞，以及新增高牆後的 `CAMCOL_01～04` 攝影機碰撞擴充。
- [Week 05 基本攻擊規格](../../../.scratch/week-05-basic-attack/spec.md) 定義共用 X 指令、單次 Montage、完整淡出鎖定與中斷清理；實測結果見 [驗收紀錄](../../../.scratch/week-05-basic-attack/verification.md)。

## Week 05 操作

在 `stage1` 啟動 PIE，以 WASD、鍵盤方向鍵、搖桿左類比或十字方向鍵移動；按鍵盤 U 或搖桿 X 出招。搖桿 X 對應 UE 的 `Gamepad_FaceButton_Left`，與 U 共用 `IA_Attack1`。每次按下攻擊鍵只嘗試一次攻擊語意 X，長按不連發，攻擊中再次按下直接丟棄。角色必須落地且沒有其他 Montage；轉身途中可以出招，保留當下朝向。

攻擊期間停止主動移動、自動轉向與自訂推擠，直到自然結束或中斷淡出完全結束。方向語意仍持續更新：D 出招後改按 A 會更新 4／6；持續按方向可在解鎖後恢復移動，已鬆鍵則回 Idle。重力、Pawn／牆碰撞與攝影機最大距離修正保持有效。

玩家與 CPU 共用 `TryExecuteCommand("X")`、`CancelAttack()`、`bIsAttacking` 與 `CurrentCommand`。CPU 無 Controller 也能出招，但不會自主選招；既有測試移動模式在攻擊期間暫停、收尾後恢復。

可在 `/Game/Fighting/Anims/AM_Attack_01` 編輯動畫片段與 Blend。維持單一非循環 Section、Auto Blend Out 開啟；修改 Slot 時須對應 `ABP_Manny_Combat` 的 `DefaultSlot`。預設 Blend In／Out 為 0.10／0.15 秒，生命週期由播放實例結束判定，不依賴固定秒數。

## 已知簡化

- Week 05 只有攻擊播放與操作鎖定，尚無 Hitbox、Hurtbox、扣血、連段、CommandBuffer、DataTable 選招或自主攻擊 AI。
- 高牆遮擋目前使用 SpringArm 標準 `Camera` Channel 碰撞；受阻時實際鏡頭會拉近，可能放大角色畫面占比。
- 目前不讓牆體半透明，也不只針對 `ArenaBoundaryTall` 處理；任何阻擋 `Camera` Channel 的場景物件都會縮短實際鏡頭臂長。

## PIE 換邊驗收

非 Shipping PIE 可用 `tnda.Debug.FighterOverlap on`（也可用 `1`）暫時讓已配對的玩家與 CPU 穿越。以 A／D 完成換邊並讓 Capsule 完全分離後，執行 `tnda.Debug.FighterOverlap off`（也可用 `0`）精確還原原始 Pawn Response；若仍重疊，`off` 會拒絕執行並在 Output Log 說明。完整指令與 Combat Debug Draw 圖例見 [Character](Docs/Character.md) 與 [Player](Docs/Player.md)。

原始碼註解與文件同步規則定義在 [Source 原始碼協作規則](../../AGENTS.md) 與本目錄的 [Fighting 原始碼協作規則](AGENTS.md)。
