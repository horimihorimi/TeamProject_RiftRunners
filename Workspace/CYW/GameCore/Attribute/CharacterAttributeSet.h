// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "CharacterAttributeSet.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class TEAMPROJECT_FIN_API UCharacterAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UCharacterAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Attributes|Health")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Attributes|Health")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, MaxHealth)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Stamina, Category = "Attributes|Stamina")
	FGameplayAttributeData Stamina;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, Stamina)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxStamina, Category = "Attributes|Stamina")
	FGameplayAttributeData MaxStamina;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, MaxStamina)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_HealthCapMultiplier, Category = "Attributes|Health")
	FGameplayAttributeData HealthCapMultiplier;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, HealthCapMultiplier)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_StaminaCapMultiplier, Category = "Attributes|Stamina")
	FGameplayAttributeData StaminaCapMultiplier;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, StaminaCapMultiplier)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_StaminaRegenRate, Category = "Attributes|Stamina")
	FGameplayAttributeData StaminaRegenRate;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, StaminaRegenRate)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_StaminaCostMultiplier, Category = "Attributes|Stamina")
	FGameplayAttributeData StaminaCostMultiplier;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, StaminaCostMultiplier)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MovementSpeed, Category = "Attributes|Movement")
	FGameplayAttributeData MovementSpeed;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, MovementSpeed)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Armor, Category = "Attributes|Defense")
	FGameplayAttributeData Armor;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, Armor)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_AttackPower, Category = "Attributes|Offense")
	FGameplayAttributeData AttackPower;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, AttackPower)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_FireIntervalMultiplier, Category = "Attributes|Weapon")
	FGameplayAttributeData FireIntervalMultiplier;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, FireIntervalMultiplier)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ReloadMontageRateMultiplier, Category = "Attributes|Weapon")
	FGameplayAttributeData ReloadMontageRateMultiplier;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, ReloadMontageRateMultiplier)

	// 메타 어트리뷰트 (복제되지 않음, 데미지 계산 및 체력 차감용 임시 변수)
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Meta")
	FGameplayAttributeData Damage;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, Damage)

protected:
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldHealth) const;

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth) const;

	UFUNCTION()
	void OnRep_Stamina(const FGameplayAttributeData& OldStamina) const;

	UFUNCTION()
	void OnRep_MaxStamina(const FGameplayAttributeData& OldMaxStamina) const;

	UFUNCTION()
	void OnRep_HealthCapMultiplier(const FGameplayAttributeData& OldHealthCapMultiplier) const;

	UFUNCTION()
	void OnRep_StaminaCapMultiplier(const FGameplayAttributeData& OldStaminaCapMultiplier) const;

	UFUNCTION()
	void OnRep_StaminaRegenRate(const FGameplayAttributeData& OldStaminaRegenRate) const;

	UFUNCTION()
	void OnRep_StaminaCostMultiplier(const FGameplayAttributeData& OldStaminaCostMultiplier) const;

	UFUNCTION()
	void OnRep_MovementSpeed(const FGameplayAttributeData& OldMovementSpeed) const;

	UFUNCTION()
	void OnRep_Armor(const FGameplayAttributeData& OldArmor) const;

	UFUNCTION()
	void OnRep_AttackPower(const FGameplayAttributeData& OldAttackPower) const;

	UFUNCTION()
	void OnRep_FireIntervalMultiplier(const FGameplayAttributeData& OldFireIntervalMultiplier) const;

	UFUNCTION()
	void OnRep_ReloadMontageRateMultiplier(const FGameplayAttributeData& OldReloadMontageRateMultiplier) const;

private:
	void AdjustAttributeForMaxChange(
		FGameplayAttributeData& AffectedAttribute,
		const FGameplayAttributeData& MaxAttribute,
		float NewMaxValue,
		const FGameplayAttribute& AffectedAttributeProperty) const;
};
