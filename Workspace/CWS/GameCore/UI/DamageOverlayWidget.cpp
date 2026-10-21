#include "DamageOverlayWidget.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameCore/Character/BaseCharacter.h"
#include "AbilitySystemComponent.h"
#include "GameCore/Attribute/CharacterAttributeSet.h"
#include "Kismet/GameplayStatics.h"

void UDamageOverlayWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 1. 머티리얼 인스턴스 초기화
	if (BloodOverlayImage)
	{
		BloodMID = BloodOverlayImage->GetDynamicMaterial();
	}

	CurrentHitFlashAlpha = 0.0f;
	CurrentHealthRatio = 1.0f;

	// 2. 로컬 플레이어의 GAS 연동 (체력 감지)
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (ABaseCharacter* LocalPlayer = Cast<ABaseCharacter>(PC->GetPawn()))
		{
			if (UAbilitySystemComponent* ASC = LocalPlayer->GetAbilitySystemComponent())
			{
				// 체력 어트리뷰트 변경 델리게이트 바인딩 (화면 테두리 용)
				ASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetHealthAttribute())
					.AddUObject(this, &UDamageOverlayWidget::OnHealthChanged);
				
				// 실제 데미지 피격 델리게이트 바인딩 (피 튀기는 이펙트 용)
				LocalPlayer->OnDamageTaken.AddDynamic(this, &UDamageOverlayWidget::OnDamageTaken);
				
				// 초기 체력 비율 캐싱
				float Health = ASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute());
				float MaxHealth = ASC->GetNumericAttribute(UCharacterAttributeSet::GetMaxHealthAttribute());
				if (MaxHealth > 0.0f)
				{
					CurrentHealthRatio = Health / MaxHealth;
				}
				
				if (BloodMID)
				{
					BloodMID->SetScalarParameterValue(FName("HealthRatio"), CurrentHealthRatio);
				}
			}
		}
	}
}

void UDamageOverlayWidget::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (ABaseCharacter* LocalPlayer = Cast<ABaseCharacter>(PC->GetPawn()))
		{
			if (UAbilitySystemComponent* ASC = LocalPlayer->GetAbilitySystemComponent())
			{
				float MaxHealth = ASC->GetNumericAttribute(UCharacterAttributeSet::GetMaxHealthAttribute());
				
				if (MaxHealth > 0.0f)
				{
					// 이제 체력 변경에서는 피격 플래시를 발생시키지 않고 비율만 업데이트합니다. (최대 체력이 깎일 때도 자연스럽게 비율 적용)
					CurrentHealthRatio = Data.NewValue / MaxHealth;
					
					if (BloodMID)
					{
						BloodMID->SetScalarParameterValue(FName("HealthRatio"), CurrentHealthRatio);
					}
				}
			}
		}
	}
}

void UDamageOverlayWidget::OnDamageTaken(float DamageAmount)
{
	if (DamageAmount > 0.0f)
	{
		// 실제 데미지를 입었을 때만 화면에 피가 튀도록 합니다.
		CurrentHitFlashAlpha = MaxHitFlashAlpha;
	}
}

void UDamageOverlayWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 3. 피격 플래시 자연스러운 감쇠 (보간)
	if (CurrentHitFlashAlpha > 0.0f)
	{
		CurrentHitFlashAlpha = FMath::FInterpTo(CurrentHitFlashAlpha, 0.0f, InDeltaTime, HitFlashDecaySpeed);
		
		// 너무 작아지면 깔끔하게 0으로 고정
		if (CurrentHitFlashAlpha < 0.01f)
		{
			CurrentHitFlashAlpha = 0.0f;
		}

		if (BloodMID)
		{
			BloodMID->SetScalarParameterValue(FName("HitFlashAlpha"), CurrentHitFlashAlpha);
		}
	}
}
