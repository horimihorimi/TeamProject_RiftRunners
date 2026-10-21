#pragma once

#include "Abilities/GameplayAbilityTypes.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StateTreeTaskBase.h"
#include "FSTT_ActivateAbilityByTag.generated.h"

class UAbilitySystemComponent;

USTRUCT()
struct FSTT_ActivateAbilityByTagInstanceData
{
	GENERATED_BODY()

	// 런타임
	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent = nullptr;

	FGameplayAbilitySpecHandle ActiveSpecHandle;
	bool bLoggedFirstTick = false;

	// 이 상태에서 어택 토큰을 실제로 받아 쥐었는지. ExitState에서 반납 판단에 쓴다.
	bool bHoldingAttackToken = false;

	/*
	 * 켜면 어택 토큰을 얻어야만 어빌리티를 발동한다.
	 *
	 * 같은 플레이어를 동시에 때리는 몬스터 수를 디렉터가 제한하므로, 적이
	 * 많아져도 한 명에게 쏟아지는 공격량이 선형으로 늘지 않는다. 공격 계열
	 * 상태에서만 켜고 이동·회피·등장에는 끈 채로 둔다.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bRequiresAttackToken = false;
};

/**
 * AMonsterAIController::GetSelectedAction()이 반환하는 태그로 GameplayAbility를
 * 발동시키고, 그 어빌리티가 끝날 때까지 대기한다.
 *
 * StateTree 바인딩(Evaluator 미러링)을 거치지 않고 컨트롤러 값을 직접 읽는다.
 * BehaviorDecision(Choose Weighted Action)과 이 Task가 서로 다른 State에 있어서
 * 바인딩으로 연결하면, Evaluator가 프레임당 한 번만 값을 복사하는 타이밍과
 * 어긋나 같은 프레임 안에서는 갱신 전 값을 읽는 경우가 있었다.
 * 완료 판정은 어빌리티가 스스로 EndAbility를 호출했는지를 매 틱 폴링해서 확인한다.
 */
USTRUCT(meta = (DisplayName = "Activate Ability By Tag", Category = "Common"))
struct TEAMPROJECT_FIN_API FSTT_ActivateAbilityByTag : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSTT_ActivateAbilityByTagInstanceData;

	FSTT_ActivateAbilityByTag();

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;

	virtual EStateTreeRunStatus Tick(
		FStateTreeExecutionContext& Context,
		float DeltaTime) const override;

	virtual void ExitState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
};
