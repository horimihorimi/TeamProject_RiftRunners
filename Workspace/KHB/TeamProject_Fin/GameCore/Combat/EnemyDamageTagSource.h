#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "EnemyDamageTagSource.generated.h"

/*
 * 데미지 표(DT_MonsterAttackDamage)를 찾을 공격 태그를 내놓는 쪽의 계약입니다.
 *
 * GA의 Asset Tags는 "어떤 행동인지" 식별(Ability.Enemy.*)에만 쓰고,
 * "얼마나 아픈지"를 정하는 데미지 키(Action.AI.*)는 이 계약으로 따로 받는다.
 */
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UEnemyDamageTagSource : public UInterface
{
	GENERATED_BODY()
};

class TEAMPROJECT_FIN_API IEnemyDamageTagSource
{
	GENERATED_BODY()

public:
	/** 데미지 표의 행 이름이 되는 Action.AI.* 태그. 비어 있으면 데미지 키가 없다는 뜻이다. */
	virtual FGameplayTag GetDamageAttackTag() const = 0;
};
