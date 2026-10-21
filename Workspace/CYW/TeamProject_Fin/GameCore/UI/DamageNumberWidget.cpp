#include "DamageNumberWidget.h"

#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "GameCore/UI/ScreenVisualFeedbackComponent.h"
#include "TimerManager.h"

void UDamageNumberWidget::InitializeDamageNumber(float InDamageAmount, const FVector2D& InScreenPosition, bool bInWeakpoint)
{
	StopAllAnimations();
	bIsInPool = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SafetyReleaseTimerHandle);
		World->GetTimerManager().SetTimer(
			SafetyReleaseTimerHandle,
			this,
			&UDamageNumberWidget::ReleaseToPool,
			SafetyLifetimeSeconds,
			false);
	}

	if (DamageText)
	{
		const int32 RoundedDamage = FMath::Max(1, FMath::RoundToInt(InDamageAmount));
		DamageText->SetText(FText::AsNumber(RoundedDamage));
	}

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
	{
		CanvasSlot->SetPosition(InScreenPosition);
	}

	OnDamageNumberInitialized(bInWeakpoint);
}

void UDamageNumberWidget::SetPoolOwner(UScreenVisualFeedbackComponent* InPoolOwner)
{
	PoolOwner = InPoolOwner;
}

void UDamageNumberWidget::ReleaseToPool()
{
	if (bIsInPool)
	{
		return;
	}

	bIsInPool = true;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SafetyReleaseTimerHandle);
	}
	StopAllAnimations();

	if (UScreenVisualFeedbackComponent* Presenter = PoolOwner.Get())
	{
		Presenter->ReturnDamageNumberToPool(this);
	}
	else
	{
		RemoveFromParent();
	}
}

void UDamageNumberWidget::OnAnimationFinished_Implementation(const UWidgetAnimation* Animation)
{
	Super::OnAnimationFinished_Implementation(Animation);
	ReleaseToPool();
}
