#include "GA_Dash.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilityTask_FollowDashSpline.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameCore/AI/MonsterAIController.h"
#include "GameCore/Tags/GameplayTagSystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"

UGA_Dash::UGA_Dash()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// 식별은 Ability.Enemy.* 로 한다. 데미지를 주지 않는 GA라 데미지 키가 없다.
	FGameplayTagContainer DefaultAssetTags;
	DefaultAssetTags.AddTag(GameplayTagSystem::Ability_Enemy_Melee_Dash);
	SetAssetTags(DefaultAssetTags);
	ActivationOwnedTags.AddTag(GameplayTagSystem::State_AI_ActionCommitted);

	// 돌진 루트모션이 도중에 끊기면 궤도만 남고 어색하게 멈춘다. 경직을 무시한다.
	ActivationOwnedTags.AddTag(GameplayTagSystem::State_AI_SuperArmor);
}

void UGA_Dash::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	APawn* AvatarPawn = Cast<APawn>(AvatarActor);
	const AMonsterAIController* Controller = IsValid(AvatarPawn)
		? Cast<AMonsterAIController>(AvatarPawn->GetController())
		: nullptr;
	AActor* TargetActor = IsValid(Controller) ? Controller->GetPerceivedTarget() : nullptr;

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo)
		|| !IsValid(AvatarActor)
		|| !IsValid(TargetActor))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FDashPatternChoice* ChosenPattern = ChooseDashPattern(AvatarActor, TargetActor);
	if (ChosenPattern == nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Dash] No pattern matches the current distance. Avatar=%s Target=%s"),
			*GetNameSafe(AvatarActor),
			*GetNameSafe(TargetActor));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ActiveDashPattern = *ChosenPattern;
	bHasActiveDashPattern = true;
	bDashMovementStarted = false;
	DashTargetActor = TargetActor;

	if (!ActiveDashPattern.bUseDashMontage)
	{
		StartDashMovement();
		return;
	}

	WaitDashEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		GameplayTagSystem::Event_AI_Dash_FollowSpline,
		nullptr,
		true,
		true);
	WaitDashEventTask->EventReceived.AddDynamic(this, &ThisClass::OnDashMovementRequested);
	WaitDashEventTask->ReadyForActivation();

	PlayMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		ActiveDashPattern.DashMontage);

	PlayMontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnMontageCompleted);
	PlayMontageTask->OnBlendOut.AddDynamic(this, &ThisClass::OnMontageCompleted);
	PlayMontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnMontageInterrupted);
	PlayMontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnMontageInterrupted);
	PlayMontageTask->ReadyForActivation();
}

const FDashPatternChoice* UGA_Dash::ChooseDashPattern(
	const AActor* AvatarActor,
	const AActor* TargetActor) const
{
	if (!IsValid(AvatarActor) || !IsValid(TargetActor))
	{
		return nullptr;
	}

	const float TargetDistance = FVector::Dist(
		AvatarActor->GetActorLocation(),
		TargetActor->GetActorLocation());

	TArray<const FDashPatternChoice*> ValidPatterns;
	float TotalWeight = 0.0f;

	for (const FDashPatternChoice& Pattern : DashPatterns)
	{
		const bool bMatchesDistance =
			(Pattern.MinDistance <= 0.0f || TargetDistance >= Pattern.MinDistance)
			&& (Pattern.MaxDistance <= 0.0f || TargetDistance <= Pattern.MaxDistance);
		// 동적 경로는 발동할 때 만들므로 그려둔 스플라인이 없어도 유효하다.
		const bool bValidMovement = Pattern.DashSpeed > KINDA_SMALL_NUMBER
			&& (Pattern.bBuildDynamicPath || IsValid(Pattern.SplinePath.Get()));
		const bool bValidAnimation = !Pattern.bUseDashMontage
			|| IsValid(Pattern.DashMontage);

		if (Pattern.Weight > 0.0f && bMatchesDistance && bValidMovement && bValidAnimation)
		{
			ValidPatterns.Add(&Pattern);
			TotalWeight += Pattern.Weight;
		}
	}

	if (ValidPatterns.IsEmpty() || TotalWeight <= 0.0f)
	{
		return nullptr;
	}

	float Roll = FMath::FRandRange(0.0f, TotalWeight);
	for (const FDashPatternChoice* Pattern : ValidPatterns)
	{
		Roll -= Pattern->Weight;
		if (Roll <= 0.0f)
		{
			return Pattern;
		}
	}

	return ValidPatterns.Last();
}

void UGA_Dash::StartDashMovement()
{
	if (bDashMovementStarted || !bHasActiveDashPattern || !IsValid(DashTargetActor))
	{
		return;
	}

	bDashMovementStarted = true;
	DashMovementTask = UAbilityTask_FollowDashSpline::PerformDash(
		this,
		ActiveDashPattern,
		DashTargetActor,
		RotationInterpSpeed,
		bDebugDrawDash);
	DashMovementTask->OnFinished.AddDynamic(this, &ThisClass::OnDashMovementFinished);
	DashMovementTask->ReadyForActivation();
}

void UGA_Dash::OnDashMovementRequested(FGameplayEventData Payload)
{
	StartDashMovement();
}

void UGA_Dash::OnDashMovementFinished()
{
	DashMovementTask = nullptr;

	if (!ActiveDashPattern.bUseDashMontage)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	const ACharacter* AvatarCharacter = CurrentActorInfo
		? Cast<ACharacter>(CurrentActorInfo->AvatarActor.Get())
		: nullptr;
	UAnimInstance* AnimInstance = IsValid(AvatarCharacter) && IsValid(AvatarCharacter->GetMesh())
		? AvatarCharacter->GetMesh()->GetAnimInstance()
		: nullptr;

	if (IsValid(AnimInstance)
		&& IsValid(ActiveDashPattern.DashMontage)
		&& !ActiveDashPattern.MontageEndSection.IsNone()
		&& ActiveDashPattern.DashMontage->GetSectionIndex(ActiveDashPattern.MontageEndSection) != INDEX_NONE)
	{
		AnimInstance->Montage_JumpToSection(
			ActiveDashPattern.MontageEndSection,
			ActiveDashPattern.DashMontage);
	}
}

void UGA_Dash::OnMontageCompleted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Dash::OnMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_Dash::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	PlayMontageTask = nullptr;
	WaitDashEventTask = nullptr;
	DashMovementTask = nullptr;
	DashTargetActor = nullptr;
	bHasActiveDashPattern = false;
	bDashMovementStarted = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
