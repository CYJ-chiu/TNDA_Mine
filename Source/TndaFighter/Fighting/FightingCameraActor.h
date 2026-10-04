// Copyright © 2026 USERJOY Technology Co., Ltd.All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FightingCameraActor.generated.h"

class AFightingCpuCharacter;
class AFightingPlayerCharacter;
class UCameraComponent;
class USceneComponent;
class USpringArmComponent;

/**
 * 玩家與 CPU 共用的一對一格鬥攝影機。
 *
 * GameMode 在雙方定位後傳入角色，立即完成初始構圖與 SpringArm 插槽，才由 GameMode
 * 設為 View Target。之後每幀追蹤兩人的水平軸與中點、平滑調整鏡頭距離，並限制玩家
 * 不得超出可構圖範圍。角色換邊時會選擇最接近目前 Yaw 的取景側，避免鏡頭翻轉 180 度。
 */
UCLASS(BlueprintType)
class AFightingCameraActor : public AActor
{
	GENERATED_BODY()

public:
	/** 建立相機根節點、SpringArm 與固定 16:9 輸出的 CameraComponent。 */
	AFightingCameraActor();

	/**
	 * 在 GameMode 指定 View Target 前呼叫，以雙方實際位置直接套用初始構圖，不做插值。
	 * 必須已完成 SpawnActor；角色無效時回傳 false，不進行初始化。
	 */
	bool InitializeForFighters(AFightingPlayerCharacter* InPlayer, AFightingCpuCharacter* InCpu);

	/** 回傳目前 CameraComponent 的世界旋轉；元件無效時退回 Actor Rotation。 */
	FRotator GetViewRotation() const;

protected:
	/** 在物理更新後限制玩家位置，接著更新雙人構圖；指定角色失效時停止更新。 */
	virtual void Tick(float DeltaSeconds) override;

private:
	/**
	 * 將玩家限制在 CPU 周圍 `MaxFighterDistance` 的水平圓形範圍內。
	 * 超界時只移除向外的水平速度，保留向內、切線與垂直速度。
	 */
	void ConstrainPlayerToMaxDistance(AActor* PlayerActor, const AActor* OpponentActor) const;

	/** 計算兩名角色中點，並把加上 `FocusHeight` 後的 Z 限制在初始焦點範圍內。 */
	FVector GetDesiredFocusLocation(const AActor* PlayerActor, const AActor* OpponentActor) const;

	/**
	 * 計算讓兩名角色連線在畫面上保持水平的鏡頭旋轉。
	 * 兩個候選 Yaw 相差 180 度，函式會選擇與目前鏡頭角度差較小的一側。
	 */
	FRotator GetDesiredCameraRotation(const AActor* PlayerActor, const AActor* OpponentActor) const;

	/** 依兩名角色完整水平距離，在最小與最大鏡頭距離之間做限制後的線性換算。 */
	float GetDesiredCameraDistance(const AActor* PlayerActor, const AActor* OpponentActor) const;

	/** 持續追蹤兩名角色中點的根節點。 */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USceneComponent> CameraRoot;

	/**
	 * 以旋轉與臂長將攝影機放在中點周圍；遇到阻擋 Camera Channel 的場景物件時縮短實際臂長。
	 */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USpringArmComponent> CameraBoom;

	/** 實際輸出 55° FOV、固定 16:9 構圖的攝影機元件。 */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UCameraComponent> FightingCamera;

	/** GameMode 指定的本機玩家；弱參照不延長角色生命週期。 */
	UPROPERTY(Transient)
	TWeakObjectPtr<AFightingPlayerCharacter> Player;

	/** GameMode 指定的唯一 CPU，不自動搜尋替代對手。 */
	UPROPERTY(Transient)
	TWeakObjectPtr<AFightingCpuCharacter> Opponent;

	/** 攝影機焦點相對角色原點向上的高度。 */
	UPROPERTY(EditAnywhere, Category="Fighting Camera", meta=(Units="cm"))
	float FocusHeight = 45.0f;

	/** 格鬥鏡頭固定使用的 Pitch、Roll；Yaw 只提供初始取景側，啟用後會依角色水平軸更新。 */
	UPROPERTY(EditAnywhere, Category="Fighting Camera")
	FRotator FixedCameraRotation = FRotator(-10.0f, 0.0f, 0.0f);

	/** 兩名角色重疊或很接近時使用的最小鏡頭距離。 */
	UPROPERTY(EditAnywhere, Category="Fighting Camera|Distance", meta=(ClampMin="100", Units="cm"))
	float MinCameraDistance = 440.0f;

	/** 兩名角色到達最大構圖間距時使用的鏡頭距離上限值；實際值不會小於 `MinCameraDistance`。 */
	UPROPERTY(EditAnywhere, Category="Fighting Camera|Distance", meta=(ClampMin="100", Units="cm"))
	float MaxCameraDistance = 690.0f;

	/** 開始由最小鏡頭距離拉遠的兩名角色水平距離。 */
	UPROPERTY(EditAnywhere, Category="Fighting Camera|Distance", meta=(ClampMin="0", Units="cm"))
	float MinFighterDistance = 110.0f;

	/**
	 * 到達最大鏡頭距離的兩名角色水平距離，同時也是玩家可離開 CPU 的最大水平距離。
	 */
	UPROPERTY(EditAnywhere, Category="Fighting Camera|Distance", meta=(ClampMin="1", Units="cm"))
	float MaxFighterDistance = 570.0f;

	/** 鏡頭臂長的插值速度；值越高越快，設為 0 時會直接套用目標距離。 */
	UPROPERTY(EditAnywhere, Category="Fighting Camera|Smoothing", meta=(ClampMin="0"))
	float DistanceInterpolationSpeed = 5.0f;

	/** 相對初始焦點允許追蹤的最大垂直偏移。 */
	UPROPERTY(EditAnywhere, Category="Fighting Camera|Focus", meta=(ClampMin="0", Units="cm"))
	float MaxVerticalFocusOffset = 250.0f;

	/** 將 PIE Instance 的攝影機調校值套用到來源 Blueprint Defaults，並直接保存該資產。 */
	UFUNCTION(CallInEditor, Category = "Runtime Tuning")
	void ApplyRuntimeValuesToBP();

	/** 第一次完成構圖時的焦點高度，用來限制跳躍造成的垂直追蹤。 */
	float InitialFocusZ = 0.0f;
};
