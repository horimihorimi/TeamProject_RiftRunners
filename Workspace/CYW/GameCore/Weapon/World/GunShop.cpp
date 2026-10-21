#include "GunShop.h"

#include "GameCore/Interact/InteractTriggerComponent.h"
#include "GameCore/BasePlayerState.h"
#include "GameCore/Weapon/World/GunObject.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerController.h"

AGunShop::AGunShop()
{
	PrimaryActorTick.bCanEverTick = false;
	ShopRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ShopRoot"));
	SetRootComponent(ShopRoot);
	ShopMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShopMesh"));
	ShopMesh->SetupAttachment(ShopRoot);
	ShopMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	InteractTrigger = CreateDefaultSubobject<UInteractTriggerComponent>(TEXT("InteractTrigger"));
	InteractTrigger->SetupAttachment(ShopRoot);
	GunSpawnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("GunSpawnPoint"));
	GunSpawnPoint->SetupAttachment(ShopMesh);
	InteractText = NSLOCTEXT("GunShop", "BuyRandomGun", " E : 총기 생성");
}

void AGunShop::Execute(APlayerController* PC)
{
	if (!PC || !PC->HasAuthority())
	{
		return;
	}

	if (!GunObjectClass)
	{
		return;
	}

	ABasePlayerState* PlayerState = PC->GetPlayerState<ABasePlayerState>();
	const int32 Cost = WeaponCost;
	if (!PlayerState || !PlayerState->TrySpendGold(Cost))
	{
		return;
	}

	const FTransform SpawnTransform = GunSpawnPoint
		? GunSpawnPoint->GetComponentTransform()
		: GetActorTransform();
	AGunObject* SpawnedGun = AGunObject::SpawnRandomShopWeapon(
		this,
		PC,
		SpawnTransform,
		GunObjectClass);
	if (SpawnedGun)
	{
		const FVector EjectionVelocity = GunSpawnPoint->GetForwardVector() * EjectionSpeed
			+ FVector::UpVector * EjectionUpwardSpeed;
		SpawnedGun->LaunchFromShop(EjectionVelocity);
	}
	else
	{
		PlayerState->AddGold(Cost);
	}
}

FText AGunShop::GetInteractText() const
{
	return FText::Format(NSLOCTEXT("GunShop", "PurchaseWithGold", "{0} ({1} 골드)"),
		InteractText, FText::AsNumber(WeaponCost));
}
