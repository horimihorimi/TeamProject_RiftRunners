#include "GameCore/Ability/GA_Death.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "GameCore/Interact/ReviveZoneActor.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "AbilitySystemComponent.h"
#include "GameCore/Character/BaseCharacter.h"
#include "GameCore/Tags/GameplayTagSystem.h"

UGA_Death::UGA_Death()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;

	FAbilityTriggerData TriggerData;
	TriggerData.TriggerTag = GameplayTagSystem::Event_Death;
	TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;

	AbilityTriggers.Add(TriggerData);
}

void UGA_Death::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ABaseCharacter* Character = ActorInfo ? Cast<ABaseCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;

	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;

	if (!Character || !ASC)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// 공격·이동·회피 등 다른 GA 취소 및 이동 정지
	BeginDeath(Character,ASC);

	// GA 내부 변수가 아니라 현재 캐릭터 BP에서 가져옴
	UAnimMontage* MontageToPlay = Character->GetDeathMontage();

	if (!MontageToPlay)
	{
		// 몽타주가 없어도 사망 처리는 완료
		FinishDeathAbility();
		return;
	}
	const FName SectionName = ComputeDeathDirectionSection(TriggerEventData);

	// 넉백 수치가 0보다 클 때만 밀려나는 물리(Root Motion) 효과를 적용합니다.
	if (DeathKnockbackForce > 0.0f && DeathKnockbackDuration > 0.0f)
	{
		FVector PushDirection = FVector::ZeroVector;

		if (SectionName == SectionBack)
		{
			// Back 섹션: 앞으로 날아감
			PushDirection = Character->GetActorForwardVector();
		}
		else if (SectionName == SectionFront)
		{
			// Front 섹션: 뒤로 약간 왼쪽 대각선으로 날아감
			PushDirection = -Character->GetActorForwardVector() - (Character->GetActorRightVector() * 0.5f);
		}
		else if (SectionName == SectionLeft)
		{
			PushDirection = Character->GetActorRightVector();
		}
		else if (SectionName == SectionRight)
		{
			PushDirection = -Character->GetActorRightVector();
		}

		PushDirection.Z = 0.0f;
		if (!PushDirection.IsNearlyZero())
		{
			PushDirection.Normalize();

			UAbilityTask_ApplyRootMotionConstantForce* PushTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
				this,
				FName("DeathKnockback"),
				PushDirection,
				DeathKnockbackForce,
				DeathKnockbackDuration,
				false,
				nullptr,
				ERootMotionFinishVelocityMode::SetVelocity,
				FVector::ZeroVector,
				0.0f,
				false
			);
			if (PushTask)
			{
				PushTask->ReadyForActivation();
			}
		}
	}

	PlayMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		TEXT("DeathMontage"),
		MontageToPlay,
		1.f,
		SectionName);

	if (!PlayMontageTask)
	{
		FinishDeathAbility();
		return;
	}

	PlayMontageTask->OnCompleted.AddDynamic(this, &UGA_Death::OnDeathMontageFinished);
	PlayMontageTask->OnInterrupted.AddDynamic(this, &UGA_Death::OnDeathMontageFinished);
	PlayMontageTask->OnCancelled.AddDynamic(this, &UGA_Death::OnDeathMontageFinished);

	PlayMontageTask->ReadyForActivation();
}

void UGA_Death::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (ABaseCharacter* Character = Cast<ABaseCharacter>(ActorInfo->AvatarActor.Get()))
	{
		Character->FinishDeath();

		if (Character->HasAuthority() && ReviveZoneClass)
		{
			FVector SpawnLocation = Character->GetActorLocation();
			FRotator SpawnRotation = Character->GetActorRotation();
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			if (AReviveZoneActor* SpawnedZone = GetWorld()->SpawnActor<AReviveZoneActor>(ReviveZoneClass, SpawnLocation, SpawnRotation, SpawnParams))
			{
				SpawnedZone->InitializeZone(Character);
			}
		}
	}

	PlayMontageTask = nullptr;

	Super::EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

FName UGA_Death::ComputeDeathDirectionSection(const FGameplayEventData* TriggerEventData) const
{
	if (!TriggerEventData)
	{
		return SectionFront;
	}

	const AActor* Attacker =
		Cast<AActor>(TriggerEventData->Instigator);

	const AActor* Victim = GetAvatarActorFromActorInfo();

	if (!Attacker || !Victim)
	{
		return SectionFront;
	}

	FVector HitDirection = Attacker->GetActorLocation() - Victim->GetActorLocation();

	HitDirection.Z = 0.0f;

	if (HitDirection.IsNearlyZero())
	{
		return SectionFront;
	}

	HitDirection.Normalize();

	FVector VictimForward = Victim->GetActorForwardVector();
	FVector VictimRight = Victim->GetActorRightVector();

	VictimForward.Z = 0.0f;
	VictimRight.Z = 0.0f;

	VictimForward.Normalize();
	VictimRight.Normalize();

	const float ForwardDot = FVector::DotProduct(VictimForward, HitDirection);

	const float RightDot = FVector::DotProduct(VictimRight, HitDirection);

	if (ForwardDot >= 0.5f)
	{
		return SectionFront;
	}

	if (ForwardDot <= -0.5f)
	{
		return SectionBack;
	}

	return RightDot > 0.0f
		? SectionRight
		: SectionLeft;
}

void UGA_Death::BeginDeath(ABaseCharacter* Character, UAbilitySystemComponent* ASC)
{
	// DeathEffect의 State.Dead 부여 실패/누락과 무관하게 사망 시작을 확정한다.
	// State.Dead 태그 콜백도 BeginDeath를 호출하지만, 캐릭터 쪽 가드가 중복 실행을 막는다.
	Character->BeginDeath();

	ASC->CancelAllAbilities(this);

	if (DeathEffectClass)
	{
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddInstigator(Character, Character);
		FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(DeathEffectClass, 1.0f, Context);
		if (SpecHandle.IsValid())
		{
			ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		}
	}

	if (Character->IsLocallyControlled())
	{
		// 이전에 사용하던 DeathScreen 위젯 팝업 호출 제거 (현재는 DamageOverlay로 대체됨)
	}
}

void UGA_Death::OnDeathMontageFinished()
{
	FinishDeathAbility();
}

void UGA_Death::FinishDeathAbility()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
