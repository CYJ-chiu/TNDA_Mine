# 對局與出生模組

## 責任

`AFightingGameMode` 是一名本機玩家對一名 CPU 的開場入口。玩家沿用 Unreal 標準生成與 Possess；GameMode 接續生成 CPU、建立雙向對手配對、設定雙方初始朝向，再建立相機並切換視角。

原始碼：`FightingGameMode.h`、`FightingGameMode.cpp`

## 關卡設定

每個戰鬥擂台放置兩個 Player Start Actor，名稱及 `PlayerStartTag` 分別為 `PlayerStart`、`CpuStart`。程式以 Tag 識別，要求每種恰有一個；Actor 顯示名稱和列舉順序不參與選取，PIE 自動注入的 `PlayerStartPIE` 也不計入。

`FindPlayerStart()` 在玩家生成前檢查兩種出生點，忽略 URL 具名入口與 Controller 的 StartSpot 快取，避免繞過固定 Tag 規則。`FinishRestartPlayer()` 先執行父類的 Possess、控制旋轉及玩家預設設定，再以 `CpuStart` 的 Transform 生成 CPU；碰撞策略為 `AdjustIfPossibleButAlwaysSpawn`。生成後建立玩家／CPU 雙向弱對手參照，不接受無效、自身或覆寫既有對手；配對失敗時移除本次 CPU 並停止開場。雙方以生成後的實際位置設定面向彼此的水平朝向，不等待落地。水平位置重合時保留原朝向。

GameMode Blueprint 的 `Class Defaults > Fighting Match > Cpu Class` 必須指定有效、非抽象的 `FightingCpuCharacter` 子類；`BP_FightingGameMode` 使用 `BP_FightingCpuCharacter`。`stage1` 的 CpuStart 保留原 CPU 膠囊中心 `(1800, 1926, 300.0001)`，不是舊 Spawner Actor 的原點高度。

## 相機設定與切換

GameMode Blueprint 的 `Fighting Match > Fighting Camera Class` 可指定相機子類，留空則使用原生 `AFightingCameraActor`。`BP_FightingGameMode` 指定 `BP_FightingCamera`，因此 `stage1` 正常流程會生成 `BP_FightingCamera_C`，並使用該 Blueprint 的構圖預設值。

`FinishRestartPlayer()` 在雙方定位完成後，以 GameMode 為 Owner 建立相機，呼叫 `InitializeForFighters()` 傳入玩家與 CPU。相機完成初始構圖及 SpringArm 插槽更新後，GameMode 才設定 View Target 與 Camera Cut；Controller 停用自動選鏡。構圖公式與參數見 [相機模組](Camera.md)。

## 失敗行為

出生點缺少／重複、玩家型別錯誤或 Possess 後失效、CPU 類別無效或生成失敗、玩家／CPU 配對失敗、Controller 非本機格鬥 Controller、相機類別不可生成或初始化失敗時：

- 以 Error 將關卡名稱與設定說明寫入 Output Log。
- 在 Editor PIE 顯示「對局初始化設定錯誤」MessageBox，指出關卡、GameMode 及需修正的 Tag 或類別設定。
- 設計師按下「確定」後結束該次 PIE；打包版保留 Error Log，不顯示 Editor 對話框。

出生點驗證發生於玩家登入階段時，引擎還會顯示一次「Couldn't spawn player: Could not find a starting spot」；關閉該提示後回到 Editor。

同一個 GameMode 生命週期只回報第一次錯誤，之後不接續生成，避免玩家出生重試反覆彈窗。PIE 在找不到出生點後可能嘗試原點備援；`SpawnDefaultPawnAtTransform()` 也會在設定失敗時拒絕生成，其餘情況沿用父類。

標準玩家 Pawn 生成失敗時，沿用 Unreal 的 `FailedToRestartPlayer` 與生成 Log；此路徑不進入本模組的 `FinishRestartPlayer`，不會建立 CPU 或相機。

## 目前限制

本模組只支援一名本機玩家與一名 CPU，不處理重生、回合重置或多人。雙方及相機只在本次開場建立，世界結束時一併清除。
