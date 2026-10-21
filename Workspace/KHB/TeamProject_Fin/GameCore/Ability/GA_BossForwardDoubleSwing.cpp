#include "GA_BossForwardDoubleSwing.h"

#include "GameCore/Tags/GameplayTagSystem.h"

UGA_BossForwardDoubleSwing::UGA_BossForwardDoubleSwing()
{
	DefaultDamageAttackTag = GameplayTagSystem::Action_AI_Boss_ForwardDoubleSwing;
	SetAssetTags(FGameplayTagContainer(GameplayTagSystem::Ability_Enemy_Boss_ForwardDoubleSwing));
}
