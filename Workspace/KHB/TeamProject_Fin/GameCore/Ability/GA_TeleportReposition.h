#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_TeleportReposition.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAnimMontage;

/**
 * 재배치 위치는 UMonsterCombatPositioningComponent가 준비한다.
 * 이 Ability는 몽타주/머티리얼 Cue가 끝난 Notify 이벤트 시점에 그 위치로 옮기는 실행 책임만 가진다.
 */
UCLASS()
class TEAMPROJECT_FIN_API UGA_TeleportReposition : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_TeleportReposition();

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
	// BP 자식에서 이동/머티리얼 효과가 끝나는 지점에 Event.AI.Teleport.Commit Notify를 둔다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Teleport")
	TObjectPtr<UAnimMontage> RepositionMontage;

	/** 은신에 걸리는 시간. Commit Notify가 오는 시점보다 짧아야 순간이동이 가려진다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Teleport", meta = (ClampMin = "0.0"))
	float CloakInDuration = 0.4f;

	/** 재등장에 걸리는 시간. 짧을수록 또렷하게 나타난다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Teleport", meta = (ClampMin = "0.0"))
	float RevealDuration = 0.2f;

private:
	class UMonsterCloakComponent* FindCloakComponent(const FGameplayAbilityActorInfo* ActorInfo) const;

	UFUNCTION()
	void OnTeleportCommit(FGameplayEventData Payload);

	UFUNCTION()
	void OnMontageCompleted();

	UFUNCTION()
	void OnMontageInterrupted();

	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> CommitEventTask;

	bool bTeleportCommitted = false;
};
