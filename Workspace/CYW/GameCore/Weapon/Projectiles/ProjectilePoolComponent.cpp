#include "ProjectilePoolComponent.h"

#include "WeaponProjectile.h"
#include "../Mods/RocketBarrage/RocketActor.h"
#include "GameFramework/Pawn.h"

UProjectilePoolComponent::UProjectilePoolComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

ARocketActor* UProjectilePoolComponent::AcquireRocket(TSubclassOf<ARocketActor> RocketClass)
{
	if (!GetOwner()->HasAuthority() || !RocketClass)
	{
		return nullptr;
	}
	Rockets.RemoveAll([](const auto& Rocket) { return !IsValid(Rocket); });
	int32 ClassCount = 0;
	for (ARocketActor* Rocket : Rockets)
	{
		if (Rocket->GetClass() != RocketClass.Get())
		{
			continue;
		}
		++ClassCount;
		if (!Rocket->IsActiveRocket())
		{
			Rocket->ActivateFromPool(this);
			return Rocket;
		}
	}
	if (!bAllowPoolExpansion && ClassCount >= InitialPoolSize)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.Owner = GetOwner();
	Params.Instigator = Cast<APawn>(GetOwner());
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARocketActor* Rocket =
		GetWorld()->SpawnActor<ARocketActor>(RocketClass, GetOwner()->GetActorLocation(), GetOwner()->GetActorRotation(), Params);
	if (Rocket)
	{
		Rockets.Add(Rocket);
		Rocket->ActivateFromPool(this);
	}
	return Rocket;
}

void UProjectilePoolComponent::ReleaseRocket(ARocketActor* Rocket)
{
	if (IsValid(Rocket) && Rockets.Contains(Rocket))
	{
		Rocket->DeactivateToPool();
	}
}

void UProjectilePoolComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (ARocketActor* Rocket : Rockets)
	{
		if (IsValid(Rocket))
		{
			Rocket->Destroy();
		}
	}
	Rockets.Reset();
	Super::EndPlay(EndPlayReason);
}

AWeaponProjectile* UProjectilePoolComponent::AcquireProjectile(
	TSubclassOf<AWeaponProjectile> ProjectileClass, const FTransform& SpawnTransform)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || !ProjectileClass)
	{
		return nullptr;
	}

	UClass* ProjectileKey = ProjectileClass.Get();
	WarmPool(ProjectileClass);

	TArray<AWeaponProjectile*>& AvailableList = AvailableProjectiles.FindOrAdd(ProjectileKey);
	AWeaponProjectile* Projectile = nullptr;

	while (AvailableList.Num() > 0 && !Projectile)
	{
		Projectile = AvailableList.Pop(EAllowShrinking::No);
	}

	if (!Projectile && bAllowPoolExpansion)
	{
		Projectile = SpawnPooledProjectile(ProjectileClass);
	}

	if (!Projectile)
	{
		return nullptr;
	}

	ActiveProjectiles.Add(Projectile);
	Projectile->ActivateFromPool(this, SpawnTransform);
	return Projectile;
}

void UProjectilePoolComponent::ReleaseProjectile(AWeaponProjectile* Projectile)
{
	if (!Projectile)
	{
		return;
	}

	if (ActiveProjectiles.Remove(Projectile) == 0)
	{
		return;
	}

	AvailableProjectiles.FindOrAdd(Projectile->GetClass()).Add(Projectile);
}

void UProjectilePoolComponent::WarmPool(TSubclassOf<AWeaponProjectile> ProjectileClass)
{
	UClass* ProjectileKey = ProjectileClass.Get();
	if (!ProjectileKey || WarmedClasses.Contains(ProjectileKey))
	{
		return;
	}

	WarmedClasses.Add(ProjectileKey);

	const int32 PoolSize = FMath::Max(InitialPoolSize, 0);
	for (int32 Index = 0; Index < PoolSize; ++Index)
	{
		if (AWeaponProjectile* Projectile = SpawnPooledProjectile(ProjectileClass))
		{
			AvailableProjectiles.FindOrAdd(ProjectileKey).Add(Projectile);
		}
	}
}

AWeaponProjectile* UProjectilePoolComponent::SpawnPooledProjectile(TSubclassOf<AWeaponProjectile> ProjectileClass)
{
	AActor* OwnerActor = GetOwner();
	UWorld* World = OwnerActor ? OwnerActor->GetWorld() : nullptr;
	if (!World || !ProjectileClass)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = OwnerActor;
	SpawnParams.Instigator = OwnerActor->GetInstigator();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AWeaponProjectile* Projectile =
		World->SpawnActor<AWeaponProjectile>(ProjectileClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);

	if (Projectile)
	{
		Projectile->SetOwningPool(this);
		Projectile->DeactivateToPool();
	}

	return Projectile;
}
