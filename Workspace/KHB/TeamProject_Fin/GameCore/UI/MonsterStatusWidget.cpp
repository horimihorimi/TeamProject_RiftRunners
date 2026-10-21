#include "MonsterStatusWidget.h"

#include "AbilitySystemComponent.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GameCore/Character/CommonEnemyCharacter.h"
#include "GameCore/Attribute/CharacterAttributeSet.h"
#include "GameCore/Character/MonsterCharacter.h"

void UMonsterStatusWidget::InitializeForMonster(AMonsterCharacter* InMonster)
{
	if (Monster.Get() == InMonster && AbilitySystem.IsValid())
	{
		ResyncFromMonster();
		return;
	}

	UnbindAttributes();
	Monster = InMonster;
	AbilitySystem = IsValid(InMonster) ? InMonster->GetAbilitySystemComponent() : nullptr;
	BindAttributes();
	BindTraitDisplay();
	RefreshMonsterName();
	bInitializedPercent = false;
	RefreshHealthPercent(false);
}

void UMonsterStatusWidget::ResyncFromMonster()
{
	AMonsterCharacter* BoundMonster = Monster.Get();
	UnbindAttributes();
	AbilitySystem = IsValid(BoundMonster) ? BoundMonster->GetAbilitySystemComponent() : nullptr;
	BindAttributes();
	BindTraitDisplay();
	RefreshMonsterName();
	bInitializedPercent = false;
	RefreshHealthPercent(false);
}

void UMonsterStatusWidget::RefreshMonsterName()
{
	if (!IsValid(MonsterNameText)) { return; }
	const ACommonEnemyCharacter* Enemy = Cast<ACommonEnemyCharacter>(Monster.Get());
	const FText Name = IsValid(Enemy) ? Enemy->GetEnemyDisplayName() : FText::GetEmpty();
	MonsterNameText->SetText(Name);
	MonsterNameText->SetVisibility(Name.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UMonsterStatusWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 풀 대기 중 화면에서 제거되며 해제된 구독을 재등장할 때 복구한다.
	if (Monster.IsValid())
	{
		ResyncFromMonster();
	}
}

void UMonsterStatusWidget::NativeDestruct()
{
	UnbindAttributes();
	Super::NativeDestruct();
}

void UMonsterStatusWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (DelayedBarRemainingDelay > 0.0f)
	{
		DelayedBarRemainingDelay = FMath::Max(DelayedBarRemainingDelay - InDeltaTime, 0.0f);
		return;
	}

	if (DelayedHealthPercent > HealthPercent)
	{
		DelayedHealthPercent = FMath::FInterpTo(
			DelayedHealthPercent, HealthPercent, InDeltaTime, DelayedBarDrainSpeed);
		ApplyBarPercents();
	}
}

void UMonsterStatusWidget::BindAttributes()
{
	UAbilitySystemComponent* ASC = AbilitySystem.Get();
	if (!IsValid(ASC))
	{
		return;
	}

	HealthChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(
		UCharacterAttributeSet::GetHealthAttribute()).AddUObject(this, &UMonsterStatusWidget::OnHealthChanged);
	MaxHealthChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(
		UCharacterAttributeSet::GetMaxHealthAttribute()).AddUObject(this, &UMonsterStatusWidget::OnMaxHealthChanged);
}

void UMonsterStatusWidget::UnbindAttributes()
{
	if (ACommonEnemyCharacter* BoundTraitMonster = TraitMonster.Get())
	{
		BoundTraitMonster->OnTraitDisplayChanged.Remove(TraitDisplayChangedHandle);
	}
	TraitDisplayChangedHandle.Reset();
	TraitMonster.Reset();
	if (UAbilitySystemComponent* ASC = AbilitySystem.Get())
	{
		if (HealthChangedHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetHealthAttribute()).Remove(HealthChangedHandle);
		}
		if (MaxHealthChangedHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetMaxHealthAttribute()).Remove(MaxHealthChangedHandle);
		}
	}

	HealthChangedHandle.Reset();
	MaxHealthChangedHandle.Reset();
	AbilitySystem.Reset();
}

void UMonsterStatusWidget::RefreshHealthPercent(const bool bAllowDelayedDamage)
{
	UAbilitySystemComponent* ASC = AbilitySystem.Get();
	if (!IsValid(ASC))
	{
		return;
	}

	const float Health = ASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute());
	const float MaxHealth = ASC->GetNumericAttribute(UCharacterAttributeSet::GetMaxHealthAttribute());
	const float NewPercent = MaxHealth > KINDA_SMALL_NUMBER
		? FMath::Clamp(Health / MaxHealth, 0.0f, 1.0f) : 0.0f;

	if (!bInitializedPercent)
	{
		HealthPercent = NewPercent;
		DelayedHealthPercent = NewPercent;
		bInitializedPercent = true;
	}
	else if (NewPercent <= KINDA_SMALL_NUMBER)
	{
		// 사망 시에는 남아 있던 회색 피해 딜레이 바도 즉시 제거한다.
		HealthPercent = 0.0f;
		DelayedHealthPercent = 0.0f;
		DelayedBarRemainingDelay = 0.0f;
	}
	else if (bAllowDelayedDamage && NewPercent < HealthPercent)
	{
		// 연속 피격 시 회색 바는 이전에 남은 더 높은 값에서 시작한다.
		DelayedHealthPercent = FMath::Max(DelayedHealthPercent, HealthPercent);
		HealthPercent = NewPercent;
		DelayedBarRemainingDelay = DelayedBarStartDelay;
	}
	else
	{
		// 회복/최대 체력 변경은 회색 바도 즉시 실제값에 맞춘다.
		HealthPercent = NewPercent;
		DelayedHealthPercent = NewPercent;
		DelayedBarRemainingDelay = 0.0f;
	}

	ApplyBarPercents();
}

void UMonsterStatusWidget::ApplyBarPercents()
{
	if (IsValid(HealthBar))
	{
		HealthBar->SetPercent(HealthPercent);
	}
	if (IsValid(DelayedHealthBar))
	{
		DelayedHealthBar->SetPercent(DelayedHealthPercent);
	}
}

void UMonsterStatusWidget::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	RefreshHealthPercent(true);
}

void UMonsterStatusWidget::OnMaxHealthChanged(const FOnAttributeChangeData& Data)
{
	RefreshHealthPercent(false);
}

void UMonsterStatusWidget::BindTraitDisplay()
{
	TraitMonster = Cast<ACommonEnemyCharacter>(Monster.Get());
	if (ACommonEnemyCharacter* BoundTraitMonster = TraitMonster.Get())
	{
		TraitDisplayChangedHandle = BoundTraitMonster->OnTraitDisplayChanged.AddUObject(
			this, &UMonsterStatusWidget::RefreshTraitDisplay);
	}
	// 위젯 초기화와 복제 순서에 관계없이 현재 표시 이름을 반영한다.
	RefreshTraitDisplay();
}

void UMonsterStatusWidget::RefreshTraitDisplay()
{
	const ACommonEnemyCharacter* BoundTraitMonster = TraitMonster.Get();
	const FText DisplayName = IsValid(BoundTraitMonster)
		? BoundTraitMonster->GetActiveTraitDisplayName() : FText::GetEmpty();
	UE_LOG(LogTemp, Log,
		TEXT("[EnemyTraitUI] Refresh | Monster=%s | Widget=%s | Name=%s | TraitText=%s | TextBG=%s"),
		*GetNameSafe(Monster.Get()), *GetNameSafe(this), *DisplayName.ToString(),
		*GetNameSafe(TraitText.Get()), *GetNameSafe(TextBG.Get()));
	const ESlateVisibility TraitVisibility = DisplayName.IsEmpty()
		? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible;
	if (IsValid(TraitText))
	{
		TraitText->SetText(DisplayName);
		TraitText->SetVisibility(TraitVisibility);
	}
	if (IsValid(TextBG))
	{
		TextBG->SetVisibility(TraitVisibility);
	}
}
