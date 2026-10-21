#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GunObjectVisualComponent.generated.h"

class UStaticMeshComponent;

/** Keeps the visible pickup mesh readable while its separate physics body moves. */
UCLASS(ClassGroup=(Weapon), meta=(BlueprintSpawnableComponent))
class TEAMPROJECT_FIN_API UGunObjectVisualComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGunObjectVisualComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void Initialize(UStaticMeshComponent* InPhysicsMesh, UStaticMeshComponent* InFrameMesh);

	/** World-space base pose for the rendered gun, independent of the physics body's rotation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gun Object|Pickup Presentation")
	FRotator PickupMeshRotation = FRotator(-90.0f, 0.0f, 0.0f);

	/** Lean angle from the upward-facing pose. Positive values lean the muzzle toward the actor's forward direction. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gun Object|Pickup Presentation", meta=(ClampMin="-89.0", ClampMax="89.0", UIMin="-45.0", UIMax="45.0", Units="deg"))
	float TiltAngle = 30.0f;

	/** Extra vertical distance from the physics body, keeping the visual mesh clear of the floor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gun Object|Pickup Presentation", meta=(ClampMin="0.0", UIMin="0.0", Units="cm"))
	float GroundOffset = 30.0f;

	/** Uniform/non-uniform scale for the displayed frame and its attached barrel mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gun Object|Pickup Presentation")
	FVector MeshScale = FVector(1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gun Object|Pickup Presentation", meta=(ClampMin="0.0", UIMin="0.0", Units="cm"))
	float HoverAmplitude = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gun Object|Pickup Presentation", meta=(ClampMin="0.0", UIMin="0.0", Units="Hz"))
	float HoverFrequency = 1.0f;

private:
	TObjectPtr<UStaticMeshComponent> PhysicsMeshComponent;
	TObjectPtr<UStaticMeshComponent> FrameMeshComponent;
	float HoverElapsedTime = 0.0f;
};
