#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "EncounterIntroTarget.generated.h"

UINTERFACE(BlueprintType)
class TEAMPROJECT_FIN_API UEncounterIntroTarget : public UInterface
{
	GENERATED_BODY()
};

/**
 * 조우 뒤 등장 연출(시네마틱)을 거쳐 전투를 시작하는 대상의 계약입니다.
 *
 * 조우 요청 자체는 기존 IEncounterTargetInterface를 그대로 쓴다. 연출을 재생하는 쪽
 * (레벨의 아레나 액터, 시퀀스 등)은 대상의 구체 클래스를 모른 채 연출 종료만 알린다.
 * 기존 인터페이스에 함수를 더하지 않은 이유는 연출이 없는 일반 몬스터까지 영향을 받기 때문이다.
 */
class TEAMPROJECT_FIN_API IEncounterIntroTarget
{
	GENERATED_BODY()

public:
	/** 등장 연출이 끝났음을 알립니다. 대상은 이때 휴면을 풀고 최초 타깃을 정해 전투를 시작합니다. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Encounter")
	void FinishEncounterIntro();
};
