#pragma once

#include "Abilities/GameplayAbility.h"
#include "CoreMinimal.h"
#include "GameCore/Combat/EnemyDamageTagSource.h"
#include "GA_EnemyPatternBase.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAnimMontage;
class UMonsterMovementComponent;
class AMonsterCharacter;
class USoundBase;

/**
 * 새 Enemy Framework의 공격 패턴 공통 GA 기반입니다.
 *
 * 패턴별 GA는 몽타주와 노티파이 반응만 정의하고, 행동 태그·이동 잠금·몽타주 종료·정리는
 * 이 클래스가 처리합니다.
 */
UCLASS(Abstract)
class TEAMPROJECT_FIN_API UGA_EnemyPatternBase : public UGameplayAbility, public IEnemyDamageTagSource
{
	GENERATED_BODY()

public:
	UGA_EnemyPatternBase();

	/** 보스 패턴의 데미지 키. 복합 패턴은 투사체·폭발마다 따로 태그를 넘긴다. */
	virtual FGameplayTag GetDamageAttackTag() const override;

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
	/**
	 * 직접 타격 NotifyState가 Attack Tag Override를 비워도 위력표를 찾을 기본 공격 태그.
	 * 복합 패턴은 각 투사체/폭발의 태그가 이를 대신하므로 비워 둘 수 있다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Pattern|Damage", meta = (Categories = "Action.AI"))
	FGameplayTag DefaultDamageAttackTag;

	/** 패턴 GA를 상속한 Blueprint에서 실제 타우라 몽타주를 지정합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Pattern")
	TObjectPtr<UAnimMontage> PatternMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Pattern", meta = (ClampMin = "0.01"))
	float MontagePlayRate = 1.0f;

	/** 하위 GA가 기존 BP의 AttackMontage 등을 계속 사용할 수 있는 확장 지점이다. */
	virtual UAnimMontage* GetPatternMontage() const { return PatternMontage; }
	virtual float GetPatternMontagePlayRate() const { return MontagePlayRate; }
	virtual FName GetPatternMontageStartSection() const { return NAME_None; }
	virtual void OnBeforePatternMontagePlay(const FGameplayAbilityActorInfo* ActorInfo) {}

	/** Asset Tags의 Ability.Enemy.* 태그를 패턴 식별자로 사용한다. */
	FGameplayTag ResolvePatternAbilityTag() const;

	/** 반복 섹션에서 Event.AI.Boss.AttackAlert을 받을 때 활성화당 한 번 재생할 경고음입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Pattern|Audio")
	TObjectPtr<USoundBase> AttackAlertSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Pattern|Audio", meta = (ClampMin = "0.0"))
	float AttackAlertVolumeMultiplier = 1.0f;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> AttackAlertEventTask;

	UFUNCTION()
	void OnPatternMontageCompleted();

	UFUNCTION()
	void OnPatternMontageInterrupted();

	UFUNCTION()
	void OnAttackAlertEvent(FGameplayEventData Payload);

	UFUNCTION()
	void OnPatternMovementArrived(FName ArrivalLoopSectionName, FName ArrivalJumpSectionName);

	/** Move During Ability의 도착 섹션 점프를 받을 이동 컴포넌트. 지상/비행/보스 공용이다. */
	UPROPERTY(Transient)
	TObjectPtr<UMonsterMovementComponent> BoundPatternMovementComponent;

	/** Ability 인스턴스가 살아 있는 동안 동일 루프 노티파이의 중복 재생을 막습니다. */
	bool bHasPlayedAttackAlert = false;
};
