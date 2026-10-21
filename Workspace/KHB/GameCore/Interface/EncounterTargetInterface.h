#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "EncounterTargetInterface.generated.h"

UINTERFACE(BlueprintType)
class TEAMPROJECT_FIN_API UEncounterTargetInterface : public UInterface
{
	GENERATED_BODY()
};

/** 인카운터 볼륨이 구체적인 대상 클래스에 의존하지 않도록 하는 공통 계약. */
class TEAMPROJECT_FIN_API IEncounterTargetInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Encounter")
	bool RequestEncounter(AActor* EncounterInstigator);
};
