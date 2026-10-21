#include "GA_Emerge.h"

#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemComponent.h"
#include "GameCore/Ability/AbilityTask_CenterPivotRotation.h"
#include "Animation/AnimInstance.h"
#include "GameCore/AI/MonsterAIController.h"
#include "GameCore/Character/CommonEnemyCharacter.h"
#include "GameCore/Character/MonsterCharacter.h"
#include "GameCore/Enemy/Data/EnemyDataAssets.h"
#include "GameCore/Tags/GameplayTagSystem.h"
#include "GameFramework/Character.h"
#include "MotionWarpingComponent.h"
#include "TimerManager.h"

UGA_Emerge::UGA_Emerge()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	/*
	 * 식별은 Ability.Enemy.* 로 한다. 데미지를 주지 않는 GA라 데미지 키가 없다.
	 * Action.AI.* 는 ST_Enemy_Elite, ST_Enemy_Melee1, ST_Enemy_Range 이(가) 아직 이 태그로 GA를 찾아서 남겨 둔다. 옮기면 지운다.
	 */
	FGameplayTagContainer DefaultAssetTags;
	DefaultAssetTags.AddTag(GameplayTagSystem::Ability_Enemy_Common_Emerge);
	DefaultAssetTags.AddTag(GameplayTagSystem::Action_AI_Emerge);
	SetAssetTags(DefaultAssetTags);
	ActivationOwnedTags.AddTag(GameplayTagSystem::State_AI_ActionCommitted);
	ActivationOwnedTags.AddTag(GameplayTagSystem::State_Invincible);
}

void UGA_Emerge::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ACharacter* AvatarCharacter =
		ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	AMonsterAIController* MonsterController =
		IsValid(AvatarCharacter)
		? Cast<AMonsterAIController>(AvatarCharacter->GetController())
		: nullptr;
	AMonsterCharacter* MonsterCharacter = Cast<AMonsterCharacter>(AvatarCharacter);
	const bool bIsWallEmerge = IsValid(MonsterCharacter)
		&& (MonsterCharacter->GetEmergeSpawnType() == EEmergeSpawnType::Wall
			|| MonsterCharacter->GetEmergeSpawnType() == EEmergeSpawnType::Ceiling);
	const bool bUseTurnSection = bIsWallEmerge
		&& MonsterCharacter->ShouldUseEmergeTurnSection();

	UAnimMontage* SelectedMontage = EmergeMontage;
	if (bIsWallEmerge)
	{
		SelectedMontage = WallEmergeMontage;
	}

	// 랜덤 SpawnPoint는 몽타주를 비워 둔다. 그때 실제로 선택된 몬스터의 Definition DA가 기본 연출을 정한다.
	if (const ACommonEnemyCharacter* CommonEnemy = Cast<ACommonEnemyCharacter>(MonsterCharacter))
	{
		if (const UEnemyDefinitionDataAsset* Definition = CommonEnemy->GetEnemyDefinition())
		{
			UAnimMontage* DefinitionMontage = Definition->Emerge.FloorMontage.Get();
			if (MonsterCharacter->GetEmergeSpawnType() == EEmergeSpawnType::Wall)
			{
				DefinitionMontage = Definition->Emerge.WallMontage.Get();
			}
			else if (MonsterCharacter->GetEmergeSpawnType() == EEmergeSpawnType::Ceiling)
			{
				DefinitionMontage = Definition->Emerge.CeilingMontage.Get();
			}

			if (IsValid(DefinitionMontage))
			{
				SelectedMontage = DefinitionMontage;
			}
		}
	}

	if (IsValid(MonsterCharacter))
	{
		// 특정 지점의 고유 연출이 필요할 때만 SpawnPoint의 값이 Definition보다 우선한다.
		if (UAnimMontage* MontageOverride = MonsterCharacter->GetEmergeMontageOverride())
		{
			SelectedMontage = MontageOverride;
		}
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[Emerge] ActivateAbility: Avatar=%s, Controller=%s, SpawnType=%s, UseTurn=%s, RotationWarp=%s, Montage=%s"),
		*GetNameSafe(AvatarCharacter),
		*GetNameSafe(MonsterController),
		bIsWallEmerge
			? (MonsterCharacter->GetEmergeSpawnType() == EEmergeSpawnType::Ceiling
				? TEXT("Ceiling")
				: TEXT("Wall"))
			: TEXT("Floor"),
		bUseTurnSection ? TEXT("True") : TEXT("False"),
		bUseTurnSection && MonsterCharacter->ShouldUseEmergeTurnRotationWarp()
			? TEXT("True")
			: TEXT("False"),
		*GetNameSafe(SelectedMontage));

	if (!IsValid(AvatarCharacter) || !IsValid(MonsterController) || !IsValid(SelectedMontage))
	{
		UE_LOG(LogTemp, Error,
			TEXT("[Emerge] Activation failed: Avatar/Controller/Montage is invalid."));

		if (IsValid(MonsterController))
		{
			MonsterController->SetMovementLocked(false);
		}

		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ActiveMontage = SelectedMontage;
	MonsterController->SetMovementLocked(true);

	if (IsValid(MonsterCharacter))
	{
		if (UMotionWarpingComponent* MotionWarpingComponent = MonsterCharacter->GetMotionWarpingComponent())
		{
			MotionWarpingComponent->RemoveWarpTarget(TurnWarpTargetName);
		}
	}

	if (bUseTurnSection)
	{
		TurnSelectionEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this,
			GameplayTagSystem::Event_AI_Emerge_SelectTurn,
			nullptr,
			true,
			true);
		TurnSelectionEventTask->EventReceived.AddDynamic(
			this,
			&UGA_Emerge::OnTurnSelectionEvent);
		TurnSelectionEventTask->ReadyForActivation();
	}

	PlayMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		SelectedMontage,
		MontagePlayRate,
		bIsWallEmerge ? WallEmergeSectionName : NAME_None);

	if (!IsValid(PlayMontageTask))
	{
		MonsterController->SetMovementLocked(false);

		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	PlayMontageTask->OnCompleted.AddDynamic(this, &UGA_Emerge::OnMontageCompleted);
	PlayMontageTask->OnInterrupted.AddDynamic(this, &UGA_Emerge::OnMontageInterrupted);
	PlayMontageTask->OnCancelled.AddDynamic(this, &UGA_Emerge::OnMontageInterrupted);
	PlayMontageTask->ReadyForActivation();

	if (bIsWallEmerge && AvatarCharacter->GetMesh())
	{
		if (UAnimInstance* AnimInstance = AvatarCharacter->GetMesh()->GetAnimInstance())
		{
			// 이벤트에서 다음 섹션을 지정할 때만 Turn이 재생되도록 기본 연결을 끊는다.
			AnimInstance->Montage_SetNextSection(WallEmergeSectionName, NAME_None, SelectedMontage);
			AnimInstance->Montage_SetNextSection(TurnLeftSectionName, NAME_None, SelectedMontage);
			AnimInstance->Montage_SetNextSection(TurnRightSectionName, NAME_None, SelectedMontage);
		}
	}
}

void UGA_Emerge::OnMontageCompleted()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	ACharacter* AvatarCharacter =
		ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;

	AMonsterCharacter* MonsterCharacter = Cast<AMonsterCharacter>(AvatarCharacter);
	if (IsValid(MonsterCharacter)
		&& MonsterCharacter->ShouldWaitForBattleCombatRelease())
	{
		BattleCombatReleaseEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this,
			GameplayTagSystem::Event_AI_Emerge_CombatRelease,
			nullptr,
			true,
			true);
		BattleCombatReleaseEventTask->EventReceived.AddDynamic(
			this,
			&UGA_Emerge::OnBattleCombatReleased);
		BattleCombatReleaseEventTask->ReadyForActivation();

		MonsterCharacter->NotifyEmergeFinished();
		return;
	}

	if (AMonsterAIController* MonsterController =
		IsValid(AvatarCharacter)
		? Cast<AMonsterAIController>(AvatarCharacter->GetController())
		: nullptr)
	{
		MonsterController->SetMovementLocked(false);
	}

	if (UAbilitySystemComponent* ASC =
		ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		ASC->AddLooseGameplayTag(GameplayTagSystem::State_AI_Encountered);
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Emerge::OnBattleCombatReleased(FGameplayEventData Payload)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	ACharacter* AvatarCharacter =
		ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;

	if (AMonsterCharacter* MonsterCharacter = Cast<AMonsterCharacter>(AvatarCharacter))
	{
		MonsterCharacter->SetWaitForBattleCombatRelease(false);
	}

	if (AMonsterAIController* MonsterController =
		IsValid(AvatarCharacter)
			? Cast<AMonsterAIController>(AvatarCharacter->GetController())
			: nullptr)
	{
		MonsterController->SetMovementLocked(false);
	}

	if (UAbilitySystemComponent* ASC =
		ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		ASC->AddLooseGameplayTag(GameplayTagSystem::State_AI_Encountered);
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Emerge::OnMontageInterrupted()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	ACharacter* AvatarCharacter =
		ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;

	if (AMonsterAIController* MonsterController =
		IsValid(AvatarCharacter)
		? Cast<AMonsterAIController>(AvatarCharacter->GetController())
		: nullptr)
	{
		MonsterController->SetMovementLocked(false);
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_Emerge::OnTurnSelectionEvent(FGameplayEventData Payload)
{
	ACharacter* AvatarCharacter =
		CurrentActorInfo ? Cast<ACharacter>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
	AMonsterAIController* MonsterController =
		IsValid(AvatarCharacter)
			? Cast<AMonsterAIController>(AvatarCharacter->GetController())
			: nullptr;
	UAnimInstance* AnimInstance =
		IsValid(AvatarCharacter) && AvatarCharacter->GetMesh()
			? AvatarCharacter->GetMesh()->GetAnimInstance()
			: nullptr;

	if (!IsValid(AvatarCharacter)
		|| !IsValid(MonsterController)
		|| !IsValid(AnimInstance)
		|| !IsValid(ActiveMontage))
	{
		return;
	}

	AMonsterCharacter* MonsterCharacter = Cast<AMonsterCharacter>(AvatarCharacter);
	if (!IsValid(MonsterCharacter)
		|| (MonsterCharacter->GetEmergeSpawnType() != EEmergeSpawnType::Wall
			&& MonsterCharacter->GetEmergeSpawnType() != EEmergeSpawnType::Ceiling)
		|| !MonsterCharacter->ShouldUseEmergeTurnSection())
	{
		return;
	}

	AActor* TargetActor = MonsterController->GetPerceivedTarget();
	if (!IsValid(TargetActor))
	{
		return;
	}

	const FVector TargetOffset =
		TargetActor->GetActorLocation() - AvatarCharacter->GetActorLocation();
	const FVector ToTarget = TargetOffset.GetSafeNormal();
	const FQuat ActorRotation = AvatarCharacter->GetActorQuat();
	const FVector ActorForward = ActorRotation.GetForwardVector().GetSafeNormal();
	const FVector ActorRight = ActorRotation.GetRightVector().GetSafeNormal();
	const FVector ActorUp = ActorRotation.GetUpVector().GetSafeNormal();

	if (ToTarget.IsNearlyZero()
		|| ActorForward.IsNearlyZero()
		|| ActorRight.IsNearlyZero()
		|| ActorUp.IsNearlyZero())
	{
		return;
	}

	// 월드 Z 기준 CrossProduct가 아니라 현재 캐릭터 기준 좌우를 사용한다.
	// 캐릭터가 180도 뒤집혀 있으면 로컬 Right 방향도 반전되므로
	// Turn_Left/Turn_Right 선택 역시 자동으로 반전된다.
	const FVector LocalTargetDirection =
		ActorRotation.UnrotateVector(ToTarget);
	const float RawSide = FVector::DotProduct(ToTarget, ActorRight);
	const float UpWorldDot = FVector::DotProduct(ActorUp, FVector::UpVector);
	const bool bUpsideDown = UpWorldDot < 0.0f;
	const float CorrectedSide = bUpsideDown ? -RawSide : RawSide;
	const FName TurnSection =
		CorrectedSide >= 0.0f
			? TurnRightSectionName
			: TurnLeftSectionName;

	UE_LOG(LogTemp, Warning,
		TEXT("[EmergeTurnDebug] Decision | Avatar=%s Target=%s | TargetDir=%s | Forward=%s | Right=%s | Up=%s | LocalTarget=%s | RawSide=%.3f | UpWorldDot=%.3f | UpsideDown=%s | CorrectedSide=%.3f | Section=%s"),
		*GetNameSafe(AvatarCharacter),
		*GetNameSafe(TargetActor),
		*ToTarget.ToString(),
		*ActorForward.ToString(),
		*ActorRight.ToString(),
		*ActorUp.ToString(),
		*LocalTargetDirection.ToString(),
		RawSide,
		UpWorldDot,
		bUpsideDown ? TEXT("True") : TEXT("False"),
		CorrectedSide,
		*TurnSection.ToString());

	if (MonsterCharacter->ShouldUseEmergeTurnRotationWarp())
	{
		CenterPivotRotationTask =
			UAbilityTask_CenterPivotRotation::StartCenterPivotRotation(
				this,
				AvatarCharacter);
		CenterPivotRotationTask->ReadyForActivation();

		if (UMotionWarpingComponent* MotionWarpingComponent = MonsterCharacter->GetMotionWarpingComponent())
		{
			// 최종 Forward는 수평 타겟 방향, Up은 월드 +Z가 되도록 전체 회전을 만든다.
			const FRotator TargetRotation =
				FRotationMatrix::MakeFromXZ(ToTarget, FVector::UpVector).Rotator();

			// 현재 위치를 그대로 사용하므로 코드에서 이동 워프는 만들지 않는다.
			FVector WarpPivotLocation;
			FVector WarpPivotExtent;
			AvatarCharacter->GetActorBounds(
				true,
				WarpPivotLocation,
				WarpPivotExtent);

			const FQuat CurrentRotation = AvatarCharacter->GetActorQuat();
			const FQuat DesiredRotation = TargetRotation.Quaternion();
			const FVector RelativeActorLocation =
				AvatarCharacter->GetActorLocation() - WarpPivotLocation;
			const FVector DesiredRelativeActorLocation =
				DesiredRotation.RotateVector(
					CurrentRotation.UnrotateVector(RelativeActorLocation));
			const FVector CorrectedActorLocation =
				WarpPivotLocation + DesiredRelativeActorLocation;

			const FTransform CenterPivotWarp(
				DesiredRotation,
				CorrectedActorLocation);

			MotionWarpingComponent->AddOrUpdateWarpTargetFromTransform(
				TurnWarpTargetName,
				CenterPivotWarp);
		}
	}

	// Emerge 종료 직전 Notify에서 즉시 Jump하면 Emerge -> None 종료 처리와
	// 같은 프레임에 충돌할 수 있다. 다음 섹션을 지정해 경계에서 자연스럽게 전환한다.
	AnimInstance->Montage_SetNextSection(
		WallEmergeSectionName,
		TurnSection,
		ActiveMontage);

	UE_LOG(LogTemp, Warning,
		TEXT("[EmergeTurnDebug] SectionQueued | Avatar=%s Target=%s Side=%.3f Section=%s"),
		*GetNameSafe(AvatarCharacter),
		*GetNameSafe(TargetActor),
		CorrectedSide,
		*TurnSection.ToString());
}

void UGA_Emerge::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (AMonsterCharacter* MonsterCharacter =
		ActorInfo ? Cast<AMonsterCharacter>(ActorInfo->AvatarActor.Get()) : nullptr)
	{
		// Super::EndAbility가 GA 소유 무적을 해제하기 직전, 등장 전 선점한 카운트도 반환한다.
		MonsterCharacter->ReleasePendingEmergeInvincibility();

		if (UMotionWarpingComponent* MotionWarpingComponent = MonsterCharacter->GetMotionWarpingComponent())
		{
			MotionWarpingComponent->RemoveWarpTarget(TurnWarpTargetName);
		}
	}

	CenterPivotRotationTask = nullptr;

	PlayMontageTask = nullptr;
	TurnSelectionEventTask = nullptr;
	BattleCombatReleaseEventTask = nullptr;
	ActiveMontage = nullptr;

	Super::EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		bReplicateEndAbility,
		bWasCancelled);
}
