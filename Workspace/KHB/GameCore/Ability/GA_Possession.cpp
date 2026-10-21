#include "GA_Possession.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "GameCore/AI/MonsterAIController.h"
#include "GameCore/Character/MonsterCharacter.h"
#include "GameCore/Movement/FlyingMonsterMovementComponent.h"
#include "GameCore/Tags/GameplayTagSystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MotionWarpingComponent.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"

UGA_Possession::UGA_Possession()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// 식별은 Ability.Enemy.* 로 한다. 데미지를 주지 않는 GA라 데미지 키가 없다.
	FGameplayTagContainer DefaultAssetTags;
	DefaultAssetTags.AddTag(GameplayTagSystem::Ability_Enemy_Melee_Possession);
	SetAssetTags(DefaultAssetTags);
	ActivationOwnedTags.AddTag(GameplayTagSystem::State_AI_ActionCommitted);

	// 빙의는 대상 상태를 바꾸는 도중 끊기면 정합성이 깨진다. 경직을 무시한다.
	ActivationOwnedTags.AddTag(GameplayTagSystem::State_AI_SuperArmor);

	// 숨겨진 동안에도 팬텀은 월드에 남아 있다. 대상의 사망으로 인한 광역 판정 등에
	// 팬텀이 먼저 죽으면 재등장 흐름이 끊기므로, 빙의 Ability가 끝날 때까지 보호한다.
	ActivationOwnedTags.AddTag(GameplayTagSystem::State_Invincible);

	RequiredTargetTags.AddTag(GameplayTagSystem::Character_Enemy_Ranged);
	RequiredTargetTags.AddTag(GameplayTagSystem::State_AI_Encountered);

	BlockedTargetTags.AddTag(GameplayTagSystem::State_Dead);
	BlockedTargetTags.AddTag(GameplayTagSystem::State_AI_Dormant);
	BlockedTargetTags.AddTag(GameplayTagSystem::State_AI_Possessed);
	// 다른 팬텀이 이미 접근 중인 대상은 후보에서 뺀다.
	BlockedTargetTags.AddTag(GameplayTagSystem::State_AI_PossessionReserved);
}

FGameplayTagContainer UGA_Possession::GetEffectiveRequiredTargetTags() const
{
	if (!RequiredTargetTags.IsEmpty())
	{
		return RequiredTargetTags;
	}

	FGameplayTagContainer Defaults;
	Defaults.AddTag(GameplayTagSystem::Character_Enemy_Ranged);
	Defaults.AddTag(GameplayTagSystem::State_AI_Encountered);
	return Defaults;
}

FGameplayTagContainer UGA_Possession::GetEffectiveBlockedTargetTags() const
{
	if (!BlockedTargetTags.IsEmpty())
	{
		return BlockedTargetTags;
	}

	FGameplayTagContainer Defaults;
	Defaults.AddTag(GameplayTagSystem::State_Dead);
	Defaults.AddTag(GameplayTagSystem::State_AI_Dormant);
	Defaults.AddTag(GameplayTagSystem::State_AI_Possessed);
	Defaults.AddTag(GameplayTagSystem::State_AI_PossessionReserved);
	return Defaults;
}

bool UGA_Possession::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const ACharacter* AvatarCharacter =
		ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;

	const AMonsterAIController* Controller =
		IsValid(AvatarCharacter) ? Cast<AMonsterAIController>(AvatarCharacter->GetController()) : nullptr;

	if (!IsValid(Controller))
	{
		return false;
	}

#if ENABLE_DRAW_DEBUG
	if (bDebugSearchRadius && IsValid(AvatarCharacter))
	{
		DrawDebugSphere(
			AvatarCharacter->GetWorld(),
			AvatarCharacter->GetActorLocation(),
			SearchRadius,
			32,
			FColor::Cyan,
			false,
			0.2f,
			0,
			2.0f);
	}
#endif

	// 반경 내에 빙의 가능한 대상이 없으면 여기서 즉시 실패한다.
	// TryActivateAbilitiesByTag가 곧바로 실패로 처리해서, StateTree가 다시 선택하게 된다.
	/*
	 * 대상 조건은 태그로만 판정한다. 예전에는 "나와 같은 플레이어를 보고 있는
	 * 센티널"만 후보로 삼았는데, 멀티에서 플레이어가 흩어지면 타깃이 갈려
	 * 빙의가 영영 성립하지 않았다.
	 */
	return Controller->FindNearestPossessionTarget(
		GetEffectiveRequiredTargetTags(),
		GetEffectiveBlockedTargetTags(),
		nullptr,
		SearchRadius) != nullptr;
}

void UGA_Possession::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ACharacter* AvatarCharacter = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	const AMonsterAIController* Controller =
		IsValid(AvatarCharacter) ? Cast<AMonsterAIController>(AvatarCharacter->GetController()) : nullptr;

	PossessedTarget = IsValid(Controller)
		? Controller->FindNearestPossessionTarget(
			GetEffectiveRequiredTargetTags(),
			GetEffectiveBlockedTargetTags(),
			nullptr,
			SearchRadius)
		: nullptr;

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo)
		|| !IsValid(AvatarCharacter)
		|| !IsValid(PossessedTarget)
		|| !IsValid(ApproachMontage))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 접근 비행이 끝날 때까지 이 대상을 선점한다. 실패 경로에서도 EndAbility가
	// 반드시 불리므로 해제는 거기 한 곳에서만 한다.
	ReservePossessionTarget();

	// 대상 방향으로 즉시 회전.
	const FVector ToTarget =
		(PossessedTarget->GetActorLocation() - AvatarCharacter->GetActorLocation()).GetSafeNormal();
	AvatarCharacter->SetActorRotation(ToTarget.Rotation());

	// 대상이 이동해도 계속 따라가도록 컴포넌트 기반으로 라이브 트래킹한다.
	if (UMotionWarpingComponent* WarpComp =
		AvatarCharacter->FindComponentByClass<UMotionWarpingComponent>())
	{
		WarpComp->AddOrUpdateWarpTargetFromComponent(
			WarpTargetName,
			PossessedTarget->GetRootComponent(),
			FName(NAME_None),
			true,
			FVector::ZeroVector,
			FRotator::ZeroRotator);
	}

	ApproachMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, ApproachMontage);

	ApproachMontageTask->OnCompleted.AddDynamic(this, &UGA_Possession::OnApproachMontageCompleted);
	ApproachMontageTask->OnBlendOut.AddDynamic(this, &UGA_Possession::OnApproachMontageCompleted);
	ApproachMontageTask->OnInterrupted.AddDynamic(this, &UGA_Possession::OnApproachMontageInterrupted);
	ApproachMontageTask->OnCancelled.AddDynamic(this, &UGA_Possession::OnApproachMontageInterrupted);
	ApproachMontageTask->ReadyForActivation();

	// 몽타주가 끝나기 전에 도착하면 바로 빙의를 시작한다.
	GetWorld()->GetTimerManager().SetTimer(
		ArrivalCheckTimerHandle, this, &UGA_Possession::CheckArrival, 0.05f, true);
}

void UGA_Possession::CheckArrival()
{
	if (bPossessionStarted)
	{
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	const ACharacter* AvatarCharacter =
		ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;

	if (!IsValid(AvatarCharacter) || !IsValid(PossessedTarget))
	{
		return;
	}

	const float DistSq = FVector::DistSquared(
		AvatarCharacter->GetActorLocation(), PossessedTarget->GetActorLocation());

	if (DistSq <= FMath::Square(ArrivalRadius))
	{
		StartPossession();
	}
}

void UGA_Possession::OnApproachMontageCompleted()
{
	StartPossession();
}

void UGA_Possession::OnApproachMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_Possession::ReservePossessionTarget()
{
	if (bReservedPossessionTarget || !IsValid(PossessedTarget))
	{
		return;
	}

	UAbilitySystemComponent* TargetASC =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(PossessedTarget);

	if (!IsValid(TargetASC))
	{
		return;
	}

	TargetASC->AddLooseGameplayTag(GameplayTagSystem::State_AI_PossessionReserved);
	bReservedPossessionTarget = true;

	UE_LOG(LogTemp, Warning, TEXT("[Possession] Reserved | Source=%s Target=%s"),
		*GetNameSafe(GetAvatarActorFromActorInfo()), *GetNameSafe(PossessedTarget));
}

void UGA_Possession::ReleasePossessionTarget()
{
	if (!bReservedPossessionTarget)
	{
		return;
	}

	bReservedPossessionTarget = false;

	UAbilitySystemComponent* TargetASC =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(PossessedTarget);

	if (!IsValid(TargetASC))
	{
		return;
	}

	TargetASC->RemoveLooseGameplayTag(GameplayTagSystem::State_AI_PossessionReserved);

	UE_LOG(LogTemp, Warning, TEXT("[Possession] ReservationReleased | Source=%s Target=%s"),
		*GetNameSafe(GetAvatarActorFromActorInfo()), *GetNameSafe(PossessedTarget));
}

void UGA_Possession::StartPossession()
{
	if (bPossessionStarted || !IsValid(PossessedTarget))
	{
		return;
	}

	bPossessionStarted = true;
	bool bAppliedPossessionEffect = false;

	GetWorld()->GetTimerManager().ClearTimer(ArrivalCheckTimerHandle);

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	ACharacter* AvatarCharacter =
		ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;

	if (!IsValid(AvatarCharacter))
	{
		return;
	}

	ApplyPossessionMovementOverride();

	// Phantom 자신을 숨긴다. Destroy가 아니라 체력을 그대로 유지해야 한다.
	if (AMonsterCharacter* MonsterCharacter = Cast<AMonsterCharacter>(AvatarCharacter))
	{
		MonsterCharacter->SetPossessionPresentationHidden(true);
	}
	AvatarCharacter->SetActorEnableCollision(false);

	if (AMonsterAIController* Controller = Cast<AMonsterAIController>(AvatarCharacter->GetController()))
	{
		Controller->SetMovementLocked(true);
	}

	// 대상에 빙의 이펙트 적용 (State.AI.Possessed 태그 + 원거리 공격 버프 +
	// GameplayCue.AI.Possession 오라 연출, 전부 GE 하나로 처리).
	if (IsValid(PossessionEffectClass))
	{
		const IAbilitySystemInterface* TargetASI = Cast<IAbilitySystemInterface>(PossessedTarget);
		UAbilitySystemComponent* TargetASC =
			TargetASI != nullptr ? TargetASI->GetAbilitySystemComponent() : nullptr;

		if (IsValid(TargetASC))
		{
			FGameplayEffectContextHandle EffectContext = TargetASC->MakeEffectContext();
			EffectContext.AddInstigator(AvatarCharacter, AvatarCharacter);

			const FGameplayEffectSpecHandle SpecHandle =
				TargetASC->MakeOutgoingSpec(PossessionEffectClass, 1.0f, EffectContext);

			if (SpecHandle.IsValid())
			{
				AppliedEffectHandle = TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
				bAppliedPossessionEffect = AppliedEffectHandle.IsValid();
				PossessedTargetASC = bAppliedPossessionEffect ? TargetASC : nullptr;

				// 이 이펙트가 자연 만료(또는 제거)되는 시점을 구독한다.
				// 별도 타이머를 두지 않고, GE의 Duration이 빙의 유지 시간의 단일 진실 소스가 된다.
				if (bAppliedPossessionEffect)
				{
					UE_LOG(LogTemp, Warning,
						TEXT("[Possession] Effect applied: Target=%s, HandleValid=%s"),
						*GetNameSafe(PossessedTarget),
						AppliedEffectHandle.IsValid() ? TEXT("true") : TEXT("false"));

					BeginWaitingForSelfDestructCompleted();
					BeginWaitingForSelfDestructDetonate();
					TargetASC->RegisterGameplayTagEvent(
						GameplayTagSystem::State_Dead,
						EGameplayTagEventType::NewOrRemoved)
						.AddUObject(this, &UGA_Possession::OnPossessedTargetDead);

					if (ABaseCharacter* TargetCharacter = Cast<ABaseCharacter>(PossessedTarget))
					{
						TargetCharacter->OnDeathStarted.AddUObject(
							this, &UGA_Possession::OnPossessedTargetDeathStarted);
					}

					GetWorld()->GetTimerManager().SetTimer(
						SelfDestructTimerHandle,
						this,
						&UGA_Possession::TriggerSelfDestruct,
						SelfDestructDelay,
						false);

					UE_LOG(LogTemp, Warning,
						TEXT("[Possession] SelfDestruct timer started: Target=%s Delay=%.2f"),
						*GetNameSafe(PossessedTarget), SelfDestructDelay);
				}
			}
		}
	}

	if (!bAppliedPossessionEffect)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Possession] Possession effect was not applied as a persistent effect."));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

void UGA_Possession::ApplyPossessionMovementOverride()
{
	ACharacter* TargetCharacter = Cast<ACharacter>(PossessedTarget);
	if (!IsValid(TargetCharacter))
	{
		return;
	}

	UFlyingMonsterMovementComponent* MovementComponent =
		TargetCharacter->FindComponentByClass<UFlyingMonsterMovementComponent>();
	UCharacterMovementComponent* CharacterMovement =
		TargetCharacter->GetCharacterMovement();

	if (!IsValid(MovementComponent) || !IsValid(CharacterMovement))
	{
		return;
	}

	PreviousFlightApproachMode = MovementComponent->GetFlightApproachMode();
	bPreviousStrafeEnabled = MovementComponent->IsStrafeEnabled();
	PreviousMaxFlySpeed = CharacterMovement->MaxFlySpeed;

	UE_LOG(LogTemp, Warning,
		TEXT("[PossessionMove] Apply | Source=%s Target=%s | StrafeBefore=%s | ApproachBefore=%s | SpeedBefore=%.1f"),
		*GetNameSafe(GetAvatarActorFromActorInfo()),
		*GetNameSafe(PossessedTarget),
		bPreviousStrafeEnabled ? TEXT("True") : TEXT("False"),
		PreviousFlightApproachMode == EMonsterFlightApproachMode::DirectTarget
			? TEXT("DirectTarget")
			: TEXT("DynamicHover"),
		PreviousMaxFlySpeed);

	MovementComponent->SetFlightApproachMode(
		EMonsterFlightApproachMode::DirectTarget);
	MovementComponent->SetStrafeEnabled(false);
	CharacterMovement->MaxFlySpeed = PreviousMaxFlySpeed * PossessedFlySpeedMultiplier;

	UE_LOG(LogTemp, Warning,
		TEXT("[PossessionMove] Applied | Target=%s | StrafeNow=%s | ApproachNow=%s | SpeedNow=%.1f"),
		*GetNameSafe(PossessedTarget),
		MovementComponent->IsStrafeEnabled() ? TEXT("True") : TEXT("False"),
		MovementComponent->GetFlightApproachMode() == EMonsterFlightApproachMode::DirectTarget
			? TEXT("DirectTarget")
			: TEXT("DynamicHover"),
		CharacterMovement->MaxFlySpeed);

	PossessedMovementComponent = MovementComponent;
	bMovementOverrideApplied = true;
}

void UGA_Possession::RestorePossessionMovementOverride()
{
	if (!bMovementOverrideApplied)
	{
		return;
	}

	if (PossessedMovementComponent.IsValid())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[PossessionMove] Restore | Target=%s | StrafeBeforeRestore=%s | StrafeAfterRestore=%s | ApproachAfterRestore=%s"),
			*GetNameSafe(PossessedTarget),
			PossessedMovementComponent->IsStrafeEnabled() ? TEXT("True") : TEXT("False"),
			bPreviousStrafeEnabled ? TEXT("True") : TEXT("False"),
			PreviousFlightApproachMode == EMonsterFlightApproachMode::DirectTarget
				? TEXT("DirectTarget")
				: TEXT("DynamicHover"));

		PossessedMovementComponent->SetFlightApproachMode(PreviousFlightApproachMode);
		PossessedMovementComponent->SetStrafeEnabled(bPreviousStrafeEnabled);
	}

	if (ACharacter* TargetCharacter = Cast<ACharacter>(PossessedTarget))
	{
		if (UCharacterMovementComponent* CharacterMovement = TargetCharacter->GetCharacterMovement())
		{
			CharacterMovement->MaxFlySpeed = PreviousMaxFlySpeed;
		}
	}

	PossessedMovementComponent = nullptr;
	PreviousMaxFlySpeed = 0.0f;
	bMovementOverrideApplied = false;
}

void UGA_Possession::BeginWaitingForSelfDestructCompleted()
{
	SelfDestructCompletedTask =
		UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this,
			GameplayTagSystem::Event_AI_SelfDestruct_Completed,
			PossessedTarget,
			false,
			true);

	if (!IsValid(SelfDestructCompletedTask))
	{
		return;
	}

	SelfDestructCompletedTask->EventReceived.AddDynamic(
		this,
		&UGA_Possession::OnSelfDestructCompleted);
	SelfDestructCompletedTask->ReadyForActivation();
}

void UGA_Possession::BeginWaitingForSelfDestructDetonate()
{
	SelfDestructDetonateTask =
		UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this,
			GameplayTagSystem::Event_AI_SelfDestruct_Detonate,
			PossessedTarget,
			false,
			true);

	if (!IsValid(SelfDestructDetonateTask))
	{
		UE_LOG(LogTemp, Error,
			TEXT("[Possession] Failed to wait for SelfDestruct.Detonate: Target=%s"),
			*GetNameSafe(PossessedTarget));
		return;
	}

	SelfDestructDetonateTask->EventReceived.AddDynamic(
		this,
		&UGA_Possession::OnSelfDestructDetonate);
	SelfDestructDetonateTask->ReadyForActivation();
}

void UGA_Possession::OnSelfDestructDetonate(FGameplayEventData Payload)
{
	UE_LOG(LogTemp, Warning,
		TEXT("[Possession] SelfDestructDetonate | Target=%s | RemovingPossessionEffect=True"),
		*GetNameSafe(PossessedTarget));

	if (PossessedTargetASC.IsValid() && AppliedEffectHandle.IsValid())
	{
		PossessedTargetASC->RemoveActiveGameplayEffect(AppliedEffectHandle);
		AppliedEffectHandle = FActiveGameplayEffectHandle();
	}
}

void UGA_Possession::TriggerSelfDestruct()
{
	UE_LOG(LogTemp, Warning,
		TEXT("[Possession] SelfDestruct timer expired: Target=%s"),
		*GetNameSafe(PossessedTarget));

	// GE가 끝나도 자폭 완료 전까지 빙의 이동 설정을 유지한다.
	// 이동 설정 원복은 OnSelfDestructCompleted 또는 EndAbility에서 처리한다.
	UE_LOG(LogTemp, Warning,
		TEXT("[PossessionMove] KeepOverrideUntilSelfDestructCompleted | Target=%s | Strafe=%s | Approach=%s"),
		*GetNameSafe(PossessedTarget),
		PossessedMovementComponent.IsValid() && PossessedMovementComponent->IsStrafeEnabled()
			? TEXT("True")
			: TEXT("False"),
		PossessedMovementComponent.IsValid()
			&& PossessedMovementComponent->GetFlightApproachMode() == EMonsterFlightApproachMode::DirectTarget
			? TEXT("DirectTarget")
			: TEXT("DynamicHover"));

	if (!IsValid(PossessedTarget))
	{
		return;
	}

	if (const ABaseCharacter* TargetCharacter = Cast<ABaseCharacter>(PossessedTarget);
		TargetCharacter && (TargetCharacter->HasStartedDeath() || TargetCharacter->HasFinishedDeath()))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Possession] SelfDestruct skipped: Target is already dead. Target=%s"),
			*GetNameSafe(PossessedTarget));
		RestorePossessionMovementOverride();
		ReappearAtTarget();
		return;
	}

	const FGameplayTag ExpiredEventTag = GameplayTagSystem::Event_AI_Possession_Expired;

	// SelfDestruct 상태의 Activate Ability By Tag Task가 읽을 다음 행동.
	if (APawn* TargetPawn = Cast<APawn>(PossessedTarget))
	{
		if (AMonsterAIController* TargetController =
			Cast<AMonsterAIController>(TargetPawn->GetController()))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[Possession] Expired: Target=%s, Action=%s, Event=%s"),
				*GetNameSafe(PossessedTarget),
				*GameplayTagSystem::Ability_Enemy_Common_SelfDestruct.GetTag().ToString(),
				*ExpiredEventTag.ToString());

			TargetController->SetSelectedAction(
				GameplayTagSystem::Ability_Enemy_Common_SelfDestruct);
			TargetController->SendAIStateTreeEvent(
				ExpiredEventTag);
		}
		else
		{
			UE_LOG(LogTemp, Error,
				TEXT("[Possession] Expired but TargetController is not AMonsterAIController: Target=%s, Controller=%s"),
				*GetNameSafe(PossessedTarget),
				*GetNameSafe(TargetPawn->GetController()));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error,
			TEXT("[Possession] Expired but PossessedTarget is not APawn: Target=%s"),
			*GetNameSafe(PossessedTarget));
	}
}

void UGA_Possession::OnSelfDestructCompleted(
	FGameplayEventData Payload)
{
	UE_LOG(LogTemp, Warning,
		TEXT("[Possession] SelfDestructCompleted | Source=%s Target=%s | Reappear=True"),
		*GetNameSafe(GetAvatarActorFromActorInfo()),
		*GetNameSafe(PossessedTarget));

	RestorePossessionMovementOverride();
	ReappearAtTarget();
}

void UGA_Possession::OnPossessedTargetDead(
	const FGameplayTag CallbackTag,
	const int32 NewCount)
{
	if (NewCount <= 0 || !bPossessionStarted || bReappearanceStarted)
	{
		return;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[Possession] Target died before self-destruct completion | Source=%s Target=%s | Reappear=True"),
		*GetNameSafe(GetAvatarActorFromActorInfo()),
		*GetNameSafe(PossessedTarget));

	RestorePossessionMovementOverride();
	ReappearAtTarget();
}

void UGA_Possession::OnPossessedTargetDeathStarted(ABaseCharacter* DeadCharacter)
{
	if (!bPossessionStarted || bReappearanceStarted || DeadCharacter != PossessedTarget)
	{
		return;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[Possession] Target death started | Source=%s Target=%s | Reappear=True"),
		*GetNameSafe(GetAvatarActorFromActorInfo()),
		*GetNameSafe(PossessedTarget));

	RestorePossessionMovementOverride();
	ReappearAtTarget();
}

void UGA_Possession::ReappearAtTarget()
{
	if (bReappearanceStarted)
	{
		return;
	}

	bReappearanceStarted = true;

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	ACharacter* AvatarCharacter =
		ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;

	if (!IsValid(AvatarCharacter))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	const FVector ReappearLocation =
		IsValid(PossessedTarget)
			? PossessedTarget->GetActorLocation()
			: FVector::ZeroVector;

	AvatarCharacter->SetActorLocation(ReappearLocation);
	if (AMonsterCharacter* MonsterCharacter = Cast<AMonsterCharacter>(AvatarCharacter))
	{
		MonsterCharacter->SetPossessionPresentationHidden(false);
	}
	AvatarCharacter->SetActorEnableCollision(true);

	if (IsValid(EmergeMontage))
	{
		EmergeMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, EmergeMontage);

		EmergeMontageTask->OnCompleted.AddDynamic(this, &UGA_Possession::OnEmergeMontageCompleted);
		EmergeMontageTask->OnBlendOut.AddDynamic(this, &UGA_Possession::OnEmergeMontageCompleted);
		EmergeMontageTask->OnInterrupted.AddDynamic(this, &UGA_Possession::OnEmergeMontageCompleted);
		EmergeMontageTask->OnCancelled.AddDynamic(this, &UGA_Possession::OnEmergeMontageCompleted);
		EmergeMontageTask->ReadyForActivation();
	}
	else
	{
		OnEmergeMontageCompleted();
	}
}

void UGA_Possession::OnEmergeMontageCompleted()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	ACharacter* AvatarCharacter =
		ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;

	if (AMonsterAIController* Controller =
		IsValid(AvatarCharacter) ? Cast<AMonsterAIController>(AvatarCharacter->GetController()) : nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Possession] EndMontageCompleted | Phantom=%s | MovementLockedBefore=%s | EndingAbility=True"),
			*GetNameSafe(AvatarCharacter),
			Controller->IsMovementLocked() ? TEXT("True") : TEXT("False"));

		Controller->SetMovementLocked(false);
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Possession::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	AActor* SourceActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	UAbilitySystemComponent* SourceASC =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(SourceActor);

	UE_LOG(LogTemp, Warning,
		TEXT("[Possession] EndAbility | Source=%s Target=%s | ActionCommittedBefore=%s | WasCancelled=%s"),
		*GetNameSafe(SourceActor),
		*GetNameSafe(PossessedTarget),
		IsValid(SourceASC) && SourceASC->HasMatchingGameplayTag(
			GameplayTagSystem::State_AI_ActionCommitted)
			? TEXT("True")
			: TEXT("False"),
		bWasCancelled ? TEXT("True") : TEXT("False"));

	const FGameplayAbilitySpec* SpecBeforeSuper =
		IsValid(SourceASC) ? SourceASC->FindAbilitySpecFromHandle(Handle) : nullptr;
	UE_LOG(LogTemp, Warning,
		TEXT("[Possession] EndAbility BeforeSuper | Source=%s | Handle=%s | SpecFound=%s | Active=%s"),
		*GetNameSafe(SourceActor),
		*Handle.ToString(),
		SpecBeforeSuper ? TEXT("true") : TEXT("false"),
		SpecBeforeSuper && SpecBeforeSuper->IsActive() ? TEXT("true") : TEXT("false"));

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ArrivalCheckTimerHandle);
		GetWorld()->GetTimerManager().ClearTimer(SelfDestructTimerHandle);
	}

	// 성공/실패/취소 어느 경로로 끝나도 선점은 반드시 푼다. 남기면 그 대상은
	// 영원히 빙의 불가가 된다.
	ReleasePossessionTarget();

	RestorePossessionMovementOverride();

	if (PossessedTargetASC.IsValid())
	{
		PossessedTargetASC->RegisterGameplayTagEvent(
			GameplayTagSystem::State_Dead,
			EGameplayTagEventType::NewOrRemoved)
			.RemoveAll(this);

		if (AppliedEffectHandle.IsValid())
		{
			PossessedTargetASC->RemoveActiveGameplayEffect(AppliedEffectHandle);

			if (FOnActiveGameplayEffectRemoved_Info* RemovedDelegate =
				PossessedTargetASC->OnGameplayEffectRemoved_InfoDelegate(AppliedEffectHandle))
			{
				RemovedDelegate->RemoveAll(this);
			}
		}

		AppliedEffectHandle = FActiveGameplayEffectHandle();
	}

	if (ABaseCharacter* TargetCharacter = Cast<ABaseCharacter>(PossessedTarget))
	{
		TargetCharacter->OnDeathStarted.RemoveAll(this);
	}

	if (ACharacter* AvatarCharacter =
		ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr)
	{
		if (bPossessionStarted)
		{
			if (AMonsterCharacter* MonsterCharacter = Cast<AMonsterCharacter>(AvatarCharacter))
			{
				MonsterCharacter->SetPossessionPresentationHidden(false);
			}
			AvatarCharacter->SetActorEnableCollision(true);
		}

		if (AMonsterAIController* Controller =
			Cast<AMonsterAIController>(AvatarCharacter->GetController()))
		{
			Controller->SetMovementLocked(false);
		}
	}

	ApproachMontageTask = nullptr;
	EmergeMontageTask = nullptr;
	SelfDestructCompletedTask = nullptr;
	SelfDestructDetonateTask = nullptr;
	PossessedTarget = nullptr;
	PossessedTargetASC = nullptr;
	AppliedEffectHandle = FActiveGameplayEffectHandle();
	bPossessionStarted = false;
	bReappearanceStarted = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

	const FGameplayAbilitySpec* SpecAfterSuper =
		IsValid(SourceASC) ? SourceASC->FindAbilitySpecFromHandle(Handle) : nullptr;
	UE_LOG(LogTemp, Warning,
		TEXT("[Possession] EndAbility AfterSuper | Source=%s | Handle=%s | SpecFound=%s | Active=%s"),
		*GetNameSafe(SourceActor),
		*Handle.ToString(),
		SpecAfterSuper ? TEXT("true") : TEXT("false"),
		SpecAfterSuper && SpecAfterSuper->IsActive() ? TEXT("true") : TEXT("false"));
}
