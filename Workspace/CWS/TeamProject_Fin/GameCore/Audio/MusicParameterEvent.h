#pragma once

#include "CoreMinimal.h"
#include "PCGMap/MapBuild/SectorBuild/Event/SectorEvent.h"
#include "MusicParameterEvent.generated.h"

// PreEvents/PostEvents에 등록하는 음악 상태 변경 명령. 실행 상태는 DA에 보관하지 않는다.
UCLASS()
class TEAMPROJECT_FIN_API UMusicParameterEvent : public UConceptEvent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Audio|Music")
	float ParameterValue = 0.0f;

	virtual void Execute(const UObject* WorldContext) override;
};
