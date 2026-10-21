#include "GCN_WeaponFire.h"

#include "../Core/WeaponBase.h"
#include "../Data/WeaponDataAsset.h"
#include "../Libraries/WeaponCueLibrary.h"
#include "../../Ability/GameplayCues/GameplayCueFxLibrary.h"
#include "../../Tags/GameplayTagSystem.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

UGCN_WeaponFire::UGCN_WeaponFire()
{
	GameplayCueTag = GameplayTagSystem::GameplayCue_Weapon_Fire;
	GameplayCueName = GameplayCueTag.GetTagName();
}

bool UGCN_WeaponFire::OnExecute_Implementation(
	AActor* MyTarget,
	const FGameplayCueParameters& Parameters) const
{
	const FWeaponCueContext CueContext =
		WeaponCueLibrary::ResolveWeaponCueContext(Parameters);

	if (!CueContext.FeedbackSet)
	{
		return false;
	}

	const AWeaponBase* Weapon = CueContext.Weapon;
	const FWeaponFeedbackSet* FeedbackSet = CueContext.FeedbackSet;

	UStaticMeshComponent* WeaponMesh = Weapon ? Weapon->GetWeaponMesh() : nullptr;
	USceneComponent* FireAttachComponent = Parameters.TargetAttachComponent.Get();
	if (!FireAttachComponent && Weapon)
	{
		FireAttachComponent = Weapon->GetFireCueAttachComponent();
	}
	UWorld* World = MyTarget ? MyTarget->GetWorld() : (Weapon ? Weapon->GetWorld() : nullptr);

	const bool bWeaponAttachment = FireAttachComponent && Weapon && FireAttachComponent->GetOwner() == Weapon;
	const FName FireAttachSocket =
		bWeaponAttachment
		? Weapon->GetFirePointSocketName()
		: NAME_None;

	// 총구 화염
	if (FeedbackSet->MuzzleFlashEffect)
	{
		if (FireAttachComponent)
		{
			GameplayCueFxLibrary::SpawnNiagaraAttached(
				FeedbackSet->MuzzleFlashEffect,
				FireAttachComponent,
				FireAttachSocket,
				EAttachLocation::SnapToTarget
			);
		}
		else
		{
			GameplayCueFxLibrary::SpawnNiagaraAtLocation(
				World,
				FeedbackSet->MuzzleFlashEffect,
				Parameters.Location,
				Parameters.Normal.Rotation()
			);
		}
	}

	// 히트스캔 궤적
	if (FeedbackSet->HitScanTracerEffect &&
		Parameters.RawMagnitude > 0.0f)
	{
		const FVector TraceDirection =
			Parameters.Normal.GetSafeNormal();
		const FRotator TraceRotation = TraceDirection.Rotation();

		GameplayCueFxLibrary::SpawnNiagaraAtLocation(
			World,
			FeedbackSet->HitScanTracerEffect,
			Parameters.Location,
			TraceRotation,
			ENCPoolMethod::AutoRelease
		);
	}

	// 탄피 배출
	if (FeedbackSet->ShellEjectionEffect && WeaponMesh && bWeaponAttachment)
	{
		if (Weapon->HasShellEjectSocket())
		{
			GameplayCueFxLibrary::SpawnNiagaraAttached(
				FeedbackSet->ShellEjectionEffect,
				WeaponMesh,
				Weapon->GetShellEjectSocketName(),
				EAttachLocation::SnapToTarget
			);
		}
		else
		{
			GameplayCueFxLibrary::SpawnNiagaraAtLocation(
				World,
				FeedbackSet->ShellEjectionEffect,
				Weapon->GetActorLocation(),
				Weapon->GetActorRotation()
			);
		}
	}

	// 발사 사운드
	const FWeaponResolvedSound FireSound = FeedbackSet->ResolveFireSound();
	if (FireSound.IsValid())
	{
		if (FireAttachComponent)
		{
			GameplayCueFxLibrary::SpawnSoundAttached(
				FireSound.Sound,
				FireAttachComponent,
				FireAttachSocket,
				EAttachLocation::SnapToTarget,
				FireSound.Volume,
				FireSound.Pitch,
				FireSound.Attenuation
			);
		}
		else
		{
			GameplayCueFxLibrary::SpawnSoundAtLocation(
				World,
				FireSound.Sound,
				Parameters.Location,
				Parameters.Normal.Rotation(),
				FireSound.Volume,
				FireSound.Pitch,
				FireSound.Attenuation
			);
		}
	}

	return true;
}
