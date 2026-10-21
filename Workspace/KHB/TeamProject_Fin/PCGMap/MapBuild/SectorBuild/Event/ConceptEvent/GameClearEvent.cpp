#include "GameClearEvent.h"

#include "Kismet/GameplayStatics.h"
#include "GameCore/BaseGameState.h"

void UGameClearEvent::Execute(const UObject* WorldContext)
{
	// GameMode가 잡히면 서버다. 배치 이벤트가 서버에서만 Execute를 부른다
	if (!UGameplayStatics::GetGameMode(WorldContext))
	{
		return;
	}

	// EventManager는 머신마다 따로 있다. GameState를 거쳐야 클라에도 창이 뜬다
	if (ABaseGameState* GameState = Cast<ABaseGameState>(UGameplayStatics::GetGameState(WorldContext)))
	{
		GameState->SetGameCleared();
	}
}
