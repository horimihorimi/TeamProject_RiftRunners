#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "GameCore/Weapon/Data/WeaponFeedbackTypes.h"
#include "TurretPresentationComponent.generated.h"

class ATurretActor;

UCLASS(ClassGroup = (Weapon), meta = (BlueprintSpawnableComponent))
class TEAMPROJECT_FIN_API UTurretPresentationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTurretPresentationComponent();
	void Start();
	void Stop();
	bool IsReady() const { return bReady; }
	void PlayDetectSound();
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastFire(FGameplayCueParameters Parameters);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turret|Sound")
	FWeaponSoundSettings SpawnSound;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turret|Sound")
	FWeaponSoundSettings DetectSound;
	// As with RocketActor, the selected sound asset must be configured to loop.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turret|Sound")
	FWeaponSoundSettings LoopSound;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turret|Sound")
	FWeaponSoundSettings DeactivateSound;

	UPROPERTY(EditDefaultsOnly, Category = "Turret|Spawn", meta = (ClampMin = "0.0", Units = "s"))
	float MeshRevealDelay = 0.15f;

private:
	ATurretActor* GetTurret() const;
	void RevealMesh();
	void PlaySoundAtLocation(const FWeaponSoundSettings& Settings) const;
	void UpdateLoopSound();

	FTimerHandle MeshRevealTimer;
	bool bReady = false;
	bool bStopped = false;
};
