#include "GCN_NiagaraSoundCueBase.h"

#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "NiagaraComponent.h"
#include "TimerManager.h"

AGCN_NiagaraSoundCueBase::AGCN_NiagaraSoundCueBase()
{
	bAutoDestroyOnRemove = false;
}

void AGCN_NiagaraSoundCueBase::StopAndDestroyNiagara(UNiagaraComponent*& NiagaraComponent) const
{
	if (IsValid(NiagaraComponent))
	{
		NiagaraComponent->DeactivateImmediate();
		NiagaraComponent->DestroyComponent();
	}
	NiagaraComponent = nullptr;
}

void AGCN_NiagaraSoundCueBase::StopAndDestroySound(UAudioComponent*& AudioComponent) const
{
	if (IsValid(AudioComponent))
	{
		AudioComponent->Stop();
		AudioComponent->DestroyComponent();
	}
	AudioComponent = nullptr;
}

void AGCN_NiagaraSoundCueBase::ClearCueTimer(FTimerHandle& TimerHandle) const
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimerHandle);
	}
}
