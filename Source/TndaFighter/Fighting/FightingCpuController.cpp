// Copyright © 2026 USERJOY Technology Co., Ltd.All Rights Reserved.

#include "FightingCpuController.h"
#include "Components/StateTreeAIComponent.h"

AFightingCpuController::AFightingCpuController()
{
	// AI 尚未實作完成，Possess 時不可自動啟動 BrainComponent／StateTree 邏輯。
	bStartAILogicOnPossess = false;

	// 保留 Controller 跟隨 Pawn 的空間關係；日後恢復 EQS 時，查詢來源才會位於角色位置。
	bAttachToPawn = true;
}

void AFightingCpuController::OnPossess(APawn* InPawn)
{
	// 目前只執行 AAIController 的標準接管流程；AI 行為資產仍刻意留空。
	Super::OnPossess(InPawn);
}
