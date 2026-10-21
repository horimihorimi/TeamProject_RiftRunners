
#pragma once

#include "CoreMinimal.h"
#include "GameCore/Ability/GA_WeaponAbilityBase.h"
#include "GameCore/Weapon/Data/SmartTrackingTypes.h"
#include "GA_SmartTracking.generated.h"

class USmartTrackingTargetingComponent;

UCLASS()
class TEAMPROJECT_FIN_API UGA_SmartTracking : public UGA_WeaponAbilityBase
{
	GENERATED_BODY()

public:
	UGA_SmartTracking();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;
	virtual void InputReleased(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;

	UPROPERTY(EditDefaultsOnly, Category = "Smart Tracking")
	TSubclassOf<class UGameplayEffect> SmartTrackingEffectClass;


private:
	void StartTrackingScan();
	void StopTrackingScan(const FGameplayAbilityActorInfo* ActorInfo);
	void BindTargetingDelegates();
	void UnbindTargetingDelegates();
	void OnTargetLocked(AActor* TargetActor);
	void OnTargetLost(AActor* TargetActor);
	void BroadcastTargetEvent(AActor* TargetActor, const FGameplayTag& EventTag);
	void BroadcastTargetStackChanged(AActor* TargetActor, int32 StackCount);
	void BroadcastLockStateChanged(ESmartTrackingLockState LockState);
	void RequestSmartTrackingFire();
	void BroadcastSmartTrackingEvent(
		const FGameplayTag& EventTag,
		AActor* TargetActor,
		float EventMagnitude = 0.0f,
		const FGameplayAbilityActorInfo* ActorInfo = nullptr);

	FActiveGameplayEffectHandle ActiveSmartTrackingEffectHandle;
	UPROPERTY(Transient)
	// Owned by UWeaponComponent; this ability controls only its active targeting state.
	TObjectPtr<USmartTrackingTargetingComponent> TargetingComponent;
	TOptional<ESmartTrackingLockState> LastBroadcastLockState;
};
