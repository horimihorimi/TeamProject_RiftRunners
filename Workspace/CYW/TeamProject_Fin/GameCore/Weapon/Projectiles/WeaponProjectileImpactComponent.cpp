#include "WeaponProjectileImpactComponent.h"

#include "WeaponProjectile.h"
#include "../Core/WeaponBase.h"
#include "../Libraries/WeaponCueLibrary.h"
#include "../Libraries/WeaponDamageLibrary.h"
#include "../Libraries/WeaponFireLibrary.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"

UWeaponProjectileImpactComponent::UWeaponProjectileImpactComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWeaponProjectileImpactComponent::Reset()
{
	bHasHit = false;
	CurrentBounceCount = 0;
}

void UWeaponProjectileImpactComponent::HandleHit(const FHitResult& Hit, const FWeaponProjectileImpactContext& Context)
{
	if (!Context.Projectile->HasAuthority() ||
		bHasHit ||
		!Context.SourceWeapon)
	{
		return;
	}

	if (!Context.bProjectileExplodeOnImpact)
	{
		return;
	}

	const bool bCharacterHit =
		WeaponFireLibrary::IsCharacterHit(Hit);

	if (Context.bProjectileShouldBounce &&
		!bCharacterHit)
	{
		return;
	}

	Detonate(Hit, bCharacterHit, Context);
}

void UWeaponProjectileImpactComponent::HandleBounce(const FHitResult& ImpactResult, const FWeaponProjectileImpactContext& Context)
{
	if (!Context.Projectile->HasAuthority() ||
		bHasHit ||
		!Context.SourceWeapon ||
		!Context.bProjectileExplodeOnImpact)
	{
		return;
	}

	if (WeaponFireLibrary::IsCharacterHit(
		ImpactResult))
	{
		Detonate(ImpactResult, true, Context);

		return;
	}

	if (CurrentBounceCount == 0
		&& Context.SourceWeapon->GetWeaponCategory() == EWeaponCategory::GrenadeLauncher
		&& Context.InstigatorActor
		&& Context.CollisionComponent)
	{
		Context.CollisionComponent->IgnoreActorWhenMoving(
			Context.InstigatorActor,
			false);
	}

	++CurrentBounceCount;

	if (CurrentBounceCount >=
		Context.MaxProjectileBounces)
	{
		Detonate(ImpactResult, false, Context);

		return;
	}

	WeaponCueLibrary::ExecuteBounceCue(
		Context.SourceWeapon,
		Context.InstigatorActor,
		Context.SourceASC,
		ImpactResult,
		Context.FeedbackOverride);

	if (Context.ProjectileMovement)
	{
		Context.ProjectileMovement->Velocity *=
			Context.ProjectileBounceSpeedMultiplier;
	}
}

void UWeaponProjectileImpactComponent::Detonate(const FHitResult& Hit, bool bCharacterHit, const FWeaponProjectileImpactContext& Context)
{
	if (bHasHit)
	{
		return;
	}

	if (!Context.SourceWeapon)
	{
		Context.Projectile->DeactivateToPool();
		return;
	}

	bHasHit = true;

	const float DamageAtDistance =
		WeaponDamageLibrary::CalculateDamageAtDistance(
			Context.RuntimeStats->Damage,
			FVector::Distance(
				Context.DamageOrigin,
				Hit.ImpactPoint),
			Context.RuntimeStats->DamageFalloff,
			Context.SourceWeapon->GetWeaponCategory());

	if (Context.RuntimeStats->ExplosionRadius > 0.0f)
	{
		FWeaponRadialDamageRequest DamageRequest;

		DamageRequest.World =
			GetWorld();

		DamageRequest.Origin =
			Hit.ImpactPoint;

		DamageRequest.Radius =
			Context.RuntimeStats->ExplosionRadius;

		DamageRequest.InstigatorActor =
			Context.InstigatorActor;

		DamageRequest.SourceASC =
			Context.SourceASC;

		DamageRequest.DamageEffectClass =
			Context.DamageEffectClass;

		DamageRequest.DamageAmount =
			DamageAtDistance;

		DamageRequest.AbilityLevel =
			Context.AbilityLevel;

		DamageRequest.IgnoredActors.Add(
			Context.SourceWeapon);

		DamageRequest.bDamageInstigator =
			true;

		WeaponDamageLibrary::ApplyGasRadialDamage(
			DamageRequest);
	}
	else
	{
		FWeaponPointDamageRequest DamageRequest;

		DamageRequest.Hit =
			Hit;

		DamageRequest.InstigatorActor =
			Context.InstigatorActor;

		DamageRequest.SourceASC =
			Context.SourceASC;

		DamageRequest.DamageEffectClass =
			Context.DamageEffectClass;

		DamageRequest.DamageAmount =
			DamageAtDistance;

		DamageRequest.WeakpointMultiplier =
			Context.RuntimeStats->WeakpointMultiplier;

		DamageRequest.AbilityLevel =
			Context.AbilityLevel;

		DamageRequest.bDamageInstigator =
			true;

		WeaponDamageLibrary::ApplyGasDamageToHit(
			DamageRequest);
	}
	WeaponCueLibrary::ExecuteImpactCue(
		Context.SourceWeapon,
		Context.InstigatorActor,
		Context.SourceASC,
		Hit,
		bCharacterHit,
		Context.FeedbackOverride,
		Context.RuntimeStats->ExplosionRadius);
	Context.Projectile->DeactivateToPool();
}
