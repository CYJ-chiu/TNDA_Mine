// Copyright © 2026 USERJOY Technology Co., Ltd.All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * 專案原生 Actor Tags 的單一字串來源。
 *
 * 這些值對應 AActor::Tags 的扁平 FName，不是 Gameplay Tags。C++ 新增或查詢 Actor Tag 時必須
 * 使用此命名空間的常數；Level Actor Details、Blueprint 與文件中的字串也必須與這裡完全一致。
 */
namespace TndaFighterActorTags
{
	/** 標示由本機玩家控制的戰鬥角色。 */
	inline const FName Player(TEXT("Player"));

	/** 標示構成合法戰鬥區域外緣的場地邊界 Actor。 */
	inline const FName ArenaBoundary(TEXT("ArenaBoundary"));

	/** 標示場地邊界中的短牆；必須與 ArenaBoundary 一起使用。 */
	inline const FName ArenaBoundaryShort(TEXT("ArenaBoundaryShort"));

	/** 標示場地邊界中的高牆；必須與 ArenaBoundary 一起使用。 */
	inline const FName ArenaBoundaryTall(TEXT("ArenaBoundaryTall"));
}
