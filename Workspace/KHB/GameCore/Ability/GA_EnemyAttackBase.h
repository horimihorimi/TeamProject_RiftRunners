#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameCore/Combat/EnemyDamageTagSource.h"
#include "GA_EnemyAttackBase.generated.h"

/**
 * 데미지를 주는 일반 몬스터 GA의 공통 부모입니다.
 *
 * 태그 역할을 보스 패턴 GA와 똑같이 나눈다.
 *  - Asset Tags: 식별(Ability.Enemy.*). 패턴 DA와 StateTree가 이 태그로 GA를 찾는다.
 *  - Damage Attack Tag: 데미지 표 키(Action.AI.*). 타격 판정과 투사체가 이 태그로 위력을 찾는다.
 */
UCLASS(Abstract)
class TEAMPROJECT_FIN_API UGA_EnemyAttackBase : public UGameplayAbility, public IEnemyDamageTagSource
{
	GENERATED_BODY()

public:
	virtual FGameplayTag GetDamageAttackTag() const override { return DamageAttackTag; }

protected:
	/** DT_MonsterAttackDamage의 행 이름. C++ 하위 GA는 생성자에서, BP 하위 GA는 클래스 기본값에서 지정한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Attack|Damage", meta = (Categories = "Action.AI"))
	FGameplayTag DamageAttackTag;
};
