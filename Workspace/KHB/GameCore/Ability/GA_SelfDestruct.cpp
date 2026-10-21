#include "GA_SelfDestruct.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "GameCore/AI/MonsterAIController.h"
#include "GameCore/Character/BaseCharacter.h"
#include "GameCore/Movement/MonsterMovementComponent.h"
#include "DrawDebugHelpers.h"
#include "GameCore/Combat/MonsterDamageLibrary.h"
#include "GameCore/Projectile/MonsterProjectile.h"
#include "GameCore/Tags/GameplayTagSystem.h"
#include "GameFramework/Character.h"

UGA_SelfDestruct::UGA_SelfDestruct()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// 전용 큐가 기본이다. BP에서 바꿀 수는 있게 두되 비어 있는 상태로 시작하지 않는다.
	ExplosionCueTag = GameplayTagSystem::GameplayCue_AI_SelfDestruct;

	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	// 식별은 Asset Tags의 Ability.Enemy.* 로, 데미지 표는 Damage Attack Tag의 Action.AI.* 로 찾는다.
	FGameplayTagContainer DefaultAssetTags;
	DefaultAssetTags.AddTag(GameplayTagSystem::Ability_Enemy_Common_SelfDestruct);
	SetAssetTags(DefaultAssetTags);
	DamageAttackTag = GameplayTagSystem::Action_AI_SelfDestruct;
	ActivationOwnedTags.AddTag(GameplayTagSystem::State_Invincible);
}

void UGA_SelfDestruct::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		TriggerEventData);

	ACharacter* AvatarCharacter =
		ActorInfo
		? Cast<ACharacter>(ActorInfo->AvatarActor.Get())
		: nullptr;

	AMonsterAIController* MonsterController =
		IsValid(AvatarCharacter)
		? Cast<AMonsterAIController>(
			AvatarCharacter->GetController())
		: nullptr;

	UE_LOG(LogTemp, Warning,
		TEXT("[SelfDestruct] ActivateAbility: Avatar=%s, Controller=%s, Montage=%s"),
		*GetNameSafe(AvatarCharacter),
		*GetNameSafe(MonsterController),
		*GetNameSafe(SelfDestructMontage));

	if (!IsValid(AvatarCharacter)
		|| !IsValid(MonsterController)
		|| !IsValid(SelfDestructMontage)
		|| !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[SelfDestruct] Activation rejected: Avatar/Controller/Montage/CommitAbility check failed."));
		EndAbility(
			Handle,
			ActorInfo,
			ActivationInfo,
			true,
			true);

		return;
	}

	BeginWaitingForDetonate();

	// 이 처리는 팬텀의 빙의 Ability가 아니라 실제 자폭 Ability가 소유한다.
	// 따라서 팬텀이 먼저 재등장하거나 빙의 흐름이 끝나도 자폭 접근은 유지된다.
	if (const AActor* CombatTarget = MonsterController->GetPerceivedTarget())
	{
		LastKnownTargetLocation = CombatTarget->GetActorLocation();
		bHasLastKnownTargetLocation = true;
	}

	GetWorld()->GetTimerManager().SetTimer(
		TargetLossApproachTimerHandle,
		this,
		&UGA_SelfDestruct::MaintainApproachAfterTargetLoss,
		0.05f,
		true);

	PlayMontageTask =
		UAbilityTask_PlayMontageAndWait::
		CreatePlayMontageAndWaitProxy(
			this,
			NAME_None,
			SelfDestructMontage,
			MontagePlayRate);

	if (!IsValid(PlayMontageTask))
	{
		EndAbility(
			Handle,
			ActorInfo,
			ActivationInfo,
			true,
			true);

		return;
	}

	PlayMontageTask->OnCompleted.AddDynamic(
		this,
		&UGA_SelfDestruct::OnMontageCompleted);

	PlayMontageTask->OnInterrupted.AddDynamic(
		this,
		&UGA_SelfDestruct::OnMontageInterrupted);

	PlayMontageTask->OnCancelled.AddDynamic(
		this,
		&UGA_SelfDestruct::OnMontageInterrupted);

	PlayMontageTask->ReadyForActivation();
}

void UGA_SelfDestruct::MaintainApproachAfterTargetLoss()
{
	ACharacter* AvatarCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	AMonsterAIController* MonsterController =
		IsValid(AvatarCharacter)
		? Cast<AMonsterAIController>(AvatarCharacter->GetController())
		: nullptr;
	UMonsterMovementComponent* MovementComponent =
		IsValid(AvatarCharacter)
		? Cast<UMonsterMovementComponent>(AvatarCharacter->GetCharacterMovement())
		: nullptr;

	if (!IsValid(MonsterController) || !IsValid(MovementComponent))
	{
		return;
	}

	// 자폭은 타깃 상실이나 StateTree 전환 때문에 취소되면 안 된다.
	SetCanBeCanceled(false);
	bDetonated = false;
	bFinished = false;

	if (const AActor* CombatTarget = MonsterController->GetPerceivedTarget())
	{
		LastKnownTargetLocation = CombatTarget->GetActorLocation();
		bHasLastKnownTargetLocation = true;
		return;
	}

	if (!bHasLastKnownTargetLocation)
	{
		return;
	}

	MovementComponent->UpdateMovementTowardLocation(LastKnownTargetLocation, 0.05f);
	UE_LOG(LogTemp, Verbose,
		TEXT("[SelfDestruct] Target lost; maintaining approach | Avatar=%s Destination=%s"),
		*GetNameSafe(AvatarCharacter),
		*LastKnownTargetLocation.ToCompactString());
}

void UGA_SelfDestruct::BeginWaitingForDetonate()
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActor))
	{
		return;
	}

	DetonateEventTask =
		UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this,
			GameplayTagSystem::Event_AI_SelfDestruct_Detonate,
			AvatarActor,
			false,
			true);

	if (!IsValid(DetonateEventTask))
	{
		UE_LOG(LogTemp, Error,
			TEXT("[SelfDestruct] Failed to wait for Detonate event: Avatar=%s"),
			*GetNameSafe(AvatarActor));
		return;
	}

	DetonateEventTask->EventReceived.AddDynamic(
		this,
		&UGA_SelfDestruct::OnDetonateEvent);
	DetonateEventTask->ReadyForActivation();
}

void UGA_SelfDestruct::OnDetonateEvent(FGameplayEventData Payload)
{
	if (bDetonated || bFinished)
	{
		return;
	}

	bDetonated = true;

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	UAbilitySystemComponent* AbilitySystemComponent =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(AvatarActor);

	if (!IsValid(AbilitySystemComponent))
	{
		UE_LOG(LogTemp, Error,
			TEXT("[SelfDestruct] Detonate received but ASC is invalid: Avatar=%s"),
			*GetNameSafe(AvatarActor));
		return;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[SelfDestruct] Detonate received: Avatar=%s | ExecuteCue=%s | CueData=%s"),
		*GetNameSafe(AvatarActor),
		*ExplosionCueTag.ToString(),
		*GetNameSafe(ExplosionCueData));

	/*
	 * GE를 지정하기 전에도 크기를 잴 수 있도록 데미지보다 먼저 그린다.
	 * ApplyExplosionDamage는 GE가 없으면 곧바로 빠져나간다.
	 */
	if (bDebugDrawExplosion)
	{
		DrawDebugSphere(
			AvatarActor->GetWorld(),
			AvatarActor->GetActorLocation(),
			ExplosionRadius,
			24,
			FColor::Red,
			false,
			DebugExplosionDrawDuration,
			0,
			2.0f);
	}

	/*
	 * 판정을 큐보다 먼저 낸다. 큐는 연출이라 늦어도 되지만, 이 어빌리티는
	 * 곧바로 사망 처리로 넘어가므로 판정을 뒤로 미룰 여유가 없다.
	 */
	ApplyExplosionDamage(AvatarActor);

	if (!ExplosionCueTag.IsValid())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[SelfDestruct] ExplosionCueTag가 비어 있어 연출을 건너뛴다: Avatar=%s"),
			*GetNameSafe(AvatarActor));
		return;
	}

	/*
	 * 데이터 에셋을 큐로 넘기는 유일한 통로다. 이펙트와 사운드가 전부 거기서
	 * 온다. 위치는 큐가 대상 액터에서 가져오므로 넣지 않는다.
	 */
	FGameplayCueParameters CueParameters;
	CueParameters.SourceObject = ExplosionCueData.Get();

	AbilitySystemComponent->ExecuteGameplayCue(ExplosionCueTag, CueParameters);
}

void UGA_SelfDestruct::ApplyExplosionDamage(AActor* AvatarActor)
{
	if (!IsValid(AvatarActor) || !AvatarActor->HasAuthority() || DamageEffectClass == nullptr)
	{
		return;
	}

	const FVector Origin = AvatarActor->GetActorLocation();

	// 시작과 끝이 같은 구 판정. 자기 자신은 제외한다.
	TMap<AActor*, FHitResult> DamagedHits;
	MonsterDamageLibrary::GatherTargets(
		AvatarActor,
		Origin,
		Origin,
		ExplosionRadius,
		{ AvatarActor },
		DamagedHits);

	/*
	 * 태그는 이 어빌리티의 Asset Tags에서 바로 꺼낸다. 노티파이를 거치지 않고
	 * 여기서 직접 때리므로 아바타에 적어둔 값을 다시 읽을 이유가 없다.
	 */
	MonsterDamageLibrary::ApplyDamageToTargets(
		AvatarActor,
		MonsterDamageLibrary::MakeAttackPayload(
			MonsterDamageLibrary::FindAttackTag(this), DamageEffectClass),
		DamagedHits,
		TEXT("SelfDestruct"));

	UE_LOG(LogTemp, Log,
		TEXT("[SelfDestruct] DamageApplied | Avatar=%s | Radius=%.1f | Targets=%d"),
		*GetNameSafe(AvatarActor),
		ExplosionRadius,
		DamagedHits.Num());
}

void UGA_SelfDestruct::OnMontageCompleted()
{
	FinishSelfDestruct();
}

void UGA_SelfDestruct::OnMontageInterrupted()
{
	UE_LOG(LogTemp, Warning,
		TEXT("[SelfDestruct] Montage interrupted; forcing terminal self-destruct | Avatar=%s Detonated=%s"),
		*GetNameSafe(GetAvatarActorFromActorInfo()),
		bDetonated ? TEXT("True") : TEXT("False"));

	// 노티파이 전에 끊겼다면 폭발을 먼저 확정한 뒤 사망/완료로 끝낸다.
	if (!bDetonated)
	{
		OnDetonateEvent(FGameplayEventData());
	}

	FinishSelfDestruct();
}

void UGA_SelfDestruct::FinishSelfDestruct()
{
	if (bFinished)
	{
		return;
	}

	bFinished = true;

	if (ABaseCharacter* Character = Cast<ABaseCharacter>(GetAvatarActorFromActorInfo()))
	{
		Character->BeginDeath();
		Character->FinishDeath();
	}

	SendSelfDestructCompletedEvent();

	EndAbility(
		CurrentSpecHandle,
		CurrentActorInfo,
		CurrentActivationInfo,
		true,
		false);
}

void UGA_SelfDestruct::NotifySelfDestructDetonate()
{
	AActor* RangedActor = GetAvatarActorFromActorInfo();
	if (!IsValid(RangedActor))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[SelfDestruct] Detonate notify ignored: Avatar is invalid."));
		return;
	}

	const FGameplayTag DetonateEventTag =
		GameplayTagSystem::Event_AI_SelfDestruct_Detonate;

	FGameplayEventData EventData;
	EventData.EventTag = DetonateEventTag;
	EventData.Instigator = RangedActor;
	EventData.Target = RangedActor;

	UE_LOG(LogTemp, Warning,
		TEXT("[SelfDestruct] Detonate notify: Avatar=%s Event=%s"),
		*GetNameSafe(RangedActor),
		*DetonateEventTag.ToString());

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		RangedActor,
		DetonateEventTag,
		EventData);
}

void UGA_SelfDestruct::SendSelfDestructCompletedEvent()
{
	AActor* RangedActor = GetAvatarActorFromActorInfo();
	if (!IsValid(RangedActor))
	{
		return;
	}

	const FGameplayTag CompletedEventTag =
		GameplayTagSystem::Event_AI_SelfDestruct_Completed;

	FGameplayEventData EventData;
	EventData.EventTag = CompletedEventTag;
	EventData.Instigator = RangedActor;
	EventData.Target = RangedActor;

	UE_LOG(LogTemp, Warning,
		TEXT("[SelfDestruct] Completed notify: Avatar=%s Event=%s"),
		*GetNameSafe(RangedActor),
		*CompletedEventTag.ToString());

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		RangedActor,
		CompletedEventTag,
		EventData);
}

void UGA_SelfDestruct::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(TargetLossApproachTimerHandle);
	}

	PlayMontageTask = nullptr;
	DetonateEventTask = nullptr;
	bHasLastKnownTargetLocation = false;

	Super::EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		bReplicateEndAbility,
		bWasCancelled);
}
