#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "WeaponPresentationComponent.generated.h"

class AWeaponBase;
class UAnimMontage;
class UWeaponComponent;
struct FGameplayEventData;

UCLASS(ClassGroup = (Weapon))
class TEAMPROJECT_FIN_API UWeaponPresentationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWeaponPresentationComponent();
	void Initialize(UWeaponComponent* InWeapons);
	void PlayEquipSound(AWeaponBase* EquippedWeapon);
	UAnimMontage* ResolveMontage(const FGameplayTag& CueTag) const;
	void PlayConfirmedCue(FGameplayTag CueTag, FGameplayCueParameters Parameters,
		UAnimMontage* Montage, AWeaponBase* EquippedWeapon);

private:
	void BindReloadSoundEvents();
	void BindReloadSoundEvent(const FGameplayTag& EventTag);
	void OnReloadSoundEvent(const FGameplayEventData* Payload);
	TWeakObjectPtr<UWeaponComponent> Weapons;
};
