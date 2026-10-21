#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameCore/Weapon/Core/WeaponBase.h"
#include "SmartTrackingFireController.generated.h"

class USplineComponent;
class USmartTrackingPathComponent;

struct FSmartTrackingControllerQueuedShot
{
	TWeakObjectPtr<AActor> TargetActor;
	TWeakObjectPtr<USplineComponent> SharedPath;
};

UCLASS(ClassGroup = (Weapon))
class TEAMPROJECT_FIN_API USmartTrackingFireController : public UActorComponent
{
	GENERATED_BODY()

public:
	USmartTrackingFireController();

	int32 ExecuteFire(
		AActor* InstigatorActor,
		UAbilitySystemComponent* SourceASC,
		TSubclassOf<UGameplayEffect> DamageEffectClass,
		float AbilityLevel,
		const TArray<FSmartTrackingFireRequest>& FireRequests);

	bool IsBusy() const { return bVolleyInProgress; }
	FSimpleMulticastDelegate OnVolleyFinished;
	void CancelPendingFire();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	AWeaponBase* GetWeapon() const;
	void ProcessNextShot();
	USmartTrackingPathComponent* GetPaths() const;
	void ClearPendingShots();

	UPROPERTY(Transient)
	FWeaponFireContext PendingFireContext;
	TArray<FSmartTrackingControllerQueuedShot> PendingShots;

	FTimerHandle FireTimerHandle;
	float SmartTrackingVolleyInterval = 0.1f;
	int32 FiredShotSequence = 0;
	bool bVolleyInProgress = false;
};
