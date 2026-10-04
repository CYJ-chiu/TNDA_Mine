// Copyright © 2026 USERJOY Technology Co., Ltd.All Rights Reserved.

#include "FightingCpuCharacter.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "FightingCpuController.h"
#include "Components/WidgetComponent.h"
#include "Engine/DamageEvents.h"
#include "TimerManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"

AFightingCpuCharacter::AFightingCpuCharacter()
{
	// 測試模式需要在沒有 AI Controller 的 CPU 上消耗 AddMovementInput。
	PrimaryActorTick.bCanEverTick = true;
	GetCharacterMovement()->bRunPhysicsWithNoController = true;

	// Blueprint 子類未覆寫時，日後重新啟用 AI 會使用專案的格鬥 CPU Controller。
	AIControllerClass = AFightingCpuController::StaticClass();

	// 保留 AI Controller 類別供後續階段使用，但目前不自動建立 Controller。
	AutoPossessAI = EAutoPossessAI::Disabled;

	// Actor Yaw 由 AFightingCharacter 對手面向統一控制，不直接複製 Controller 旋轉。
	bUseControllerRotationYaw = false;


	// 對齊玩家角色的膠囊尺寸，維持雙方接觸與出生高度的一致基準。
	GetCapsuleComponent()->InitCapsuleSize(35.0f, 90.0f);

	// AI 恢復後也不能讓 MovementComponent 與共同對手面向競爭 Actor Yaw。
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
}

void AFightingCpuCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateTestMovement();
}

void AFightingCpuCharacter::UpdateTestMovement()
{
	// 攻擊包含完整淡出；解鎖後沿用既有模式，不停止 Tick 或重力。
	if (bIsAttacking || TestMovementMode == EFightingCpuTestMovementMode::Disabled)
	{
		return;
	}

	FVector DirectionToPlayer;
	if (!TryGetOpponentDirection(DirectionToPlayer))
	{
		return;
	}

	const bool bApproachesPlayer = TestMovementMode == EFightingCpuTestMovementMode::ApproachPlayerSlow
		|| TestMovementMode == EFightingCpuTestMovementMode::ApproachPlayerFast;
	const bool bSlowMovement = TestMovementMode == EFightingCpuTestMovementMode::ApproachPlayerSlow
		|| TestMovementMode == EFightingCpuTestMovementMode::RetreatFromPlayerSlow;
	const float MovementScale = bSlowMovement ? 0.25f : 1.0f;
	// 這是 CPU 的主動測試意圖，因此必須使用 CharacterMovement 正常消耗的輸入路徑，讓動畫正確播放移動動作。
	AddMovementInput(bApproachesPlayer ? DirectionToPlayer : -DirectionToPlayer, MovementScale);
}

void AFightingCpuCharacter::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}
