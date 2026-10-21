#pragma once

#include "CoreMinimal.h"
#include "../SectorEvent.h"
#include "BlueprintTriggerEvent.generated.h"

// 자유롭게 블루프린트에서 기능을 구현할 수 있는 범용 커스텀 이벤트
UCLASS(Blueprintable)
class ABlueprintTriggerEvent : public AScenePlacedEvent
{
	GENERATED_BODY()

public:
	virtual void Execute(const UObject* WorldContext = nullptr) override;

protected:
	// 이벤트가 발동(Execute)될 때 블루프린트에서 실행될 노드입니다.
	UFUNCTION(BlueprintImplementableEvent, Category = "Sector")
	void ReceiveExecute();
};
