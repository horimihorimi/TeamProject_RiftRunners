#include "CombatTargetLibrary.h"

#include "../Tags/GameplayTagSystem.h"
#include "../Character/BaseCharacter.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"

namespace
{
	enum class ECombatTeam : uint8
	{
		None,
		Player,
		Enemy
	};

	ECombatTeam GetCombatTeam(const AActor* Actor)
	{
		const ABaseCharacter* Character = Cast<ABaseCharacter>(Actor);
		if (!Character)
		{
			return ECombatTeam::None;
		}

		const FGameplayTag RoleTag = Character->GetCharacterRoleTag();
		if (RoleTag.MatchesTag(GameplayTagSystem::Character_Player))
		{
			return ECombatTeam::Player;
		}
		if (RoleTag.MatchesTag(GameplayTagSystem::Character_Enemy)
			|| RoleTag.MatchesTag(GameplayTagSystem::Character_Boss))
		{
			return ECombatTeam::Enemy;
		}
		return ECombatTeam::None;
	}

	ECombatTeam GetCombatTeam(const UAbilitySystemComponent* ASC)
	{
		if (!ASC)
		{
			return ECombatTeam::None;
		}

		const ECombatTeam AvatarTeam = GetCombatTeam(ASC->GetAvatarActor());
		if (AvatarTeam != ECombatTeam::None)
		{
			return AvatarTeam;
		}
		if (ASC->HasMatchingGameplayTag(GameplayTagSystem::Character_Player))
		{
			return ECombatTeam::Player;
		}
		if (ASC->HasMatchingGameplayTag(GameplayTagSystem::Character_Enemy)
			|| ASC->HasMatchingGameplayTag(GameplayTagSystem::Character_Boss))
		{
			return ECombatTeam::Enemy;
		}
		return ECombatTeam::None;
	}

	bool HasLineOfSight(UWorld* World, const FVector& Start, const FVector& End, const AActor* Source, const AActor* Target)
	{
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(LineOfSightTrace), true, Source);
		QueryParams.AddIgnoredActor(Target);
		return !World->LineTraceTestByChannel(Start, End, ECC_Visibility, QueryParams);
	}
}

namespace CombatTargetLibrary
{
	bool AreHostile(const AActor* SourceActor, const AActor* TargetActor)
	{
		const ECombatTeam SourceTeam = GetCombatTeam(SourceActor);
		const ECombatTeam TargetTeam = GetCombatTeam(TargetActor);
		return SourceTeam != ECombatTeam::None
			&& TargetTeam != ECombatTeam::None
			&& SourceTeam != TargetTeam;
	}

	bool AreFriendly(const UAbilitySystemComponent* SourceASC, const UAbilitySystemComponent* TargetASC)
	{
		const ECombatTeam SourceTeam = GetCombatTeam(SourceASC);
		return SourceTeam != ECombatTeam::None && SourceTeam == GetCombatTeam(TargetASC);
	}

	bool AreHostile(const UAbilitySystemComponent* SourceASC, const UAbilitySystemComponent* TargetASC)
	{
		const ECombatTeam SourceTeam = GetCombatTeam(SourceASC);
		const ECombatTeam TargetTeam = GetCombatTeam(TargetASC);
		return SourceTeam != ECombatTeam::None
			&& TargetTeam != ECombatTeam::None
			&& SourceTeam != TargetTeam;
	}

	bool IsHostileTarget(
		const AActor* TargetActor,
		const UAbilitySystemComponent* SourceASC,
		const AActor* SourceActor,
		bool bAllowDeadTarget)
	{
		if (!TargetActor || !SourceASC || TargetActor == SourceActor)
		{
			return false;
		}

		const UAbilitySystemComponent* TargetASC =
			UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor);
		return TargetASC
			&& (bAllowDeadTarget || !TargetASC->HasMatchingGameplayTag(GameplayTagSystem::State_Dead))
			&& AreHostile(SourceActor ? SourceActor : SourceASC->GetAvatarActor(), TargetActor);
	}

	FVector GetTargetPoint(const AActor* TargetActor, FName SocketName)
	{
		if (!TargetActor)
		{
			return FVector::ZeroVector;
		}

		if (!SocketName.IsNone())
		{
			if (const USkeletalMeshComponent* Mesh = TargetActor->FindComponentByClass<USkeletalMeshComponent>())
			{
				if (Mesh->DoesSocketExist(SocketName))
				{
					return Mesh->GetSocketLocation(SocketName);
				}
			}
		}
		return TargetActor->GetActorLocation();
	}

	TArray<AActor*> SelectRandomHostileTargets(
		UWorld* World,
		const FVector& Origin,
		const AActor* PrimaryTarget,
		const UAbilitySystemComponent* SourceASC,
		AActor* SourceActor,
		float SearchRadius,
		float MaxStepDistance,
		int32 MaxTargets,
		bool bRequireLineOfSight,
		FName TargetSocketName)
	{
		TArray<AActor*> SelectedTargets;
		if (!World || !PrimaryTarget || !SourceASC || SearchRadius <= 0.0f
			|| MaxStepDistance <= 0.0f || MaxTargets <= 0)
		{
			return SelectedTargets;
		}

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(HostileTargetOverlap), false, SourceActor);
		QueryParams.AddIgnoredActor(PrimaryTarget);
		FCollisionObjectQueryParams ObjectQueryParams;
		ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

		TArray<FOverlapResult> OverlapResults;
		World->OverlapMultiByObjectType(
			OverlapResults,
			Origin,
			FQuat::Identity,
			ObjectQueryParams,
			FCollisionShape::MakeSphere(SearchRadius),
			QueryParams
		);

		TArray<AActor*> Candidates;
		TSet<AActor*> UniqueCandidates;
		for (const FOverlapResult& Result : OverlapResults)
		{
			AActor* Candidate = Result.GetActor();
			if (!UniqueCandidates.Contains(Candidate)
				&& IsHostileTarget(Candidate, SourceASC, SourceActor))
			{
				UniqueCandidates.Add(Candidate);
				Candidates.Add(Candidate);
			}
		}

		FVector CurrentPoint = Origin;
		const AActor* PreviousTarget = PrimaryTarget;
		const float MaxDistanceSquared = FMath::Square(MaxStepDistance);
		TArray<int32> ReachableIndices;
		ReachableIndices.Reserve(Candidates.Num());
		while (SelectedTargets.Num() < MaxTargets && !Candidates.IsEmpty())
		{
			ReachableIndices.Reset();
			for (int32 Index = 0; Index < Candidates.Num(); ++Index)
			{
				const FVector CandidatePoint = GetTargetPoint(Candidates[Index], TargetSocketName);
				if (FVector::DistSquared(CurrentPoint, CandidatePoint) <= MaxDistanceSquared
					&& (!bRequireLineOfSight || HasLineOfSight(World, CurrentPoint, CandidatePoint, PreviousTarget, Candidates[Index])))
				{
					ReachableIndices.Add(Index);
				}
			}

			if (ReachableIndices.IsEmpty())
			{
				break;
			}

			const int32 CandidateIndex = ReachableIndices[FMath::RandHelper(ReachableIndices.Num())];
			AActor* SelectedTarget = Candidates[CandidateIndex];
			Candidates.RemoveAtSwap(CandidateIndex, EAllowShrinking::No);
			SelectedTargets.Add(SelectedTarget);
			CurrentPoint = GetTargetPoint(SelectedTarget, TargetSocketName);
			PreviousTarget = SelectedTarget;
		}

		return SelectedTargets;
	}
}
