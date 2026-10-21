#include "GCN_AttachNiagaraAndSound.h"

#include "GameplayCueFxLibrary.h"
#include "../../Weapon/Core/WeaponBase.h"
#include "../../Weapon/Core/WeaponComponent.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "NiagaraComponent.h"

AGCN_AttachNiagaraAndSound::AGCN_AttachNiagaraAndSound()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void AGCN_AttachNiagaraAndSound::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (AttachTarget != EGameplayCueAttachTarget::TargetFeet)
	{
		return;
	}

	AActor* Target = ActiveTarget.Get();
	USceneComponent* TargetRoot = Target ? Target->GetRootComponent() : nullptr;
	if (!TargetRoot)
	{
		return;
	}

	const FVector RelativeFeetLocation = TargetRoot->GetComponentTransform().InverseTransformPosition(
		GameplayCueFxLibrary::GetActorFeetLocation(Target)
	);
	if (IsValid(EffectComponent))
	{
		EffectComponent->SetRelativeLocation(RelativeFeetLocation);
	}
	if (IsValid(SoundComponent))
	{
		SoundComponent->SetRelativeLocation(RelativeFeetLocation);
	}
}

bool AGCN_AttachNiagaraAndSound::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	bExecutedBurst = true;
	bBurstEffectFinished = true;
	bBurstSoundFinished = true;
	if (!StartEffects(MyTarget, Parameters))
	{
		bExecutedBurst = false;
		GameplayCueFinishedCallback();
		return false;
	}

	if (EffectComponent)
	{
		bBurstEffectFinished = false;
		EffectComponent->OnSystemFinished.AddUniqueDynamic(this, &AGCN_AttachNiagaraAndSound::OnBurstEffectFinished);
	}
	if (SoundComponent)
	{
		bBurstSoundFinished = false;
		SoundComponent->OnAudioFinished.AddUniqueDynamic(this, &AGCN_AttachNiagaraAndSound::OnBurstSoundFinished);
	}

	return true;
}

bool AGCN_AttachNiagaraAndSound::OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	BindWeaponChanged(MyTarget, Parameters);
	return StartEffects(MyTarget, Parameters);
}

bool AGCN_AttachNiagaraAndSound::WhileActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	BindWeaponChanged(MyTarget, Parameters);
	return StartEffects(MyTarget, Parameters);
}

bool AGCN_AttachNiagaraAndSound::OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	UnbindWeaponChanged();
	StopEffects();
	bExecutedBurst = false;
	GameplayCueFinishedCallback();
	return true;
}

bool AGCN_AttachNiagaraAndSound::Recycle()
{
	UnbindWeaponChanged();
	StopEffects();
	bExecutedBurst = false;
	return Super::Recycle();
}

void AGCN_AttachNiagaraAndSound::OnBurstEffectFinished(UNiagaraComponent* FinishedComponent)
{
	if (!bExecutedBurst || FinishedComponent != EffectComponent)
	{
		return;
	}

	EffectComponent->OnSystemFinished.RemoveDynamic(this, &AGCN_AttachNiagaraAndSound::OnBurstEffectFinished);
	EffectComponent = nullptr;
	bBurstEffectFinished = true;
	TryFinishBurst();
}

void AGCN_AttachNiagaraAndSound::OnBurstSoundFinished()
{
	if (!bExecutedBurst)
	{
		return;
	}

	if (SoundComponent)
	{
		SoundComponent->OnAudioFinished.RemoveDynamic(this, &AGCN_AttachNiagaraAndSound::OnBurstSoundFinished);
	}
	SoundComponent = nullptr;
	bBurstSoundFinished = true;
	TryFinishBurst();
}

void AGCN_AttachNiagaraAndSound::TryFinishBurst()
{
	if (bExecutedBurst && bBurstEffectFinished && bBurstSoundFinished)
	{
		bExecutedBurst = false;
		GameplayCueFinishedCallback();
	}
}

void AGCN_AttachNiagaraAndSound::BindWeaponChanged(
	AActor* MyTarget,
	const FGameplayCueParameters& Parameters)
{
	if (AttachTarget != EGameplayCueAttachTarget::EquippedWeapon || !MyTarget)
	{
		return;
	}

	UWeaponComponent* WeaponComponent = MyTarget->FindComponentByClass<UWeaponComponent>();
	if (BoundWeaponComponent.Get() != WeaponComponent)
	{
		UnbindWeaponChanged();
		BoundWeaponComponent = WeaponComponent;
		if (WeaponComponent)
		{
			WeaponComponent->OnWeaponEquipped.AddDynamic(
				this,
				&AGCN_AttachNiagaraAndSound::OnEquippedWeaponChanged
			);
		}
	}

	ActiveTarget = MyTarget;
	ActiveParameters = Parameters;
}

void AGCN_AttachNiagaraAndSound::UnbindWeaponChanged()
{
	if (UWeaponComponent* WeaponComponent = BoundWeaponComponent.Get())
	{
		WeaponComponent->OnWeaponEquipped.RemoveDynamic(
			this,
			&AGCN_AttachNiagaraAndSound::OnEquippedWeaponChanged
		);
	}
	BoundWeaponComponent.Reset();
	ActiveTarget.Reset();
}

void AGCN_AttachNiagaraAndSound::OnEquippedWeaponChanged(AWeaponBase*)
{
	if (bExecutedBurst || AttachTarget != EGameplayCueAttachTarget::EquippedWeapon)
	{
		return;
	}

	StopEffects();
	StartEffects(ActiveTarget.Get(), ActiveParameters);
}

bool AGCN_AttachNiagaraAndSound::StartEffects(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	if (AttachTarget == EGameplayCueAttachTarget::TargetFeet)
	{
		ActiveTarget = MyTarget;
		SetActorTickEnabled(true);
	}
	FName AttachSocketName = NAME_None;
	FVector RelativeLocation = FVector::ZeroVector;
	USceneComponent* AttachComponent = ResolveAttachComponent(
		MyTarget,
		Parameters,
		AttachSocketName,
		RelativeLocation
	);
	if (!AttachComponent || (!Effect && !Sound))
	{
		return false;
	}

	if (!EffectComponent && Effect)
	{
		EffectComponent = GameplayCueFxLibrary::SpawnNiagaraAttached(
			Effect,
			AttachComponent,
			AttachSocketName,
			EAttachLocation::SnapToTarget,
			ENCPoolMethod::None,
			true
		);
		if (EffectComponent)
		{
			EffectComponent->SetRelativeLocation(RelativeLocation);
		}
	}

	if (!SoundComponent && Sound)
	{
		SoundComponent = GameplayCueFxLibrary::SpawnSoundAttached(
			Sound,
			AttachComponent,
			AttachSocketName,
			EAttachLocation::SnapToTarget,
			SoundVolume,
			SoundPitch,
			SoundAttenuation
		);
		if (SoundComponent)
		{
			SoundComponent->SetRelativeLocation(RelativeLocation);
		}
	}

	return EffectComponent || SoundComponent;
}

void AGCN_AttachNiagaraAndSound::StopEffects()
{
	SetActorTickEnabled(false);
	if (IsValid(EffectComponent))
	{
		EffectComponent->OnSystemFinished.RemoveDynamic(this, &AGCN_AttachNiagaraAndSound::OnBurstEffectFinished);
	}
	UNiagaraComponent* EffectComponentPtr = EffectComponent.Get();
	StopAndDestroyNiagara(EffectComponentPtr);
	EffectComponent = EffectComponentPtr;

	if (IsValid(SoundComponent))
	{
		SoundComponent->OnAudioFinished.RemoveDynamic(this, &AGCN_AttachNiagaraAndSound::OnBurstSoundFinished);
	}
	UAudioComponent* SoundComponentPtr = SoundComponent.Get();
	StopAndDestroySound(SoundComponentPtr);
	SoundComponent = SoundComponentPtr;
	bBurstEffectFinished = true;
	bBurstSoundFinished = true;
}

USceneComponent* AGCN_AttachNiagaraAndSound::ResolveAttachComponent(
	AActor* MyTarget,
	const FGameplayCueParameters& Parameters,
	FName& OutSocketName,
	FVector& OutRelativeLocation) const
{
	OutSocketName = NAME_None;
	OutRelativeLocation = FVector::ZeroVector;

	if (!MyTarget)
	{
		return nullptr;
	}

	if (AttachTarget == EGameplayCueAttachTarget::TargetFeet)
	{
		USceneComponent* TargetRoot = MyTarget->GetRootComponent();
		if (TargetRoot)
		{
			OutRelativeLocation = TargetRoot->GetComponentTransform().InverseTransformPosition(
				GameplayCueFxLibrary::GetActorFeetLocation(MyTarget)
			);
		}
		return TargetRoot;
	}

	if (AttachTarget == EGameplayCueAttachTarget::EquippedWeapon)
	{
		const UWeaponComponent* WeaponComponent = MyTarget->FindComponentByClass<UWeaponComponent>();
		const AWeaponBase* Weapon = WeaponComponent ? WeaponComponent->GetEquippedWeapon() : nullptr;
		if (Weapon)
		{
			if (USceneComponent* WeaponAttachComponent = Weapon->GetFireCueAttachComponent())
			{
				OutSocketName = Weapon->GetFirePointSocketName();
				return WeaponAttachComponent;
			}

			return Weapon->GetRootComponent();
		}

		if (!WeaponComponent)
		{
			if (USceneComponent* CueAttachComponent = Parameters.TargetAttachComponent.Get())
			{
				OutRelativeLocation = CueAttachComponent->GetComponentTransform().InverseTransformPosition(
					Parameters.Location
				);
				return CueAttachComponent;
			}
		}
	}

	return MyTarget->GetRootComponent();
}
