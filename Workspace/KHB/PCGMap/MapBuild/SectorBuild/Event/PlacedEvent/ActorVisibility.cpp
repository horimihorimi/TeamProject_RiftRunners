#include "ActorVisibility.h"
#include "GameCore/Match/ActorVisibilitySubsystem.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

AActorVisibility::AActorVisibility()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
}

void AActorVisibility::BeginPlay()
{
	Super::BeginPlay();
	if (bApplyOnBeginPlay)
	{
		// 클라이언트에서도 시작 상태를 적용하되, 이후 실행된 상태를 우선한다.
		if (HasAuthority()) ApplyVisibility();
		else ApplyLocally(ExecutionRevision);
	}
}

void AActorVisibility::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AActorVisibility, ExecutionRevision);
}

void AActorVisibility::Execute(const UObject* WorldContext)
{
	if (!HasAuthority()) return;
	Super::Execute(WorldContext);
	ApplyVisibility();
	RunEventGroups(PostEvents);
}

void AActorVisibility::ApplyVisibility()
{
	if (!HasAuthority()) return;
	ExecutionRevision = GetWorld()->GetSubsystem<UActorVisibilitySubsystem>()->NextRevision();
	MulticastApply(ExecutionRevision);
	ForceNetUpdate();
}

void AActorVisibility::OnRep_ExecutionRevision()
{
	if (ExecutionRevision > 0) ApplyLocally(ExecutionRevision);
}

void AActorVisibility::MulticastApply_Implementation(int64 Revision)
{
	ApplyLocally(Revision);
}

void AActorVisibility::ApplyLocally(int64 Revision)
{
	UActorVisibilitySubsystem* State = GetWorld()->GetSubsystem<UActorVisibilitySubsystem>();
	const auto SetGroup = [this, State, Revision](const TArray<TObjectPtr<AActor>>& Group, bool bVisible)
	{
		for (AActor* Actor : Group)
		{
			if (!IsValid(Actor) || Actor == this || !State->Accept(Actor, Revision)) continue;
			Actor->SetActorHiddenInGame(!bVisible);
			if (bUpdateCollision) Actor->SetActorEnableCollision(bVisible);
		}
	};
	// 목적지를 먼저 켠다. 중복 지정된 액터는 HideActors를 우선한다.
	SetGroup(ShowActors, true);
	SetGroup(HideActors, false);
}

