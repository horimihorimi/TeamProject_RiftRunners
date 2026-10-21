// Copyright Epic Games, Inc. All Rights Reserved.

#include "DamageSystemLibrary.h"
#include "CombatTargetLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "../Combat/CombatSurfaceTypes.h"
#include "../Attribute/CharacterAttributeSet.h"
#include "../Tags/GameplayTagSystem.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

void UDamageSystemLibrary::LogDamageApplied(
	const AActor* InstigatorActor,
	const AActor* TargetActor,
	const FName AttackType)
{
	const UAbilitySystemComponent* SourceASC =
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(InstigatorActor);

	const float AttackPower = SourceASC
		? SourceASC->GetNumericAttribute(UCharacterAttributeSet::GetAttackPowerAttribute())
		: 0.0f;

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Damage] Type=%s Instigator=%s Target=%s AttackPower=%.1f"),
		*AttackType.ToString(),
		*GetNameSafe(InstigatorActor),
		*GetNameSafe(TargetActor),
		AttackPower);
}

namespace
{
	bool IsWeakpointHit(const FHitResult* HitResult)
	{
		return HitResult
			&& HitResult->PhysMaterial.IsValid()
			&& UPhysicalMaterial::DetermineSurfaceType(HitResult->PhysMaterial.Get()) == CombatSurfaceTypes::Weakpoint;
	}
}

void UDamageSystemLibrary::ApplyGameDamage(const UObject* WorldContextObject, AActor* InstigatorActor, const FDamagePayload& Payload)
{
	if (!InstigatorActor || !InstigatorActor->HasAuthority())
	{
		// 이 노드는 무조건 서버(Authority)에서만 실행되어야 합니다.
		return;
	}

	if (!Payload.DamageEffectClass)
	{
		UE_LOG(LogTemp, Error, TEXT("ApplyGameDamage: DamageEffectClass is missing! Please assign GE_Damage_Base in the payload."));
		return;
	}

	UAbilitySystemComponent* SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(InstigatorActor);
	if (!SourceASC) return;

	AActor* TargetActor = Payload.HitResult.GetActor();
	if (!TargetActor) return;

	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor);
	if (!TargetASC) return;

	ApplyDamageToTarget(InstigatorActor, SourceASC, TargetActor, TargetASC, Payload, &Payload.HitResult);
}

bool UDamageSystemLibrary::ApplyDamageToTarget(
	AActor* InstigatorActor,
	UAbilitySystemComponent* SourceASC,
	AActor* TargetActor,
	UAbilitySystemComponent* TargetASC,
	const FDamagePayload& Payload,
	const FHitResult* HitResult)
{
	if (!InstigatorActor || !InstigatorActor->HasAuthority()
		|| !SourceASC || !TargetActor || !TargetASC || !Payload.DamageEffectClass)
	{
		return false;
	}

	// GE 생성
	FGameplayEffectContextHandle ContextHandle = SourceASC->MakeEffectContext();
	ContextHandle.AddInstigator(InstigatorActor, Payload.EffectCauser ? Payload.EffectCauser.Get() : InstigatorActor);
	if (HitResult)
	{
		ContextHandle.AddHitResult(*HitResult, true);
	}

	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(
		Payload.DamageEffectClass,
		FMath::Max(Payload.AbilityLevel, 1.0f),
		ContextHandle);
	if (!SpecHandle.IsValid()) return false;

	FGameplayEffectSpec* Spec = SpecHandle.Data.Get();

	// DoT 세팅 (주의: 원본 GE가 HasDuration 또는 Infinite일 때만 안전하게 적용 가능)
	if (Payload.DoTDuration > 0.0f && Payload.DoTPeriod > 0.0f)
	{
		// 만약 실수로 Instant(즉발) GE를 넣고 DoT를 켜면 엔진이 크래시납니다. 이를 방지하는 안전장치.
		if (Spec->Def && Spec->Def->DurationPolicy == EGameplayEffectDurationType::Instant)
		{
			UE_LOG(LogTemp, Error, TEXT("ApplyGameDamage: DoT를 적용하려면 Payload의 DamageEffectClass가 'Has Duration' 정책을 가진 GE여야 합니다! 현재 Instant GE가 들어가 있어 즉발 데미지로 강제 전환됩니다."));
		}
		else
		{
			Spec->SetDuration(Payload.DoTDuration, true);
			Spec->Period = Payload.DoTPeriod;
			Spec->AddDynamicAssetTag(GameplayTagSystem::Damage_Type_DoT);
		}
	}

	// 그로기 유발 판별 (디자이너가 Payload에서 선택한 경우)
	if (Payload.GroggyType == EDamageGroggyType::HeavyGroggy)
	{
		Spec->AddDynamicAssetTag(GameplayTagSystem::Damage_Type_HeavyGroggy);
	}
	else if (Payload.GroggyType == EDamageGroggyType::LightGroggy)
	{
		Spec->AddDynamicAssetTag(GameplayTagSystem::Damage_Type_Groggy);
	}

	// 자폭 및 팀킬 판별 (GAS 태그 기반)
	const bool bIsSelf = (TargetActor == InstigatorActor);
	const bool bIsTeammate = !bIsSelf && CombatTargetLibrary::AreFriendly(SourceASC, TargetASC);

	if (bIsSelf || bIsTeammate)
	{
		Spec->AddDynamicAssetTag(GameplayTagSystem::Hit_FriendlyFire);
	}

	// 약점 판별
	if (IsWeakpointHit(HitResult))
	{
		Spec->AddDynamicAssetTag(GameplayTagSystem::Hit_Weakpoint);
	}

	// SetByCaller 데이터 전송 (방어구 관통력 및 각종 배수)
	Spec->SetSetByCallerMagnitude(GameplayTagSystem::Data_Damage, FMath::Abs(Payload.BaseDamage));
	Spec->SetSetByCallerMagnitude(GameplayTagSystem::Data_ArmorPenetration, Payload.ArmorPenetration);
	Spec->SetSetByCallerMagnitude(GameplayTagSystem::Data_Damage_MaxHealthPercent, Payload.MaxHealthPercent);
	Spec->SetSetByCallerMagnitude(GameplayTagSystem::Data_Multiplier_Weakpoint, Payload.WeakpointMultiplier);
	Spec->SetSetByCallerMagnitude(GameplayTagSystem::Data_Multiplier_FriendlyFire, Payload.FriendlyFireMultiplier);

	// 서버에서 피격자에게 적용
	SourceASC->ApplyGameplayEffectSpecToTarget(*Spec, TargetASC);
	return true;
}
