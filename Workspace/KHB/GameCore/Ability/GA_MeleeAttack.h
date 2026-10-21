#pragma once

#include "CoreMinimal.h"
#include "GameCore/Ability/GA_EnemyPatternBase.h"
#include "GA_MeleeAttack.generated.h"

class UAnimMontage;
class UAbilityTask_PlayMontageAndWait;

/**
 * 근접 공격 패턴들이 공유하는 실행 절차만 담당하는 기반 Ability.
 *
 * 활성화 중 State.AI.ActionCommitted를 소유자에게 부여해 이동 레이어
 * (AMonsterAIController::UpdateMovement)를 멈추고, Motion Warping 타깃을
 * 등록한 뒤 몽타주를 재생한다. 몽타주가 끝나면 스스로 종료한다.
 *
 * 타격 판정은 몽타주의 ANS_MeleeAttackHit이, 실제 위치 보정은 몽타주의
 * Motion Warping 노티파이가 맡는다. 이 클래스는 어느 쪽도 직접 하지 않는다.
 *
 * 어떤 패턴인지는 여기서 정하지 않는다. BP 자식에서
 *  - Asset Tags: Ability.Enemy.Melee.Claw 같은 식별 태그
 *  - Damage Attack Tag: Action.AI.Melee 같은 데미지 키
 * 를 지정한다.
 */
UCLASS()
class TEAMPROJECT_FIN_API UGA_MeleeAttack : public UGA_EnemyPatternBase
{
	GENERATED_BODY()

public:
	UGA_MeleeAttack();

protected:
	virtual UAnimMontage* GetPatternMontage() const override { return AttackMontage; }
	virtual void OnBeforePatternMontagePlay(const FGameplayAbilityActorInfo* ActorInfo) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee")
	TObjectPtr<UAnimMontage> AttackMontage;

	// 몽타주의 Motion Warping 노티파이에 지정한 Warp Target Name과 일치해야 한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee")
	FName WarpTargetName = TEXT("TargetActor");

	// 타깃을 지나쳐 그 뒤로 이 거리(cm)만큼 더 간 지점까지 워프한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee")
	float WarpOvershootDistance = 150.0f;
};
