#include "GunCrate.h"

#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameCore/Ability/GameplayCues/GameplayCueFxLibrary.h"
#include "GameCore/BasePlayerState.h"
#include "GameCore/Combat/MonsterDropComponent.h"
#include "GameCore/Interact/InteractTriggerComponent.h"
#include "GameCore/Weapon/World/GunObject.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

AGunCrate::AGunCrate()
{
	PrimaryActorTick.bCanEverTick = false;
	SetReplicates(true);
	RarityWeights.Add(EGunRarity::Common, 60.0f);
	RarityWeights.Add(EGunRarity::Uncommon, 30.0f);
	RarityWeights.Add(EGunRarity::Epic, 9.0f);
	RarityWeights.Add(EGunRarity::Legendary, 1.0f);

	CrateRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CrateRoot"));
	SetRootComponent(CrateRoot);
	CrateMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CrateMesh"));
	CrateMesh->SetupAttachment(CrateRoot);
	CrateMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CapMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CapMesh"));
	CapMesh->SetupAttachment(CrateMesh);
	CapMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	InteractTrigger = CreateDefaultSubobject<UInteractTriggerComponent>(TEXT("InteractTrigger"));
	InteractTrigger->SetupAttachment(CrateRoot);
	GunSpawnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("GunSpawnPoint"));
	GunSpawnPoint->SetupAttachment(CrateMesh);
	InteractText = NSLOCTEXT("GunCrate", "GetRandomGun", " E : 보상 획득");
}

void AGunCrate::Execute(APlayerController* PC)
{
	if (!HasAuthority() || !PC || bIsOpened || !GunObjectClass)
	{
		return;
	}

	AGameStateBase* GameState = GetWorld()->GetGameState();
	if (!GameState)
	{
		return;
	}

	bIsOpened = true;
	OnRep_Opened();
	ForceNetUpdate();

	const FTransform SpawnTransform = GunSpawnPoint->GetComponentTransform();
	const FVector EjectionVelocity = GunSpawnPoint->GetForwardVector() * EjectionSpeed
		+ FVector::UpVector * EjectionUpwardSpeed;
	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		ABasePlayerState* BasePlayerState = Cast<ABasePlayerState>(PlayerState);
		APlayerController* EligiblePlayer = BasePlayerState
			? Cast<APlayerController>(BasePlayerState->GetOwner()) : nullptr;
		if (!EligiblePlayer || !UMonsterDropComponent::IsPlayerAlive(BasePlayerState))
		{
			continue;
		}

		if (AGunObject* Gun = AGunObject::SpawnGeneratedWeapon(
			this, EligiblePlayer, SpawnTransform, GunObjectClass,
			EWeaponGenerationSource::EnemyDrop, &RarityWeights))
		{
			Gun->LaunchFromShop(EjectionVelocity);
		}
	}
}

void AGunCrate::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGunCrate, bIsOpened);
}

void AGunCrate::OnRep_Opened()
{
	InteractTrigger->DisableInteraction();
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (OpeningMontage)
	{
		// The cap only needs this montage; single-node playback supplies its slot without an Anim Blueprint.
		CapMesh->PlayAnimation(OpeningMontage, false);
	}
	GameplayCueFxLibrary::SpawnSoundAtLocation(
		GetWorld(), OpeningSound, GetActorLocation(), GetActorRotation(), SoundVolume, 1.0f, SoundAttenuation);
}

FText AGunCrate::GetInteractText() const
{
	return bIsOpened ? FText::GetEmpty() : InteractText;
}
