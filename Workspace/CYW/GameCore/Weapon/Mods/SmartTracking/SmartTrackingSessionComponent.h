#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameCore/Weapon/Data/SmartTrackingTypes.h"
#include "SmartTrackingSessionComponent.generated.h"

class AWeaponBase;
class USmartTrackingFireController;
class UWeaponComponent;
class UAbilitySystemComponent;

// Character-owned network session. The weapon's fire controller owns execution.
UCLASS(ClassGroup = (Weapon))
class TEAMPROJECT_FIN_API USmartTrackingSessionComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	USmartTrackingSessionComponent();
	void RequestSmartTrackingFire(float AbilityLevel, const TArray<FSmartTrackingFireRequest>& FireRequests);
	void BeginSmartTrackingLock(AActor* TargetActor);
	void EndSmartTrackingLock(AActor* TargetActor);
	void ClearSmartTrackingLocks();
	void FinishSmartTrackingVolley();
	bool IsVolleyActive() const { return bSmartTrackingVolleyActive || bPredictedSmartTrackingVolleyActive; }
protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	UWeaponComponent* GetWeaponComponent() const;
	AWeaponBase* GetEquippedWeapon() const;
	bool CanRequestSmartTrackingFire(const AWeaponBase* Weapon, const TArray<FSmartTrackingFireRequest>& FireRequests) const;
	UFUNCTION(Server, Reliable)
	void ServerRequestSmartTrackingFire(AWeaponBase* ExpectedWeapon, float AbilityLevel, const TArray<FSmartTrackingFireRequest>& FireRequests);
	UFUNCTION(Server, Reliable)
	void ServerBeginSmartTrackingLock(AWeaponBase* ExpectedWeapon, AActor* TargetActor);
	UFUNCTION(Server, Reliable)
	void ServerEndSmartTrackingLock(AWeaponBase* ExpectedWeapon, AActor* TargetActor);
	UFUNCTION(Server, Reliable)
	void ServerClearSmartTrackingLocks(AWeaponBase* ExpectedWeapon);
	UFUNCTION(Client, Reliable)
	void ClientSetSmartTrackingVolleyActive(bool bActive);
	int32 ExecuteSmartTrackingFire(UAbilitySystemComponent* SourceASC, float AbilityLevel,
		const TArray<FSmartTrackingFireRequest>& FireRequests);
	void SetSmartTrackingVolleyActive(bool bActive);
	TArray<FSmartTrackingFireRequest> BuildServerFireRequests(UAbilitySystemComponent* SourceASC,
		const TArray<FSmartTrackingFireRequest>& ClientRequests) const;
	bool BuildValidatedRequest(const FSmartTrackingFireRequest& ClientRequest, UAbilitySystemComponent* SourceASC,
		const AWeaponBase* Weapon, const FSmartTrackingTargetingSettings& Settings, double Now,
		int32& RemainingAmmo, TSet<AActor*>& AcceptedTargets, FSmartTrackingFireRequest& OutRequest) const;
	UPROPERTY(Replicated)
	bool bSmartTrackingVolleyActive = false;
	bool bPredictedSmartTrackingVolleyActive = false;
	TMap<TWeakObjectPtr<AActor>, double> ServerSmartTrackingLockStartTimes;
	TWeakObjectPtr<USmartTrackingFireController> ActiveFireController;
	FDelegateHandle VolleyFinishedHandle;
};
