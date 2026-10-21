#include "WeaponTrajectoryPreviewComponent.h"
#include "DashedSplineRenderer.h"
#include "Engine/World.h"

#include "../Core/WeaponBase.h"
#include "../Core/WeaponComponent.h"
#include "../Projectiles/WeaponProjectile.h"
#include "../../Character/BaseCharacter.h"
#include "../../Tags/GameplayTagSystem.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStaticsTypes.h"
#include "Kismet/GameplayStatics.h"

UWeaponTrajectoryPreviewComponent::UWeaponTrajectoryPreviewComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	SetIsReplicatedByDefault(false);
}

void UWeaponTrajectoryPreviewComponent::BeginPlay()
{
	Super::BeginPlay();

	// Rendering state belongs to this live component, never to a Blueprint template.
	Renderer = NewObject<UDashedSplineRenderer>(this);
	Renderer->Initialize(GetOwner());
	RefreshCachedComponents();
}

void UWeaponTrajectoryPreviewComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Renderer)
	{
		Renderer->Shutdown();
		Renderer = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void UWeaponTrajectoryPreviewComponent::RefreshCachedComponents()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (!WeaponComponent)
	{
		WeaponComponent = Owner->FindComponentByClass<UWeaponComponent>();
	}

	UAbilitySystemComponent* CurrentAbilitySystemComponent = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Owner);
	if (AbilitySystemComponent != CurrentAbilitySystemComponent)
	{
		AbilitySystemComponent = CurrentAbilitySystemComponent;
	}
}

void UWeaponTrajectoryPreviewComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	RefreshCachedComponents();
	if (!Renderer)
	{
		return;
	}

	if (!ShouldDrawTrajectory())
	{
		Renderer->Hide();
		return;
	}

	if (AWeaponBase* Weapon = GetPreviewWeapon())
	{
		DrawTrajectory(Weapon, DeltaTime);
		return;
	}

	Renderer->Hide();
}

bool UWeaponTrajectoryPreviewComponent::ShouldDrawTrajectory() const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	return OwnerPawn
		&& OwnerPawn->IsLocallyControlled()
		&& AbilitySystemComponent
		&& AbilitySystemComponent->HasMatchingGameplayTag(GameplayTagSystem::State_Weapon_Aiming)
		&& !AbilitySystemComponent->HasMatchingGameplayTag(GameplayTagSystem::State_Weapon_Reloading);
}

AWeaponBase* UWeaponTrajectoryPreviewComponent::GetPreviewWeapon() const
{
	AWeaponBase* Weapon = WeaponComponent ? WeaponComponent->GetEquippedWeapon() : nullptr;
	if (!Weapon)
	{
		return nullptr;
	}

	const FWeaponRuntimeStats& RuntimeStats = Weapon->GetRuntimeStats();
	const bool bIsGrenadeLauncher = Weapon->GetWeaponCategory() == EWeaponCategory::GrenadeLauncher;
	const bool bUsesProjectile = RuntimeStats.FireType == EWeaponFireType::Projectile;
	return bIsGrenadeLauncher && bUsesProjectile ? Weapon : nullptr;
}

void UWeaponTrajectoryPreviewComponent::DrawTrajectory(AWeaponBase* Weapon, float DeltaTime)
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner || !Weapon)
	{
		return;
	}

	const AWeaponProjectile* ProjectileDefaults = Weapon->GetWeaponData()
		&& Weapon->GetWeaponData()->ProjectileClass
		? Weapon->GetWeaponData()->ProjectileClass->GetDefaultObject<AWeaponProjectile>()
		: nullptr;
	const float ProjectileSpeed = ProjectileDefaults ? ProjectileDefaults->GetProjectileSpeed() : 3000.0f;
	const FTransform FireTransform = Weapon->GetFireTransform(Owner);
	const FVector FireDirection = Weapon->GetPreviewFireDirection(Owner);

	FPredictProjectilePathParams Params(
		ProjectileRadius,
		FireTransform.GetLocation(),
		FireDirection * ProjectileSpeed,
		MaxSimTime,
		ECC_TraceAttack,
		Owner
	);
	Params.SimFrequency = SimFrequency;
	Params.DrawDebugType = EDrawDebugTrace::None;
	Params.DrawDebugTime = DrawDuration;
	Params.bTraceComplex = false;
	Params.ActorsToIgnore.Add(Weapon);

	Params.OverrideGravityZ = ProjectileDefaults && ProjectileDefaults->IsProjectileAffectedByGravity()
		? World->GetGravityZ() * ProjectileDefaults->GetProjectileGravityScale()
		: KINDA_SMALL_NUMBER;

	FPredictProjectilePathResult Result;
	UGameplayStatics::PredictProjectilePath(World, Params, Result);

	TArray<FVector> Positions;
	Positions.Reserve(Result.PathData.Num());
	for (const FPredictProjectilePathPointData& Point : Result.PathData)
	{
		Positions.Add(Point.Location);
	}

	FDashedSplineStyle Style;
	Style.Mesh = DashMesh;
	Style.Material = DashMaterial;
	Style.Length = DashLength;
	Style.Gap = DashGap;
	Style.MoveSpeed = DashMoveSpeed;
	Style.Scale = DashScale;
	Style.ForwardAxis = DashForwardAxis;
	Renderer->Update(Positions, Style, World->GetTimeSeconds());
}
