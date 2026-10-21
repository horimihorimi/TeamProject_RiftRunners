#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "GCN_WeaponFire.generated.h"

UCLASS(Blueprintable)
class TEAMPROJECT_FIN_API UGCN_WeaponFire : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	UGCN_WeaponFire();

	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
};
