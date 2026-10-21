// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameCore/UI/HealthBarFrostVisualWidget.h"

#include "Components/Image.h"
#include "GameCore/Attribute/CharacterAttributeSet.h"
#include "GameCore/Character/BaseCharacter.h"

namespace HealthBarFrostVisual
{
	constexpr float MaxFrozenPercent = 0.25f;
}

void UHealthBarFrostVisualWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 플레이어 참조가 준비되기 전에도 정상 프레임이 보이도록 기본 상태를 먼저 적용합니다.
	UpdateFrostVisuals(1.0f);

	OwningCharacter = Cast<ABaseCharacter>(GetOwningPlayerPawn());
	if (!OwningCharacter.IsValid())
	{
		return;
	}

	OwningCharacter->OnHealthCapMultiplierChanged.AddDynamic(
		this,
		&UHealthBarFrostVisualWidget::HandleHealthCapMultiplierChanged);

	if (const UCharacterAttributeSet* AttributeSet = OwningCharacter->GetCharacterAttributeSet())
	{
		UpdateFrostVisuals(AttributeSet->GetHealthCapMultiplier());
	}
}

void UHealthBarFrostVisualWidget::NativeDestruct()
{
	if (OwningCharacter.IsValid())
	{
		OwningCharacter->OnHealthCapMultiplierChanged.RemoveDynamic(
			this,
			&UHealthBarFrostVisualWidget::HandleHealthCapMultiplierChanged);
	}

	OwningCharacter.Reset();
	Super::NativeDestruct();
}

void UHealthBarFrostVisualWidget::HandleHealthCapMultiplierChanged(float NewMultiplier)
{
	UpdateFrostVisuals(NewMultiplier);
}

void UHealthBarFrostVisualWidget::UpdateFrostVisuals(float Multiplier)
{
	const float FrozenPercent = FMath::Clamp(
		1.0f - Multiplier,
		0.0f,
		HealthBarFrostVisual::MaxFrozenPercent);
	const float FrostProgress = FrozenPercent / HealthBarFrostVisual::MaxFrozenPercent;

	const int32 FrameIndex = FrozenPercent > 0.0f
		? FMath::Clamp(FMath::CeilToInt(FrostProgress * 4.0f), 1, 4)
		: 0;

	if (FrameImage
		&& FrameTextures.IsValidIndex(FrameIndex)
		&& LastFrameIndex != FrameIndex)
	{
		FrameImage->SetBrushFromTexture(FrameTextures[FrameIndex]);
		LastFrameIndex = FrameIndex;
	}
}
