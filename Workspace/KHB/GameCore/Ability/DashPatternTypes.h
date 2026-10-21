// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DashSplinePath.h"
#include "DashPatternTypes.generated.h"

class UAnimMontage;
class UCurveFloat;

USTRUCT(BlueprintType)
struct FDashPatternChoice
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Selection",
		meta = (ClampMin = "0.0"))
	float Weight = 1.0f;

	/** A value less than or equal to zero means there is no minimum distance. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Selection",
		meta = (ClampMin = "0.0"))
	float MinDistance = 0.0f;

	/** A value less than or equal to zero means there is no maximum distance. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Selection",
		meta = (ClampMin = "0.0"))
	float MaxDistance = 0.0f;

	/** Base dash speed in Unreal units per second. The speed curve is multiplied by this value. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement",
		meta = (ClampMin = "1.0"))
	float DashSpeed = 2000.0f;

	/**
	 * Optional normalized time to speed multiplier curve.
	 * X is dash progress (0-1), Y is the multiplier applied to DashSpeed.
	 * When unset, the dash immediately uses DashSpeed.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta = (DisplayName = "Speed Curve"))
	TObjectPtr<UCurveFloat> MovementCurve = nullptr;

	/** Local-space path used by Spline movement. Ignored when Build Dynamic Path is enabled. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	TSubclassOf<ADashSplinePath> SplinePath;

	/**
	 * 그려둔 경로 대신 타깃까지의 경로를 발동할 때마다 만든다.
	 *
	 * 작가가 그린 스플라인은 길이가 고정이라 어디서 시작하든 같은 거리만
	 * 나간다. 타깃 바로 앞에 멈춰야 하는 패턴은 그 방식으로 표현할 수 없다.
	 *
	 * 경로는 발동 시점에 한 번 만들고 그 뒤로 갱신하지 않는다. 이동 중에
	 * 타깃을 따라가면 회피로 벗어날 방법이 사라진다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Dynamic Path")
	bool bBuildDynamicPath = false;

	/** 타깃에서 이만큼 앞에 멈춘다. 공격 사거리와 맞춘다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Dynamic Path",
		meta = (ClampMin = "0.0", EditCondition = "bBuildDynamicPath", EditConditionHides))
	float StopDistance = 150.0f;

	/**
	 * 도착 지점의 높이 보정. 타깃의 액터 위치가 기준이다.
	 *
	 * 캐릭터의 액터 위치는 발밑이 아니라 캡슐 중심이므로, 0이면 타깃과 같은
	 * 높이에서 멈춘다. 근접 공격은 대개 이 값이 맞다. 양수를 주면 그만큼
	 * 위에서 멈추므로 머리 위를 치게 된다.
	 *
	 * 공중 몹은 평소 타깃보다 높이 떠 있으므로(FlyingMonsterMovementComponent의
	 * BaseHoverHeight)이 값으로 내려오는 셈이 된다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Dynamic Path",
		meta = (EditCondition = "bBuildDynamicPath", EditConditionHides))
	float ArrivalHeightOffset = 0.0f;

	/** 이동 거리 대비 아치 높이 비율. 0이면 직선. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Dynamic Path",
		meta = (ClampMin = "0.0", EditCondition = "bBuildDynamicPath", EditConditionHides))
	float ArcHeightRatio = 0.25f;

	/** 아치 높이 상한. 실내에서 천장을 뚫지 않도록 잡는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Dynamic Path",
		meta = (ClampMin = "0.0", EditCondition = "bBuildDynamicPath", EditConditionHides))
	float MaxArcHeight = 300.0f;

	/**
	 * 도착 탄젠트의 길이 배율.
	 *
	 * 값이 작으면 위에서 내리꽂는 각으로 들어오고, 크면 눕혀서 들어온다.
	 * 공격 모션이 정면을 가정한다면 눕히는 편이 맞는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Dynamic Path",
		meta = (ClampMin = "0.1", EditCondition = "bBuildDynamicPath", EditConditionHides))
	float ArrivalTangentScale = 0.6f;

	/** When enabled, keep facing the current target while following the spline. Otherwise follow the spline tangent. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	bool bFaceTargetWhileMoving = false;

	/** Creates a transient copy of SplinePath and deforms it once for each dash execution. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Variation")
	bool bUseSplineVariation = false;

	/** Maximum local Y deformation applied to the spline. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Variation",
		meta = (ClampMin = "0.0", EditCondition = "bUseSplineVariation", EditConditionHides))
	float HorizontalVariation = 150.0f;

	/** Maximum local Z deformation applied to the spline. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Variation",
		meta = (ClampMin = "0.0", EditCondition = "bUseSplineVariation", EditConditionHides))
	float VerticalVariation = 75.0f;

	/** Noise frequency sampled across the normalized path. Higher values create more bends. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Variation",
		meta = (ClampMin = "0.25", EditCondition = "bUseSplineVariation", EditConditionHides))
	float VariationFrequency = 1.0f;

	/** Number of points sampled from the authored spline for the transient deformed copy. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Variation",
		meta = (ClampMin = "4", ClampMax = "64", EditCondition = "bUseSplineVariation", EditConditionHides))
	int32 VariationSampleCount = 16;

	/** Keeps the authored end position fixed. The start position is always fixed to prevent snapping. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Variation",
		meta = (EditCondition = "bUseSplineVariation", EditConditionHides))
	bool bPreserveSplineEnd = true;

	/** Spline mode can use a dash montage. Disable this to keep the locomotion Blend Space active. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	bool bUseDashMontage = true;

	/** Used only when Use Dash Montage is enabled. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation",
		meta = (EditCondition = "bUseDashMontage", EditConditionHides))
	TObjectPtr<UAnimMontage> DashMontage = nullptr;

	/** Section entered after movement finishes when Use Dash Montage is enabled. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation",
		meta = (EditCondition = "bUseDashMontage", EditConditionHides))
	FName MontageEndSection = TEXT("End");
};
