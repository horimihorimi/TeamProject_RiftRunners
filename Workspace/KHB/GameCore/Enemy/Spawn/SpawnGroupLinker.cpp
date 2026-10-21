#include "SpawnGroupLinker.h"

#include "Engine/Level.h"
#include "GameFramework/Actor.h"
#include "GameCore/Enemy/Spawn/SpawnPoint.h"

TArray<ASpawnPoint*> FSpawnGroupLinker::FindSpawnPoints(const AActor* Context, FName GroupId)
{
	TArray<ASpawnPoint*> Result;
	if (!IsValid(Context) || GroupId.IsNone())
	{
		return Result;
	}

	// 조각 BP 안이면 가장 바깥 부모(조각)까지 올라가 그 아래만 본다
	const AActor* Root = Context;
	while (AActor* Parent = Root->GetParentActor())
	{
		Root = Parent;
	}

	TArray<AActor*> Candidates;
	if (Root != Context)
	{
		Root->GetAllChildActors(Candidates, true);
	}
	else if (const ULevel* Level = Context->GetLevel())
	{
		for (AActor* Actor : Level->Actors)
		{
			Candidates.Add(Actor);
		}
	}

	for (AActor* Candidate : Candidates)
	{
		ASpawnPoint* Point = Cast<ASpawnPoint>(Candidate);
		if (IsValid(Point) && Point->GetSpawnGroupId() == GroupId)
		{
			Result.Add(Point);
		}
	}

	if (Result.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[SpawnGroup] 그룹 '%s' 에 속한 스폰 포인트가 없다 | From=%s — ID 오타 확인"),
			*GroupId.ToString(), *GetNameSafe(Context));
	}

	return Result;
}
