#pragma once
#include "CoreMinimal.h"
#include "Engine/HitResult.h"

class UAbilitySystemComponent;
class UGameplayEffect;

// One synchronous discharge. GA owns activation, hit counts and effect lifetime.
struct FOverdriveDischarge
{
	UWorld* World = nullptr;
	UAbilitySystemComponent* SourceAbilitySystem = nullptr;
	AActor* OwnerActor = nullptr;
	float AbilityLevel = 1.0f;
	float ExplosionRadius = 0.0f;
	float ExplosionDamage = 0.0f;
	TSubclassOf<UGameplayEffect> ExplosionDamageEffectClass;
	float ChainSearchRadius = 0.0f;
	float ChainMaxDistance = 0.0f;
	int32 MaxChainTargets = 0;
	float ChainDamage = 0.0f;
	TSubclassOf<UGameplayEffect> ChainDamageEffectClass;
	bool bRequireChainLineOfSight = false;
	FName ChainTargetSocketName = NAME_None;
	bool bShowDebug = false;
	void Execute(AActor* PrimaryTarget, const FHitResult& HitResult);
private:
	void TriggerChainLightning(AActor* PrimaryTarget, const FVector& Origin,
		UAbilitySystemComponent* SourceASC, AActor* AvatarActor);
	void ExecuteExplosionCue(UAbilitySystemComponent* ASC, AActor* AvatarActor, const FVector& Origin) const;
	void ExecuteChainCue(UAbilitySystemComponent* ASC, AActor* AvatarActor,
		AActor* StartTarget, AActor* ChainTarget, const FVector& Start,
		const FVector& End, bool bPlaySound) const;
};
