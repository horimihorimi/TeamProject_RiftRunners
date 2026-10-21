#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DamageOverlayWidget.generated.h"

class UImage;
class UMaterialInstanceDynamic;

/**
 * 데미지 오버레이 (피격 및 체력 저하 시 시야 차단/Vignette) UI 클래스
 * 상시 띄워두는 HUD 컴포넌트이므로 UUserWidget을 상속합니다.
 */
UCLASS()
class TEAMPROJECT_FIN_API UDamageOverlayWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
	// 화면 전체를 덮는 피 효과 이미지 (반드시 WBP에서 이름을 동일하게 맞출 것)
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> BloodOverlayImage;

	// 동적으로 제어할 머티리얼 인스턴스
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BloodMID;

	// 피격 시 치솟았다가 0으로 서서히 깎이는 값
	float CurrentHitFlashAlpha;

	// 현재 체력 비율 (캐싱용)
	float CurrentHealthRatio;

	// 피격 시 깎이는 속도 (높을수록 빨리 사라짐)
	UPROPERTY(EditDefaultsOnly, Category = "Overlay Settings")
	float HitFlashDecaySpeed = 3.0f;

	// 데미지를 입었을 때 확 튀어오를 HitFlashAlpha 최대치
	UPROPERTY(EditDefaultsOnly, Category = "Overlay Settings")
	float MaxHitFlashAlpha = 0.3f;

	// 체력이 깎였을 때(데미지를 입었을 때) 호출될 콜백 함수
	void OnHealthChanged(const struct FOnAttributeChangeData& Data);

	// 실제 데미지를 입었을 때 호출될 콜백 함수 (피격 플래시용)
	UFUNCTION()
	void OnDamageTaken(float DamageAmount);
};
