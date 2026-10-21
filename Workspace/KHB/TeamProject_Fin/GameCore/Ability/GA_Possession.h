#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "GA_Possession.generated.h"

class UAnimMontage;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UFlyingMonsterMovementComponent;
class ABaseCharacter;
enum class EMonsterFlightApproachMode : uint8;

/**
 * Ability.Enemy.Melee.Possession 태그로 발동되는 빙의 패턴.
 *
 * 반경 내 필수 태그를 가지고 현재 전투 타겟을 보유한 액터가 있을 때만 발동 가능하다
 * (CanActivateAbility에서 확인: 없으면 즉시 실패해서 StateTree가 다시 선택한다).
 *
 * 흐름: 대상으로 접근(모션워핑) -> 대상에 빙의 이펙트 적용(버프+태그+Cue) 하고
 * 자신은 숨김 -> 이펙트가 자연 만료되면 대상에 만료 이벤트를 보내고(실제 자폭은
 * 대상 자신의 몫: 아직 대상 몹 클래스가 없어서 여기선 신호만 보낸다)
 * 그 자리에 재소환.
 */
UCLASS()
class TEAMPROJECT_FIN_API UGA_Possession : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Possession();

	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

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
	// 디버그용: 탐색 반경을 시각화한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Possession|Debug")
	bool bDebugSearchRadius = false;

	// 비어 있으면 생성자의 기본 필수 태그를 사용한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Possession")
	FGameplayTagContainer RequiredTargetTags;

	// 비어 있으면 생성자의 기본 차단 태그를 사용한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Possession")
	FGameplayTagContainer BlockedTargetTags;

	// 빙의 대상 탐색 반경(cm).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Possession", meta = (ClampMin = "0.0"))
	float SearchRadius = 800.0f;

	// 대상으로 접근하는 대쉬 몽타주(Dash_Start). 모션워핑으로 대상 위치까지 보정한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Possession")
	TObjectPtr<UAnimMontage> ApproachMontage;

	// 몽타주의 Motion Warping 노티파이에 지정한 Warp Target Name과 일치해야 한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Possession")
	FName WarpTargetName = TEXT("PossessTarget");

	// 이 거리 안으로 들어오면 몽타주가 끝나기 전이라도 빙의를 시작한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Possession", meta = (ClampMin = "0.0"))
	float ArrivalRadius = 100.0f;

	/*
	 * 빙의 대상에게 적용할 Duration 이펙트.
	 * - Granted Tags: State.AI.Possessed (대상의 BehaviorDecision이 강화 패턴을 고르는 기준)
	 * - GameplayCueTags: GameplayCue.AI.Possession (오라 연출, Cue Notify BP가 자동 처리)
	 * - Duration: 여기서 정한 시간이 빙의 유지 시간의 단일 진실 소스다.
	 *   GA_Possession은 별도 타이머를 두지 않고 이 이펙트가 제거되는 시점을 구독한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Possession")
	TSubclassOf<UGameplayEffect> PossessionEffectClass;

	// 재소환 시 재생할 몽타주.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Possession")
	TObjectPtr<UAnimMontage> EmergeMontage;

	// 빙의 중 대상 비행 속도 배율.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Possession|Movement", meta = (ClampMin = "1.0"))
	float PossessedFlySpeedMultiplier = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Possession", meta = (ClampMin = "0.0"))
	float SelfDestructDelay = 3.0f;

private:
	UFUNCTION()
	void OnApproachMontageCompleted();

	UFUNCTION()
	void OnApproachMontageInterrupted();

	UFUNCTION()
	void OnEmergeMontageCompleted();

	void CheckArrival();
	void StartPossession();

	/*
	 * State.AI.Possessed는 도착 후 GE가 붙을 때에야 생긴다. 접근 비행 구간이
	 * 통째로 비어 있어서, 그 사이 다른 팬텀이 같은 대상을 고를 수 있다.
	 * 대상 확정 시점에 예약 태그를 걸고 EndAbility에서 반드시 푼다.
	 */
	void ReservePossessionTarget();
	void ReleasePossessionTarget();
	void ApplyPossessionMovementOverride();
	void RestorePossessionMovementOverride();

	// PossessionEffectClass가 대상에게서 자연 만료(또는 제거)될 때 호출된다.
	void TriggerSelfDestruct();

	void ReappearAtTarget();

	UFUNCTION()
	void OnSelfDestructCompleted(FGameplayEventData Payload);

	UFUNCTION()
	void OnSelfDestructDetonate(FGameplayEventData Payload);

	UFUNCTION()
	void OnPossessedTargetDead(FGameplayTag CallbackTag, int32 NewCount);

	void OnPossessedTargetDeathStarted(ABaseCharacter* DeadCharacter);

	void BeginWaitingForSelfDestructCompleted();
	void BeginWaitingForSelfDestructDetonate();

	FGameplayTagContainer GetEffectiveRequiredTargetTags() const;
	FGameplayTagContainer GetEffectiveBlockedTargetTags() const;

	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> ApproachMontageTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> EmergeMontageTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> SelfDestructCompletedTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> SelfDestructDetonateTask;

	UPROPERTY()
	TObjectPtr<AActor> PossessedTarget;

	TWeakObjectPtr<UAbilitySystemComponent> PossessedTargetASC;

	FActiveGameplayEffectHandle AppliedEffectHandle;

	TWeakObjectPtr<UFlyingMonsterMovementComponent> PossessedMovementComponent;
	EMonsterFlightApproachMode PreviousFlightApproachMode;
	bool bPreviousStrafeEnabled = true;
	float PreviousMaxFlySpeed = 0.0f;
	bool bMovementOverrideApplied = false;

	bool bPossessionStarted = false;
	bool bReappearanceStarted = false;

	// 예약 태그를 실제로 걸었는지. 걸지 않았으면 해제도 하지 않는다.
	bool bReservedPossessionTarget = false;

	FTimerHandle ArrivalCheckTimerHandle;
	FTimerHandle SelfDestructTimerHandle;
};
