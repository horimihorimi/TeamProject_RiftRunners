#include "GA_BossTendrilHorrorEject.h"

#include "GameCore/Tags/GameplayTagSystem.h"

UGA_BossTendrilHorrorEject::UGA_BossTendrilHorrorEject()
{
	DefaultDamageAttackTag = GameplayTagSystem::Action_AI_Boss_TendrilHorrorEject;
	SetAssetTags(FGameplayTagContainer(GameplayTagSystem::Ability_Enemy_Boss_TendrilHorrorEject));
}
