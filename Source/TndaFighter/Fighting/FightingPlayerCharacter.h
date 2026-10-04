// Copyright © 2026 USERJOY Technology Co., Ltd.All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FightingCharacter.h"
#include "FightingPlayerCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
struct FInputActionValue;
class UWidgetComponent;

/** 玩家目前相對方向語意改變時發布的新值；不保存歷史事件。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRelativeDirectionChanged, ERelativeDirection, NewDirection);

/**
 * 本機玩家操作的格鬥角色。
 *
 * 原生類別負責移動輸入、相對格鬥攝影機的方向換算，以及一對一地面推擠的單一解算點。
 * 推擠只讀取雙方主動移動意圖；被動位移不會偽造成輸入，因此待機角色可保持待機動畫。
 */
UCLASS(abstract)
class AFightingPlayerCharacter : public AFightingCharacter
{
	GENERATED_BODY()
	
protected:

	/** 輸出 Vector2D 的 Enhanced Input 移動動作；X 為左右、Y 為前後。 */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* MoveAction;

	/** Class Defaults 指定 Boolean 攻擊動作；只綁 Started，未指定時保留既有移動。 */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> AttackAction;

public:
	
	/** 設定碰撞膠囊、移動速度與供其他系統辨識玩家的 Actor Tag。 */
	AFightingPlayerCharacter();

	/** 玩家另要求有效 Controller，通過後沿用共同攻擊入口；失去控制權時不接受新攻擊。 */
	virtual bool TryExecuteCommand(FName Command) override;

protected:

	/** 接收 Enhanced Input 值並轉送到共用的 `DoMove` 入口。 */
	void Move(const FInputActionValue& Value);

	/** Enhanced Input 移動動作結束或取消時，立刻移除推擠意圖，避免鬆鍵後仍推動對手。 */
	void StopMove(const FInputActionValue& Value);

	/** 每次按下只送出一次 X；共用入口拒絕時不排隊或重試。 */
	void Attack(const FInputActionValue& Value);

	/** 成功出招只清推擠意圖，保留當下方向語意供攻擊期間繼續更新。 */
	virtual void ClearAttackMovementIntent() override;

	/** 失去 Controller 時清除移動與方向並請求攻擊淡出，不自動重新 Possess。 */
	virtual void UnPossessed() override;

public:

	/**
	 * 處理硬體輸入或 UI 傳入的移動軸值。
	 * 方向以啟用中的格鬥攝影機為準；攻擊中仍更新相對方向，但不產生主動移動或推擠。
	 */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** 目前移動輸入相對於配對對手的 4／6／Neutral；移動結束時回到 Neutral。 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Input|Relative Direction")
	ERelativeDirection CurrentRelativeDirection = ERelativeDirection::Neutral;

	/** 只在 `CurrentRelativeDirection` 語意實際改變時發布，不會因站位變更回頭重播舊值。 */
	UPROPERTY(BlueprintAssignable, Category = "Input|Relative Direction")
	FOnRelativeDirectionChanged OnRelativeDirectionChanged;

	/** 顯示目前配對、戰鬥軸、對手方向、目標面向與相對方向；Class Default 預設關閉，Console 指令可改寫執行中 instance。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fighting Debug")
	bool bShowCombatDebug = false;

protected:

	/** 角色進入遊戲世界時的初始化入口；目前只保留父類生命週期。 */
	virtual void BeginPlay() override;

	/**
	 * 在 PrePhysics 解算當前 1P 與 CPU 的地面推擠，再讓 PostPhysics 的格鬥攝影機處理最大距離限制。
	 * 只有此角色執行一對一解算，避免兩名角色同時對同一接觸施加重複位移。
	 */
	virtual void Tick(float DeltaSeconds) override;

	/** 角色離開世界時的清理入口；目前只保留父類生命週期。 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 綁定移動的持續／結束事件；AttackAction 有效時另以 Started 綁定單次攻擊。 */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

private:
	/** 更新目前相對方向並在值改變時發布通知；相同值不重複廣播。 */
	void SetCurrentRelativeDirection(ERelativeDirection NewDirection);

	/**
	 * 解算接觸中兩名地面角色的推擠、對頂與同向追上。
	 * 相向意圖會停止額外位移；單方朝向意圖以 Capsule Sweep 取得可行分量，再讓推動者同步跟上實際位移。
	 */
	void ResolveGroundPushInteraction(float DeltaSeconds);

	/** 以一幀 Debug Draw 顯示共同 Gameplay 資料；Shipping 組態不繪製。 */
	void DrawCombatDebug() const;

	/**
	 * 由輸入回呼保存到本幀結束的 1P 地面意圖。
	 * CharacterMovement 在 PostPhysics 前已消耗 CurrentAcceleration，因此推擠解析不能再從它讀取 1P 輸入。
	 */
	FVector GroundPushIntent = FVector::ZeroVector;

	/** 與 `GroundPushIntent` 對應的類比輸入比例，範圍為 0 到 1。 */
	float GroundPushInputScale = 0.0f;

};
