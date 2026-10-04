// Copyright © 2026 USERJOY Technology Co., Ltd.All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "FightingPlayerController.generated.h"

class UInputMappingContext;

/**
 * 本機格鬥玩家的控制中樞。
 *
 * 負責掛載 Enhanced Input Mapping Context，以及提供實際 View Target 的格鬥相機旋轉。
 * 關卡再戰或 Pawn 重建流程不在此類處理，日後由 GameMode 或關卡流程另行評估。
 */
UCLASS(abstract, Config="Game")
class AFightingPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** 停用引擎自動選鏡，由 GameMode 在雙方定位後指定 View Target。 */
	AFightingPlayerController();
	
protected:

	/** 本機玩家啟用的 Enhanced Input Mapping Context；全部以優先權 0 加入。 */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** 將預設 Mapping Context 掛到本機 LocalPlayer 的 Enhanced Input Subsystem。 */
	virtual void SetupInputComponent() override;

public:

	/** 目前 View Target 為有效格鬥攝影機時回傳 true。 */
	bool IsFightingCameraActive() const;

	/**
	 * 提供目前格鬥攝影機旋轉，供角色計算相對畫面的移動方向。
	 * 攝影機尚未啟用時回傳 false，且不修改 `OutRotation`。
	 */
	bool GetFightingCameraRotation(FRotator& OutRotation) const;

};
