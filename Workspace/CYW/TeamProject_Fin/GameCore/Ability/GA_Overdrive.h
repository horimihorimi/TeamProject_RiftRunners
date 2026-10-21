#pragma once

#include "CoreMinimal.h"
#include "GA_CharacterSkillBase.h"
#include "GA_Overdrive.generated.h"

class UAbilityTask_WaitGameplayEvent;
class UAbilitySystemComponent;
class UGameplayEffect;

UCLASS()
class TEAMPROJECT_FIN_API UGA_Overdrive : public UGA_CharacterSkillBase
{
	GENERATED_BODY()

public:
	UGA_Overdrive();

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Overdrive", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float Duration = 8.0f;

	/** Has Duration GE that grants State.Skill.Duration for HUD time queries. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Overdrive")
	TSubclassOf<UGameplayEffect> DurationEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Overdrive|Weapon", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float FireIntervalMultiplier = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Overdrive|Weapon", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float ReloadMontageRateMultiplier = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Overdrive|Electric Explosion", meta = (ClampMin = "1", UIMin = "1"))
	int32 HitsToExplosion = 10;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Overdrive|Electric Explosion", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ExplosionRadius = 500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Overdrive|Electric Explosion", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ExplosionDamage = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Overdrive|Electric Explosion")
	TSubclassOf<UGameplayEffect> ExplosionDamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Overdrive|Chain Lightning", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ChainSearchRadius = 1200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Overdrive|Chain Lightning", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ChainMaxDistance = 600.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Overdrive|Chain Lightning", meta = (ClampMin = "0", UIMin = "0"))
	int32 MaxChainTargets = 4;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Overdrive|Chain Lightning", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ChainDamage = 50.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Overdrive|Chain Lightning")
	TSubclassOf<UGameplayEffect> ChainDamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Overdrive|Chain Lightning")
	bool bRequireChainLineOfSight = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Overdrive|Chain Lightning")
	FName ChainTargetSocketName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Overdrive|Debug")
	bool bShowDebug = false;

private:
	void OnDurationEffectRemoved(const FGameplayEffectRemovalInfo& RemovalInfo);

	UFUNCTION()
	void OnOverdriveHit(FGameplayEventData Payload);

	void ExecuteActivationCue(UAbilitySystemComponent* ASC, AActor* AvatarActor) const;
	void AddWeaponCue(UAbilitySystemComponent* ASC, AActor* AvatarActor) const;
	void RemoveWeaponCue(UAbilitySystemComponent* ASC) const;
	bool ApplyDurationEffect(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo);
	void TriggerElectricExplosion(AActor* PrimaryTarget, const FHitResult& HitResult);
	AActor* GetAvatarActor(const FGameplayAbilityActorInfo* ActorInfo) const;

	FDelegateHandle DurationRemovedHandle;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> HitEventTask;

	FActiveGameplayEffectHandle ActiveTimingEffectHandle;
	FActiveGameplayEffectHandle ActiveDurationEffectHandle;

	TMap<TWeakObjectPtr<AActor>, int32> HitCountsByTarget;
};
