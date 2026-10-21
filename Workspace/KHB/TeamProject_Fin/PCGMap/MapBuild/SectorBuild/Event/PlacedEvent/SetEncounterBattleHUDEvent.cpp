#include "SetEncounterBattleHUDEvent.h"
#include "GameCore/BaseGameState.h"
#include "GameCore/Character/CommonEnemyCharacter.h"

void ASetEncounterBattleHUDEvent::Execute(const UObject* WorldContext)
{
	Super::Execute(WorldContext);
	if (!HasAuthority() || (bShow && !IsValid(TargetEnemy)))
	{
		return;
	}
	if (auto* GS = GetWorld()->GetGameState<ABaseGameState>())
	{
		GS->MulticastEncounterBattleHUD(TargetEnemy, bShow);
	}
}
