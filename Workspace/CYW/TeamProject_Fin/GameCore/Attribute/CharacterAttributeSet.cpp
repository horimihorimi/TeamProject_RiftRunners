// 저작권: Epic Games, Inc. 모든 권리 보유.

#include "CharacterAttributeSet.h"
#include "GameCore/Character/CommonEnemyCharacter.h"
#include "GameCore/Combat/EnemySplitComponent.h"

#include "GameCore/Character/PlayerCharacter.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"
#include "../Combat/CombatDamageEventRouter.h"
#include "GameFramework/Actor.h"
#include "../Tags/GameplayTagSystem.h"

UCharacterAttributeSet::UCharacterAttributeSet()
{
	InitHealth(100.0f);
	InitMaxHealth(100.0f);
	InitStamina(100.0f);
	InitMaxStamina(100.0f);
	InitStaminaRegenRate(20.0f);
	InitMovementSpeed(300.0f);
	InitArmor(0.0f);
	InitAttackPower(1.0f); // 공격력 배수 (기본 1.0 = 100%)
	InitFireIntervalMultiplier(1.0f);
	InitReloadMontageRateMultiplier(1.0f);
	InitStaminaCostMultiplier(1.0f);
	InitDamage(0.0f);
	InitHealthCapMultiplier(1.0f);
	InitStaminaCapMultiplier(1.0f);
}

void UCharacterAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UCharacterAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCharacterAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCharacterAttributeSet, Stamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCharacterAttributeSet, MaxStamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCharacterAttributeSet, StaminaRegenRate, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCharacterAttributeSet, StaminaCostMultiplier, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCharacterAttributeSet, MovementSpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCharacterAttributeSet, Armor, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCharacterAttributeSet, AttackPower, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCharacterAttributeSet, FireIntervalMultiplier, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCharacterAttributeSet, ReloadMontageRateMultiplier, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCharacterAttributeSet, HealthCapMultiplier, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCharacterAttributeSet, StaminaCapMultiplier, COND_None, REPNOTIFY_Always);
}

void UCharacterAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetMaxHealthAttribute())
	{
		AdjustAttributeForMaxChange(Health, MaxHealth, NewValue, GetHealthAttribute());
		NewValue = FMath::Max(NewValue, 1.0f);
	}
	else if (Attribute == GetMaxStaminaAttribute())
	{
		AdjustAttributeForMaxChange(Stamina, MaxStamina, NewValue, GetStaminaAttribute());
		NewValue = FMath::Max(NewValue, 1.0f);
	}
	else if (Attribute == GetHealthCapMultiplierAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.75f, 1.0f);
	}
	else if (Attribute == GetStaminaCapMultiplierAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.75f, 1.0f);
	}
	else if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth() * GetHealthCapMultiplier());
	}
	else if (Attribute == GetStaminaAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxStamina() * GetStaminaCapMultiplier());
	}
	else if (Attribute == GetMovementSpeedAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
	else if (Attribute == GetStaminaRegenRateAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
	else if (Attribute == GetFireIntervalMultiplierAttribute() ||
	         Attribute == GetReloadMontageRateMultiplierAttribute() || Attribute == GetStaminaCostMultiplierAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.01f);
	}
}

void UCharacterAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	const FGameplayAttribute& ModifiedAttribute = Data.EvaluatedData.Attribute;

	if (ModifiedAttribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth() * GetHealthCapMultiplier()));
	}
	else if (ModifiedAttribute == GetStaminaAttribute())
	{
		SetStamina(FMath::Clamp(GetStamina(), 0.0f, GetMaxStamina() * GetStaminaCapMultiplier()));
	}
	else if (ModifiedAttribute == GetMaxHealthAttribute() || ModifiedAttribute == GetHealthCapMultiplierAttribute())
	{
		if (ModifiedAttribute == GetHealthCapMultiplierAttribute())
		{
			SetHealthCapMultiplier(FMath::Clamp(GetHealthCapMultiplier(), 0.75f, 1.0f));
		}
		SetMaxHealth(FMath::Max(GetMaxHealth(), 1.0f));
		SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth() * GetHealthCapMultiplier()));
	}
	else if (ModifiedAttribute == GetMaxStaminaAttribute() || ModifiedAttribute == GetStaminaCapMultiplierAttribute())
	{
		if (ModifiedAttribute == GetStaminaCapMultiplierAttribute())
		{
			SetStaminaCapMultiplier(FMath::Clamp(GetStaminaCapMultiplier(), 0.75f, 1.0f));
		}
		SetMaxStamina(FMath::Max(GetMaxStamina(), 1.0f));
		SetStamina(FMath::Clamp(GetStamina(), 0.0f, GetMaxStamina() * GetStaminaCapMultiplier()));
	}
	else if (ModifiedAttribute == GetMovementSpeedAttribute())
	{
		SetMovementSpeed(FMath::Max(GetMovementSpeed(), 0.0f));
	}
	else if (ModifiedAttribute == GetDamageAttribute())
	{
		// 메타 어트리뷰트: 누적된 데미지를 Health에서 깎고 즉시 0으로 초기화
		const float LocalDamageDone = GetDamage();
		SetDamage(0.0f);

		if (LocalDamageDone > 0.0f)
		{
			ACommonEnemyCharacter* SharedEnemy =
			    Cast<ACommonEnemyCharacter>(GetOwningAbilitySystemComponent()->GetAvatarActor());
			UEnemySplitHealthState* SharedEncounter = SharedEnemy ? SharedEnemy->GetSharedHealthPool() : nullptr;
			const float PreviousHealth = SharedEncounter ? SharedEncounter->GetHealth() : GetHealth();
			const float NewHealth = SharedEncounter
			                            ? SharedEncounter->ApplySharedDamage(LocalDamageDone)
			                            : FMath::Clamp(PreviousHealth - LocalDamageDone, 0.0f, GetMaxHealth());
			const float AppliedDamage = PreviousHealth - NewHealth;
			SetHealth(NewHealth);

			if (APlayerCharacter* PlayerCharacter =
			        Cast<APlayerCharacter>(GetOwningAbilitySystemComponent()->GetAvatarActor()))
			{
				PlayerCharacter->ClientNotifyDamageTaken(AppliedDamage);
			}

			const FGameplayTagContainer& DynamicTags = Data.EffectSpec.GetDynamicAssetTags();

			// --- 데미지 타입 태그 감지 ---
			const bool bIsHeavyGroggy = DynamicTags.HasTag(GameplayTagSystem::Damage_Type_HeavyGroggy);
			const bool bIsLightGroggy = DynamicTags.HasTag(GameplayTagSystem::Damage_Type_Groggy);

			// --- 캐릭터 사망 여부 감지 ---
			const bool bWasAlive = PreviousHealth > 0.0f;
			const bool bJustDied = bWasAlive && NewHealth <= 0.0f;

			UAbilitySystemComponent* TargetASC = GetOwningAbilitySystemComponent();
			AActor* TargetActor = TargetASC ? TargetASC->GetAvatarActor() : nullptr;
			AActor* InstigatorActor = Data.EffectSpec.GetEffectContext().GetInstigator();
			CombatDamageEventRouter::RouteDamageExecuted(TargetASC, TargetActor, InstigatorActor, Data, LocalDamageDone,
			                                             AppliedDamage, PreviousHealth, NewHealth, GetMaxHealth());
			if (SharedEncounter && NewHealth <= 0.f)
			{
				SharedEncounter->FinishSharedDeath(TargetActor, InstigatorActor, Data.EffectSpec.GetEffectContext());
			}
		}
	}
	if (ModifiedAttribute == GetHealthAttribute() || ModifiedAttribute == GetMaxHealthAttribute() ||
	    ModifiedAttribute == GetHealthCapMultiplierAttribute())
	{
		if (auto* Enemy = Cast<ACommonEnemyCharacter>(GetOwningAbilitySystemComponent()->GetAvatarActor()))
		{
			if (auto* Encounter = Enemy->GetSharedHealthPool())
			{
				Encounter->UpdateSharedVitals(GetHealth(), GetMaxHealth());
				if (GetHealth() <= 0.f)
				{
					Encounter->FinishSharedDeath(nullptr, Data.EffectSpec.GetEffectContext().GetInstigator(),
					                             Data.EffectSpec.GetEffectContext());
				}
			}
		}
	}
}

void UCharacterAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCharacterAttributeSet, Health, OldHealth);
}

void UCharacterAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCharacterAttributeSet, MaxHealth, OldMaxHealth);
}

void UCharacterAttributeSet::OnRep_Stamina(const FGameplayAttributeData& OldStamina) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCharacterAttributeSet, Stamina, OldStamina);
}

void UCharacterAttributeSet::OnRep_MaxStamina(const FGameplayAttributeData& OldMaxStamina) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCharacterAttributeSet, MaxStamina, OldMaxStamina);
}

void UCharacterAttributeSet::OnRep_HealthCapMultiplier(const FGameplayAttributeData& OldHealthCapMultiplier) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCharacterAttributeSet, HealthCapMultiplier, OldHealthCapMultiplier);
}

void UCharacterAttributeSet::OnRep_StaminaCapMultiplier(const FGameplayAttributeData& OldStaminaCapMultiplier) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCharacterAttributeSet, StaminaCapMultiplier, OldStaminaCapMultiplier);
}

void UCharacterAttributeSet::OnRep_StaminaRegenRate(const FGameplayAttributeData& OldStaminaRegenRate) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCharacterAttributeSet, StaminaRegenRate, OldStaminaRegenRate);
}

void UCharacterAttributeSet::OnRep_StaminaCostMultiplier(const FGameplayAttributeData& OldStaminaCostMultiplier) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCharacterAttributeSet, StaminaCostMultiplier, OldStaminaCostMultiplier);
}

void UCharacterAttributeSet::OnRep_MovementSpeed(const FGameplayAttributeData& OldMovementSpeed) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCharacterAttributeSet, MovementSpeed, OldMovementSpeed);
}

void UCharacterAttributeSet::OnRep_Armor(const FGameplayAttributeData& OldArmor) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCharacterAttributeSet, Armor, OldArmor);
}

void UCharacterAttributeSet::OnRep_AttackPower(const FGameplayAttributeData& OldAttackPower) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCharacterAttributeSet, AttackPower, OldAttackPower);
}

void UCharacterAttributeSet::OnRep_FireIntervalMultiplier(const FGameplayAttributeData& OldFireIntervalMultiplier) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCharacterAttributeSet, FireIntervalMultiplier, OldFireIntervalMultiplier);
}

void UCharacterAttributeSet::OnRep_ReloadMontageRateMultiplier(
    const FGameplayAttributeData& OldReloadMontageRateMultiplier) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCharacterAttributeSet, ReloadMontageRateMultiplier, OldReloadMontageRateMultiplier);
}

void UCharacterAttributeSet::AdjustAttributeForMaxChange(FGameplayAttributeData& AffectedAttribute,
                                                         const FGameplayAttributeData& MaxAttribute, float NewMaxValue,
                                                         const FGameplayAttribute& AffectedAttributeProperty) const
{
	UAbilitySystemComponent* AbilityComp = GetOwningAbilitySystemComponent();
	const float CurrentMaxValue = MaxAttribute.GetCurrentValue();

	if (!AbilityComp || FMath::IsNearlyEqual(CurrentMaxValue, NewMaxValue))
	{
		return;
	}

	const float CurrentValue = AffectedAttribute.GetCurrentValue();
	const float NewDelta =
	    CurrentMaxValue > 0.0f ? (CurrentValue * NewMaxValue / CurrentMaxValue) - CurrentValue : NewMaxValue;

	AbilityComp->ApplyModToAttributeUnsafe(AffectedAttributeProperty, EGameplayModOp::Additive, NewDelta);
}
