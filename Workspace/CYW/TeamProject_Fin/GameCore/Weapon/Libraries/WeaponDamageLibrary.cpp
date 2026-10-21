#include "WeaponDamageLibrary.h"

#include "../../Ability/CombatTargetLibrary.h"
#include "../../Ability/DamageSystemLibrary.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "../../Tags/GameplayTagSystem.h"

namespace WeaponDamageLibrary
{
	float CalculateDamageAtDistance(
		float BaseDamage,
		float Distance,
		const FWeaponDamageFalloffStats& Falloff,
		EWeaponCategory WeaponCategory)
	{
		const float ClampedBaseDamage = FMath::Max(BaseDamage, 0.0f);
		if (WeaponCategory == EWeaponCategory::GrenadeLauncher)
		{
			return ClampedBaseDamage;
		}

		const float StartDistance = FMath::Max(Falloff.StartDistance, 0.0f);
		const float EndDistance = FMath::Max(Falloff.EndDistance, StartDistance);
		const float MinimumMultiplier = FMath::Clamp(Falloff.MinimumDamageMultiplier, 0.0f, 1.0f);
		const float ClampedDistance = FMath::Max(Distance, 0.0f);

		if (ClampedDistance <= StartDistance)
		{
			return ClampedBaseDamage;
		}

		if (EndDistance <= StartDistance || ClampedDistance >= EndDistance)
		{
			return ClampedBaseDamage * MinimumMultiplier;
		}

		const float FalloffAlpha = (ClampedDistance - StartDistance) / (EndDistance - StartDistance);
		return ClampedBaseDamage * FMath::Lerp(1.0f, MinimumMultiplier, FalloffAlpha);
	}

	void ApplyGasDamageToHit(
		const FWeaponPointDamageRequest& Request)
	{
		AActor* HitActor = Request.Hit.GetActor();
		if (!HitActor
			|| (!Request.bDamageInstigator && HitActor == Request.InstigatorActor)
			|| !Request.SourceASC
			|| !Request.DamageEffectClass)
		{
			return;
		}

		if (UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor))
		{
			const bool bWasAliveBeforeHit = !TargetASC->HasMatchingGameplayTag(GameplayTagSystem::State_Dead);

			FDamagePayload DamagePayload;
			DamagePayload.BaseDamage = Request.DamageAmount;
			DamagePayload.WeakpointMultiplier = Request.WeakpointMultiplier;
			DamagePayload.HitResult = Request.Hit;
			DamagePayload.DamageEffectClass = Request.DamageEffectClass;
			DamagePayload.AbilityLevel = Request.AbilityLevel;
			if (!UDamageSystemLibrary::ApplyDamageToTarget(
				Request.InstigatorActor,
				Request.SourceASC,
				HitActor,
				TargetASC,
				DamagePayload,
				&Request.Hit))
			{
				return;
			}

			FGameplayEventData EventData;
			EventData.EventTag = GameplayTagSystem::Event_Weapon_Hit;
			EventData.Instigator = Request.InstigatorActor;
			EventData.Target = HitActor;
			EventData.EventMagnitude = bWasAliveBeforeHit ? 1.0f : 0.0f;
			EventData.TargetData = UAbilitySystemBlueprintLibrary::AbilityTargetDataFromHitResult(Request.Hit);
			Request.SourceASC->HandleGameplayEvent(GameplayTagSystem::Event_Weapon_Hit, &EventData);
		}
	}

	void ApplyGasDamageToHit(
		const FHitResult& Hit,
		AActor* InstigatorActor,
		UAbilitySystemComponent* SourceASC,
		TSubclassOf<UGameplayEffect> DamageEffectClass,
		float DamageAmount,
		float WeakpointMultiplier,
		float AbilityLevel,
		bool bDamageInstigator)
	{
		FWeaponPointDamageRequest Request;
		Request.Hit = Hit;
		Request.InstigatorActor = InstigatorActor;
		Request.SourceASC = SourceASC;
		Request.DamageEffectClass = DamageEffectClass;
		Request.DamageAmount = DamageAmount;
		Request.WeakpointMultiplier = WeakpointMultiplier;
		Request.AbilityLevel = AbilityLevel;
		Request.bDamageInstigator = bDamageInstigator;
		ApplyGasDamageToHit(Request);
	}

	void ApplyGasRadialDamage(
		const FWeaponRadialDamageRequest& Request)
	{
		if (!Request.World || Request.Radius <= 0.0f || !Request.SourceASC || !Request.DamageEffectClass)
		{
			return;
		}

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WeaponRadialDamage), false, Request.InstigatorActor);
		for (AActor* IgnoredActor : Request.IgnoredActors)
		{
			if (IgnoredActor)
			{
				QueryParams.AddIgnoredActor(IgnoredActor);
			}
		}

		FCollisionObjectQueryParams ObjectQueryParams;
		ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

		TArray<FOverlapResult> OverlapResults;
		if (!Request.World->OverlapMultiByObjectType(
			OverlapResults,
			Request.Origin,
			FQuat::Identity,
			ObjectQueryParams,
			FCollisionShape::MakeSphere(Request.Radius),
			QueryParams))
		{
			return;
		}

		FDamagePayload DamagePayload;
		DamagePayload.BaseDamage = Request.DamageAmount;
		DamagePayload.DamageEffectClass = Request.DamageEffectClass;
		DamagePayload.AbilityLevel = Request.AbilityLevel;
		DamagePayload.EffectCauser = Request.EffectCauser;

		TSet<AActor*> DamagedActors;
		for (const FOverlapResult& OverlapResult : OverlapResults)
		{
			AActor* TargetActor = OverlapResult.GetActor();
			if (!TargetActor
				|| (!Request.bDamageInstigator && TargetActor == Request.InstigatorActor)
				|| DamagedActors.Contains(TargetActor))
			{
				continue;
			}

			UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
			if (!TargetASC)
			{
				continue;
			}
			if (TargetASC->HasMatchingGameplayTag(GameplayTagSystem::State_Dead)
				|| (Request.bHostileTargetsOnly && !CombatTargetLibrary::AreHostile(Request.SourceASC, TargetASC)))
			{
				continue;
			}

			if (UDamageSystemLibrary::ApplyDamageToTarget(
				Request.InstigatorActor,
				Request.SourceASC,
				TargetActor,
				TargetASC,
				DamagePayload))
			{
				DamagedActors.Add(TargetActor);
			}
		}
	}

	void ApplyGasRadialDamage(
		UWorld* World,
		const FVector& Origin,
		float Radius,
		AActor* InstigatorActor,
		UAbilitySystemComponent* SourceASC,
		TSubclassOf<UGameplayEffect> DamageEffectClass,
		float DamageAmount,
		float AbilityLevel,
		const TArray<AActor*>& IgnoredActors,
		bool bHostileTargetsOnly,
		bool bDamageInstigator)
	{
		FWeaponRadialDamageRequest Request;
		Request.World = World;
		Request.Origin = Origin;
		Request.Radius = Radius;
		Request.InstigatorActor = InstigatorActor;
		Request.SourceASC = SourceASC;
		Request.DamageEffectClass = DamageEffectClass;
		Request.DamageAmount = DamageAmount;
		Request.AbilityLevel = AbilityLevel;
		Request.IgnoredActors = IgnoredActors;
		Request.bHostileTargetsOnly = bHostileTargetsOnly;
		Request.bDamageInstigator = bDamageInstigator;
		ApplyGasRadialDamage(Request);
	}
}
