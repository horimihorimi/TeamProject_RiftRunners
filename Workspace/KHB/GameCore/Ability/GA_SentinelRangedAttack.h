#pragma once

#include "GameCore/Ability/GA_EnemyPatternBase.h"
#include "GA_SentinelRangedAttack.generated.h"

class AMonsterProjectile;
class UAnimMontage;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UGameplayEffect;

/** Riot Sentinel 전용 쌍포 에너지탄 공격. */
UCLASS()
class TEAMPROJECT_FIN_API UGA_SentinelRangedAttack : public UGA_EnemyPatternBase
{
	GENERATED_BODY()

public:
	UGA_SentinelRangedAttack();

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	virtual UAnimMontage* GetPatternMontage() const override { return AttackMontage; }
	virtual void OnBeforePatternMontagePlay(const FGameplayAbilityActorInfo* ActorInfo) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sentinel Ranged")
	TObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sentinel Ranged")
	TSubclassOf<AMonsterProjectile> ProjectileClass;

	/** 같은 Fired Notify 한 번에 이 목록의 모든 소켓에서 한 발씩 발사한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sentinel Ranged")
	TArray<FName> MuzzleSocketNames;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sentinel Ranged|Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

private:
	UFUNCTION()
	void OnFireEventReceived(FGameplayEventData Payload);
	void FireTwinCannons();
	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitFireEventTask;
};
