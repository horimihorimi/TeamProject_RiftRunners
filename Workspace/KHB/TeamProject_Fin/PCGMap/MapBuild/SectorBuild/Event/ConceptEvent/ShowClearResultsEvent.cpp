#include "ShowClearResultsEvent.h"

#include "GameCore/BaseGameState.h"
#include "Kismet/GameplayStatics.h"

void UShowClearResultsEvent::Execute(const UObject* WorldContext)
{
	if (!WorldContext || !UGameplayStatics::GetGameMode(WorldContext))
	{
		return;
	}

	if (ABaseGameState* GameState = Cast<ABaseGameState>(UGameplayStatics::GetGameState(WorldContext)))
	{
		GameState->ShowClearResults();
	}
}
