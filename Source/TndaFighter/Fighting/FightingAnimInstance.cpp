// Copyright © 2026 USERJOY Technology Co., Ltd.All Rights Reserved.

#include "FightingAnimInstance.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "KismetAnimationLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

namespace
{
	constexpr double ShouldMoveSpeedThreshold = 3.0;

	/**
	 * 依既有 AnimBP 規則選擇動畫速度。受阻判斷刻意使用三維實際速度；沒有加速度時
	 * 一律保留實際速度，避免角色放開輸入後仍沿用合成的移動動畫。
	 */
	FVector SelectAnimationVelocity(
		const FVector& ActualVelocity,
		const FVector& Acceleration,
		const double BlockedMovementAnimationSpeed)
	{
		const bool bHasMovementInput = !Acceleration.IsNearlyZero();
		if (bHasMovementInput && ActualVelocity.Size() <= BlockedMovementAnimationSpeed)
		{
			return Acceleration.GetSafeNormal() * BlockedMovementAnimationSpeed;
		}

		return ActualVelocity;
	}

	/** 計算 BlendSpace 使用的水平速度，垂直跳躍或落下速度不得影響地面動畫。 */
	double CalculateGroundSpeed(const FVector& AnimationVelocity)
	{
		return AnimationVelocity.Size2D();
	}

	/**
	 * 計算動畫速度相對角色面向的方向；orient-to-movement 模式限制側向角度，
	 * strafe 模式則保留 Unreal 標準方向的完整範圍。
	 */
	double CalculateMovementDirection(
		const FVector& AnimationVelocity,
		const FRotator& ActorRotation,
		const bool bOrientRotationToMovement)
	{
		const double RawDirection = UKismetAnimationLibrary::CalculateDirection(AnimationVelocity, ActorRotation);
		return bOrientRotationToMovement ? FMath::Clamp(RawDirection, -45.0, 45.0) : RawDirection;
	}
}

void UFightingAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	CacheOwnerReferences();
}

void UFightingAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!IsValid(OwningCharacter) || !IsValid(CharacterMovement) || TryGetPawnOwner() != OwningCharacter)
	{
		CacheOwnerReferences();
	}

	if (!IsValid(OwningCharacter) || !IsValid(CharacterMovement))
	{
		return;
	}

	const FVector ActualVelocity = CharacterMovement->Velocity;
	const FVector Acceleration = CharacterMovement->GetCurrentAcceleration();
	const bool bHasMovementInput = !Acceleration.IsNearlyZero();

	AnimationVelocity = SelectAnimationVelocity(ActualVelocity, Acceleration, BlockedMovementAnimationSpeed);
	GroundSpeed = CalculateGroundSpeed(AnimationVelocity);
	bShouldMove = GroundSpeed > ShouldMoveSpeedThreshold && bHasMovementInput;
	bIsFalling = CharacterMovement->IsFalling();
	MovementDirection = CalculateMovementDirection(
		AnimationVelocity,
		OwningCharacter->GetActorRotation(),
		CharacterMovement->bOrientRotationToMovement);
}

void UFightingAnimInstance::CacheOwnerReferences()
{
	OwningCharacter = Cast<ACharacter>(TryGetPawnOwner());
	CharacterMovement = IsValid(OwningCharacter) ? OwningCharacter->GetCharacterMovement() : nullptr;

	if (!IsValid(OwningCharacter) || !IsValid(CharacterMovement))
	{
		OwningCharacter = nullptr;
		CharacterMovement = nullptr;
		ResetLocomotionData();
	}
}

void UFightingAnimInstance::ResetLocomotionData()
{
	AnimationVelocity = FVector::ZeroVector;
	GroundSpeed = 0.0;
	MovementDirection = 0.0;
	bShouldMove = false;
	bIsFalling = false;
}

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFightingAnimInstanceBlockedVelocityTest,
	"TndaFighter.Fighting.Animation.BlockedVelocityThreshold",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFightingAnimInstanceBlockedVelocityTest::RunTest(const FString& Parameters)
{
	const FVector Acceleration = FVector::YAxisVector * 2.0;

	TestTrue(
		TEXT("低於門檻時使用輸入方向與 300 cm/s"),
		SelectAnimationVelocity(FVector(299.9, 0.0, 0.0), Acceleration, 300.0).Equals(FVector(0.0, 300.0, 0.0)));
	TestTrue(
		TEXT("等於門檻時仍使用輸入方向與 300 cm/s"),
		SelectAnimationVelocity(FVector(300.0, 0.0, 0.0), Acceleration, 300.0).Equals(FVector(0.0, 300.0, 0.0)));
	TestTrue(
		TEXT("高於門檻時保留實際速度"),
		SelectAnimationVelocity(FVector(300.1, 0.0, 0.0), Acceleration, 300.0).Equals(FVector(300.1, 0.0, 0.0)));
	TestTrue(
		TEXT("受阻判斷使用完整三維速度"),
		SelectAnimationVelocity(FVector(0.0, 0.0, 301.0), Acceleration, 300.0).Equals(FVector(0.0, 0.0, 301.0)));
	TestEqual(TEXT("Ground Speed 的產品計算忽略垂直分量"), CalculateGroundSpeed(FVector(0.0, 0.0, 301.0)), 0.0);
	TestEqual(
		TEXT("orient-to-movement 會把超出範圍的方向限制在 -45 度"),
		CalculateMovementDirection(FVector(0.0, -300.0, 0.0), FRotator::ZeroRotator, true),
		-45.0);
	TestEqual(
		TEXT("strafe 模式保留 Unreal 標準方向的完整值"),
		CalculateMovementDirection(FVector(0.0, -300.0, 0.0), FRotator::ZeroRotator, false),
		-90.0);

	return true;
}

#endif
