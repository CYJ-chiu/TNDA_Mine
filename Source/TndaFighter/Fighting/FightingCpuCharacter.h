// Copyright © 2026 USERJOY Technology Co., Ltd.All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FightingCharacter.h"
#include "Animation/AnimMontage.h"
#include "Engine/TimerHandle.h"
#include "FightingCpuCharacter.generated.h"

/** 預留給 StateTree：CPU 攻擊動畫播放完畢時通知等待中的任務。 */
DECLARE_DELEGATE(FOnEnemyAttackCompleted);

/** 預留給 StateTree：CPU 從空中落地時通知等待中的任務。 */
DECLARE_DELEGATE(FOnEnemyLanded);

/** CPU 死亡時供 Blueprint 與其他執行期系統訂閱的多播事件型別。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEnemyDied);

/** CPU 在沒有正式 AI 時可用的相對 1P 測試移動模式。 */
UENUM(BlueprintType)
enum class EFightingCpuTestMovementMode : uint8
{
	/** 不提供測試移動輸入，讓 CPU 維持既有非 AI 行為。 */
	Disabled UMETA(DisplayName = "關閉測試流程"),

	/** 以 CPU 原本最高移速的一半向 1P 靠近。 */
	ApproachPlayerSlow UMETA(DisplayName = "往 1P 方向靠近（慢）"),

	/** 以 CPU 原本最高移速向 1P 靠近。 */
	ApproachPlayerFast UMETA(DisplayName = "往 1P 方向靠近（快）"),

	/** 以 CPU 原本最高移速的一半遠離 1P。 */
	RetreatFromPlayerSlow UMETA(DisplayName = "往 1P 方向遠離（慢）"),

	/** 以 CPU 原本最高移速遠離 1P。 */
	RetreatFromPlayerFast UMETA(DisplayName = "往 1P 方向遠離（快）")
};

/**
 * CPU 對手的原生角色基底。
 *
 * 建構時指定 `AFightingCpuController`，但目前將 Auto Possess AI 設為 Disabled，
 * 因此生成後不會自動建立 Controller 或執行 StateTree。Editor 測試模式依目前相對 1P 的方向
 * 提供地面輸入；這個模式不取代日後的正式 AI。
 */
UCLASS(abstract)
class AFightingCpuCharacter : public AFightingCharacter
{
	GENERATED_BODY()

public:
	
	/** 設定 AI Controller 類別、暫停自動接管，並初始化碰撞與旋轉方式。 */
	AFightingCpuCharacter();


protected:

	/** CPU 的一般生命週期 Tick；Editor 測試移動委派給私有入口。 */
	virtual void Tick(float DeltaSeconds) override;

	/** CPU 離開世界時的清理入口；目前只保留父類生命週期。 */
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

	/**
	 * Editor 用的 CPU 移動來源，預設關閉；啟用時提供向 1P 靠近或遠離的慢／快四個選項。
	 * 正式 AI 不受此列舉實作或啟用狀態影響。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CPU Test Movement")
	EFightingCpuTestMovementMode TestMovementMode = EFightingCpuTestMovementMode::Disabled;

private:
	/** 集中處理 Editor 用的相對 1P 測試移動，不影響一般 CPU 生命週期。 */
	void UpdateTestMovement();
};
