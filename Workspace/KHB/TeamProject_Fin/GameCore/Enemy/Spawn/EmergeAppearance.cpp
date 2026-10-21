#include "EmergeAppearance.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

void UEmergeAppearance::Apply(AMonsterCharacter* Monster, AActor* InstigatorHint)
{
	if (!IsValid(Monster))
	{
		return;
	}

	APawn* Target = Cast<APawn>(InstigatorHint);
	if (!Target || !Target->IsPlayerControlled())
	{
		Target = FindNearestPlayerPawn(Monster->GetActorLocation());
	}

	FMonsterEmergeParams Params;
	Params.SpawnType = SpawnType;
	Params.bUseTurnSection = bUseTurnSection;
	Params.bUseTurnRotationWarp = bUseTurnRotationWarp;

	Monster->PlayEmerge(Params, Target);
}

APawn* UEmergeAppearance::FindNearestPlayerPawn(const FVector& From) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	APawn* Nearest = nullptr;
	double NearestDistSq = TNumericLimits<double>::Max();

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APawn* Pawn = It->IsValid() ? It->Get()->GetPawn() : nullptr;
		if (!IsValid(Pawn))
		{
			continue;
		}

		const double DistSq = FVector::DistSquared(From, Pawn->GetActorLocation());
		if (DistSq < NearestDistSq)
		{
			NearestDistSq = DistSq;
			Nearest = Pawn;
		}
	}

	return Nearest;
}
