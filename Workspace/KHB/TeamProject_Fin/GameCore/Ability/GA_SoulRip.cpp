#include "GA_SoulRip.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameCore/AI/MonsterAIController.h"
#include "GameCore/Ability/DamageSystemLibrary.h"
#include "GameCore/Combat/MonsterDamageLibrary.h"
#include "GameCore/Tags/GameplayTagSystem.h"
#include "GameFramework/Character.h"
#include "MotionWarpingComponent.h"

UGA_SoulRip::UGA_SoulRip()
{
	// 식별은 Asset Tags의 Ability.Enemy.* 로, 데미지 표는 DefaultDamageAttackTag의 Action.AI.* 로 찾는다.
	DefaultDamageAttackTag = GameplayTagSystem::Action_AI_SoulRip;
	SetAssetTags(FGameplayTagContainer(GameplayTagSystem::Ability_Enemy_Melee_SoulRip));

	// 잡기 연출은 대상과 물려 있어 중간에 끊기면 안 된다. 경직을 무시한다.
	ActivationOwnedTags.AddTag(GameplayTagSystem::State_AI_SuperArmor);

	// 쿨다운은 태그로 막는다. 태그를 붙이는 시점은 EndAbility다.
	ActivationBlockedTags.AddTag(GameplayTagSystem::Cooldown_AI_SoulRip);

	SoulEffigyCueTag = GameplayTagSystem::GameplayCue_AI_SoulRip;
}

void UGA_SoulRip::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	/*
	 * 워프 타깃은 몽타주보다 먼저 등록해야 첫 프레임부터 워핑이 걸린다.
	 * 부모가 몽타주를 재생하므로 부모 호출 전에 등록한다.
	 */
	RegisterWarpTarget();

	// 몽타주 재생, 공격 태그 기록, 몽타주 종료 시 어빌리티 종료는 부모가 맡는다.
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive())
	{
		return;
	}

	bGrabbed = false;
	bDamageApplied = false;
	bEffigyCueActive = false;
	CaughtTarget = nullptr;

	BreakSelfLoopingSections();

	/*
	 * 잡기 판정과 뽑기 시점은 노티파이가 알려준다. 어빌리티가 시간을 재지
	 * 않는 이유는, 몽타주를 고치면 그 시간이 바로 어긋나기 때문이다.
	 */
	CaughtEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, GameplayTagSystem::Event_AI_SoulRip_Caught, nullptr, false, true);
	if (IsValid(CaughtEventTask))
	{
		CaughtEventTask->EventReceived.AddDynamic(this, &UGA_SoulRip::OnCaughtEvent);
		CaughtEventTask->ReadyForActivation();
	}

	YankEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, GameplayTagSystem::Event_AI_SoulRip_Yank, nullptr, false, true);
	if (IsValid(YankEventTask))
	{
		YankEventTask->EventReceived.AddDynamic(this, &UGA_SoulRip::OnYankEvent);
		YankEventTask->ReadyForActivation();
	}

	if (MaxAbilityDuration > 0.0f)
	{
		MaxDurationTask = UAbilityTask_WaitDelay::WaitDelay(this, MaxAbilityDuration);
		if (IsValid(MaxDurationTask))
		{
			MaxDurationTask->OnFinish.AddDynamic(this, &UGA_SoulRip::OnMaxDurationReached);
			MaxDurationTask->ReadyForActivation();
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[SoulRip] Activated | Avatar=%s"), *GetNameSafe(GetAvatarActorFromActorInfo()));
}

void UGA_SoulRip::RegisterWarpTarget() const
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	const AActor* TargetActor = GetCombatTarget();
	UMotionWarpingComponent* WarpComp = IsValid(AvatarActor)
		? AvatarActor->FindComponentByClass<UMotionWarpingComponent>()
		: nullptr;
	if (!IsValid(WarpComp) || !IsValid(TargetActor))
	{
		UE_LOG(LogTemp, Warning, TEXT("[SoulRip] WarpSkipped | Avatar=%s | WarpComp=%s | Target=%s"),
			*GetNameSafe(AvatarActor), *GetNameSafe(WarpComp), *GetNameSafe(TargetActor));
		return;
	}

	// 발동 순간의 위치로 고정한다. 플레이어에서 몬스터 쪽으로 거리만큼 물러난 지점이고, 플레이어를 바라본다.
	const FVector TargetLocation = TargetActor->GetActorLocation();
	FVector TowardOwner = AvatarActor->GetActorLocation() - TargetLocation;
	TowardOwner.Z = 0.0f;
	TowardOwner = TowardOwner.GetSafeNormal();
	if (TowardOwner.IsNearlyZero())
	{
		TowardOwner = -AvatarActor->GetActorForwardVector().GetSafeNormal2D();
	}

	FVector WarpLocation = TargetLocation + TowardOwner * WarpOffsetDistance;
	WarpLocation.Z = TargetLocation.Z + WarpHeightOffset;
	const FRotator WarpRotation = (-TowardOwner).Rotation();
	WarpComp->AddOrUpdateWarpTargetFromLocationAndRotation(WarpTargetName, WarpLocation, WarpRotation);

	UE_LOG(LogTemp, Warning, TEXT("[SoulRip] WarpTarget | Avatar=%s | Target=%s | From=%s | To=%s"),
		*GetNameSafe(AvatarActor), *GetNameSafe(TargetActor),
		*AvatarActor->GetActorLocation().ToCompactString(), *WarpLocation.ToCompactString());
}

void UGA_SoulRip::BreakSelfLoopingSections() const
{
	const ACharacter* AvatarCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	USkeletalMeshComponent* Mesh = IsValid(AvatarCharacter) ? AvatarCharacter->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = IsValid(Mesh) ? Mesh->GetAnimInstance() : nullptr;
	if (!IsValid(AnimInstance) || !IsValid(PatternMontage))
	{
		return;
	}

	for (const FCompositeSection& Section : PatternMontage->CompositeSections)
	{
		if (Section.NextSectionName == Section.SectionName)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[SoulRip] SelfLoopBroken | Montage=%s | Section=%s | 반복 섹션이 있으면 몽타주가 끝나지 않는다."),
				*GetNameSafe(PatternMontage), *Section.SectionName.ToString());
			AnimInstance->Montage_SetNextSection(Section.SectionName, NAME_None, PatternMontage);
		}
	}
}

void UGA_SoulRip::OnMaxDurationReached()
{
	MaxDurationTask = nullptr;
	if (!IsActive())
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[SoulRip] MaxDurationReached | Avatar=%s | Duration=%.2f | 몽타주가 끝나지 않아 강제로 종료한다."),
		*GetNameSafe(GetAvatarActorFromActorInfo()), MaxAbilityDuration);
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_SoulRip::OnCaughtEvent(FGameplayEventData Payload)
{
	// 윈도우가 여러 틱 열려 있으므로 이벤트는 여러 번 올 수 있다. 첫 번째만 받는다.
	if (bGrabbed)
	{
		return;
	}

	AActor* HitTarget = const_cast<AActor*>(Payload.Target.Get());
	if (!IsValid(HitTarget))
	{
		return;
	}

	bGrabbed = true;
	CaughtTarget = HitTarget;

	/*
	 * 영혼 복제본은 연출이라 각 클라이언트가 로컬에서 만든다. 서버가 액터를
	 * 스폰해 복제하면 지연 때문에 손에서 어긋나 보인다.
	 *
	 * 어떤 캐릭터를 본뜰지는 큐 파라미터로 넘긴다.
	 */
	UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo();
	if (IsValid(AbilitySystemComponent) && SoulEffigyCueTag.IsValid())
	{
		FGameplayCueParameters CueParameters;
		CueParameters.Instigator = GetAvatarActorFromActorInfo();
		CueParameters.EffectCauser = CaughtTarget;
		CueParameters.SourceObject = CaughtTarget;

		AbilitySystemComponent->AddGameplayCue(SoulEffigyCueTag, CueParameters);
		bEffigyCueActive = true;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[SoulRip] Caught | Avatar=%s | Target=%s"),
		*GetNameSafe(GetAvatarActorFromActorInfo()),
		*GetNameSafe(CaughtTarget));
}

void UGA_SoulRip::OnYankEvent(FGameplayEventData Payload)
{
	/*
	 * 못 잡았을 때도 이 프레임은 지나간다. 같은 구간을 재생하기 때문이다.
	 * 잡은 적이 없으면 뽑을 영혼도 없으므로 데미지 없이 넘어간다.
	 */
	if (!bGrabbed || bDamageApplied)
	{
		return;
	}

	bDamageApplied = true;
	ApplyRipDamage();
	RemoveSoulEffigyCue();
}

void UGA_SoulRip::ApplyRipDamage()
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActor) || !IsValid(CaughtTarget))
	{
		return;
	}

	/*
	 * 잡힌 뒤에는 거리를 다시 보지 않는다. 카운터플레이는 잡히기 전에
	 * 회피하는 것이었고, 여기서 또 조건을 걸면 규칙이 흐려진다.
	 */
	UAbilitySystemComponent* SourceASC =
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(AvatarActor);
	UAbilitySystemComponent* TargetASC =
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(CaughtTarget);
	if (!IsValid(SourceASC) || !IsValid(TargetASC))
	{
		return;
	}

	// 위력은 위력표의 Action.AI.SoulRip 행이 정한다. 부모가 발동 때 아바타에 적어 둔 태그다.
	const FDamagePayload Payload = MonsterDamageLibrary::MakeAttackPayload(
		MonsterDamageLibrary::ResolveAttackTag(AvatarActor),
		DamageEffectClass);

	UDamageSystemLibrary::LogDamageApplied(AvatarActor, CaughtTarget, TEXT("SoulRip"));
	UDamageSystemLibrary::ApplyDamageToTarget(AvatarActor, SourceASC, CaughtTarget, TargetASC, Payload);

	UE_LOG(LogTemp, Warning,
		TEXT("[SoulRip] Yank | Avatar=%s | Target=%s"),
		*GetNameSafe(AvatarActor),
		*GetNameSafe(CaughtTarget));
}

AActor* UGA_SoulRip::GetCombatTarget() const
{
	const ACharacter* AvatarCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const AMonsterAIController* Controller = IsValid(AvatarCharacter)
		? Cast<AMonsterAIController>(AvatarCharacter->GetController())
		: nullptr;
	return IsValid(Controller) ? Controller->GetPerceivedTarget() : nullptr;
}

void UGA_SoulRip::RemoveSoulEffigyCue()
{
	if (!bEffigyCueActive)
	{
		return;
	}

	bEffigyCueActive = false;

	UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo();
	if (IsValid(AbilitySystemComponent) && SoulEffigyCueTag.IsValid())
	{
		AbilitySystemComponent->RemoveGameplayCue(SoulEffigyCueTag);
	}
}

void UGA_SoulRip::ApplyCooldownEffect()
{
	UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo();
	if (!IsValid(CooldownEffectClass) || !IsValid(AbilitySystemComponent))
	{
		return;
	}

	const FGameplayEffectSpecHandle SpecHandle =
		MakeOutgoingGameplayEffectSpec(CooldownEffectClass, GetAbilityLevel());
	if (SpecHandle.IsValid())
	{
		AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}
}

void UGA_SoulRip::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// 몽타주 종료와 안전장치가 겹치면 두 번 들어올 수 있다. 쿨다운이 두 번 걸리지 않게 한 번만 정리한다.
	if (!IsEndAbilityValid(Handle, ActorInfo))
	{
		return;
	}

	/*
	 * 경직으로 끊기면 뽑기 노티파이가 오지 않아 큐가 남는다. 남은 영혼이
	 * 허공에 떠 있게 되므로 어떤 경로로 끝나든 여기서 지운다.
	 */
	RemoveSoulEffigyCue();

	if (IsValid(MaxDurationTask))
	{
		MaxDurationTask->EndTask();
		MaxDurationTask = nullptr;
	}

	/*
	 * 쿨다운은 패턴이 끝난 시점부터 센다. 커밋 시점에 걸면 모션 길이만큼
	 * 실제 재사용 간격이 짧아지고, 애님을 고칠 때마다 조용히 달라진다.
	 */
	if (!bWasCancelled || bApplyCooldownOnCancel)
	{
		ApplyCooldownEffect();
	}

	CaughtEventTask = nullptr;
	YankEventTask = nullptr;
	CaughtTarget = nullptr;
	bGrabbed = false;
	bDamageApplied = false;

	// 공격 태그 정리, 몽타주 정리는 부모가 맡는다.
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
