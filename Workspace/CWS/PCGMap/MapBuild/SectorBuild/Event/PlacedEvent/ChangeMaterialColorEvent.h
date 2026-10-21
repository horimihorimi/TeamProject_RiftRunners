#pragma once

#include "CoreMinimal.h"
#include "../SectorEvent.h"
#include "ChangeMaterialColorEvent.generated.h"

USTRUCT(BlueprintType)
struct FMaterialColorChangeTarget
{
	GENERATED_BODY()

	// 색상을 변경할 대상 액터 (월드에 배치된 액터를 스포이드로 찍으세요)
	UPROPERTY(EditInstanceOnly, Category = "MaterialEvent")
	TObjectPtr<AActor> TargetActor = nullptr;

	// 대상 액터 내에서 머티리얼을 변경할 메쉬 컴포넌트의 이름 (비워두면 모든 메쉬를 변경합니다)
	UPROPERTY(EditInstanceOnly, Category = "MaterialEvent")
	FName TargetComponentName;

	// 변경할 머티리얼의 파라미터 이름 (예: "BaseColor", "EmissiveColor" 등)
	UPROPERTY(EditInstanceOnly, Category = "MaterialEvent")
	FName ColorParameterName = TEXT("Color");

	// 적용할 새로운 색상
	UPROPERTY(EditInstanceOnly, Category = "MaterialEvent")
	FLinearColor NewColor = FLinearColor::Red;

	// 변경할 컴포넌트의 머티리얼 인덱스 (기본값 0)
	UPROPERTY(EditInstanceOnly, Category = "MaterialEvent")
	int32 MaterialIndex = 0;
};

/**
 * 하나 이상의 액터의 머티리얼 파라미터(색상)를 변경하는 이벤트입니다.
 */
UCLASS()
class TEAMPROJECT_FIN_API AChangeMaterialColorEvent : public AScenePlacedEvent
{
	GENERATED_BODY()

public:
	virtual void Execute(const UObject* WorldContext = nullptr) override;

	// 복제된 버튼 상태를 받은 클라이언트가 머티리얼 시각 효과만 적용할 때 사용합니다.
	void ApplyMaterialColorChanges();

protected:
	// 색상을 변경할 대상 목록. 각 항목마다 대상 액터와 색상을 별도로 지정할 수 있습니다.
	UPROPERTY(EditInstanceOnly, Category = "MaterialEvent")
	TArray<FMaterialColorChangeTarget> AdditionalMaterialTargets;
};
