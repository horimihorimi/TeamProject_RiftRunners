// Copyright Epic Games, Inc. All Rights Reserved.

#include "GA_HitReact.h"
#include "GameFramework/Actor.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "../Tags/GameplayTagSystem.h"

UGA_HitReact::UGA_HitReact()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
	
	// 1. C++에서 강제로 트리거 등록 (블루프린트에서 세팅할 필요 없게 만듦)
	FAbilityTriggerData TriggerData;
	TriggerData.TriggerTag = GameplayTagSystem::Event_HitReact;
	TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(TriggerData);
	
	// 2. 어빌리티가 활성화된 동안 이 태그들을 소유함 (이동/회전 불가 상태)
	ActivationOwnedTags.AddTag(GameplayTagSystem::State_Groggy);

	// 무적 중에는 피격 반응 Ability도 활성화하지 않는다.
	ActivationBlockedTags.AddTag(GameplayTagSystem::State_Invincible);

	// 2. 피격 시 현재 진행 중인 행동들을 즉시 취소시킴
	CancelAbilitiesWithTag.AddTag(GameplayTagSystem::Ability_Weapon_Fire);
	CancelAbilitiesWithTag.AddTag(GameplayTagSystem::Ability_Weapon_Aim);
	CancelAbilitiesWithTag.AddTag(GameplayTagSystem::Ability_Weapon_Reload);
	CancelAbilitiesWithTag.AddTag(GameplayTagSystem::Ability_Movement_Dodge);
	CancelAbilitiesWithTag.AddTag(GameplayTagSystem::Ability_Movement_Sprint);
	
	// 3. 피격 도중에는 다른 행동들을 할 수 없도록 차단함
	BlockAbilitiesWithTag.AddTag(GameplayTagSystem::Ability_Weapon_Fire);
	BlockAbilitiesWithTag.AddTag(GameplayTagSystem::Ability_Weapon_Aim);
	BlockAbilitiesWithTag.AddTag(GameplayTagSystem::Ability_Weapon_Reload);
	BlockAbilitiesWithTag.AddTag(GameplayTagSystem::Ability_Movement_Dodge);
	BlockAbilitiesWithTag.AddTag(GameplayTagSystem::Ability_Movement_Sprint);
}

void UGA_HitReact::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bool bIsHeavy = TriggerEventData->EventTag.MatchesTagExact(GameplayTagSystem::Event_HitReact_Heavy);
	UAnimMontage* MontageToPlay = bIsHeavy ? HeavyHitReactMontage : HitReactMontage;

	if (!MontageToPlay)
	{
		UE_LOG(LogTemp, Error, TEXT("[GA_HitReact] 재생할 몽타주(HitReactMontage 또는 HeavyHitReactMontage)가 할당되지 않음! 어빌리티 강제 종료."));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 1. 피격 방향에 따른 몽타주 섹션 계산
	FName SectionName = ComputeHitDirectionSection(TriggerEventData);

	// 2. 강한 그로기일 경우 넉백(Pushback) 물리력 적용
	if (bIsHeavy)
	{
		AActor* Attacker = TriggerEventData->Instigator ? const_cast<AActor*>(Cast<AActor>(TriggerEventData->Instigator)) : nullptr;
		AActor* Victim = GetAvatarActorFromActorInfo();

		if (Victim)
		{
			FVector PushDirection;
			if (Attacker)
			{
				// 공격자가 있다면: 공격자로부터 멀어지는 방향(넉백)
				PushDirection = Victim->GetActorLocation() - Attacker->GetActorLocation();
			}
			else
			{
				// 공격자 정보가 없다면: 일단 캐릭터가 바라보는 방향의 반대(뒤)로 밀려남
				PushDirection = -Victim->GetActorForwardVector();
			}
			
			PushDirection.Z = 0.0f;
			PushDirection.Normalize();

			UAbilityTask_ApplyRootMotionConstantForce* PushTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
				this,
				FName("HeavyKnockback"),
				PushDirection,
				KnockbackForce,
				KnockbackDuration,
				false,
				nullptr,
				ERootMotionFinishVelocityMode::SetVelocity,
				FVector::ZeroVector,
				0.0f,
				false
			);
			PushTask->ReadyForActivation();
		}
	}

	// 3. 몽타주 재생 태스크 생성 및 섹션 지정
	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		FName("HitReactMontageTask"),
		MontageToPlay,
		1.0f,
		SectionName
	);

	// 4. 콜백 델리게이트 바인딩 (끝나면 어빌리티 자동 종료)
	MontageTask->OnCompleted.AddDynamic(this, &UGA_HitReact::OnMontageCompleted);
	MontageTask->OnBlendOut.AddDynamic(this, &UGA_HitReact::OnMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &UGA_HitReact::OnMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &UGA_HitReact::OnMontageInterrupted);

	MontageTask->ReadyForActivation();
}

void UGA_HitReact::OnMontageCompleted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_HitReact::OnMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

FName UGA_HitReact::ComputeHitDirectionSection(const FGameplayEventData* TriggerEventData)
{
	if (!TriggerEventData)
	{
		return SectionFront;
	}

	const AActor* Attacker = Cast<AActor>(TriggerEventData->Instigator);
	const AActor* Victim = GetAvatarActorFromActorInfo();

	// 공격자나 피해자 정보가 유효하지 않으면 기본값(정면) 반환
	if (!Attacker || !Victim)
	{
		return SectionFront;
	}

	const FVector AttackerLocation = Attacker->GetActorLocation();
	const FVector VictimLocation = Victim->GetActorLocation();

	// Z축(높이) 차이를 무시한 평면 상의 방향 벡터 계산
	FVector HitDirection = (AttackerLocation - VictimLocation);
	HitDirection.Z = 0.0f;
	
	if (HitDirection.IsNearlyZero())
	{
		return SectionFront;
	}

	HitDirection.Normalize();

	// 캐릭터의 전방 및 우측 기준 벡터
	const FVector VictimForward = Victim->GetActorForwardVector();
	const FVector VictimRight = Victim->GetActorRightVector();

	// 내적(Dot Product)을 통해 각도 산출
	const float ForwardDot = FVector::DotProduct(VictimForward, HitDirection);
	const float RightDot = FVector::DotProduct(VictimRight, HitDirection);

	// 4방향 판정 (코사인 값 기준: 0.5 = 60도)
	// 전방 120도, 후방 120도, 좌우 각각 60도 범위 할당
	if (ForwardDot >= 0.5f)
	{
		return SectionFront;
	}
	else if (ForwardDot <= -0.5f)
	{
		return SectionBack;
	}
	else
	{
		// 측면 판정
		if (RightDot > 0.0f)
		{
			return SectionRight;
		}
		else
		{
			return SectionLeft;
		}
	}
}
