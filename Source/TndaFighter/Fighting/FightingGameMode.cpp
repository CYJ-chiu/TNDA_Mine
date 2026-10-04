// Copyright © 2026 USERJOY Technology Co., Ltd.All Rights Reserved.

#include "FightingGameMode.h"
#include "FightingCameraActor.h"
#include "FightingCpuCharacter.h"
#include "FightingPlayerCharacter.h"
#include "FightingPlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/PlayerStartPIE.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "TndaFighter.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Misc/MessageDialog.h"
#endif

APlayerStart* AFightingGameMode::FindSpawnPoint(FName StartTag)
{
	TArray<AActor*> PlayerStarts;
	UGameplayStatics::GetAllActorsOfClass(this, APlayerStart::StaticClass(), PlayerStarts);
	APlayerStart* Result = nullptr;
	int32 MatchCount = 0;
	for (AActor* Candidate : PlayerStarts)
	{
		APlayerStart* Start = Cast<APlayerStart>(Candidate);
		// PIE 的臨時出生點不屬於關卡配置；即使同名也不能掩蓋缺少出生點的錯誤。
		if (IsValid(Start) && !Start->IsA<APlayerStartPIE>() && Start->PlayerStartTag == StartTag)
		{
			Result = Start;
			++MatchCount;
		}
	}

	if (MatchCount != 1)
	{
		ReportStartupError(FString::Printf(
			TEXT("PlayerStartTag '%s' 必須恰有一個出生點，目前找到 %d 個。\n請放置 Player Start Actor，並設定 Player Start > Player Start Tag；Actor 顯示名稱不作為識別。"),
			*StartTag.ToString(), MatchCount));
		return nullptr;
	}
	return Result;
}

AActor* AFightingGameMode::FindPlayerStart_Implementation(AController* Player, const FString& IncomingName)
{
	if (bStartupFailed)
	{
		return nullptr;
	}

	APlayerStart* PlayerStart = FindSpawnPoint(TEXT("PlayerStart"));
	// 父類會先接受具名入口或 StartSpot 快取；在此統一驗證，避免那些路徑繞過固定出生規則。
	return PlayerStart && FindSpawnPoint(TEXT("CpuStart")) ? PlayerStart : nullptr;
}

APawn* AFightingGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	return bStartupFailed ? nullptr : Super::SpawnDefaultPawnAtTransform_Implementation(NewPlayer, SpawnTransform);
}

void AFightingGameMode::FinishRestartPlayer(AController* NewPlayer, const FRotator& StartRotation)
{
	Super::FinishRestartPlayer(NewPlayer, StartRotation);
	if (bStartupFailed || IsValid(CpuCharacter))
	{
		return;
	}

	AFightingPlayerCharacter* PlayerCharacter = NewPlayer ? Cast<AFightingPlayerCharacter>(NewPlayer->GetPawn()) : nullptr;
	if (!IsValid(PlayerCharacter))
	{
		ReportStartupError(TEXT("玩家初始化失敗：Default Pawn Class 必須為 FightingPlayerCharacter 子類，且已成功 Possess。"));
		return;
	}
	APlayerStart* CpuStart = FindSpawnPoint(TEXT("CpuStart"));
	if (!CpuStart)
	{
		return;
	}
	if (!IsValid(CpuClass) || CpuClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		ReportStartupError(TEXT("Class Defaults > Fighting Match > Cpu Class 必須指定有效且非抽象的 FightingCpuCharacter 子類。"));
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	// 延續原 CPU 出生的碰撞策略；構圖與朝向均使用調整後的位置。
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	CpuCharacter = GetWorld()->SpawnActor<AFightingCpuCharacter>(CpuClass, CpuStart->GetActorTransform(), SpawnParameters);
	if (!IsValid(CpuCharacter))
	{
		ReportStartupError(TEXT("CPU 生成失敗，請檢查 Cpu Class 與 CpuStart 設定。"));
		return;
	}
	if (!PlayerCharacter->SetOpponent(CpuCharacter) || !CpuCharacter->SetOpponent(PlayerCharacter))
	{
		// 配對只允許在開場建立一次；失敗時移除本次 CPU，避免留下只有單向參照的半成品對局。
		CpuCharacter->Destroy();
		CpuCharacter = nullptr;
		ReportStartupError(TEXT("玩家與 CPU 配對失敗；角色不可無效、自我配對或覆寫既有對手。"));
		return;
	}

	FVector DirectionToCpu = CpuCharacter->GetActorLocation() - PlayerCharacter->GetActorLocation();
	DirectionToCpu.Z = 0.0f;
	// 出生點水平重合時保留原朝向，避免由零向量決定任意旋轉；不等待落地或額外 Tick。
	if (!DirectionToCpu.IsNearlyZero())
	{
		PlayerCharacter->SetActorRotation(DirectionToCpu.Rotation());
		CpuCharacter->SetActorRotation((-DirectionToCpu).Rotation());
	}
	UE_LOG(LogTndaFighter, Log, TEXT("Fighters paired and positioned: %s and %s."), *PlayerCharacter->GetName(), *CpuCharacter->GetName());

	AFightingPlayerController* PlayerController = Cast<AFightingPlayerController>(NewPlayer);
	if (!PlayerController || !PlayerController->IsLocalPlayerController())
	{
		ReportStartupError(TEXT("Player Controller Class 必須為本機 FightingPlayerController 子類。"));
		return;
	}
	UClass* CameraClass = FightingCameraClass ? FightingCameraClass.Get() : AFightingCameraActor::StaticClass();
	if (CameraClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		ReportStartupError(TEXT("Class Defaults > Fighting Match > Fighting Camera Class 必須為可生成的 FightingCameraActor 子類，或留空使用原生相機。"));
		return;
	}
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AFightingCameraActor* Camera = GetWorld()->SpawnActor<AFightingCameraActor>(CameraClass, FTransform::Identity, SpawnParameters);
    if (IsValid(Camera))
    {
		// Player 跟 Cpu 都 Spawn 完成後才初始化相機，因為 FightingCameraActor 需要雙方角色的參考來計算構圖。
        if (!Camera->InitializeForFighters(PlayerCharacter, CpuCharacter))
        {
            ReportStartupError(TEXT("格鬥相機初始化失敗，請檢查 Fighting Camera Class 與雙方角色。"));
            return;
        }

		// 同步完成 SpringArm 插槽與 CameraComponent 的位置後才切換；Controller 已停用自動選鏡。
		PlayerController->SetViewTarget(Camera);
		if (PlayerController->PlayerCameraManager)
		{
            // 立即切換鏡頭，避免上一個鏡頭的殘影。
			PlayerController->PlayerCameraManager->SetGameCameraCutThisFrame();
		}
    }
	else {
        ReportStartupError(TEXT("格鬥相機生成失敗，請檢查 Fighting Camera Class 設定。"));
        return;
	}

}

void AFightingGameMode::ReportStartupError(const FString& Message)
{
	if (bStartupFailed)
	{
		return;
	}
	bStartupFailed = true;
	FString BlueprintName = GetClass()->GetName();
	BlueprintName.RemoveFromEnd(TEXT("_C"));
	const FString ErrorMessage = FString::Printf(TEXT("關卡：%s\nGameMode：%s\n%s"),
		*GetWorld()->GetMapName(), *BlueprintName, *Message);
	UE_LOG(LogTndaFighter, Error, TEXT("%s"), *ErrorMessage);
#if WITH_EDITOR
	if (GEditor && GetWorld()->WorldType == EWorldType::PIE)
	{
		FMessageDialog::Open(EAppMsgCategory::Error, EAppMsgType::Ok, EAppReturnType::Ok,
			FText::FromString(ErrorMessage), NSLOCTEXT("FightingGameMode", "StartupErrorTitle", "對局初始化設定錯誤"));
		GEditor->RequestEndPlayMap();
	}
#endif
}
