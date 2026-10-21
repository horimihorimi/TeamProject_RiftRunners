#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "../Weapon/Data/WeaponTypes.h"
#include "GA_WeaponAbilityBase.generated.h"

class UAbilitySystemComponent;
class AWeaponBase;
class UWeaponComponent;

UCLASS(Abstract)
class TEAMPROJECT_FIN_API UGA_WeaponAbilityBase : public UGameplayAbility
{
	GENERATED_BODY()

protected:
	UGA_WeaponAbilityBase();

	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	AActor* GetAvatarActor(const FGameplayAbilityActorInfo* ActorInfo) const;
	UAbilitySystemComponent* GetAbilitySystemComponentFromInfo(const FGameplayAbilityActorInfo* ActorInfo) const;
	UWeaponComponent* FindAvatarWeaponComponent(const FGameplayAbilityActorInfo* ActorInfo) const;
	AWeaponBase* GetEquippedWeapon(const FGameplayAbilityActorInfo* ActorInfo) const;
	bool HasRequiredWeaponModifier(const FGameplayAbilityActorInfo* ActorInfo) const;
	void BroadcastSelfGameplayEvent(UAbilitySystemComponent* ASC, AActor* AvatarActor, const FGameplayTag& EventTag) const;
	void FinishAbility(bool bWasCancelled);
	virtual const FGameplayTagContainer* GetCooldownTags() const override;
	virtual void ApplyCooldown(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;
	void ClearModCooldown();
	FGameplayTagContainer ModCooldownTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Mod", meta = (ClampMin = "0.0", Units = "s"))
	float InternalCooldown = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Ability")
	EWeaponModiferType RequiredWeaponModifier = EWeaponModiferType::None;
private:
	mutable FActiveGameplayEffectHandle ModCooldownHandle;
};
