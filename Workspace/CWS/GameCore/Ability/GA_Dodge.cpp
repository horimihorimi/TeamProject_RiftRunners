#include "GA_Dodge.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "GameFramework/RootMotionSource.h"
#include "../Character/PlayerCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "../Tags/GameplayTagSystem.h"
#include "../Attribute/CharacterAttributeSet.h"

UGA_Dodge::UGA_Dodge()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	DodgeForceStrength = 800.0f;
	DodgeDuration = 0.6f;
	FGameplayTagContainer DodgeAbilityTags = GetAssetTags();
	DodgeAbilityTags.AddTag(GameplayTagSystem::Ability_Movement_Dodge);
	SetAssetTags(DodgeAbilityTags);
	ActivationOwnedTags.AddTag(GameplayTagSystem::State_Movement_Dodging);
	ActivationBlockedTags.AddTag(GameplayTagSystem::State_Dead);
	ActivationBlockedTags.AddTag(GameplayTagSystem::State_Interacting);
}

void UGA_Dodge::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// 1. 코스트 검사 (스태미나가 부족하면 발동 안됨)
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	if (!Character)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 2. 구르는 동안 스태미나 회복 정지 및 이동/회전 방지 태그 부여
	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (ASC)
	{
		FGameplayTagContainer FireTags;
		FireTags.AddTag(
			GameplayTagSystem::Ability_Weapon_Fire
		);

		ASC->CancelAbilities(&FireTags);
		ASC->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("Cooldown.StaminaRegen")));
	}

	// 2.5 구를 때 다이내믹 카메라 래그 적용 (BP 설정에 따라 속도 조절)
	if (bEnableDodgeCameraLag)
	{
		APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(Character);
		if (PlayerChar && PlayerChar->GetCameraBoom())
		{
			USpringArmComponent* CameraBoom = PlayerChar->GetCameraBoom();
			
			// 원래 상태 보존
			bWasCameraLagEnabled = CameraBoom->bEnableCameraLag;
			bWasCameraRotationLagEnabled = CameraBoom->bEnableCameraRotationLag;
			OriginalCameraLagSpeed = CameraBoom->CameraLagSpeed;
			OriginalCameraRotationLagSpeed = CameraBoom->CameraRotationLagSpeed;

			// 구를 때 쫀득하게 카메라가 따라오도록 강제 활성화 및 속도 늦춤
			CameraBoom->bEnableCameraLag = true;
			CameraBoom->bEnableCameraRotationLag = true;
			CameraBoom->CameraLagSpeed = DodgeCameraLagSpeed;
			CameraBoom->CameraRotationLagSpeed = DodgeCameraRotationLagSpeed;
		}
	}

	UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement();
	if (MoveComp)
	{
		bWasOrientRotationToMovement = MoveComp->bOrientRotationToMovement;
		bWasUseControllerDesiredRotation = MoveComp->bUseControllerDesiredRotation;

		// 구르는 동안 방향키나 마우스에 의해 캐릭터가 제멋대로 도는 것을 방지
		MoveComp->bOrientRotationToMovement = false;
		MoveComp->bUseControllerDesiredRotation = false;
	}

	// 3. 방향 계산 (서버와 클라이언트 동기화를 위해 캐릭터가 저장/전달해둔 완벽한 입력 방향 사용)
	FVector InputVector = FVector::ZeroVector;
	APlayerCharacter* PlayerCharForInput = Cast<APlayerCharacter>(Character);
	if (PlayerCharForInput)
	{
		InputVector = PlayerCharForInput->ReplicatedDodgeDirection;
	}
	else
	{
		InputVector = Character->GetCharacterMovement()->GetLastInputVector();
	}

	FRotator ControlRot = Character->GetControlRotation();
	FVector CamForward = FRotationMatrix(FRotator(0.0f, ControlRot.Yaw, 0.0f)).GetUnitAxis(EAxis::X);
	FVector CamRight = FRotationMatrix(FRotator(0.0f, ControlRot.Yaw, 0.0f)).GetUnitAxis(EAxis::Y);

	if (InputVector.IsNearlyZero())
	{
		// 입력이 없으면 기본적으로 뒤로 구르기 (카메라 기준 후방)
		InputVector = -CamForward;
	}
	else
	{
		InputVector.Normalize();
	}

	float DotF = FVector::DotProduct(InputVector, CamForward);
	float DotR = FVector::DotProduct(InputVector, CamRight);

	FName SectionName = NAME_None;
	FVector DodgeDirection = InputVector;

	// 몽타주 재생 및 루트모션 적용 전, 구르기 방향과 몽타주 방향이 일치하도록 캐릭터를 카메라가 바라보는 방향으로 우선 정렬합니다.
	FRotator CamRotation = CamForward.Rotation();
	Character->SetActorRotation(FRotator(0.0f, CamRotation.Yaw, 0.0f));

	// 뒤로 구르기: 카메라는 앞을 보고, 캐릭터는 즉시 뒤를 돈 후 전방 구르기 몽타주 재생
	if (DotF < -0.5f && FMath::Abs(DotR) < 0.5f)
	{
		// 캐릭터가 카메라 뒤쪽(입력 방향)을 바라보도록 즉시 회전
		Character->SetActorRotation(FRotator(0.0f, InputVector.Rotation().Yaw, 0.0f));
		SectionName = FName("Forward");
	}
	// 대각선 후방의 경우 (입력이 뒤+좌우)
	else if (DotF < -0.1f)
	{
		Character->SetActorRotation(FRotator(0.0f, InputVector.Rotation().Yaw, 0.0f));
		SectionName = FName("Forward");
	}
	// 전방 구르기
	else if (DotF > 0.5f && FMath::Abs(DotR) < 0.5f)
	{
		SectionName = FName("Forward");
	}
	// 우측 구르기
	else if (DotR > 0.5f && FMath::Abs(DotF) < 0.5f)
	{
		SectionName = FName("Right");
	}
	// 좌측 구르기
	else if (DotR < -0.5f && FMath::Abs(DotF) < 0.5f)
	{
		SectionName = FName("Left");
	}
	// 우전방 구르기
	else if (DotF > 0.1f && DotR > 0.1f)
	{
		SectionName = FName("ForwardRight");
	}
	// 좌전방 구르기
	else if (DotF > 0.1f && DotR < -0.1f)
	{
		SectionName = FName("ForwardLeft");
	}
	else
	{
		// Fallback
		SectionName = FName("Forward");
	}

	if (!DodgeMontage || SectionName.IsNone())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 4. 몽타주 재생 태스크 (지정된 섹션부터 재생)
	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, 
		NAME_None, 
		DodgeMontage, 
		1.0f, // 재생 속도
		SectionName, // 시작할 섹션 이름
		true, // 어빌리티 종료 시 몽타주도 정리
		1.0f,
		0.0f,
		true // 블렌드아웃 이후 중단되어도 종료 콜백을 받는다
	);
	if (!MontageTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	MontageTask->OnCompleted.AddDynamic(this, &UGA_Dodge::OnDodgeFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &UGA_Dodge::OnDodgeInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &UGA_Dodge::OnDodgeInterrupted);
	MontageTask->ReadyForActivation();

	// 재생 실패나 중단 콜백이 동기적으로 종료한 경우 루트모션을 새로 만들지 않는다.
	if (!IsActive())
	{
		return;
	}

	// 5. 인플레이스 애니메이션을 위해 루트 모션 포스(Force) 적용
	RootMotionTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
		this, 
		NAME_None, 
		DodgeDirection, 
		DodgeForceStrength, 
		DodgeDuration, // 유저님이 수동으로 튜닝(예: 0.77)할 수 있도록 원상복구
		false, // IsAdditive
		DodgeForceCurve, // [중요] 뚝 끊기는 현상을 막으려면 블루프린트에서 이 커브를 꼭 지정해야 합니다!
		ERootMotionFinishVelocityMode::MaintainLastRootMotionVelocity, // 감속 커브의 마지막 속도와 낙하/바람의 수직 속도를 유지한다.
		FVector::ZeroVector, // SetVelocity 모드가 아니므로 종료 속도를 강제로 지정하지 않는다.
		0.0f, 
		true // 중력(Gravity) 받도록 수정!
	);
	
	if (RootMotionTask)
	{
		RootMotionTask->ReadyForActivation();
	}
}

void UGA_Dodge::OnDodgeFinished()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UGA_Dodge::OnDodgeInterrupted()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
}

void UGA_Dodge::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 취소 델리게이트와 CancelAbility가 연달아 종료를 요청해도 한 번만 정리한다.
	if (!IsEndAbilityValid(Handle, ActorInfo))
	{
		return;
	}

	// 어빌리티 종료 시 Force 제거 (몽타주 도중 끊겼을 때 대비)
	if (RootMotionTask)
	{
		RootMotionTask->EndTask();
		RootMotionTask = nullptr;
	}

	// 카메라 래그 원래 상태로 복구 및 회전 잠금 해제
	APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(ActorInfo->AvatarActor.Get());
	if (PlayerChar)
	{
		if (bEnableDodgeCameraLag && PlayerChar->GetCameraBoom())
		{
			USpringArmComponent* CameraBoom = PlayerChar->GetCameraBoom();
			CameraBoom->bEnableCameraLag = bWasCameraLagEnabled;
			CameraBoom->bEnableCameraRotationLag = bWasCameraRotationLagEnabled;
			CameraBoom->CameraLagSpeed = OriginalCameraLagSpeed;
			CameraBoom->CameraRotationLagSpeed = OriginalCameraRotationLagSpeed;
		}

		UCharacterMovementComponent* MoveComp = PlayerChar->GetCharacterMovement();
		if (MoveComp)
		{
			MoveComp->bOrientRotationToMovement = bWasOrientRotationToMovement;
			MoveComp->bUseControllerDesiredRotation = bWasUseControllerDesiredRotation;
		}
	}

	// 상태 태그(회복 정지, 구르기 중) 해제
	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (ASC)
	{
		ASC->RemoveLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("Cooldown.StaminaRegen")));
		
		// 구르기가 온전히 '끝난 직후'부터 스태미나 회복 딜레이(Pause) 발동
		if (!bWasCancelled && DodgeRegenPauseEffectClass)
		{
			FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
			ContextHandle.AddInstigator(ActorInfo->AvatarActor.Get(), ActorInfo->AvatarActor.Get());
			FGameplayEffectSpecHandle PauseSpecHandle = ASC->MakeOutgoingSpec(DodgeRegenPauseEffectClass, GetAbilityLevel(), ContextHandle);
			if (PauseSpecHandle.IsValid())
			{
				ASC->ApplyGameplayEffectSpecToSelf(*PauseSpecHandle.Data.Get());
			}
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

bool UGA_Dodge::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	// 1. 기본 비용(GE) 검사 통과 여부 확인
	if (!Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags))
	{
		return false;
	}

	// 2. 현재 스태미나가 0 이하인지 명시적으로 확인
	if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
	{
		if (const UCharacterAttributeSet* Attr = Cast<UCharacterAttributeSet>(ActorInfo->AbilitySystemComponent->GetAttributeSet(UCharacterAttributeSet::StaticClass())))
		{
			if (Attr->GetStamina() <= 0.0f)
			{
				return false;
			}
		}
	}

	return true;
}
