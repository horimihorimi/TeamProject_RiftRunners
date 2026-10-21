#include "GameCore/UI/BossShoutOverlayWidget.h"

#include "Components/Image.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameCore/Tags/GameplayTagSystem.h"
#include "Materials/MaterialInstanceDynamic.h"

void UBossShoutOverlayWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ShoutOverlayImage)
	{
		ShoutMID = ShoutOverlayImage->GetDynamicMaterial();
	}

	bShoutActive = false;
	CurrentIntensity = 0.0f;
	SetMaterialIntensity(CurrentIntensity);
	TryBindToStaggerTag();
}

void UBossShoutOverlayWidget::NativeDestruct()
{
	if (UAbilitySystemComponent* ASC = BoundAbilitySystemComponent.Get())
	{
		ASC->RegisterGameplayTagEvent(
			GameplayTagSystem::State_Debuff_Staggered,
			EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
	}
	BoundAbilitySystemComponent = nullptr;

	Super::NativeDestruct();
}

void UBossShoutOverlayWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	TryBindToStaggerTag();

	const float TargetIntensity = bShoutActive ? ShoutIntensity : 0.0f;
	const float InterpSpeed = bShoutActive ? FadeInSpeed : FadeOutSpeed;
	const float NewIntensity = FMath::FInterpTo(CurrentIntensity, TargetIntensity, InDeltaTime, InterpSpeed);

	if (!FMath::IsNearlyEqual(CurrentIntensity, NewIntensity))
	{
		CurrentIntensity = NewIntensity;
		SetMaterialIntensity(CurrentIntensity);
	}
}

void UBossShoutOverlayWidget::TryBindToStaggerTag()
{
	if (BoundAbilitySystemComponent.IsValid())
	{
		return;
	}

	APawn* Pawn = GetOwningPlayer() ? GetOwningPlayer()->GetPawn() : nullptr;
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
	if (!IsValid(ASC))
	{
		return;
	}

	BoundAbilitySystemComponent = ASC;
	ASC->RegisterGameplayTagEvent(
		GameplayTagSystem::State_Debuff_Staggered,
		EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ThisClass::OnStaggeredTagChanged);

	OnStaggeredTagChanged(
		GameplayTagSystem::State_Debuff_Staggered,
		ASC->GetTagCount(GameplayTagSystem::State_Debuff_Staggered));
}

void UBossShoutOverlayWidget::OnStaggeredTagChanged(const FGameplayTag Tag, const int32 NewCount)
{
	if (NewCount > 0)
	{
		ActivateShout();
	}
	else
	{
		DeactivateShout();
	}
}

void UBossShoutOverlayWidget::ActivateShout()
{
	bShoutActive = true;
}

void UBossShoutOverlayWidget::DeactivateShout()
{
	bShoutActive = false;
}

void UBossShoutOverlayWidget::SetMaterialIntensity(const float NewIntensity)
{
	if (ShoutMID)
	{
		ShoutMID->SetScalarParameterValue(TEXT("Intensity"), NewIntensity);
	}
}
