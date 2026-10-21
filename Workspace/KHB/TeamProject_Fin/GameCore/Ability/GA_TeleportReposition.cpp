#include "GA_TeleportReposition.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "GameCore/AI/MonsterAIController.h"
#include "GameCore/Combat/MonsterCloakComponent.h"
#include "GameCore/Combat/MonsterCombatPositioningComponent.h"
#include "GameCore/Tags/GameplayTagSystem.h"
#include "GameFramework/Pawn.h"

UMonsterCloakComponent* UGA_TeleportReposition::FindCloakComponent(
	const FGameplayAbilityActorInfo* ActorInfo) const
{
	APawn* AvatarPawn = ActorInfo ? Cast<APawn>(ActorInfo->AvatarActor.Get()) : nullptr;
	return IsValid(AvatarPawn) ? AvatarPawn->FindComponentByClass<UMonsterCloakComponent>() : nullptr;
}

UGA_TeleportReposition::UGA_TeleportReposition()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	// 식별은 Ability.Enemy.* 로 한다. 데미지를 주지 않는 GA라 데미지 키가 없다.
	SetAssetTags(FGameplayTagContainer(GameplayTagSystem::Ability_Enemy_Elite_TeleportReposition));
	ActivationOwnedTags.AddTag(GameplayTagSystem::State_AI_ActionCommitted);
}

void UGA_TeleportReposition::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	APawn* AvatarPawn = ActorInfo ? Cast<APawn>(ActorInfo->AvatarActor.Get()) : nullptr;
	UMonsterCombatPositioningComponent* PositioningComponent =
		IsValid(AvatarPawn) ? AvatarPawn->FindComponentByClass<UMonsterCombatPositioningComponent>() : nullptr;
	AMonsterAIController* Controller =
		IsValid(AvatarPawn) ? Cast<AMonsterAIController>(AvatarPawn->GetController()) : nullptr;

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo)
		|| !IsValid(AvatarPawn)
		|| !IsValid(PositioningComponent)
		|| !PositioningComponent->HasPendingReposition())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (IsValid(Controller))
	{
		Controller->SetMovementLocked(true);
		Controller->SetInterruptible(false);
	}
	bTeleportCommitted = false;

	// 몽타주가 진행되는 동안 서서히 사라진다. 좌표 교체는 Commit Notify 시점이다.
	if (UMonsterCloakComponent* CloakComponent = FindCloakComponent(ActorInfo))
	{
		CloakComponent->StartCloak(CloakInDuration);
	}

	if (!IsValid(RepositionMontage))
	{
		// 애님 에셋이 아직 없는 테스트 단계에서는 즉시 이동을 허용한다.
		bTeleportCommitted = PositioningComponent->CommitPendingReposition();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, !bTeleportCommitted);
		return;
	}

	CommitEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		GameplayTagSystem::Event_AI_Teleport_Commit,
		nullptr,
		true,
		true);
	CommitEventTask->EventReceived.AddDynamic(this, &ThisClass::OnTeleportCommit);
	CommitEventTask->ReadyForActivation();

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		RepositionMontage);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnMontageCompleted);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::OnMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnMontageInterrupted);
	MontageTask->ReadyForActivation();
}

void UGA_TeleportReposition::OnTeleportCommit(FGameplayEventData Payload)
{
	APawn* AvatarPawn = CurrentActorInfo ? Cast<APawn>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
	UMonsterCombatPositioningComponent* PositioningComponent =
		IsValid(AvatarPawn) ? AvatarPawn->FindComponentByClass<UMonsterCombatPositioningComponent>() : nullptr;
	if (!IsValid(PositioningComponent) || bTeleportCommitted)
	{
		return;
	}

	bTeleportCommitted = PositioningComponent->CommitPendingReposition();
	if (!bTeleportCommitted)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// 좌표를 옮긴 직후부터 새 위치에서 다시 나타난다.
	if (UMonsterCloakComponent* CloakComponent = FindCloakComponent(CurrentActorInfo))
	{
		CloakComponent->StartReveal(RevealDuration);
	}
}

void UGA_TeleportReposition::OnMontageCompleted()
{
	// Commit Notify 없이 몽타주가 끝나면 위치 변경 없이 실패 처리한다.
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, !bTeleportCommitted);
}

void UGA_TeleportReposition::OnMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_TeleportReposition::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	APawn* AvatarPawn = ActorInfo ? Cast<APawn>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (UMonsterCombatPositioningComponent* PositioningComponent =
		IsValid(AvatarPawn) ? AvatarPawn->FindComponentByClass<UMonsterCombatPositioningComponent>() : nullptr)
	{
		if (!bTeleportCommitted)
		{
			PositioningComponent->CancelPendingReposition();
		}
	}

	if (UMonsterCloakComponent* CloakComponent = FindCloakComponent(ActorInfo))
	{
		if (!bTeleportCommitted)
		{
			// 커밋 전에 끊기면 반투명인 채로 남는다. 즉시 원상복구한다.
			CloakComponent->SetCloakProgress(0.0f);
		}
	}

	if (AMonsterAIController* Controller =
		IsValid(AvatarPawn) ? Cast<AMonsterAIController>(AvatarPawn->GetController()) : nullptr)
	{
		Controller->SetMovementLocked(false);
		Controller->SetInterruptible(true);
	}

	MontageTask = nullptr;
	CommitEventTask = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
