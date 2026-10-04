// Copyright © 2026 USERJOY Technology Co., Ltd.All Rights Reserved.

#include "Fighting/FightingCameraActor.h"
#include "Fighting/FightingCharacter.h"
#include "Fighting/FightingCpuCharacter.h"
#include "Fighting/FightingPlayerCharacter.h"
#include "TndaFighter.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/PackageName.h"
#include "ScopedTransaction.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#endif

AFightingCameraActor::AFightingCameraActor()
{
	PrimaryActorTick.bCanEverTick = true;
	// 先讓 CharacterMovement 與物理更新完成，再依最終角色位置限制範圍並計算本幀鏡頭。
	PrimaryActorTick.TickGroup = TG_PostPhysics;

	// Actor 本身位於雙方焦點；SpringArm 從這個根節點向後配置實際攝影機位置。
	CameraRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CameraRoot"));
	SetRootComponent(CameraRoot);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(CameraRoot);
	CameraBoom->TargetArmLength = MinCameraDistance;
	// 高牆可能落在期望鏡頭位置；沿用引擎的 Camera Channel 探測，受阻時縮短實際臂長。
	CameraBoom->bDoCollisionTest = true;
	CameraBoom->ProbeChannel = ECC_Camera;
	// SpringArm 必須使用本幀構圖結果，不能先於 Actor 更新而留下上一幀插槽。
	CameraBoom->AddTickPrerequisiteActor(this);

	FightingCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FightingCamera"));
	FightingCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	// 固定 FOV，畫面縮放只由 SpringArm 臂長控制，避免同時改變透視感。
	FightingCamera->FieldOfView = 55.0f;
	// stage1 的對戰構圖以 16:9 為基準；其他 Viewport 比例以黑邊保留構圖，避免拉伸。
	FightingCamera->AspectRatio = 16.0f / 9.0f;
	FightingCamera->bConstrainAspectRatio = true;
}

bool AFightingCameraActor::InitializeForFighters(AFightingPlayerCharacter* InPlayer, AFightingCpuCharacter* InCpu)
{
	if (!IsValid(InPlayer) || !IsValid(InCpu))
	{
		return false;
	}
	Player = InPlayer;
	Opponent = InCpu;
	const FVector InitialFocusLocation = AFightingCharacter::CalculateCombatMidpoint(
		InPlayer->GetActorLocation(), InCpu->GetActorLocation())
		+ FVector::UpVector * FocusHeight;
	InitialFocusZ = InitialFocusLocation.Z;
	SetActorLocation(InitialFocusLocation);
	CameraBoom->SetRelativeRotation(FixedCameraRotation);
	CameraBoom->SetRelativeRotation(GetDesiredCameraRotation(InPlayer, InCpu));
	CameraBoom->TargetArmLength = GetDesiredCameraDistance(InPlayer, InCpu);

	// 直接設定臂長不會更新插槽；首次同步計算且略過 Lag，避免 View Target 讀到註冊時的舊位置。
	const bool bLocationLag = CameraBoom->bEnableCameraLag;
	const bool bRotationLag = CameraBoom->bEnableCameraRotationLag;
	CameraBoom->bEnableCameraLag = false;
	CameraBoom->bEnableCameraRotationLag = false;
	CameraBoom->TickComponent(0.0f, LEVELTICK_All, nullptr);
	CameraBoom->bEnableCameraLag = bLocationLag;
	CameraBoom->bEnableCameraRotationLag = bRotationLag;
	return true;
}

FRotator AFightingCameraActor::GetViewRotation() const
{
	return IsValid(FightingCamera) ? FightingCamera->GetComponentRotation() : GetActorRotation();
}

void AFightingCameraActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AFightingPlayerCharacter* PlayerCharacter = Player.Get();
	AFightingCpuCharacter* CpuCharacter = Opponent.Get();
	// 本次對局只追蹤初始化時指定的角色；失效後停止更新，不搜尋替代對手。
	if (!IsValid(PlayerCharacter) || !IsValid(CpuCharacter))
	{
		return;
	}
	ConstrainPlayerToMaxDistance(PlayerCharacter, CpuCharacter);
	SetActorLocation(GetDesiredFocusLocation(PlayerCharacter, CpuCharacter));
	CameraBoom->SetRelativeRotation(GetDesiredCameraRotation(PlayerCharacter, CpuCharacter));
	CameraBoom->TargetArmLength = FMath::FInterpTo(
		CameraBoom->TargetArmLength, GetDesiredCameraDistance(PlayerCharacter, CpuCharacter), DeltaSeconds, DistanceInterpolationSpeed);
}

void AFightingCameraActor::ConstrainPlayerToMaxDistance(AActor* PlayerActor, const AActor* OpponentActor) const
{
	const FVector PlayerLocation = PlayerActor->GetActorLocation();
	const FVector OpponentLocation = OpponentActor->GetActorLocation();
	FVector OpponentToPlayer = PlayerLocation - OpponentLocation;
	OpponentToPlayer.Z = 0.0f;

	// 邊界只看 XY 平面，跳躍高度不應縮小玩家可用的水平移動範圍。
	const float FighterDistance = OpponentToPlayer.Size();
	if (FighterDistance <= MaxFighterDistance || FighterDistance <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector OutwardDirection = OpponentToPlayer / FighterDistance;
	FVector ConstrainedLocation = OpponentLocation + OutwardDirection * MaxFighterDistance;
	// 只修正水平座標，保留玩家目前的跳躍或落下高度。
	ConstrainedLocation.Z = PlayerLocation.Z;
	// 使用 TeleportPhysics 直接校正位置，避免 Sweep 被場景碰撞擋住而持續留在邊界外。
	PlayerActor->SetActorLocation(ConstrainedLocation, false, nullptr, ETeleportType::TeleportPhysics);

	// 移除向外速度，但保留向內、沿邊界與垂直速度，避免角色持續頂住邊界抖動。
	if (ACharacter* PlayerCharacter = Cast<ACharacter>(PlayerActor))
	{
		if (UCharacterMovementComponent* MovementComponent = PlayerCharacter->GetCharacterMovement())
		{
			const float OutwardSpeed = FVector::DotProduct(MovementComponent->Velocity, OutwardDirection);
			if (OutwardSpeed > 0.0f)
			{
				MovementComponent->Velocity -= OutwardDirection * OutwardSpeed;
			}
		}
	}
}

FVector AFightingCameraActor::GetDesiredFocusLocation(const AActor* PlayerActor, const AActor* OpponentActor) const
{
	// X、Y 永遠取即時中點；只有 Z 需要限制，避免其中一名角色跳躍時鏡頭大幅上下晃動。
	FVector FocusLocation = AFightingCharacter::CalculateCombatMidpoint(
		PlayerActor->GetActorLocation(), OpponentActor->GetActorLocation());
	FocusLocation.Z = FMath::Clamp(
		FocusLocation.Z + FocusHeight,
		InitialFocusZ - MaxVerticalFocusOffset,
		InitialFocusZ + MaxVerticalFocusOffset);
	return FocusLocation;
}

FRotator AFightingCameraActor::GetDesiredCameraRotation(const AActor* PlayerActor, const AActor* OpponentActor) const
{
	FVector FighterAxis = OpponentActor->GetActorLocation() - PlayerActor->GetActorLocation();
	FighterAxis.Z = 0.0f;
	if (FighterAxis.IsNearlyZero())
	{
		// 雙方水平位置重合時沒有可用軸向，保留上一幀旋轉可避免 Yaw 跳到任意值。
		return CameraBoom->GetRelativeRotation();
	}

	// 同一條水平軸有兩個相差 180 度的取景側；選離目前 Yaw 最近的一側可讓角色換邊而不翻鏡。
	const float AxisYaw = FighterAxis.Rotation().Yaw;
	const float FirstSideYaw = AxisYaw - 90.0f;
	const float OtherSideYaw = AxisYaw + 90.0f;
	const float CurrentYaw = CameraBoom->GetRelativeRotation().Yaw;
	const float FirstSideDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(CurrentYaw, FirstSideYaw));
	const float OtherSideDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(CurrentYaw, OtherSideYaw));
	const float DesiredYaw = FirstSideDelta <= OtherSideDelta ? FirstSideYaw : OtherSideYaw;

	return FRotator(FixedCameraRotation.Pitch, DesiredYaw, FixedCameraRotation.Roll);
}

float AFightingCameraActor::GetDesiredCameraDistance(const AActor* PlayerActor, const AActor* OpponentActor) const
{
	// Dist2D 忽略跳躍高度，鏡頭遠近只反映角色在格鬥平面上的分離程度。
	const float FighterDistance = FVector::Dist2D(PlayerActor->GetActorLocation(), OpponentActor->GetActorLocation());
	// Max 防止 Blueprint 把上下限設反或設成相同值，避免零長度輸入區間與負向臂長範圍。
	return FMath::GetMappedRangeValueClamped(
		FVector2D(MinFighterDistance, FMath::Max(MinFighterDistance + KINDA_SMALL_NUMBER, MaxFighterDistance)),
		FVector2D(MinCameraDistance, FMath::Max(MinCameraDistance, MaxCameraDistance)),
		FighterDistance);
}

void AFightingCameraActor::ApplyRuntimeValuesToBP()
{
#if WITH_EDITOR
	// PIE Actor 才持有設計者在執行中調出的值；Editor World 的 Actor 不是本功能的來源，
	// Game／Preview World 也不能安全地回寫 Content 資產。
	UWorld* World = GetWorld();
	if (!World || World->WorldType != EWorldType::PIE)
	{
		UE_LOG(
			LogTndaFighter,
			Warning,
			TEXT("ApplyRuntimeValuesToBP can only be used on a PIE actor instance."));
		return;
	}

	// 從實例的 exact generated class 反查來源 Blueprint，避免把子類調校值誤寫到原生父類
	// 或另一個共用相機 Blueprint。
	UBlueprint* Blueprint = Cast<UBlueprint>(GetClass()->ClassGeneratedBy);
	if (!Blueprint)
	{
		UE_LOG(
			LogTndaFighter,
			Warning,
			TEXT("%s was not spawned from a Blueprint class; there is no Blueprint asset to update."),
			*GetName());
		return;
	}

	// 同樣取 exact generated class 的 CDO；這是 Class Defaults 實際序列化的物件。
	AFightingCameraActor* BlueprintCDO =
		GetClass()->GetDefaultObject<AFightingCameraActor>();

	if (!BlueprintCDO || BlueprintCDO == this)
	{
		UE_LOG(
			LogTndaFighter,
			Error,
			TEXT("Blueprint %s does not provide a valid class default object for runtime camera write-back."),
			*Blueprint->GetName());
		return;
	}

	// 採白名單而非複製整個 Actor，避免把 Owner、Transform、元件即時狀態或對局中的
	// transient 參照寫進 Blueprint。GET_MEMBER_NAME_CHECKED 也讓欄位改名在編譯期就會失敗。
	static const FName RuntimeTuningProperties[] =
	{
		GET_MEMBER_NAME_CHECKED(AFightingCameraActor, FocusHeight),
		GET_MEMBER_NAME_CHECKED(AFightingCameraActor, FixedCameraRotation),
		GET_MEMBER_NAME_CHECKED(AFightingCameraActor, MinCameraDistance),
		GET_MEMBER_NAME_CHECKED(AFightingCameraActor, MaxCameraDistance),
		GET_MEMBER_NAME_CHECKED(AFightingCameraActor, MinFighterDistance),
		GET_MEMBER_NAME_CHECKED(AFightingCameraActor, MaxFighterDistance),
		GET_MEMBER_NAME_CHECKED(AFightingCameraActor, DistanceInterpolationSpeed),
		GET_MEMBER_NAME_CHECKED(AFightingCameraActor, MaxVerticalFocusOffset),
	};

	// 先解析並比較全部欄位，再開始任何修改；若白名單與反射資料不同步，這次操作不會
	// 只套用一半。Identical_InContainer 會依實際屬性型別比較，不靠字串轉換。
	TArray<FProperty*> ChangedProperties;

	for (const FName PropertyName : RuntimeTuningProperties)
	{
		FProperty* Property =
			FindFProperty<FProperty>(AFightingCameraActor::StaticClass(), PropertyName);

		if (!Property)
		{
			UE_LOG(
				LogTndaFighter,
				Error,
				TEXT("Runtime camera write-back property %s could not be resolved; Blueprint %s was not modified."),
				*PropertyName.ToString(),
				*Blueprint->GetName());
			return;
		}

		if (!Property->Identical_InContainer(this, BlueprintCDO))
		{
			ChangedProperties.Add(Property);
		}
	}

	if (ChangedProperties.IsEmpty())
	{
		UE_LOG(
			LogTndaFighter,
			Log,
			TEXT("No runtime camera values changed for Blueprint %s."),
			*Blueprint->GetName());
		return;
	}

	// 把 CDO 變更納入 Editor Undo；Modify 必須在 CopyCompleteValue_InContainer 前呼叫，
	// 才能讓 Transaction 保存修改前的 Blueprint 與 CDO 狀態。
	const FScopedTransaction Transaction(
		NSLOCTEXT(
			"FightingCameraActor",
			"ApplyRuntimeValuesToBP",
			"Apply Runtime Camera Values to Blueprint"));

	Blueprint->Modify();
	BlueprintCDO->Modify();

	for (FProperty* Property : ChangedProperties)
	{
		// PreEdit／PostEdit 通知 Details、資產註冊與其他 Editor 系統這是正式屬性異動；
		// CopyCompleteValue_InContainer 則保留 FVector／FRotator 等複合型別的完整值。
		BlueprintCDO->PreEditChange(Property);

		Property->CopyCompleteValue_InContainer(
			BlueprintCDO,
			this);

		FPropertyChangedEvent ChangedEvent(
			Property,
			EPropertyChangeType::ValueSet);

		BlueprintCDO->PostEditChangeProperty(ChangedEvent);
	}

	// 只標記 Blueprint 與 package，不在 PIE 中 Compile；Compile 會 reinstance generated class，
	// 使目前正在執行的相機實例失效。需要編譯時應在停止 PIE 後另行執行。
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	Blueprint->MarkPackageDirty();

	// ClassGeneratedBy 已確認來源，但仍拒絕 transient、compiled-in 與 PIE 複本 package，
	// 避免把暫存世界或引擎類別當成可保存的 Content 資產。
	UPackage* BlueprintPackage = Blueprint->GetOutermost();
	if (!BlueprintPackage ||
		BlueprintPackage == GetTransientPackage() ||
		BlueprintPackage->HasAnyPackageFlags(PKG_CompiledIn | PKG_PlayInEditor))
	{
		UE_LOG(
			LogTndaFighter,
			Error,
			TEXT("Applied %d runtime camera values to Blueprint %s, but its package is not a saveable asset package."),
			ChangedProperties.Num(),
			*Blueprint->GetName());
		return;
	}

	FString PackageFilename;
	if (!FPackageName::TryConvertLongPackageNameToFilename(
			BlueprintPackage->GetName(),
			PackageFilename,
			FPackageName::GetAssetPackageExtension()))
	{
		UE_LOG(
			LogTndaFighter,
			Error,
			TEXT("Applied %d runtime camera values to Blueprint %s, but could not resolve package %s to an asset filename."),
			ChangedProperties.Num(),
			*Blueprint->GetName(),
			*BlueprintPackage->GetName());
		return;
	}

	// SavePackage 是同步、無對話框的精確資產保存。先 FullyLoad，避免只載入部分 package
	// 時覆寫同一 Blueprint 內尚未載入的 exports；TopLevelFlags 保留標準資產根物件旗標。
	BlueprintPackage->FullyLoad();

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	SaveArgs.Error = GError;
	SaveArgs.bSlowTask = false;

	if (!UPackage::SavePackage(BlueprintPackage, Blueprint, *PackageFilename, SaveArgs))
	{
		UE_LOG(
			LogTndaFighter,
			Error,
			TEXT("Applied %d runtime camera values to Blueprint %s, but failed to save asset %s. The package remains dirty in memory."),
			ChangedProperties.Num(),
			*Blueprint->GetName(),
			*PackageFilename);
		return;
	}

	// 成功返回後仍 dirty，代表 package 另有未包含在這次保存完成點的 Editor 異動；保留
	// dirty 狀態並警告，不能把它誤報成所有變更都已安全落盤。
	if (BlueprintPackage->IsDirty())
	{
		UE_LOG(
			LogTndaFighter,
			Warning,
			TEXT("Saved Blueprint asset %s, but its package is still dirty because additional changes remain in memory."),
			*PackageFilename);
	}

	UE_LOG(
		LogTndaFighter,
		Log,
		TEXT("Applied %d runtime camera values to Blueprint %s and saved asset %s."),
		ChangedProperties.Num(),
		*Blueprint->GetName(),
		*PackageFilename);
#endif
}
