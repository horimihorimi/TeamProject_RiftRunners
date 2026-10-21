#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "IncomingAttackReceiver.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UIncomingAttackReceiver : public UInterface
{
	GENERATED_BODY()
};

class TEAMPROJECT_FIN_API IIncomingAttackReceiver
{
	GENERATED_BODY()
public:
	// True accepts an evade: instantaneous hit damage must be suppressed.
	virtual bool NotifyIncomingAttack(AActor* Attacker, const FVector& AttackOrigin) = 0;
};
