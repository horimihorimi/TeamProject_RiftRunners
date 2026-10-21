#pragma once

#include "CoreMinimal.h"
#include "GameCore/Ability/GA_EnemyPatternBase.h"
#include "GA_BossTendrilHorrorEject.generated.h"

/**
 * Alepsis-Taura의 공중 촉수 연속 찌르기 패턴.
 *
 * 몽타주의 ANS_FaceTarget 구간에서 Pitch를 포함해 타겟을 향하고,
 * 실제 찌르기와 타격 판정은 몽타주 노티파이가 담당한다.
 */
UCLASS()
class TEAMPROJECT_FIN_API UGA_BossTendrilHorrorEject : public UGA_EnemyPatternBase
{
	GENERATED_BODY()

public:
	UGA_BossTendrilHorrorEject();
};
