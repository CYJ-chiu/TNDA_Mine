// Copyright © 2026 USERJOY Technology Co., Ltd.All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "FightingCharacter.generated.h"

class UAnimInstance;
class UAnimMontage;

/** 玩家移動輸入相對於目前對手方向的格鬥語意。 */
UENUM(BlueprintType)
enum class ERelativeDirection : uint8
{
	/** 沒有有效水平移動，或移動對對手方向的投影落在中立區間。 */
	Neutral UMETA(DisplayName = "Neutral"),

	/** 移動方向明確背離對手，對應格鬥數字方向 4。 */
	Backward4 UMETA(DisplayName = "Backward 4"),

	/** 移動方向明確朝向對手，對應格鬥數字方向 6。 */
	Forward6 UMETA(DisplayName = "Forward 6")
};

/**
 * 玩家與 CPU 共用的格鬥角色基底。
 *
 * 此類別保存由對局啟動流程建立的唯一對手參照，並從雙方位置提供戰鬥中點、穩定的水平對手方向與共同自動面向。
 * 它不搜尋或更換對手；角色面向只在一般地面狀態更新，避免與 Montage、Root Motion 或空中動作競爭。
 */
UCLASS(abstract)
class AFightingCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	/** 啟用共同資料更新 Tick；衍生類別仍可指定自己的 Tick Group。 */
	AFightingCharacter();

	/** 嘗試立即執行攻擊語意；本週僅接受 X，不合法或已有播放時拒絕且不預約。CPU 不要求 Controller。 */
	UFUNCTION(BlueprintCallable, Category = "Fighting Character|Attack")
	virtual bool TryExecuteCommand(FName Command);

	/** 請求目前攻擊依資產設定淡出；重複呼叫不延長淡出，完整結束前仍保持鎖定。 */
	UFUNCTION(BlueprintCallable, Category = "Fighting Character|Attack")
	void CancelAttack();

	/** 正常播放與中斷淡出期間皆為 true；僅由共同生命週期寫入。 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Fighting Character|Attack")
	bool bIsAttacking = false;

	/** 本次已接受的語意；完整收尾後清為 None，拒絕的輸入不會覆寫它。 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Fighting Character|Attack")
	FName CurrentCommand = NAME_None;

	/**
	 * 指定本角色在當次對局中的唯一對手。
	 * 無效、自身或與既有有效配對不同的角色會被拒絕；相同角色可安全重複指定。
	 */
	bool SetOpponent(AFightingCharacter* InOpponent);

	/** 回傳由 GameMode 建立的弱對手參照；對手已失效或尚未配對時回傳 nullptr。 */
	UFUNCTION(BlueprintPure, Category = "Fighting Character|Opponent")
	AFightingCharacter* GetOpponent() const { return Opponent.Get(); }

	/**
	 * 取得雙方世界位置的中點。
	 * 尚未配對或對手已失效時輸出零向量並回傳 false。
	 */
	UFUNCTION(BlueprintPure, Category = "Fighting Character|Opponent")
	bool TryGetCombatMidpoint(FVector& OutCombatMidpoint) const;

	/**
	 * 取得由本角色指向對手的最後有效水平單位向量。
	 * 水平距離不大於 10 cm 時沿用歷史值；尚無歷史值或對手無效時輸出零向量並回傳 false。
	 */
	UFUNCTION(BlueprintPure, Category = "Fighting Character|Opponent")
	bool TryGetOpponentDirection(FVector& OutDirection) const;

	/**
	 * 回報角色是否處於一般地面待機或移動狀態。
	 * 攻擊鎖定、離地、播放 Montage 或使用 Root Motion 時回傳 false，供推擠與自動面向共用資格判定。
	 */
	UFUNCTION(BlueprintPure, Category = "Fighting Character|State")
	bool IsInNormalGroundState() const;

	/** 純計算雙方世界位置的中點，供不依賴 World 的規則測試與執行期查詢共用。 */
	static FVector CalculateCombatMidpoint(const FVector& FirstLocation, const FVector& SecondLocation);

	/**
	 * 依雙方位置更新水平對手方向。
	 * 水平距離大於 10 cm 時寫入新的單位向量；否則保留傳入歷史值，並以回傳值表示是否已有有效方向。
	 */
	static bool UpdateHorizontalOpponentDirection(
		const FVector& CharacterLocation,
		const FVector& OpponentLocation,
		FVector& InOutLastValidDirection);

	/**
	 * 將本次世界移動方向相對於對手方向分類為 4／6／Neutral。
	 * 兩個向量都會先移除 Z 並正規化；Dot Product 嚴格大於 0.1 為 6、嚴格小於 -0.1 為 4。
	 */
	static ERelativeDirection ClassifyRelativeDirection(
		const FVector& WorldMovementDirection,
		const FVector& OpponentDirection);

protected:
	/** 每幀檢查攻擊播放者及資格，再更新方向；攻擊淡出期間仍不寫入面向。 */
	virtual void Tick(float DeltaSeconds) override;

	/** Actor／World 結束時先讓回呼失效並釋放本次播放，不等待動畫 Tick。 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 成功出招後清除衍生角色專用移動意圖；不清除仍按住的方向語意。 */
	virtual void ClearAttackMovementIntent();

	/** Class Defaults 指定單次、非循環且啟用 Auto Blend Out 的 Montage；未配置時安全拒絕攻擊。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fighting Character|Attack")
	TObjectPtr<UAnimMontage> BasicAttackMontage;

	/** 自動面向的旋轉插值速度；0 代表符合資格時立即套用目標角度。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fighting Character|Facing", meta = (ClampMin = "0.0"))
	float FacingInterpolationSpeed = 10.0f;

	/** 最近一次有效水平對手方向換算出的世界 Yaw；近距離重疊期間保持不變。 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Fighting Character|Facing")
	float DesiredFacingYaw = 0.0f;

private:
	/** 只接受原 AnimInstance 與 instance ID 的結束事件，防止同資產舊回呼解除新攻擊。 */
	void HandleAttackEnded(UAnimMontage* Montage, bool bInterrupted, TWeakObjectPtr<UAnimInstance> PlaybackOwner, int32 InstanceId);

	/** 可重複收尾；必要時僅停止自己擁有的 instance，且先使排程中的舊回呼失效。 */
	void FinishAttack(bool bStopPlayback);

	/** 在面向更新前偵測失效；播放者重建直接收尾，離地或失去對手則請求淡出。 */
	void UpdateAttack();

	/** 以弱參照配合 ID 找回含淡出的原始 instance，禁止保存可能被引擎刪除的裸指標。 */
	TWeakObjectPtr<UAnimInstance> AttackAnimInstance;

	/** 僅識別當次播放，收尾後為 INDEX_NONE；不能用 Montage 資產指標取代。 */
	int32 AttackInstanceId = INDEX_NONE;

	/** 本次確實 Push 過才可對仍存活的同一 instance Pop。 */
	bool bAttackRootMotionDisabled = false;

	/** 依目前配對位置刷新方向歷史；無效配對不會建立任意方向。 */
	void RefreshOpponentDirection();

	/** 在一般地面狀態寫入 Actor Yaw；第一次有效面向立即套用，之後依設定速度插值。 */
	void UpdateFacing(float DeltaSeconds);

	/** 由 GameMode 一次建立的唯一弱對手參照；不在角色 Tick 內搜尋或重新配對。 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Fighting Character|Opponent", meta = (AllowPrivateAccess = "true"))
	TWeakObjectPtr<AFightingCharacter> Opponent;

	/** 最近一次在水平距離大於 10 cm 時取得的對手方向；接近重疊期間保持不變。 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Fighting Character|Opponent", meta = (AllowPrivateAccess = "true"))
	FVector LastValidOpponentDirection = FVector::ZeroVector;

	/** 區分尚未取得方向與方向值本身，避免用零向量假裝可分類。 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Fighting Character|Opponent", meta = (AllowPrivateAccess = "true"))
	bool bHasValidOpponentDirection = false;

	/** 區分 DesiredFacingYaw 的預設值與從有效對手方向取得的實際目標。 */
	bool bHasDesiredFacingYaw = false;

	/** 首次成功寫入面向後才啟用插值，確保配對開場不從任意初始角度慢慢轉身。 */
	bool bHasAppliedFacing = false;
};
