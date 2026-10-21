#pragma once

#include "GA_RangedAttack.h"
#include "GA_PhantomRangedAttack.generated.h"

/**
 * 네루드 팬텀 전용 원거리 공격.
 *
 * 단일 유도탄과 정지 후 횡열 분열탄의 선택은 부모 구현과 투사체 설정이 담당한다.
 * 기존 GA_RangedAttack BP를 깨지 않기 위해 부모 클래스는 호환용으로 남긴다.
 */
UCLASS()
class TEAMPROJECT_FIN_API UGA_PhantomRangedAttack : public UGA_RangedAttack
{
	GENERATED_BODY()

public:
	UGA_PhantomRangedAttack();

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	virtual void OnBeforePatternMontagePlay(const FGameplayAbilityActorInfo* ActorInfo) override;
	virtual FName GetPatternMontageStartSection() const override;

	UFUNCTION()
	void OnPhantomFireEventReceived(FGameplayEventData Payload);
	void FireProjectile(bool bUseSplitPattern);
	UPROPERTY()
	TObjectPtr<class UAbilityTask_WaitGameplayEvent> PhantomFireEventTask;

protected:
	/** 분열탄 모탄이 스폰 위치에서 멈춰 있는 시간(초). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ranged|Split", meta = (ClampMin = "0.0"))
	float SplitHoverDuration = 0.50f;

	/** 분열 시 생성할 자탄 수. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ranged|Split", meta = (ClampMin = "1", UIMin = "1"))
	int32 SplitProjectileCount = 4;

	/** 분열 자탄 간의 가로 간격(cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ranged|Split", meta = (ClampMin = "0.0"))
	float SplitLateralSpacing = 100.0f;

	/** 자탄의 원본 대비 크기/충돌 반경/피해 배율. 0.5면 원본의 50%다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ranged|Split|VFX", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float SplitChildScale = 0.50f;

	/** 자탄 크기를 받는 Niagara User Float 이름. 보통 User.BulletScale. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ranged|Split|VFX")
	FName SplitChildScaleParameterName = TEXT("User.BulletScale");

	bool bUsingRightHand = true;
};
