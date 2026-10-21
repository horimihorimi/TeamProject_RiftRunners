#include "RocketActor.h"

#include "AbilitySystemComponent.h"
#include "Components/AudioComponent.h"
#include "GameCore/Ability/GameplayCues/GameplayCueFxLibrary.h"
#include "Components/ArrowComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameCore/Ability/CombatTargetLibrary.h"
#include "GameCore/Character/BaseCharacter.h"
#include "GameCore/Tags/GameplayTagSystem.h"
#include "GameCore/Weapon/Core/WeaponComponent.h"
#include "GameCore/Weapon/Core/WeaponBase.h"
#include "GameCore/Weapon/Projectiles/ProjectilePoolComponent.h"
#include "GameCore/Weapon/Libraries/WeaponDamageLibrary.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"

ARocketActor::ARocketActor()
{
	LaunchCue = GameplayTagSystem::GameplayCue_Weapon_RocketBarrage_Launch;
	ExplosionCue = GameplayTagSystem::GameplayCue_Weapon_RocketBarrage_Explosion;
	bReplicates = true;
	SetReplicateMovement(true);
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	SetActorHiddenInGame(true);
	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	SetRootComponent(Collision);
	Collision->InitSphereRadius(12.0f);
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Collision->SetCollisionObjectType(ECC_TraceAttack);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Collision->OnComponentHit.AddDynamic(this, &ARocketActor::OnHit);
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Direction = CreateDefaultSubobject<UArrowComponent>(TEXT("Direction"));
	Direction->SetupAttachment(Collision);
	Trail = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Trail"));
	Trail->SetupAttachment(Mesh);
	Trail->SetAutoActivate(false);
	Beam = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Beam"));
	Beam->SetupAttachment(Mesh);
	Beam->SetAutoActivate(false);
	SpawnEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("SpawnEffect"));
	SpawnEffect->SetupAttachment(Collision);
	SpawnEffect->SetAutoActivate(false);
	FlightAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("FlightAudio"));
	FlightAudio->SetupAttachment(Collision);
	FlightAudio->SetAutoActivate(false);
	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->SetAutoActivate(false);
	Movement->ProjectileGravityScale = 0.0f;
	Movement->bRotationFollowsVelocity = true;
}

void ARocketActor::Prepare(UAbilitySystemComponent* InASC, int32 InStackIndex)
{
	SourceASC = InASC;
	StackIndex = InStackIndex;
	HorizontalOffset = FVector(FMath::FRandRange(-HorizontalOffsetRange, HorizontalOffsetRange),
		FMath::FRandRange(-HorizontalOffsetRange, HorizontalOffsetRange), 0.0f);
	HoverPhase = FMath::FRandRange(0.0f, 2.0f * PI);
	SetActorLocation(GetFormationLocation());
	OnRep_State();
}

FVector ARocketActor::GetFormationLocation() const
{
	const FVector Offset =
		FollowOffset + HorizontalOffset + FVector(0.0f, 0.0f, StackSpacing * StackIndex + HoverAmplitude * FMath::Sin(HoverPhase));
	return GetOwner()->GetActorLocation() + GetOwner()->GetActorQuat().RotateVector(Offset);
}

void ARocketActor::ActivateFromPool(UProjectilePoolComponent* InPool)
{
	OwningPool = InPool;
	State = ERocketState::Spawning;
	SetActorRotation(GetOwner()->GetActorRotation());
	ForceNetUpdate();
}

void ARocketActor::DeactivateToPool()
{
	State = ERocketState::Inactive;
	SetLifeSpan(0.0f);
	Movement->StopMovementImmediately();
	Movement->Deactivate();
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Collision->ClearMoveIgnoreActors();
	Target.Reset();
	SourceASC = nullptr;
	DamageEffect = nullptr;
	OnRep_State();
	ForceNetUpdate();
}

void ARocketActor::OnRep_State()
{
	const bool bActive = State != ERocketState::Inactive;
	const bool bLaunched = State == ERocketState::Launched;
	SetActorHiddenInGame(!bActive);
	SetActorTickEnabled(State == ERocketState::Spawning || State == ERocketState::Ready);
	SetFlightEffects(bLaunched);
	GetWorldTimerManager().ClearTimer(MeshRevealTimer);
	SpawnEffect->DeactivateImmediate();
	const bool bPlaySpawn = State == ERocketState::Spawning && SpawnEffect->GetAsset();
	Mesh->SetVisibility(State != ERocketState::Spawning || !bPlaySpawn || MeshRevealDelay <= 0.0f);
	if (State == ERocketState::Spawning && GetNetMode() != NM_DedicatedServer)
	{
		PlaySoundAtLocation(SpawnSound, GetActorLocation(), GetActorRotation());
	}
	if (bPlaySpawn)
	{
		if (GetNetMode() != NM_DedicatedServer)
		{
			SpawnEffect->Activate(true);
		}
		if (MeshRevealDelay > 0.0f)
		{
			GetWorldTimerManager().SetTimer(MeshRevealTimer, this, &ARocketActor::RevealMesh, MeshRevealDelay, false);
		}
	}
	if (State == ERocketState::Launched)
	{
		GetWorldTimerManager().ClearTimer(MeshRevealTimer);
		Mesh->SetVisibility(true);
	}
	UpdateBeam();
}

void ARocketActor::RevealMesh()
{
	State = ERocketState::Ready;
	Mesh->SetVisibility(true);
	UpdateBeam();
}

bool ARocketActor::CanLaunch() const
{
	return State == ERocketState::Ready;
}

void ARocketActor::LifeSpanExpired()
{
	if (HasAuthority())
	{
		OwningPool->ReleaseRocket(this);
	}
}

void ARocketActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (State != ERocketState::Spawning && State != ERocketState::Ready)
	{
		return;
	}
	if (HasAuthority() && GetOwner())
	{
		HoverPhase = FMath::Fmod(HoverPhase + DeltaSeconds * HoverFrequency * 2.0f * PI, 2.0f * PI);
		SetActorLocation(FMath::VInterpTo(GetActorLocation(), GetFormationLocation(), DeltaSeconds, FollowSpeed));
		const FVector Facing =
			Target.IsValid() ? CombatTargetLibrary::GetTargetPoint(Target.Get()) - GetActorLocation() : GetOwner()->GetActorForwardVector();
		SetActorRotation(Facing.Rotation());
	}
	UpdateBeam();
}

void ARocketActor::UpdateBeam()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (State != ERocketState::Spawning && State != ERocketState::Ready || !Mesh->IsVisible() || !Target.IsValid())
	{
		Beam->DeactivateImmediate();
		return;
	}
	const UNiagaraSystem* System = Beam->GetAsset();
	if (!System)
	{
		return;
	}
	const FVector End = CombatTargetLibrary::GetTargetPoint(Target.Get());
	const auto& Parameters = System->GetExposedParameters();
	if (Parameters.IndexOf(FNiagaraVariable(FNiagaraTypeDefinition::GetPositionDef(), BeamEndParameter)) != INDEX_NONE)
	{
		Beam->SetVariablePosition(BeamEndParameter, End);
	}
	else
	{
		Beam->SetVariableVec3(BeamEndParameter, End);
	}
	if (!Beam->IsActive())
	{
		Beam->Activate(true);
	}
}

void ARocketActor::Launch(const FVector& AimLocation, TSubclassOf<UGameplayEffect> InDamageEffect, float InAbilityLevel)
{
	if (!HasAuthority() || !CanLaunch())
	{
		return;
	}
	DamageEffect = InDamageEffect;
	AbilityLevel = InAbilityLevel;
	const FVector Destination = Target.IsValid() ? CombatTargetLibrary::GetTargetPoint(Target.Get()) : AimLocation;
	SetActorRotation((Destination - GetActorLocation()).Rotation());
	State = ERocketState::Launched;
	SetActorTickEnabled(false);
	Collision->IgnoreActorWhenMoving(GetOwner(), true);
	if (UWeaponComponent* Weapons = GetOwner()->FindComponentByClass<UWeaponComponent>())
	{
		Collision->IgnoreActorWhenMoving(Weapons->GetEquippedWeapon(), true);
	}
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Movement->SetUpdatedComponent(Collision);
	Movement->MaxSpeed = FlightSpeed;
	Movement->Velocity = GetActorForwardVector() * FlightSpeed;
	Movement->Activate();
	OnRep_State();
	ExecuteCue(LaunchCue, GetActorLocation());
	MulticastPlayLaunchSound(GetActorLocation());
	SetLifeSpan(FlightLifetime);
	ForceNetUpdate();
}

void ARocketActor::OnHit(
	UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!HasAuthority() || State != ERocketState::Launched)
	{
		return;
	}
	State = ERocketState::Detonated;
	FWeaponRadialDamageRequest Request;
	Request.EffectCauser = this;
	Request.World = GetWorld();
	Request.Origin = Hit.ImpactPoint;
	Request.Radius = ExplosionRadius;
	Request.InstigatorActor = GetOwner();
	Request.SourceASC = SourceASC;
	Request.DamageEffectClass = DamageEffect;
	Request.DamageAmount = Damage;
	Request.AbilityLevel = AbilityLevel;
	Request.bHostileTargetsOnly = true;
	WeaponDamageLibrary::ApplyGasRadialDamage(Request);
	ExecuteCue(ExplosionCue, Hit.ImpactPoint);
	OwningPool->ReleaseRocket(this);
}

void ARocketActor::ExecuteCue(FGameplayTag Tag, const FVector& Location)
{
	if (!SourceASC || !Tag.IsValid())
	{
		return;
	}
	FGameplayCueParameters Parameters;
	Parameters.Instigator = GetOwner();
	Parameters.Location = Location;
	Parameters.Normal = GetActorForwardVector();
	Parameters.RawMagnitude = ExplosionRadius;
	SourceASC->ExecuteGameplayCue(Tag, Parameters);
}

void ARocketActor::MulticastPlayLaunchSound_Implementation(FVector_NetQuantize Location)
{
	PlaySoundAtLocation(LaunchSound, Location, FRotator::ZeroRotator);
}

void ARocketActor::PlaySoundAtLocation(const FWeaponSoundSettings& Settings, const FVector& Location, const FRotator& Rotation) const
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const FWeaponResolvedSound Sound = Settings.Resolve();
	if (Sound.IsValid())
	{
		GameplayCueFxLibrary::SpawnSoundAtLocation(
			GetWorld(), Sound.Sound, Location, Rotation, Sound.Volume, Sound.Pitch, Sound.Attenuation);
	}
}


void ARocketActor::SetFlightEffects(bool bEnabled)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (bEnabled)
	{
		if (!Trail->IsActive())
		{
			Trail->Activate(true);
		}
		if (!FlightAudio->IsPlaying())
		{
			const FWeaponResolvedSound Sound = FlightSound.Resolve();
			if (Sound.IsValid())
			{
				FlightAudio->SetSound(Sound.Sound);
				FlightAudio->SetVolumeMultiplier(Sound.Volume);
				FlightAudio->SetPitchMultiplier(Sound.Pitch);
				FlightAudio->AttenuationSettings = Sound.Attenuation;
				FlightAudio->Play();
			}
		}
	}
	else
	{
		Trail->DeactivateImmediate();
		FlightAudio->Stop();
		FlightAudio->SetSound(nullptr);
	}
}

void ARocketActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(MeshRevealTimer);
	SpawnEffect->DeactivateImmediate();
	SetFlightEffects(false);
	Beam->DeactivateImmediate();
	Super::EndPlay(EndPlayReason);
}

void ARocketActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARocketActor, State);
	DOREPLIFETIME(ARocketActor, Target);
}
