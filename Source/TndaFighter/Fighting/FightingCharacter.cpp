// Copyright © 2026 USERJOY Technology Co., Ltd.All Rights Reserved.

#include "FightingCharacter.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "FightingCpuCharacter.h"
#include "FightingPlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "TndaFighter.h"

namespace
{
	/** 小於或等於此水平距離時，雙方位置不足以產生穩定的新對手方向。 */
	constexpr float MinimumOpponentDirectionDistanceCm = 10.0f;

	/** 移動方向投影必須嚴格越過此值，才離開相對方向的中立區間。 */
	constexpr double RelativeDirectionDotThreshold = 0.1;

#if !UE_BUILD_SHIPPING
	/** `tnda.Debug.FighterOverlap` 啟用期間需精確還原的當次 PIE 膠囊狀態。 */
	struct FFighterOverlapState
	{
		/** 啟用指令時建立配對的玩家；只用於確認關閉指令仍作用於同一組角色。 */
		TWeakObjectPtr<AFightingPlayerCharacter> Player;

		/** 啟用指令時建立配對的 CPU；只用於確認關閉指令仍作用於同一組角色。 */
		TWeakObjectPtr<AFightingCpuCharacter> Cpu;

		/** 玩家膠囊弱參照，避免 PIE 結束後保存已銷毀元件。 */
		TWeakObjectPtr<UCapsuleComponent> PlayerCapsule;

		/** CPU 膠囊弱參照，避免 PIE 結束後保存已銷毀元件。 */
		TWeakObjectPtr<UCapsuleComponent> CpuCapsule;

		/** 指令開啟前玩家膠囊對 Pawn Channel 的精確回應。 */
		ECollisionResponse PlayerPawnResponse = ECR_Block;

		/** 指令開啟前 CPU 膠囊對 Pawn Channel 的精確回應。 */
		ECollisionResponse CpuPawnResponse = ECR_Block;
	};

	/** 判斷兩個直立膠囊的實際形狀是否仍相交，避免在重疊中恢復 Block。 */
	bool AreCapsulesOverlapping(const UCapsuleComponent& First, const UCapsuleComponent& Second)
	{
		const FVector FirstLocation = First.GetComponentLocation();
		const FVector SecondLocation = Second.GetComponentLocation();
		const FVector2D HorizontalOffset(
			FirstLocation.X - SecondLocation.X,
			FirstLocation.Y - SecondLocation.Y);

		const float FirstSegmentHalfLength = FMath::Max(
			First.GetScaledCapsuleHalfHeight() - First.GetScaledCapsuleRadius(),
			0.0f);
		const float SecondSegmentHalfLength = FMath::Max(
			Second.GetScaledCapsuleHalfHeight() - Second.GetScaledCapsuleRadius(),
			0.0f);
		const float VerticalGap = FMath::Max(
			FMath::Abs(FirstLocation.Z - SecondLocation.Z) - FirstSegmentHalfLength - SecondSegmentHalfLength,
			0.0f);
		const float RadiusSum = First.GetScaledCapsuleRadius() + Second.GetScaledCapsuleRadius();
		return HorizontalOffset.SizeSquared() + FMath::Square(VerticalGap) < FMath::Square(RadiusSum);
	}

	/** 註冊非 Shipping 換邊指令，並以 WorldCleanup 清除每次 PIE 保存的碰撞狀態。 */
	class FFighterOverlapConsoleCommand
	{
	public:
		/** 註冊 Console 指令與 WorldCleanup 回呼。 */
		FFighterOverlapConsoleCommand()
			: Command(
				TEXT("tnda.Debug.FighterOverlap"),
				TEXT("Temporarily allow the paired 1P and CPU to overlap. Usage: tnda.Debug.FighterOverlap on|1|off|0"),
				FConsoleCommandWithWorldAndArgsDelegate::CreateRaw(this, &FFighterOverlapConsoleCommand::Execute))
		{
			WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddRaw(
				this,
				&FFighterOverlapConsoleCommand::HandleWorldCleanup);
		}

		/** 模組卸載時解除全域 WorldCleanup 回呼。 */
		~FFighterOverlapConsoleCommand()
		{
			FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		}

	private:
		/** 取得目前 World 中由 GameMode 建立的有效玩家／CPU 雙向配對。 */
		static bool TryGetFighterPair(
			UWorld* World,
			AFightingPlayerCharacter*& OutPlayer,
			AFightingCpuCharacter*& OutCpu,
			FString& OutFailureReason)
		{
			OutPlayer = nullptr;
			OutCpu = nullptr;
			if (!IsValid(World) || !World->IsGameWorld())
			{
				OutFailureReason = TEXT("the command requires an active PIE or game World");
				return false;
			}

			OutPlayer = Cast<AFightingPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(World, 0));
			if (!IsValid(OutPlayer))
			{
				OutFailureReason = TEXT("player 0 is not a valid AFightingPlayerCharacter");
				return false;
			}

			OutCpu = Cast<AFightingCpuCharacter>(OutPlayer->GetOpponent());
			if (!IsValid(OutCpu))
			{
				OutFailureReason = TEXT("the player has no valid AFightingCpuCharacter opponent");
				return false;
			}

			if (OutCpu->GetOpponent() != OutPlayer)
			{
				OutFailureReason = TEXT("the player and CPU are not a reciprocal pair");
				return false;
			}

			if (!IsValid(OutPlayer->GetCapsuleComponent()) || !IsValid(OutCpu->GetCapsuleComponent()))
			{
				OutFailureReason = TEXT("the paired fighters do not have valid Capsule Components");
				return false;
			}

			return true;
		}

		/** 驗證 `on|1|off|0` 參數與當前配對後，派送至對應的碰撞切換流程。 */
		void Execute(const TArray<FString>& Args, UWorld* World)
		{
			const bool bEnable = Args.Num() == 1
				&& (Args[0].Equals(TEXT("on"), ESearchCase::IgnoreCase) || Args[0] == TEXT("1"));
			const bool bDisable = Args.Num() == 1
				&& (Args[0].Equals(TEXT("off"), ESearchCase::IgnoreCase) || Args[0] == TEXT("0"));
			if (!bEnable && !bDisable)
			{
				UE_LOG(LogTndaFighter, Warning, TEXT("Usage: tnda.Debug.FighterOverlap on|1|off|0"));
				return;
			}

			AFightingPlayerCharacter* Player = nullptr;
			AFightingCpuCharacter* Cpu = nullptr;
			FString FailureReason;
			if (!TryGetFighterPair(World, Player, Cpu, FailureReason))
			{
				UE_LOG(LogTndaFighter, Warning, TEXT("tnda.Debug.FighterOverlap failed: %s."), *FailureReason);
				return;
			}

			if (bEnable)
			{
				EnableOverlap(*World, *Player, *Cpu);
			}
			else
			{
				DisableOverlap(*World, *Player, *Cpu);
			}
		}

		/** 保存本次 World 的原始 Pawn 回應後，允許指定配對互相重疊。 */
		void EnableOverlap(UWorld& World, AFightingPlayerCharacter& Player, AFightingCpuCharacter& Cpu)
		{
			const TWeakObjectPtr<UWorld> WorldKey(&World);
			UCapsuleComponent* PlayerCapsule = Player.GetCapsuleComponent();
			UCapsuleComponent* CpuCapsule = Cpu.GetCapsuleComponent();
			if (FFighterOverlapState* ExistingState = States.Find(WorldKey))
			{
				if (ExistingState->Player == &Player
					&& ExistingState->Cpu == &Cpu
					&& ExistingState->PlayerCapsule.IsValid()
					&& ExistingState->CpuCapsule.IsValid())
				{
					PlayerCapsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
					CpuCapsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
					UE_LOG(LogTndaFighter, Display, TEXT("tnda.Debug.FighterOverlap is already on for %s and %s."),
						*Player.GetName(), *Cpu.GetName());
					return;
				}

				States.Remove(WorldKey);
			}

			FFighterOverlapState& NewState = States.Add(WorldKey);
			NewState.Player = &Player;
			NewState.Cpu = &Cpu;
			NewState.PlayerCapsule = PlayerCapsule;
			NewState.CpuCapsule = CpuCapsule;
			NewState.PlayerPawnResponse = PlayerCapsule->GetCollisionResponseToChannel(ECC_Pawn);
			NewState.CpuPawnResponse = CpuCapsule->GetCollisionResponseToChannel(ECC_Pawn);

			PlayerCapsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
			CpuCapsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
			UE_LOG(LogTndaFighter, Display, TEXT("tnda.Debug.FighterOverlap on: %s and %s now overlap on Pawn; original responses saved."),
				*Player.GetName(), *Cpu.GetName());
		}

		/** 僅在膠囊已分離時還原原始 Pawn 回應，避免切回 Block 造成穿透修正。 */
		void DisableOverlap(UWorld& World, AFightingPlayerCharacter& Player, AFightingCpuCharacter& Cpu)
		{
			const TWeakObjectPtr<UWorld> WorldKey(&World);
			FFighterOverlapState* ExistingState = States.Find(WorldKey);
			if (ExistingState == nullptr)
			{
				UE_LOG(LogTndaFighter, Display, TEXT("tnda.Debug.FighterOverlap is already off; no saved collision state exists."));
				return;
			}

			UCapsuleComponent* PlayerCapsule = ExistingState->PlayerCapsule.Get();
			UCapsuleComponent* CpuCapsule = ExistingState->CpuCapsule.Get();
			if (ExistingState->Player != &Player
				|| ExistingState->Cpu != &Cpu
				|| !IsValid(PlayerCapsule)
				|| !IsValid(CpuCapsule))
			{
				States.Remove(WorldKey);
				UE_LOG(LogTndaFighter, Warning, TEXT("tnda.Debug.FighterOverlap off failed: the saved fighter pair is no longer valid; state was cleared."));
				return;
			}

			if (AreCapsulesOverlapping(*PlayerCapsule, *CpuCapsule))
			{
				UE_LOG(LogTndaFighter, Warning, TEXT("tnda.Debug.FighterOverlap off refused: %s and %s capsules still overlap; separate them first."),
					*Player.GetName(), *Cpu.GetName());
				return;
			}

			PlayerCapsule->SetCollisionResponseToChannel(ECC_Pawn, ExistingState->PlayerPawnResponse);
			CpuCapsule->SetCollisionResponseToChannel(ECC_Pawn, ExistingState->CpuPawnResponse);
			States.Remove(WorldKey);
			UE_LOG(LogTndaFighter, Display, TEXT("tnda.Debug.FighterOverlap off: restored the saved Pawn responses for %s and %s."),
				*Player.GetName(), *Cpu.GetName());
		}

		/** World 銷毀時清除其弱參照與保存值，避免下一次 PIE 沿用舊狀態。 */
		void HandleWorldCleanup(UWorld* World, bool, bool)
		{
			States.Remove(TWeakObjectPtr<UWorld>(World));
		}

		/** `tnda.Debug.FighterOverlap` 的全域註冊物件。 */
		FAutoConsoleCommandWithWorldAndArgs Command;

		/** 解除 WorldCleanup 回呼所需的 Delegate Handle。 */
		FDelegateHandle WorldCleanupHandle;

		/** 依遊戲 World 保存當次指令啟用前的配對與碰撞回應。 */
		TMap<TWeakObjectPtr<UWorld>, FFighterOverlapState> States;
	};

	/** 模組生命週期內唯一的換邊指令實例。 */
	FFighterOverlapConsoleCommand FighterOverlapConsoleCommand;
#endif
}

AFightingCharacter::AFightingCharacter()
{
	// 共同方向資料需隨角色位置更新；衍生類別可覆寫 Tick Group 以配合各自的移動時序。
	PrimaryActorTick.bCanEverTick = true;

	// 世界面向由本類別統一寫入，CharacterMovement 不得再依移動方向旋轉角色。
	GetCharacterMovement()->bOrientRotationToMovement = false;
}

bool AFightingCharacter::SetOpponent(AFightingCharacter* InOpponent)
{
	if (!IsValid(InOpponent) || InOpponent == this)
	{
		return false;
	}

	if (Opponent.IsValid() && Opponent.Get() != InOpponent)
	{
		// 當次對局不支援換目標；呼叫端必須先處理重複角色或重新配對生命週期。
		return false;
	}

	if (Opponent.Get() == InOpponent)
	{
		return true;
	}

	Opponent = InOpponent;
	LastValidOpponentDirection = FVector::ZeroVector;
	bHasValidOpponentDirection = false;
	bHasDesiredFacingYaw = false;
	bHasAppliedFacing = false;
	// 配對當下先取得方向，讓同一幀後續的攝影機、推擠與測試移動可使用一致資料。
	RefreshOpponentDirection();
	UpdateFacing(0.0f);
	return true;
}

bool AFightingCharacter::TryGetCombatMidpoint(FVector& OutCombatMidpoint) const
{
	const AFightingCharacter* CurrentOpponent = Opponent.Get();
	if (!IsValid(CurrentOpponent))
	{
		OutCombatMidpoint = FVector::ZeroVector;
		return false;
	}

	OutCombatMidpoint = CalculateCombatMidpoint(GetActorLocation(), CurrentOpponent->GetActorLocation());
	return true;
}

bool AFightingCharacter::TryGetOpponentDirection(FVector& OutDirection) const
{
	if (!Opponent.IsValid() || !bHasValidOpponentDirection)
	{
		OutDirection = FVector::ZeroVector;
		return false;
	}

	OutDirection = LastValidOpponentDirection;
	return true;
}

bool AFightingCharacter::IsInNormalGroundState() const
{
	const UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	const UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	return !bIsAttacking && IsValid(MovementComponent)
		&& MovementComponent->IsMovingOnGround()
		&& !IsPlayingRootMotion()
		&& (!IsValid(AnimInstance) || !AnimInstance->IsAnyMontagePlaying());
}

FVector AFightingCharacter::CalculateCombatMidpoint(const FVector& FirstLocation, const FVector& SecondLocation)
{
	return (FirstLocation + SecondLocation) * 0.5f;
}

bool AFightingCharacter::UpdateHorizontalOpponentDirection(
	const FVector& CharacterLocation,
	const FVector& OpponentLocation,
	FVector& InOutLastValidDirection)
{
	FVector HorizontalOffset = OpponentLocation - CharacterLocation;
	HorizontalOffset.Z = 0.0f;
	if (HorizontalOffset.SizeSquared() <= FMath::Square(MinimumOpponentDirectionDistanceCm))
	{
		return !InOutLastValidDirection.IsNearlyZero();
	}

	InOutLastValidDirection = HorizontalOffset.GetSafeNormal();
	return true;
}

ERelativeDirection AFightingCharacter::ClassifyRelativeDirection(
	const FVector& WorldMovementDirection,
	const FVector& OpponentDirection)
{
	FVector HorizontalMovement = WorldMovementDirection;
	HorizontalMovement.Z = 0.0f;
	HorizontalMovement = HorizontalMovement.GetSafeNormal();

	FVector HorizontalOpponentDirection = OpponentDirection;
	HorizontalOpponentDirection.Z = 0.0f;
	HorizontalOpponentDirection = HorizontalOpponentDirection.GetSafeNormal();

	if (HorizontalMovement.IsNearlyZero() || HorizontalOpponentDirection.IsNearlyZero())
	{
		return ERelativeDirection::Neutral;
	}

	const double DirectionDot = FVector::DotProduct(HorizontalMovement, HorizontalOpponentDirection);
	if (DirectionDot > RelativeDirectionDotThreshold)
	{
		return ERelativeDirection::Forward6;
	}

	if (DirectionDot < -RelativeDirectionDotThreshold)
	{
		return ERelativeDirection::Backward4;
	}

	return ERelativeDirection::Neutral;
}

void AFightingCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateAttack();
	RefreshOpponentDirection();
	UpdateFacing(DeltaSeconds);
}

bool AFightingCharacter::TryExecuteCommand(FName Command)
{
	if (Command != FName(TEXT("X")) || bIsAttacking || !Opponent.IsValid() || !IsInNormalGroundState())
	{
		return false;
	}

	UAnimInstance* AnimInstance = IsValid(GetMesh()) ? GetMesh()->GetAnimInstance() : nullptr;
	if (!IsValid(AnimInstance) || !IsValid(BasicAttackMontage) || BasicAttackMontage->GetPlayLength() <= 0.0f)
	{
		UE_LOG(LogTndaFighter, Warning, TEXT("Attack rejected: actor=%s command=%s montage=%s reason=invalid animation configuration"),
			*GetName(), *Command.ToString(), *GetNameSafe(BasicAttackMontage));
		return false;
	}
	if (AnimInstance->IsAnyMontagePlaying())
	{
		return false;
	}

	// 已確認沒有任何播放；禁止 Montage_Play 幫忙停止其他播放，失敗也不改動移動或鎖定。
	const float Duration = AnimInstance->Montage_Play(BasicAttackMontage, 1.0f, EMontagePlayReturnType::MontageLength, 0.0f, false);
	FAnimMontageInstance* Instance = AnimInstance->GetActiveInstanceForMontage(BasicAttackMontage);
	if (Duration <= 0.0f || Instance == nullptr)
	{
		if (Instance != nullptr)
		{
			Instance->Stop(FAlphaBlend(0.0f));
		}
		UE_LOG(LogTndaFighter, Warning, TEXT("Attack rejected: actor=%s command=%s montage=%s reason=playback failed"),
			*GetName(), *Command.ToString(), *GetNameSafe(BasicAttackMontage));
		return false;
	}

	AttackAnimInstance = AnimInstance;
	AttackInstanceId = Instance->GetInstanceID();
	CurrentCommand = Command;
	bIsAttacking = true;
	Instance->OnMontageEnded.BindUObject(this, &AFightingCharacter::HandleAttackEnded, AttackAnimInstance, AttackInstanceId);
	// 保留 Sequence 根骨抽取與 AnimBP 模式；僅禁止這次播放把根骨位移累積給 CharacterMovement。
	Instance->PushDisableRootMotion();
	bAttackRootMotionDisabled = true;
	ConsumeMovementInputVector();
	GetCharacterMovement()->StopMovementImmediately();
	ClearAttackMovementIntent();
	UE_LOG(LogTndaFighter, Verbose, TEXT("Attack started: actor=%s command=%s montage=%s owner=%s instance=%d"),
		*GetName(), *CurrentCommand.ToString(), *GetNameSafe(Instance->Montage), *AnimInstance->GetName(), AttackInstanceId);
	return true;
}

void AFightingCharacter::CancelAttack()
{
	UAnimInstance* AnimInstance = AttackAnimInstance.Get();
	FAnimMontageInstance* Instance = AnimInstance ? AnimInstance->GetMontageInstanceForID(AttackInstanceId) : nullptr;
	if (!bIsAttacking || Instance == nullptr || Instance->IsStopped())
	{
		return;
	}
	UE_LOG(LogTndaFighter, Verbose, TEXT("Attack cancel: actor=%s command=%s montage=%s instance=%d"),
		*GetName(), *CurrentCommand.ToString(), *GetNameSafe(Instance->Montage), AttackInstanceId);
	// Stop 只開始淡出，不能在此解除鎖定；已淡出的播放不重新設定剩餘時間。
	Instance->Stop(Instance->Montage->BlendOut);
}

void AFightingCharacter::HandleAttackEnded(UAnimMontage* Montage, bool bInterrupted, TWeakObjectPtr<UAnimInstance> PlaybackOwner, int32 InstanceId)
{
	if (!bIsAttacking || AttackAnimInstance != PlaybackOwner || AttackInstanceId != InstanceId)
	{
		return;
	}
	UE_LOG(LogTndaFighter, Verbose, TEXT("Attack ended: actor=%s command=%s montage=%s instance=%d interrupted=%d"),
		*GetName(), *CurrentCommand.ToString(), *GetNameSafe(Montage), InstanceId, bInterrupted);
	FinishAttack(false);
}

void AFightingCharacter::FinishAttack(bool bStopPlayback)
{
	UAnimInstance* AnimInstance = AttackAnimInstance.Get();
	FAnimMontageInstance* Instance = AnimInstance ? AnimInstance->GetMontageInstanceForID(AttackInstanceId) : nullptr;
	// Stop 可能產生立即或排程的回呼；先清除身分，任何舊事件都不能再改動狀態。
	AttackAnimInstance.Reset();
	AttackInstanceId = INDEX_NONE;
	bIsAttacking = false;
	CurrentCommand = NAME_None;
	if (Instance != nullptr)
	{
		Instance->OnMontageEnded.Unbind();
		if (bAttackRootMotionDisabled)
		{
			Instance->PopDisableRootMotion();
		}
		if (bStopPlayback)
		{
			Instance->Stop(FAlphaBlend(0.0f));
		}
	}
	bAttackRootMotionDisabled = false;
}

void AFightingCharacter::UpdateAttack()
{
	if (!bIsAttacking)
	{
		return;
	}
	UAnimInstance* AnimInstance = AttackAnimInstance.Get();
	if (!IsValid(GetMesh()) || !IsValid(AnimInstance) || GetMesh()->GetAnimInstance() != AnimInstance
		|| AnimInstance->GetMontageInstanceForID(AttackInstanceId) == nullptr)
	{
		UE_LOG(LogTndaFighter, Warning, TEXT("Attack cleanup: actor=%s command=%s montage=%s instance=%d reason=playback owner or instance lost"),
			*GetName(), *CurrentCommand.ToString(), *GetNameSafe(BasicAttackMontage), AttackInstanceId);
		FinishAttack(true);
		return;
	}
	if (!Opponent.IsValid() || !GetCharacterMovement()->IsMovingOnGround())
	{
		CancelAttack();
	}
}

void AFightingCharacter::ClearAttackMovementIntent()
{
	// 共同移動已清除；衍生類別只需移除自身保存的推擠意圖。
}

void AFightingCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	FinishAttack(true);
	Super::EndPlay(EndPlayReason);
}

void AFightingCharacter::RefreshOpponentDirection()
{
	const AFightingCharacter* CurrentOpponent = Opponent.Get();
	if (!IsValid(CurrentOpponent))
	{
		return;
	}

	bHasValidOpponentDirection = UpdateHorizontalOpponentDirection(
		GetActorLocation(),
		CurrentOpponent->GetActorLocation(),
		LastValidOpponentDirection);
	if (bHasValidOpponentDirection)
	{
		DesiredFacingYaw = LastValidOpponentDirection.Rotation().Yaw;
		bHasDesiredFacingYaw = true;
	}
}

void AFightingCharacter::UpdateFacing(float DeltaSeconds)
{
	if (!Opponent.IsValid() || !bHasDesiredFacingYaw || !IsInNormalGroundState())
	{
		return;
	}

	const FRotator CurrentRotation = GetActorRotation();
	const FRotator TargetRotation(CurrentRotation.Pitch, DesiredFacingYaw, CurrentRotation.Roll);
	const bool bShouldSnap = !bHasAppliedFacing || FacingInterpolationSpeed <= 0.0f;
	const FRotator NewRotation = bShouldSnap
		? TargetRotation
		: FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaSeconds, FacingInterpolationSpeed);
	SetActorRotation(NewRotation);
	bHasAppliedFacing = true;
}
