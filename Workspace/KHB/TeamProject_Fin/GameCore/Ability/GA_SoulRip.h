#pragma once

#include "CoreMinimal.h"
#include "GameCore/Ability/GA_EnemyPatternBase.h"
#include "GameplayEffectTypes.h"
#include "GA_SoulRip.generated.h"

class UAbilityTask_WaitDelay;
class UAbilityTask_WaitGameplayEvent;
class UGameplayEffect;

/**
 * Ability.Enemy.Melee.SoulRip 태그로 발동되는 잡기 공격.
 *
 * 이동은 몽타주의 모션 워핑이 전부 맡는다. 패턴 DA의 이동 옵션도, 섹션 전환도 쓰지 않는다.
 *
 *   1. 발동 순간 "플레이어에서 몬스터 쪽으로 Warp Offset Distance만큼 떨어진 지점"을 워프 목표로 등록한다.
 *      높이는 플레이어 Z + Warp Height Offset이다. 워핑이 수평과 높이를 함께 맞춘다.
 *      목표는 발동 순간에 고정한다. 옆으로 피해서 벗어날 수 있어야 이 패턴의 카운터플레이가 성립한다.
 *   2. 잡기 윈도우에 걸리면 영혼 복제본 큐를 켜고, 뽑는 프레임에 데미지를 준다.
 *   3. 몽타주가 끝나면 어빌리티도 끝난다.
 *
 * 몽타주가 끝나지 않으면 어빌리티도 끝나지 않고 StateTree가 다음 패턴으로 넘어가지 못한다.
 * 그래서 자기 자신으로 반복하는 섹션은 발동할 때 끊고, Max Ability Duration으로 한 번 더 막는다.
 *
 * 잡혀도 대상의 조작을 뺏지 않는다. 팬텀 손에 끌려가는 것은 대상의 복제본이고
 * 대상 본인은 계속 움직일 수 있다. 그래서 대상을 구속하는 이펙트가 없고,
 * 어빌리티가 중간에 끊겨도 대상에게 남는 것이 없다.
 *
 * 잡힌 뒤에는 거리를 다시 보지 않는다. 도망쳐서 데미지를 피할 수 있게 하면
 * "될 때도 있고 안 될 때도 있는" 기술이 되어 규칙을 배울 수 없다.
 *
 * 쿨다운은 CooldownGameplayEffectClass가 아니라 EndAbility에서 직접 건다.
 * 커밋 시점에 걸면 모션 길이만큼 실제 재사용 간격이 짧아지고, 몽타주를 고칠
 * 때마다 그 값이 조용히 달라진다.
 */
UCLASS()
class TEAMPROJECT_FIN_API UGA_SoulRip : public UGA_EnemyPatternBase
{
	GENERATED_BODY()

public:
	UGA_SoulRip();

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
	/** 몽타주 Motion Warping 노티파이에 지정한 Warp Target Name과 같아야 한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SoulRip|Warp")
	FName WarpTargetName = TEXT("TargetActor");

	/** 플레이어에서 몬스터 쪽으로 이만큼 떨어진 지점(cm)을 워프 목표로 쓴다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SoulRip|Warp", meta = (ClampMin = "0.0"))
	float WarpOffsetDistance = 400.0f;

	/**
	 * 워프 목표 높이. 플레이어 액터 Z에 더한다(cm). 두 캡슐 중심을 기준으로 한다.
	 *
	 * 0이면 플레이어와 같은 높이로 맞춘다. 높이 보정은 노티파이의 Ignore Z Axis가 꺼져 있어야 적용된다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SoulRip|Warp")
	float WarpHeightOffset = 0.0f;

	/**
	 * 몽타주가 끝나지 않아도 어빌리티를 끝낼 시간(초). 0이면 쓰지 않는다.
	 *
	 * 몽타주 설정 실수로 끝이 나지 않을 때 몬스터가 영원히 이 패턴에 묶이지 않게 하는 안전장치다.
	 * 몽타주 길이보다 넉넉하게 잡는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SoulRip|Safety", meta = (ClampMin = "0.0"))
	float MaxAbilityDuration = 8.0f;

	/** 영혼 복제본 연출. 잡힌 순간 실행하고 뽑는 순간 제거한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SoulRip|Cue",
		meta = (Categories = "GameplayCue.AI"))
	FGameplayTag SoulEffigyCueTag;

	/**
	 * 데미지 적용에 쓸 GameplayEffect.
	 *
	 * 위력은 여기서 정하지 않는다. 위력표(Action.AI.SoulRip 행)가 정한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SoulRip|Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	/**
	 * 종료 시점에 적용할 쿨다운 이펙트.
	 *
	 * Cooldown.AI.SoulRip 태그를 부여해야 한다. 태그가 없으면 지속시간이
	 * 있어도 ActivationBlockedTags가 걸러낼 것이 없어 쿨다운이 없는 것과 같다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SoulRip|Cooldown")
	TSubclassOf<UGameplayEffect> CooldownEffectClass;

	/**
	 * 취소로 끝났을 때도 쿨다운을 걸지 여부.
	 *
	 * 끄면 경직으로 끊길 때마다 쿨다운이 초기화되어, 맞을수록 이 패턴이 자주
	 * 나오는 뒤집힌 동작이 된다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SoulRip|Cooldown")
	bool bApplyCooldownOnCancel = true;

private:
	UFUNCTION()
	void OnCaughtEvent(FGameplayEventData Payload);

	UFUNCTION()
	void OnYankEvent(FGameplayEventData Payload);

	UFUNCTION()
	void OnMaxDurationReached();

	void RegisterWarpTarget() const;
	/** 자기 자신으로 반복하는 섹션을 끊는다. 반복이 남아 있으면 몽타주가 끝나지 않는다. */
	void BreakSelfLoopingSections() const;

	AActor* GetCombatTarget() const;
	void ApplyRipDamage();
	void ApplyCooldownEffect();
	void RemoveSoulEffigyCue();

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> CaughtEventTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> YankEventTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitDelay> MaxDurationTask;

	UPROPERTY()
	TObjectPtr<AActor> CaughtTarget;

	bool bGrabbed = false;
	bool bDamageApplied = false;
	bool bEffigyCueActive = false;
};
