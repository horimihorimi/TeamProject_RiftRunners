// Copyright Epic Games, Inc. All Rights Reserved.

#include "ExecCalc_Damage.h"
#include "AbilitySystemComponent.h"
#include "../Attribute/CharacterAttributeSet.h"
#include "../Tags/GameplayTagSystem.h"

// 캡처를 위한 매크로 세팅
struct FDamageStatics
{
	DECLARE_ATTRIBUTE_CAPTUREDEF(Armor);
	DECLARE_ATTRIBUTE_CAPTUREDEF(MaxHealth);
	DECLARE_ATTRIBUTE_CAPTUREDEF(AttackPower);
	DECLARE_ATTRIBUTE_CAPTUREDEF(Damage);

	FDamageStatics()
	{
		// 피격자의 방어력 캡처
		DEFINE_ATTRIBUTE_CAPTUREDEF(UCharacterAttributeSet, Armor, Target, false);
		// 피격자의 최대 체력 캡처(비례 피해용)
		DEFINE_ATTRIBUTE_CAPTUREDEF(UCharacterAttributeSet, MaxHealth, Target, false);
		// 공격자의 공격력 캡처
		DEFINE_ATTRIBUTE_CAPTUREDEF(UCharacterAttributeSet, AttackPower, Source, false);
		// 피격자의 데미지 메타 어트리뷰트 캡처
		DEFINE_ATTRIBUTE_CAPTUREDEF(UCharacterAttributeSet, Damage, Target, false);
	}
};

static const FDamageStatics& DamageStatics()
{
	static FDamageStatics Statics;
	return Statics;
}

UExecCalc_Damage::UExecCalc_Damage()
{
	RelevantAttributesToCapture.Add(DamageStatics().ArmorDef);
	RelevantAttributesToCapture.Add(DamageStatics().MaxHealthDef);
	RelevantAttributesToCapture.Add(DamageStatics().AttackPowerDef);
	RelevantAttributesToCapture.Add(DamageStatics().DamageDef);
}

void UExecCalc_Damage::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	UAbilitySystemComponent* TargetASC = ExecutionParams.GetTargetAbilitySystemComponent();
	UAbilitySystemComponent* SourceASC = ExecutionParams.GetSourceAbilitySystemComponent();

	if (!TargetASC || !SourceASC)
	{
		return;
	}

	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
	const FGameplayTagContainer* TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();
	
	// DoT 태그 확인 (도트 딜은 무적 상태를 무시하고 들어감)
	const bool bIsDoT = Spec.GetDynamicAssetTags().HasTag(GameplayTagSystem::Damage_Type_DoT);

	// 1. 무적(Invincible) 판별 로직
	if (TargetTags && TargetTags->HasTag(GameplayTagSystem::State_Invincible) && !bIsDoT)
	{
		// 도트가 아닌 즉발 데미지는 무적일 때 완전히 무효화
		return;
	}

	FAggregatorEvaluateParameters EvalParams;
	EvalParams.TargetTags = TargetTags;
	EvalParams.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();

	// 2. 공격자의 공격력(AttackPower) 캡처
	float AttackPower = 1.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().AttackPowerDef, EvalParams, AttackPower);
	AttackPower = FMath::Max(AttackPower, 0.0f);

	// 3. 피격자의 방어력(Armor) 캡처
	float Armor = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().ArmorDef, EvalParams, Armor);
	Armor = FMath::Max(Armor, 0.0f);

	// 4. Base Damage 가져오기 (BP 노드에서 SetByCaller로 넘긴 값)
	float BaseDamage = Spec.GetSetByCallerMagnitude(GameplayTagSystem::Data_Damage, false, 0.0f);

	// 5. 증폭 배수 캡처 (BP 노드에서 SetByCaller로 넘긴 값, 기본값 세팅)
	float WeakpointMultiplier = Spec.GetSetByCallerMagnitude(GameplayTagSystem::Data_Multiplier_Weakpoint, false, 1.5f);
	float FriendlyFireMultiplier = Spec.GetSetByCallerMagnitude(GameplayTagSystem::Data_Multiplier_FriendlyFire, false, 0.5f);

	// 공격력 스탯 반영
	float FinalDamage = BaseDamage * AttackPower;

	// 태그 기반 배수 적용
	if (Spec.GetDynamicAssetTags().HasTag(GameplayTagSystem::Hit_Weakpoint))
	{
		FinalDamage *= WeakpointMultiplier;
	}
	
	if (Spec.GetDynamicAssetTags().HasTag(GameplayTagSystem::Hit_FriendlyFire))
	{
		FinalDamage *= FriendlyFireMultiplier;
	}

	// 6. 방어력(Armor) 공식 적용 (스탠다드 경감 공식: 100 / (100 + 방어력))
	// 무기 고유의 방어구 관통력(ArmorPenetration)을 가져와 상대 방어력을 깎음
	float ArmorPenetration = Spec.GetSetByCallerMagnitude(GameplayTagSystem::Data_ArmorPenetration, false, 0.0f);
	float EffectiveArmor = FMath::Max(Armor - ArmorPenetration, 0.0f); // 방어력은 0 이하로 떨어지지 않음

	float DamageMitigation = 100.0f / (100.0f + EffectiveArmor);
	FinalDamage *= DamageMitigation;

	/*
	 * 최대 체력 비례 피해는 경감을 적용한 뒤에 더한다.
	 *
	 * 이 항의 목적이 "빌드와 무관하게 정해진 비율만큼 깎는 것"이라, 방어력을
	 * 태우면 방어 특화 캐릭터에게만 약한 공격이 되어 의도가 사라진다.
	 */
	const float MaxHealthPercent = Spec.GetSetByCallerMagnitude(
		GameplayTagSystem::Data_Damage_MaxHealthPercent, false, 0.0f);

	if (MaxHealthPercent > 0.0f)
	{
		float TargetMaxHealth = 0.0f;
		ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
			DamageStatics().MaxHealthDef, EvalParams, TargetMaxHealth);

		const float PercentDamage = FMath::Max(TargetMaxHealth, 0.0f) * MaxHealthPercent;
		FinalDamage += PercentDamage;

		UE_LOG(LogTemp, Verbose,
			TEXT("[DamageCalc] 최대체력 비례 %.1f (최대체력 %.0f x %.2f) 추가"),
			PercentDamage,
			TargetMaxHealth,
			MaxHealthPercent);
	}

	/*
	 * 밸런싱용 상세 로그. 평소에는 조용하고 필요할 때만 켠다.
	 *   켜기: Log LogTemp Verbose     끄기: Log LogTemp Log
	 *
	 * 지속 타격은 ReHitInterval마다 반복되므로 Log 등급으로 두면 도배된다.
	 */
	UE_LOG(LogTemp, Verbose,
		TEXT("[DamageCalc] %.1f = 계수 %.2f x 공격력 %.1f x 경감 %.2f (방어 %.0f - 관통 %.0f)%s%s"),
		FinalDamage,
		BaseDamage,
		AttackPower,
		DamageMitigation,
		Armor,
		ArmorPenetration,
		Spec.GetDynamicAssetTags().HasTag(GameplayTagSystem::Hit_Weakpoint)
			? TEXT(" [약점]") : TEXT(""),
		Spec.GetDynamicAssetTags().HasTag(GameplayTagSystem::Hit_FriendlyFire)
			? TEXT(" [자해]") : TEXT(""));

	// 7. 최종 산출 데미지 적용
	if (FinalDamage > 0.0f)
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(DamageStatics().DamageProperty, EGameplayModOp::Additive, FinalDamage));
	}
}
