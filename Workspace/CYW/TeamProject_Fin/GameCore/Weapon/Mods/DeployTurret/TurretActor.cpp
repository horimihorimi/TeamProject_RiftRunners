#include "TurretActor.h"

#include "TurretCombatComponent.h"
#include "TurretMovementComponent.h"
#include "TurretPresentationComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"

ATurretActor::ATurretActor()
{
	bReplicates = true;
	SetReplicateMovement(false);
	SetNetUpdateFrequency(30.0f);
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(SceneRoot);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Muzzle = CreateDefaultSubobject<USceneComponent>(TEXT("Muzzle"));
	Muzzle->SetupAttachment(SceneRoot);
	Muzzle->SetRelativeLocation(FVector(30.0f, 0.0f, 10.0f));
	Direction = CreateDefaultSubobject<UArrowComponent>(TEXT("Direction"));
	Direction->SetupAttachment(SceneRoot);
	Movement = CreateDefaultSubobject<UTurretMovementComponent>(TEXT("Movement"));
	SpawnEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("SpawnEffect"));
	SpawnEffect->SetupAttachment(Mesh);
	SpawnEffect->SetAutoActivate(false);
	Trail = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Trail"));
	Trail->SetupAttachment(Mesh);
	Trail->SetAutoActivate(false);
	LoopAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("LoopAudio"));
	LoopAudio->SetupAttachment(SceneRoot);
	LoopAudio->SetAutoActivate(false);
	Combat = CreateDefaultSubobject<UTurretCombatComponent>(TEXT("TurretCombat"));
	Presentation = CreateDefaultSubobject<UTurretPresentationComponent>(TEXT("Presentation"));
}

bool ATurretActor::IsConfigured(const AWeaponBase* Weapon) const
{
	return Mesh->GetStaticMesh() && Combat->IsConfigured(Weapon);
}

void ATurretActor::Initialize(AWeaponBase* InWeapon, UAbilitySystemComponent* InASC,
	TSubclassOf<UGameplayEffect> InDamageEffect, float InAbilityLevel, float InDamage, int32 InAmmo)
{
	Combat->Initialize(InWeapon, InASC, InDamageEffect, InAbilityLevel, InDamage, InAmmo);
}

void ATurretActor::BeginPlay()
{
	Super::BeginPlay();
	if (bDeactivated)
	{
		OnRep_Deactivated();
		return;
	}
	if (!Combat->Start())
	{
		Destroy();
		return;
	}
	Presentation->Start();
}

void ATurretActor::Deactivate()
{
	if (!HasAuthority() || bDeactivated)
	{
		return;
	}
	bDeactivated = true;
	const FVector Impulse = -FVector::UpVector * DropImpulse
		+ GetActorForwardVector() * FMath::FRandRange(-500.0f, 500.0f)
		+ GetActorRightVector() * FMath::FRandRange(-150.0f, 150.0f);
	SetReplicateMovement(true);
	OnRep_Deactivated();
	Mesh->AddImpulse(Impulse, NAME_None, true);
	SetLifeSpan(DestroyDelay);
	ForceNetUpdate();
}

void ATurretActor::OnRep_Deactivated()
{
	Combat->Stop();
	Presentation->Stop();
	// Actor movement replication follows the simulated root after flight ends.
	if (GetRootComponent() != Mesh)
	{
		Mesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		SetRootComponent(Mesh);
		SceneRoot->AttachToComponent(Mesh, FAttachmentTransformRules::KeepWorldTransform);
	}
	Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetSimulatePhysics(true);
	if (!HasAuthority() && GetReplicatedMovement().bRepPhysics)
	{
		// Replicated physics may have arrived before the deactivation notification.
		OnRep_ReplicatedMovement();
	}
}

void ATurretActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATurretActor, bDeactivated);
}
