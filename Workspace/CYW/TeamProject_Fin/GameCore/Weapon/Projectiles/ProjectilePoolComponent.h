#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ProjectilePoolComponent.generated.h"

class AWeaponProjectile;
class ARocketActor;

// 유탄 발사기처럼 투사체를 쓰는 무기의 투사체 액터를 재사용한다.
UCLASS(ClassGroup = (Weapon), meta = (BlueprintSpawnableComponent))
class TEAMPROJECT_FIN_API UProjectilePoolComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UProjectilePoolComponent();

	AWeaponProjectile* AcquireProjectile(TSubclassOf<AWeaponProjectile> ProjectileClass, const FTransform& SpawnTransform);
	void ReleaseProjectile(AWeaponProjectile* Projectile);
	ARocketActor* AcquireRocket(TSubclassOf<ARocketActor> RocketClass);
	void ReleaseRocket(ARocketActor* Rocket);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile Pool")
	int32 InitialPoolSize = 8;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile Pool")
	bool bAllowPoolExpansion = true;

private:
	void WarmPool(TSubclassOf<AWeaponProjectile> ProjectileClass);
	AWeaponProjectile* SpawnPooledProjectile(TSubclassOf<AWeaponProjectile> ProjectileClass);

	TMap<UClass*, TArray<AWeaponProjectile*>> AvailableProjectiles;

	TSet<AWeaponProjectile*> ActiveProjectiles;

	TSet<UClass*> WarmedClasses;

	UPROPERTY()
	TArray<TObjectPtr<ARocketActor>> Rockets;
};
