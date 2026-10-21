#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnvironmentCharacterMovementComponent.generated.h"

UENUM(BlueprintType)
enum ECustomMovementMode
{
	CMOVE_None      UMETA(Hidden),
	CMOVE_RopeSwinging  UMETA(DisplayName = "Rope Swinging"),
	CMOVE_Max       UMETA(Hidden)
};

/**
 * 
 */
UCLASS()
class TEAMPROJECT_FIN_API UEnvironmentCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UEnvironmentCharacterMovementComponent(const FObjectInitializer& ObjectInitializer);

	virtual void UpdateFromCompressedFlags(uint8 Flags) override final;
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override final;

	virtual void PhysCustom(float deltaTime, int32 Iterations) override final;
	virtual void PerformMovement(float DeltaTime) override;

	// Rope Interaction
	UFUNCTION(BlueprintCallable, Category = "Rope")
	void BeginRopeSwing(AActor* InAnchor, float InRopeLength);

	UFUNCTION(BlueprintCallable, Category = "Rope")
	void DismountRope();

	// Wind Hazard Integration (WindZone 블루프린트에서 Tick을 쓰지 않기 위한 함수)
	// BeginOverlap에서 호출
	UFUNCTION(BlueprintCallable, Category = "Wind")
	void AddActiveWindForce(const FVector& InWindVelocity, float InTerminalVelocity);

	// EndOverlap에서 호출
	UFUNCTION(BlueprintCallable, Category = "Wind")
	void RemoveActiveWindForce(const FVector& InWindVelocity, float InTerminalVelocity);

	// AnimBP Frame Evaluation (-1.0 ~ 1.0)
	UFUNCTION(BlueprintPure, Category = "Rope")
	float GetSwingPhase() const;

	UPROPERTY(Transient)
	AActor* CurrentRopeAnchor;

	UPROPERTY(Transient)
	float RopeLength;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope")
	float SwingPumpStrength = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope")
	float SwingDamping = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope")
	float MaxSwingSpeed = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope")
	float DismountForwardBoost = 500.0f;

	// 로프 스윙 방향이 바뀔 때 재생할 사운드
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Audio")
	USoundBase* RopeSwingSound;

protected:
	void PhysRopeSwinging(float deltaTime, int32 Iterations);

	UPROPERTY(Transient)
	float PreviousSwingPhase = 0.0f;

	UPROPERTY(Transient)
	FVector ActiveWindForce;

	UPROPERTY(Transient)
	float CurrentTerminalVelocity;

	// Network Prediction (Multiplayer)
	class FSavedMove_Environment : public FSavedMove_Character
	{
	public:
		typedef FSavedMove_Character Super;

		virtual void Clear() override;
		virtual uint8 GetCompressedFlags() const override;
		virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;
		virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character& ClientData) override;
		virtual void PrepMoveFor(class ACharacter* C) override;

		// Custom saved states
		bool bSavedIsRopeSwinging;
	};

	class FNetworkPredictionData_Client_Environment : public FNetworkPredictionData_Client_Character
	{
	public:
		FNetworkPredictionData_Client_Environment(const UCharacterMovementComponent& ClientMovement);
		typedef FNetworkPredictionData_Client_Character Super;

		virtual FSavedMovePtr AllocateNewMove() override;
	};
};
