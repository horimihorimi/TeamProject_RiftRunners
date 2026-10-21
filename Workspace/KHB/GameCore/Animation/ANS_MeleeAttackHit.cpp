#include "GameCore/Animation/ANS_MeleeAttackHit.h"

#include "Components/SkeletalMeshComponent.h"

void UANS_MeleeAttackHit::OnWindowBegin(USkeletalMeshComponent* MeshComp)
{
	// 첫 틱이 원점에서 소켓까지 훑지 않도록 모든 명시 소켓의 현재 위치를 저장한다.
	TMap<FName, FVector>& Locations = PreviousSocketLocations.FindOrAdd(MeshComp);
	Locations.Reset();
	for (const FName SocketName : HitSocketNames)
	{
		Locations.Add(SocketName, MeshComp->GetSocketLocation(SocketName));
	}
}

void UANS_MeleeAttackHit::OnWindowEnd(USkeletalMeshComponent* MeshComp)
{
	PreviousSocketLocations.Remove(MeshComp);
}

bool UANS_MeleeAttackHit::GetTraceSegment(
	USkeletalMeshComponent* MeshComp,
	const float DeltaTime,
	FVector& OutStart,
	FVector& OutEnd)
{
	// 기반 클래스의 단일 구간 경로와 호환하기 위한 구현이다.
	TMap<FName, FVector>* Locations = PreviousSocketLocations.Find(MeshComp);
	if (Locations == nullptr || HitSocketNames.IsEmpty())
	{
		return false;
	}

	const FName SocketName = HitSocketNames[0];
	const FVector* PreviousLocation = Locations->Find(SocketName);
	if (PreviousLocation == nullptr)
	{
		return false;
	}

	OutStart = *PreviousLocation;
	OutEnd = MeshComp->GetSocketLocation(SocketName);
	Locations->Add(SocketName, OutEnd);
	return true;
}

void UANS_MeleeAttackHit::GetTraceSegments(
	USkeletalMeshComponent* MeshComp,
	const float DeltaTime,
	TArray<FHitTraceSegment>& OutSegments)
{
	TMap<FName, FVector>* Locations = PreviousSocketLocations.Find(MeshComp);
	if (Locations == nullptr)
	{
		return;
	}

	for (const FName SocketName : HitSocketNames)
	{
		const FVector* PreviousLocation = Locations->Find(SocketName);
		if (PreviousLocation == nullptr)
		{
			continue;
		}

		const FVector CurrentLocation = MeshComp->GetSocketLocation(SocketName);
		OutSegments.Add({ *PreviousLocation, CurrentLocation });
		Locations->Add(SocketName, CurrentLocation);
	}
}
