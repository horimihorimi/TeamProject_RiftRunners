#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_CenterPivotRotation.generated.h"

class ACharacter;

/** Turn 구간 동안 캐릭터 중심을 고정하고 Actor 위치를 보정한다. */
UCLASS()
class TEAMPROJECT_FIN_API UAbilityTask_CenterPivotRotation : public UAbilityTask
{
	GENERATED_BODY()

public:
	UAbilityTask_CenterPivotRotation();

	static UAbilityTask_CenterPivotRotation* StartCenterPivotRotation(
		UGameplayAbility* OwningAbility,
		ACharacter* TargetCharacter);

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;
	virtual void OnDestroy(bool AbilityEnded) override;

private:
	UPROPERTY()
	TObjectPtr<ACharacter> TargetCharacter;

	FVector PivotLocation = FVector::ZeroVector;
	FVector LocalActorOffset = FVector::ZeroVector;
};
