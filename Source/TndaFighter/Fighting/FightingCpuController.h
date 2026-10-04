// Copyright © 2026 USERJOY Technology Co., Ltd.All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "FightingCpuController.generated.h"

class UStateTreeAIComponent;

/**
 * CPU 對手使用的 AI Controller 基底。
 *
 * 類別保留 Pawn 附著與日後接回 StateTree 的執行條件，但目前不會在 Possess 時自動啟動
 * AI 邏輯；`AFightingCpuCharacter` 也預設停用 Auto Possess AI。
 */
UCLASS(abstract)
class AFightingCpuController : public AAIController
{
	GENERATED_BODY()

public:

	/** 設定 AI 啟動時機與 Controller 是否跟隨 Pawn。 */
	AFightingCpuController();

protected:

	/** 接管 Pawn 時保留父類初始化；目前不啟動 StateTree 或其他 AI 邏輯。 */
	virtual void OnPossess(APawn* InPawn) override;
};
