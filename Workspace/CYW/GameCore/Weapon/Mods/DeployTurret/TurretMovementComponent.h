#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TurretMovementComponent.generated.h"

UCLASS(ClassGroup = (Weapon), meta = (BlueprintSpawnableComponent))
class TEAMPROJECT_FIN_API UTurretMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTurretMovementComponent();
	void Initialize();
	FVector GetFollowLocation() const;
	void FollowPlayer(float DeltaTime);
	void StartIntercept(const AActor* Target);
	bool MoveToIntercept(float DeltaTime);
	void ChooseNextPosition(const AActor* Target);
	bool FollowTarget(const AActor* Target, float DeltaTime, bool bRepositioning);
	void RotateToTarget(const AActor* Target, float DeltaTime);
	void UpdateVisualOffset(float DeltaTime);
	void InterpolateReplicatedMovement(float DeltaTime);
	void AddKickback();

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditDefaultsOnly, Category = "Turret|Follow")
	FVector FollowOffset = FVector(0.0f, 90.0f, 60.0f);
	UPROPERTY(EditDefaultsOnly, Category = "Turret|Follow", meta = (ClampMin = "0.1"))
	float MoveInterpSpeed = 6.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Turret|Follow", meta = (ClampMin = "0.1"))
	float RotateInterpSpeed = 10.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Turret|Follow", meta = (ClampMin = "0.0"))
	float MoveInterpRandModifier = 1.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Turret|Follow", meta = (ClampMin = "0.0"))
	float RotateInterpRandModifier = 1.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Turret|Follow", meta = (ClampMin = "0.0"))
	float FollowOffsetRandomRange = 50.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Turret|Hover", meta = (ClampMin = "0.0"))
	float HoverAmplitude = 10.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Turret|Hover", meta = (ClampMin = "0.0"))
	float HoverFrequency = 2.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Turret|Intercept", meta = (ClampMin = "1.0"))
	float InterceptMoveSpeed = 900.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Turret|Intercept")
	float InterceptMinHeightOffset = -100.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Turret|Intercept")
	float InterceptMaxHeightOffset = 250.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Turret|Intercept", meta = (ClampMin = "0.0"))
	float InterceptCurveStrength = 300.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Turret|Intercept", meta = (ClampMin = "1.0"))
	float InterceptRadius = 250.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Turret|Intercept", meta = (ClampMin = "0.1"))
	float InterceptPositionMoveSpeed = 5.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Turret|Intercept", meta = (ClampMin = "1.0"))
	float InterceptPositionArriveDistance = 40.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Turret|Recoil", meta = (ClampMin = "0.0"))
	float KickbackStrength = 10.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Turret|Recoil", meta = (ClampMin = "0.1"))
	float KickbackRecoverSpeed = 10.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Turret|Network", meta = (ClampMin = "0.1"))
	float NetworkInterpSpeed = 20.0f;

private:
	UPROPERTY(Replicated)
	FTransform FlightTransform = FTransform::Identity;
	FVector BaseLocation = FVector::ZeroVector;
	FVector RandomOffset = FVector::ZeroVector;
	FVector InterceptStartLocation = FVector::ZeroVector;
	FVector InterceptDestination = FVector::ZeroVector;
	FVector InterceptControlOffset = FVector::ZeroVector;
	FVector InterceptTargetOffset = FVector::ZeroVector;
	float RandomizedMoveInterpSpeed = 6.0f;
	float RandomizedRotateInterpSpeed = 10.0f;
	float InterceptAlpha = 0.0f;
	float HoverTime = 0.0f;
	float CurrentKickback = 0.0f;
};
