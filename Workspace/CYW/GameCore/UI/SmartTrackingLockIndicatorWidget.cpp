#include "GameCore/UI/SmartTrackingLockIndicatorWidget.h"

#include "GameCore/UI/SmartTrackingVisualComponent.h"

void USmartTrackingLockIndicatorWidget::SetVisualOwner(USmartTrackingVisualComponent* InVisualOwner)
{
	VisualOwner = InVisualOwner;
}

void USmartTrackingLockIndicatorWidget::InitializeLockIndicator(AActor* InTargetActor)
{
	TrackedTargetActor = InTargetActor;
	OnLockAcquired(InTargetActor);
}

void USmartTrackingLockIndicatorWidget::PlayLockLost()
{
	OnLockLost(TrackedTargetActor.Get());
}

void USmartTrackingLockIndicatorWidget::NotifyLockAcquireAnimationFinished()
{
	// Kept for existing widget Blueprints; gameplay timing is independent of animation.
}

void USmartTrackingLockIndicatorWidget::NotifyLockLostAnimationFinished()
{
	if (USmartTrackingVisualComponent* OwnerComponent = VisualOwner.Get())
	{
		OwnerComponent->NotifyLockLostAnimationFinished(this);
	}
}

void USmartTrackingLockIndicatorWidget::SetLockedBulletCount(int32 NewLockedBulletCount)
{
	LockedBulletCount = FMath::Max(0, NewLockedBulletCount);
	OnLockedBulletCountChanged(LockedBulletCount);
}
