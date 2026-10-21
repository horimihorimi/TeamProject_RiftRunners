#pragma once

#include "CoreMinimal.h"
#include "../SectorEvent.h"
#include "GameClearEvent.generated.h"

//게임 클리어를 전역으로 알리는 이벤트. 창을 직접 띄우지 않는다. UI 등은 EventManager의 OnGameClear에 바인드한다
UCLASS()
class UGameClearEvent : public UConceptEvent
{
	GENERATED_BODY()
public:
	virtual void Execute(const UObject* WorldContext) override;
};
