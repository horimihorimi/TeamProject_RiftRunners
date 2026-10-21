#pragma once

#include "GameCore/Ability/GA_EnemyPatternBase.h"
#include "GameCore/Projectile/BossRingWave.h"
#include "GA_BossProjectileRings.generated.h"

class UAbilityTask_WaitGameplayEvent;
class UGameplayEffect;

UCLASS()
class TEAMPROJECT_FIN_API UGA_BossProjectileRings : public UGA_EnemyPatternBase
{
	GENERATED_BODY()
public:
	UGA_BossProjectileRings();
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
protected:
	UFUNCTION() void OnRingSpawnEvent(FGameplayEventData Payload);
	UFUNCTION() void OnRingLaunchEvent(FGameplayEventData Payload);
	UPROPERTY(EditDefaultsOnly, Category="Ring") EBossRingWavePlane InitialPlane = EBossRingWavePlane::Vertical;
	UPROPERTY(EditDefaultsOnly, Category="Ring") TSubclassOf<ABossRingWave> RingWaveClass;
	UPROPERTY(EditDefaultsOnly, Category="Ring") float ChargeRadius = 500.0f;
	UPROPERTY(EditDefaultsOnly, Category="Ring") float ChargeExpansionSpeed = 350.0f;
	UPROPERTY(EditDefaultsOnly, Category="Ring") float MaxRadius = 3000.0f;
	UPROPERTY(EditDefaultsOnly, Category="Ring") float LaunchExpansionSpeed = 2500.0f;
	UPROPERTY(EditDefaultsOnly, Category="Ring") float BandHalfWidth = 45.0f;
	UPROPERTY(EditDefaultsOnly, Category="Ring") float PlaneHalfThickness = 100.0f;
	UPROPERTY(EditDefaultsOnly, Category="Ring") FGameplayTag RingAttackTag;
	UPROPERTY(EditDefaultsOnly, Category="Ring") TSubclassOf<UGameplayEffect> RingDamageEffectClass;
private:
	void SpawnRing(EBossRingWavePlane Plane);
	UPROPERTY(Transient) TObjectPtr<UAbilityTask_WaitGameplayEvent> RingSpawnEventTask;
	UPROPERTY(Transient) TObjectPtr<UAbilityTask_WaitGameplayEvent> RingLaunchEventTask;
	EBossRingWavePlane NextPlane = EBossRingWavePlane::Vertical;
	TWeakObjectPtr<ABossRingWave> LastSpawnedRing;
	/** 이 패턴이 생성한 모든 링. 발사 뒤에도 종료/취소 시 제거해야 한다. */
	TArray<TWeakObjectPtr<ABossRingWave>> ActiveRings;
};
