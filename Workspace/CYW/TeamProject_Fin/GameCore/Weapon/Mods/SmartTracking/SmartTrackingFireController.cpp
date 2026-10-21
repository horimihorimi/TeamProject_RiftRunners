#include "GameCore/Weapon/Mods/SmartTracking/SmartTrackingFireController.h"

#include "AbilitySystemComponent.h"
#include "Components/SplineComponent.h"
#include "GameCore/Ability/CombatTargetLibrary.h"
#include "SmartTrackingPathComponent.h"
#include "SmartTrackingPathLibrary.h"
#include "GameCore/Weapon/Core/WeaponComponent.h"
#include "GameCore/Weapon/Libraries/WeaponCueLibrary.h"
#include "GameCore/Weapon/Projectiles/WeaponProjectile.h"
#include "TimerManager.h"

USmartTrackingFireController::USmartTrackingFireController()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

int32 USmartTrackingFireController::ExecuteFire(
	AActor* InstigatorActor,
	UAbilitySystemComponent* SourceASC,
	TSubclassOf<UGameplayEffect> DamageEffectClass,
	float AbilityLevel,
	const TArray<FSmartTrackingFireRequest>& FireRequests)
{
	AWeaponBase* Weapon = GetWeapon();
	const bool bUseStandardProjectile = Weapon && Weapon->GetWeaponCategory() == EWeaponCategory::GrenadeLauncher;
	const TSubclassOf<AWeaponProjectile> ProjectileClass = Weapon && Weapon->GetWeaponData()
		? (bUseStandardProjectile
			? Weapon->GetWeaponData()->ProjectileClass
			: Weapon->GetWeaponData()->SmartTrackingProjectileClass)
		: nullptr;
	if (!Weapon || !GetPaths() || !Weapon->HasAuthority() || !Weapon->GetWeaponData()
		|| !ProjectileClass
		|| !SourceASC || Weapon->GetCurrentAmmo() <= 0 || IsBusy()
		|| !Weapon->HasModifier(EWeaponModiferType::SmartTracking)
		|| FireRequests.IsEmpty()
		|| FireRequests.Num() > SmartTrackingConstants::MaxTargetRequests)
	{
		return 0;
	}

	const FWeaponFireContext FireContext = Weapon->MakeFireContext(
		InstigatorActor, SourceASC, DamageEffectClass, AbilityLevel, false);
	int32 QueuedCount = 0;
	TSet<AActor*> AcceptedTargets;

	for (const FSmartTrackingFireRequest& Request : FireRequests)
	{
		if (QueuedCount >= Weapon->GetCurrentAmmo())
		{
			break;
		}

		AActor* TargetActor = Request.TargetActor;
		if (!IsValid(TargetActor) || Request.BulletCount <= 0
			|| AcceptedTargets.Contains(TargetActor)
			|| !CombatTargetLibrary::IsHostileTarget(TargetActor, SourceASC, InstigatorActor))
		{
			continue;
		}

		const FVector TargetPoint = SmartTrackingPathLibrary::GetTargetPoint(TargetActor);
		const FVector StartPoint = FireContext.FireTransform.GetLocation();
		const int32 BulletCount = FMath::Min(Request.BulletCount, Weapon->GetCurrentAmmo() - QueuedCount);
		USplineComponent* SharedPath = GetPaths()->CreatePath(
			InstigatorActor, TargetActor, StartPoint, TargetPoint);
		if (!SharedPath)
		{
			continue;
		}

		AcceptedTargets.Add(TargetActor);
		for (int32 Index = 0; Index < BulletCount; ++Index)
		{
			FSmartTrackingControllerQueuedShot& Shot = PendingShots.AddDefaulted_GetRef();
			Shot.TargetActor = TargetActor;
			Shot.SharedPath = SharedPath;
			GetPaths()->AddPathReference(SharedPath);
			++QueuedCount;
		}
	}

	if (QueuedCount <= 0)
	{
		return 0;
	}

	bVolleyInProgress = true;
	FiredShotSequence = 0;
	PendingFireContext = FireContext;
	SmartTrackingVolleyInterval = FMath::Max(
		Weapon->GetWeaponData()->SmartTrackingFire.GetVolleyInterval(Weapon->GetWeaponCategory()),
		KINDA_SMALL_NUMBER);
	ProcessNextShot();
	return QueuedCount;
}

void USmartTrackingFireController::ProcessNextShot()
{
	AWeaponBase* Weapon = GetWeapon();
	if (!Weapon || !GetPaths() || !Weapon->HasAuthority() || PendingShots.IsEmpty() || Weapon->GetCurrentAmmo() <= 0)
	{
		ClearPendingShots();
		return;
	}

	const FSmartTrackingControllerQueuedShot Shot = PendingShots[0];
	PendingShots.RemoveAt(0, 1, EAllowShrinking::No);
	AActor* TargetActor = Shot.TargetActor.Get();
	USplineComponent* SharedPath = Shot.SharedPath.Get();
	if (IsValid(TargetActor) && IsValid(SharedPath))
	{
		const FVector TargetPoint = SmartTrackingPathLibrary::GetTargetPoint(TargetActor);
		PendingFireContext.FireTransform = Weapon->GetFireTransform(
			PendingFireContext.InstigatorActor);
		GetPaths()->RebuildPath(
			SharedPath,
			PendingFireContext.FireTransform.GetLocation(),
			TargetPoint);
		PendingFireContext.FireDirection = SharedPath->GetDirectionAtDistanceAlongSpline(
			0.0f, ESplineCoordinateSpace::World);
	}
	if (CombatTargetLibrary::IsHostileTarget(
		TargetActor, PendingFireContext.SourceASC, PendingFireContext.InstigatorActor)
		&& Weapon->SpawnSmartTrackingProjectile(PendingFireContext, TargetActor, SharedPath))
	{
		Weapon->CommitFiredShot(PendingFireContext);
		WeaponCueLibrary::ExecuteSmartTrackingFireCue(
			Weapon, PendingFireContext.InstigatorActor,
			PendingFireContext.SourceASC, PendingFireContext.FireTransform);

		if (UWeaponComponent* WeaponComponent =
			Weapon->GetOwner() ? Weapon->GetOwner()->FindComponentByClass<UWeaponComponent>() : nullptr)
		{
			FSmartTrackingShotResult Result;
			Result.TargetActor = TargetActor;
			Result.ShotIndex = FiredShotSequence++;
			WeaponComponent->BroadcastSmartTrackingShotFired(Result);
		}
	}
	else
	{
		GetPaths()->ReleasePathReference(SharedPath);
	}

	if (!PendingShots.IsEmpty() && Weapon->GetCurrentAmmo() > 0)
	{
		GetWorld()->GetTimerManager().SetTimer(
			FireTimerHandle, this, &USmartTrackingFireController::ProcessNextShot,
			SmartTrackingVolleyInterval, false);
	}
	else
	{
		ClearPendingShots();
	}
}

void USmartTrackingFireController::ClearPendingShots()
{
	for (const FSmartTrackingControllerQueuedShot& Shot : PendingShots)
	{
		if (USmartTrackingPathComponent* Paths = GetPaths()) Paths->ReleasePathReference(Shot.SharedPath.Get());
	}
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(FireTimerHandle);
	PendingShots.Reset();
	PendingFireContext = FWeaponFireContext();

	const bool bWasInProgress = bVolleyInProgress;
	bVolleyInProgress = false;
	if (bWasInProgress) OnVolleyFinished.Broadcast();
}

void USmartTrackingFireController::CancelPendingFire()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(FireTimerHandle);
	}
	ClearPendingShots();
}

void USmartTrackingFireController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelPendingFire();
	Super::EndPlay(EndPlayReason);
}

AWeaponBase* USmartTrackingFireController::GetWeapon() const
{
	return Cast<AWeaponBase>(GetOwner());
}

USmartTrackingPathComponent* USmartTrackingFireController::GetPaths() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<USmartTrackingPathComponent>() : nullptr;
}
