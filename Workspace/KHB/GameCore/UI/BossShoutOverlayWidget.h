#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Blueprint/UserWidget.h"
#include "BossShoutOverlayWidget.generated.h"

class UImage;
class UMaterialInstanceDynamic;
class UAbilitySystemComponent;

/**
 * 보스 샤우트에 맞은 로컬 플레이어의 화면 가장자리 효과를 제어합니다.
 * 위젯은 HUD에 상시 배치하고, ActivateShout/DeactivateShout만 호출합니다.
 */
UCLASS()
class TEAMPROJECT_FIN_API UBossShoutOverlayWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** 샤우트 경직 시작 시 호출합니다. */
	UFUNCTION(BlueprintCallable, Category = "Boss Shout Overlay")
	void ActivateShout();

	/** 샤우트 경직 종료 시 호출합니다. */
	UFUNCTION(BlueprintCallable, Category = "Boss Shout Overlay")
	void DeactivateShout();

	UFUNCTION(BlueprintPure, Category = "Boss Shout Overlay")
	bool IsShoutActive() const { return bShoutActive; }

protected:
	/** 전체 화면을 덮는 Image. WBP의 위젯 이름도 ShoutOverlayImage로 맞춥니다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> ShoutOverlayImage;

	/** 평상시에는 0, 샤우트 중에는 ShoutIntensity로 향합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss Shout Overlay", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ShoutIntensity = 0.45f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss Shout Overlay", meta = (ClampMin = "0.0"))
	float FadeInSpeed = 8.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss Shout Overlay", meta = (ClampMin = "0.0"))
	float FadeOutSpeed = 3.0f;

private:
	void TryBindToStaggerTag();
	void OnStaggeredTagChanged(FGameplayTag Tag, int32 NewCount);
	void SetMaterialIntensity(float NewIntensity);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ShoutMID;

	TWeakObjectPtr<UAbilitySystemComponent> BoundAbilitySystemComponent;

	float CurrentIntensity = 0.0f;
	bool bShoutActive = false;
};
