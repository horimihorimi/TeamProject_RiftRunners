#pragma once

#include "CoreMinimal.h"
#include "ProjectAudioTypes.generated.h"

UENUM(BlueprintType)
enum class EProjectAudioCategory : uint8
{
	Master,
	Music,
	SFX,
	UI
};
