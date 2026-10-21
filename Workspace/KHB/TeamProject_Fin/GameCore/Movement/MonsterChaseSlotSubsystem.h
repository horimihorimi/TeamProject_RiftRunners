#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MonsterChaseSlotSubsystem.generated.h"

/** 추적 목적지만 배정한다. 공격 허용 여부와 실제 이동은 기존 AI/Movement가 맡는다. */
UCLASS()
class TEAMPROJECT_FIN_API UMonsterChaseSlotSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	bool ResolveGoal(AActor* Owner, AActor* Target, bool bFlying, int32 SlotCount,
		float Radius, float WaitingSpacing, int32 WaitingRings, float RefreshInterval,
		FVector& OutFloorGoal, bool& bOutWaiting);
	void Release(AActor* Owner);

private:
	struct FReservation
	{
		TWeakObjectPtr<AActor> Owner;
		TWeakObjectPtr<AActor> Target;
		bool bFlying = false;
		int32 SlotCount = 0;
		int32 Ring = 0;
		int32 Slot = 0;
		float Radius = 0;
		float Spacing = 0;
		float BodyRadius = 0;
		FVector Goal = FVector::ZeroVector;
		double LastUse = 0;
		double NextRefresh = 0;
	};
	bool ValidateGoal(AActor* Owner, AActor* Target, bool bFlying, const FVector& Raw,
		FVector& OutFloorGoal) const;
	bool IsReserved(const FReservation& Candidate, const AActor* Owner) const;
	TArray<FReservation> Reservations;
};
