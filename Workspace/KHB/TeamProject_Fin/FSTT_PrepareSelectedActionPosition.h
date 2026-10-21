#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "StateTreeTaskBase.h"
#include "FSTT_PrepareSelectedActionPosition.generated.h"

class UAbilitySystemComponent;

USTRUCT()
struct FSTT_PrepareSelectedActionPositionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent = nullptr;

	FGameplayAbilitySpecHandle ActiveSpecHandle;
	bool bWaitingForRepositionAbility = false;
};

/**
 * SelectedAction을 바꾸지 않은 채, 그 행동의 요구 거리/시야에 맞는 위치를 준비한다.
 *
 * 프로필이 없거나 현재 위치가 이미 맞으면 즉시 성공한다. 재배치가 필요한 경우에만
 * 프로필에 지정된 GA를 직접 실행한다. 따라서 기존 Activate Ability By Tag Task는
 * 여전히 SelectedAction의 실제 공격 GA만 실행한다.
 */
USTRUCT(meta = (DisplayName = "Prepare Selected Action Position", Category = "Monster"))
struct TEAMPROJECT_FIN_API FSTT_PrepareSelectedActionPosition : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSTT_PrepareSelectedActionPositionInstanceData;

	FSTT_PrepareSelectedActionPosition();

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
