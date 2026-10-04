# Changelog

本檔記錄 TNDAFighter 各週教材版本的主要變更，分類方式採精簡的 Keep a Changelog 結構。

## `week-05`

### 新增

- 加入基本攻擊語意 `X`，名稱沿用搖桿 X 按鈕：玩家在 `stage1` 按鍵盤 U 或搖桿 X 會播放一次 `AM_Attack_01`；長按不連發，攻擊鎖定期間再次按下不會排隊或重啟。
- 建立玩家與 CPU 共用的攻擊生命週期，包含合法開始檢查、播放實例身分追蹤、完整 Blend Out 後才解鎖、取消、中斷、播放者重建與 World 結束清理。
- 攻擊期間停用主動移動、自動面向與自訂推擠，但保留當下 4／6 方向判定、重力、Pawn／場地碰撞與攝影機最大距離修正；Montage 的 Root Motion 僅對當次播放實例停用，角色維持原地攻擊。
- 新增 Boolean `IA_Attack1`，在 `IMC_Combat` 將鍵盤 U 與搖桿 X（`Gamepad_FaceButton_Left`）綁到同一個攻擊動作，並新增單次、非循環的 `AM_Attack_01`；玩家與 CPU Blueprint 共用此 Montage。CPU 可透過共用入口出招，但不新增自主選招 AI。
- 在既有 `IMC_Combat` 加入搖桿十字方向鍵的四向 `IA_Move` mapping，沿用鍵盤四方向的軸向設定，並保留 WASD、鍵盤方向鍵與左類比搖桿操作。

### 已知限制

- Week 05 只涵蓋單次基本攻擊與操作鎖定；尚未實作 Hitbox、Hurtbox、傷害、連段、指令緩衝、多招式選擇或自主攻擊 AI。
- Montage 目前必須維持單一非循環 Section、Auto Blend Out 與 `DefaultGroup.DefaultSlot`；任意 Skeleton 或 Slot 配置的通用相容性驗證留待後續週次。

## `week-04`

### 新增

- 在 `stage1` 的綠色 `SM_Cylinder` 外緣加入 24 段可見 WorldStatic 短牆，明確標示半徑 `900 cm` 的合法戰鬥區域。
- 1P 與 CPU 的主動移動、推擠及目前的跳躍都會被場地邊界阻擋；碰牆後可立即往內移動，切線與斜向輸入仍可沿牆滑動。
- Pawn Overlap 測試模式只影響角色彼此，場地牆持續阻擋雙方。

### 已知限制

- Week 04 只提供不可穿越的可見場地邊界；尚未實作 Ring Out、KO、回合結束、場外出生修正或自動回場。

## `week-03`

### 新增

- 加入地面角色推擠與對頂。角色可推動待機對手、在正面相向時對頂、沿斜向繞行，也支援同向追上、牆邊受阻與推落場地邊緣。
- 加入 CPU 測試移動模式，預設關閉；啟用後可選擇相對 1P 靠近或遠離的慢速、快速模式，方便重現推擠與對頂情境。
- 建立玩家與 CPU 共用的 `AFightingCharacter` 基底，由 GameMode 設定唯一的雙向對手配對，並讓角色移動、攝影機與戰鬥資料共用同一組角色。
- 加入 `Neutral`、`Backward4`、`Forward6` 相對方向判定。方向在輸入事件產生時保存，換邊不會回頭改寫已記錄的語意。
- 加入地面待機與一般移動期間的自動面向。角色換邊後會平順轉向對手，接近重疊時沿用最後有效方向，避免方向與旋轉反覆跳動。
- 加入非 Shipping 的 `tnda.Debug.FighterOverlap on|1|off|0` 與 `tnda.Debug.ShowCombatDebug on|1|off|0` 指令；前者供 PIE 動態換邊，後者切換 Combat Debug Draw，方便查證方向與面向。

### 已知限制

- 專案目前沒有可播放的 Root Motion Montage，因此 Root Motion 期間暫停自動面向的執行期案例尚未覆蓋。
- 現有內容沒有可重現的非 Montage、非 Root Motion 攻擊、受擊或挑釁生命週期，這些特殊動作期間的推擠暫停情境尚未完整覆蓋。

## `week-02`

### 新增

- Play 後直接進入 `stage1`，由 PlayerController 控制 1P，CPU 則由場上的 Spawner 生成並停留在出生位置。
- 加入以格鬥攝影機為基準的 W／A／S／D 移動，以及待機、移動和受阻時對應的 locomotion 動畫。
- 加入 1P 與 CPU 的雙人共同構圖攝影機。攝影機會追蹤雙方中點、維持水平站位與 16:9 可見區域，並依角色距離平順拉近或拉遠。
- 加入穩定的攝影機換邊與近距離處理，避免角色交換左右位置或接近重疊時，鏡頭突然翻轉或跳向另一側。
- 加入雙方最大距離限制，讓兩名角色維持在共同構圖內。

