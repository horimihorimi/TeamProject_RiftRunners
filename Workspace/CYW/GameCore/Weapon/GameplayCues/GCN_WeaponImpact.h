#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "GCN_WeaponImpact.generated.h"

UCLASS(Blueprintable)
class TEAMPROJECT_FIN_API UGCN_WeaponImpact : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	UGCN_WeaponImpact();

	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
};

UCLASS(Blueprintable)
class TEAMPROJECT_FIN_API UGCN_WeaponImpactCharacter : public UGCN_WeaponImpact
{
	GENERATED_BODY()

public:
	UGCN_WeaponImpactCharacter();
};
