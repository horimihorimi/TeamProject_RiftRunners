#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_RadialExplosion.generated.h"

class UGameplayEffect;
class UNiagaraSoundCueData;

/**
 * 시전자 위치에서 즉시 반경 피해와 게임플레이 큐를 실행하는 공용 폭발 어빌리티.
 *
 * 이동, 몽타주, 사망 같은 패턴별 흐름은 소유하지 않는다. 해당 흐름은 이 어빌리티를
 * 활성화하는 패턴 GA가 담당한다.
 */
UCLASS()
class TEAMPROJECT_FIN_API UGA_RadialExplosion : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_RadialExplosion();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

protected:
	/** 데미지 판정 반경. 시전자 자신은 판정에서 제외한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Radial Explosion", meta = (ClampMin = "0.0"))
	float ExplosionRadius = 300.0f;

	/** 폭발 데미지에 적용할 GameplayEffect. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Radial Explosion")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	/** 위력표를 찾을 공격 태그. Asset Tag에서 추론하지 않고 명시한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Radial Explosion", meta = (Categories = "Action.AI"))
	FGameplayTag AttackTag;

	/** 폭발 연출에 실행할 Gameplay Cue. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Radial Explosion", meta = (Categories = "GameplayCue"))
	FGameplayTag ExplosionCueTag;

	/** 큐에 SourceObject로 전달할 이펙트·사운드 데이터. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Radial Explosion")
	TObjectPtr<UNiagaraSoundCueData> ExplosionCueData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Radial Explosion|Debug")
	bool bDebugDrawExplosion = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Radial Explosion|Debug",
		meta = (EditCondition = "bDebugDrawExplosion", ClampMin = "0.0"))
	float DebugExplosionDrawDuration = 3.0f;

private:
	void ApplyExplosionDamage(AActor* AvatarActor);
};
