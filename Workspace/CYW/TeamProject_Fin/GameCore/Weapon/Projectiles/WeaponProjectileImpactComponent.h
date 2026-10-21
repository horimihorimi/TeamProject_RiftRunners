#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WeaponProjectileImpactComponent.generated.h"

class AActor;
class AWeaponBase;
class AWeaponProjectile;
class UAbilitySystemComponent;
class UGameplayEffect;
class UProjectileMovementComponent;
class USphereComponent;
class UWeaponDataAsset;
struct FWeaponRuntimeStats;

// Borrowed inputs for a synchronous impact; never retained by the component.
struct FWeaponProjectileImpactContext
{
	AWeaponProjectile* Projectile = nullptr;
	AWeaponBase* SourceWeapon = nullptr;
	AActor* InstigatorActor = nullptr;
	UAbilitySystemComponent* SourceASC = nullptr;
	TSubclassOf<UGameplayEffect> DamageEffectClass;
	UWeaponDataAsset* FeedbackOverride = nullptr;
	USphereComponent* CollisionComponent = nullptr;
	UProjectileMovementComponent* ProjectileMovement = nullptr;
	const FWeaponRuntimeStats* RuntimeStats = nullptr;
	FVector DamageOrigin = FVector::ZeroVector;
	float AbilityLevel = 1.0f;
	bool bProjectileShouldBounce = false;
	bool bProjectileExplodeOnImpact = true;
	int32 MaxProjectileBounces = 3;
	float ProjectileBounceSpeedMultiplier = 0.75f;
};

UCLASS(ClassGroup = (Weapon))
class TEAMPROJECT_FIN_API UWeaponProjectileImpactComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWeaponProjectileImpactComponent();
	void Reset();
	void ResetBounceCount() { CurrentBounceCount = 0; }
	bool HasHit() const { return bHasHit; }
	void HandleHit(const FHitResult& Hit, const FWeaponProjectileImpactContext& Context);
	void HandleBounce(const FHitResult& ImpactResult, const FWeaponProjectileImpactContext& Context);

private:
	void Detonate(const FHitResult& Hit, bool bCharacterHit, const FWeaponProjectileImpactContext& Context);
	int32 CurrentBounceCount = 0;
	bool bHasHit = false;
};
