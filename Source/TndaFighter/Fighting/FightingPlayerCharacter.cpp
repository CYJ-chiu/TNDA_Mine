// Copyright © 2026 USERJOY Technology Co., Ltd.All Rights Reserved.

#include "FightingPlayerCharacter.h"
#include "TndaFighterActorTags.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "FightingPlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "TndaFighter.h"

namespace
{
	/** Capsule 接觸容差，涵蓋 CharacterMovement 停在 Blocking Hit 前留下的微小間距。 */
	constexpr float PushContactToleranceCm = 2.0f;

	/** 方向投影超過此值才視為角色確實想朝對手移動，避免幾乎平行的側移誤觸推擠。 */
	constexpr float PushTowardIntentThreshold = 0.01f;

	/** 取得 CharacterMovement 本幀的地面主動意圖；零向量代表沒有主動移動輸入。 */
	FVector GetGroundMovementIntent(const ACharacter& Character)
	{
		const UCharacterMovementComponent* MovementComponent = Character.GetCharacterMovement();
		if (!IsValid(MovementComponent) || !MovementComponent->IsMovingOnGround())
		{
			return FVector::ZeroVector;
		}

		FVector CurrentAcceleration = MovementComponent->GetCurrentAcceleration();
		CurrentAcceleration.Z = 0.0f;
		return CurrentAcceleration.GetSafeNormal();
	}

	/** 以最大加速度正規化本幀輸入大小，讓類比輸入能維持其原本的地面移動比例。 */
	float GetGroundMovementInputScale(const ACharacter& Character)
	{
		const UCharacterMovementComponent* MovementComponent = Character.GetCharacterMovement();
		if (!IsValid(MovementComponent) || !MovementComponent->IsMovingOnGround())
		{
			return 0.0f;
		}

		FVector CurrentAcceleration = MovementComponent->GetCurrentAcceleration();
		CurrentAcceleration.Z = 0.0f;
		const float MaxAcceleration = MovementComponent->GetMaxAcceleration();
		return MaxAcceleration > KINDA_SMALL_NUMBER
			? FMath::Clamp(CurrentAcceleration.Size() / MaxAcceleration, 0.0f, 1.0f)
			: 0.0f;
	}

#if !UE_BUILD_SHIPPING
	/**
	 * 將 Console 的 on／1／off／0 參數寫入目前 Player 0 角色既有的 `bShowCombatDebug`。
	 * 指令只改執行中 instance，不改 Blueprint Class Defaults；沒有有效遊戲 World 或玩家時不保留待套用狀態。
	 */
	void SetShowCombatDebug(const TArray<FString>& Args, UWorld* World)
	{
		const bool bEnable = Args.Num() == 1
			&& (Args[0].Equals(TEXT("on"), ESearchCase::IgnoreCase) || Args[0] == TEXT("1"));
		const bool bDisable = Args.Num() == 1
			&& (Args[0].Equals(TEXT("off"), ESearchCase::IgnoreCase) || Args[0] == TEXT("0"));
		if (!bEnable && !bDisable)
		{
			UE_LOG(LogTndaFighter, Warning, TEXT("Usage: tnda.Debug.ShowCombatDebug on|1|off|0"));
			return;
		}

		if (!IsValid(World) || !World->IsGameWorld())
		{
			UE_LOG(LogTndaFighter, Warning, TEXT("tnda.Debug.ShowCombatDebug failed: the command requires an active PIE or game World."));
			return;
		}

		AFightingPlayerCharacter* Player = Cast<AFightingPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(World, 0));
		if (!IsValid(Player))
		{
			UE_LOG(LogTndaFighter, Warning, TEXT("tnda.Debug.ShowCombatDebug failed: player 0 is not a valid AFightingPlayerCharacter."));
			return;
		}

		Player->bShowCombatDebug = bEnable;
		UE_LOG(LogTndaFighter, Display, TEXT("tnda.Debug.ShowCombatDebug %s: Combat Debug Draw is now %s for %s."),
			*Args[0], Player->bShowCombatDebug ? TEXT("on") : TEXT("off"), *Player->GetName());
	}

	/** 非 Shipping 組態中的 Combat Debug Draw 執行期開關。 */
	FAutoConsoleCommandWithWorldAndArgs ShowCombatDebugConsoleCommand(
		TEXT("tnda.Debug.ShowCombatDebug"),
		TEXT("Toggle AFightingPlayerCharacter Combat Debug Draw. Usage: tnda.Debug.ShowCombatDebug on|1|off|0"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SetShowCombatDebug));
#endif
}

AFightingPlayerCharacter::AFightingPlayerCharacter()
{
	// 推擠需在角色本幀原生移動前補足相對速度，讓同向接觸的兩者在同一幀同步；攝影機在 PostPhysics 讀取本幀結果。
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	// Actor Yaw 由 AFightingCharacter 對手面向統一控制，不直接複製 Controller 旋轉。
	bUseControllerRotationYaw = false;
	// 與目前 Manny 角色尺寸一致，讓碰撞中心和角色腳底位置保持既有配置。
	GetCapsuleComponent()->InitCapsuleSize(35.0f, 90.0f);

	// 原生預設步行速度；Blueprint 子類仍可在 Class Defaults 覆寫。
	GetCharacterMovement()->MaxWalkSpeed = 400.0f;

	// 供使用 Actor Tag 查詢的執行期系統辨識這名玩家角色。
	Tags.Add(TndaFighterActorTags::Player);
}

void AFightingPlayerCharacter::Move(const FInputActionValue& Value)
{
	// MoveAction 約定輸出 Vector2D：X 是左右，Y 是前後。
	FVector2D MovementVector = Value.Get<FVector2D>();

	// 集中走 DoMove，讓 Enhanced Input 與 Blueprint／UI 共用完全相同的方向換算。
	DoMove(MovementVector.X, MovementVector.Y);
}

void AFightingPlayerCharacter::StopMove(const FInputActionValue&)
{
	GroundPushIntent = FVector::ZeroVector;
	GroundPushInputScale = 0.0f;
	SetCurrentRelativeDirection(ERelativeDirection::Neutral);
}

void AFightingPlayerCharacter::DoMove(float Right, float Forward)
{
	if (GetController() != nullptr)
	{
		// View Target 不是有效格鬥攝影機時，沿用 Controller 旋轉作為移動基準。
		FRotator Rotation = GetController()->GetControlRotation();
		if (const AFightingPlayerController* PlayerController = Cast<AFightingPlayerController>(GetController()))
		{
			// 共用攝影機啟用後，以實際畫面方向作為相對攝影機移動的唯一基準。
			PlayerController->GetFightingCameraRotation(Rotation);
		}
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// 只取 Yaw，避免攝影機俯角讓水平移動產生垂直分量。
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// 右方向和前方向使用同一份 Yaw 基準，確保搖桿兩軸在畫面空間互相垂直。
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// 推擠解算與 CharacterMovement 同處 PrePhysics，仍保存畫面空間意圖以避免 Tick 順序消耗輸入後失去 1P 意圖。
		FVector MovementIntent = ForwardDirection * Forward + RightDirection * Right;
		MovementIntent.Z = 0.0f;
		GroundPushInputScale = FMath::Clamp(MovementIntent.Size(), 0.0f, 1.0f);
		GroundPushIntent = MovementIntent.GetSafeNormal();

		FVector OpponentDirection;
		const ERelativeDirection NewRelativeDirection = TryGetOpponentDirection(OpponentDirection)
			? ClassifyRelativeDirection(MovementIntent, OpponentDirection)
			: ERelativeDirection::Neutral;
		SetCurrentRelativeDirection(NewRelativeDirection);
		// 方向語意已更新；只阻擋實際移動，按住方向可在解鎖後由下一次 Triggered 自然恢復。
		if (bIsAttacking)
		{
			ClearAttackMovementIntent();
			return;
		}
		// CharacterMovement 會合成兩軸輸入並限制最大加速度，不需在這裡自行正規化。
		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
	else
	{
		SetCurrentRelativeDirection(ERelativeDirection::Neutral);
	}
}

void AFightingPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	GroundPushIntent = FVector::ZeroVector;
	GroundPushInputScale = 0.0f;
}

bool AFightingPlayerCharacter::TryExecuteCommand(FName Command)
{
	return IsValid(GetController()) && Super::TryExecuteCommand(Command);
}

void AFightingPlayerCharacter::Attack(const FInputActionValue&)
{
	TryExecuteCommand(TEXT("X"));
}

void AFightingPlayerCharacter::ClearAttackMovementIntent()
{
	GroundPushIntent = FVector::ZeroVector;
	GroundPushInputScale = 0.0f;
}

void AFightingPlayerCharacter::UnPossessed()
{
	ConsumeMovementInputVector();
	ClearAttackMovementIntent();
	SetCurrentRelativeDirection(ERelativeDirection::Neutral);
	CancelAttack();
	Super::UnPossessed();
}

void AFightingPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ResolveGroundPushInteraction(DeltaSeconds);
	DrawCombatDebug();
}

void AFightingPlayerCharacter::ResolveGroundPushInteraction(float DeltaSeconds)
{
	// 1P 是一對一推擠的唯一協調者：此函式在 1P 的 PrePhysics Tick 執行，CPU 不會再做第二次對稱解算，
	// PostPhysics 的格鬥攝影機因此能直接讀取本幀雙方最終位置。非普通地面狀態仍交由原生
	// CharacterMovement、Montage 或 Root Motion 處理，Pawn 碰撞未維持 Block 時也不介入測試用 Overlap 流程。
	AFightingCharacter* CurrentOpponent = GetOpponent();
	if (!IsValid(CurrentOpponent)
		|| DeltaSeconds <= 0.0f
		|| !IsInNormalGroundState()
		|| !CurrentOpponent->IsInNormalGroundState()
		|| GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block
		|| CurrentOpponent->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block)
	{
		return;
	}

	// 推擠只解算地面 XY 平面；Capsule 半徑總和是實際接觸距離，微小容差只吸收碰撞解算後的數值間隙，
	// 不用來補償單幀推距或允許遠距推動。距離近乎零時方向無法穩定正規化，直接保留既有碰撞結果。
	FVector PlayerToCpu = CurrentOpponent->GetActorLocation() - GetActorLocation();
	PlayerToCpu.Z = 0.0f;
	const float PlayerToCpuDistance = PlayerToCpu.Size();
	const float ContactDistance = GetCapsuleComponent()->GetScaledCapsuleRadius()
		+ CurrentOpponent->GetCapsuleComponent()->GetScaledCapsuleRadius()
		+ PushContactToleranceCm;
	if (PlayerToCpuDistance > ContactDistance || PlayerToCpuDistance <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	// 1P 意圖來自 Enhanced Input 保存的世界方向；CPU 意圖取自 CharacterMovement 的本幀主動加速度。
	// 將兩者投影到 Capsule 中心連線後，正值只代表「朝對手靠近」的分量；切線輸入仍由原生移動保留。
	const FVector PlayerToCpuDirection = PlayerToCpu / PlayerToCpuDistance;
	const FVector PlayerIntent = GroundPushIntent;
	const FVector CpuIntent = GetGroundMovementIntent(*CurrentOpponent);
	const float PlayerTowardCpu = FVector::DotProduct(PlayerIntent, PlayerToCpuDirection);
	const float CpuTowardPlayer = FVector::DotProduct(CpuIntent, -PlayerToCpuDirection);

	// 雙方都有相向意圖時不比較速度或決定勝負，維持原生 Blocking Collision 的對頂結果；
	// 兩人的主動加速度不會被清除，受阻 locomotion 因此仍能維持各自的移動動畫。
	if (PlayerTowardCpu > PushTowardIntentThreshold && CpuTowardPlayer > PushTowardIntentThreshold)
	{
		return;
	}

	// 只有一方朝對手靠近時才建立推動者／被推者。若被推者正沿相同方向主動移動，只補足推動者
	// 超過其前進速度的差額；前方角色速度相同或更快時 PushDistance 為零，雙方會自然分離而不被牽引。
	ACharacter* PushingCharacter = nullptr;
	ACharacter* PushedCharacter = nullptr;
	FVector PushDirection = FVector::ZeroVector;
	float PushDistance = 0.0f;
	if (PlayerTowardCpu > PushTowardIntentThreshold)
	{
		const UCharacterMovementComponent* PlayerMovement = GetCharacterMovement();
		const UCharacterMovementComponent* CpuMovement = CurrentOpponent->GetCharacterMovement();
		PushingCharacter = this;
		PushedCharacter = CurrentOpponent;
		PushDirection = PlayerToCpuDirection;
		const float PlayerPushSpeed = PlayerMovement->GetMaxSpeed() * GroundPushInputScale * PlayerTowardCpu;
		const float CpuForwardSpeed = CpuMovement->GetMaxSpeed()
			* GetGroundMovementInputScale(*CurrentOpponent)
			* FMath::Max(FVector::DotProduct(CpuIntent, PushDirection), 0.0f);
		PushDistance = FMath::Max(PlayerPushSpeed - CpuForwardSpeed, 0.0f) * DeltaSeconds;
	}
	else if (CpuTowardPlayer > PushTowardIntentThreshold)
	{
		const UCharacterMovementComponent* CpuMovement = CurrentOpponent->GetCharacterMovement();
		const UCharacterMovementComponent* PlayerMovement = GetCharacterMovement();
		PushingCharacter = CurrentOpponent;
		PushedCharacter = this;
		PushDirection = -PlayerToCpuDirection;
		const float CpuPushSpeed = CpuMovement->GetMaxSpeed() * GetGroundMovementInputScale(*CurrentOpponent) * CpuTowardPlayer;
		const float PlayerForwardSpeed = PlayerMovement->GetMaxSpeed()
			* GroundPushInputScale
			* FMath::Max(FVector::DotProduct(PlayerIntent, PushDirection), 0.0f);
		PushDistance = FMath::Max(CpuPushSpeed - PlayerForwardSpeed, 0.0f) * DeltaSeconds;
	}

	// 沒有單方靠近意圖，或同向速度差不足以追上時，不新增任何被動位移與慣性。
	if (!IsValid(PushingCharacter) || !IsValid(PushedCharacter) || PushDistance <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector PushOffset = PushDirection * PushDistance;
	// 每個水平軸都以三段式協作取得「雙方共同可達位移」：
	// 1. 被推者先用 Capsule Sweep 探出實際可走距離，牆面等 WorldStatic 仍是權威限制。
	// 2. 推動者只嘗試跟進被推者真正完成的位移，避免單幀推距大於接觸容差時把兩人拆開。
	// 3. 推動者若又受其他場景碰撞限制，立即 Sweep 收回被推者未能共同完成的差額。
	// 位移量使用 Sweep 前後 Transform 的差值，而非假設 RequestedOffset 全數成功，連同碰撞截短結果一起納入。
	// MovePairAlongAxis 是只活在本函式裡的區域小函式（lambda），下方會分別拿 X、Y 位移呼叫它。
	// 中括號把本次選出的推動者與被推者指標保存進 lambda；RequestedOffset 則是這次「希望移動多少」的世界座標差值，
	// 例如 (10, 0, 0) 代表沿世界 X 正方向移動 10 Unreal Units，而不是把角色直接放到 X = 10 的位置。
	const auto MovePairAlongAxis = [PushingCharacter, PushedCharacter](const FVector& RequestedOffset)
	{
		// 浮點運算可能留下極小誤差，所以不直接與 FVector::ZeroVector 比較；近乎零就沒有必要執行碰撞查詢。
		if (RequestedOffset.IsNearlyZero())
		{
			return;
		}

		// 先記住被推者移動前的世界位置。Sweep 遇牆時可能只完成部分位移，因此稍後必須用「移動後位置 - 起點」
		// 量出真正完成的距離，不能直接把 RequestedOffset 當成結果。
		const FVector PushedStart = PushedCharacter->GetActorLocation();
		FHitResult PushedHit;
		// AddActorWorldOffset 是「在目前世界位置上再加一段位移」，不是設定絕對座標。四個參數依序是：
		// 1. RequestedOffset：想增加的世界座標位移。
		// 2. true：開啟 Sweep。Character 的 Root Component 是 Capsule；引擎會讓 Capsule 沿路檢查阻擋，
		//    有足夠空間就走完整段，途中碰牆就只走到不穿牆的位置，也可能完全走不動。
		// 3. &PushedHit：讓引擎把碰到什麼、碰撞點與法線等結果寫入 PushedHit。本演算法以實際前後位置計算距離，
		//    不依賴 Hit.Time，仍保留 HitResult 來接收 Sweep API 的輸出並方便除錯觀察。
		// 4. ETeleportType::None：採用一般位移流程，不把這次移動視為物理瞬移；是否沿路檢查碰撞仍由第二個參數 Sweep 決定。
		PushedCharacter->AddActorWorldOffset(RequestedOffset, true, &PushedHit, ETeleportType::None);

		// AvailableOffset 是被推者實際走完的距離，可能等於 RequestedOffset、被牆截短，或為零。
		// 清掉 Z 是為了讓推動者只跟隨地面 XY 位移，不把碰撞修正可能產生的垂直微量誤差傳給另一人。
		FVector AvailableOffset = PushedCharacter->GetActorLocation() - PushedStart;
		AvailableOffset.Z = 0.0f;
		// 被推者完全走不動時，推動者也不能繼續向前，否則兩個 Capsule 會互相擠壓並產生顫抖。
		if (AvailableOffset.IsNearlyZero())
		{
			return;
		}

		// 接著讓推動者嘗試跟上「被推者實際完成的 AvailableOffset」，而不是原本要求的 RequestedOffset。
		// 推動者同樣使用 Sweep，所以自己若被牆或其他 Blocking 物件擋住，也只會完成安全可達的部分。
		const FVector PusherStart = PushingCharacter->GetActorLocation();
		FHitResult PusherHit;
		PushingCharacter->AddActorWorldOffset(AvailableOffset, true, &PusherHit, ETeleportType::None);

		// FollowedOffset 是推動者實際跟上的距離；UnmatchedOffset 是被推者已走、推動者卻沒能跟上的差額。
		// 例：被推者走了 7、推動者受阻只走 5，差額就是 2。若不處理，兩人會平白多出 2 Units 的間隙。
		FVector FollowedOffset = PushingCharacter->GetActorLocation() - PusherStart;
		FollowedOffset.Z = 0.0f;
		const FVector UnmatchedOffset = AvailableOffset - FollowedOffset;
		// 將被推者往反方向移動差額（上例為 -2），讓雙方最後共同完成約 5 Units，維持原本的接觸距離。
		// 回收也維持 Sweep；若反方向另有阻擋，引擎仍以不穿牆為優先，不用 Teleport 強行還原而造成 Capsule 重疊。
		if (!UnmatchedOffset.IsNearlyZero())
		{
			FHitResult RollbackHit;
			PushedCharacter->AddActorWorldOffset(-UnmatchedOffset, true, &RollbackHit, ETeleportType::None);
		}
	};

	// X、Y 分軸依固定順序各自求共同可達距離，避免一個受阻分量取消整段斜向位移；
	// 未受阻的水平分量仍可繼續，保留目前場地牆面的沿牆與繞行行為。
	const FVector XOffset(PushOffset.X, 0.0f, 0.0f);
	const FVector YOffset(0.0f, PushOffset.Y, 0.0f);
	MovePairAlongAxis(XOffset);
	MovePairAlongAxis(YOffset);
}

void AFightingPlayerCharacter::SetCurrentRelativeDirection(ERelativeDirection NewDirection)
{
	if (CurrentRelativeDirection == NewDirection)
	{
		return;
	}

	CurrentRelativeDirection = NewDirection;
	OnRelativeDirectionChanged.Broadcast(CurrentRelativeDirection);
}

void AFightingPlayerCharacter::DrawCombatDebug() const
{
#if !UE_BUILD_SHIPPING
	if (!bShowCombatDebug || !IsValid(GetWorld()))
	{
		return;
	}

	const AFightingCharacter* CurrentOpponent = GetOpponent();
	const FVector TextLocation = GetActorLocation() + FVector(0.0f, 0.0f, 150.0f);
	if (!IsValid(CurrentOpponent))
	{
		DrawDebugString(GetWorld(), TextLocation, TEXT("Opponent: None"), nullptr, FColor::Red, 0.0f, false);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(static_cast<int32>(GetUniqueID()), 0.1f, FColor::Red, TEXT("Combat Debug | Opponent: None"));
		}
		return;
	}

	constexpr float DrawHeightCm = 100.0f;
	constexpr float DirectionLengthCm = 150.0f;
	const FVector HeightOffset(0.0f, 0.0f, DrawHeightCm);
	const FVector PlayerLocation = GetActorLocation() + HeightOffset;
	const FVector OpponentLocation = CurrentOpponent->GetActorLocation() + HeightOffset;
	DrawDebugLine(GetWorld(), PlayerLocation, OpponentLocation, FColor::Yellow, false, 0.0f, 0, 2.0f);

	FColor RelativeDirectionColor = FColor::Silver;
	if (CurrentRelativeDirection == ERelativeDirection::Backward4)
	{
		RelativeDirectionColor = FColor::Red;
	}
	else if (CurrentRelativeDirection == ERelativeDirection::Forward6)
	{
		RelativeDirectionColor = FColor::Green;
	}
	DrawDebugSphere(
		GetWorld(),
		PlayerLocation + FVector(0.0f, 0.0f, 40.0f),
		10.0f,
		12,
		RelativeDirectionColor,
		false,
		0.0f,
		0,
		2.0f);

	FVector CombatMidpoint;
	if (TryGetCombatMidpoint(CombatMidpoint))
	{
		CombatMidpoint += HeightOffset;
		DrawDebugSphere(GetWorld(), CombatMidpoint, 12.0f, 12, FColor::Cyan, false, 0.0f, 0, 2.0f);

		FRotator CameraRotation = GetController() ? GetController()->GetControlRotation() : FRotator::ZeroRotator;
		if (const AFightingPlayerController* PlayerController = Cast<AFightingPlayerController>(GetController()))
		{
			PlayerController->GetFightingCameraRotation(CameraRotation);
		}
		const FVector ScreenXAxis = FRotationMatrix(FRotator(0.0f, CameraRotation.Yaw, 0.0f)).GetUnitAxis(EAxis::Y);
		DrawDebugDirectionalArrow(
			GetWorld(),
			CombatMidpoint - ScreenXAxis * DirectionLengthCm,
			CombatMidpoint + ScreenXAxis * DirectionLengthCm,
			20.0f,
			FColor::Magenta,
			false,
			0.0f,
			0,
			3.0f);
	}

	FVector OpponentDirection;
	if (TryGetOpponentDirection(OpponentDirection))
	{
		DrawDebugDirectionalArrow(
			GetWorld(),
			PlayerLocation,
			PlayerLocation + OpponentDirection * DirectionLengthCm,
			20.0f,
			FColor::Green,
			false,
			0.0f,
			0,
			3.0f);
	}

	const FVector DesiredFacingDirection = FRotator(0.0f, DesiredFacingYaw, 0.0f).Vector();
	DrawDebugDirectionalArrow(
		GetWorld(),
		PlayerLocation,
		PlayerLocation + DesiredFacingDirection * DirectionLengthCm,
		20.0f,
		FColor::Blue,
		false,
		0.0f,
		0,
		3.0f);

	const FString RelativeDirectionText = UEnum::GetDisplayValueAsText(CurrentRelativeDirection).ToString();
	const FString DebugText = FString::Printf(
		TEXT("Opponent: %s | Relative: %s | Desired Yaw: %.1f"),
		*CurrentOpponent->GetName(),
		*RelativeDirectionText,
		DesiredFacingYaw);
	DrawDebugString(GetWorld(), TextLocation, DebugText, nullptr, FColor::White, 0.0f, false);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(static_cast<int32>(GetUniqueID()), 0.1f, FColor::White, FString::Printf(TEXT("Combat Debug | %s"), *DebugText));
	}
	#endif
}

void AFightingPlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void AFightingPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// 只有 Enhanced Input Component 才能綁定 Input Action；其他元件類型交由父類處理。
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// 使用 Triggered 讓按住輸入時每幀持續更新移動向量。
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AFightingPlayerCharacter::Move);
		// Input Action 結束或被觸發條件取消時，立即停止被動推擠，不能依賴 CharacterMovement 已消耗的加速度。
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &AFightingPlayerCharacter::StopMove);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Canceled, this, &AFightingPlayerCharacter::StopMove);
		if (IsValid(AttackAction))
		{
			EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Started, this, &AFightingPlayerCharacter::Attack);
		}
	}
}
