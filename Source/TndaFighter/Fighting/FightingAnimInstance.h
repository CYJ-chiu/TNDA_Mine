// Copyright © 2026 USERJOY Technology Co., Ltd.All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "FightingAnimInstance.generated.h"

class ACharacter;
class UCharacterMovementComponent;

/**
 * 戰鬥角色共用的原生動畫資料來源。
 *
 * 這個類別只整理 AnimGraph 與狀態轉換需要的 locomotion 資料，不負責播放動畫、
 * 切換狀態或處理輸入。玩家與 CPU 的 AnimBP 可以共用同一份計算規則，避免各自在
 * EventGraph 維護生命週期與每幀資料更新。
 */
UCLASS()
class UFightingAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:

	/** AnimInstance 取得擁有者時快取角色與移動元件；預覽或擁有者無效時保留安全空值。 */
	virtual void NativeInitializeAnimation() override;

	/** 每幀從 CharacterMovement 更新 AnimGraph 需要的移動、方向與空中狀態。 */
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:

	/** AnimGraph 使用的速度；角色受阻但仍有輸入時，改用輸入方向模擬持續行走。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Animation|Locomotion", meta = (DisplayName = "Velocity"))
	FVector AnimationVelocity = FVector::ZeroVector;

	/** 動畫速度的水平長度，忽略跳躍與落下造成的垂直分量。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Animation|Locomotion", meta = (DisplayName = "Ground Speed", Units = "cm/s"))
	double GroundSpeed = 0.0;

	/** 動畫速度相對角色面向的角度；依 CharacterMovement 旋轉模式決定是否限制側向角度。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Animation|Locomotion", meta = (DisplayName = "Direction", Units = "deg"))
	double MovementDirection = 0.0;

	/** 水平動畫速度超過待機門檻，而且角色目前仍有移動輸入。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Animation|Locomotion", meta = (DisplayName = "Should Move"))
	bool bShouldMove = false;

	/** CharacterMovement 目前是否處於 falling 移動模式。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Animation|Locomotion", meta = (DisplayName = "Is Falling"))
	bool bIsFalling = false;

	/**
	 * 角色仍有輸入但實際速度過低時，提供給 BlendSpace 的替代速度。
	 * 預設 300 cm/s 對應目前 Walk sample，可在 AnimBP Class Defaults 調整。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Locomotion", meta = (DisplayName = "Blocked Movement Animation Speed", ClampMin = "0.0", Units = "cm/s"))
	double BlockedMovementAnimationSpeed = 300.0;

private:

	/** 重新取得目前的 Pawn Owner；角色或移動元件不存在時清除快取與輸出資料。 */
	void CacheOwnerReferences();

	/** 將所有 AnimGraph 輸出恢復成待機值，避免失效擁有者留下上一影格資料。 */
	void ResetLocomotionData();

	/** AnimInstance 的角色擁有者；只在遊戲執行緒的動畫更新生命週期中使用。 */
	UPROPERTY(Transient)
	TObjectPtr<ACharacter> OwningCharacter;

	/** 角色的 CharacterMovement；所有 locomotion 原始資料都由這個元件取得。 */
	UPROPERTY(Transient)
	TObjectPtr<UCharacterMovementComponent> CharacterMovement;
};
