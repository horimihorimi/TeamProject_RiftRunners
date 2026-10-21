	// Copyright Epic Games, Inc. All Rights Reserved.

	#pragma once

	#include "CoreMinimal.h"
	#include "Kismet/BlueprintFunctionLibrary.h"
	#include "GameplayTagContainer.h"
	#include "Engine/HitResult.h"
	#include "DamageSystemLibrary.generated.h"

	class UGameplayEffect;
	class UAbilitySystemComponent;
	class AActor;

	UENUM(BlueprintType)
	enum class EDamageGroggyType : uint8
	{
		None			UMETA(DisplayName = "없음 (일반 데미지)"),
		LightGroggy		UMETA(DisplayName = "약한 그로기 (제자리 피격)"),
		HeavyGroggy		UMETA(DisplayName = "강한 그로기 (넉백 피격)")
	};

	USTRUCT(BlueprintType)
	struct FDamagePayload
	{
		GENERATED_BODY()

	public:
		FDamagePayload()
			: BaseDamage(0.0f)
			, ArmorPenetration(0.0f)
			, MaxHealthPercent(0.0f)
			, WeakpointMultiplier(1.5f)
			, FriendlyFireMultiplier(0.25f)
			, DoTDuration(0.0f)
			, DoTPeriod(0.0f)
			, GroggyType(EDamageGroggyType::None)
			, DamageEffectClass(nullptr)
			, AbilityLevel(1.0f)
		{}

		// 기본 데미지
		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
		float BaseDamage;

		// 방어구 관통력 (적의 방어력을 깎고 데미지 계산)
		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
		float ArmorPenetration;

		// 피격자 최대 체력 비례 추가 피해(0~1). 방어 경감을 타지 않는다.
		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
		float MaxHealthPercent;

		// 약점 명중 시 배수
		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
		float WeakpointMultiplier;

		// 아군(자폭 포함) 피격 시 배수
		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
		float FriendlyFireMultiplier;

		// 단일 타격 대상 정보 (AoE가 아닐 때 필수)
		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
		FHitResult HitResult;


		// 도트 딜(DoT) 총 지속 시간 (0이면 즉발 데미지)
		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage|DoT")
		float DoTDuration;

		// 도트 딜 타격 간격 (예: 1.0이면 1초마다 타격)
		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage|DoT")
		float DoTPeriod;

		// 그로기 유발 타입 (일반 / 약한 그로기 / 강한 넉백 그로기)
		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage|Effect")
		EDamageGroggyType GroggyType;

		// 데미지 적용에 사용할 GE 클래스. 지정하지 않으면 데미지가 적용되지 않음.
		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage|Advanced")
		TSubclassOf<UGameplayEffect> DamageEffectClass;

		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage|Advanced", meta = (ClampMin = "1.0"))
		float AbilityLevel;

		UPROPERTY()
		TObjectPtr<AActor> EffectCauser = nullptr;
	};

	/**
	 * 모든 공격 및 기믹에서 공통으로 호출하는 통합 데미지 파이프라인
	 */
	UCLASS()
	class TEAMPROJECT_FIN_API UDamageSystemLibrary : public UBlueprintFunctionLibrary
	{
		GENERATED_BODY()

	public:
		// 공격자가 피격자(들)에게 데미지를 가합니다. (반드시 서버에서만 호출)
		UFUNCTION(BlueprintCallable, Category = "DamageSystem", meta = (WorldContext = "WorldContextObject"))
		static void ApplyGameDamage(const UObject* WorldContextObject, AActor* InstigatorActor, const FDamagePayload& Payload);

		static bool ApplyDamageToTarget(
			AActor* InstigatorActor,
			UAbilitySystemComponent* SourceASC,
			AActor* TargetActor,
			UAbilitySystemComponent* TargetASC,
			const FDamagePayload& Payload,
			const FHitResult* HitResult = nullptr);

		// Shared combat log used after a valid target has been confirmed.
		static void LogDamageApplied(const AActor* InstigatorActor, const AActor* TargetActor, FName AttackType);
	};
