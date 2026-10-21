#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BossBattleHUDWidget.generated.h"

class ABossMonsterCharacter;
class ACommonEnemyCharacter;

class UAbilitySystemComponent;
class UProgressBar;
class UTextBlock;
struct FOnAttributeChangeData;

/**
 * GamePlayBase 안에 상주시킬 보스 전투 HUD의 공통 로직입니다.
 * 위젯 BP는 이름, 특성 텍스트, 특성 행과 기존 체력바를 같은 변수 이름으로 연결합니다.
 */
UCLASS(Abstract, Blueprintable)
class TEAMPROJECT_FIN_API UBossBattleHUDWidget : public UUserWidget
{
	GENERATED_BODY()


public:
	UFUNCTION(BlueprintCallable, Category = "Boss HUD")
	void ShowForBoss(ABossMonsterCharacter* InBoss);

	UFUNCTION(BlueprintCallable, Category = "Encounter HUD")
	void ShowForEncounter(ACommonEnemyCharacter* Enemy);

	UFUNCTION(BlueprintCallable, Category = "Boss HUD")
	void HideBoss();

protected:
	virtual void NativeConstruct() override;

	virtual void NativeDestruct() override;

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TraitText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> TraitRow;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> BossHealthBar;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> DelayedHealthBar;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss HUD|Delayed", meta = (ClampMin = "0.0"))
	float DelayedBarStartDelay = 0.20f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss HUD|Delayed", meta = (ClampMin = "0.01"))
	float DelayedBarDrainSpeed = 3.0f;

private:
	void RefreshTraitText();

	FText DisplayedTraits;

	bool bInitializedTraits = false;

	void BindAttributes();

	void UnbindAttributes();

	void RefreshHealthPercent(bool bAllowDelayedDamage = false);

	void ApplyBarPercents();

	void OnHealthChanged(const FOnAttributeChangeData& Data);

	void OnMaxHealthChanged(const FOnAttributeChangeData& Data);

	UFUNCTION()
	void HandleBossBattleHUDRequested(ABossMonsterCharacter* InBoss, bool bShow);

	UFUNCTION()
	void HandleEncounterBattleHUDRequested(ACommonEnemyCharacter* Enemy, bool bShow);

	TWeakObjectPtr<ACommonEnemyCharacter> Boss;

	bool bUsingSharedHealth = false;

	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;

	FDelegateHandle HealthChangedHandle;

	FDelegateHandle MaxHealthChangedHandle;

	float HealthPercent = 0.0f;

	float DelayedHealthPercent = 0.0f;

	float DelayedBarRemainingDelay = 0.0f;

	bool bInitializedPercent = false;
};
