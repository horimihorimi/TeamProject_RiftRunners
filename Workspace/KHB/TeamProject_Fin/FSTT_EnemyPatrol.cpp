#include "FSTT_EnemyPatrol.h"

#include "Engine/World.h"
#include "NavigationSystem.h"
#include "GameCore/AI/MonsterAIController.h"
#include "GameCore/Character/CommonEnemyCharacter.h"
#include "GameCore/Enemy/Patrol/PatrolArea.h"
#include "GameCore/Movement/MonsterMovementComponent.h"
#include "StateTreeExecutionContext.h"

namespace EnemyPatrolStateTree
{
	static ACommonEnemyCharacter* GetCharacter(const FStateTreeExecutionContext& Context)
	{
		const AMonsterAIController* Controller = Cast<AMonsterAIController>(Context.GetOwner());
		return IsValid(Controller) ? Cast<ACommonEnemyCharacter>(Controller->GetPawn()) : nullptr;
	}

	/** 스폰 포인트가 넘겨 준 영역. 영역을 담은 스폰 포인트가 이미 사라졌으면 널. */
	static const IPatrolArea* GetArea(const ACommonEnemyCharacter* Character)
	{
		if (!IsValid(Character))
		{
			return nullptr;
		}

		const TScriptInterface<IPatrolArea> Area = Character->GetPatrolArea();
		return IsValid(Area.GetObject()) ? Area.GetInterface() : nullptr;
	}

	/*
	 * 영역 안에서 다음 목적지를 고른다.
	 *
	 * 중심은 항상 영역 중심이다. 현재 위치 기준으로 뽑으면 조금씩 흘러가 영역을 벗어난다.
	 * 현재 위치에서 최소 거리 이상 떨어진 지점이 나올 때까지 다시 뽑고, 끝내 안 나오면
	 * (벽 모서리처럼 좁은 곳) 뽑은 것 중 가장 먼 지점으로 간다. 제자리에 서 있지 않게 한다.
	 */
	static bool PickDestination(
		UWorld* World,
		const IPatrolArea& Area,
		const FVector& From,
		const int32 MaxAttempts,
		FVector& OutDestination)
	{
		const FVector Center = Area.GetPatrolCenter();
		const float Radius = Area.GetPatrolRadius();
		const float MinDistSq = FMath::Square(Area.GetMinMoveDistance());
		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);

		bool bFound = false;
		float BestDistSq = -1.0f;

		for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
		{
			FVector Candidate;
			FNavLocation NavLocation;
			if (NavSys && NavSys->GetRandomReachablePointInRadius(Center, Radius, NavLocation))
			{
				Candidate = NavLocation.Location;
			}
			else
			{
				// NavMesh가 없는 곳(비행 전용 구역 등)은 원 안의 임의 지점으로 대신한다
				const FVector2D Offset = FMath::RandPointInCircle(Radius);
				Candidate = Center + FVector(Offset.X, Offset.Y, 0.0);
			}

			const float DistSq = FVector::DistSquared2D(From, Candidate);
			if (DistSq > BestDistSq)
			{
				BestDistSq = DistSq;
				OutDestination = Candidate;
				bFound = true;
			}

			if (DistSq >= MinDistSq)
			{
				break;
			}
		}

		return bFound;
	}
}

FSTT_EnemyPatrol::FSTT_EnemyPatrol()
{
	bShouldCallTick = true;
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

EStateTreeRunStatus FSTT_EnemyPatrol::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const ACommonEnemyCharacter* Character = EnemyPatrolStateTree::GetCharacter(Context);

	if (!EnemyPatrolStateTree::GetArea(Character))
	{
		// 순찰 영역을 받지 못한 배치다. 제자리 대기 상태가 이 실패를 받는다.
		return EStateTreeRunStatus::Failed;
	}

	// 전투에서 돌아왔을 수도 있다. 쉬지 않고 곧바로 새 목적지를 잡아 영역으로 향한다
	InstanceData.bHasDestination = false;
	InstanceData.WaitEndTime = -1.0;
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FSTT_EnemyPatrol::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	ACommonEnemyCharacter* Character = EnemyPatrolStateTree::GetCharacter(Context);
	UMonsterMovementComponent* MovementComponent = IsValid(Character)
		? Cast<UMonsterMovementComponent>(Character->GetCharacterMovement())
		: nullptr;
	UWorld* World = IsValid(Character) ? Character->GetWorld() : nullptr;
	const IPatrolArea* Area = EnemyPatrolStateTree::GetArea(Character);

	if (!IsValid(MovementComponent) || !IsValid(World) || !Area)
	{
		return EStateTreeRunStatus::Failed;
	}

	// 목적지에서 머무는 중이면 시간이 다 될 때까지 움직이지 않는다.
	const double Now = World->GetTimeSeconds();
	if (InstanceData.WaitEndTime >= 0.0)
	{
		if (Now < InstanceData.WaitEndTime)
		{
			return EStateTreeRunStatus::Running;
		}

		InstanceData.WaitEndTime = -1.0;
	}

	// 목적지가 없거나 너무 오래 못 닿으면(막힘) 새로 뽑는다
	if (!InstanceData.bHasDestination || Now - InstanceData.LegStartTime > InstanceData.MaxLegDuration)
	{
		FVector Picked;
		if (!EnemyPatrolStateTree::PickDestination(
			World, *Area, Character->GetActorLocation(), InstanceData.MaxPickAttempts, Picked))
		{
			return EStateTreeRunStatus::Running;
		}

		// 영역 지점은 바닥에 찍힌다. 비행 적은 그 위를 날도록 높이를 더한다.
		Picked.Z += Character->GetPatrolHeightOffset();
		InstanceData.Destination = Picked;
		InstanceData.bHasDestination = true;
		InstanceData.LegStartTime = Now;
	}

	if (FVector::Dist(Character->GetActorLocation(), InstanceData.Destination) <= InstanceData.AcceptanceRadius)
	{
		MovementComponent->ResetMovementState();
		InstanceData.bHasDestination = false;

		const float WaitTime = Area->GetRandomWaitTime();
		if (WaitTime > 0.0f)
		{
			InstanceData.WaitEndTime = Now + WaitTime;
		}
		return EStateTreeRunStatus::Running;
	}

	// 이동 방식(지상 NavMesh / 비행)은 이동 컴포넌트가 정한다. 순찰은 목적지만 넘긴다.
	MovementComponent->UpdateMovementTowardLocation(InstanceData.Destination, DeltaTime);
	return EStateTreeRunStatus::Running;
}

void FSTT_EnemyPatrol::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	ACommonEnemyCharacter* Character = EnemyPatrolStateTree::GetCharacter(Context);
	if (UMonsterMovementComponent* MovementComponent = IsValid(Character)
		? Cast<UMonsterMovementComponent>(Character->GetCharacterMovement())
		: nullptr)
	{
		// 전투로 넘어갈 때 순찰 경로가 남아 있으면 추격과 섞인다.
		MovementComponent->ResetMovementState();
	}
}
