#pragma once

#include "CoreMinimal.h"
#include "GameCore/Animation/BaseCharacterAnimInstance.h"
#include "MonsterAnimInstance.generated.h"

/**
 * 몬스터 AnimBP가 공통으로 사용하는 이동 데이터를 제공한다.
 * Pawn 접근은 게임 스레드에서 수행하고, 계산은 Thread Safe Update에서 처리한다.
 */
UCLASS()
class TEAMPROJECT_FIN_API UMonsterAnimInstance : public UBaseCharacterAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Monster|Config")
	float MovingSpeedThreshold = 10.0f;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Monster|Locomotion")
	float Speed = 0.0f;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Monster|Locomotion")
	float GroundSpeed = 0.0f;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Monster|Locomotion")
	float LocalVelocityX = 0.0f;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Monster|Locomotion")
	float LocalVelocityY = 0.0f;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Monster|Locomotion")
	float LocalVelocityZ = 0.0f;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Monster|Locomotion")
	bool bIsMoving = false;

private:
	UPROPERTY(Transient)
	TObjectPtr<APawn> OwningPawn = nullptr;

	FVector WorldVelocity = FVector::ZeroVector;
	FTransform MeshTransform = FTransform::Identity;
};
