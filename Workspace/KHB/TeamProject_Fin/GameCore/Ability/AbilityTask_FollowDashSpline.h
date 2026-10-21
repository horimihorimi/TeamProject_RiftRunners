#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "DashPatternTypes.h"
#include "AbilityTask_FollowDashSpline.generated.h"

class ADashSplinePath;
class ACharacter;
class UCharacterMovementComponent;
class UPrimitiveComponent;
class USplineComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDashSplineFollowFinished);

/** Ability 실행 중 매 프레임 스플라인을 따라 Avatar Actor를 이동시키는 Task. */
UCLASS()
class TEAMPROJECT_FIN_API UAbilityTask_FollowDashSpline : public UAbilityTask
{
	GENERATED_BODY()

public:
	/** Executes a spline dash from the supplied pattern definition. */
	static UAbilityTask_FollowDashSpline* PerformDash(
		UGameplayAbility* OwningAbility,
		const FDashPatternChoice& Pattern,
		AActor* TargetActor,
		float InRotationInterpSpeed,
		bool bInDebugDraw);

	/** Legacy spline-only entry point retained for the old GA_DashFollowSpline asset. */
	static UAbilityTask_FollowDashSpline* FollowDashSpline(
		UGameplayAbility* OwningAbility,
		TSubclassOf<ADashSplinePath> ChosenPath,
		float InMoveSpeed,
		float InRotationInterpSpeed,
		bool bInRotateAlongSpline,
		bool bInDebugDraw);

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;
	virtual void OnDestroy(bool AbilityEnding) override;

	UPROPERTY(BlueprintAssignable)
	FOnDashSplineFollowFinished OnFinished;

private:
	bool BuildRuntimeSplineVariation(const USplineComponent* SourceSpline);

	/** 시작 지점에서 타깃 앞까지 이어지는 3점 경로를 만든다. */
	bool BuildRuntimeSplineToTarget();
	float EvaluateSpeedMultiplier(float TimeAlpha) const;
	float CalculateAverageSpeedMultiplier() const;
	void FinishDash();

	FDashPatternChoice DashPattern;
	TSubclassOf<ADashSplinePath> DashPathClass;
	TWeakObjectPtr<AActor> AvatarActor;
	TWeakObjectPtr<AActor> TargetActor;
	TWeakObjectPtr<ACharacter> Character;
	TWeakObjectPtr<UCharacterMovementComponent> MovementComponent;
	TWeakObjectPtr<UPrimitiveComponent> DashCollisionComponent;
	TWeakObjectPtr<USplineComponent> SelectedSpline;

	/** Transient per-execution copy used only when spline variation is enabled. */
	UPROPERTY()
	TObjectPtr<USplineComponent> RuntimeSpline = nullptr;

	FTransform DashStartTransform = FTransform::Identity;
	FVector DashStartLocation = FVector::ZeroVector;
	float TotalDistance = 0.0f;
	float DashDuration = 0.0f;
	float ElapsedTime = 0.0f;
	float AccumulatedDistance = 0.0f;
	float InitialSpeedMultiplier = 0.0f;
	float CurrentDashSpeed = 0.0f;
	bool bRotateAlongMovement = false;
	bool bFaceTargetWhileMoving = false;
	bool bPawnCollisionOverridden = false;
	TEnumAsByte<ECollisionResponse> PreviousPawnCollisionResponse = ECR_Block;
	bool bDebugDraw = false;
	float RotationInterpSpeed = 12.0f;
};
