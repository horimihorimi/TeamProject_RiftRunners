#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WeaponFireExecutorComponent.generated.h"

class AWeaponBase;
class UAbilitySystemComponent;
class UGameplayEffect;
class UProjectilePoolComponent;
class USplineComponent;
struct FWeaponFireContext;

// Executes shots; weapon state and queued-volley scheduling stay with their owners.
UCLASS(ClassGroup = (Weapon))
class TEAMPROJECT_FIN_API UWeaponFireExecutorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWeaponFireExecutorComponent();
	FVector GetAimDirection(AActor* InstigatorActor) const;
	FWeaponFireContext MakeFireContext(AActor* InstigatorActor, UAbilitySystemComponent* SourceASC,
		TSubclassOf<UGameplayEffect> DamageEffectClass, float AbilityLevel, bool bShowDebug) const;
	bool ExecuteFire(const FWeaponFireContext& FireContext, UProjectilePoolComponent* ProjectilePool);
	// Spawns only: the smart-tracking controller commits ammo and plays its cue.
	bool SpawnSmartTrackingProjectile(const FWeaponFireContext& FireContext, AActor* TargetActor,
		USplineComponent* SharedPath, UProjectilePoolComponent* ProjectilePool);

private:
	AWeaponBase* GetWeapon() const;
	FVector CalculateBaseFireDirection(AActor* InstigatorActor, const FTransform& FireTransform) const;
	float GetCurrentSpreadAngle(const UAbilitySystemComponent* SourceASC) const;
	static FVector ApplySpreadToDirection(const FVector& Direction, float SpreadAngle);
	bool HandleHitScanFire(const FWeaponFireContext& FireContext);
	bool HandleProjectileFire(const FWeaponFireContext& FireContext, UProjectilePoolComponent* ProjectilePool);
	void DrawFireDebug(const FVector& TraceStart, const FVector& TraceEnd, bool bHit, bool bShowDebug) const;
};
