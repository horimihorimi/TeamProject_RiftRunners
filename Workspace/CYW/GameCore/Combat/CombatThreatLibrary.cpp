#include "CombatThreatLibrary.h"
#include "IncomingAttackReceiver.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Engine/World.h"
#include "CollisionShape.h"
#include "Kismet/GameplayStatics.h"
#include "GameCore/Character/BaseCharacter.h"

namespace
{
	bool NotifyTarget(AActor* Target, AActor* Instigator, const FVector& Origin)
	{
		if (!IsValid(Target))
		{
			return false;
		}
		IIncomingAttackReceiver* Receiver = Cast<IIncomingAttackReceiver>(Target);
		if (!Receiver)
		{
			const APawn* Pawn = Cast<APawn>(Target);
			Receiver = Pawn ? Cast<IIncomingAttackReceiver>(Pawn->GetController()) : nullptr;
		}
		return Receiver && Receiver->NotifyIncomingAttack(Instigator, Origin);
	}
}

bool CombatThreatLibrary::NotifyHitScanThreat(AActor* Instigator, const FHitResult& Hit)
{
	return NotifyTarget(Hit.GetActor(), Instigator, Hit.TraceStart);
}

void CombatThreatLibrary::NotifyProjectileThreats(const FProjectileThreatRequest& Request)
{
	if (!Request.World || !IsValid(Request.Instigator) || Request.Speed <= 0.0f)
	{
		return;
	}
	FPredictProjectilePathParams Params(12.0f, Request.Origin, Request.Direction * Request.Speed,
		FMath::Max(Request.Range / FMath::Max(Request.Speed, 1.0f), 0.1f),
		ECC_TraceAttack, Request.Instigator);
	Params.bTraceComplex = false;
	Params.ActorsToIgnore.Add(Request.Causer);
	Params.ActorsToIgnore.Add(Request.Instigator);
	Params.OverrideGravityZ = FMath::IsNearlyZero(Request.GravityZ) ? KINDA_SMALL_NUMBER : Request.GravityZ;
	FPredictProjectilePathResult Result;
	UGameplayStatics::PredictProjectilePath(Request.World, Params, Result);
	if (Result.PathData.Num() < 2)
	{
		return;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PredictedProjectileThreats), false);
	QueryParams.AddIgnoredActor(Request.Causer);
	QueryParams.AddIgnoredActor(Request.Instigator);

	FCollisionObjectQueryParams PawnQuery;
	PawnQuery.AddObjectTypesToQuery(ECC_Pawn);
	const FCollisionShape TraceShape = FCollisionShape::MakeSphere(
		FMath::Max(Request.TraceRadius, 0.0f));

	// PathData는 발사 위치부터 첫 지형 충돌 지점까지 순서대로 정렬된다.
	// 각 구간을 순회하다 첫 Pawn만 회피 후보로 사용한다.
	for (int32 Index = 1; Index < Result.PathData.Num(); ++Index)
	{
		const FVector SegmentStart = Result.PathData[Index - 1].Location;
		const FVector SegmentEnd = Result.PathData[Index].Location;
		if (SegmentStart.Equals(SegmentEnd))
		{
			continue;
		}

		TArray<FHitResult> Hits;
		if (!Request.World->SweepMultiByObjectType(
			Hits,
			SegmentStart,
			SegmentEnd,
			FQuat::Identity,
			PawnQuery,
			TraceShape,
			QueryParams))
		{
			continue;
		}

		const FHitResult* FirstPawnHit = nullptr;
		for (const FHitResult& Hit : Hits)
		{
			if (IsValid(Hit.GetActor())
				&& (!FirstPawnHit || Hit.Time < FirstPawnHit->Time))
			{
				FirstPawnHit = &Hit;
			}
		}

		if (FirstPawnHit)
		{
			NotifyTarget(FirstPawnHit->GetActor(), Request.Instigator, Request.Origin);
			return;
		}
	}
}
