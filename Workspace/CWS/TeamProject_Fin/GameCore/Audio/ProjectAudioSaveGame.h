#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "ProjectAudioSaveGame.generated.h"

UCLASS()
class TEAMPROJECT_FIN_API UProjectAudioSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame)
	float Master = 1.0f;

	UPROPERTY(SaveGame)
	float Music = 1.0f;

	UPROPERTY(SaveGame)
	float SFX = 1.0f;

	UPROPERTY(SaveGame)
	float UI = 1.0f;
};
