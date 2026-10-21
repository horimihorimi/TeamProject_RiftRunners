#include "SnowOverlayWidget.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameCore/Character/BaseCharacter.h"
#include "AbilitySystemComponent.h"
#include "GameCore/Attribute/CharacterAttributeSet.h"

void USnowOverlayWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 1. 머티리얼 인스턴스 초기화
	if (SnowOverlayImage)
	{
		SnowMID = SnowOverlayImage->GetDynamicMaterial();
	}

	// 2. 로컬 플레이어의 GAS 연동 (최대 체력 배율 감지)
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (ABaseCharacter* LocalPlayer = Cast<ABaseCharacter>(PC->GetPawn()))
		{
			if (UAbilitySystemComponent* ASC = LocalPlayer->GetAbilitySystemComponent())
			{
				// 최대 체력 배율 델리게이트 바인딩 (눈보라/동결 오버레이 용)
				LocalPlayer->OnHealthCapMultiplierChanged.AddDynamic(this, &USnowOverlayWidget::OnHealthCapMultiplierChanged);
				
				// 초기값 적용
				float CapMultiplier = ASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthCapMultiplierAttribute());
				OnHealthCapMultiplierChanged(CapMultiplier);
			}
		}
	}
}

void USnowOverlayWidget::OnHealthCapMultiplierChanged(float NewMultiplier)
{
	if (SnowMID)
	{
		// HealthCapMultiplier는 정상(1.0) ~ 눈보라3단계 풀스택(0.75) 범위입니다.
		// 이 값을 반전시키고 증폭해서 1.0(정상)일 때 Alpha=0.0, 0.75(최대디버프)일 때 Alpha=1.0이 되도록 만듭니다.
		// 계산식: (1.0 - Multiplier) * 4.0
		// 예: Multiplier 1.0 -> (0.0) * 4 = 0.0
		// 예: Multiplier 0.75 -> (0.25) * 4 = 1.0
		float SnowAlpha = FMath::Clamp((1.0f - NewMultiplier) * 4.0f, 0.0f, 1.0f);
		
		SnowMID->SetScalarParameterValue(FName("SnowAlpha"), SnowAlpha);
	}
}
