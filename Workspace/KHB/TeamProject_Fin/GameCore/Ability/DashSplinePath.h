#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DashSplinePath.generated.h"

class USplineComponent;

/**
 * 레벨에 배치하지 않는 대쉬 경로 프리셋이다.
 * 이 클래스를 블루프린트로 상속해 Spline 포인트만 편집한다.
 */
UCLASS(BlueprintType)
class TEAMPROJECT_FIN_API ADashSplinePath : public AActor
{
	GENERATED_BODY()

public:
	ADashSplinePath();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dash Path")
	TObjectPtr<USplineComponent> Spline;
};
