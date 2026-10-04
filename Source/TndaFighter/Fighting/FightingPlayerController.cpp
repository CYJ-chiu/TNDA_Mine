// Copyright © 2026 USERJOY Technology Co., Ltd.All Rights Reserved.

#include "FightingPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "FightingCameraActor.h"
#include "Engine/LocalPlayer.h"

AFightingPlayerController::AFightingPlayerController()
{
	// GameMode 在雙方定位後指定相機，避免標準玩家初始化再把 View Target 改回 Pawn。
	bAutoManageActiveCameraTarget = false;
}

void AFightingPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// Enhanced Input Subsystem 隸屬 LocalPlayer；遠端 Controller 沒有本機輸入，不應掛載 IMC。
	if (IsLocalPlayerController())
	{
		// 所有預設 Context 使用相同優先權；衝突規則交由各 Mapping Context 的設定處理。
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}
		}
	}

}

bool AFightingPlayerController::IsFightingCameraActive() const
{
	return IsValid(Cast<AFightingCameraActor>(GetViewTarget()));
}

bool AFightingPlayerController::GetFightingCameraRotation(FRotator& OutRotation) const
{
	// 以實際 View Target 為準，避免保存一份可能與目前視角不同步的相機參照。
	const AFightingCameraActor* FightingCamera = Cast<AFightingCameraActor>(GetViewTarget());
	if (!IsValid(FightingCamera))
	{
		return false;
	}

	OutRotation = FightingCamera->GetViewRotation();
	return true;
}
