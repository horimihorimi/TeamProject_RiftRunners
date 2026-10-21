#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SmartTrackingLockIndicatorWidget.generated.h"

UCLASS()
class TEAMPROJECT_FIN_API USmartTrackingLockIndicatorWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetLockPreparationTime(float Seconds) { LockPreparationTime = FMath::Max(Seconds, 0.0f); }
	void SetVisualOwner(class USmartTrackingVisualComponent* InVisualOwner);

	UFUNCTION(BlueprintCallable, Category = "Smart Tracking")
	void InitializeLockIndicator(AActor* InTargetActor);

	UFUNCTION(BlueprintCallable, Category = "Smart Tracking")
	void PlayLockLost();

	UFUNCTION(BlueprintCallable, Category = "Smart Tracking")
	void NotifyLockAcquireAnimationFinished();

	UFUNCTION(BlueprintCallable, Category = "Smart Tracking")
	void NotifyLockLostAnimationFinished();

	UFUNCTION(BlueprintCallable, Category = "Smart Tracking")
	void SetLockedBulletCount(int32 NewLockedBulletCount);

	UFUNCTION(BlueprintPure, Category = "Smart Tracking")
	AActor* GetTargetActor() const { return TrackedTargetActor.Get(); }

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Smart Tracking")
	float LockPreparationTime = 0.0f;

	UFUNCTION(BlueprintImplementableEvent, Category = "Smart Tracking")
	void OnLockAcquired(AActor* LockedTargetActor);

	UFUNCTION(BlueprintImplementableEvent, Category = "Smart Tracking")
	void OnLockLost(AActor* LostTargetActor);

	UFUNCTION(BlueprintImplementableEvent, Category = "Smart Tracking")
	void OnLockedBulletCountChanged(int32 NewLockedBulletCount);

private:
	TWeakObjectPtr<AActor> TrackedTargetActor;
	TWeakObjectPtr<class USmartTrackingVisualComponent> VisualOwner;
	int32 LockedBulletCount = 0;
};
