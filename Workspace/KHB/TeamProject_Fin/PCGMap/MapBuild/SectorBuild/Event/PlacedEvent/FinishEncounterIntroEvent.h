#pragma once

#include "CoreMinimal.h"
#include "../SectorEvent.h"
#include "FinishEncounterIntroEvent.generated.h"

/**
 * 시네마틱 등 외부 등장 연출이 끝났음을 조우 대상에게 알리는 레벨 배치 이벤트입니다.
 *
 * APlayCinematicEvent의 Post Events에 배치한다. 자연 종료와 스킵은 모두 서버의
 * 동일한 Post Events 경로를 지나므로, 여기서 FinishEncounterIntro를 호출하면
 * 대상의 전투가 한 번만 시작된다.
 */

UCLASS(Blueprintable)
class TEAMPROJECT_FIN_API AFinishEncounterIntroEvent : public AScenePlacedEvent
{
	GENERATED_BODY()


public:
	/** FinishEncounterIntro를 받을 레벨 배치 대상. EncounterIntroTarget 인터페이스 구현이 필요하다. */
	UPROPERTY(EditInstanceOnly, Category = "Encounter")
	TObjectPtr<AActor> EncounterIntroTarget = nullptr;

	virtual void Execute(const UObject* WorldContext = nullptr) override;
};
