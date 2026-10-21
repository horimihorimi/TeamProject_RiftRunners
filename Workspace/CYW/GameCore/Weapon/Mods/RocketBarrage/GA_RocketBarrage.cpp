#include "GA_RocketBarrage.h"

#include "RocketActor.h"
#include "AbilitySystemComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "GameCore/Ability/CombatTargetLibrary.h"
#include "GameCore/Tags/GameplayTagSystem.h"
#include "GameCore/Weapon/Core/WeaponBase.h"
#include "GameCore/Weapon/Core/WeaponComponent.h"
#include "GameCore/Weapon/Projectiles/ProjectilePoolComponent.h"
#include "GameCore/Character/BaseCharacter.h"
#include "TimerManager.h"

UGA_RocketBarrage::UGA_RocketBarrage()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	bReplicateInputDirectly = false;
	ModCooldownTags.AddTag(GameplayTagSystem::Cooldown_WeaponMod_RocketBarrage);
	RequiredWeaponModifier = EWeaponModiferType::RocketBarrage;
	ActivationBlockedTags.AddTag(GameplayTagSystem::State_Dead);
	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = GameplayTagSystem::Event_Weapon_ReloadStarted;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);
	RocketClass = ARocketActor::StaticClass();
}

void UGA_RocketBarrage::OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Unbind();
	ResetRockets();
	Super::OnAvatarSet(ActorInfo, Spec);
	if (!ActorInfo->IsNetAuthority())
	{
		return;
	}
	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	BoundASC = ASC;
	AActor* Avatar = ActorInfo->AvatarActor.Get();
	RocketPool = Avatar->FindComponentByClass<UProjectilePoolComponent>();
	if (!RocketPool)
	{
		RocketPool = NewObject<UProjectilePoolComponent>(Avatar);
		Avatar->AddInstanceComponent(RocketPool);
		RocketPool->RegisterComponent();
	}
	// Charging stays active while the reload-triggered ability is on cooldown.
	DamageHandle = ASC->GenericGameplayEventCallbacks.FindOrAdd(GameplayTagSystem::Event_Combat_DamageDealt)
					   .AddUObject(this, &UGA_RocketBarrage::OnDamageDealt);
	DeathHandle = ASC->RegisterGameplayTagEvent(GameplayTagSystem::State_Dead).AddUObject(this, &UGA_RocketBarrage::OnDeathChanged);
	UWeaponComponent* Weapons = FindAvatarWeaponComponent(ActorInfo);
	BoundWeapons = Weapons;
	if (Weapons)
	{
		Weapons->OnWeaponEquipped.AddDynamic(this, &UGA_RocketBarrage::OnWeaponChanged);
	}
	OnWeaponChanged(Weapons ? Weapons->GetEquippedWeapon() : nullptr);
}

void UGA_RocketBarrage::OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Unbind();
	ResetRockets();
	Super::OnRemoveAbility(ActorInfo, Spec);
}

void UGA_RocketBarrage::Unbind()
{
	if (UAbilitySystemComponent* ASC = BoundASC.Get())
	{
		if (FGameplayEventMulticastDelegate* DamageCallbacks = ASC->GenericGameplayEventCallbacks.Find(GameplayTagSystem::Event_Combat_DamageDealt))
		{
			DamageCallbacks->Remove(DamageHandle);
		}
		ASC->RegisterGameplayTagEvent(GameplayTagSystem::State_Dead).Remove(DeathHandle);
	}
	if (UWeaponComponent* Weapons = BoundWeapons.Get())
	{
		Weapons->OnWeaponEquipped.RemoveDynamic(this, &UGA_RocketBarrage::OnWeaponChanged);
	}
	if (AWeaponBase* Weapon = BoundWeapon.Get())
	{
		Weapon->OnModifiersChanged.Remove(ModifiersHandle);
	}
	BoundASC.Reset();
	BoundWeapons.Reset();
	BoundWeapon.Reset();
	DamageHandle.Reset();
	DeathHandle.Reset();
	ModifiersHandle.Reset();
}

void UGA_RocketBarrage::OnWeaponChanged(AWeaponBase* Weapon)
{
	if (AWeaponBase* Previous = BoundWeapon.Get())
	{
		Previous->OnModifiersChanged.Remove(ModifiersHandle);
	}
	BoundWeapon = Weapon;
	ModifiersHandle.Reset();
	if (Weapon)
	{
		ModifiersHandle = Weapon->OnModifiersChanged.AddUObject(this, &UGA_RocketBarrage::ResetRockets);
	}
	ResetRockets();
}

void UGA_RocketBarrage::OnDeathChanged(FGameplayTag Tag, int32 Count)
{
	if (Count > 0)
	{
		ResetRockets();
	}
}

void UGA_RocketBarrage::ResetRockets()
{
	for (const auto& Rocket : Rockets)
	{
		if (Rocket.IsValid())
		{
			Rocket->DeactivateToPool();
		}
	}
	Rockets.Reset();
	AccumulatedDamage = 0.0f;
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ScanTimer);
	}
	ClearModCooldown();
}

void UGA_RocketBarrage::OnDamageDealt(const FGameplayEventData* Event)
{
	UAbilitySystemComponent* ASC = BoundASC.Get();
	AActor* Avatar = GetAvatarActor(GetCurrentActorInfo());
	if (!Event || Event->EventMagnitude <= 0.0f || !ASC || !Avatar || ASC->HasMatchingGameplayTag(GameplayTagSystem::State_Dead)
		|| !HasRequiredWeaponModifier(GetCurrentActorInfo())
		|| !CombatTargetLibrary::IsHostileTarget(Event->Target.Get(), ASC, Avatar, true))
	{
		return;
	}
	const AActor* DamageCauser = Event->ContextHandle.GetEffectCauser();
	if (DamageCauser && DamageCauser->IsA<ARocketActor>())
	{
		return;
	}
	Rockets.RemoveAll([](const auto& Rocket) { return !Rocket.IsValid(); });
	if (Rockets.Num() >= MaxRockets || !RocketClass)
	{
		return;
	}
	AccumulatedDamage += Event->EventMagnitude;
	while (AccumulatedDamage >= DamageThreshold && Rockets.Num() < MaxRockets)
	{
		ARocketActor* Rocket = RocketPool->AcquireRocket(RocketClass);
		if (!Rocket)
		{
			break;
		}
		Rocket->Prepare(ASC, Rockets.Num());
		Rockets.Add(Rocket);
		AccumulatedDamage -= DamageThreshold;
	}
	// A full rack cannot bank another rack's worth of damage.
	if (Rockets.Num() >= MaxRockets)
	{
		AccumulatedDamage = 0.0f;
	}
	if (!Rockets.IsEmpty() && !GetWorld()->GetTimerManager().IsTimerActive(ScanTimer))
	{
		ScanTargets();
		GetWorld()->GetTimerManager().SetTimer(ScanTimer, this, &UGA_RocketBarrage::ScanTargets, ScanInterval, true);
	}
}

void UGA_RocketBarrage::ScanTargets()
{
	AActor* Avatar = GetAvatarActor(GetCurrentActorInfo());
	if (!Avatar)
	{
		ResetRockets();
		return;
	}
	Rockets.RemoveAll([](const auto& Rocket) { return !Rocket.IsValid(); });
	const FVector Origin = Avatar->GetActorLocation();
	FCollisionQueryParams Query(SCENE_QUERY_STAT(RocketTargets), false, Avatar);
	if (BoundWeapon.IsValid())
	{
		Query.AddIgnoredActor(BoundWeapon.Get());
	}
	for (const auto& Rocket : Rockets)
	{
		Query.AddIgnoredActor(Rocket.Get());
	}
	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(
		Overlaps, Origin, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(SearchRadius), Query);
	TArray<AActor*> Targets;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();
		if (Targets.Contains(Target) || !CombatTargetLibrary::IsHostileTarget(Target, BoundASC.Get(), Avatar))
		{
			continue;
		}
		FHitResult Hit;
		const bool bBlocked =
			GetWorld()->LineTraceSingleByChannel(Hit, Origin, CombatTargetLibrary::GetTargetPoint(Target), ECC_Visibility, Query);
		if (!bBlocked || Hit.GetActor() == Target)
		{
			Targets.Add(Target);
		}
	}
	Targets.Sort([Origin](const AActor& A, const AActor& B) {
		return FVector::DistSquared(Origin, A.GetActorLocation()) < FVector::DistSquared(Origin, B.GetActorLocation());
	});
	TArray<int32> TargetCounts;
	TargetCounts.Init(0, Targets.Num());
	// Reserve every valid lock before distributing rockets that need a new target.
	for (const auto& Rocket : Rockets)
	{
		const int32 TargetIndex = Targets.IndexOfByKey(Rocket->GetTarget());
		if (TargetIndex != INDEX_NONE)
		{
			++TargetCounts[TargetIndex];
		}
		else
		{
			Rocket->SetTarget(nullptr);
		}
	}
	for (const auto& Rocket : Rockets)
	{
		if (Rocket->GetTarget() || Targets.IsEmpty())
		{
			continue;
		}
		int32 BestIndex = 0;
		for (int32 Index = 1; Index < Targets.Num(); ++Index)
		{
			if (TargetCounts[Index] < TargetCounts[BestIndex])
			{
				BestIndex = Index;
			}
		}
		Rocket->SetTarget(Targets[BestIndex]);
		++TargetCounts[BestIndex];
	}
}

void UGA_RocketBarrage::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	Rockets.RemoveAll([](const auto& Rocket) { return !Rocket.IsValid(); });
	UWeaponComponent* Weapons = BoundWeapons.Get();
	if (ActorInfo->IsNetAuthority() && TriggerEventData && TriggerEventData->EventTag == GameplayTagSystem::Event_Weapon_ReloadStarted
		&& HasRequiredWeaponModifier(ActorInfo) && Rockets.ContainsByPredicate([](const auto& Rocket) { return Rocket->CanLaunch(); })
		&& Weapons && Weapons->GetFireDamageEffectClass() && CommitAbilityCooldown(Handle, ActorInfo, ActivationInfo, false))
	{
		OnTriggered();
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UGA_RocketBarrage::OnTriggered()
{
	ScanTargets();
	AActor* Avatar = GetAvatarActor(GetCurrentActorInfo());
	FVector ViewLocation;
	FRotator ViewRotation;
	Avatar->GetActorEyesViewPoint(ViewLocation, ViewRotation);
	if (const APawn* Pawn = Cast<APawn>(Avatar))
	{
		if (AController* Controller = Pawn->GetController())
		{
			Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
		}
	}
	FCollisionQueryParams Query(SCENE_QUERY_STAT(RocketAim), false, Avatar);
	Query.AddIgnoredActor(BoundWeapon.Get());
	for (const auto& Rocket : Rockets)
	{
		Query.AddIgnoredActor(Rocket.Get());
	}
	const FVector End = ViewLocation + ViewRotation.Vector() * AimDistance;
	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, End, ECC_TraceAttack, Query);
	for (int32 Index = Rockets.Num() - 1; Index >= 0; --Index)
	{
		if (!Rockets[Index]->CanLaunch())
		{
			continue;
		}
		Rockets[Index]->Launch(bHit ? Hit.ImpactPoint : End, BoundWeapons->GetFireDamageEffectClass(), GetAbilityLevel());
		Rockets.RemoveAt(Index);
	}
	for (int32 Index = 0; Index < Rockets.Num(); ++Index)
	{
		Rockets[Index]->SetStackIndex(Index);
	}
	if (Rockets.IsEmpty())
	{
		GetWorld()->GetTimerManager().ClearTimer(ScanTimer);
	}
}
