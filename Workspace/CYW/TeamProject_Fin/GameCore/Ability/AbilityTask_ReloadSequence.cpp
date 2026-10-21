#include "AbilityTask_ReloadSequence.h"

#include "../Attribute/CharacterAttributeSet.h"
#include "../Tags/GameplayTagSystem.h"
#include "../Weapon/Core/WeaponBase.h"
#include "../Animation/AN_SendGameplayEvent.h"
#include "../Animation/Animnotify_SendGameplayEvent.h"
#include "../Animation/PlayerAnimInstance.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
	float CalculateReloadMontageRate(
		const UAnimMontage* Montage,
		float ReloadTime,
		float SpeedMultiplier)
	{
		if (!Montage)
		{
			return 1.0f;
		}

		// Align the actual commit notify with the weapon's reload duration.
		// Montages without a commit notify use their completion as the commit point.
		float CommitTime = Montage->GetPlayLength();
		for (const FAnimNotifyEvent& Notify : Montage->Notifies)
		{
			const UAN_SendGameplayEvent* EventNotify = Cast<UAN_SendGameplayEvent>(Notify.Notify);
			const UAnimnotify_SendGameplayEvent* LegacyNotify =
				Cast<UAnimnotify_SendGameplayEvent>(Notify.Notify);
			const FGameplayTag Tag = EventNotify ? EventNotify->EventTag
			: LegacyNotify ? LegacyNotify->GetEventTag() : FGameplayTag();

			if (Tag == GameplayTagSystem::Event_Weapon_ReloadCommit && Notify.GetTime() > 0.0f)
			{
				CommitTime = FMath::Min(CommitTime, Notify.GetTime());
			}
		}

		return CommitTime / FMath::Max(ReloadTime, 0.01f)
			* FMath::Max(SpeedMultiplier, 0.01f)
			/ FMath::Max(Montage->RateScale, 0.01f);
	}
}

UAbilityTask_ReloadSequence* UAbilityTask_ReloadSequence::CreateReloadSequence(
	UGameplayAbility* OwningAbility, UAnimMontage* Montage, AWeaponBase* Weapon)
{
	UAbilityTask_ReloadSequence* Task = NewAbilityTask<UAbilityTask_ReloadSequence>(OwningAbility);
	Task->ReloadMontage = Montage;
	Task->ReloadWeapon = Weapon;
	return Task;
}

void UAbilityTask_ReloadSequence::Activate()
{
	if (!Ability || !ReloadMontage || !ReloadWeapon.IsValid() || !CreateReloadEventTask())
	{
		OnMontageCancelled();
		return;
	}
	if (bStopping) return;
	CreateLeftHandIKRecoverTask();
	if (bStopping) return;
	if (!CreateReloadMontageTask()) OnMontageCancelled();
}

void UAbilityTask_ReloadSequence::RequestCommit()
{
	if (bStopping || bCommitRequested || !ShouldBroadcastAbilityTaskDelegates()) return;
	bCommitRequested = true;
	OnCommitRequested.Broadcast();
}

void UAbilityTask_ReloadSequence::OnReloadCommit(FGameplayEventData Payload)
{
	RequestCommit();
}

void UAbilityTask_ReloadSequence::OnMontageCompleted()
{
	RequestCommit();
}

void UAbilityTask_ReloadSequence::OnMontageCancelled()
{
	if (bStopping) return;
	if (ShouldBroadcastAbilityTaskDelegates()) OnCancelled.Broadcast();
	if (!bStopping) EndTask();
}

void UAbilityTask_ReloadSequence::OnDestroy(bool AbilityEnded)
{
	bStopping = true;
	CleanupAbilityTasks();
	Super::OnDestroy(AbilityEnded);
}

void UAbilityTask_ReloadSequence::OnLeftHandIKRecover(FGameplayEventData Payload)
{
	const FGameplayAbilityActorInfo* ActorInfo = (Ability ? Ability->GetCurrentActorInfo() : nullptr);

	if (!ActorInfo)
	{
		return;
	}

	USkeletalMeshComponent* Mesh = ActorInfo->SkeletalMeshComponent.Get();

	if (!Mesh)
	{
		return;
	}

	UPlayerAnimInstance* AnimInstance = Cast<UPlayerAnimInstance>(Mesh->GetAnimInstance());

	if (!AnimInstance)
	{
		return;
	}

	AnimInstance->StartLeftHandIKRecovery();
}

bool UAbilityTask_ReloadSequence::CreateReloadEventTask()
{
	ReloadEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		Ability,
		GameplayTagSystem::Event_Weapon_ReloadCommit,
		nullptr,
		true,
		true
	);

	if (!ReloadEventTask)
	{
		return false;
	}

	ReloadEventTask->EventReceived.AddDynamic(this, &UAbilityTask_ReloadSequence::OnReloadCommit);
	ReloadEventTask->ReadyForActivation();
	return true;
}

void UAbilityTask_ReloadSequence::CreateLeftHandIKRecoverTask()
{
	LeftHandIKRecoverTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		Ability,
		GameplayTagSystem::Event_Weapon_LeftHandIKRecover,
		nullptr,
		true,
		true
	);

	if (!LeftHandIKRecoverTask)
	{
		return;
	}

	LeftHandIKRecoverTask->EventReceived.AddDynamic(this, &UAbilityTask_ReloadSequence::OnLeftHandIKRecover);
	LeftHandIKRecoverTask->ReadyForActivation();
}

bool UAbilityTask_ReloadSequence::CreateReloadMontageTask()
{
	const FGameplayAbilityActorInfo* ActorInfo = Ability ? Ability->GetCurrentActorInfo() : nullptr;
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const float ReloadSpeedMultiplier = ASC
		? ASC->GetNumericAttribute(UCharacterAttributeSet::GetReloadMontageRateMultiplierAttribute())
		: 1.0f;

	const AWeaponBase* Weapon = ReloadWeapon.Get();
	if (!Weapon || !ReloadMontage)
	{
		return false;
	}

	const float ReloadMontageRate = CalculateReloadMontageRate(
		ReloadMontage, Weapon->GetReloadTime(), ReloadSpeedMultiplier);
	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		Ability,
		NAME_None,
		ReloadMontage,
		ReloadMontageRate,
		NAME_None,
		true
	);

	if (!MontageTask)
	{
		return false;
	}

	MontageTask->OnCompleted.AddDynamic(this, &UAbilityTask_ReloadSequence::OnMontageCompleted);
	MontageTask->OnCancelled.AddDynamic(this, &UAbilityTask_ReloadSequence::OnMontageCancelled);
	MontageTask->OnInterrupted.AddDynamic(this, &UAbilityTask_ReloadSequence::OnMontageCancelled);
	MontageTask->ReadyForActivation();
	if (!bStopping && ActorInfo->IsNetAuthority() && ASC
		&& ASC->GetCurrentMontage() == ReloadMontage)
	{
		FGameplayEventData Payload;
		Payload.EventTag = GameplayTagSystem::Event_Weapon_ReloadStarted;
		Payload.Instigator = ActorInfo->AvatarActor.Get();
		Payload.Target = ActorInfo->AvatarActor.Get();
		ASC->HandleGameplayEvent(Payload.EventTag, &Payload);
	}
	return true;
}

void UAbilityTask_ReloadSequence::CleanupAbilityTasks()
{
	if (ReloadEventTask)
	{
		ReloadEventTask->EventReceived.RemoveAll(this);
		ReloadEventTask->EndTask();
		ReloadEventTask = nullptr;
	}

	if (MontageTask)
	{
		MontageTask->OnCompleted.RemoveAll(this);
		MontageTask->OnCancelled.RemoveAll(this);
		MontageTask->OnInterrupted.RemoveAll(this);
		MontageTask->EndTask();
		MontageTask = nullptr;
	}

	if (LeftHandIKRecoverTask)
	{
		LeftHandIKRecoverTask->EventReceived.RemoveAll(this);
		LeftHandIKRecoverTask->EndTask();
		LeftHandIKRecoverTask = nullptr;
	}
}
