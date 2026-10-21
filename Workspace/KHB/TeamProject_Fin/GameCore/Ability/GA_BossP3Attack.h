#pragma once

#include "GameCore/Ability/GA_EnemyPatternBase.h"
#include "GameplayEffectTypes.h"
#include "GA_BossP3Attack.generated.h"

class ABossWaveProjectile;
class ABossP3AttackPillar;
class AMonsterProjectile;
class UAbilityTask_WaitGameplayEvent;
class UGameplayEffect;
class UAbilitySystemComponent;

/** 미사일 발사 이벤트 한 번에 사용할 스폰 위치 묶음이다. */
USTRUCT(BlueprintType)
struct FBossMissileBurstPreset
{
	GENERATED_BODY()

	/** 보스 로컬 기준 스폰 위치. X=전방, Y=오른쪽, Z=위. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Missile Burst")
	TArray<FVector> SpawnOffsets;
};

/**
 * Alepsis-Taura P3 복합 패턴의 첫 단계입니다.
 *
 * 현재는 미러 한 장의 스폰·하강·피격만 담당합니다. Pillar, 미사일, Shout은
 * 후속 단계에서 이 GA에 추가하되 기존 ShatteredMirror 패턴에는 영향을 주지 않습니다.
 */
UCLASS()
class TEAMPROJECT_FIN_API UGA_BossP3Attack : public UGA_EnemyPatternBase
{
	GENERATED_BODY()

public:
	UGA_BossP3Attack();

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
	UFUNCTION()
	void OnMirrorLaunchEvent(FGameplayEventData Payload);

	UFUNCTION()
	void OnPillarSpawnEvent(FGameplayEventData Payload);

	UFUNCTION()
	void OnMissileFireEvent(FGameplayEventData Payload);

	UFUNCTION()
	void OnShoutEvent(FGameplayEventData Payload);

	/** ABossWaveProjectile 기반의 미러 외형 BP입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shattered Mirrors")
	TSubclassOf<ABossWaveProjectile> MirrorProjectileClass;

	/** 보스 로컬 기준 미러 시작 위치입니다. X=전방, Y=오른쪽, Z=위. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shattered Mirrors")
	FVector MirrorSpawnOffset = FVector(900.0f, 0.0f, 1800.0f);

	/** 보스 정면에 더할 하강 각도입니다. 음수면 아래로 향합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shattered Mirrors", meta = (ClampMin = "-89.0", ClampMax = "89.0"))
	float MirrorPitchDegrees = -30.0f;

	/** 미러 박스 판정의 절반 크기(cm)이며 메시 크기도 이 값에 맞춰집니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shattered Mirrors", meta = (ClampMin = "1.0"))
	FVector MirrorBoxExtent = FVector(1000.0f, 80.0f, 700.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shattered Mirrors", meta = (ClampMin = "0.0"))
	float MirrorSpeed = 1600.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shattered Mirrors", meta = (ClampMin = "0.0"))
	float MirrorLifeSpan = 4.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shattered Mirrors")
	TSubclassOf<UGameplayEffect> MirrorDamageEffectClass;

	/** Mirror Fire 투사체 위력표에 사용할 공격 태그. BP에서 반드시 지정한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shattered Mirrors", meta = (Categories = "Action.AI"))
	FGameplayTag MirrorAttackTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "P3 Attack|Pillar")
	TSubclassOf<ABossP3AttackPillar> PillarClass;

	/**
	 * 미러 중심에서의 수평 배치 오프셋입니다.
	 * X=보스 정면 기준 앞/뒤, Y=보스 우측 기준 좌/우, Z=시작 높이 보정입니다.
	 * 미러의 기울어진 진행 축을 쓰지 않으므로 X/Y를 바꿔도 Pillar 길이축을 따라 밀리지 않습니다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "P3 Attack|Pillar")
	TArray<FVector> PillarSpawnOffsets;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "P3 Attack|Pillar", meta = (ClampMin = "1.0"))
	float PillarRadius = 150.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "P3 Attack|Pillar", meta = (ClampMin = "1.0"))
	float PillarTraceLength = 5000.0f;

	/** 트레이스로 잰 길이(미러 위치~바닥)에 곱하는 배율. 1이면 그대로, 0.5면 절반. 바닥 쪽 끝은 그대로 두고 미러 쪽 끝을 당긴다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "P3 Attack|Pillar", meta = (ClampMin = "0.05"))
	float PillarLengthScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "P3 Attack|Pillar", meta = (ClampMin = "0.0"))
	float PillarLifeSpan = 6.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "P3 Attack|Pillar")
	TEnumAsByte<ECollisionChannel> PillarGroundTraceChannel = ECC_Visibility;

	/** 기존 AMonsterProjectile 기반의 P3 유도탄 BP입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "P3 Attack|Missile")
	TSubclassOf<AMonsterProjectile> MissileProjectileClass;

	/** 발사 이벤트마다 순서대로 선택하며, 끝까지 가면 처음 프리셋부터 다시 사용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "P3 Attack|Missile")
	TArray<FBossMissileBurstPreset> MissileBurstPresets;

	/** 0이면 거리 제한 없이 살아 있는 플레이어를 모두 유도 대상 후보로 쓴다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "P3 Attack|Missile", meta = (ClampMin = "0.0"))
	float MissileTargetSearchRadius = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "P3 Attack|Missile")
	TSubclassOf<UGameplayEffect> MissileDamageEffectClass;

	/** 투사체 위력표에 사용할 공격 태그입니다. 비워 두면 P3 Ability 태그를 사용합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "P3 Attack|Missile")
	FGameplayTag MissileAttackTag;

	/** Infinite GE. 반드시 State.Debuff.Staggered를 Granted Tag로 부여해야 합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "P3 Attack|Shout")
	TSubclassOf<UGameplayEffect> ShoutStaggerEffectClass;

	/** 한 번의 샤우트 노티파이가 경직을 발동할 확률입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "P3 Attack|Shout", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ShoutChance = 1.0f;

	/** 0이면 살아 있는 모든 플레이어가 대상입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "P3 Attack|Shout", meta = (ClampMin = "0.0"))
	float ShoutTargetSearchRadius = 0.0f;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> MirrorLaunchTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> PillarSpawnTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> MissileFireTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> ShoutTask;

	struct FAppliedShoutStagger
	{
		TWeakObjectPtr<UAbilitySystemComponent> TargetASC;
		FActiveGameplayEffectHandle Handle;
	};

	void RemoveAppliedShoutStaggers();
	TArray<FAppliedShoutStagger> AppliedShoutStaggers;
	bool bShoutResolved = false;

	TWeakObjectPtr<ABossWaveProjectile> ActiveMirror;
	FVector SharedPillarDirection = FVector::ForwardVector;
	int32 MissileBurstIndex = 0;
};
