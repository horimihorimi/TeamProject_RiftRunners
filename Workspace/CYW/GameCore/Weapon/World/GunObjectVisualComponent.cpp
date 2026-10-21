#include "GunObjectVisualComponent.h"

#include "Components/StaticMeshComponent.h"

UGunObjectVisualComponent::UGunObjectVisualComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UGunObjectVisualComponent::Initialize(UStaticMeshComponent* InPhysicsMesh, UStaticMeshComponent* InFrameMesh)
{
	PhysicsMeshComponent = InPhysicsMesh;
	FrameMeshComponent = InFrameMesh;
}

void UGunObjectVisualComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!PhysicsMeshComponent || !FrameMeshComponent)
	{
		return;
	}

	HoverElapsedTime += DeltaTime;
	const float HoverOffset = HoverAmplitude * FMath::Sin(HoverElapsedTime * HoverFrequency * 2.0f * PI);
	FrameMeshComponent->SetWorldLocation(
		PhysicsMeshComponent->GetComponentLocation() + FVector::UpVector * (GroundOffset + HoverOffset));

	FRotator DisplayRotation = PickupMeshRotation;
	DisplayRotation.Pitch += TiltAngle;
	FrameMeshComponent->SetWorldRotation(DisplayRotation);
	FrameMeshComponent->SetWorldScale3D(MeshScale);
}
