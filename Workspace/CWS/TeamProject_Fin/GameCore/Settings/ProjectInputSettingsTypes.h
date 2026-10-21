#pragma once

#include "CoreMinimal.h"
#include "ProjectInputSettingsTypes.generated.h"

UENUM(BlueprintType)
enum class EProjectInputAction : uint8
{
	Sprint UMETA(DisplayName = "달리기"),
	Crouch UMETA(DisplayName = "앉기"),
	Aim UMETA(DisplayName = "조준")
};

UENUM(BlueprintType)
enum class EProjectInputMode : uint8
{
	Hold UMETA(DisplayName = "홀드"),
	Toggle UMETA(DisplayName = "토글")
};
