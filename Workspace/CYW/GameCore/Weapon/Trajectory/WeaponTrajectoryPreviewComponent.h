#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/SplineMeshComponent.h"
#include "WeaponTrajectoryPreviewComponent.generated.h"

class AWeaponBase;
class UAbilitySystemComponent;
class UMaterialInterface;
class UDashedSplineRenderer;
class UStaticMesh;
class UWeaponComponent;

UCLASS(ClassGroup = (Weapon), meta = (BlueprintSpawnableComponent))
class TEAMPROJECT_FIN_API UWeaponTrajectoryPreviewComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWeaponTrajectoryPreviewComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void RefreshCachedComponents();
	bool ShouldDrawTrajectory() const;
	AWeaponBase* GetPreviewWeapon() const;
	void DrawTrajectory(AWeaponBase* Weapon, float DeltaTime);

	UPROPERTY(Transient)
	TObjectPtr<UWeaponComponent> WeaponComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(Transient)
	TObjectPtr<UDashedSplineRenderer> Renderer;

	UPROPERTY(EditAnywhere, Category = "Trajectory")
	float ProjectileRadius = 12.0f;

	UPROPERTY(EditAnywhere, Category = "Trajectory")
	float MaxSimTime = 2.0f;

	UPROPERTY(EditAnywhere, Category = "Trajectory")
	float SimFrequency = 20.0f;

	UPROPERTY(EditAnywhere, Category = "Trajectory")
	float DrawDuration = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Trajectory|Dash")
	TObjectPtr<UStaticMesh> DashMesh;

	UPROPERTY(EditAnywhere, Category = "Trajectory|Dash")
	TObjectPtr<UMaterialInterface> DashMaterial;

	UPROPERTY(EditAnywhere, Category = "Trajectory|Dash")
	float DashLength = 18.0f;

	UPROPERTY(EditAnywhere, Category = "Trajectory|Dash")
	float DashGap = 22.0f;

	UPROPERTY(EditAnywhere, Category = "Trajectory|Dash")
	float DashMoveSpeed = 180.0f;

	UPROPERTY(EditAnywhere, Category = "Trajectory|Dash")
	FVector2D DashScale = FVector2D(0.08f, 0.08f);

	UPROPERTY(EditAnywhere, Category = "Trajectory|Dash")
	TEnumAsByte<ESplineMeshAxis::Type> DashForwardAxis = ESplineMeshAxis::Z;
};
