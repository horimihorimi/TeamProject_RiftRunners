#include "OverdriveDischarge.h"
#include "CombatTargetLibrary.h"
#include "DamageSystemLibrary.h"
#include "GameCore/Weapon/Libraries/WeaponDamageLibrary.h"
#include "GameCore/Tags/GameplayTagSystem.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"

void FOverdriveDischarge::Execute(AActor* PrimaryTarget, const FHitResult& HitResult)
{
	UAbilitySystemComponent* ASC = SourceAbilitySystem;
	AActor* AvatarActor = OwnerActor;

	if (!World || !ASC)
	{
		return;
	}

	const FVector Origin = HitResult.bBlockingHit
		? FVector(HitResult.ImpactPoint)
		: CombatTargetLibrary::GetTargetPoint(PrimaryTarget);
	ExecuteExplosionCue(ASC, AvatarActor, Origin);

	if (ExplosionDamageEffectClass && ExplosionRadius > 0.0f && ExplosionDamage > 0.0f)
	{
		FWeaponRadialDamageRequest DamageRequest;
		DamageRequest.World = World;
		DamageRequest.Origin = Origin;
		DamageRequest.Radius = ExplosionRadius;
		DamageRequest.InstigatorActor = AvatarActor;
		DamageRequest.SourceASC = ASC;
		DamageRequest.DamageEffectClass = ExplosionDamageEffectClass;
		DamageRequest.DamageAmount = ExplosionDamage;
		DamageRequest.AbilityLevel = AbilityLevel;
		DamageRequest.IgnoredActors.Add(AvatarActor);
		DamageRequest.bHostileTargetsOnly = true;
		WeaponDamageLibrary::ApplyGasRadialDamage(DamageRequest);
	}

	TriggerChainLightning(PrimaryTarget, Origin, ASC, AvatarActor);
}

void FOverdriveDischarge::TriggerChainLightning(
	AActor* PrimaryTarget,
	const FVector& Origin,
	UAbilitySystemComponent* SourceASC,
	AActor* AvatarActor)
{
	const TArray<AActor*> ChainTargets = CombatTargetLibrary::SelectRandomHostileTargets(
		World,
		Origin,
		PrimaryTarget,
		SourceASC,
		AvatarActor,
		ChainSearchRadius,
		ChainMaxDistance,
		FMath::Max(MaxChainTargets, 0),
		bRequireChainLineOfSight,
		ChainTargetSocketName
	);
	FVector ChainStart = Origin;
	AActor* ChainStartTarget = PrimaryTarget;
	for (int32 ChainIndex = 0; ChainIndex < ChainTargets.Num(); ++ChainIndex)
	{
		AActor* ChainTarget = ChainTargets[ChainIndex];
		if (!ChainTarget)
		{
			continue;
		}

		UAbilitySystemComponent* ChainTargetASC =
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(ChainTarget);
		const FVector ChainEnd = CombatTargetLibrary::GetTargetPoint(ChainTarget, ChainTargetSocketName);
		ExecuteChainCue(
			ChainTargetASC ? ChainTargetASC : SourceASC,
			AvatarActor,
			ChainStartTarget,
			ChainTarget,
			ChainStart,
			ChainEnd,
			ChainIndex == 0
		);

		if (ChainTargetASC && ChainDamageEffectClass && ChainDamage > 0.0f)
		{
			FDamagePayload DamagePayload;
			DamagePayload.BaseDamage = ChainDamage;
			DamagePayload.DamageEffectClass = ChainDamageEffectClass;
			DamagePayload.AbilityLevel = AbilityLevel;
			UDamageSystemLibrary::ApplyDamageToTarget(
				AvatarActor,
				SourceASC,
				ChainTarget,
				ChainTargetASC,
				DamagePayload
			);
		}

		ChainStart = ChainEnd;
		ChainStartTarget = ChainTarget;
	}
}

void FOverdriveDischarge::ExecuteExplosionCue(UAbilitySystemComponent* ASC, AActor* AvatarActor, const FVector& Origin) const
{
	if (!ASC)
	{
		return;
	}

	FGameplayCueParameters CueParameters;
	CueParameters.Instigator = AvatarActor;
	CueParameters.EffectCauser = AvatarActor;
	CueParameters.Location = Origin;
	CueParameters.RawMagnitude = FMath::Max(ExplosionRadius, 0.0f);
	if (bShowDebug)
	{
		CueParameters.AggregatedSourceTags.AddTag(GameplayTagSystem::CueParameter_ShowDebug);
	}
	ASC->ExecuteGameplayCue(GameplayTagSystem::GameplayCue_Skill_Overdrive_Explosion, CueParameters);
}

void FOverdriveDischarge::ExecuteChainCue(
	UAbilitySystemComponent* ASC,
	AActor* AvatarActor,
	AActor* StartTarget,
	AActor* ChainTarget,
	const FVector& Start,
	const FVector& End,
	bool bPlaySound) const
{
	if (!ASC)
	{
		return;
	}

	const FVector ChainOffset = End - Start;
	FGameplayCueParameters CueParameters;
	CueParameters.Instigator = AvatarActor;
	CueParameters.EffectCauser = StartTarget;
	CueParameters.TargetAttachComponent = ChainTarget ? ChainTarget->GetRootComponent() : nullptr;
	CueParameters.Location = Start;
	CueParameters.Normal = ChainOffset.GetSafeNormal();
	CueParameters.RawMagnitude = ChainOffset.Length();
	if (bShowDebug)
	{
		CueParameters.AggregatedSourceTags.AddTag(GameplayTagSystem::CueParameter_ShowDebug);
	}
	if (bPlaySound)
	{
		CueParameters.AggregatedSourceTags.AddTag(GameplayTagSystem::CueParameter_PlaySound);
	}
	ASC->ExecuteGameplayCue(GameplayTagSystem::GameplayCue_Skill_Overdrive_ChainLightning, CueParameters);
}

