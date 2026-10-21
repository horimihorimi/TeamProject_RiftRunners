#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "MonsterAttackContext.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class UMonsterAttackContext : public UInterface
{
	GENERATED_BODY()
};

/**
 * 지금 시전 중인 공격이 무엇인지 알려주는 쪽.
 *
 * 공격 태그는 GameplayAbility가 Asset Tags로 이미 갖고 있다. 그런데 데미지를
 * 넣는 쪽은 노티파이라, 어빌리티와 상속 관계가 없어 그 태그를 볼 방법이 없다.
 * 둘 사이를 잇는 창구가 이 인터페이스다.
 *
 * 어빌리티가 활성화될 때 자기 태그를 적어두고, 끝날 때 지운다. 노티파이는
 * 타격 윈도우가 열릴 때 한 번 읽어 간다.
 *
 * 어빌리티보다 오래 사는 것(드론, 투사체)은 이 인터페이스를 쓰지 않는다.
 * 그쪽은 스폰될 때 태그를 값으로 복사받는다. 자폭할 무렵이면 시전자가 이미
 * 파괴돼 물어볼 상대가 없기 때문이다.
 */
class IMonsterAttackContext
{
	GENERATED_BODY()

public:
	/** 어빌리티가 활성화될 때 자기 공격 태그를 적는다. 끝날 때 빈 태그로 지운다. */
	virtual void SetCurrentAttackTag(const FGameplayTag& InAttackTag) = 0;

	/** 시전 중인 공격 태그. 시전 중이 아니면 빈 태그. */
	virtual FGameplayTag GetCurrentAttackTag() const = 0;
};
