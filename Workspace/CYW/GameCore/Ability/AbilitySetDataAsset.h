#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AbilitySetDataAsset.generated.h"

class UGameplayAbility;

UCLASS()
class TEAMPROJECT_FIN_API UAbilitySetDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities")
	TArray<TSubclassOf<UGameplayAbility>> Abilities;
};
