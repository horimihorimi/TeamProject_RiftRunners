#include "GA_EnemyPatternBase.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "GameCore/AI/MonsterAIController.h"
#include "GameCore/Movement/MonsterMovementComponent.h"
#include "GameCore/Character/MonsterCharacter.h"
#include "GameCore/Tags/GameplayTagSystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Kismet/GameplayStatics.h"

UGA_EnemyPatternBase::UGA_EnemyPatternBase()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	ActivationOwnedTags.AddTag(GameplayTagSystem::State_AI_ActionCommitted);
}

FGameplayTag UGA_EnemyPatternBase::GetDamageAttackTag() const
{
	return DefaultDamageAttackTag;
}

FGameplayTag UGA_EnemyPatternBase::ResolvePatternAbilityTag() const
{
	TArray<FGameplayTag> AssetTags;
	GetAssetTags().GetGameplayTagArray(AssetTags);
	for (const FGameplayTag& Tag : AssetTags)
	{
		if (Tag.ToString().StartsWith(TEXT("Ability.Enemy.")))
		{
			return Tag;
		}
	}

	return FGameplayTag();
}

void UGA_EnemyPatternBase::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	UAnimMontage* SelectedMontage = GetPatternMontage();
	const FGameplayTag ResolvedPatternTag = ResolvePatternAbilityTag();
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo) || !IsValid(SelectedMontage))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AMonsterCharacter* Character = ActorInfo ? Cast<AMonsterCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!IsValid(Character) || !ResolvedPatternTag.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	Character->SetCurrentAttackTag(
		GetDamageAttackTag().IsValid() ? GetDamageAttackTag() : ResolvedPatternTag);
	bHasPlayedAttackAlert = false;
	AttackAlertEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, GameplayTagSystem::Event_AI_Boss_AttackAlert, nullptr, false, true);
	if (!IsValid(AttackAlertEventTask))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	AttackAlertEventTask->EventReceived.AddDynamic(this, &ThisClass::OnAttackAlertEvent);
	AttackAlertEventTask->ReadyForActivation();

	BoundPatternMovementComponent = Character->FindComponentByClass<UMonsterMovementComponent>();
	if (IsValid(BoundPatternMovementComponent))
	{
		BoundPatternMovementComponent->OnPatternMovementArrived.AddDynamic(
			this, &UGA_EnemyPatternBase::OnPatternMovementArrived);
		BoundPatternMovementComponent->ConfigureArrivalLoopTiming(SelectedMontage, GetPatternMontagePlayRate());
	}

	OnBeforePatternMontagePlay(ActorInfo);

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, SelectedMontage, GetPatternMontagePlayRate(), GetPatternMontageStartSection());
	if (!IsValid(MontageTask))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	MontageTask->OnCompleted.AddDynamic(this, &UGA_EnemyPatternBase::OnPatternMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &UGA_EnemyPatternBase::OnPatternMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &UGA_EnemyPatternBase::OnPatternMontageInterrupted);
	MontageTask->ReadyForActivation();
}

void UGA_EnemyPatternBase::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	AMonsterCharacter* Character = ActorInfo ? Cast<AMonsterCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (IsValid(Character))
	{
		const FGameplayTag ActiveAttackTag = GetDamageAttackTag().IsValid()
			? GetDamageAttackTag()
			: ResolvePatternAbilityTag();
		if (Character->GetCurrentAttackTag() == ActiveAttackTag)
		{
			Character->SetCurrentAttackTag(FGameplayTag());
		}

	}

	if (IsValid(BoundPatternMovementComponent))
	{
		BoundPatternMovementComponent->OnPatternMovementArrived.RemoveDynamic(
			this, &UGA_EnemyPatternBase::OnPatternMovementArrived);
	}
	BoundPatternMovementComponent = nullptr;

	MontageTask = nullptr;
	AttackAlertEventTask = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_EnemyPatternBase::OnPatternMontageCompleted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_EnemyPatternBase::OnPatternMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_EnemyPatternBase::OnAttackAlertEvent(FGameplayEventData Payload)
{
	if (bHasPlayedAttackAlert || !IsValid(AttackAlertSound))
	{
		return;
	}

	AMonsterCharacter* Character = Cast<AMonsterCharacter>(GetAvatarActorFromActorInfo());
	if (!IsValid(Character))
	{
		return;
	}

	// 이 GA는 서버에서만 돈다. 로컬 재생은 호스트만 들으므로 멀티캐스트로 전원에게 보낸다.
	bHasPlayedAttackAlert = true;
	Character->MulticastPlaySoundAtLocation(
		AttackAlertSound,
		Character->GetActorLocation(),
		AttackAlertVolumeMultiplier);
}

void UGA_EnemyPatternBase::OnPatternMovementArrived(const FName ArrivalLoopSectionName, const FName ArrivalJumpSectionName)
{
	UAnimMontage* SelectedMontage = GetPatternMontage();
	if (ArrivalLoopSectionName.IsNone() || ArrivalJumpSectionName.IsNone() || !IsValid(SelectedMontage) || !CurrentActorInfo)
	{
		return;
	}

	AMonsterCharacter* Character = Cast<AMonsterCharacter>(CurrentActorInfo->AvatarActor.Get());
	USkeletalMeshComponent* Mesh = IsValid(Character) ? Character->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = IsValid(Mesh) ? Mesh->GetAnimInstance() : nullptr;
	if (IsValid(AnimInstance) && AnimInstance->Montage_IsPlaying(SelectedMontage))
	{
		// 도착 순간에 끊지 않고 현재 ApproachLoop의 끝에서 Slam으로 넘어간다.
		AnimInstance->Montage_SetNextSection(ArrivalLoopSectionName, ArrivalJumpSectionName, SelectedMontage);
	}
}
