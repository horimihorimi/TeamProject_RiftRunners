#include "GameCore/Weapon/Mods/SmartTracking/SmartTrackingTargetingComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameCore/Ability/CombatTargetLibrary.h"
#include "GameCore/Attribute/CharacterAttributeSet.h"
#include "GameCore/Weapon/Core/WeaponBase.h"
#include "GameCore/Weapon/Core/WeaponComponent.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "SmartTrackingRules.h"
#include "SmartTrackingPathLibrary.h"

USmartTrackingTargetingComponent::USmartTrackingTargetingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USmartTrackingTargetingComponent::StartTargeting(
	APlayerController* InPlayerController,
	UAbilitySystemComponent* InSourceASC,
	UWeaponComponent* InWeaponComponent,
	const FSmartTrackingTargetingSettings& InSettings)
{
	StopTargeting();
	PlayerController = InPlayerController;
	SourceASC = InSourceASC;
	WeaponComponent = InWeaponComponent;
	Settings = InSettings;
	LockEntries.Reset();
	LastLockState.Reset();
	ScanTargets();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ScanTimerHandle,
			this,
			&USmartTrackingTargetingComponent::ScanTargets,
			FMath::Max(Settings.ScanInterval, 0.01f),
			true);
		World->GetTimerManager().SetTimer(
			LockStackTimerHandle,
			this,
			&USmartTrackingTargetingComponent::UpdateLockStacks,
			FMath::Max(GetLockStackInterval(), 0.01f),
			true);
	}
}

void USmartTrackingTargetingComponent::StopTargeting()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ScanTimerHandle);
		World->GetTimerManager().ClearTimer(LockStackTimerHandle);
	}

	if (!TrackedTargets.IsEmpty())
	{
		TrackedTargets.Reset();
	}
	OnLockStateChanged.Broadcast(ESmartTrackingLockState::Inactive);
	PlayerController.Reset();
	SourceASC.Reset();
	WeaponComponent.Reset();
	LockEntries.Reset();
	LastLockState.Reset();
}

void USmartTrackingTargetingComponent::ScanTargets()
{
	AActor* SourceActor = GetOwner();
	UAbilitySystemComponent* ASC = SourceASC.Get();
	UWorld* World = GetWorld();
	if (!SourceActor || !ASC || !PlayerController.IsValid() || !World || Settings.Range <= 0.0f)
	{
		return;
	}

	const TArray<TWeakObjectPtr<AActor>> PreviousTargets = TrackedTargets;
	TrackedTargets.Reset();
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SmartTrackingTargetOverlap), false, SourceActor);
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	TArray<FOverlapResult> Results;
	World->OverlapMultiByObjectType(
		Results,
		SourceActor->GetActorLocation(),
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(Settings.Range),
		QueryParams);

	TSet<AActor*> UniqueTargets;
	for (const FOverlapResult& Result : Results)
	{
		AActor* Candidate = Result.GetActor();
		if (!Candidate || UniqueTargets.Contains(Candidate)
			|| !SmartTrackingRules::IsValidTarget(Candidate, SourceActor, ASC, WeaponComponent.IsValid() ? WeaponComponent->GetEquippedWeapon() : nullptr)
			|| !IsInsideReticleLockBox(Candidate))
		{
			continue;
		}
		UniqueTargets.Add(Candidate);
		TrackedTargets.Add(Candidate);
	}
	const FVector AimOrigin = SourceActor->GetActorLocation();
	TrackedTargets.Sort([this, AimOrigin](const TWeakObjectPtr<AActor>& Left, const TWeakObjectPtr<AActor>& Right)
	{
		const FVector AimDirection = WeaponComponent.IsValid() && WeaponComponent->GetEquippedWeapon()
			? WeaponComponent->GetEquippedWeapon()->GetAimDirection(GetOwner(), SourceASC.Get())
			: GetOwner()->GetActorForwardVector();
		const float LeftScore = FVector::DotProduct(AimDirection, (Left->GetActorLocation() - AimOrigin).GetSafeNormal());
		const float RightScore = FVector::DotProduct(AimDirection, (Right->GetActorLocation() - AimOrigin).GetSafeNormal());
		return LeftScore > RightScore;
	});

	TrackedTargets.SetNum(FMath::Min(TrackedTargets.Num(), SmartTrackingConstants::MaxTargetRequests), EAllowShrinking::No);
	UpdateTargetDelta(PreviousTargets);
	UpdateLockedAmmoCount();
	BroadcastAggregateState();
}

void USmartTrackingTargetingComponent::UpdateLockStacks()
{
	if (SourceASC.IsValid() && WeaponComponent.IsValid())
	{
		UpdateLockedAmmoCount();
		BroadcastAggregateState();
	}
}

bool USmartTrackingTargetingComponent::ContainsTarget(const AActor* TargetActor) const
{
	return TargetActor && TrackedTargets.ContainsByPredicate(
		[TargetActor](const TWeakObjectPtr<AActor>& Target)
		{
			return Target.Get() == TargetActor;
		});
}

bool USmartTrackingTargetingComponent::IsInsideReticleLockBox(const AActor* TargetActor) const
{
	APlayerController* PC = PlayerController.Get();
	if (!PC || !TargetActor)
	{
		return false;
	}
	FVector2D ScreenPosition;
	if (!PC->ProjectWorldLocationToScreen(
		SmartTrackingPathLibrary::GetTargetPoint(TargetActor), ScreenPosition, true))
	{
		return false;
	}
	int32 ViewportX = 0;
	int32 ViewportY = 0;
	PC->GetViewportSize(ViewportX, ViewportY);
	if (ViewportX <= 0 || ViewportY <= 0)
	{
		return false;
	}
	const FVector2D ViewportSize(ViewportX, ViewportY);
	const FVector2D HalfBox = ViewportSize * Settings.ReticleLockBoxRatio * 0.5f;
	const FVector2D Offset = ScreenPosition - ViewportSize * 0.5f;
	return FMath::Abs(Offset.X) <= HalfBox.X && FMath::Abs(Offset.Y) <= HalfBox.Y;
}

void USmartTrackingTargetingComponent::UpdateTargetDelta(
	const TArray<TWeakObjectPtr<AActor>>& PreviousTargets)
{
	for (const TWeakObjectPtr<AActor>& Target : TrackedTargets)
	{
		AActor* TargetActor = Target.Get();
		if (TargetActor && !PreviousTargets.ContainsByPredicate(
			[TargetActor](const TWeakObjectPtr<AActor>& Previous) { return Previous.Get() == TargetActor; }))
		{
			FSmartTrackingLockEntry& Entry = LockEntries.AddDefaulted_GetRef();
			Entry.TargetActor = TargetActor;
			Entry.MaxBulletCount = GetTargetMaxLockCount(TargetActor);
			Entry.LockStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
			OnTargetLocked.Broadcast(TargetActor);
			OnStackChanged.Broadcast(TargetActor, 0);
		}
	}

	for (const TWeakObjectPtr<AActor>& Previous : PreviousTargets)
	{
		AActor* TargetActor = Previous.Get();
		if (TargetActor && !ContainsTarget(TargetActor))
		{
			OnTargetLost.Broadcast(TargetActor);
			LockEntries.RemoveAll(
				[TargetActor](const FSmartTrackingLockEntry& Entry)
				{
					return Entry.TargetActor.Get() == TargetActor;
				});
		}
	}
}

void USmartTrackingTargetingComponent::UpdateLockedAmmoCount()
{
	LockEntries.RemoveAll([](const FSmartTrackingLockEntry& Entry) { return !Entry.TargetActor.IsValid(); });
	const int32 CurrentAmmo = GetCurrentAmmo();
	if (CurrentAmmo <= 0)
	{
		for (FSmartTrackingLockEntry& Entry : LockEntries)
		{
			if (Entry.LockedBulletCount != 0)
			{
				Entry.LockedBulletCount = 0;
				OnStackChanged.Broadcast(Entry.TargetActor.Get(), 0);
			}
		}
		return;
	}

	const double CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	int32 RemainingAmmo = CurrentAmmo;
	TArray<int32> PreviousCounts;
	PreviousCounts.Reserve(LockEntries.Num());
	// Keep existing allocations before assigning newly earned stacks to any target.
	for (FSmartTrackingLockEntry& Entry : LockEntries)
	{
		PreviousCounts.Add(Entry.LockedBulletCount);
		AActor* TargetActor = Entry.TargetActor.Get();
		Entry.MaxBulletCount = TargetActor && ContainsTarget(TargetActor) ? GetTargetMaxLockCount(TargetActor) : 0;
		Entry.LockedBulletCount = FMath::Min3(Entry.LockedBulletCount, Entry.MaxBulletCount, RemainingAmmo);
		RemainingAmmo -= Entry.LockedBulletCount;
	}
	for (int32 Index = 0; Index < LockEntries.Num(); ++Index)
	{
		FSmartTrackingLockEntry& Entry = LockEntries[Index];
		const int32 EarnedCount = SmartTrackingRules::GetStackCount(
			CurrentTime - Entry.LockStartTime, Settings.LockPreparationTime, GetLockStackInterval());
		const int32 Additional = FMath::Min(RemainingAmmo,
			FMath::Max(0, FMath::Min(Entry.MaxBulletCount, EarnedCount) - Entry.LockedBulletCount));
		Entry.LockedBulletCount += Additional;
		RemainingAmmo -= Additional;
		if (PreviousCounts[Index] != Entry.LockedBulletCount)
			OnStackChanged.Broadcast(Entry.TargetActor.Get(), Entry.LockedBulletCount);
	}
}

TArray<FSmartTrackingFireRequest> USmartTrackingTargetingComponent::BuildFireRequests()
{
	UpdateLockStacks();
	TArray<FSmartTrackingFireRequest> Requests;
	for (const FSmartTrackingLockEntry& Entry : LockEntries)
	{
		AActor* TargetActor = Entry.TargetActor.Get();
		if (TargetActor && Entry.LockedBulletCount > 0 && ContainsTarget(TargetActor))
		{
			FSmartTrackingFireRequest& Request = Requests.AddDefaulted_GetRef();
			Request.TargetActor = TargetActor;
			Request.BulletCount = Entry.LockedBulletCount;
		}
	}
	return Requests;
}

int32 USmartTrackingTargetingComponent::GetCurrentAmmo() const
{
	const UWeaponComponent* Component = WeaponComponent.Get();
	const AWeaponBase* Weapon = Component ? Component->GetEquippedWeapon() : nullptr;
	return Weapon ? Weapon->GetCurrentAmmo() : 0;
}

float USmartTrackingTargetingComponent::GetBulletDamage() const
{
	const UWeaponComponent* Component = WeaponComponent.Get();
	const AWeaponBase* Weapon = Component ? Component->GetEquippedWeapon() : nullptr;
	return Weapon ? Weapon->GetRuntimeStats().Damage : 0.0f;
}

float USmartTrackingTargetingComponent::GetLockStackInterval() const
{
	const UWeaponComponent* Component = WeaponComponent.Get();
	const AWeaponBase* Weapon = Component ? Component->GetEquippedWeapon() : nullptr;
	return Weapon
		? SmartTrackingRules::GetStackInterval(Weapon->GetCurrentFireInterval(SourceASC.Get()), Settings)
		: Settings.ScanInterval;
}

int32 USmartTrackingTargetingComponent::GetTargetMaxLockCount(AActor* TargetActor) const
{
	const float Damage = GetBulletDamage();
	const UAbilitySystemComponent* TargetASC =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
	const float Health = TargetASC
		? TargetASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute()) : 0.0f;
	return SmartTrackingRules::GetMaxBulletCount(Health, Damage);
}

int32 USmartTrackingTargetingComponent::GetTotalLockedAmmoCount() const
{
	int32 Total = 0;
	for (const FSmartTrackingLockEntry& Entry : LockEntries)
	{
		Total += Entry.LockedBulletCount;
	}
	return Total;
}

ESmartTrackingLockState USmartTrackingTargetingComponent::GetLockState() const
{
	if (TrackedTargets.IsEmpty())
	{
		return ESmartTrackingLockState::Inactive;
	}
	const int32 Ammo = GetCurrentAmmo();
	if (Ammo > 0 && GetTotalLockedAmmoCount() >= Ammo)
	{
		return ESmartTrackingLockState::FullyLocked;
	}
	for (const FSmartTrackingLockEntry& Entry : LockEntries)
	{
		if (Entry.MaxBulletCount > 0 && Entry.LockedBulletCount < Entry.MaxBulletCount)
		{
			return ESmartTrackingLockState::Tracking;
		}
	}
	return ESmartTrackingLockState::FullyLocked;
}

void USmartTrackingTargetingComponent::BroadcastAggregateState()
{
	const ESmartTrackingLockState State = GetLockState();
	if (!LastLockState.IsSet() || LastLockState.GetValue() != State)
	{
		LastLockState = State;
		OnLockStateChanged.Broadcast(State);
	}
}

void USmartTrackingTargetingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopTargeting();
	OnTargetLocked.Clear();
	OnTargetLost.Clear();
	OnStackChanged.Clear();
	OnLockStateChanged.Clear();
	Super::EndPlay(EndPlayReason);
}
