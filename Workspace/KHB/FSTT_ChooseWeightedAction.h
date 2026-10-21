#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StateTreeTaskBase.h"
#include "FSTT_ChooseWeightedAction.generated.h"

class AActor;

USTRUCT()
struct FWeightedActionChoice
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Action")
	FGameplayTag Action;

	// 값이 클수록 더 자주 뽑힌다.
	UPROPERTY(EditAnywhere, Category = "Action", meta = (ClampMin = "0.0"))
	float Weight = 1.0f;

	// 0 이하면 하한 없음.
	UPROPERTY(EditAnywhere, Category = "Action")
	float MinDistance = 0.0f;

	// 0 이하면 상한 없음.
	UPROPERTY(EditAnywhere, Category = "Action")
	float MaxDistance = 0.0f;

	/*
	 * 자신의 체력 비율(0~1) 조건. 기본값(0~1)이면 체력과 무관하게 항상 후보다.
	 *
	 * 거리 조건과 같은 층위의 필터다. "체력 70% 이하에서만 돌진 클로" 같은
	 * 패턴은 MaxHealthRatio를 0.7로 두면 된다.
	 *
	 * 경계에서 되돌아갈 수 있는 무른 조건이라는 점에 주의한다. 힐을 받아
	 * 다시 위로 올라가면 그 행동은 후보에서 빠진다. 한 번 넘어가면 고정되는
	 * 보스 페이즈는 이 필터가 아니라 별도 상태로 다뤄야 한다.
	 */
	UPROPERTY(EditAnywhere, Category = "Action", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinHealthRatio = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Action", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxHealthRatio = 1.0f;
};

USTRUCT()
struct FSTT_ChooseWeightedActionInstanceData
{
	GENERATED_BODY()

	// 후보 목록. 각 항목의 거리 조건(MinDistance~MaxDistance)과
	// 체력 조건(MinHealthRatio~MaxHealthRatio)에 맞고, 지금 발동 가능한
	// (쿨다운이 돌지 않은) 것만 뽑힌다.
	UPROPERTY(EditAnywhere, Category = "Action")
	TArray<FWeightedActionChoice> Choices;

	// 거리 계산 기준. Monster Perception.Target Actor를 바인딩한다.
	UPROPERTY(EditAnywhere, Category = "Action")
	TObjectPtr<AActor> TargetActor = nullptr;

	// 출력. 다른 Task나 Transition 조건에서 바인딩해서 읽는다.
	UPROPERTY(EditAnywhere, Category = "Action")
	FGameplayTag SelectedAction;
};

/**
 * 태그+가중치+거리조건+체력조건 목록 중, 지금 조건에 맞는 후보들만 걸러서
 * 그중 하나를 진입 시 한 번 뽑아 SelectedAction에 저장하고 종료한다.
 *
 * 쿨다운·코스트 같은 발동 조건은 여기에 적지 않는다. 후보를 추릴 때 어빌리티
 * 본인에게 CanActivateAbility로 물어보므로, 조건이 바뀌어도 이 데이터는
 * 손댈 필요가 없다.
 *
 * 조건에 맞는 후보가 하나도 없으면 Failed를 반환한다. 쿨다운까지 보게 된
 * 뒤로는 이 경우가 실제로 생기므로, 조건이 없는 후보를 하나 넣어 두거나
 * 실패 전이를 대기 상태로 돌려 두는 편이 안전하다.
 * 특정 몬스터에 종속되지 않는다. 몬스터마다 Choices만 다르게 채우면 된다.
 */
USTRUCT(meta = (DisplayName = "Choose Weighted Action", Category = "Common"))
struct TEAMPROJECT_FIN_API FSTT_ChooseWeightedAction : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSTT_ChooseWeightedActionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
};
