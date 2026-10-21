#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "GameCore/Settings/ProjectInputSettingsTypes.h"
#include "ProjectInputSaveGame.generated.h"

UCLASS()
class TEAMPROJECT_FIN_API UProjectInputSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame)
	float LookSensitivity = 1.0f;

	UPROPERTY(SaveGame)
	float AimSensitivity = 1.0f;

	UPROPERTY(SaveGame)
	EProjectInputMode SprintMode = EProjectInputMode::Hold;

	UPROPERTY(SaveGame)
	EProjectInputMode CrouchMode = EProjectInputMode::Hold;

	UPROPERTY(SaveGame)
	EProjectInputMode AimMode = EProjectInputMode::Hold;
};
