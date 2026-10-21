#include "GameCore/UI/BossBattleHUDWidget.h"

#include "AbilitySystemComponent.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GameCore/Attribute/CharacterAttributeSet.h"
#include "GameCore/Character/BossMonsterCharacter.h"
#include "GameCore/BaseGameState.h"
#include "PCGMap/MapBuild/SectorBuild/Event/EventManager.h"

void UBossBattleHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		GameInstance->GetSubsystem<UEventManager>()->OnEncounterBattleHUDRequested.AddUniqueDynamic(
		    this, &ThisClass::HandleEncounterBattleHUDRequested);
		GameInstance->GetSubsystem<UEventManager>()->OnBossBattleHUDRequested.AddUniqueDynamic(
		    this, &ThisClass::HandleBossBattleHUDRequested);
	}
}

void UBossBattleHUDWidget::ShowForBoss(ABossMonsterCharacter* InBoss)
{
	ShowForEncounter(InBoss);
}

void UBossBattleHUDWidget::ShowForEncounter(ACommonEnemyCharacter* InBoss)
{
	if (!IsValid(InBoss))
	{
		HideBoss();
		return;
	}
	UnbindAttributes();
	Boss = InBoss;
	const auto* GS = GetWorld()->GetGameState<ABaseGameState>();
	bUsingSharedHealth = GS && GS->bSharedEncounterHealth;
	AbilitySystem = InBoss->GetAbilitySystemComponent();
	if (!bUsingSharedHealth)
	{
		BindAttributes();
	}
	if (NameText)
	{
		NameText->SetText(InBoss->GetEnemyDisplayName());
	}
	bInitializedPercent = false;
	bInitializedTraits = false;
	RefreshTraitText();
	RefreshHealthPercent();
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UBossBattleHUDWidget::HandleEncounterBattleHUDRequested(ACommonEnemyCharacter* Enemy, bool bShow)
{
	if (bShow)
	{
		ShowForEncounter(Enemy);
	}
	else
	{
		HideBoss();
	}
}

void UBossBattleHUDWidget::HideBoss()
{
	UnbindAttributes();
	Boss.Reset();
	bInitializedTraits = false;
	DisplayedTraits = FText::GetEmpty();
	if (TraitText)
	{
		TraitText->SetText(FText::GetEmpty());
	}
	if (TraitRow)
	{
		TraitRow->SetVisibility(ESlateVisibility::Collapsed);
	}
	bUsingSharedHealth = false;
	SetVisibility(ESlateVisibility::Collapsed);
}

void UBossBattleHUDWidget::NativeDestruct()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		GameInstance->GetSubsystem<UEventManager>()->OnEncounterBattleHUDRequested.RemoveDynamic(
		    this, &ThisClass::HandleEncounterBattleHUDRequested);
		GameInstance->GetSubsystem<UEventManager>()->OnBossBattleHUDRequested.RemoveDynamic(
		    this, &ThisClass::HandleBossBattleHUDRequested);
	}

	UnbindAttributes();
	Super::NativeDestruct();
}

void UBossBattleHUDWidget::HandleBossBattleHUDRequested(ABossMonsterCharacter* InBoss, const bool bShow)
{
	if (bShow)
	{
		ShowForBoss(InBoss);
	}
	else
	{
		HideBoss();
	}
}

void UBossBattleHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (bUsingSharedHealth)
	{
		if (const auto* GS = GetWorld()->GetGameState<ABaseGameState>(); !GS || !GS->bSharedEncounterHealth)
		{
			HideBoss();
			return;
		}
	}
	if (!bUsingSharedHealth)
	{
		if (const auto* GS = GetWorld()->GetGameState<ABaseGameState>(); GS && GS->bSharedEncounterHealth)
		{
			UnbindAttributes();
			bUsingSharedHealth = true;
		}
	}
	if (bUsingSharedHealth)
	{
		RefreshHealthPercent(true);
	}
	else if (!Boss.IsValid())
	{
		HideBoss();
		return;
	}
	RefreshTraitText();
	if (DelayedBarRemainingDelay > 0.0f)
	{
		DelayedBarRemainingDelay = FMath::Max(0.0f, DelayedBarRemainingDelay - InDeltaTime);
		return;
	}
	if (DelayedHealthPercent > HealthPercent)
	{
		DelayedHealthPercent = FMath::FInterpTo(DelayedHealthPercent, HealthPercent, InDeltaTime, DelayedBarDrainSpeed);
		ApplyBarPercents();
	}
}

void UBossBattleHUDWidget::BindAttributes()
{
	UAbilitySystemComponent* ASC = AbilitySystem.Get();
	if (!IsValid(ASC))
	{
		return;
	}

	HealthChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetHealthAttribute())
	                          .AddUObject(this, &ThisClass::OnHealthChanged);
	MaxHealthChangedHandle =
	    ASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetMaxHealthAttribute())
	        .AddUObject(this, &ThisClass::OnMaxHealthChanged);
}

void UBossBattleHUDWidget::UnbindAttributes()
{
	if (UAbilitySystemComponent* ASC = AbilitySystem.Get())
	{
		if (HealthChangedHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetHealthAttribute())
			    .Remove(HealthChangedHandle);
		}
		if (MaxHealthChangedHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetMaxHealthAttribute())
			    .Remove(MaxHealthChangedHandle);
		}
	}

	HealthChangedHandle.Reset();
	MaxHealthChangedHandle.Reset();
	AbilitySystem.Reset();
	bInitializedPercent = false;
	DelayedBarRemainingDelay = 0.0f;
}

void UBossBattleHUDWidget::RefreshHealthPercent(bool bAllowDelayedDamage)
{
	UAbilitySystemComponent* ASC = AbilitySystem.Get();
	if ((!IsValid(ASC) && !bUsingSharedHealth) || !IsValid(BossHealthBar))
	{
		return;
	}

	const auto* GS = GetWorld()->GetGameState<ABaseGameState>();
	if (bUsingSharedHealth && !GS)
	{
		return;
	}
	const float Health = bUsingSharedHealth ? GS->SharedEncounterHealth
	                                        : ASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute());
	const float MaxHealth = bUsingSharedHealth
	                            ? GS->SharedEncounterMaxHealth
	                            : ASC->GetNumericAttribute(UCharacterAttributeSet::GetMaxHealthAttribute());
	const float NewPercent = MaxHealth > KINDA_SMALL_NUMBER ? FMath::Clamp(Health / MaxHealth, 0.0f, 1.0f) : 0.0f;
	if (bUsingSharedHealth && bInitializedPercent && FMath::IsNearlyEqual(NewPercent, HealthPercent))
	{
		return;
	}
	if (bInitializedPercent && bAllowDelayedDamage && NewPercent > KINDA_SMALL_NUMBER && NewPercent < HealthPercent)
	{
		DelayedHealthPercent = FMath::Max(DelayedHealthPercent, HealthPercent);
		DelayedBarRemainingDelay = DelayedBarStartDelay;
	}
	else
	{
		DelayedHealthPercent = NewPercent;
		DelayedBarRemainingDelay = 0.0f;
	}
	HealthPercent = NewPercent;
	bInitializedPercent = true;
	ApplyBarPercents();
}

void UBossBattleHUDWidget::ApplyBarPercents()
{
	if (IsValid(BossHealthBar))
	{
		BossHealthBar->SetPercent(HealthPercent);
	}
	if (IsValid(DelayedHealthBar))
	{
		DelayedHealthBar->SetPercent(DelayedHealthPercent);
	}
}

void UBossBattleHUDWidget::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	RefreshHealthPercent(true);
}

void UBossBattleHUDWidget::OnMaxHealthChanged(const FOnAttributeChangeData& Data)
{
	RefreshHealthPercent();
}

void UBossBattleHUDWidget::RefreshTraitText()
{
	FText Traits;
	if (bUsingSharedHealth)
	{
		if (const auto* GS = GetWorld()->GetGameState<ABaseGameState>())
		{
			Traits = GS->SharedEncounterTraits;
		}
	}
	else if (const auto* Enemy = Boss.Get())
	{
		Traits = Enemy->GetActiveTraitDisplayName();
	}
	if (bInitializedTraits && DisplayedTraits.EqualTo(Traits))
	{
		return;
	}
	DisplayedTraits = Traits;
	bInitializedTraits = true;
	if (TraitText)
	{
		TraitText->SetText(Traits);
	}
	if (TraitRow)
	{
		TraitRow->SetVisibility(Traits.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}
