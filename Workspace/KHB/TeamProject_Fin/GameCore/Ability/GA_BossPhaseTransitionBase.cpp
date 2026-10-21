#include "GA_BossPhaseTransitionBase.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameCore/Audio/MusicParameterEvent.h"
#include "GameCore/Character/BossMonsterCharacter.h"
#include "GameCore/Movement/BossMovementComponent.h"
#include "GameCore/Tags/GameplayTagSystem.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"

UGA_BossPhaseTransitionBase::UGA_BossPhaseTransitionBase()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	ActivationOwnedTags.AddTag(GameplayTagSystem::State_AI_ActionCommitted);
	RepositionEventTag = GameplayTagSystem::Event_AI_Boss_PhaseTransition_Reposition;
	TransitionMoveEventTag = GameplayTagSystem::Event_AI_Boss_PhaseTransition_Rise;
}

void UGA_BossPhaseTransitionBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo) || !IsValid(TransitionMontage))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ACharacter* Boss = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	BossMovement = IsValid(Boss) ? Boss->FindComponentByClass<UBossMovementComponent>() : nullptr;
	if (!IsValid(Boss) || !IsValid(BossMovement))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 실행 조건을 확인한 뒤 서버에서 요청한다. 이후 전환 취소 시 음악을 자동 복원하지 않는다.
	if (Boss->HasAuthority() && IsValid(TransitionMusicEvent))
	{
		TransitionMusicEvent->Execute(Boss);
	}

	bRepositioned = false;
	bTransitionMoveStarted = false;
	BossMovement->OnTransitionMoveCompleted.AddDynamic(this, &ThisClass::OnTransitionMoveCompleted);
	RepositionEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, RepositionEventTag, nullptr, false, true);
	RepositionEventTask->EventReceived.AddDynamic(this, &ThisClass::OnRepositionEvent);
	RepositionEventTask->ReadyForActivation();
	TransitionMoveEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, TransitionMoveEventTag, nullptr, false, true);
	TransitionMoveEventTask->EventReceived.AddDynamic(this, &ThisClass::OnTransitionMoveEvent);
	TransitionMoveEventTask->ReadyForActivation();

	SetupTransitionSpecificEvents();
	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, TransitionMontage, MontagePlayRate);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnMontageInterrupted);
	MontageTask->ReadyForActivation();
}

void UGA_BossPhaseTransitionBase::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility, const bool bWasCancelled)
{
	if (IsValid(BossMovement))
	{
		BossMovement->OnTransitionMoveCompleted.RemoveDynamic(this, &ThisClass::OnTransitionMoveCompleted);
		BossMovement->StopPatternMovement();
	}
	MontageTask = nullptr;
	RepositionEventTask = nullptr;
	TransitionMoveEventTask = nullptr;
	BossMovement = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_BossPhaseTransitionBase::OnRepositionEvent(FGameplayEventData)
{
	if (bRepositioned) return;
	bRepositioned = true;

	if (!IsValid(BossMovement) || !BossMovement->TeleportToArenaAnchorOffset(TransitionAnchorOffset))
	{
		UE_LOG(LogTemp, Warning, TEXT("[BossPhaseTransition] Reposition failed | Ability=%s"), *GetName());
	}
}

void UGA_BossPhaseTransitionBase::OnTransitionMoveEvent(FGameplayEventData)
{
	if (bTransitionMoveStarted) return;
	bTransitionMoveStarted = true;

	// 서버 전용 GA라 로컬 재생은 호스트만 듣는다. 멀티캐스트로 전원에게 보낸다.
	AMonsterCharacter* Character = Cast<AMonsterCharacter>(GetAvatarActorFromActorInfo());
	if (IsValid(Character) && IsValid(RiseStartSound))
	{
		Character->MulticastPlaySoundAtLocation(
			RiseStartSound,
			Character->GetActorLocation(),
			RiseStartSoundVolumeMultiplier);
	}

	if (!IsValid(BossMovement) || !BossMovement->BeginTransitionMoveToAnchorOffset(
		GetTransitionTargetAnchorOffset(), RiseSpeed, RiseAcceleration, RiseAcceptanceTolerance, RiseDuration))
	{
		UE_LOG(LogTemp, Warning, TEXT("[BossPhaseTransition] Transition move start failed | Ability=%s"), *GetName());
	}
}

void UGA_BossPhaseTransitionBase::OnTransitionMoveCompleted()
{
	HandleTransitionMoveCompleted();
}

void UGA_BossPhaseTransitionBase::HandleTransitionMoveCompleted()
{
	QueueNextMontageSection(RiseLoopSectionName, SectionAfterTransitionMove);
}

FVector UGA_BossPhaseTransitionBase::GetTransitionTargetAnchorOffset() const
{
	return TransitionTargetAnchorOffset;
}

void UGA_BossPhaseTransitionBase::OnMontageCompleted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_BossPhaseTransitionBase::OnMontageInterrupted()
{
	if (ABossMonsterCharacter* Boss = CurrentActorInfo
		? Cast<ABossMonsterCharacter>(CurrentActorInfo->AvatarActor.Get())
		: nullptr)
	{
		Boss->ApplyCurrentPhaseForm();
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_BossPhaseTransitionBase::QueueNextMontageSection(const FName FromSection, const FName ToSection)
{
	if (FromSection.IsNone() || ToSection.IsNone() || !IsValid(TransitionMontage) || !CurrentActorInfo) return;
	const ACharacter* Boss = Cast<ACharacter>(CurrentActorInfo->AvatarActor.Get());
	UAnimInstance* AnimInstance = IsValid(Boss) && IsValid(Boss->GetMesh()) ? Boss->GetMesh()->GetAnimInstance() : nullptr;
	if (IsValid(AnimInstance) && AnimInstance->Montage_IsPlaying(TransitionMontage))
	{
		AnimInstance->Montage_SetNextSection(FromSection, ToSection, TransitionMontage);
	}
}
