#include "TurretCombatComponent.h"

#include "TurretActor.h"
#include "TurretMovementComponent.h"
#include "TurretPresentationComponent.h"
#include "AbilitySystemComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameCore/Ability/CombatTargetLibrary.h"
#include "GameCore/Combat/CombatThreatLibrary.h"
#include "GameCore/Tags/GameplayTagSystem.h"
#include "GameCore/Weapon/Core/WeaponBase.h"
#include "GameCore/Weapon/Libraries/WeaponCueLibrary.h"
#include "GameCore/Weapon/Libraries/WeaponDamageLibrary.h"
#include "GameCore/Weapon/Libraries/WeaponFireLibrary.h"
#include "GameCore/Weapon/Projectiles/ProjectilePoolComponent.h"
#include "GameCore/Weapon/Projectiles/WeaponProjectile.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

UTurretCombatComponent::UTurretCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true);
	ShotsPerPosition.Add(EWeaponCategory::Rifle, 3);
	ShotsPerPosition.Add(EWeaponCategory::GrenadeLauncher, 1);
	ShotsPerPosition.Add(EWeaponCategory::SniperRifle, 1);
}

ATurretActor* UTurretCombatComponent::GetTurret() const
{
	return CastChecked<ATurretActor>(GetOwner());
}

bool UTurretCombatComponent::Start()
{
	Movement = GetTurret()->GetMovement();
	if (GetOwner()->HasAuthority())
	{
		if (!IsValid(GetOwner()->GetOwner()) || !SourceASC || !GetTurret()->IsConfigured(SourceWeapon))
		{
			return false;
		}
		Movement->Initialize();
		SearchForTarget();
		GetWorld()->GetTimerManager().SetTimer(SearchTimer, this, &UTurretCombatComponent::SearchForTarget, SearchInterval, true);
	}
	SetComponentTickEnabled(true);
	return true;
}

void UTurretCombatComponent::Stop()
{
	SetComponentTickEnabled(false);
	GetWorld()->GetTimerManager().ClearTimer(SearchTimer);
	if (GetOwner()->HasAuthority())
	{
		SetState(EDeployTurretState::Deactivated);
		CurrentTarget.Reset();
	}
}

void UTurretCombatComponent::NotifyTargetDetected()
{
	OnRep_CurrentTarget();
}

bool UTurretCombatComponent::IsConfigured(const AWeaponBase* Weapon) const
{
	const UWeaponDataAsset* Data = Weapon ? Weapon->GetWeaponData() : nullptr;
	return Data
		&& (Weapon->GetWeaponCategory() != EWeaponCategory::GrenadeLauncher
			|| (Data->ProjectileClass && Weapon->FindComponentByClass<UProjectilePoolComponent>()));
}

void UTurretCombatComponent::Initialize(AWeaponBase* InWeapon, UAbilitySystemComponent* InASC,
	TSubclassOf<UGameplayEffect> InDamageEffect, float InAbilityLevel, float InDamage, int32 InAmmo)
{
	SourceWeapon = InWeapon;
	SourceASC = InASC;
	DamageEffect = InDamageEffect;
	AbilityLevel = InAbilityLevel;
	Damage = InDamage;
	CurrentAmmo = InAmmo;
	ShotStats = InWeapon->GetRuntimeStats();
	ShotStats.Damage = InDamage;
	ShotStats.Modifiers.Reset();
	ShotStats.FireType = InWeapon->GetWeaponCategory() == EWeaponCategory::GrenadeLauncher
		? EWeaponFireType::Projectile : EWeaponFireType::HitScan;
	ProjectileClass = InWeapon->GetWeaponData()->ProjectileClass;
	ProjectilePool = InWeapon->FindComponentByClass<UProjectilePoolComponent>();
	const int32* CategoryBurstSize = ShotsPerPosition.Find(InWeapon->GetWeaponCategory());
	BurstSize = InWeapon->GetWeaponCategory() == EWeaponCategory::Rifle
		? (CategoryBurstSize ? FMath::Max(*CategoryBurstSize, 1) : 3) : 1;
}

void UTurretCombatComponent::OnRep_CurrentTarget()
{
	if (GetTurret()->GetPresentation()->IsReady() && !GetTurret()->IsDeactivated() && CurrentTarget.IsValid())
	{
		GetTurret()->GetPresentation()->PlayDetectSound();
	}
}

void UTurretCombatComponent::SetState(EDeployTurretState NewState)
{
	if (State == NewState)
	{
		return;
	}
	State = NewState;
	GetOwner()->ForceNetUpdate();
}

bool UTurretCombatComponent::IsValidTarget(const AActor* Target) const
{
	return IsValid(Target) && CombatTargetLibrary::IsHostileTarget(Target, SourceASC, GetOwner()->GetOwner())
		&& FVector::DistSquared(GetOwner()->GetActorLocation(), Target->GetActorLocation()) <= FMath::Square(SearchRadius);
}

bool UTurretCombatComponent::HasLineOfSight(const AActor* Target) const
{
	FCollisionQueryParams Query(SCENE_QUERY_STAT(TurretSight), false, GetOwner());
	Query.AddIgnoredActor(GetOwner()->GetOwner());
	Query.AddIgnoredActor(SourceWeapon);
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByChannel(Hit, GetTurret()->GetMuzzle()->GetComponentLocation(),
		CombatTargetLibrary::GetTargetPoint(Target), ECC_Visibility, Query) || Hit.GetActor() == Target;
}

AActor* UTurretCombatComponent::FindNearestEnemy() const
{
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(TurretTargets), false, GetOwner()->GetOwner());
	GetWorld()->OverlapMultiByObjectType(Overlaps, GetOwner()->GetActorLocation(), FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(SearchRadius), Query);
	AActor* Nearest = nullptr;
	float BestDistance = FMath::Square(SearchRadius);
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();
		if (!IsValidTarget(Target))
		{
			continue;
		}
		const float Distance = FVector::DistSquared(GetOwner()->GetActorLocation(), Target->GetActorLocation());
		if (Distance < BestDistance && HasLineOfSight(Target))
		{
			Nearest = Target;
			BestDistance = Distance;
		}
	}
	return Nearest;
}

void UTurretCombatComponent::SearchForTarget()
{
	if (GetTurret()->IsDeactivated() || IsValidTarget(CurrentTarget.Get()))
	{
		return;
	}
	if (AActor* Target = FindNearestEnemy())
	{
		StartIntercept(Target);
	}
	else if (State != EDeployTurretState::Following)
	{
		GetTurret()->Deactivate();
	}
}

void UTurretCombatComponent::StartIntercept(AActor* Target)
{
	CurrentTarget = Target;
	OnRep_CurrentTarget();
	SetState(EDeployTurretState::Intercepting);
	GetOwner()->ForceNetUpdate();
	ShotsFiredAtPosition = 0;
	Movement->StartIntercept(Target);
}

void UTurretCombatComponent::TickComponent(float DeltaSeconds, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaSeconds, TickType, ThisTickFunction);
	if (GetTurret()->IsDeactivated())
	{
		return;
	}
	if (!GetOwner()->HasAuthority())
	{
		Movement->InterpolateReplicatedMovement(DeltaSeconds);
		return;
	}
	if (!IsValid(GetOwner()->GetOwner()) || !IsValid(SourceWeapon) || !IsValid(SourceASC)
		|| SourceASC->HasMatchingGameplayTag(GameplayTagSystem::State_Dead) || CurrentAmmo <= 0)
	{
		GetTurret()->Deactivate();
		return;
	}
	if (!GetTurret()->GetPresentation()->IsReady())
	{
		return;
	}
	if (State != EDeployTurretState::Following && !IsValidTarget(CurrentTarget.Get()))
	{
		SearchForTarget();
	}
	switch (State)
	{
	case EDeployTurretState::Following:
		Movement->FollowPlayer(DeltaSeconds);
		break;
	case EDeployTurretState::Intercepting:
		if (Movement->MoveToIntercept(DeltaSeconds))
		{
			SetState(EDeployTurretState::Attacking);
		}
		break;
	case EDeployTurretState::Attacking:
	case EDeployTurretState::Repositioning:
		UpdateCombat(DeltaSeconds);
		break;
	default:
		break;
	}
	if (!GetTurret()->IsDeactivated())
	{
		Movement->UpdateVisualOffset(DeltaSeconds);
	}
}

void UTurretCombatComponent::UpdateCombat(float DeltaTime)
{
	const bool bRepositioning = State == EDeployTurretState::Repositioning;
	const bool bArrived = Movement->FollowTarget(CurrentTarget.Get(), DeltaTime, bRepositioning);
	if (bRepositioning && !bArrived)
	{
		return;
	}
	SetState(EDeployTurretState::Attacking);
	if (GetWorld()->GetTimeSeconds() - LastShotTime < SourceWeapon->GetCurrentFireInterval(SourceASC))
	{
		return;
	}
	if (!HasLineOfSight(CurrentTarget.Get()))
	{
		Reposition();
		return;
	}
	if (FireAtTarget())
	{
		--CurrentAmmo;
		++ShotsFiredAtPosition;
		LastShotTime = GetWorld()->GetTimeSeconds();
		if (CurrentAmmo <= 0)
		{
			GetTurret()->Deactivate();
		}
		else if (ShotsFiredAtPosition >= BurstSize)
		{
			Reposition();
		}
	}
}

void UTurretCombatComponent::Reposition()
{
	Movement->ChooseNextPosition(CurrentTarget.Get());
	ShotsFiredAtPosition = 0;
	SetState(EDeployTurretState::Repositioning);
}

bool UTurretCombatComponent::FireAtTarget()
{
	const FVector Start = GetTurret()->GetMuzzle()->GetComponentLocation();
	const FVector AimDirection = (CombatTargetLibrary::GetTargetPoint(CurrentTarget.Get()) - Start).GetSafeNormal();
	const FRotator Rotation = FMath::VRandCone(AimDirection, FMath::DegreesToRadians(SpreadAngle)).Rotation();
	const FTransform FireTransform(Rotation, Start);
	if (ShotStats.FireType == EWeaponFireType::Projectile)
	{
		if (!FireProjectile(FireTransform))
		{
			return false;
		}
	}
	else
	{
		FireHitScan(FireTransform);
	}
	Movement->AddKickback();
	return true;
}

bool UTurretCombatComponent::FireProjectile(const FTransform& FireTransform)
{
	AWeaponProjectile* Projectile = ProjectilePool->AcquireProjectile(ProjectileClass, FireTransform);
	if (!Projectile)
	{
		return false;
	}
	FProjectileThreatRequest Threat;
	Threat.World = GetWorld();
	Threat.Causer = GetOwner();
	Threat.Instigator = GetOwner()->GetOwner();
	Threat.Origin = FireTransform.GetLocation();
	Threat.Direction = FireTransform.GetRotation().GetForwardVector();
	Threat.Speed = Projectile->GetProjectileSpeed();
	Threat.Range = ShotStats.Range;
	Threat.GravityZ = Projectile->IsProjectileAffectedByGravity()
		? GetWorld()->GetGravityZ() * Projectile->GetProjectileGravityScale() : 0.0f;
	CombatThreatLibrary::NotifyProjectileThreats(Threat);
	Projectile->InitializeProjectile(SourceWeapon, GetOwner()->GetOwner(), SourceASC, DamageEffect, AbilityLevel, ShotStats);
	FGameplayCueParameters Parameters = WeaponCueLibrary::MakeWeaponCueParameters(
		SourceWeapon, GetOwner()->GetOwner(), FireTransform.GetLocation(), Threat.Direction);
	PlayFireCue(Parameters);
	return true;
}

void UTurretCombatComponent::FireHitScan(const FTransform& FireTransform)
{
	const FVector Start = FireTransform.GetLocation();
	const FVector ShotDirection = FireTransform.GetRotation().GetForwardVector();
	FHitResult Hit;
	const bool bHit = WeaponFireLibrary::PerformHitScan(
		GetWorld(), SourceWeapon, GetOwner()->GetOwner(), Start, ShotDirection, ShotStats.Range, Hit);
	const FVector End = bHit ? Hit.ImpactPoint : Start + ShotDirection * ShotStats.Range;
	FGameplayCueParameters Parameters = WeaponCueLibrary::MakeTraceCueParameters(SourceWeapon, GetOwner()->GetOwner(), FireTransform, End);
	PlayFireCue(Parameters);
	if (bHit && !CombatThreatLibrary::NotifyHitScanThreat(GetOwner()->GetOwner(), Hit))
	{
		const float DamageAtDistance = WeaponDamageLibrary::CalculateDamageAtDistance(
			ShotStats.Damage, FVector::Dist(Start, Hit.ImpactPoint), ShotStats.DamageFalloff, SourceWeapon->GetWeaponCategory());
		WeaponDamageLibrary::ApplyGasDamageToHit(Hit, GetOwner()->GetOwner(), SourceASC, DamageEffect,
			DamageAtDistance, ShotStats.WeakpointMultiplier, AbilityLevel);
		WeaponCueLibrary::ExecuteImpactCue(SourceWeapon, GetOwner()->GetOwner(), SourceASC, Hit, WeaponFireLibrary::IsCharacterHit(Hit));
	}
}

void UTurretCombatComponent::PlayFireCue(FGameplayCueParameters Parameters) const
{
	Parameters.TargetAttachComponent = GetTurret()->GetMuzzle();
	GetTurret()->GetPresentation()->MulticastFire(Parameters);
}

void UTurretCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->GetTimerManager().ClearTimer(SearchTimer);
	Super::EndPlay(EndPlayReason);
}

void UTurretCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UTurretCombatComponent, State);
	DOREPLIFETIME(UTurretCombatComponent, CurrentTarget);
}
