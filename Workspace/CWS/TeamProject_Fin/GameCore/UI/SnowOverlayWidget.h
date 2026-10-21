#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SnowOverlayWidget.generated.h"

class UImage;
class UMaterialInstanceDynamic;

/**
 * 눈보라/동결 상태 시 시야를 차단하는 오버레이 UI 클래스
 */
UCLASS()
class TEAMPROJECT_FIN_API USnowOverlayWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

protected:
	// 화면 전체를 덮는 눈 효과 이미지 (반드시 WBP에서 이름을 동일하게 맞출 것)
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> SnowOverlayImage;

	// 동적으로 제어할 머티리얼 인스턴스
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SnowMID;

	// 최대 체력 감소 디버프(눈보라 3단계 등) 적용 시 호출될 콜백 함수
	UFUNCTION()
	void OnHealthCapMultiplierChanged(float NewMultiplier);
};
