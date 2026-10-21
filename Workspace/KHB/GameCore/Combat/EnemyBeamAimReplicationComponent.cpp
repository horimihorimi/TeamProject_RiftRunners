#include "EnemyBeamAimReplicationComponent.h"

#include "Net/UnrealNetwork.h"

UEnemyBeamAimReplicationComponent::UEnemyBeamAimReplicationComponent()
{
	SetIsReplicatedByDefault(true);
}

void UEnemyBeamAimReplicationComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UEnemyBeamAimReplicationComponent, bBeamAimActive);
	DOREPLIFETIME(UEnemyBeamAimReplicationComponent, BeamAimDirection);
}

void UEnemyBeamAimReplicationComponent::SetServerAimDirection(const FVector& InDirection)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	const FVector SafeDirection = InDirection.GetSafeNormal();
	if (!SafeDirection.IsNearlyZero())
	{
		BeamAimDirection = SafeDirection;
	}
	bBeamAimActive = true;
}

void UEnemyBeamAimReplicationComponent::StopServerAim()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		bBeamAimActive = false;
	}
}

bool UEnemyBeamAimReplicationComponent::GetReplicatedAimDirection(FVector& OutDirection) const
{
	if (!bBeamAimActive || BeamAimDirection.IsNearlyZero())
	{
		return false;
	}

	OutDirection = BeamAimDirection;
	return true;
}
