# Arena

`Arena` 說明 `/Game/Fighting/stage1` 的戰鬥區域與場地邊界。這是關卡資產責任，不包含執行期 C++ 類別。

## 術語與責任

- **戰鬥區域**是關卡中央 `SM_Cylinder` 的綠色圓形頂面，圓心 `(1790,1800)`、半徑 `900 cm`。外層高台與低樓層不屬於合法對戰空間。
- **場地邊界**是沿戰鬥區域外緣放置的可見實體牆。牆段依目前關卡配置分為既有基準牆、高牆與短牆，只負責以 WorldStatic 碰撞阻止 1P 與 CPU 越界。
- 雙方最大距離是攝影機共同構圖規則，不是場地邊界；正常 `stage1` 使用的 `BP_FightingCamera` 設為 `650 cm`，原生 fallback 預設為 `570 cm`。碰牆不會觸發 Ring Out、KO、回合結束或其他勝負結果。

## `stage1` Actor 結構

| 設定 | 值 |
| --- | --- |
| Actor | `ArenaBoundary_Wall_00`～`ArenaBoundary_Wall_23` |
| Outliner Folder | `Gameplay/ArenaBoundary` |
| 共通 Actor Tag | `ArenaBoundary` |
| Static Mesh | `/Game/LevelPrototyping/Meshes/SM_Cube` |
| Material | `/Game/LevelPrototyping/Materials/MI_PrototypeGrid_TopDark` |
| Mobility／Object Type | Static／WorldStatic |
| Collision | QueryAndPhysics／BlockAll |

### 牆體類型

| 類型 | Actor | 類型 Tag | Scale | 垂直範圍 |
| --- | --- | --- | --- | --- |
| 既有基準牆 | `00`～`04`、`09`～`16`、`20`～`23` | 無 | `(2.5,0.3,1.1)` | `Z=200..310` |
| 高牆 | `05`～`08` | `ArenaBoundaryTall` | `(2.5,2.5,6.0)` | 約 `Z=200..803.123` |
| 短牆 | `17`～`19` | `ArenaBoundaryShort` | `(2.5,0.3,0.6)` | `Z=200..260` |

既有基準牆與短牆保留原本 `15°` 槽位及平面尺寸：幾何中心位於半徑 `915 cm`，長邊沿切線，實際佔用範圍為徑向 `900..930 cm`、切向 `-125..125 cm`。基準牆牆底嵌入擂台 `10 cm`，擂台頂面以上可見高度為 `100 cm`；短牆的可見高度為 `50 cm`。高牆已另外加厚、加高並手動調整位置，不套用這組半徑與接縫尺寸。

`SM_Cube` Pivot 位於本地 Bounds 最小角。調整牆段時，不能把 Actor Location 當成幾何中心。既有基準牆與短牆應先算出幾何中心，再扣除旋轉後的本地偏移 `(125,15,0)`；高牆的本地平面偏移為 `(125,125,0)`，並以目前手動配置為準。基準牆段長 `250 cm`，相鄰外緣所需長度約 `244.8736 cm`，每個接縫總重疊約 `5.1264 cm`。

Actor Tag 的命名、組合與變更程序見 [Actor Tags 約定](../../../../.agents/references/actor-tags.md)。

## 執行期行為

- 玩家輸入、CPU 測試移動、地面推擠與目前的跳躍都由既有 Character Movement 掃掠碰撞，牆體不另做 Tick、Clamp 或 Teleport。
- 碰牆後可立即往場內移動；切線與斜向輸入保留沿牆分量。
- `tnda.Debug.FighterOverlap on|off` 只改 Pawn 對 Pawn 回應，牆體的 WorldStatic Block 不受影響。
- 場地牆的 `BlockAll` 也會阻擋 `Camera` Trace Channel；格鬥攝影機的 SpringArm 遇到高牆時縮短實際臂長，避免鏡頭進入牆體。這不改變牆體 Tag、角色碰撞或期望鏡頭距離。
- 若 `PlayerStart`、`CpuStart` 或測試指令把角色直接放到場外，視為關卡製作錯誤；目前不提供自動回場或場外復原。

## Editor 調整與驗證

牆體直接位於 `stage1` 的 `Gameplay/ArenaBoundary` 資料夾。修改後至少確認：

1. 名稱 `00`～`23` 恰有 24 個，Mesh、材質、Folder 與共通 Tag `ArenaBoundary` 一致；`05`～`08` 另有 `ArenaBoundaryTall` Tag，`17`～`19` 另有 `ArenaBoundaryShort` Tag，兩種分類互斥。
2. 牆體類型、Scale 與垂直範圍符合上表；既有基準牆與短牆的 Bounds 內側位於半徑 `900 cm`、厚度向外，高牆維持目前手動配置，所有接縫都沒有可穿越缺口。
3. 每段維持 WorldStatic、QueryAndPhysics、BlockAll。
4. 只保存 `stage1` 與對應 External Actor packages，重新載入關卡後再做 readback。
5. 在新鮮 PIE 回歸徑向／向內／切線／斜向碰牆、CPU 四種模式、雙向推擠、Pawn Overlap、跳躍、最大距離、4／6、換邊與共同攝影機。

Week 04 的完整尺寸、決策與 T01～T12 實測證據見 [規格](../../../../.scratch/week-04-arena-boundary/spec.md)。
