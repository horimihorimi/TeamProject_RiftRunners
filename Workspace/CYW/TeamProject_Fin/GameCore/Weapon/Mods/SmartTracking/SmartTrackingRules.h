#pragma once

#include "CoreMinimal.h"
#include "GameCore/Weapon/Data/SmartTrackingTypes.h"

class AActor;
class AWeaponBase;
class UAbilitySystemComponent;

namespace SmartTrackingRules
{
	float GetStackInterval(float FireInterval, const FSmartTrackingTargetingSettings& Settings);
	int32 GetStackCount(double ElapsedTime, float PreparationTime, float StackInterval);
	int32 GetMaxBulletCount(float Health, float Damage);
	bool IsValidTarget(AActor* Target, AActor* Instigator, UAbilitySystemComponent* SourceASC, const AWeaponBase* Weapon);
}
