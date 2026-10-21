#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "DashPatternTypes.h"
#include "GA_Dash.generated.h"

class AActor;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAbilityTask_FollowDashSpline;

/**
 * Action.Dash 태그로 발동되는 대시 공격.
 *
 * 몽타주만 재생하고 대기한다. 몽타주 안의 노티파이가 Event.Dash.FollowSpline을
 * 보내면 UGA_DashFollowSpline이 알아서 끼어들어 스플라인 이동을 담당한다
 * (이 Ability의 책임이 아니다).
 * 활성화 중 State.AI.ActionCommitted 태그를 소유자에게 부여해서, 이동
 * 레이어(AMonsterAIController::UpdateMovement)가 이 동안 멈추게 한다.
 */
UCLASS()
class TEAMPROJECT_FIN_API UGA_Dash : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Dash();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

protected:
	/** Distance-filtered and weighted monster dash patterns. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash")
	TArray<FDashPatternChoice> DashPatterns;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash|Movement",
		meta = (ClampMin = "0.0"))
	float RotationInterpSpeed = 12.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash|Debug")
	bool bDebugDrawDash = false;

private:
	const FDashPatternChoice* ChooseDashPattern(
		const AActor* AvatarActor,
		const AActor* TargetActor) const;

	void StartDashMovement();

	UFUNCTION()
	void OnDashMovementRequested(FGameplayEventData Payload);

	UFUNCTION()
	void OnDashMovementFinished();

	UFUNCTION()
	void OnMontageCompleted();

	UFUNCTION()
	void OnMontageInterrupted();

	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> PlayMontageTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitDashEventTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_FollowDashSpline> DashMovementTask;

	UPROPERTY()
	TObjectPtr<AActor> DashTargetActor;

	FDashPatternChoice ActiveDashPattern;
	bool bHasActiveDashPattern = false;
	bool bDashMovementStarted = false;
};
