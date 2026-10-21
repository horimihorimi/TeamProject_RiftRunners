#pragma once

#include "CoreMinimal.h"
#include "../SectorEvent.h"
#include "ShowClearResultsEvent.generated.h"

// 포탈의 PreEvents에 연결한다. UI는 OnShowClearResults에 바인딩한다.
UCLASS(BlueprintType)
class TEAMPROJECT_FIN_API UShowClearResultsEvent : public UConceptEvent
{
	GENERATED_BODY()

public:
	virtual void Execute(const UObject* WorldContext) override;
};
