#include "MonsterChaseSlotSubsystem.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationData.h"
#include "NavigationSystem.h"

void UMonsterChaseSlotSubsystem::Release(AActor* Owner)
{
	Reservations.RemoveAll([Owner](const FReservation& Entry) { return Entry.Owner.Get() == Owner; });
}

bool UMonsterChaseSlotSubsystem::IsReserved(const FReservation& Candidate, const AActor* Owner) const
{
	for (const FReservation& Entry : Reservations)
	{
		if (Entry.Owner.Get() == Owner || Entry.Target != Candidate.Target || Entry.bFlying != Candidate.bFlying)
		{
			continue;
		}
		if ((Entry.SlotCount == Candidate.SlotCount && Entry.Ring == Candidate.Ring && Entry.Slot == Candidate.Slot
			&& FMath::IsNearlyEqual(Entry.Radius, Candidate.Radius) && FMath::IsNearlyEqual(Entry.Spacing, Candidate.Spacing))
			|| FVector::DistSquared2D(Entry.Goal, Candidate.Goal) < FMath::Square(Entry.BodyRadius + Candidate.BodyRadius + 10.f))
		{
			return true;
		}
	}
	return false;
}

bool UMonsterChaseSlotSubsystem::ValidateGoal(AActor* Owner, AActor* Target, bool bFlying,
	const FVector& Raw, FVector& OutFloorGoal) const
{
	const ACharacter* Character = Cast<ACharacter>(Owner);
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Character || (!bFlying && !Nav)) { return false; }
	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ChaseSlot), false, Owner);
	Params.AddIgnoredActor(Target);
	FCollisionResponseParams Responses;
	Responses.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);
	FNavLocation Projected;
	if (bFlying)
	{
		// 열린 비행 공간은 NavMesh가 없어도 기존 비행 이동으로 접근할 수 있다.
		FHitResult Floor;
		if (!GetWorld()->LineTraceSingleByChannel(Floor, Raw + FVector(0, 0, 100.f), Raw - FVector(0, 0, 3000.f),
			Capsule->GetCollisionObjectType(), Params, Responses) || Floor.bStartPenetrating) { return false; }
		Projected.Location = Floor.ImpactPoint;
	}
	else if (!Nav->ProjectPointToNavigation(Raw, Projected, FVector(60.f, 60.f, 250.f))) { return false; }
	// 투영으로 벽 반대편에 크게 옮겨진 후보는 채택하지 않는다.
	if (FVector::DistSquared2D(Raw, Projected.Location) > FMath::Square(60.f)) { return false; }
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FVector Center = Projected.Location + FVector(0, 0, bFlying
		? FMath::Max(HalfHeight + 4.f, Character->GetActorLocation().Z - Projected.Location.Z)
		: HalfHeight + 4.f);
	if (GetWorld()->OverlapBlockingTestByChannel(Center, FQuat::Identity, Capsule->GetCollisionObjectType(),
		FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), HalfHeight), Params, Responses)) { return false; }
	FHitResult Hit;
	const FVector TargetCenter(Target->GetActorLocation().X, Target->GetActorLocation().Y, Center.Z);
	if (GetWorld()->SweepSingleByChannel(Hit, TargetCenter, Center, FQuat::Identity, Capsule->GetCollisionObjectType(),
		FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), HalfHeight), Params, Responses)) { return false; }
	if (!bFlying)
	{
		const ANavigationData* NavData = Nav->GetNavDataForProps(Character->GetCharacterMovement()->GetNavAgentPropertiesRef(), Owner->GetActorLocation());
		if (!NavData) { return false; }
		const FVector Feet = Owner->GetActorLocation() - FVector(0, 0, HalfHeight);
		FPathFindingQuery Query(Owner, *NavData, Feet, Projected.Location);
		Query.SetAllowPartialPaths(false);
		if (!Nav->TestPathSync(Query)) { return false; }
	}
	OutFloorGoal = Projected.Location;
	return true;
}

bool UMonsterChaseSlotSubsystem::ResolveGoal(AActor* Owner, AActor* Target, bool bFlying, int32 SlotCount,
	float Radius, float WaitingSpacing, int32 WaitingRings, float RefreshInterval,
	FVector& OutFloorGoal, bool& bOutWaiting)
{
	if (!IsValid(Owner) || !Owner->HasAuthority() || !IsValid(Target)) { return false; }
	const double Now = GetWorld()->GetTimeSeconds();
	Reservations.RemoveAll([Now](const FReservation& Entry)
	{
		return !Entry.Owner.IsValid() || !Entry.Target.IsValid() || Now - Entry.LastUse > 2.0;
	});
	SlotCount = FMath::Clamp(SlotCount, 1, 32);
	WaitingRings = FMath::Clamp(WaitingRings, 0, 4);
	Radius = FMath::Max(Radius, 1.f);
	WaitingSpacing = FMath::Max(WaitingSpacing, 1.f);
	FReservation* Existing = Reservations.FindByPredicate([Owner](const FReservation& Entry) { return Entry.Owner.Get() == Owner; });
	if (Existing && Existing->Target.Get() == Target && Existing->bFlying == bFlying
		&& Existing->SlotCount == SlotCount && FMath::IsNearlyEqual(Existing->Radius, Radius)
		&& FMath::IsNearlyEqual(Existing->Spacing, WaitingSpacing) && Existing->Ring <= WaitingRings)
	{
		Existing->LastUse = Now;
		if (Now < Existing->NextRefresh)
		{
			OutFloorGoal = Existing->Goal;
			bOutWaiting = Existing->Ring > 0;
			return true;
		}
	}
	else { Release(Owner); Existing = nullptr; }

	const ACharacter* Character = Cast<ACharacter>(Owner);
	if (!Character) { return false; }
	FReservation Best;
	bool bFound = false;
	for (int32 Ring = 0; Ring <= WaitingRings; ++Ring)
	{
		float BestDistance = TNumericLimits<float>::Max();
		for (int32 Index = 0; Index < SlotCount; ++Index)
		{
			const int32 Slot = (Index + (Existing && Existing->Ring == Ring ? Existing->Slot : 0)) % SlotCount;
			FReservation Candidate;
			Candidate.Owner = Owner; Candidate.Target = Target; Candidate.bFlying = bFlying;
			Candidate.SlotCount = SlotCount; Candidate.Ring = Ring; Candidate.Slot = Slot;
			Candidate.Radius = Radius; Candidate.Spacing = WaitingSpacing;
			Candidate.BodyRadius = Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
			const float Angle = 2.f * PI * Slot / SlotCount;
			const FVector Raw = Target->GetActorLocation() + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0)
				* (Radius + Ring * WaitingSpacing);
			Candidate.Goal = Raw;
			if (IsReserved(Candidate, Owner)) { continue; }
			if (!ValidateGoal(Owner, Target, bFlying, Raw, Candidate.Goal) || IsReserved(Candidate, Owner)) { continue; }
			float Distance = FVector::DistSquared2D(Owner->GetActorLocation(), Candidate.Goal);
			// 같은 링에서는 기존 배정을 우선해 좌우 왕복을 막는다.
			if (Existing && Existing->Ring == Ring && Existing->Slot == Slot) { Distance = -1.f; }
			if (Distance < BestDistance) { BestDistance = Distance; Best = Candidate; bFound = true; }
			if (Distance < 0.f) { break; }
		}
		if (bFound) { break; }
	}
	Release(Owner);
	if (!bFound) { return false; }
	Best.LastUse = Now;
	Best.NextRefresh = Now + FMath::Max(RefreshInterval, 0.05f);
	Reservations.Add(Best);
	OutFloorGoal = Best.Goal;
	bOutWaiting = Best.Ring > 0;
	return true;
}
