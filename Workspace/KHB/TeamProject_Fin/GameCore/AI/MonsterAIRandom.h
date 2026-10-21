#pragma once

#include "CoreMinimal.h"

/**
 * 몬스터 행동 Task가 진입 시 한 번만 호출해 사용할 공용 랜덤 유틸리티.
 * 결과는 각 Task의 InstanceData에 저장해서 행동이 끝날 때까지 유지한다.
 */
namespace MonsterAIRandom
{
	/** Min~Max 사이의 연속된 실수 하나를 반환한다. 입력 순서는 상관없다. */
	inline float RandomRange(const float First, const float Second)
	{
		return FMath::FRandRange(FMath::Min(First, Second), FMath::Max(First, Second));
	}

	/** -Range~+Range 사이의 연속된 실수 하나를 반환한다. */
	inline float RandomSymmetricRange(const float Range)
	{
		const float AbsoluteRange = FMath::Abs(Range);
		return FMath::FRandRange(-AbsoluteRange, AbsoluteRange);
	}

	/** -1 또는 +1을 반환한다. 좌/우, 시계/반시계 선택에 사용한다. */
	inline float RandomSign()
	{
		return FMath::RandBool() ? 1.0f : -1.0f;
	}

	/** 후보 배열에서 하나를 반환한다. 후보가 비어 있으면 Fallback을 반환한다. */
	template <typename T>
	T RandomChoice(const TArray<T>& Choices, const T& Fallback)
	{
		return Choices.IsEmpty()
			? Fallback
			: Choices[FMath::RandRange(0, Choices.Num() - 1)];
	}
}
