#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ReviveGaugeWidget.generated.h"

class UHorizontalBox;
class UOverlay;

/** 고정된 빈칸 위에서 끝부분 알맹이와 글로우를 함께 축소하는 게이지입니다. */
UCLASS(meta = (DisableNativeTick))
class TEAMPROJECT_FIN_API UReviveGaugeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 생성 직후 현재값을 전달하고, 이후 값이 바뀔 때만 호출합니다. 범위는 0~1입니다. */
	UFUNCTION(BlueprintCallable, Category = "Gauge")
	void UpdateGauge(float InPercent);

	/** 시간, 체력, 스태미나처럼 현재값과 최대값을 그대로 연결할 때 사용합니다. */
	UFUNCTION(BlueprintCallable, Category = "Gauge")
	void UpdateGaugeFromValues(float CurrentValue, float MaximumValue);

	UFUNCTION(BlueprintPure, Category = "Gauge")
	float GetGaugePercent() const { return Percent; }

protected:
	virtual void NativePreConstruct() override;

	/** WBP에 같은 이름의 HorizontalBox를 배치하고 각 칸에 FillGroup_0~19를 둡니다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHorizontalBox> SegmentBox;

	/** 디자이너 미리보기 전용 값입니다. 실제 플레이 값은 UpdateGauge에서 전달받습니다. */
	UPROPERTY(EditDefaultsOnly, Category = "Gauge", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PreviewPercent = 0.5f;

private:
	void CacheFillGroups();
	void ApplyPercent(float InPercent);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UOverlay>> FillGroups;

	float Percent = 0.0f;
};
