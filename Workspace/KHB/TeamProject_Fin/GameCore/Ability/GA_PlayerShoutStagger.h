#pragma once

#include "Abilities/GameplayAbility.h"
#include "GA_PlayerShoutStagger.generated.h"

class UAbilitySystemComponent;
class UAnimMontage;
class UAbilityTask_PlayMontageAndWait;

/**
 * State.Debuff.Staggered가 존재하는 동안 플레이어 행동을 취소·차단합니다.
 * 화면 효과는 같은 태그를 감지하는 BossShoutOverlayWidget이 별도로 처리합니다.
 */
UCLASS()
class TEAMPROJECT_FIN_API UGA_PlayerShoutStagger : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_PlayerShoutStagger();

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

private:
	UFUNCTION()
	void OnStaggerMontageFinished();

	void OnStaggeredTagChanged(FGameplayTag Tag, int32 NewCount);

	/** State.Debuff.Staggered가 추가될 때 플레이어에게 재생할 경직 몽타주입니다. */
	UPROPERTY(EditDefaultsOnly, Category = "Shout Stagger")
	TObjectPtr<UAnimMontage> StaggerMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Shout Stagger", meta = (ClampMin = "0.01"))
	float StaggerMontagePlayRate = 1.0f;

	TWeakObjectPtr<UAbilitySystemComponent> BoundAbilitySystemComponent;
	TObjectPtr<UAbilityTask_PlayMontageAndWait> StaggerMontageTask;
};
