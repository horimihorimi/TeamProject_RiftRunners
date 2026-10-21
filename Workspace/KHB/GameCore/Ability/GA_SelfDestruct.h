#pragma once

#include "CoreMinimal.h"
#include "GameCore/Ability/GA_EnemyAttackBase.h"
#include "GA_SelfDestruct.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAnimMontage;
class UNiagaraSoundCueData;

UCLASS()
class TEAMPROJECT_FIN_API UGA_SelfDestruct : public UGA_EnemyAttackBase
{
	GENERATED_BODY()
	
public:
	UGA_SelfDestruct();

	// SelfDestruct 몽타주의 Detonate Notify에서 호출한다.
	UFUNCTION(BlueprintCallable, Category = "SelfDestruct")
	void NotifySelfDestructDetonate();

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
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct")
	TObjectPtr<UAnimMontage> SelfDestructMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct")
	float MontagePlayRate = 1.0f;

	/**
	 * 자폭 판정 반경. 데미지에만 쓴다.
	 *
	 * 이펙트 크기는 큐 데이터 에셋의 Scale Parameter Value가 따로 정하며 이
	 * 값과 연동되지 않는다. 한쪽을 바꾸면 다른 쪽도 손으로 맞춰야 한다.
	 *
	 * 기준: 같은 나이아가라가 배율 1.0에서 반경 300. 배율 0.5면 150이다.
	 * bDebugDrawExplosion으로 확인하고 조정한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct|Explosion", meta = (ClampMin = "0.0"))
	float ExplosionRadius = 150.0f;

	/**
	 * 데미지 적용에 쓸 GameplayEffect.
	 *
	 * 지정하지 않으면 폭발 연출만 나가고 아무도 다치지 않는다. 다른 몬스터
	 * 공격과 같은 GE를 쓰면 된다. 위력은 위력표가 정한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct|Explosion")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	/**
	 * 자폭 순간 실행할 게임플레이 큐.
	 *
	 * 생성자에서 GameplayCue.AI.SelfDestruct로 채워둔다. 다른 연출을 쓰려면
	 * BP에서 바꾼다. 비우면 폭발이 보이지 않으므로 비워두지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct|Explosion",
		meta = (Categories = "GameplayCue.AI"))
	FGameplayTag ExplosionCueTag;

	/**
	 * 큐에 넘길 이펙트·사운드 묶음.
	 *
	 * 큐 파라미터의 SourceObject로 전달되므로, 같은 큐 태그를 쓰는 다른
	 * 몬스터와 연출을 다르게 가져갈 수 있다. 비워두면 큐 에셋의 기본값이 쓰인다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct|Explosion")
	TObjectPtr<UNiagaraSoundCueData> ExplosionCueData;

	/**
	 * 자폭 순간의 데미지 판정 반경을 구로 그린다.
	 *
	 * 판정과 이펙트가 서로 연동되지 않으므로 눈으로 겹쳐 보고 맞춰야 한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct|Debug")
	bool bDebugDrawExplosion = false;

	/**
	 * 판정 구를 남겨 둘 시간(초).
	 *
	 * 한 프레임만 그리면 이펙트가 재생되는 동안 겹쳐볼 수가 없다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct|Debug",
		meta = (EditCondition = "bDebugDrawExplosion", ClampMin = "0.0"))
	float DebugExplosionDrawDuration = 3.0f;

private:
	/** 반경 안의 플레이어에게 자폭 데미지를 넣는다. 서버에서만 실행된다. */
	void ApplyExplosionDamage(AActor* AvatarActor);
	void FinishSelfDestruct();

	void BeginWaitingForDetonate();
	void MaintainApproachAfterTargetLoss();

	UFUNCTION()
	void OnDetonateEvent(FGameplayEventData Payload);

	void SendSelfDestructCompletedEvent();

	UFUNCTION()
	void OnMontageCompleted();

	UFUNCTION()
	void OnMontageInterrupted();

	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> PlayMontageTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> DetonateEventTask;

	/*
	 * 자폭 시작 시점에 타깃 위치를 기억한다. 자폭 몽타주 중 타깃이 죽어
	 * AIController의 추적 대상이 사라져도, 이 위치까지 이동을 계속한다.
	 */
	FTimerHandle TargetLossApproachTimerHandle;
	FVector LastKnownTargetLocation = FVector::ZeroVector;
	bool bHasLastKnownTargetLocation = false;
	bool bDetonated = false;
	bool bFinished = false;
};
