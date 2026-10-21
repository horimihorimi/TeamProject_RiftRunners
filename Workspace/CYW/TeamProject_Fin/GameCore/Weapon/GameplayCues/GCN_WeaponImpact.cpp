#include "GCN_WeaponImpact.h"

#include "../Core/WeaponBase.h"
#include "../Data/WeaponDataAsset.h"
#include "../Libraries/WeaponCueLibrary.h"
#include "../../Ability/GameplayCues/GameplayCueFxLibrary.h"
#include "../../Tags/GameplayTagSystem.h"

UGCN_WeaponImpact::UGCN_WeaponImpact()
{
	GameplayCueTag = GameplayTagSystem::GameplayCue_Weapon_Impact_Surface;
	GameplayCueName = GameplayCueTag.GetTagName();
}

UGCN_WeaponImpactCharacter::UGCN_WeaponImpactCharacter()
{
	GameplayCueTag = GameplayTagSystem::GameplayCue_Weapon_Impact_Character;
	GameplayCueName = GameplayCueTag.GetTagName();
}

bool UGCN_WeaponImpact::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	const FWeaponCueContext CueContext = WeaponCueLibrary::ResolveWeaponCueContext(Parameters);
	if (!CueContext.FeedbackSet)
	{
		return false;
	}

	const AWeaponBase* Weapon = CueContext.Weapon;
	const FWeaponFeedbackSet* FeedbackSet = CueContext.FeedbackSet;
	const bool bCharacterImpact =
		Parameters.OriginalTag == GameplayTagSystem::GameplayCue_Weapon_Impact_Character
		|| Parameters.MatchedTagName == GameplayTagSystem::GameplayCue_Weapon_Impact_Character;
	const bool bBounceImpact = Parameters.RawMagnitude < 0.0f;
	const bool bExplosionImpact = Parameters.RawMagnitude > 0.0f;

	UNiagaraSystem* ImpactEffect = bCharacterImpact
		? FeedbackSet->CharacterImpactEffect
		: FeedbackSet->SurfaceImpactEffect;
	if (bExplosionImpact && FeedbackSet->ExplosionEffect)
	{
		ImpactEffect = FeedbackSet->ExplosionEffect;
	}
	if (bBounceImpact)
	{
		ImpactEffect = nullptr;
	}

	UWorld* World = MyTarget ? MyTarget->GetWorld() : (Weapon ? Weapon->GetWorld() : nullptr);

	if (ImpactEffect && World)
	{
		GameplayCueFxLibrary::SpawnNiagaraAtLocation(
			World,
			ImpactEffect,
			Parameters.Location,
			Parameters.Normal.Rotation()
		);
	}

	FWeaponResolvedSound ImpactSound = FeedbackSet->ResolveImpactSound();
	if (bExplosionImpact)
	{
		ImpactSound = FeedbackSet->ResolveExplosionSound();
	}
	else if (bBounceImpact)
	{
		ImpactSound = FeedbackSet->ResolveBounceSound();
	}

	if (ImpactSound.IsValid() && World)
	{
		GameplayCueFxLibrary::SpawnSoundAtLocation(
			World,
			ImpactSound.Sound,
			Parameters.Location,
			Parameters.Normal.Rotation(),
			ImpactSound.Volume,
			ImpactSound.Pitch,
			ImpactSound.Attenuation
		);
	}

	return true;
}
