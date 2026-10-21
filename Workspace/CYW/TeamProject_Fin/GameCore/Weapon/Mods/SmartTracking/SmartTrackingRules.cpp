#include "SmartTrackingRules.h"
#include "GameCore/Ability/CombatTargetLibrary.h"
#include "GameCore/Weapon/Core/WeaponBase.h"
#include "Engine/World.h"
#include "SmartTrackingPathLibrary.h"

float SmartTrackingRules::GetStackInterval(float FireInterval, const FSmartTrackingTargetingSettings& Settings)
{
	return FMath::Max(FireInterval * FMath::Max(Settings.LockStackIntervalMultiplier, 0.01f), 0.01f);
}

int32 SmartTrackingRules::GetStackCount(double ElapsedTime, float PreparationTime, float StackInterval)
{
	const double StackTime = ElapsedTime - FMath::Max(PreparationTime, 0.0f);
	if (!FMath::IsFinite(StackTime) || StackTime < 0.0) return 0;
	return static_cast<int32>(FMath::Min(FMath::FloorToDouble(StackTime / FMath::Max(StackInterval, 0.01f)) + 1.0, static_cast<double>(MAX_int32)));
}

int32 SmartTrackingRules::GetMaxBulletCount(float Health, float Damage)
{
	return Damage > KINDA_SMALL_NUMBER ? FMath::Max(0, FMath::CeilToInt(Health / Damage)) : 0;
}

bool SmartTrackingRules::IsValidTarget(AActor* Target, AActor* Instigator, UAbilitySystemComponent* SourceASC, const AWeaponBase* Weapon)
{
	if (!Weapon || !Weapon->GetWeaponData() || !Instigator || !SourceASC
		|| !CombatTargetLibrary::IsHostileTarget(Target, SourceASC, Instigator)) return false;
	const FSmartTrackingTargetingSettings& Settings = Weapon->GetWeaponData()->SmartTrackingTargeting;
	const FVector TargetPoint = SmartTrackingPathLibrary::GetTargetPoint(Target);
	const FVector Start = Weapon->GetFireTransform(Instigator).GetLocation();
	const FVector ToTarget = TargetPoint - Start;
	if (ToTarget.SizeSquared() > FMath::Square(FMath::Max(Settings.Range, 0.0f))) return false;
	const float AimDot = FVector::DotProduct(Weapon->GetAimDirection(Instigator, SourceASC).GetSafeNormal(), ToTarget.GetSafeNormal());
	if (!ToTarget.IsNearlyZero() && AimDot < FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(Settings.ConeHalfAngleDegrees, 0.0f, 180.0f)))) return false;
	if (Settings.bRequireLineOfSight)
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(SmartTrackingSight), false, Instigator);
		Params.AddIgnoredActor(Weapon);
		FHitResult Hit;
		if (Weapon->GetWorld()->LineTraceSingleByChannel(Hit, Start, TargetPoint, ECC_Visibility, Params) && Hit.GetActor() != Target) return false;
	}
	return true;
}
