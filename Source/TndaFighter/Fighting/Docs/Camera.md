# 攝影機模組

## 責任

`AFightingCameraActor` 提供玩家與 CPU 共用的一對一格鬥視角。它接收 GameMode 已完成雙向配對的玩家與 CPU，負責初始構圖、後續追蹤與玩家最大水平間距。

原始碼：`FightingCameraActor.h`、`FightingCameraActor.cpp`

## 建立與啟用

`AFightingGameMode` 在雙方生成及初始朝向完成後建立一個 `AFightingCameraActor`，Owner 為 GameMode，隨關卡世界結束清除。攝影機類別由 GameMode Blueprint 的 `Fighting Match > FightingCameraClass` Class Default 指定，且只能選擇 `AFightingCameraActor` 的子類。`BP_FightingGameMode` 預設指定 `BP_FightingCamera`，因此 `stage1` 正常流程會生成 `BP_FightingCamera_C`；未指定此欄位時使用原生 C++ 類別。構圖參數可在選用的 Camera Blueprint Class Defaults 調整。

`InitializeForFighters(Player, Cpu)` 只接受已生成、完成雙向配對的有效角色。它以共同角色資料的雙方中點與出生碰撞調整後的實際位置設定焦點、初始高度、旋轉與臂長；首次不插值。直接修改臂長不會更新 SpringArm 插槽，因此初始化會暫時略過 SpringArm Lag，同步更新一次插槽及 CameraComponent，再由 GameMode 指定 View Target 與 Camera Cut。

## 雙人構圖

攝影機在 `PostPhysics` Tick，SpringArm 以 Actor Tick 為前置條件，使用同幀更新後的構圖。每幀依角色完成移動與物理更新後的位置執行下列工作：

- 精確追蹤玩家與 CPU 的世界座標中點。
- 讓鏡頭 Yaw 與雙方水平連線保持垂直，使兩名角色在畫面上的站位線維持水平。
- 把角色連線視為沒有固定方向的軸；雙方交換左右時只交換畫面站位，不讓鏡頭翻轉 180 度。
- 依雙方完整水平 2D 距離平滑調整 SpringArm 臂長。

FOV 固定為 `55°`。SpringArm Collision Test 使用 `Camera` Trace Channel；期望鏡頭路徑被高牆或其他阻擋物截斷時，引擎會縮短實際臂長，把鏡頭留在第一個碰撞面前。`TargetArmLength` 仍由雙方距離規則決定，路徑恢復暢通後會回到該期望距離。

這是高牆遮擋的第一版處理。靠牆時角色在畫面中的占比可能因鏡頭拉近而變大；目前不做牆體半透明、Dither Fade 或高牆 Tag 專用 Trace，所有阻擋 `Camera` Channel 的場景物件都遵循同一套 SpringArm 行為。

攝影機將可見遊戲區固定為 16:9。執行視窗使用其他比例時，Unreal 會以黑邊保留構圖，不拉伸畫面，也不改變作業系統視窗的像素尺寸。此設定只隨 `AFightingCameraActor` 生效，目前套用於 `stage1` 的格鬥視角。

## 可調參數

參數位於 `FightingCameraActor.h` 的 `Fighting Camera` 類別屬性。

| 屬性 | 原生預設 | `BP_FightingCamera` | 用途 |
|---|---:|---:|---|
| `FocusHeight` | `45 cm` | 沿用 | 將焦點抬高到角色原點上方。 |
| `FixedCameraRotation` | Pitch `-10°`、Yaw `0°`、Roll `0°` | 沿用 | Pitch 與 Roll 固定使用；Yaw 只決定初始取景側。 |
| `MinCameraDistance` | `440 cm` | `500 cm` | 雙方接近時的 SpringArm 臂長。 |
| `MaxCameraDistance` | `690 cm` | `760 cm` | 雙方到達最大構圖間距時的臂長。 |
| `MinFighterDistance` | `110 cm` | 沿用 | 開始將鏡頭由最近距離拉遠的角色間距。 |
| `MaxFighterDistance` | `570 cm` | `650 cm` | 到達最遠鏡頭距離的角色間距，也是 1P 的水平移動邊界。 |
| `DistanceInterpolationSpeed` | `5` | 沿用 | SpringArm 臂長的插值速度。 |
| `MaxVerticalFocusOffset` | `250 cm` | 沿用 | 焦點相對初始高度可上下追蹤的最大距離。 |

距離端點以示範圖的最近與最遠角色畫面占比為基準，並以 `stage1` 的實際 Skeletal Mesh Bounds 投影校正。正常 `stage1` 使用的 `BP_FightingCamera` 會把兩名角色的水平距離 `110–650 cm` 線性對應到 `500–760 cm` 的鏡頭距離；未指定 Blueprint 類別時，原生 fallback 使用 `110–570 cm` 對應 `440–690 cm`。

## PIE 即時調校回寫

PIE 中可在 World Outliner 選取即時生成的 `BP_FightingCamera_C`，修改上述八個構圖參數後，執行 `Runtime Tuning > Apply Runtime Values To BP`。操作只把白名單內的 Actor 參數寫回產生該實例的 exact Blueprint Class Defaults；Actor Transform、元件狀態、SpringArm 當前臂長與對局 transient 狀態不會被複製。

成功時會直接且同步保存 Blueprint asset，不顯示 Save 或 source-control checkout 對話框，也不在操作中編譯 Blueprint，以免目前執行中的 PIE Actor 被 reinstancing。保存失敗時，異動仍保留在 Editor 記憶體並維持 package dirty，詳細原因寫入 Output Log；原生 C++ fallback、非 PIE 實例、無有效 Blueprint／CDO、無法解析 package 或檔案不可寫時都不會猜測其他保存目標。

由於保存會把該 Blueprint package 當下所有未保存異動一起寫入磁碟，執行前應確認同一資產內沒有不打算保留的其他修改。需要 Compile 時，請在結束 PIE 後另行執行。

## 玩家移動邊界

攝影機在取景前將 1P 限制於 CPU 周圍 `MaxFighterDistance` 的水平圓形範圍。玩家超界時，攝影機會校正水平位置並移除遠離 CPU 的速度分量；向 CPU 靠近、沿邊界側移與垂直跳躍不受影響。

玩家角色的地面推擠在 `PrePhysics` 階段先完成；攝影機 Actor 位於 `PostPhysics`，因此最大距離校正與雙人構圖會讀取推擠後的位置，避免同一幀使用舊位置造成跳回或抖動。SpringArm 另以攝影機 Actor Tick 為 prerequisite，確保元件插槽使用同幀構圖結果。

`AFightingPlayerCharacter` 會向 PlayerController 查詢實際攝影機旋轉，作為相對畫面的移動方向。輸入與動畫行為記錄於 [玩家模組](Player.md)。

## 生命週期與首幀

GameMode 在雙方定位後同步完成相機初始化，Controller 停用自動管理 View Target。沒有搜尋、輪詢、World Spawn 委派、啟動黑幕、Widget 隱藏／恢復或延後解除遮罩；角色 UI 保持原本的可見設定。

相機保存雙方弱參照，任何一方失效時停止更新，不搜尋替代對手，也不處理角色生成、重生或回合重置。
