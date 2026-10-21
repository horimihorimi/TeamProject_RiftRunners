#include "GameCore/Audio/MusicParameterEvent.h"

#include "GameCore/BaseGameState.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectMusicEvent, Log, All);

void UMusicParameterEvent::Execute(const UObject* WorldContext)
{
	ABaseGameState* GameState = Cast<ABaseGameState>(UGameplayStatics::GetGameState(WorldContext));
	if (!GameState)
	{
		UE_LOG(LogProjectMusicEvent, Warning, TEXT("음악 이벤트 실행에 BaseGameState가 필요합니다: %s"), *GetName());
		return;
	}
	if (GameState->HasAuthority())
	{
		GameState->SetMusicState(ParameterValue);
	}
}
