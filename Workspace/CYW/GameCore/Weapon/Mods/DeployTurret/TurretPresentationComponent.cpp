#include "TurretPresentationComponent.h"

#include "TurretActor.h"
#include "TurretCombatComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayCueManager.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameCore/Ability/GameplayCues/GameplayCueFxLibrary.h"
#include "GameCore/Tags/GameplayTagSystem.h"
#include "NiagaraComponent.h"
#include "TimerManager.h"

UTurretPresentationComponent::UTurretPresentationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

ATurretActor* UTurretPresentationComponent::GetTurret() const
{
	return CastChecked<ATurretActor>(GetOwner());
}

void UTurretPresentationComponent::Start()
{
	PlaySoundAtLocation(SpawnSound);
	UpdateLoopSound();
	const bool bPlaySpawn = GetTurret()->GetSpawnEffect()->GetAsset() != nullptr;
	if (bPlaySpawn && GetNetMode() != NM_DedicatedServer)
	{
		GetTurret()->GetSpawnEffect()->Activate(true);
	}
	if (bPlaySpawn && MeshRevealDelay > 0.0f)
	{
		GetTurret()->GetMesh()->SetVisibility(false);
		GetWorld()->GetTimerManager().SetTimer(MeshRevealTimer, this, &UTurretPresentationComponent::RevealMesh, MeshRevealDelay, false);
	}
	else
	{
		RevealMesh();
	}
}

void UTurretPresentationComponent::RevealMesh()
{
	bReady = true;
	GetTurret()->GetMesh()->SetVisibility(true);
	if (!GetTurret()->IsDeactivated() && GetNetMode() != NM_DedicatedServer)
	{
		GetTurret()->GetTrail()->Activate(true);
	}
	GetTurret()->GetCombat()->NotifyTargetDetected();
}

void UTurretPresentationComponent::PlaySoundAtLocation(const FWeaponSoundSettings& Settings) const
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const FWeaponResolvedSound Sound = Settings.Resolve();
	if (Sound.IsValid())
	{
		GameplayCueFxLibrary::SpawnSoundAtLocation(GetWorld(), Sound.Sound, GetOwner()->GetActorLocation(), GetOwner()->GetActorRotation(),
			Sound.Volume, Sound.Pitch, Sound.Attenuation);
	}
}

void UTurretPresentationComponent::UpdateLoopSound()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (GetTurret()->IsDeactivated())
	{
		GetTurret()->GetLoopAudio()->Stop();
		GetTurret()->GetLoopAudio()->SetSound(nullptr);
		return;
	}
	if (!GetTurret()->GetLoopAudio()->IsPlaying())
	{
		const FWeaponResolvedSound Sound = LoopSound.Resolve();
		if (Sound.IsValid())
		{
			GetTurret()->GetLoopAudio()->SetSound(Sound.Sound);
			GetTurret()->GetLoopAudio()->SetVolumeMultiplier(Sound.Volume);
			GetTurret()->GetLoopAudio()->SetPitchMultiplier(Sound.Pitch);
			GetTurret()->GetLoopAudio()->AttenuationSettings = Sound.Attenuation;
			GetTurret()->GetLoopAudio()->Play();
		}
	}
}

void UTurretPresentationComponent::MulticastFire_Implementation(FGameplayCueParameters Parameters)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	// Use the shared cue locally, without playing the player's firing montage or camera feedback.
	Parameters.TargetAttachComponent = GetTurret()->GetMuzzle();
	if (UGameplayCueManager* Manager = UAbilitySystemGlobals::Get().GetGameplayCueManager())
	{
		Manager->HandleGameplayCue(GetOwner(), GameplayTagSystem::GameplayCue_Weapon_Fire, EGameplayCueEvent::Executed, Parameters);
	}
}

void UTurretPresentationComponent::Stop()
{
	if (bStopped)
	{
		return;
	}
	bStopped = true;
	GetWorld()->GetTimerManager().ClearTimer(MeshRevealTimer);
	GetTurret()->GetMesh()->SetVisibility(true);
	GetTurret()->GetTrail()->Deactivate();
	GetTurret()->GetSpawnEffect()->Deactivate();
	UpdateLoopSound();
	PlaySoundAtLocation(DeactivateSound);
}

void UTurretPresentationComponent::PlayDetectSound()
{
	if (bReady && !GetTurret()->IsDeactivated())
	{
		PlaySoundAtLocation(DetectSound);
	}
}

void UTurretPresentationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetTurret()->GetLoopAudio()->Stop();
	GetWorld()->GetTimerManager().ClearTimer(MeshRevealTimer);
	Super::EndPlay(EndPlayReason);
}
