#include "GunObject.h"

#include "Components/StaticMeshComponent.h"
#include "GameCore/Interact/InteractTriggerComponent.h"
#include "GameCore/Weapon/Core/WeaponBase.h"
#include "GameCore/Weapon/Core/WeaponComponent.h"
#include "GameCore/Weapon/Generation/WeaponGenerationSubsystem.h"
#include "GameCore/Weapon/World/GunObjectVisualComponent.h"
#include "GameCore/Weapon/World/GunPickupUIComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

AGunObject::AGunObject()
{
	PrimaryActorTick.bCanEverTick = false;
	SetReplicates(true);
	SetReplicateMovement(true);
	bOnlyRelevantToOwner = true;
	EnemyDropRarityWeights.Add(EGunRarity::Common, 60.0f);
	EnemyDropRarityWeights.Add(EGunRarity::Uncommon, 30.0f);
	EnemyDropRarityWeights.Add(EGunRarity::Epic, 9.0f);
	EnemyDropRarityWeights.Add(EGunRarity::Legendary, 1.0f);

	PhysicsMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PhysicsMesh"));
	SetRootComponent(PhysicsMeshComponent);
	PhysicsMeshComponent->SetVisibility(false, true);
	PhysicsMeshComponent->SetHiddenInGame(true, true);
	PhysicsMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	PhysicsMeshComponent->SetCollisionObjectType(ECC_PhysicsBody);
	PhysicsMeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	PhysicsMeshComponent->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	PhysicsMeshComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	PhysicsMeshComponent->SetSimulatePhysics(false);

	FrameMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FrameMesh"));
	FrameMeshComponent->SetupAttachment(PhysicsMeshComponent);
	FrameMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	GunRoot = CreateDefaultSubobject<USceneComponent>(TEXT("GunRoot"));
	GunRoot->SetupAttachment(PhysicsMeshComponent);

	BarrelMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BarrelMesh"));
	BarrelMeshComponent->SetupAttachment(FrameMeshComponent);
	BarrelMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	InteractTrigger = CreateDefaultSubobject<UInteractTriggerComponent>(TEXT("InteractTrigger"));
	InteractTrigger->SetupAttachment(GunRoot);

	PickupVisualComponent = CreateDefaultSubobject<UGunObjectVisualComponent>(TEXT("GunVisualComponent"));
	PickupVisualComponent->Initialize(PhysicsMeshComponent, FrameMeshComponent);
	GunPickupUIComponent = CreateDefaultSubobject<UGunPickupUIComponent>(TEXT("GunPickupUI"));
}

void AGunObject::BeginPlay()
{
	Super::BeginPlay();
	ApplyGeneratedState();
}

void AGunObject::OnRep_Owner()
{
	Super::OnRep_Owner();
	RefreshLocalVisibility();
	RefreshRarityEffect();
}

void AGunObject::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(AGunObject, GeneratedData, COND_None, REPNOTIFY_Always);
}

void AGunObject::OnRep_GeneratedData()
{
	ApplyGeneratedState();
}

void AGunObject::ApplyGeneratedState()
{
	if (!GeneratedData.bIsGeneratedWeapon || !FrameMeshComponent || !BarrelMeshComponent || !PhysicsMeshComponent)
	{
		return;
	}

	FrameMeshComponent->SetStaticMesh(GeneratedData.FrameMesh);
	PhysicsMeshComponent->SetStaticMesh(GeneratedData.FrameMesh);
	BarrelMeshComponent->SetStaticMesh(GeneratedData.BarrelMesh);
	BarrelMeshComponent->AttachToComponent(
		FrameMeshComponent,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		GeneratedData.BarrelSocketName);
	BarrelMeshComponent->SetRelativeTransform(GeneratedData.BarrelRelativeTransform);
	GunPickupUIComponent->RefreshData();
	RefreshLocalVisibility();
	RefreshRarityEffect();
}

void AGunObject::RefreshLocalVisibility()
{
	if (!FrameMeshComponent || !BarrelMeshComponent || !GetWorld())
	{
		return;
	}

	const APlayerController* OwningPlayer = Cast<APlayerController>(GetOwner());
	const bool bVisibleToLocalPlayer = OwningPlayer && OwningPlayer->IsLocalController();
	FrameMeshComponent->SetVisibility(bVisibleToLocalPlayer, true);
	BarrelMeshComponent->SetVisibility(bVisibleToLocalPlayer, true);
}

void AGunObject::RefreshRarityEffect()
{
	if (ActiveRarityEffect)
	{
		ActiveRarityEffect->DestroyComponent();
		ActiveRarityEffect = nullptr;
	}

	const TObjectPtr<UNiagaraSystem>* Effect = RarityEffects.Find(GeneratedData.Rarity);
	if (!FrameMeshComponent || !FrameMeshComponent->IsVisible() || !Effect || !*Effect)
	{
		return;
	}

	ActiveRarityEffect = UNiagaraFunctionLibrary::SpawnSystemAttached(
		*Effect,
		FrameMeshComponent,
		NAME_None,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		EAttachLocation::SnapToTarget,
		true);
	if (ActiveRarityEffect)
	{
		ActiveRarityEffect->SetAbsolute(false, true, false);
	}
}

AGunObject* AGunObject::SpawnRandomShopWeapon(
	UObject* WorldContextObject,
	APlayerController* OwningPlayer,
	const FTransform& Transform,
	TSubclassOf<AGunObject> GunObjectClass)
{
	return SpawnGeneratedWeapon(
		WorldContextObject,
		OwningPlayer,
		Transform,
		GunObjectClass,
		EWeaponGenerationSource::Shop);
}

AGunObject* AGunObject::SpawnRandomEnemyDrop(
	UObject* WorldContextObject,
	APlayerController* EligiblePlayer,
	const FTransform& Transform,
	TSubclassOf<AGunObject> GunObjectClass)
{
	return SpawnGeneratedWeapon(
		WorldContextObject,
		EligiblePlayer,
		Transform,
		GunObjectClass,
		EWeaponGenerationSource::EnemyDrop);
}

AGunObject* AGunObject::SpawnGeneratedWeapon(
	UObject* WorldContextObject,
	APlayerController* OwningPlayer,
	const FTransform& Transform,
	TSubclassOf<AGunObject> GunObjectClass,
	EWeaponGenerationSource Source,
	const TMap<EGunRarity, float>* RarityWeights)
{
	if (!WorldContextObject || !OwningPlayer || !OwningPlayer->HasAuthority())
	{
		return nullptr;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	if (!GunObjectClass)
	{
		GunObjectClass = AGunObject::StaticClass();
	}

	const AGunObject* Defaults = GunObjectClass->GetDefaultObject<AGunObject>();
	UWeaponGenerationSubsystem* Generator = World->GetGameInstance()
		? World->GetGameInstance()->GetSubsystem<UWeaponGenerationSubsystem>()
		: nullptr;
	if (!Defaults || !Generator || !Defaults->WeaponClass)
	{
		return nullptr;
	}

	FGeneratedWeaponData GeneratedResult;
	if (!Generator->GenerateWeapon(
		OwningPlayer,
		Source,
		Defaults->FrameTable,
		Defaults->BarrelTable,
		Defaults->ModifierTable,
		Defaults->WeaponTemplate,
		GeneratedResult,
		RarityWeights ? *RarityWeights : Defaults->EnemyDropRarityWeights))
	{
		return nullptr;
	}

	AGunObject* GunObject = World->SpawnActorDeferred<AGunObject>(
		GunObjectClass,
		Transform,
		OwningPlayer,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!GunObject)
	{
		return nullptr;
	}

	GunObject->GeneratedData = MoveTemp(GeneratedResult);
	UGameplayStatics::FinishSpawningActor(GunObject, Transform);
	if (!IsValid(GunObject) || GunObject->IsActorBeingDestroyed())
	{
		return nullptr;
	}

	if (Source == EWeaponGenerationSource::EnemyDrop)
	{
		// Enemy drops have no shop launch to start their server-side physics.
		GunObject->PhysicsMeshComponent->SetEnableGravity(true);
		GunObject->PhysicsMeshComponent->SetSimulatePhysics(true);
		GunObject->PhysicsMeshComponent->WakeAllRigidBodies();
		const FVector OutwardDirection = FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f).Vector();
		GunObject->PhysicsMeshComponent->SetPhysicsLinearVelocity(
			OutwardDirection * FMath::Max(GunObject->DropOutwardSpeed, 0.0f)
			+ FVector::UpVector * FMath::Max(GunObject->DropUpwardSpeed, 0.0f));
	}

	Generator->CommitGeneration(OwningPlayer, GunObject->GeneratedData);
	return GunObject;
}

void AGunObject::LaunchFromShop(const FVector& EjectionVelocity)
{
	if (!HasAuthority() || !PhysicsMeshComponent)
	{
		return;
	}

	PhysicsMeshComponent->SetSimulatePhysics(true);
	PhysicsMeshComponent->WakeAllRigidBodies();
	PhysicsMeshComponent->SetPhysicsLinearVelocity(EjectionVelocity);
}

FVector AGunObject::GetPickupDisplayLocation() const
{
	return FrameMeshComponent->GetComponentLocation();
}

bool AGunObject::CanInteract(const APlayerController* PC) const
{
	return GeneratedData.bIsGeneratedWeapon && PC && PC == GetOwner() && PC->GetPawn()
		&& WeaponClass && WeaponTemplate && !IsActorBeingDestroyed()
		&& PC->GetPawn()->FindComponentByClass<UWeaponComponent>();
}

void AGunObject::Execute(APlayerController* PC)
{
	if (!HasAuthority() || !GeneratedData.bIsGeneratedWeapon || !PC || PC != GetOwner() || !PC->GetPawn()
		|| !WeaponClass || !WeaponTemplate)
	{
		return;
	}

	UWeaponComponent* WeaponComponent = PC->GetPawn()->FindComponentByClass<UWeaponComponent>();
	if (!WeaponComponent)
	{
		return;
	}

	// Equipping destroys the previous weapon, so capture its live state first.
	AWeaponBase* PreviousWeapon = WeaponComponent->GetEquippedWeapon();
	FGeneratedWeaponData PreviousData;
	TSubclassOf<AWeaponBase> PreviousClass;
	const UWeaponDataAsset* PreviousTemplate = nullptr;
	int32 PreviousCurrentAmmo = 0;
	FTransform DropStartTransform;
	if (PreviousWeapon)
	{
		DropStartTransform = PreviousWeapon->GetWeaponMesh()->GetComponentTransform();
		PreviousData = PreviousWeapon->GetDropData();
		PreviousClass = PreviousWeapon->GetClass();
		PreviousTemplate = PreviousWeapon->GetWeaponData();
		PreviousCurrentAmmo = PreviousWeapon->GetCurrentAmmo();
	}

	if (!WeaponComponent->SpawnAndEquipGeneratedWeapon(
		WeaponClass, WeaponTemplate, GeneratedData, CurrentAmmo))
	{
		return;
	}

	if (!PreviousWeapon)
	{
		Destroy();
		return;
	}

	const FVector DropTarget = GetActorLocation();
	PhysicsMeshComponent->SetSimulatePhysics(false);
	GeneratedData = MoveTemp(PreviousData);
	WeaponClass = PreviousClass;
	WeaponTemplate = PreviousTemplate;
	CurrentAmmo = PreviousCurrentAmmo;
	ApplyGeneratedState();
	SetActorLocation(DropStartTransform.GetLocation(), false, nullptr, ETeleportType::TeleportPhysics);
	const FVector Displacement = DropTarget - GetActorLocation();
	const float Gravity = GetWorld()->GetGravityZ();
	const float DropDuration = FMath::Clamp(
		FMath::Max(Displacement.Size2D() / FMath::Max(SwapOutwardSpeed, 1.0f),
			2.0f * SwapUpwardSpeed / FMath::Max(FMath::Abs(Gravity), 1.0f)),
		0.2f, 0.8f);
	PhysicsMeshComponent->SetEnableGravity(true);
	LaunchFromShop(Displacement / DropDuration - FVector::UpVector * (0.5f * Gravity * DropDuration));
	ForceNetUpdate();
}

FText AGunObject::GetInteractText() const
{
	return NSLOCTEXT("GunObject", "Pickup", " E : 총기 장착");
}
