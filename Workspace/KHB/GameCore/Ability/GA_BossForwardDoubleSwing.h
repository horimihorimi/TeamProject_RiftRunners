#pragma once

#include "CoreMinimal.h"
#include "GameCore/Ability/GA_EnemyPatternBase.h"
#include "GA_BossForwardDoubleSwing.generated.h"

/**
 * 보스 전방 2연속 휘두르기 패턴.
 *
 * 현재는 공통 패턴 GA의 몽타주 재생만 사용한다. 이동, Motion Warping,
 * 타격 판정은 애님 재생 결과를 확인한 뒤 각각 추가한다.
 */
UCLASS()
class TEAMPROJECT_FIN_API UGA_BossForwardDoubleSwing : public UGA_EnemyPatternBase
{
	GENERATED_BODY()

public:
	UGA_BossForwardDoubleSwing();
};
