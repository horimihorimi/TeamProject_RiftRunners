#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MonsterStatusWidget.generated.h"

class AMonsterCharacter;
class ACommonEnemyCharacter;
class UTextBlock;
class UWidget;
class UAbilitySystemComponent;
class UProgressBar;
struct FOnAttributeChangeData;

/**
 * 몬스터 머리 위 체력바의 공통 로직.
 * 실제 체력은 ASC AttributeSet 복제를 사용하고, 회색 딜레이 바는 각 클라이언트에서만 보간한다.
 */
UCLASS(Abstract, Blueprintable)
class TEAMPROJECT_FIN_API UMonsterStatusWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeForMonster(AMonsterCharacter* InMonster);
	void ResyncFromMonster();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MonsterNameText;

	/** TextBG는 TraitText를 포함하는 배경 컨테이너로 배치한다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> TextBG;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TraitText;

	/** 실제 체력. WBP에서 Is Variable을 켠 ProgressBar 이름을 HealthBar로 맞춘다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthBar;

	/** 직전 체력을 잠시 유지한 뒤 줄어드는 회색 바. WBP 변수 이름은 DelayedHealthBar. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> DelayedHealthBar;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Health Bar|Delayed")
	float DelayedBarStartDelay = 0.20f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Health Bar|Delayed")
	float DelayedBarDrainSpeed = 3.0f;

private:
	void RefreshMonsterName();
	void BindTraitDisplay();
	void RefreshTraitDisplay();
	TWeakObjectPtr<ACommonEnemyCharacter> TraitMonster;
	FDelegateHandle TraitDisplayChangedHandle;

	void BindAttributes();
	void UnbindAttributes();
	void RefreshHealthPercent(bool bAllowDelayedDamage);
	void ApplyBarPercents();
	void OnHealthChanged(const FOnAttributeChangeData& Data);
	void OnMaxHealthChanged(const FOnAttributeChangeData& Data);

	TWeakObjectPtr<AMonsterCharacter> Monster;
	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
	FDelegateHandle HealthChangedHandle;
	FDelegateHandle MaxHealthChangedHandle;

	float HealthPercent = 1.0f;
	float DelayedHealthPercent = 1.0f;
	float DelayedBarRemainingDelay = 0.0f;
	bool bInitializedPercent = false;
};
