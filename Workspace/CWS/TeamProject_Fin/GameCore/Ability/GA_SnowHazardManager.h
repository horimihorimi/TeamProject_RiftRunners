#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_SnowHazardManager.generated.h"

/**
 * 눈 태그(Snow Level 1, 2, 3)를 감지하여 해당하는 디버프 GE를 스스로 적용하고 해제하는 패시브 어빌리티입니다.
 * 몬스터에게는 이 어빌리티를 부여하지 않으면 눈 패널티를 받지 않습니다.
 */
UCLASS()
class TEAMPROJECT_FIN_API UGA_SnowHazardManager : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_SnowHazardManager();

	virtual void OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	// 눈 태그(L1, L2, L3)에 대응하는 디버프 GE 클래스 맵
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects")
	TMap<FGameplayTag, TSubclassOf<class UGameplayEffect>> SnowDebuffEffects;

private:
	// 현재 적용 중인 눈 디버프의 핸들을 추적하여 나중에 제거하기 위함
	TMap<FGameplayTag, struct FActiveGameplayEffectHandle> ActiveSnowDebuffs;

	UFUNCTION()
	void OnSnowTagChanged(const FGameplayTag CallbackTag, int32 NewCount);
};
