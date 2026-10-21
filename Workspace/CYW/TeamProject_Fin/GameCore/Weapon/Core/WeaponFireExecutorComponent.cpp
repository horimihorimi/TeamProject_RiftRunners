#include "WeaponFireExecutorComponent.h"

#include "WeaponBase.h"
#include "../Libraries/WeaponCueLibrary.h"
#include "../Libraries/WeaponDamageLibrary.h"
#include "../Libraries/WeaponFireLibrary.h"
#include "../Projectiles/ProjectilePoolComponent.h"
#include "../Projectiles/WeaponProjectile.h"
#include "../../Tags/GameplayTagSystem.h"
#include "../../Combat/CombatThreatLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/SplineComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

UWeaponFireExecutorComponent::UWeaponFireExecutorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

AWeaponBase* UWeaponFireExecutorComponent::GetWeapon() const
{
	return CastChecked<AWeaponBase>(GetOwner());
}

FVector UWeaponFireExecutorComponent::GetAimDirection(
	AActor* InstigatorActor) const
{
	const FTransform FireTransform = GetWeapon()->GetFireTransform(InstigatorActor);
	return CalculateBaseFireDirection(InstigatorActor, FireTransform);
}

bool UWeaponFireExecutorComponent::ExecuteFire(const FWeaponFireContext& FireContext, UProjectilePoolComponent* ProjectilePool)
{
	switch (GetWeapon()->GetRuntimeStats().FireType)
	{
	case EWeaponFireType::Projectile:
		return HandleProjectileFire(FireContext, ProjectilePool);
	case EWeaponFireType::HitScan:
	default:
		return HandleHitScanFire(FireContext);
	}
}

float UWeaponFireExecutorComponent::GetCurrentSpreadAngle(const UAbilitySystemComponent* SourceASC) const
{
	const FWeaponRuntimeStats& RuntimeStats = GetWeapon()->GetRuntimeStats();
	const float SpreadMultiplier = (SourceASC && SourceASC->HasMatchingGameplayTag(GameplayTagSystem::State_Weapon_Aiming)) ? RuntimeStats.Spread.AimingMultiplier : 1.0f;
	return RuntimeStats.Spread.Angle * FMath::Max(SpreadMultiplier, 0.0f);
}

FVector UWeaponFireExecutorComponent::CalculateBaseFireDirection(AActor* InstigatorActor, const FTransform& FireTransform) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return FireTransform.GetRotation().GetForwardVector();
	}

	return WeaponFireLibrary::CalculateAimDirection(
		World,
		GetWeapon(),
		InstigatorActor,
		FireTransform.GetLocation(),
		FireTransform.GetRotation().GetForwardVector(),
		GetWeapon()->GetRuntimeStats().Range
	);
}

FVector UWeaponFireExecutorComponent::ApplySpreadToDirection(const FVector& Direction, float SpreadAngle)
{
	return SpreadAngle > 0.0f
		? FMath::VRandCone(Direction.GetSafeNormal(), FMath::DegreesToRadians(SpreadAngle)).GetSafeNormal()
		: Direction.GetSafeNormal();
}

FWeaponFireContext UWeaponFireExecutorComponent::MakeFireContext(
	AActor* InstigatorActor,
	UAbilitySystemComponent* SourceASC,
	TSubclassOf<UGameplayEffect> DamageEffectClass,
	float AbilityLevel,
	bool bShowDebug) const
{
	FWeaponFireContext FireContext;
	FireContext.InstigatorActor = InstigatorActor;
	FireContext.SourceASC = SourceASC;
	FireContext.DamageEffectClass = DamageEffectClass;
	FireContext.AbilityLevel = AbilityLevel;
	FireContext.bShowDebug = bShowDebug;
	FireContext.FireTransform = GetWeapon()->GetFireTransform(InstigatorActor);
	FireContext.FireDirection = ApplySpreadToDirection(
		CalculateBaseFireDirection(InstigatorActor, FireContext.FireTransform),
		GetCurrentSpreadAngle(SourceASC));
	return FireContext;
}

bool UWeaponFireExecutorComponent::HandleHitScanFire(const FWeaponFireContext& FireContext)
{
	AWeaponBase* Weapon = GetWeapon();
	const FWeaponRuntimeStats& RuntimeStats = Weapon->GetRuntimeStats();
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FHitResult Hit;

	const bool bHit = WeaponFireLibrary::PerformHitScan(
		World,
		Weapon,
		FireContext.InstigatorActor,
		FireContext.FireTransform.GetLocation(),
		FireContext.FireDirection,
		RuntimeStats.Range,
		Hit
	);
	DrawFireDebug(
		FireContext.FireTransform.GetLocation(),
		bHit ? Hit.ImpactPoint : FireContext.FireTransform.GetLocation() + FireContext.FireDirection * RuntimeStats.Range,
		bHit,
		FireContext.bShowDebug);

	const FVector TraceEnd = bHit ? Hit.ImpactPoint : FireContext.FireTransform.GetLocation() + FireContext.FireDirection * RuntimeStats.Range;
	Weapon->CommitFiredShot(FireContext);
	WeaponCueLibrary::ExecuteFireCue(Weapon, FireContext.InstigatorActor, FireContext.SourceASC, FireContext.FireTransform, TraceEnd);

	if (bHit)
	{
		if (CombatThreatLibrary::NotifyHitScanThreat(FireContext.InstigatorActor, Hit))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[HitScanEvade] DamageSkipped | Target=%s | Reason=EvadeAccepted"),
				*GetNameSafe(Hit.GetActor()));
			return true;
		}
		const float DamageAtDistance = WeaponDamageLibrary::CalculateDamageAtDistance(
			RuntimeStats.Damage,
			FVector::Distance(FireContext.FireTransform.GetLocation(), Hit.ImpactPoint),
			RuntimeStats.DamageFalloff,
			Weapon->GetWeaponCategory());
		FWeaponPointDamageRequest DamageRequest;
		DamageRequest.Hit = Hit;
		DamageRequest.InstigatorActor = FireContext.InstigatorActor;
		DamageRequest.SourceASC = FireContext.SourceASC;
		DamageRequest.DamageEffectClass = FireContext.DamageEffectClass;
		DamageRequest.DamageAmount = DamageAtDistance;
		DamageRequest.WeakpointMultiplier = RuntimeStats.WeakpointMultiplier;
		DamageRequest.AbilityLevel = FireContext.AbilityLevel;
		WeaponDamageLibrary::ApplyGasDamageToHit(DamageRequest);
		WeaponCueLibrary::ExecuteImpactCue(Weapon, FireContext.InstigatorActor, FireContext.SourceASC, Hit, WeaponFireLibrary::IsCharacterHit(Hit));
	}

	return true;
}

bool UWeaponFireExecutorComponent::HandleProjectileFire(const FWeaponFireContext& FireContext, UProjectilePoolComponent* ProjectilePool)
{
	AWeaponBase* Weapon = GetWeapon();
	const UWeaponDataAsset* WeaponData = Weapon->GetWeaponData();
	const FWeaponRuntimeStats& RuntimeStats = Weapon->GetRuntimeStats();
	if (!WeaponData || !WeaponData->ProjectileClass || !ProjectilePool)
	{
		return false;
	}

	const AWeaponProjectile* Defaults = WeaponData->ProjectileClass->GetDefaultObject<AWeaponProjectile>();
	FProjectileThreatRequest Threat;
	Threat.World = GetWorld();
	Threat.Causer = Weapon;
	Threat.Instigator = FireContext.InstigatorActor;
	Threat.Origin = FireContext.FireTransform.GetLocation();
	Threat.Direction = FireContext.FireDirection;
	Threat.Speed = Defaults->GetProjectileSpeed();
	Threat.Range = RuntimeStats.Range;
	Threat.GravityZ = Defaults->IsProjectileAffectedByGravity() && GetWorld()
		? GetWorld()->GetGravityZ() * Defaults->GetProjectileGravityScale() : 0.0f;
	CombatThreatLibrary::NotifyProjectileThreats(Threat);

	const FTransform ProjectileTransform(
		FireContext.FireDirection.Rotation(),
		FireContext.FireTransform.GetLocation(),
		FireContext.FireTransform.GetScale3D());
	AWeaponProjectile* Projectile = ProjectilePool->AcquireProjectile(WeaponData->ProjectileClass, ProjectileTransform);
	if (!Projectile)
	{
		return false;
	}

	Projectile->InitializeProjectile(
		Weapon,
		FireContext.InstigatorActor,
		FireContext.SourceASC,
		FireContext.DamageEffectClass,
		FireContext.AbilityLevel,
		RuntimeStats);
	DrawFireDebug(
		ProjectileTransform.GetLocation(),
		ProjectileTransform.GetLocation() + FireContext.FireDirection * FMath::Min(RuntimeStats.Range, 500.0f),
		false,
		FireContext.bShowDebug);
	Weapon->CommitFiredShot(FireContext);
	WeaponCueLibrary::ExecuteFireCue(Weapon, FireContext.InstigatorActor, FireContext.SourceASC, ProjectileTransform);
	return true;
}

bool UWeaponFireExecutorComponent::SpawnSmartTrackingProjectile(
	const FWeaponFireContext& FireContext,
	AActor* TargetActor,
	USplineComponent* SharedPath, UProjectilePoolComponent* ProjectilePool)
{
	AWeaponBase* Weapon = GetWeapon();
	const UWeaponDataAsset* WeaponData = Weapon->GetWeaponData();
	const FWeaponRuntimeStats& RuntimeStats = Weapon->GetRuntimeStats();
	const bool bUseStandardProjectile = Weapon->GetWeaponCategory() == EWeaponCategory::GrenadeLauncher;
	const TSubclassOf<AWeaponProjectile> ProjectileClass = bUseStandardProjectile
		? WeaponData ? WeaponData->ProjectileClass : nullptr
		: WeaponData ? WeaponData->SmartTrackingProjectileClass : nullptr;
	if (!WeaponData || !ProjectileClass || !ProjectilePool
		|| !TargetActor || !SharedPath)
	{
		return false;
	}

	const FVector StartLocation = SharedPath->GetLocationAtDistanceAlongSpline(
		0.0f, ESplineCoordinateSpace::World);
	const FVector FireDirection = SharedPath->GetDirectionAtDistanceAlongSpline(
		0.0f, ESplineCoordinateSpace::World);
	const FTransform ProjectileTransform(
		FireDirection.Rotation(),
		StartLocation,
		FireContext.FireTransform.GetScale3D());

	AWeaponProjectile* Projectile = ProjectilePool->AcquireProjectile(ProjectileClass, ProjectileTransform);
	if (!Projectile)
	{
		return false;
	}

	Projectile->InitializeSmartTrackingProjectile(
		Weapon,
		FireContext.InstigatorActor,
		FireContext.SourceASC,
		FireContext.DamageEffectClass,
		FireContext.AbilityLevel,
		RuntimeStats,
		TargetActor,
		SharedPath);
	return true;
}

void UWeaponFireExecutorComponent::DrawFireDebug(const FVector& TraceStart, const FVector& TraceEnd, bool bHit, bool bShowDebug) const
{
	if (!bShowDebug)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		constexpr float DebugDuration = 1.0f;
		DrawDebugLine(World, TraceStart, TraceEnd, bHit ? FColor::Green : FColor::Red, false, DebugDuration, 0, 1.5f);
		DrawDebugSphere(World, TraceStart, 8.0f, 12, FColor::Cyan, false, DebugDuration);
		DrawDebugSphere(World, TraceEnd, 10.0f, 12, bHit ? FColor::Yellow : FColor::Red, false, DebugDuration);
	}
}
