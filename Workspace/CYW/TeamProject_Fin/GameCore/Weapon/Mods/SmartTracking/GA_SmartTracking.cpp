

#include "GameCore/Weapon/Mods/SmartTracking/GA_SmartTracking.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameCore/Tags/GameplayTagSystem.h"
#include "GameCore/Weapon/Core/WeaponBase.h"
#include "GameCore/Weapon/Core/WeaponComponent.h"
#include "GameCore/Weapon/Mods/SmartTracking/SmartTrackingTargetingComponent.h"

UGA_SmartTracking::UGA_SmartTracking()
{
	FGameplayTagContainer DefaultAbilityTags;
	DefaultAbilityTags.AddTag(GameplayTagSystem::Ability_Weapon_Aim);
	SetAssetTags(DefaultAbilityTags);
	ActivationBlockedTags.AddTag(GameplayTagSystem::State_Dead);
	RequiredWeaponModifier = EWeaponModiferType::SmartTracking;
}

void UGA_SmartTracking::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromInfo(ActorInfo);
	if (ActorInfo && ASC && SmartTrackingEffectClass)
	{
		FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
		ContextHandle.AddInstigator(ActorInfo->AvatarActor.Get(), ActorInfo->AvatarActor.Get());

		FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(
			SmartTrackingEffectClass,
			GetAbilityLevel(),
			ContextHandle);
		if (SpecHandle.IsValid())
		{
			ActiveSmartTrackingEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		}
	}

	StartTrackingScan();
}

void UGA_SmartTracking::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromInfo(ActorInfo);
	if (ASC && ActiveSmartTrackingEffectHandle.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(ActiveSmartTrackingEffectHandle);
		ActiveSmartTrackingEffectHandle.Invalidate();
	}

	StopTrackingScan(ActorInfo);
	if (bWasCancelled)
	{
		if (UWeaponComponent* WeaponComponent = FindAvatarWeaponComponent(ActorInfo))
		{
			if (AWeaponBase* Weapon = WeaponComponent->GetEquippedWeapon())
			{
				Weapon->CancelSmartTrackingFire();
			}
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_SmartTracking::InputReleased(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	if (ActorInfo && ActorInfo->IsLocallyControlled())
	{
		RequestSmartTrackingFire();
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UGA_SmartTracking::StartTrackingScan()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (!ActorInfo || !ActorInfo->IsLocallyControlled())
	{
		return;
	}

	LastBroadcastLockState.Reset();

	AActor* AvatarActor = GetAvatarActor(ActorInfo);
	UWeaponComponent* WeaponComponent = FindAvatarWeaponComponent(ActorInfo);
	AWeaponBase* Weapon = WeaponComponent ? WeaponComponent->GetEquippedWeapon() : nullptr;
	const UWeaponDataAsset* WeaponData = Weapon ? Weapon->GetWeaponData() : nullptr;
	if (AvatarActor && WeaponComponent && WeaponData)
	{
		TargetingComponent = WeaponComponent->GetSmartTrackingTargetingComponent();
		if (TargetingComponent)
		{
			BindTargetingDelegates();
			TargetingComponent->StartTargeting(
				ActorInfo->PlayerController.Get(),
				GetAbilitySystemComponentFromInfo(ActorInfo),
				WeaponComponent,
				WeaponData->SmartTrackingTargeting);
		}
		else
		{
			EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo,
				GetCurrentActivationInfo(), true, true);
		}
	}
	else
	{
		EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo,
			GetCurrentActivationInfo(), true, true);
	}
}

void UGA_SmartTracking::StopTrackingScan(const FGameplayAbilityActorInfo* ActorInfo)
{
	if (TargetingComponent)
	{
		TargetingComponent->StopTargeting();
		if (UWeaponComponent* WeaponComponent = FindAvatarWeaponComponent(ActorInfo))
		{
			WeaponComponent->ClearSmartTrackingLocks();
		}
		UnbindTargetingDelegates();
		TargetingComponent = nullptr;
	}

	LastBroadcastLockState.Reset();
}

void UGA_SmartTracking::BindTargetingDelegates()
{
	if (!TargetingComponent)
	{
		return;
	}

	UnbindTargetingDelegates();
	TargetingComponent->OnTargetLocked.AddUObject(this, &UGA_SmartTracking::OnTargetLocked);
	TargetingComponent->OnTargetLost.AddUObject(this, &UGA_SmartTracking::OnTargetLost);
	TargetingComponent->OnStackChanged.AddUObject(this, &UGA_SmartTracking::BroadcastTargetStackChanged);
	TargetingComponent->OnLockStateChanged.AddUObject(this, &UGA_SmartTracking::BroadcastLockStateChanged);
}

void UGA_SmartTracking::UnbindTargetingDelegates()
{
	if (!TargetingComponent)
	{
		return;
	}

	TargetingComponent->OnTargetLocked.RemoveAll(this);
	TargetingComponent->OnTargetLost.RemoveAll(this);
	TargetingComponent->OnStackChanged.RemoveAll(this);
	TargetingComponent->OnLockStateChanged.RemoveAll(this);
}

void UGA_SmartTracking::BroadcastTargetEvent(AActor* TargetActor, const FGameplayTag& EventTag)
{
	BroadcastSmartTrackingEvent(EventTag, TargetActor);
}

void UGA_SmartTracking::BroadcastTargetStackChanged(AActor* TargetActor, int32 StackCount)
{
	BroadcastSmartTrackingEvent(
		GameplayTagSystem::Event_Weapon_SmartTracking_TargetStackChanged,
		TargetActor, static_cast<float>(StackCount));
}

void UGA_SmartTracking::BroadcastLockStateChanged(ESmartTrackingLockState LockState)
{
	if (LastBroadcastLockState.IsSet() && LockState == LastBroadcastLockState.GetValue())
	{
		return;
	}

	BroadcastSmartTrackingEvent(
		GameplayTagSystem::Event_Weapon_SmartTracking_LockStateChanged,
		GetAvatarActor(GetCurrentActorInfo()),
		static_cast<float>(static_cast<uint8>(LockState)));

	LastBroadcastLockState = LockState;
}

void UGA_SmartTracking::RequestSmartTrackingFire()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	const TArray<FSmartTrackingFireRequest> FireRequests = TargetingComponent
		? TargetingComponent->BuildFireRequests()
		: TArray<FSmartTrackingFireRequest>();
	if (FireRequests.IsEmpty())
	{
		return;
	}
	const float AbilityLevel = GetAbilityLevel(GetCurrentAbilitySpecHandle(), ActorInfo);
	if (UWeaponComponent* Component = FindAvatarWeaponComponent(ActorInfo))
	{
		Component->RequestSmartTrackingFire(AbilityLevel, FireRequests);
	}
	else
	{
		EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo,
			GetCurrentActivationInfo(), true, true);
	}
}

void UGA_SmartTracking::BroadcastSmartTrackingEvent(
	const FGameplayTag& EventTag,
	AActor* TargetActor,
	float EventMagnitude,
	const FGameplayAbilityActorInfo* InActorInfo)
{
	const FGameplayAbilityActorInfo* ActorInfo = InActorInfo ? InActorInfo : GetCurrentActorInfo();
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromInfo(ActorInfo);
	AActor* AvatarActor = GetAvatarActor(ActorInfo);
	if (!ASC || !EventTag.IsValid())
	{
		return;
	}

	FGameplayEventData Payload;
	Payload.EventTag = EventTag;
	Payload.Instigator = AvatarActor;
	Payload.Target = TargetActor ? TargetActor : AvatarActor;
	Payload.EventMagnitude = EventMagnitude;
	ASC->HandleGameplayEvent(EventTag, &Payload);
}

void UGA_SmartTracking::OnTargetLocked(AActor* TargetActor)
{
	if (UWeaponComponent* Component = FindAvatarWeaponComponent(GetCurrentActorInfo()))
	{
		Component->BeginSmartTrackingLock(TargetActor);
	}
	BroadcastTargetEvent(TargetActor, GameplayTagSystem::Event_Weapon_SmartTracking_TargetLocked);
}

void UGA_SmartTracking::OnTargetLost(AActor* TargetActor)
{
	if (UWeaponComponent* WeaponComponent = FindAvatarWeaponComponent(GetCurrentActorInfo()))
	{
		WeaponComponent->EndSmartTrackingLock(TargetActor);
	}
	BroadcastTargetEvent(TargetActor, GameplayTagSystem::Event_Weapon_SmartTracking_TargetLost);
}


