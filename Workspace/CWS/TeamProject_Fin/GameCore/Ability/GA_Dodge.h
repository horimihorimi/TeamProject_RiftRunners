#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_Dodge.generated.h"

/**
 * 인플레이스 구르기(Dodge) 상태를 제어하는 Gameplay Ability
 * 활성화 시 스태미나 코스트(BP_GE_StaminaCost)를 지불하고 지정된 방향의 몽타주를 재생하며 캐릭터를 물리적으로 밀어줍니다.
 */
UCLASS()
class TEAMPROJECT_FIN_API UGA_Dodge : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Dodge();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	/** 구르기에 사용할 통합 몽타주 (내부 섹션으로 방향 구분) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dodge|Montages")
	UAnimMontage* DodgeMontage;

	/** 인플레이스 애니메이션이므로, 캐릭터를 해당 방향으로 밀어줄 힘(속도) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dodge|Movement")
	float DodgeForceStrength;

	/** 인플레이스 몽타주 재생 중 캐릭터를 밀어줄 지속 시간 (보통 애니메이션 길이에 맞춤) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dodge|Movement")
	float DodgeDuration;

	/** (선택사항) 구르는 동안 속도를 시간에 따라 조절하는 커브. 
	 * 1.0(시작) -> 0.0(끝)으로 부드럽게 떨어지는 커브를 넣으면 미끄러짐/급정거가 완벽히 해결됩니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dodge|Movement")
	class UCurveFloat* DodgeForceCurve;

	/** 구를 때 카메라 래그(지연)를 강제로 켤지 여부 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dodge|Camera")
	bool bEnableDodgeCameraLag = true;

	/** 구를 때 카메라 위치 추적 속도 (낮을수록 묵직하게 늦게 따라옴. 추천: 3~5) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dodge|Camera", meta = (EditCondition = "bEnableDodgeCameraLag"))
	float DodgeCameraLagSpeed = 5.0f;

	/** 구를 때 카메라 회전 추적 속도 (추천: 3~5) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dodge|Camera", meta = (EditCondition = "bEnableDodgeCameraLag"))
	float DodgeCameraRotationLagSpeed = 5.0f;

	/** 스태미나 회복 지연 이펙트 (구르기 직후 발동할 0.8초 딜레이) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dodge|Cost")
	TSubclassOf<class UGameplayEffect> DodgeRegenPauseEffectClass;

private:
	UFUNCTION()
	void OnDodgeFinished();

	UFUNCTION()
	void OnDodgeInterrupted();

	/** 어빌리티 종료 시 적용된 Root Motion을 해제하기 위해 보관 */
	UPROPERTY()
	class UAbilityTask_ApplyRootMotionConstantForce* RootMotionTask;

	/** 구르기 시작 전 원래 카메라 래그 상태 저장용 */
	bool bWasCameraLagEnabled = false;
	bool bWasCameraRotationLagEnabled = false;
	float OriginalCameraLagSpeed = 10.0f;
	float OriginalCameraRotationLagSpeed = 10.0f;

	/** 구르기 시작 전 원래 회전 모드 저장용 */
	bool bWasOrientRotationToMovement = false;
	bool bWasUseControllerDesiredRotation = false;
};
