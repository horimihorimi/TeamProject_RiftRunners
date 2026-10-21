#include "SmartTrackingSessionComponent.h"
#include "GameCore/Weapon/Core/WeaponComponent.h"
#include "GameCore/Weapon/Core/WeaponBase.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"
#include "SmartTrackingFireController.h"
#include "SmartTrackingRules.h"
#include "GameCore/Attribute/CharacterAttributeSet.h"

USmartTrackingSessionComponent::USmartTrackingSessionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

UWeaponComponent* USmartTrackingSessionComponent::GetWeaponComponent() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UWeaponComponent>() : nullptr;
}

AWeaponBase* USmartTrackingSessionComponent::GetEquippedWeapon() const
{
	const UWeaponComponent* Component = GetWeaponComponent();
	return Component ? Component->GetEquippedWeapon() : nullptr;
}

bool USmartTrackingSessionComponent::CanRequestSmartTrackingFire(
	const AWeaponBase* Weapon,
	const TArray<FSmartTrackingFireRequest>& FireRequests) const
{
	return Weapon
		&& Weapon->HasModifier(EWeaponModiferType::SmartTracking)
		&& !FireRequests.IsEmpty()
		&& FireRequests.Num() <= SmartTrackingConstants::MaxTargetRequests
		&& !Weapon->IsPendingKillPending()
		&& Weapon->CanFire();
}

void USmartTrackingSessionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(USmartTrackingSessionComponent, bSmartTrackingVolleyActive);
}

void USmartTrackingSessionComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (USmartTrackingFireController* Fire = ActiveFireController.Get()) Fire->OnVolleyFinished.Remove(VolleyFinishedHandle);
	ServerSmartTrackingLockStartTimes.Reset();
	bPredictedSmartTrackingVolleyActive = false;
	Super::EndPlay(Reason);
}

int32 USmartTrackingSessionComponent::ExecuteSmartTrackingFire(
	UAbilitySystemComponent* SourceASC,
	float AbilityLevel,
	const TArray<FSmartTrackingFireRequest>& FireRequests)
{
	AActor* OwnerActor = GetOwner();
	UWeaponComponent* WeaponComponent = GetWeaponComponent();
	AWeaponBase* Weapon = WeaponComponent ? WeaponComponent->GetEquippedWeapon() : nullptr;
	if (!OwnerActor || !OwnerActor->HasAuthority() || !WeaponComponent || !Weapon)
	{
		return 0;
	}

	const TSubclassOf<UGameplayEffect> DamageEffect = WeaponComponent->GetFireDamageEffectClass();
	if (bSmartTrackingVolleyActive || !SourceASC || !DamageEffect || !CanRequestSmartTrackingFire(Weapon, FireRequests))
	{
		return 0;
	}

	const TArray<FSmartTrackingFireRequest> ValidatedRequests = BuildServerFireRequests(SourceASC, FireRequests);
	// ExecuteSmartTrackingFire can fire and finish a one-shot volley immediately.
	// Set the gate first so that FinishSmartTrackingVolley clears the final state.
	USmartTrackingFireController* Fire = Weapon->FindComponentByClass<USmartTrackingFireController>();
	if (!Fire || ValidatedRequests.IsEmpty()) return 0;
	if (USmartTrackingFireController* Previous = ActiveFireController.Get()) Previous->OnVolleyFinished.Remove(VolleyFinishedHandle);
	ActiveFireController = Fire;
	VolleyFinishedHandle = Fire->OnVolleyFinished.AddUObject(this, &USmartTrackingSessionComponent::FinishSmartTrackingVolley);
	SetSmartTrackingVolleyActive(true);
	const int32 QueuedShots = Weapon->ExecuteSmartTrackingFire(
		OwnerActor,
		SourceASC,
		DamageEffect,
		AbilityLevel,
		ValidatedRequests);
	if (QueuedShots > 0)
	{
		ServerSmartTrackingLockStartTimes.Reset();
	}
	else
	{
		FinishSmartTrackingVolley();
	}
	return QueuedShots;
}

void USmartTrackingSessionComponent::RequestSmartTrackingFire(
	float AbilityLevel,
	const TArray<FSmartTrackingFireRequest>& FireRequests)
{
	AActor* OwnerActor = GetOwner();
	AWeaponBase* Weapon = GetEquippedWeapon();
	if (!OwnerActor || !CanRequestSmartTrackingFire(Weapon, FireRequests))
	{
		return;
	}

	if (!OwnerActor->HasAuthority())
	{
		bPredictedSmartTrackingVolleyActive = true;
		ServerRequestSmartTrackingFire(Weapon, AbilityLevel, FireRequests);
		return;
	}

	ExecuteSmartTrackingFire(
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwnerActor),
		AbilityLevel,
		FireRequests);
}

void USmartTrackingSessionComponent::ServerRequestSmartTrackingFire_Implementation(
	AWeaponBase* ExpectedWeapon,
	float AbilityLevel,
	const TArray<FSmartTrackingFireRequest>& FireRequests)
{
	if (ExpectedWeapon && ExpectedWeapon == GetEquippedWeapon())
	{
		RequestSmartTrackingFire(AbilityLevel, FireRequests);
	}
	if (!bSmartTrackingVolleyActive)
	{
		ClientSetSmartTrackingVolleyActive(false);
	}
}

void USmartTrackingSessionComponent::BeginSmartTrackingLock(AActor* TargetActor)
{
	AWeaponBase* Weapon = GetEquippedWeapon();
	if (!IsValid(TargetActor) || !Weapon || !Weapon->HasModifier(EWeaponModiferType::SmartTracking))
	{
		return;
	}
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
		if (!SmartTrackingRules::IsValidTarget(TargetActor, GetOwner(), ASC, Weapon)) return;
		if (ServerSmartTrackingLockStartTimes.Num() < SmartTrackingConstants::MaxTargetRequests
			&& !ServerSmartTrackingLockStartTimes.Contains(TargetActor))
		{
			ServerSmartTrackingLockStartTimes.Add(TargetActor, GetWorld()->GetTimeSeconds());
		}
	}
	else
	{
		ServerBeginSmartTrackingLock(Weapon, TargetActor);
	}
}

void USmartTrackingSessionComponent::EndSmartTrackingLock(AActor* TargetActor)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		ServerSmartTrackingLockStartTimes.Remove(TargetActor);
	}
	else
	{
		ServerEndSmartTrackingLock(GetEquippedWeapon(), TargetActor);
	}
}

void USmartTrackingSessionComponent::ClearSmartTrackingLocks()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		ServerSmartTrackingLockStartTimes.Reset();
	}
	else
	{
		ServerClearSmartTrackingLocks(GetEquippedWeapon());
	}
}

void USmartTrackingSessionComponent::FinishSmartTrackingVolley()
{
	if (USmartTrackingFireController* Fire = ActiveFireController.Get()) Fire->OnVolleyFinished.Remove(VolleyFinishedHandle);
	ActiveFireController.Reset();
	VolleyFinishedHandle.Reset();
	// A locally predicted volley can be cancelled before the server's state update arrives.
	// Always clear the local gate so reload/fire input cannot remain locked.
	bPredictedSmartTrackingVolleyActive = false;

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		SetSmartTrackingVolleyActive(false);
	}
}

void USmartTrackingSessionComponent::ServerBeginSmartTrackingLock_Implementation(AWeaponBase* ExpectedWeapon, AActor* TargetActor)
{
	if (ExpectedWeapon && ExpectedWeapon == GetEquippedWeapon())
	{
		BeginSmartTrackingLock(TargetActor);
	}
}

void USmartTrackingSessionComponent::ServerEndSmartTrackingLock_Implementation(AWeaponBase* ExpectedWeapon, AActor* TargetActor)
{
	if (ExpectedWeapon && ExpectedWeapon == GetEquippedWeapon())
	{
		EndSmartTrackingLock(TargetActor);
	}
}

void USmartTrackingSessionComponent::ServerClearSmartTrackingLocks_Implementation(AWeaponBase* ExpectedWeapon)
{
	if (ExpectedWeapon && ExpectedWeapon == GetEquippedWeapon())
	{
		ClearSmartTrackingLocks();
	}
}

void USmartTrackingSessionComponent::ClientSetSmartTrackingVolleyActive_Implementation(bool bActive)
{
	bPredictedSmartTrackingVolleyActive = bActive;
}

void USmartTrackingSessionComponent::SetSmartTrackingVolleyActive(bool bActive)
{
	bSmartTrackingVolleyActive = bActive;
	ClientSetSmartTrackingVolleyActive(bActive);
}

TArray<FSmartTrackingFireRequest> USmartTrackingSessionComponent::BuildServerFireRequests(
	UAbilitySystemComponent* SourceASC,
	const TArray<FSmartTrackingFireRequest>& ClientRequests) const
{
	TArray<FSmartTrackingFireRequest> Requests;
	const AWeaponBase* Weapon = GetEquippedWeapon();
	if (!Weapon || !Weapon->GetWeaponData() || !SourceASC || !GetWorld())
	{
		return Requests;
	}
	const FSmartTrackingTargetingSettings& Settings = Weapon->GetWeaponData()->SmartTrackingTargeting;
	const float Interval = SmartTrackingRules::GetStackInterval(Weapon->GetCurrentFireInterval(SourceASC), Settings);
	const double Now = GetWorld()->GetTimeSeconds();
	int32 RemainingAmmo = Weapon->GetCurrentAmmo();
	TSet<AActor*> AcceptedTargets;
	for (const FSmartTrackingFireRequest& ClientRequest : ClientRequests)
	{
		FSmartTrackingFireRequest Request;
		if (!BuildValidatedRequest(ClientRequest, SourceASC, Weapon, Settings, Now, RemainingAmmo, AcceptedTargets, Request))
		{
			continue;
		}
		Requests.Add(Request);
		if (RemainingAmmo <= 0) break;
	}
	return Requests;
}

bool USmartTrackingSessionComponent::BuildValidatedRequest(
	const FSmartTrackingFireRequest& ClientRequest, UAbilitySystemComponent* SourceASC,
	const AWeaponBase* Weapon, const FSmartTrackingTargetingSettings& Settings, double Now,
	int32& RemainingAmmo, TSet<AActor*>& AcceptedTargets, FSmartTrackingFireRequest& OutRequest) const
{
	AActor* Target = ClientRequest.TargetActor;
	const double* StartedAt = ServerSmartTrackingLockStartTimes.Find(Target);
	if (!StartedAt || ClientRequest.BulletCount <= 0 || AcceptedTargets.Contains(Target)
		|| !SmartTrackingRules::IsValidTarget(Target, GetOwner(), SourceASC, Weapon))
	{
		return false;
	}
	const UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
	const float Health = TargetASC ? TargetASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute()) : 0.0f;
	const float Interval = SmartTrackingRules::GetStackInterval(Weapon->GetCurrentFireInterval(SourceASC), Settings);
	const int32 EarnedCount = SmartTrackingRules::GetStackCount(Now - *StartedAt, Settings.LockPreparationTime, Interval);
	const int32 Count = FMath::Min(FMath::Min(ClientRequest.BulletCount, EarnedCount),
		FMath::Min(RemainingAmmo, SmartTrackingRules::GetMaxBulletCount(Health, Weapon->GetRuntimeStats().Damage)));
	if (Count <= 0)
	{
		return false;
	}
	OutRequest.TargetActor = Target;
	OutRequest.BulletCount = Count;
	AcceptedTargets.Add(Target);
	RemainingAmmo -= Count;
	return true;
}
