#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameCore/Weapon/Data/WeaponTypes.h"
#include "GameCore/Weapon/Data/SmartTrackingTypes.h"
#include "SmartTrackingTargetingComponent.generated.h"

class APlayerController;
class UAbilitySystemComponent;
class UWeaponComponent;

struct FSmartTrackingLockEntry
{
	TWeakObjectPtr<AActor> TargetActor;
	int32 LockedBulletCount = 0;
	int32 MaxBulletCount = 0;
	double LockStartTime = 0.0;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnSmartTrackingTargetEvent, AActor*);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSmartTrackingStackChanged, AActor*, int32);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnSmartTrackingLockStateChanged, ESmartTrackingLockState);

UCLASS(ClassGroup = (Weapon))
class TEAMPROJECT_FIN_API USmartTrackingTargetingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USmartTrackingTargetingComponent();

	void StartTargeting(
		APlayerController* InPlayerController,
		UAbilitySystemComponent* InSourceASC,
		UWeaponComponent* InWeaponComponent,
		const FSmartTrackingTargetingSettings& InSettings);
	void StopTargeting();

	const TArray<TWeakObjectPtr<AActor>>& GetTrackedTargets() const { return TrackedTargets; }
	bool ContainsTarget(const AActor* TargetActor) const;
	TArray<FSmartTrackingFireRequest> BuildFireRequests();

	FOnSmartTrackingTargetEvent OnTargetLocked;
	FOnSmartTrackingTargetEvent OnTargetLost;
	FOnSmartTrackingStackChanged OnStackChanged;
	FOnSmartTrackingLockStateChanged OnLockStateChanged;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void ScanTargets();
	void UpdateLockStacks();
	bool IsInsideReticleLockBox(const AActor* TargetActor) const;
	void UpdateTargetDelta(const TArray<TWeakObjectPtr<AActor>>& PreviousTargets);
	void UpdateLockedAmmoCount();
	int32 GetCurrentAmmo() const;
	float GetBulletDamage() const;
	float GetLockStackInterval() const;
	int32 GetTargetMaxLockCount(AActor* TargetActor) const;
	int32 GetTotalLockedAmmoCount() const;
	ESmartTrackingLockState GetLockState() const;
	void BroadcastAggregateState();

	TWeakObjectPtr<APlayerController> PlayerController;
	TWeakObjectPtr<UAbilitySystemComponent> SourceASC;
	TWeakObjectPtr<UWeaponComponent> WeaponComponent;
	FSmartTrackingTargetingSettings Settings;
	TArray<TWeakObjectPtr<AActor>> TrackedTargets;
	TArray<FSmartTrackingLockEntry> LockEntries;
	TOptional<ESmartTrackingLockState> LastLockState;
	FTimerHandle ScanTimerHandle;
	FTimerHandle LockStackTimerHandle;
};
