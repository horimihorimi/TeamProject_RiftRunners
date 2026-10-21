#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SmartTrackingProjectileFlightComponent.generated.h"

class AWeaponProjectile;
class USplineComponent;
class USmartTrackingPathComponent;

USTRUCT()
struct FSmartTrackingPathState
{
	GENERATED_BODY()
	UPROPERTY() bool bIsActive = false;
	UPROPERTY() FVector Start = FVector::ZeroVector;
	UPROPERTY() FVector Mid = FVector::ZeroVector;
	UPROPERTY() FVector End = FVector::ZeroVector;
	UPROPERTY() float DistanceTravelled = 0.0f;
};

UCLASS(ClassGroup = (Weapon))
class TEAMPROJECT_FIN_API USmartTrackingProjectileFlightComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	USmartTrackingProjectileFlightComponent();
	void StartFlight(USplineComponent* SharedPath);
	void StopFlight();
protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
private:
	UFUNCTION() void OnRep_PathState();
	void ReleasePath();
	void UpdateServerFlight(AWeaponProjectile* Projectile, float DeltaTime);
	void UpdateClientFlight(AWeaponProjectile* Projectile, float DeltaTime);
	UPROPERTY(ReplicatedUsing = OnRep_PathState)
	FSmartTrackingPathState PathState;
	UPROPERTY(Transient)
	TObjectPtr<USplineComponent> Path;
	UPROPERTY(Transient)
	TObjectPtr<USplineComponent> ClientPath;
	TWeakObjectPtr<USmartTrackingPathComponent> PathOwner;
	float ClientDistance = 0.0f;
};
