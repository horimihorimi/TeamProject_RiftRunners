#include "PatrolAppearance.h"

#include "GameCore/Character/MonsterCharacter.h"

void UPatrolAppearance::Apply(AMonsterCharacter* Monster, AActor* InstigatorHint)
{
	if (IsValid(Monster))
	{
		Monster->StartPatrol(TScriptInterface<IPatrolArea>(this));
	}
}

FVector UPatrolAppearance::GetPatrolCenter() const
{
	// 조각이 어디에 조립되든 스폰 포인트를 따라간다
	const AActor* SpawnPoint = GetOwningSpawnPoint();
	return IsValid(SpawnPoint) ? SpawnPoint->GetActorLocation() : FVector::ZeroVector;
}
