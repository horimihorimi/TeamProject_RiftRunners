#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "EnemyMovementProfileReceiver.generated.h"

class UEnemyMovementProfileDataAsset;

UINTERFACE(meta = (CannotImplementInterfaceInBlueprint))
class TEAMPROJECT_FIN_API UEnemyMovementProfileReceiver : public UInterface
{
	GENERATED_BODY()
};

/**
 * 캐릭터가 활성 이동 프로필을 이동 컴포넌트에 넘겨주는 계약입니다.
 *
 * 캐릭터는 어떤 이동 컴포넌트가 붙어 있는지 모른다. 프로필이 바뀌는 시점(초기화, 페이즈 전환)만
 * 알려주고, 어떤 값을 어떻게 적용할지는 이동 방식별 구현체가 정한다.
 */
class TEAMPROJECT_FIN_API IEnemyMovementProfileReceiver
{
	GENERATED_BODY()

public:
	/** 활성 프로필이 바뀌었을 때 호출됩니다. 널이면 컴포넌트 기본값으로 되돌립니다. */
	virtual void HandleMovementProfileChanged(const UEnemyMovementProfileDataAsset* Profile) = 0;
};
