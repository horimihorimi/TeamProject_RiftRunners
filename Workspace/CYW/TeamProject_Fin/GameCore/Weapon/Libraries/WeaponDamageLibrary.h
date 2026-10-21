#pragma once

#include "CoreMinimal.h"
#include "../Data/WeaponTypes.h"

class UAbilitySystemComponent;
class UGameplayEffect;
class UWorld;

struct TEAMPROJECT_FIN_API FWeaponPointDamageRequest
{
	FHitResult Hit;
	AActor* InstigatorActor = nullptr;
	UAbilitySystemComponent* SourceASC = nullptr;
	TSubclassOf<UGameplayEffect> DamageEffectClass;
	float DamageAmount = 0.0f;
	float WeakpointMultiplier = 1.0f;
	float AbilityLevel = 1.0f;
	bool bDamageInstigator = false;
};

struct TEAMPROJECT_FIN_API FWeaponRadialDamageRequest
{
	AActor* EffectCauser = nullptr;
	UWorld* World = nullptr;
	FVector Origin = FVector::ZeroVector;
	float Radius = 0.0f;
	AActor* InstigatorActor = nullptr;
	UAbilitySystemComponent* SourceASC = nullptr;
	TSubclassOf<UGameplayEffect> DamageEffectClass;
	float DamageAmount = 0.0f;
	float AbilityLevel = 1.0f;
	TArray<AActor*> IgnoredActors;
	bool bHostileTargetsOnly = false;
	bool bDamageInstigator = false;
};

namespace WeaponDamageLibrary
{
	TEAMPROJECT_FIN_API float CalculateDamageAtDistance(
		float BaseDamage,
		float Distance,
		const FWeaponDamageFalloffStats& Falloff,
		EWeaponCategory WeaponCategory);

	TEAMPROJECT_FIN_API void ApplyGasDamageToHit(
		const FWeaponPointDamageRequest& Request);

	TEAMPROJECT_FIN_API void ApplyGasDamageToHit(
		const FHitResult& Hit,
		AActor* InstigatorActor,
		UAbilitySystemComponent* SourceASC,
		TSubclassOf<UGameplayEffect> DamageEffectClass,
		float DamageAmount,
		float WeakpointMultiplier,
		float AbilityLevel,
		bool bDamageInstigator = false);

	TEAMPROJECT_FIN_API void ApplyGasRadialDamage(
		const FWeaponRadialDamageRequest& Request);

	TEAMPROJECT_FIN_API void ApplyGasRadialDamage(
		UWorld* World,
		const FVector& Origin,
		float Radius,
		AActor* InstigatorActor,
		UAbilitySystemComponent* SourceASC,
		TSubclassOf<UGameplayEffect> DamageEffectClass,
		float DamageAmount,
		float AbilityLevel,
		const TArray<AActor*>& IgnoredActors,
		bool bHostileTargetsOnly = false,
		bool bDamageInstigator = false);
}
