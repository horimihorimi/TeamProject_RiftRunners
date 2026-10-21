#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Actor.h"
#include "GCN_NiagaraSoundCueBase.generated.h"

class UAudioComponent;
class UNiagaraComponent;

UCLASS(Abstract, Blueprintable)
class TEAMPROJECT_FIN_API AGCN_NiagaraSoundCueBase : public AGameplayCueNotify_Actor
{
	GENERATED_BODY()

protected:
	AGCN_NiagaraSoundCueBase();

	void StopAndDestroyNiagara(UNiagaraComponent*& NiagaraComponent) const;
	void StopAndDestroySound(UAudioComponent*& AudioComponent) const;
	void ClearCueTimer(FTimerHandle& TimerHandle) const;
};
