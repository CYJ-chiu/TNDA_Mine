// Copyright © 2026 USERJOY Technology Co., Ltd.All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "FightingGameMode.generated.h"

class AFightingCameraActor;
class AFightingCpuCharacter;
class APlayerStart;

/**
 * 一名本機玩家對一名 CPU 的開場規則入口。
 * 玩家沿用 Unreal 標準生成與 Possess；完成後建立 CPU、雙向配對並設定朝向，再建立相機並切換視角。
 * 關卡須各放置一個 PlayerStartTag 為 PlayerStart、CpuStart 的出生點，不支援重生或回合重置。
 */
UCLASS(abstract)
class AFightingGameMode : public AGameModeBase
{
	GENERATED_BODY()

protected:
	/** 固定選用 PlayerStart Tag；忽略 URL 具名入口與 StartSpot 快取，兩種出生點均須唯一。 */
	virtual AActor* FindPlayerStart_Implementation(AController* Player, const FString& IncomingName) override;

	/** PIE 可能在找不到出生點後改走原點備援；設定已失敗時阻止該路徑生成玩家，其餘沿用父類。 */
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;

	/** 標準玩家初始化完成後，依序完成 CPU、雙向配對、雙方朝向與相機構圖，再指定本機 View Target。 */
	virtual void FinishRestartPlayer(AController* NewPlayer, const FRotator& StartRotation) override;

	/** GameMode Blueprint 的 CPU 類別；必須為有效、非抽象的 FightingCpuCharacter 子類，否則停止開場。 */
	UPROPERTY(EditDefaultsOnly, Category="Fighting Match")
	TSubclassOf<AFightingCpuCharacter> CpuClass;

	/** 可選的格鬥攝影機 Blueprint 子類；未指定時使用原生相機，構圖參數留在相機類別。 */
	UPROPERTY(EditDefaultsOnly, Category="Fighting Match")
	TSubclassOf<AFightingCameraActor> FightingCameraClass;

private:
	/** 只接受關卡內指定 PlayerStartTag 的唯一出生點；排除 PlayerStartPIE，錯誤交由 ReportStartupError 回報。 */
	APlayerStart* FindSpawnPoint(FName StartTag);

	/** 每次世界生命週期只回報一次設定錯誤；PIE 顯示提示並結束遊戲，打包版只記錄 Error。 */
	void ReportStartupError(const FString& Message);

	/** 本次開場建立的 CPU；保留參照可避免重複進入初始化時生成第二名對手。 */
	UPROPERTY(Transient)
	TObjectPtr<AFightingCpuCharacter> CpuCharacter;

	/** 設定失敗後禁止接續生成，也避免 Unreal 重試玩家出生時反覆彈窗。 */
	bool bStartupFailed = false;
};
