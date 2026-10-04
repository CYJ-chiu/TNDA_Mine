// Copyright © 2026 USERJOY Technology Co., Ltd.All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Fighting/FightingCharacter.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFightingCharacterCombatMidpointTest,
	"TndaFighter.Fighting.Character.CombatMidpoint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFightingCharacterCombatMidpointTest::RunTest(const FString& Parameters)
{
	const FVector Midpoint = AFightingCharacter::CalculateCombatMidpoint(
		FVector(-120.0f, 40.0f, 10.0f),
		FVector(280.0f, -20.0f, 90.0f));

	TestTrue(TEXT("戰鬥中點會平均雙方的世界位置"), Midpoint.Equals(FVector(80.0f, 10.0f, 50.0f)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFightingCharacterStableOpponentDirectionTest,
	"TndaFighter.Fighting.Character.StableOpponentDirection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFightingCharacterStableOpponentDirectionTest::RunTest(const FString& Parameters)
{
	FVector LastValidDirection = FVector::ZeroVector;
	TestFalse(
		TEXT("雙方水平位置完全相同且沒有歷史方向時無法分類"),
		AFightingCharacter::UpdateHorizontalOpponentDirection(
			FVector(25.0f, -40.0f, 100.0f),
			FVector(25.0f, -40.0f, -100.0f),
			LastValidDirection));
	TestTrue(TEXT("零水平距離不會建立任意方向"), LastValidDirection.IsNearlyZero());

	TestFalse(
		TEXT("尚無歷史方向且水平距離等於 10 cm 時無法分類"),
		AFightingCharacter::UpdateHorizontalOpponentDirection(
			FVector::ZeroVector,
			FVector(10.0f, 0.0f, 300.0f),
			LastValidDirection));
	TestTrue(TEXT("無效距離不會建立任意方向"), LastValidDirection.IsNearlyZero());

	TestTrue(
		TEXT("水平距離大於 10 cm 時建立正規化對手方向"),
		AFightingCharacter::UpdateHorizontalOpponentDirection(
			FVector::ZeroVector,
			FVector(0.0f, 20.0f, 300.0f),
			LastValidDirection));
	TestTrue(TEXT("對手方向只保留水平分量"), LastValidDirection.Equals(FVector::YAxisVector));

	TestTrue(
		TEXT("接近重疊時沿用最後有效方向"),
		AFightingCharacter::UpdateHorizontalOpponentDirection(
			FVector::ZeroVector,
			FVector(0.0f, -5.0f, -300.0f),
			LastValidDirection));
	TestTrue(TEXT("接近重疊不會讓方向翻轉"), LastValidDirection.Equals(FVector::YAxisVector));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFightingCharacterRelativeDirectionTest,
	"TndaFighter.Fighting.Character.RelativeDirection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFightingCharacterRelativeDirectionTest::RunTest(const FString& Parameters)
{
	const FVector OpponentToScreenRight = FVector::YAxisVector;
	TestEqual(
		TEXT("畫面向右且朝向對手時分類為 6"),
		AFightingCharacter::ClassifyRelativeDirection(FVector::YAxisVector, OpponentToScreenRight),
		ERelativeDirection::Forward6);
	TestEqual(
		TEXT("畫面向左且背離對手時分類為 4"),
		AFightingCharacter::ClassifyRelativeDirection(-FVector::YAxisVector, OpponentToScreenRight),
		ERelativeDirection::Backward4);
	TestEqual(
		TEXT("純畫面縱深移動分類為 Neutral"),
		AFightingCharacter::ClassifyRelativeDirection(FVector::XAxisVector, OpponentToScreenRight),
		ERelativeDirection::Neutral);
	TestEqual(
		TEXT("反向站位時畫面向左且朝向對手分類為 6"),
		AFightingCharacter::ClassifyRelativeDirection(-FVector::YAxisVector, -OpponentToScreenRight),
		ERelativeDirection::Forward6);
	TestEqual(
		TEXT("反向站位時畫面向右且背離對手分類為 4"),
		AFightingCharacter::ClassifyRelativeDirection(FVector::YAxisVector, -OpponentToScreenRight),
		ERelativeDirection::Backward4);
	TestEqual(
		TEXT("反向站位的純畫面縱深移動仍為 Neutral"),
		AFightingCharacter::ClassifyRelativeDirection(-FVector::XAxisVector, -OpponentToScreenRight),
		ERelativeDirection::Neutral);
	TestEqual(
		TEXT("分類前會移除 Z 並正規化水平向量"),
		AFightingCharacter::ClassifyRelativeDirection(
			FVector(0.0f, 100.0f, 900.0f),
			FVector(0.0f, 5.0f, -900.0f)),
		ERelativeDirection::Forward6);

	const FVector ExactPositiveThreshold(0.1, FMath::Sqrt(0.99), 0.0);
	const FVector ExactNegativeThreshold(-0.1, FMath::Sqrt(0.99), 0.0);
	const FVector OutsidePositiveThreshold(0.11, FMath::Sqrt(1.0 - FMath::Square(0.11)), 0.0);
	const FVector InsidePositiveThreshold(0.09, FMath::Sqrt(1.0 - FMath::Square(0.09)), 0.0);
	const FVector OutsideNegativeThreshold(-0.11, FMath::Sqrt(1.0 - FMath::Square(0.11)), 0.0);
	const FVector InsideNegativeThreshold(-0.09, FMath::Sqrt(1.0 - FMath::Square(0.09)), 0.0);
	TestEqual(
		TEXT("Dot Product 等於 0.1 時仍為 Neutral"),
		AFightingCharacter::ClassifyRelativeDirection(ExactPositiveThreshold, FVector::XAxisVector),
		ERelativeDirection::Neutral);
	TestEqual(
		TEXT("Dot Product 等於 -0.1 時仍為 Neutral"),
		AFightingCharacter::ClassifyRelativeDirection(ExactNegativeThreshold, FVector::XAxisVector),
		ERelativeDirection::Neutral);
	TestEqual(
		TEXT("斜向投影剛越過正門檻時為 6"),
		AFightingCharacter::ClassifyRelativeDirection(OutsidePositiveThreshold, FVector::XAxisVector),
		ERelativeDirection::Forward6);
	TestEqual(
		TEXT("斜向投影位於正門檻內時為 Neutral"),
		AFightingCharacter::ClassifyRelativeDirection(InsidePositiveThreshold, FVector::XAxisVector),
		ERelativeDirection::Neutral);
	TestEqual(
		TEXT("斜向投影剛越過負門檻時為 4"),
		AFightingCharacter::ClassifyRelativeDirection(OutsideNegativeThreshold, FVector::XAxisVector),
		ERelativeDirection::Backward4);
	TestEqual(
		TEXT("斜向投影位於負門檻內時為 Neutral"),
		AFightingCharacter::ClassifyRelativeDirection(InsideNegativeThreshold, FVector::XAxisVector),
		ERelativeDirection::Neutral);
	TestEqual(
		TEXT("無移動方向時為 Neutral"),
		AFightingCharacter::ClassifyRelativeDirection(FVector::ZeroVector, OpponentToScreenRight),
		ERelativeDirection::Neutral);
	return true;
}

#endif
