#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SmartTrackingPathComponent.generated.h"

class AWeaponBase;
class USplineComponent;

USTRUCT()
struct FSmartTrackingPathEntry
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<AActor> Instigator;
	UPROPERTY(Transient)
	TObjectPtr<AActor> Target;
	int32 ReferenceCount = 0;
	float SideDirection = 1.0f;
};

UCLASS(ClassGroup = (Weapon))
class TEAMPROJECT_FIN_API USmartTrackingPathComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	USmartTrackingPathComponent();
	USplineComponent* CreatePath(AActor* InstigatorActor, AActor* TargetActor,
		const FVector& StartLocation, const FVector& EndLocation);
	void RebuildPath(USplineComponent* Path, const FVector& StartLocation, const FVector& EndLocation) const;
	void AddPathReference(USplineComponent* SharedPath);
	void ReleasePathReference(USplineComponent* SharedPath);
protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	AWeaponBase* GetWeapon() const;
	void UpdateActivePaths();
	UPROPERTY(Transient)
	TMap<TObjectPtr<USplineComponent>, FSmartTrackingPathEntry> Paths;
};
