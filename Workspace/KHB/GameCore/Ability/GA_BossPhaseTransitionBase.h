#pragma once

#include "Abilities/GameplayAbility.h"
#include "GA_BossPhaseTransitionBase.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAnimMontage;
class UBossMovementComponent;
class UMusicParameterEvent;
class USoundBase;

/**
 * 보스 페이즈 전환의 공용 골격이다.
 * 전환 음악 요청, 재배치, 앵커 Offset 이동, 이동 완료 후 섹션 전환, 몽타주 종료를 담당한다.
 */
UCLASS(Abstract, Blueprintable)
class TEAMPROJECT_FIN_API UGA_BossPhaseTransitionBase : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_BossPhaseTransitionBase();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase Transition") TObjectPtr<UAnimMontage> TransitionMontage;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase Transition", meta = (ClampMin = "0.01")) float MontagePlayRate = 1.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase Transition|Reposition") FVector TransitionAnchorOffset = FVector(0.0f, 0.0f, -3000.0f);
	/** 앵커 기준 전환 이동 목표. P2는 (0, 0, 높이), P3는 XY/Z를 모두 사용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase Transition|Move") FVector TransitionTargetAnchorOffset = FVector(0.0f, 0.0f, 2500.0f);
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase Transition|Move", meta = (ClampMin = "0.0")) float RiseSpeed = 2000.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase Transition|Move", meta = (ClampMin = "0.0")) float RiseAcceleration = 2048.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase Transition|Move", meta = (ClampMin = "0.0")) float RiseAcceptanceTolerance = 50.0f;
	/** 0이면 기존 RiseSpeed/RiseAcceleration 이동을 사용한다. 0보다 크면 지정한 실제 시간 동안 목표 위치까지 보간한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase Transition|Move", meta = (ClampMin = "0.0")) float RiseDuration = 0.0f;
	/** Rise 시작 이벤트가 최초 처리될 때 한 번 재생하는 전환 상승 사운드입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase Transition|Audio") TObjectPtr<USoundBase> RiseStartSound;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase Transition|Audio", meta = (ClampMin = "0.0")) float RiseStartSoundVolumeMultiplier = 1.0f;
	/** 전환 시작 시 실행할 음악 변경 DA. 미지정 시 기존 음악을 유지한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Phase Transition|Audio") TObjectPtr<UMusicParameterEvent> TransitionMusicEvent;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase Transition|Move") FName RiseLoopSectionName = TEXT("RiseLoop");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase Transition|Move") FName SectionAfterTransitionMove;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase Transition|Events") FGameplayTag RepositionEventTag;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase Transition|Events") FGameplayTag TransitionMoveEventTag;

	virtual void SetupTransitionSpecificEvents() {}
	virtual void HandleTransitionMoveCompleted();
	FVector GetTransitionTargetAnchorOffset() const;
	void QueueNextMontageSection(FName FromSection, FName ToSection);
	UBossMovementComponent* GetBossMovement() const { return BossMovement; }

	UFUNCTION() void OnRepositionEvent(FGameplayEventData Payload);
	UFUNCTION() void OnTransitionMoveEvent(FGameplayEventData Payload);
	UFUNCTION() void OnTransitionMoveCompleted();
	UFUNCTION() void OnMontageCompleted();
	UFUNCTION() void OnMontageInterrupted();

	UPROPERTY(Transient) TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;
	UPROPERTY(Transient) TObjectPtr<UAbilityTask_WaitGameplayEvent> RepositionEventTask;
	UPROPERTY(Transient) TObjectPtr<UAbilityTask_WaitGameplayEvent> TransitionMoveEventTask;
	UPROPERTY(Transient) TObjectPtr<UBossMovementComponent> BossMovement;
	bool bRepositioned = false;
	bool bTransitionMoveStarted = false;
};
