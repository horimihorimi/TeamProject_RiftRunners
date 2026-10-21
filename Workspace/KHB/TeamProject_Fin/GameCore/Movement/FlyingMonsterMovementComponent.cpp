#include "FlyingMonsterMovementComponent.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Components/CapsuleComponent.h"
#include "GameCore/Enemy/Data/EnemyDataAssets.h"
#include "GameFramework/Character.h"
#include "NavigationPath.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "GameCore/AI/MonsterAIController.h"

void UFlyingMonsterMovementComponent::SetFlightApproachMode(const EMonsterFlightApproachMode NewMode)
{
	FlightApproachMode = NewMode;
}

void UFlyingMonsterMovementComponent::SetStrafeEnabled(const bool bEnabled)
{
	bStrafeEnabled = bEnabled;
}

void UFlyingMonsterMovementComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// CMC 물리 계산이 끝난 뒤에 적용한다. 따라서 몽타주 중 CMC가 입력 Velocity를
	// 0으로 확정해도 이 위치 보정은 그 결과에 의존하지 않는다.
	ApplyAttackPositioningMovement(DeltaTime);
}

void UFlyingMonsterMovementComponent::UpdateMovementTowardTarget(AActor* InTargetActor, const float DeltaSeconds)
{
	// 패턴 이동 중에는 이동을 패턴이 맡는다. 추격 입력이 더해지면 접근 각도와 정지 거리가 무너진다.
	if (IsPatternMovementActive())
	{
		return;
	}

	ACharacter* Character = GetCharacterOwner();

	if (!IsValid(Character) || !IsValid(InTargetActor))
	{
		ResetMovementState();
		return;
	}

	TargetActor = InTargetActor;
	UpdateDynamicHoverAnchor();
	FVector ChaseGoal = GetFlyDestination();
	if (bUseChaseSlots)
	{
		if (!ResolveChaseSlotGoal(InTargetActor, true, ChaseGoal))
		{
			StopMovementImmediately();
			SmoothedFlyDirection = FVector::ZeroVector;
			return;
		}
	}
	else { ReleaseChaseSlot(); }
	// 먼저 경로를 갱신해야 첫 프레임부터 그 경로의 통과 높이로 보정할 수 있다.
	ChaseMovementDestination = ResolveChaseDestination(ChaseGoal, InTargetActor);
	UpdateDesiredFlightHeight(DeltaSeconds);
	DrawChaseDebug(InTargetActor);
	if (bChaseWaitingForPath)
	{
		StopMovementImmediately();
		SmoothedFlyDirection = FVector::ZeroVector;
		return;
	}

	if (bUseChaseSlots && bChaseSlotHolding)
	{
		bIsInStrafeMode = false;
		bHasStrafeDirection = false;
		Velocity.X = 0.f;
		Velocity.Y = 0.f;
		SmoothedFlyDirection = FVector::ZeroVector;
		ApplyVerticalMovement();
		if (bFaceTarget) { RotateTowardTarget(DeltaSeconds); }
		return;
	}

	if (bUseChaseSlots || !bStrafeEnabled)
	{
		bIsInStrafeMode = false;
		bHasStrafeDirection = false;
		PerformFlyMovement(DeltaSeconds);

		if (bFaceTarget)
		{
			RotateTowardTarget(DeltaSeconds);
		}

		return;
	}

	const float Distance = FVector::Dist2D(Character->GetActorLocation(), InTargetActor->GetActorLocation());
	const bool bWasInStrafeMode = bIsInStrafeMode;

	// 벽 너머로는 타깃 둘레를 돌 수 없다. 가까워 보여도 막혀 있으면 경로를 따라 접근하는 쪽으로 둔다
	if (bHasLocationPathQuery || bChaseWaitingForHeight ||
		IsStraightPathBlocked(Character->GetActorLocation(), InTargetActor->GetActorLocation(), InTargetActor))
	{
		bIsInStrafeMode = false;
	}
	else if (bIsInStrafeMode)
	{
		if (Distance > MovementRangeThreshold + MovementModeHysteresis)
		{
			bIsInStrafeMode = false;
		}
	}
	else if (Distance < MovementRangeThreshold - MovementModeHysteresis)
	{
		bIsInStrafeMode = true;
	}

	if (bIsInStrafeMode)
	{
		PerformStrafeMovement();
	}
	else
	{
		bHasStrafeDirection = false;
		PerformFlyMovement(DeltaSeconds);
	}

	if (bFaceTarget)
	{
		RotateTowardTarget(DeltaSeconds);
	}
}

void UFlyingMonsterMovementComponent::UpdateMovementTowardLocation(const FVector& Destination, const float DeltaSeconds)
{
	if (IsPatternMovementActive()) { return; }
	ReleaseChaseSlot();
	TargetActor.Reset();
	bIsInStrafeMode = false;
	bHasStrafeDirection = false;
	ACharacter* Character = GetCharacterOwner();
	if (!Character) { return; }
	ChaseMovementDestination = ResolveChaseDestination(Destination, nullptr);
	UpdateDesiredFlightHeight(DeltaSeconds);
	if (bChaseWaitingForPath)
	{
		StopMovementImmediately();
		SmoothedFlyDirection = FVector::ZeroVector;
		return;
	}
	PerformFlyMovement(DeltaSeconds);
	if (bFaceTarget)
	{
		const FVector Heading = (ChaseMovementDestination - Character->GetActorLocation()).GetSafeNormal2D();
		if (!Heading.IsNearlyZero())
		{
			Character->SetActorRotation(FMath::RInterpTo(Character->GetActorRotation(),
				Heading.Rotation(), DeltaSeconds, FacingInterpSpeed));
		}
	}
}

bool UFlyingMonsterMovementComponent::BuildLocationPath(const FVector& Destination)
{
	bLastPathBuildFailed = true;

	ACharacter* Character = GetCharacterOwner();
	UWorld* World = GetWorld();
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!Character || !NavSys)
	{
		return false;
	}

	// 공중 위치를 넓은 Z 범위로 투영하면 위쪽 지붕의 NavMesh가 선택될 수 있다.
	// 아래쪽 실제 바닥을 먼저 구하고 그 주변의 NavMesh만 선택한다.
	const FVector ProjectExtent(100.0f, 100.0f, FMath::Max(50.0f, MaxStepHeight));
	FNavLocation StartOnNav;
	FNavLocation EndOnNav;
	const ANavigationData* NavData = NavSys->GetNavDataForProps(GetNavAgentPropertiesRef(), Character->GetActorLocation());

	if (!NavData)
	{
		return false;
	}
	const auto ProjectFloorToNav = [&](const FVector& Location, FNavLocation& OutNav)
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(FlyPathFloorProjection), false, Character);
		if (TargetActor.IsValid())
		{
			Params.AddIgnoredActor(TargetActor.Get());
		}
		FCollisionResponseParams Responses;
		Responses.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);
		FHitResult FloorHit;
		const bool bHit = World->LineTraceSingleByChannel(FloorHit, Location,
			Location - FVector::UpVector * LocationPathProjectDepth,
			Character->GetCapsuleComponent()->GetCollisionObjectType(), Params, Responses);
		const bool bWalkable = bHit && !FloorHit.bStartPenetrating && FloorHit.ImpactNormal.Z >= GetWalkableFloorZ();
		const bool bProjected = bWalkable && NavSys->ProjectPointToNavigation(FloorHit.ImpactPoint, OutNav, ProjectExtent, NavData);

		return bProjected;
	};
	if (!ProjectFloorToNav(Destination, EndOnNav)) { return false; }
	TArray<FVector> JoinPoints;
	const UNavigationPath* Path = nullptr;
	if (ProjectFloorToNav(Character->GetActorLocation(), StartOnNav))
	{
		Path = UNavigationSystemV1::FindPathToLocationSynchronously(
			World, StartOnNav.Location, EndOnNav.Location, const_cast<ANavigationData*>(NavData));
	}
	// 경로 연결뿐 아니라 실제 몸체 통과까지 확인한 뒤 합류 탐색 여부를 결정한다.
	const auto ValidatePathBody = [&](const UNavigationPath* CandidatePath, bool bUseFullCapsule = false)
	{
		if (!CandidatePath || !CandidatePath->IsValid() || CandidatePath->PathPoints.Num() < 2)
		{
			return false;
		}
		NavPathFloorPoints.Reset(CandidatePath->PathPoints.Num());
		for (const FNavPathPoint& Point : CandidatePath->PathPoints)
		{
			NavPathFloorPoints.Add(Point.Location);
		}
		bHasLocationPathQuery = true;
		BuildPathClearance(bUseFullCapsule);
		return bPathBodyPassable;
	};
	const bool bOriginalPathPassable = ValidatePathBody(Path);
	if ((!bOriginalPathPassable || Path->IsPartial()) && World->GetTimeSeconds() >= NextFlightJoinSearchTime)
	{
		NextFlightJoinSearchTime = World->GetTimeSeconds() + 2.0;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(FlyNavJoin), false, Character);
		if (TargetActor.IsValid()) { Params.AddIgnoredActor(TargetActor.Get()); }
		FCollisionResponseParams Responses;
		Responses.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);
		const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
		const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		const FCollisionShape Body = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), HalfHeight);
		const FVector Current = Character->GetActorLocation();
		const auto Passable = [&](const FVector& From, const FVector& To)
		{
			FHitResult Hit;
			return !World->SweepSingleByChannel(Hit, From, To, FQuat::Identity,
				ObstacleTraceChannel, Body, Params, Responses);
		};
		float BestLength = TNumericLimits<float>::Max();
		const UNavigationPath* BestPath = nullptr;
		TArray<FVector> BestJoin;
		// 최대 34개 위치/높이 후보, 경로 탐색은 최대 8회로 제한한다.
		int32 PathQueries = 0;
		for (const float Distance : {0.0f, 250.0f, 500.0f})
		{
			for (int32 Direction = 0; Direction < (Distance == 0.0f ? 1 : 8); ++Direction)
			{
				const float Angle = Direction * PI / 4.0f;
				const FVector Offset(FMath::Cos(Angle) * Distance, FMath::Sin(Angle) * Distance, 0.0f);
				for (const float FloorZ : {EndOnNav.Location.Z, bHasGroundHeight ? SmoothedGroundHeight : StartOnNav.Location.Z})
				{
					FNavLocation Candidate;
					if (!NavSys->ProjectPointToNavigation(FVector(Current.X + Offset.X, Current.Y + Offset.Y, FloorZ),
						Candidate, ProjectExtent, NavData)) { continue; }
					FVector Join(Candidate.Location.X, Candidate.Location.Y,
						Candidate.Location.Z + HalfHeight + FloorClearanceMargin + HeightTolerance);
					const FVector Above(Join.X, Join.Y, Current.Z);
					// 먼저 옆으로 빠진 다음 하강한다. 지붕을 관통하는 수직 합류는 거부한다.
					if (PathQueries >= 8 || !Passable(Current, Above) || !Passable(Above, Join)) { continue; }
					++PathQueries;
					const UNavigationPath* CandidatePath = UNavigationSystemV1::FindPathToLocationSynchronously(
						World, Candidate.Location, EndOnNav.Location, const_cast<ANavigationData*>(NavData));
					if (!CandidatePath || !CandidatePath->IsValid() || CandidatePath->IsPartial()
						|| CandidatePath->PathPoints.Num() < 2 || !ValidatePathBody(CandidatePath, true)) { continue; }
					// 합류 높이도 경로 시작 지점의 실제 통과 범위에 맞춘다.
					if (ClearanceSamples.IsEmpty()) { continue; }
					const FPathClearanceSample& Entry = ClearanceSamples[0];
					Join.Z = FMath::Clamp(Join.Z, Entry.MinCenterZ, Entry.MaxCenterZ);
					if (!Passable(Above, Join)) { continue; }
					const float Length = FVector::Dist(Current, Above) + FVector::Dist(Above, Join) + CandidatePath->GetPathLength();
					if (Length < BestLength)
					{
						BestLength = Length;
						BestPath = CandidatePath;
						BestJoin = {Above, Join};
					}
				}
			}
		}
		if (BestPath) { Path = BestPath; JoinPoints = MoveTemp(BestJoin); }
	}
	if (!Path || !Path->IsValid() || Path->PathPoints.Num() < 2) { return false; }
	// 실패한 재탐색은 기존 경로를 지우지 않는다. 유효한 결과가 있을 때만 교체한다.
	ClearLocationPath();
	FlightJoinPoints = MoveTemp(JoinPoints);
	bHasLocationPathQuery = true;
	bLocationPathPartial = Path->IsPartial();
	bLastPathBuildFailed = false;
	LocationPathDestination = Destination;

	// 바닥 높이가 담긴 원본 지점은 통과 검사가 쓴다. 아래에서 저장하는 이동 지점은 높이를 얹은 값이다
	NavPathFloorPoints.Reset(Path->PathPoints.Num());
	for (const FNavPathPoint& PathPoint : Path->PathPoints)
	{
		NavPathFloorPoints.Add(PathPoint.Location);
	}

	// 경로 지점은 바닥에 찍힌다. 목적지가 바닥보다 떠 있던 만큼 모든 지점을 띄운다
	const float HeightOffset = Destination.Z - EndOnNav.Location.Z;

	// 첫 지점은 지금 서 있는 자리라 뺀다. 마지막 지점은 넘겨받은 목적지를 그대로 쓴다
	for (int32 i = 1; i < Path->PathPoints.Num() - 1; ++i)
	{
		LocationPathPoints.Add(Path->PathPoints[i] + FVector(0.0, 0.0, HeightOffset));
	}
	// 부분 경로의 끝을 타깃 위치로 바꾸면 아직 연결되지 않은 벽을 향해 직진하게 된다.
	LocationPathPoints.Add(bLocationPathPartial
		? Path->PathPoints.Last() + FVector(0.0, 0.0, HeightOffset) : Destination);
	// 후보 검사에서 쓴 임시 상태를 최종 경로의 통과 높이로 확정한다.
	BuildPathClearance(!FlightJoinPoints.IsEmpty());
	return true;
}

void UFlyingMonsterMovementComponent::ClearLocationPath()
{
	FlightJoinPoints.Reset();
	FlightJoinIndex = 0;
	LocationPathPoints.Reset();
	LocationPathIndex = 0;
	bHasLocationPathQuery = false;
	bLocationPathPartial = false;
	bLastPathBuildFailed = false;
	NavPathFloorPoints.Reset();
	ClearanceSamples.Reset();
	ClearanceSampleIndex = 0;
	bPathBodyPassable = true;
}

void UFlyingMonsterMovementComponent::BuildPathClearance(const bool bUseFullCapsule)
{
	ClearanceSamples.Reset();
	ClearanceSampleIndex = 0;
	bPathBodyPassable = true;

	ACharacter* Character = GetCharacterOwner();
	UWorld* World = GetWorld();
	const UCapsuleComponent* Capsule = IsValid(Character) ? Character->GetCapsuleComponent() : nullptr;
	if (!bValidateBodyClearance || !World || !Capsule || NavPathFloorPoints.Num() < 2)
	{
		return;
	}

	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const float Radius = Capsule->GetScaledCapsuleRadius();

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(FlyPathClearance), false, Character);
	FCollisionResponseParams Responses;
	Responses.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);
	if (const AActor* Target = TargetActor.Get())
	{
		QueryParams.AddIgnoredActor(Target);
	}

	// 1. 경로를 촘촘히 나눠 각 지점의 바닥과 천장으로 캡슐 중심이 있을 수 있는 높이 범위를 만든다
	const float Spacing = FMath::Max(ClearanceSampleSpacing, 50.0f);
	for (int32 i = 0; i + 1 < NavPathFloorPoints.Num(); ++i)
	{
		const FVector& From = NavPathFloorPoints[i];
		const FVector& To = NavPathFloorPoints[i + 1];
		const int32 Steps = FMath::Max(FMath::CeilToInt(FVector::Dist(From, To) / Spacing), 1);

		// 구간의 끝점은 다음 구간의 첫 점과 같아서 마지막 구간에서만 넣는다
		const int32 LastStep = (i + 2 == NavPathFloorPoints.Num()) ? Steps : Steps - 1;
		for (int32 Step = 0; Step <= LastStep; ++Step)
		{
			FVector Floor = FMath::Lerp(From, To, static_cast<float>(Step) / Steps);
			// NavMesh는 계단 표면을 단순화한다. 주변 실제 바닥을 먼저 찾아 계단 턱을 천장으로 오인하지 않게 한다.
			const float FloorProbe = FMath::Max(MaxStepHeight + FloorClearanceMargin, 50.0f);
			FHitResult FloorHit;
			if (World->LineTraceSingleByChannel(FloorHit, Floor + FVector::UpVector * FloorProbe,
				Floor - FVector::UpVector * FloorProbe, ObstacleTraceChannel, QueryParams, Responses)
				&& !FloorHit.bStartPenetrating && FloorHit.ImpactNormal.Z >= GetWalkableFloorZ())
			{
				Floor.Z = FloorHit.ImpactPoint.Z;
			}

			// 바닥 바로 위에서 위로 쏜다. 시작점이 물체 속이거나 다른 폰에 맞은 결과는 천장이 아니라서 버린다
			const FVector Start = Floor + FVector(0.0, 0.0, Radius + FloorClearanceMargin);
			FHitResult Hit;
			const bool bHitCeiling = World->SweepSingleByChannel(
					Hit, Start, Start + FVector::UpVector * CeilingTraceDistance, FQuat::Identity,
					ObstacleTraceChannel, FCollisionShape::MakeSphere(Radius), QueryParams, Responses)
				&& !Hit.bStartPenetrating && Hit.ImpactNormal.Z < -0.1f;
			const float CeilingZ = bHitCeiling ? Hit.Location.Z + Radius : Start.Z + CeilingTraceDistance;

			FPathClearanceSample& Sample = ClearanceSamples.AddDefaulted_GetRef();
			Sample.Location = Floor;
			Sample.MinCenterZ = Floor.Z + HalfHeight + FloorClearanceMargin;
			Sample.MaxCenterZ = CeilingZ - HalfHeight - CeilingClearance;
		}
	}

	// 2. 위아래로 몸이 들어가는가
	for (int32 Index = 0; Index < ClearanceSamples.Num(); ++Index)
	{
		const FPathClearanceSample& Sample = ClearanceSamples[Index];
		if (Sample.MaxCenterZ < Sample.MinCenterZ)
		{
			bPathBodyPassable = false;
			if (World->GetTimeSeconds() >= NextClearanceLogTime)
			{
				NextClearanceLogTime = World->GetTimeSeconds() + 2.0;
				UE_LOG(LogTemp, Warning,
					TEXT("[FlyPath] 통과 불가(높이) | Pawn=%s | 경로 지점 %d/%d | 열린 높이 %.0fcm < 필요 %.0fcm(캡슐 %.0fcm + 여유)"),
					*GetNameSafe(Character), Index, ClearanceSamples.Num(),
					Sample.MaxCenterZ - Sample.MinCenterZ + 2.0f * HalfHeight + FloorClearanceMargin + CeilingClearance,
					2.0f * HalfHeight + FloorClearanceMargin + CeilingClearance,
					2.0f * HalfHeight);
			}
			return;
		}
	}

	/*
	 * 3. 좌우 폭: 인접한 지점 사이를 캡슐로 쓸어 벽이나 문틀에 걸리는지 본다.
	 * 한 높이에서 걸렸다고 포기하지 않는다. 두 지점이 함께 허용하는 높이 범위에서 여러 높이로 쓸어 보고,
	 * 지나갈 수 있는 높이를 찾으면 그 구간의 허용 높이를 그 높이로 좁힌다. 바닥의 낮은 장식은 위로, 낮은 문틀은 아래로 피하게 된다
	 */
	const int32 SampleCount = ClearanceSamples.Num();
	const float CapsuleScale = bUseFullCapsule ? 1.0f : FMath::Clamp(ClearanceCapsuleScale, 0.3f, 1.0f);
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius * CapsuleScale, HalfHeight * CapsuleScale);
	const int32 Levels = FMath::Max(ClearanceHeightLevels, 2);

	// 지점마다 지나갈 수 있는 높이의 교집합. 인접한 두 구간이 서로 다른 높이만 허용하면 그 지점은 지나갈 수 없다
	TArray<float> PassMin;
	TArray<float> PassMax;
	PassMin.Init(-TNumericLimits<float>::Max(), SampleCount);
	PassMax.Init(TNumericLimits<float>::Max(), SampleCount);

	for (int32 Index = 0; Index + 1 < SampleCount; ++Index)
	{
		const FPathClearanceSample& A = ClearanceSamples[Index];
		const FPathClearanceSample& B = ClearanceSamples[Index + 1];

		// 두 지점 모두에서 캡슐 중심이 있을 수 있는 높이. 서로 안 겹치면(바닥이 급히 오르는데 천장이 낮은 곳)이 구간은 지나갈 수 없다
		const float Low = FMath::Max(A.MinCenterZ, B.MinCenterZ);
		const float High = FMath::Min(A.MaxCenterZ, B.MaxCenterZ);
		const float SearchTop = FMath::Min(High, Low + ClearanceSearchSpan);

		int32 LowestPass = INDEX_NONE;
		int32 HighestPass = INDEX_NONE;
		float LowestPassZ = 0.0f;
		float HighestPassZ = 0.0f;
		FHitResult LastBlockingHit;
		for (int32 Level = 0; High >= Low && Level < Levels; ++Level)
		{
			const float Z = FMath::Lerp(Low, SearchTop, static_cast<float>(Level) / (Levels - 1));
			FHitResult Hit;
			const bool bBlocked = World->SweepSingleByChannel(
					Hit, FVector(A.Location.X, A.Location.Y, Z), FVector(B.Location.X, B.Location.Y, Z),
					FQuat::Identity, ObstacleTraceChannel, Shape, QueryParams, Responses);
			if (bBlocked)
			{
				LastBlockingHit = Hit;
				continue;
			}

			if (LowestPass == INDEX_NONE)
			{
				LowestPass = Level;
				LowestPassZ = Z;
			}
			HighestPass = Level;
			HighestPassZ = Z;
		}

		if (LowestPass == INDEX_NONE)
		{
			bPathBodyPassable = false;
			if (World->GetTimeSeconds() >= NextClearanceLogTime)
			{
				NextClearanceLogTime = World->GetTimeSeconds() + 2.0;
				if (High < Low)
				{
					UE_LOG(LogTemp, Warning,
						TEXT("[FlyPath] 통과 불가(높이) | Pawn=%s | 구간 %d->%d / %d | 두 지점이 함께 허용하는 높이가 없다(바닥 %.0f 이상, 천장 %.0f 이하)"),
						*GetNameSafe(Character), Index, Index + 1, SampleCount, Low, High);
				}
				else
				{
					UE_LOG(LogTemp, Warning,
						TEXT("[FlyPath] 통과 불가(폭) | Pawn=%s | 구간 %d->%d / %d | 캡슐 중심 높이 %.0f~%.0fcm 사이 %d단계 모두 막힘 | 막은 것=%s(%s) 위치=%s"),
						*GetNameSafe(Character), Index, Index + 1, SampleCount, Low, SearchTop, Levels,
						*GetNameSafe(LastBlockingHit.GetActor()), *GetNameSafe(LastBlockingHit.GetComponent()),
						*LastBlockingHit.ImpactPoint.ToCompactString());
				}
			}
			return;
		}

		// 가장 낮은 높이와 가장 높은 높이가 모두 지나가면 그쪽으로는 열려 있다고 보고 원래 범위를 유지한다
		const float NewMin = LowestPass == 0 ? Low : LowestPassZ;
		const float NewMax = HighestPass == Levels - 1 ? High : HighestPassZ;
		PassMin[Index] = FMath::Max(PassMin[Index], NewMin);
		PassMin[Index + 1] = FMath::Max(PassMin[Index + 1], NewMin);
		PassMax[Index] = FMath::Min(PassMax[Index], NewMax);
		PassMax[Index + 1] = FMath::Min(PassMax[Index + 1], NewMax);
	}

	for (int32 Index = 0; Index < SampleCount; ++Index)
	{
		FPathClearanceSample& Sample = ClearanceSamples[Index];
		Sample.MinCenterZ = FMath::Max(Sample.MinCenterZ, PassMin[Index]);
		Sample.MaxCenterZ = FMath::Min(Sample.MaxCenterZ, PassMax[Index]);
		if (Sample.MaxCenterZ < Sample.MinCenterZ)
		{
			bPathBodyPassable = false;
			if (World->GetTimeSeconds() >= NextClearanceLogTime)
			{
				NextClearanceLogTime = World->GetTimeSeconds() + 2.0;
				UE_LOG(LogTemp, Warning,
					TEXT("[FlyPath] 통과 불가(높이가 이어지지 않음) | Pawn=%s | 경로 지점 %d/%d | 앞 구간과 뒤 구간이 지나갈 수 있는 높이가 서로 다르다"),
					*GetNameSafe(Character), Index, SampleCount);
			}
			return;
		}
	}
}

bool UFlyingMonsterMovementComponent::GetPathHeightBand(float& OutMinCenterZ, float& OutMaxCenterZ)
{
	const ACharacter* Character = GetCharacterOwner();
	if (!Character || !bHasLocationPathQuery || !bPathBodyPassable || ClearanceSamples.IsEmpty())
	{
		return false;
	}

	const FVector Location = Character->GetActorLocation();

	// 지금 몹이 있는 지점을 경로를 따라 앞으로만 옮겨 가며 잡는다. 경로를 다시 구하면 처음부터 다시 잡는다
	while (ClearanceSampleIndex + 1 < ClearanceSamples.Num()
		&& FVector::DistSquared2D(Location, ClearanceSamples[ClearanceSampleIndex + 1].Location)
			<= FVector::DistSquared2D(Location, ClearanceSamples[ClearanceSampleIndex].Location))
	{
		++ClearanceSampleIndex;
	}

	// 속도에 비례해 앞을 본다. 낮은 문틀은 닿기 전에 내려가기 시작해야 한다
	const float LookAheadDistance = FMath::Max(CeilingLookAheadDistance,
		Character->GetVelocity().Size2D() * ClearanceLookAheadTime);
	const float LookAheadSq = FMath::Square(LookAheadDistance);

	// 지나가는 중인 낮은 구간에서 벗어나기 전에 다시 올라가지 않도록 바로 뒤 지점도 함께 본다
	float BandMin = -TNumericLimits<float>::Max();
	float BandMax = TNumericLimits<float>::Max();
	for (int32 Index = FMath::Max(ClearanceSampleIndex - 1, 0); Index < ClearanceSamples.Num(); ++Index)
	{
		const FPathClearanceSample& Sample = ClearanceSamples[Index];
		if (Index > ClearanceSampleIndex && FVector::DistSquared2D(Location, Sample.Location) > LookAheadSq)
		{
			break;
		}
		BandMin = FMath::Max(BandMin, Sample.MinCenterZ);
		BandMax = FMath::Min(BandMax, Sample.MaxCenterZ);
	}

	// 바닥이 오르는 구간과 천장이 낮은 구간이 겹쳐 범위가 뒤집히면 천장에 박히지 않는 쪽을 택한다
	OutMaxCenterZ = BandMax;
	OutMinCenterZ = FMath::Min(BandMin, BandMax);
	return true;
}

void UFlyingMonsterMovementComponent::DrawChaseDebug(const AActor* Target) const
{
#if ENABLE_DRAW_DEBUG
	const UWorld* World = GetWorld();
	const ACharacter* Character = GetCharacterOwner();
	if (!bDebugDrawChasePath || !World || !Character || !IsValid(Target))
	{
		return;
	}

	const FVector From = Character->GetActorLocation();
	const FVector To = Target->GetActorLocation();

	// 노랑은 열린 직선, 주황은 막힌 직선과 경로 탐색 대기를 표시한다.
	if (!bHasLocationPathQuery || LocationPathPoints.IsEmpty())
	{
		DrawDebugLine(World, From, To, bStraightPathBlocked ? FColor::Orange : FColor::Yellow, false, 0.0f, 0, 3.0f);
		DrawDebugSphere(World, To, 20.0f, 8, bStraightPathBlocked ? FColor::Orange : FColor::Yellow, false, 0.0f, 0, 2.0f);
		return;
	}

	// 빨강은 통과 검사에 실패한 경로다. 실제 이동은 재탐색을 기다린다.
	const FColor PathColor = bPathBodyPassable ? FColor::Green : FColor::Red;
	if (!bPathBodyPassable)
	{
		DrawDebugLine(World, From, To, FColor::Yellow, false, 0.0f, 0, 2.0f);
	}

	// 몹 -> 아직 지나지 않은 경로 지점들 -> 목적지. 점은 지점, 선은 이동 방향이다
	FVector Previous = From;
	for (int32 Index = FMath::Clamp(LocationPathIndex, 0, LocationPathPoints.Num() - 1); Index < LocationPathPoints.Num(); ++Index)
	{
		const FVector& Point = LocationPathPoints[Index];
		DrawDebugLine(World, Previous, Point, PathColor, false, 0.0f, 0, 4.0f);
		DrawDebugSphere(World, Point, 25.0f, 8, PathColor, false, 0.0f, 0, 2.0f);
		Previous = Point;
	}

	// 경로 지점별로 캡슐 중심이 있을 수 있는 높이 범위. 위쪽이 열려 있으면 너무 길어지니 400cm까지만 그린다
	for (const FPathClearanceSample& Sample : ClearanceSamples)
	{
		const FVector Bottom(Sample.Location.X, Sample.Location.Y, Sample.MinCenterZ);
		const FVector Top(Sample.Location.X, Sample.Location.Y, FMath::Min(Sample.MaxCenterZ, Sample.MinCenterZ + 400.0f));
		DrawDebugLine(World, Bottom, Top, FColor::Cyan, false, 0.0f, 0, 1.5f);
	}

	// 지금 몹이 향하는 목표 부유 높이
	if (bHasDesiredFlightHeight)
	{
		DrawDebugPoint(World, FVector(From.X, From.Y, DesiredFlightHeight), 14.0f, FColor::Magenta, false, 0.0f, 0);
	}
#endif
}

bool UFlyingMonsterMovementComponent::IsStraightPathBlocked(const FVector& From, const FVector& To, const AActor* IgnoreActor)
{
	const UWorld* World = GetWorld();
	if (!bUseChasePath || !World)
	{
		return false;
	}

	const double Now = World->GetTimeSeconds();
	if (Now < NextStraightPathCheckTime)
	{
		return bStraightPathBlocked;
	}
	NextStraightPathCheckTime = Now + StraightPathCheckInterval;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(FlyChaseStraightPath), false, GetCharacterOwner());
	FCollisionResponseParams Responses;
	Responses.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);
	if (IgnoreActor)
	{
		QueryParams.AddIgnoredActor(IgnoreActor);
	}

	FHitResult Hit;
	const bool bLineBlocked = World->LineTraceSingleByChannel(Hit, From, To, ObstacleTraceChannel, QueryParams, Responses)
		&& !Cast<APawn>(Hit.GetActor());

	/*
	 * 라인 트레이스는 두께가 없어서 문 한가운데를 지나는 직선은 뚫려 있다고 나온다. 하지만 몸(캡슐)은 부유 높이에 있어서 문틀 위의 벽에 걸린다.
	 * 그래서 부유 높이에서 캡슐을 수평으로 쓸어 본다. 낮춰진 현재 높이가 아니라 제한 전 부유 높이로 재야,
	 * 문틀 아래로 내려간 동안 직선이 뚫린 것으로 보여 경로를 버리고 다시 올라가는 일이 없다
	 */
	bool bBodyBlocked = false;
	FHitResult SweepHit;
	const ACharacter* Character = GetCharacterOwner();
	const UCapsuleComponent* Capsule = IsValid(Character) ? Character->GetCapsuleComponent() : nullptr;
	if (bValidateBodyClearance && Capsule)
	{
		const float SweepZ = bHasPreferredHoverHeight ? PreferredHoverHeight : From.Z;
		const float Scale = FMath::Clamp(ClearanceCapsuleScale, 0.3f, 1.0f);
		const FCollisionShape Shape = FCollisionShape::MakeCapsule(
			Capsule->GetScaledCapsuleRadius() * Scale, Capsule->GetScaledCapsuleHalfHeight() * Scale);

		bBodyBlocked = World->SweepSingleByChannel(
				SweepHit, FVector(From.X, From.Y, SweepZ), FVector(To.X, To.Y, SweepZ),
				FQuat::Identity, ObstacleTraceChannel, Shape, QueryParams, Responses)
			&& !Cast<APawn>(SweepHit.GetActor());
	}

	const bool bBlockedNow = bLineBlocked || bBodyBlocked;
	bStraightPathBlocked = bBlockedNow;
	return bStraightPathBlocked;
}

FVector UFlyingMonsterMovementComponent::ResolveChaseDestination(const FVector& Destination, const AActor* IgnoreActor)
{
	const ACharacter* Character = GetCharacterOwner();
	const UWorld* World = GetWorld();
	bChaseWaitingForPath = false;
	if (Character && bPathBodyPassable && FlightJoinPoints.IsValidIndex(FlightJoinIndex))
	{
		if (FVector::DistSquared2D(Character->GetActorLocation(), FlightJoinPoints[FlightJoinIndex]) <= FMath::Square(20.0f)
			&& FMath::Abs(Character->GetActorLocation().Z - FlightJoinPoints[FlightJoinIndex].Z) <= FMath::Max(HeightTolerance, 20.0f))
		{
			++FlightJoinIndex;
		}
		if (FlightJoinPoints.IsValidIndex(FlightJoinIndex)) { return FlightJoinPoints[FlightJoinIndex]; }
		FlightJoinPoints.Reset();
		FlightJoinIndex = 0;
		NextStraightPathCheckTime = 0.0;
	}
	if (!Character || !World || !IsStraightPathBlocked(Character->GetActorLocation(), Destination, IgnoreActor))
	{
		// 직선이 뚫렸다. 남은 경로가 있어도 더 이상 필요 없다
		if (bHasLocationPathQuery)
		{
			ClearLocationPath();
		}
		return Destination;
	}

	// 경로 탐색은 동기 호출이라 간격을 둔다. 타깃이 조금 움직인 것만으로는 다시 구하지 않는다
	const double Now = World->GetTimeSeconds();
	const bool bTargetMoved = FVector::DistSquared(Destination, LocationPathDestination)
		> FMath::Square(LocationPathRebuildDistance);
	const bool bAtPathEnd = !LocationPathPoints.IsEmpty()
		&& FVector::DistSquared2D(Character->GetActorLocation(), LocationPathPoints.Last()) <= FMath::Square(10.0f);
	if (Now >= NextChasePathRebuildTime && (!bHasLocationPathQuery
		|| LocationPathPoints.IsEmpty() || !bPathBodyPassable || bTargetMoved
		|| bLocationPathPartial || bLastPathBuildFailed || bAtPathEnd))
	{
		const bool bHadPassablePath = bHasLocationPathQuery && bPathBodyPassable && !LocationPathPoints.IsEmpty();
		const auto PreviousJoin = FlightJoinPoints;
		const int32 PreviousJoinIndex = FlightJoinIndex;
		const auto PreviousPoints = LocationPathPoints;
		const auto PreviousFloors = NavPathFloorPoints;
		const auto PreviousClearance = ClearanceSamples;
		const int32 PreviousPointIndex = LocationPathIndex;
		const int32 PreviousClearanceIndex = ClearanceSampleIndex;
		const bool bPreviousPartial = bLocationPathPartial;
		const FVector PreviousDestination = LocationPathDestination;
		if (BuildLocationPath(Destination))
		{
			if (!bPathBodyPassable && bHadPassablePath)
			{
				FlightJoinPoints = PreviousJoin;
				FlightJoinIndex = PreviousJoinIndex;
				LocationPathPoints = PreviousPoints;
				NavPathFloorPoints = PreviousFloors;
				ClearanceSamples = PreviousClearance;
				LocationPathIndex = PreviousPointIndex;
				ClearanceSampleIndex = PreviousClearanceIndex;
				bLocationPathPartial = bPreviousPartial;
				LocationPathDestination = PreviousDestination;
				bPathBodyPassable = true;
				bLastPathBuildFailed = true;
			}
		}
		NextChasePathRebuildTime = Now + ChasePathRebuildInterval;
	}

	// 통과 검사 실패나 경로 탐색 실패를 벽 쪽 직선 이동으로 바꾸지 않는다.
	if (!bPathBodyPassable || LocationPathPoints.IsEmpty())
	{
		bChaseWaitingForPath = true;

		return Character->GetActorLocation();
	}

	if (FlightJoinPoints.IsValidIndex(FlightJoinIndex)) { return FlightJoinPoints[FlightJoinIndex]; }

	// 시야가 막힌 동안에는 저장한 경로 끝까지만 이동한다. 새 타깃 위치로 직진하지 않는다.
	if (!LocationPathPoints.IsValidIndex(LocationPathIndex) || LocationPathIndex >= LocationPathPoints.Num() - 1)
	{
		const FVector Endpoint = LocationPathPoints.Last();
		bChaseWaitingForPath = FVector::DistSquared2D(Character->GetActorLocation(), Endpoint) <= FMath::Square(10.0f);
		return Endpoint;
	}

	// 좁은 문에서 코너를 100cm 앞에서 넘기면 다음 선분이 벽을 가로지른다.
	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const float CornerAcceptance = Capsule
		? FMath::Min(LocationPathPointAcceptance, FMath::Max(10.0f, Capsule->GetScaledCapsuleRadius() * 0.5f))
		: LocationPathPointAcceptance;
	if (FVector::DistSquared2D(Character->GetActorLocation(), LocationPathPoints[LocationPathIndex])
		<= FMath::Square(CornerAcceptance))
	{
		++LocationPathIndex;
	}

	return LocationPathPoints[LocationPathIndex];
}

void UFlyingMonsterMovementComponent::ResetMovementState()
{
	NextFlightJoinSearchTime = 0.0;
	Super::ResetMovementState();
	TargetActor.Reset();
	bIsInStrafeMode = false;
	bHasStrafeDirection = false;
	bHasDesiredFlightHeight = false;
	bHasHoverOffset = false;
	bHasGroundHeight = false;
	bHasHoverAnchor = false;
	NextHoverOffsetChangeTime = 0.0;
	NextGroundTraceTime = 0.0;
	NextHoverAnchorRefreshTime = 0.0;
	SmoothedFlyDirection = FVector::ZeroVector;
	ClearLocationPath();
	bStraightPathBlocked = false;
	bHasPreferredHoverHeight = false;
	bChaseWaitingForPath = false;
	bChaseWaitingForHeight = false;
	NextStraightPathCheckTime = 0.0;
	NextChasePathRebuildTime = 0.0;
	bHasCeiling = false;
	LastCeilingHitTime = -1.0;
	NextCeilingTraceTime = 0.0;
}

void UFlyingMonsterMovementComponent::BeginAttackPositioning(
	AActor* InTargetActor, const float InAttackDistance, const float InHeightFromFloor, const float InAcceptanceRadius,
	const float InSpeedOverride, const float InAccelerationOverride)
{
	bAttackPositioningActive = true;
	AttackPositioningTarget = InTargetActor;
	AttackPositioningVelocity = FVector::ZeroVector;
	AttackPositioningDistance = FMath::Max(InAttackDistance, 0.0f);
	AttackPositioningHeightFromFloor = FMath::Max(InHeightFromFloor, 0.0f);
	AttackPositioningAcceptanceRadius = FMath::Max(InAcceptanceRadius, 0.0f);
	AttackPositioningMaxSpeed = InSpeedOverride > 0.0f ? InSpeedOverride : MaxFlySpeed;
	AttackPositioningAcceleration = InAccelerationOverride > 0.0f ? InAccelerationOverride : MaxAcceleration;
	SetMovementMode(MOVE_Flying);
}

void UFlyingMonsterMovementComponent::TickAttackPositioning(AActor* InTargetActor)
{
	if (bAttackPositioningActive)
	{
		AttackPositioningTarget = InTargetActor;
	}
}

void UFlyingMonsterMovementComponent::EndAttackPositioning()
{
	bAttackPositioningActive = false;
	AttackPositioningTarget.Reset();
	AttackPositioningVelocity = FVector::ZeroVector;
	AttackPositioningMaxSpeed = 0.0f;
	AttackPositioningAcceleration = 0.0f;
}

void UFlyingMonsterMovementComponent::ApplyAttackPositioningMovement(const float DeltaSeconds)
{
	if (!bAttackPositioningActive || DeltaSeconds <= 0.0f || !HasValidData() || !IsValid(UpdatedComponent))
	{
		return;
	}

	const FVector Location = UpdatedComponent->GetComponentLocation();
	FVector DesiredDirection = FVector::ZeroVector;
	float HorizontalDistance = -1.0f;
	float RemainingHorizontalDistance = 0.0f;
	if (AActor* Target = AttackPositioningTarget.Get())
	{
		FVector ObservedLocation;
		const AMonsterAIController* Controller = Cast<AMonsterAIController>(GetCharacterOwner()->GetController());
		if (Controller && !Controller->GetPursuitTargetLocation(Target, ObservedLocation))
		{
			AttackPositioningVelocity = FVector::ZeroVector;
			return;
		}
		if (!Controller) { ObservedLocation = Target->GetActorLocation(); }
		FVector ToTarget = ObservedLocation - Location;
		ToTarget.Z = 0.0f;
		HorizontalDistance = ToTarget.Size();
		RemainingHorizontalDistance = HorizontalDistance - (AttackPositioningDistance + AttackPositioningAcceptanceRadius);
		if (RemainingHorizontalDistance > 0.0f)
		{
			FCollisionQueryParams Params(SCENE_QUERY_STAT(AttackPositioningRoute), false, GetCharacterOwner());
			Params.AddIgnoredActor(Target);
			FCollisionResponseParams Responses;
			Responses.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);
			FHitResult Obstacle;
			const UCapsuleComponent* Capsule = GetCharacterOwner()->GetCapsuleComponent();
			if (!GetWorld()->SweepSingleByChannel(Obstacle, Location, FVector(ObservedLocation.X, ObservedLocation.Y, Location.Z), FQuat::Identity,
				ObstacleTraceChannel, FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),
				Capsule->GetScaledCapsuleHalfHeight()), Params, Responses))
			{
				DesiredDirection = ToTarget.GetSafeNormal();
			}
		}
	}

	const float GroundZ = GetGroundZForAttackPositioning();
	SmoothedGroundHeight = GroundZ;
	bHasGroundHeight = true;
	RefreshCeiling(DesiredDirection);
	const float DesiredZ = LimitHeightByCeiling(GroundZ + AttackPositioningHeightFromFloor);
	const float BodyHalfHeight = GetCharacterOwner()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	if (bHasCeiling && CachedCeilingZ - BodyHalfHeight - CeilingClearance
		< GroundZ + BodyHalfHeight + FloorClearanceMargin)
	{
		AttackPositioningVelocity = FVector::ZeroVector;
		return;
	}
	const float VerticalDelta = DesiredZ - Location.Z;
	// 낮은 천장에서는 공격 준비 중에도 먼저 하강한다.
	if (bHasCeiling && Location.Z > LimitHeightByCeiling(TNumericLimits<float>::Max()) + HeightTolerance)
	{
		DesiredDirection.X = 0.0f;
		DesiredDirection.Y = 0.0f;
		AttackPositioningVelocity.X = 0.0f;
		AttackPositioningVelocity.Y = 0.0f;
	}
	if (FMath::Abs(VerticalDelta) > HeightTolerance)
	{
		DesiredDirection.Z = FMath::Sign(VerticalDelta);
	}

	const FVector TargetVelocity = DesiredDirection.GetSafeNormal() * AttackPositioningMaxSpeed;
	AttackPositioningVelocity = FMath::VInterpConstantTo(
		AttackPositioningVelocity, TargetVelocity, DeltaSeconds, AttackPositioningAcceleration);

	FVector DeltaLocation = AttackPositioningVelocity * DeltaSeconds;
	if (RemainingHorizontalDistance <= 0.0f)
	{
		DeltaLocation.X = 0.0f;
		DeltaLocation.Y = 0.0f;
	}
	else
	{
		const float HorizontalStep = DeltaLocation.Size2D();
		if (HorizontalStep > RemainingHorizontalDistance)
		{
			DeltaLocation = DeltaLocation.GetSafeNormal2D() * RemainingHorizontalDistance + FVector::UpVector * DeltaLocation.Z;
		}
	}

	if (FMath::Abs(VerticalDelta) <= HeightTolerance)
	{
		DeltaLocation.Z = 0.0f;
	}
	else if (FMath::Abs(DeltaLocation.Z) > FMath::Abs(VerticalDelta))
	{
		DeltaLocation.Z = VerticalDelta;
	}

	if (!DeltaLocation.IsNearlyZero())
	{
		FHitResult Hit;
		SafeMoveUpdatedComponent(DeltaLocation, UpdatedComponent->GetComponentQuat(), true, Hit);
		if (Hit.IsValidBlockingHit())
		{
			SlideAlongSurface(DeltaLocation, 1.0f - Hit.Time, Hit.Normal, Hit, true);
		}
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[EnemyAttackPositioning] SafeMove | Owner=%s Target=%s CurrentZ=%.1f DesiredZ=%.1f TargetDistance=%.1f Delta=%s CustomVelocity=%s"),
		*GetNameSafe(GetCharacterOwner()), *GetNameSafe(AttackPositioningTarget.Get()), Location.Z, DesiredZ,
		HorizontalDistance, *DeltaLocation.ToCompactString(), *AttackPositioningVelocity.ToCompactString());
}

float UFlyingMonsterMovementComponent::GetGroundZForAttackPositioning() const
{
	const ACharacter* Character = GetCharacterOwner();
	UWorld* World = GetWorld();
	if (!IsValid(Character) || !IsValid(World))
	{
		return 0.0f;
	}

	const FVector Start = Character->GetActorLocation();
	const FVector End = Start - FVector::UpVector * GroundTraceDistance;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AttackPositioningGroundTrace), false, Character);
	FHitResult Hit;
	return World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, QueryParams)
		? Hit.ImpactPoint.Z
		: Start.Z;
}

void UFlyingMonsterMovementComponent::UpdateDesiredFlightHeight(const float DeltaSeconds)
{
	bChaseWaitingForHeight = false;
	ACharacter* Character = GetCharacterOwner();
	AActor* Target = TargetActor.Get();
	UWorld* World = GetWorld();
	if (!Character || !World)
	{
		return;
	}

	if (FlightJoinPoints.IsValidIndex(FlightJoinIndex))
	{
		DesiredFlightHeight = FlightJoinPoints[FlightJoinIndex].Z;
		bHasDesiredFlightHeight = true;
		bChaseWaitingForHeight = FVector::DistSquared2D(Character->GetActorLocation(), FlightJoinPoints[FlightJoinIndex])
			<= FMath::Square(20.0f) && FMath::Abs(DesiredFlightHeight - Character->GetActorLocation().Z) > HeightTolerance;
		return;
	}
	const double Time = World->GetTimeSeconds();
	if (!bHasHoverOffset || Time >= NextHoverOffsetChangeTime)
	{
		CurrentHoverOffset = FMath::FRandRange(RandomHeightOffsetMin, RandomHeightOffsetMax);
		bHasHoverOffset = true;
		NextHoverOffsetChangeTime = Time + HoverTargetChangeInterval;
	}

	if (!bHasGroundHeight || Time >= NextGroundTraceTime)
	{
		const FVector Start = Character->GetActorLocation();
		const FVector End = Start - FVector::UpVector * GroundTraceDistance;
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MonsterFlightGroundTrace), false, Character);
		if (Target) { QueryParams.AddIgnoredActor(Target); }
		FHitResult Hit;
		const float GroundHeight = World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, QueryParams)
			? Hit.ImpactPoint.Z
			: (bHasGroundHeight ? SmoothedGroundHeight : Start.Z - BaseHoverHeight);
		SmoothedGroundHeight = bHasGroundHeight
			? FMath::FInterpTo(SmoothedGroundHeight, GroundHeight, DeltaSeconds, FlightHeightInterpSpeed)
			: GroundHeight;
		bHasGroundHeight = true;
		NextGroundTraceTime = Time + 0.1;
	}

	if (bHasLocationPathQuery && bPathBodyPassable && ClearanceSamples.IsValidIndex(ClearanceSampleIndex))
	{
		SmoothedGroundHeight = ClearanceSamples[ClearanceSampleIndex].Location.Z;
		bHasGroundHeight = true;
	}
	const float RawHeight = SmoothedGroundHeight + BaseHoverHeight + CurrentHoverOffset;
	const float MinimumHeight = SmoothedGroundHeight + MinimumGroundClearance;
	const float MaximumHeight = SmoothedGroundHeight + FMath::Max(MinimumGroundClearance, MaximumGroundClearance);
	const float BaseHeight = FMath::Clamp(RawHeight, MinimumHeight, MaximumHeight);

	// 제한을 걸기 전의 부유 높이. 직선이 몸으로 지나갈 수 있는지 잴 때 쓴다
	PreferredHoverHeight = BaseHeight;
	bHasPreferredHoverHeight = true;

	float LimitedHeight = BaseHeight;
	bool bLimitedByPath = false;
	float PathBandMin = 0.0f;
	float PathBandMax = 0.0f;
	if (GetPathHeightBand(PathBandMin, PathBandMax))
	{
		// 벽을 돌아 경로를 따라가는 중이면 경로 지점의 바닥과 천장이 정한 범위 안에 둔다. 낮은 문틀은 닿기 전에 내려가고 계단은 미리 올라간다
		// 경계 바로 위에서 HeightTolerance 때문에 하강이 멈추지 않도록 목표를 범위 안쪽에 둔다.
		const float EntryInset = FMath::Min(HeightTolerance, (PathBandMax - PathBandMin) * 0.25f);
		const float PreferredMax = FMath::Max(PathBandMin + EntryInset,
			PathBandMax - FMath::Max(ComfortableCeilingClearance - CeilingClearance, 0.0f));
		LimitedHeight = FMath::Clamp(BaseHeight, PathBandMin + EntryInset,
			FMath::Min(PathBandMax - EntryInset, PreferredMax));
		bLimitedByPath = LimitedHeight < BaseHeight - KINDA_SMALL_NUMBER;
		// 뒤따르는 수평 이동보다 먼저 높이를 맞춘다. Z 속도는 유지해 하강/상승을 막지 않는다.
		const float EntryTolerance = FMath::Min(HeightTolerance, FMath::Min(FloorClearanceMargin, CeilingClearance) * 0.5f);
		bChaseWaitingForHeight = Character->GetActorLocation().Z > PathBandMax + EntryTolerance
			|| Character->GetActorLocation().Z < PathBandMin - EntryTolerance;

	}
	else
	{
		// 경로가 없으면 지면 기준으로만 자르면 천장이 낮은 곳에서 천장에 박히므로 천장 아래로 한 번 더 자른다.
		// 앞쪽 천장은 몹이 실제로 향하는 방향으로 잰다
		const FVector CeilingHeading = SmoothedFlyDirection.IsNearlyZero()
			? ChaseMovementDestination - Character->GetActorLocation()
			: SmoothedFlyDirection;
		RefreshCeiling(CeilingHeading);
		LimitedHeight = LimitHeightByCeiling(BaseHeight);
		const float HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		if (bHasCeiling && CachedCeilingZ - HalfHeight - CeilingClearance
			< SmoothedGroundHeight + HalfHeight + FloorClearanceMargin)
		{
			bChaseWaitingForPath = true;
		}
		bLimitedByPath = LimitedHeight < BaseHeight - KINDA_SMALL_NUMBER;
		bChaseWaitingForHeight = bHasCeiling
			&& Character->GetActorLocation().Z > LimitHeightByCeiling(TNumericLimits<float>::Max()) + HeightTolerance;
	}

	// 낮은 구간에 맞춰 내려갈 때는 닿기 전에 내려가 있도록 평소보다 빠르게 따라간다
	const float InterpSpeed = FlightHeightInterpSpeed * (bLimitedByPath ? ClearanceDescentInterpMultiplier : 1.0f);
	DesiredFlightHeight = bHasDesiredFlightHeight
		? FMath::FInterpTo(DesiredFlightHeight, LimitedHeight, DeltaSeconds, InterpSpeed)
		: LimitedHeight;
	bHasDesiredFlightHeight = true;
}

void UFlyingMonsterMovementComponent::ApplyVerticalMovement() const
{
	const ACharacter* Character = GetCharacterOwner();
	if (!Character || !bHasDesiredFlightHeight)
	{
		return;
	}

	const float HeightError = DesiredFlightHeight - Character->GetActorLocation().Z;
	if (FMath::Abs(HeightError) <= HeightTolerance && !bChaseWaitingForHeight)
	{
		return;
	}

	const float InputScale = FMath::Clamp(
		HeightError / FMath::Max(VerticalCorrectionRange, 1.0f),
		-MaxVerticalMovementInputScale,
		MaxVerticalMovementInputScale);
	const_cast<ACharacter*>(Character)->AddMovementInput(FVector::UpVector, InputScale);
}

void UFlyingMonsterMovementComponent::PerformStrafeMovement()
{
	ACharacter* Character = GetCharacterOwner();
	AActor* Target = TargetActor.Get();
	if (!Character || !Target)
	{
		return;
	}

	if (!bHasStrafeDirection)
	{
		StrafeDirection = FMath::RandBool() ? 1.0f : -1.0f;
		bHasStrafeDirection = true;
	}

	FVector ToTarget = Target->GetActorLocation() - Character->GetActorLocation();
	ToTarget.Z = 0.0f;
	const float Distance = ToTarget.Size();
	const FVector RadialDirection = ToTarget.GetSafeNormal();
	const FVector TangentDirection = FVector::CrossProduct(FVector::UpVector, RadialDirection) * StrafeDirection;
	const float RadialError = (Distance - MovementRangeThreshold) / FMath::Max(MovementRangeThreshold, 1.0f);
	const FVector MoveDirection = (TangentDirection + RadialDirection * RadialError * StrafeRadialCorrectionSpeed).GetSafeNormal();
	Character->AddMovementInput(MoveDirection, FlyMovementInputScale);
	ApplyVerticalMovement();
}

void UFlyingMonsterMovementComponent::UpdateDynamicHoverAnchor()
{
	if (FlightApproachMode != EMonsterFlightApproachMode::DynamicHover)
	{
		return;
	}

	ACharacter* Character = GetCharacterOwner();
	AActor* Target = TargetActor.Get();
	UWorld* World = GetWorld();
	if (!Character || !Target || !World)
	{
		return;
	}

	const FVector TargetLocation = Target->GetActorLocation();
	const FVector NewDirection = (TargetLocation - Character->GetActorLocation()).GetSafeNormal2D();
	const FVector CurrentDirection = SmoothedFlyDirection.GetSafeNormal2D();
	const bool bIntervalElapsed = World->GetTimeSeconds() >= NextHoverAnchorRefreshTime;
	const bool bTargetMovedEnough = !bHasHoverAnchor || FVector::Dist2D(TargetLocation, HoverAnchorTargetLocation) >= HoverAnchorTargetMoveThreshold;
	const bool bDirectionChangedEnough = !CurrentDirection.IsNearlyZero()
		&& FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(CurrentDirection, NewDirection), -1.0f, 1.0f))) >= HoverAnchorRefreshAngle;

	if (!bHasHoverAnchor || bIntervalElapsed || bTargetMovedEnough || bDirectionChangedEnough)
	{
		HoverAnchorLocation = TargetLocation;
		HoverAnchorTargetLocation = TargetLocation;
		bHasHoverAnchor = true;
		NextHoverAnchorRefreshTime = World->GetTimeSeconds() + HoverAnchorRefreshInterval;
	}
}

FVector UFlyingMonsterMovementComponent::GetFlyDestination() const
{
	if (FlightApproachMode == EMonsterFlightApproachMode::DynamicHover &&
		bHasHoverAnchor)
	{
		return HoverAnchorLocation;
	}
	return TargetActor.IsValid() ? TargetActor->GetActorLocation() : FVector::ZeroVector;
}

void UFlyingMonsterMovementComponent::PerformFlyMovement(const float DeltaSeconds)
{
	ACharacter* Character = GetCharacterOwner();
	if (!Character)
	{
		return;
	}

	// 직선이 막혀 있으면 바닥 경로를 따라 벽을 돌아간다. 높이는 아래 ApplyVerticalMovement가 그대로 맞춘다
	if (bChaseWaitingForHeight)
	{
		Velocity.X = 0.0f;
		Velocity.Y = 0.0f;
		SmoothedFlyDirection = FVector::ZeroVector;
		ApplyVerticalMovement();
		return;
	}
	FVector DesiredDirection = ChaseMovementDestination - Character->GetActorLocation();
	DesiredDirection.Z = 0.0f;
	DesiredDirection = DesiredDirection.GetSafeNormal();
	SmoothedFlyDirection = bHasLocationPathQuery || SmoothedFlyDirection.IsNearlyZero()
		? DesiredDirection
		: FMath::VInterpNormalRotationTo(SmoothedFlyDirection, DesiredDirection, DeltaSeconds, FlyTurnInterpSpeed);
	const float HeightError = FMath::Abs(DesiredFlightHeight - Character->GetActorLocation().Z);
	const float HeightSpeedScale = FMath::Clamp(1.0f - HeightError / FMath::Max(VerticalCorrectionRange, 1.0f), 0.2f, 1.0f);
	Character->AddMovementInput(SmoothedFlyDirection, FlyMovementInputScale * HeightSpeedScale);
	ApplyVerticalMovement();

#if ENABLE_DRAW_DEBUG
	if (bDebugDrawFlightDirection && GetWorld())
	{
		const FVector Start = Character->GetActorLocation();
		DrawDebugDirectionalArrow(GetWorld(), Start, Start + SmoothedFlyDirection * DebugFlightDirectionLength, 50.0f, FColor::Cyan, false, 0.0f, 0, 3.0f);
	}
#endif
}

void UFlyingMonsterMovementComponent::RotateTowardTarget(const float DeltaSeconds) const
{
	ACharacter* Character = GetCharacterOwner();
	AActor* Target = TargetActor.Get();
	if (!Character || !Target)
	{
		return;
	}

	FVector Direction = Target->GetActorLocation() - Character->GetActorLocation();
	Direction.Z = 0.0f;
	if (Direction.IsNearlyZero())
	{
		return;
	}
	const FRotator DesiredRotation = Direction.Rotation();
	Character->SetActorRotation(FMath::RInterpTo(Character->GetActorRotation(), DesiredRotation, DeltaSeconds, FacingInterpSpeed));
}

void UFlyingMonsterMovementComponent::MoveTowardLocation(const FVector& Destination, const float DeltaSeconds)
{
	ACharacter* Character = GetCharacterOwner();
	if (!Character)
	{
		return;
	}
	const FVector Direction = (Destination - Character->GetActorLocation()).GetSafeNormal();
	Character->AddMovementInput(Direction, FlyMovementInputScale);
	if (!Direction.IsNearlyZero())
	{
		const FRotator DesiredRotation = Direction.Rotation();
		Character->SetActorRotation(FMath::RInterpTo(Character->GetActorRotation(), DesiredRotation, DeltaSeconds, FacingInterpSpeed));
	}
}

float UFlyingMonsterMovementComponent::GetHeightFromFloor() const
{
	const ACharacter* Character = GetCharacterOwner();
	return IsValid(Character) ? Character->GetActorLocation().Z - GetPatternFloorZ() : 0.0f;
}

bool UFlyingMonsterMovementComponent::ResolvePatternMovementContext(const FEnemyPatternMovementOption& MovementOption)
{
	const ACharacter* Character = GetCharacterOwner();
	if (!IsValid(Character))
	{
		return false;
	}

	const float FloorZ = GetPatternFloorZ();
	switch (MovementOption.Intent)
	{
	case EEnemyMovementIntent::MatchHeight:
		PatternDestination = Character->GetActorLocation();
		PatternTargetWorldZ = FloorZ + ResolvePatternHeightFromFloor(MovementOption);
		return true;
	case EEnemyMovementIntent::DescendToFloor:
		PatternDestination = Character->GetActorLocation();
		PatternTargetWorldZ = FloorZ;
		return true;
	default:
		if (!Super::ResolvePatternMovementContext(MovementOption))
		{
			return false;
		}

		// 접근과 앵커 이동도 같은 높이 기준을 봐야 이동을 오갈 때 목표 Z가 어긋나지 않는다.
		PatternTargetWorldZ = FloorZ + ResolvePatternHeightFromFloor(MovementOption);
		return true;
	}
}

float UFlyingMonsterMovementComponent::ResolvePatternHeightFromFloor(const FEnemyPatternMovementOption& MovementOption) const
{
	if (MovementOption.HeightFromFloor >= 0.0f)
	{
		return MovementOption.HeightFromFloor;
	}

	if (const UEnemyMovementProfileDataAsset* Profile = GetActiveMovementProfile())
	{
		return Profile->Flying.DefaultHeightFromFloor;
	}

	// 프로필이 없는 레거시 몬스터는 지금 높이를 유지한다.
	return GetHeightFromFloor();
}

float UFlyingMonsterMovementComponent::GetPatternFloorZ() const
{
	const ACharacter* Character = GetCharacterOwner();
	const UCapsuleComponent* Capsule = IsValid(Character) ? Character->GetCapsuleComponent() : nullptr;
	const float HalfHeight = IsValid(Capsule) ? Capsule->GetScaledCapsuleHalfHeight() : 0.0f;
	return GetGroundZForAttackPositioning() + HalfHeight;
}

FVector UFlyingMonsterMovementComponent::GetPatternMovementDirection() const
{
	const ACharacter* Character = GetCharacterOwner();
	if (!IsValid(Character))
	{
		return FVector::ZeroVector;
	}

	const FVector Location = Character->GetActorLocation();
	if (ActivePatternOption.Intent == EEnemyMovementIntent::ApproachTarget)
	{
		// 수평은 타깃을 향해 곧장, 높이는 오차에 비례해 서서히 맞춘다.
		FVector Direction = Super::GetPatternMovementDirection();
		Direction.Z = FMath::Clamp((PatternTargetWorldZ - Location.Z) / FMath::Max(VerticalCorrectionRange, 1.0f), -1.0f, 1.0f);
		return Direction.GetClampedToMaxSize(1.0f);
	}

	FVector Direction = PatternDestination - Location;
	Direction.Z = PatternTargetWorldZ - Location.Z;
	return Direction.GetSafeNormal();
}

void UFlyingMonsterMovementComponent::RefreshCeiling(const FVector& Heading)
{
	const UWorld* World = GetWorld();
	const ACharacter* Character = GetCharacterOwner();
	if (!bLimitHeightByCeiling || !World || !Character)
	{
		bHasCeiling = false;
		return;
	}
	const double Now = World->GetTimeSeconds();
	if (Now < NextCeilingTraceTime) { return; }
	NextCeilingTraceTime = Now + CeilingCheckInterval;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FlyCeilingTrace), false, Character);
	FCollisionResponseParams Responses;
	Responses.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);
	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const FVector Location = Character->GetActorLocation();
	const FVector Ahead = Heading.GetSafeNormal2D() * CeilingLookAheadDistance;
	bool bFound = false;
	float LowestCeilingZ = TNumericLimits<float>::Max();
	for (const float Fraction : {0.0f, 0.5f, 1.0f})
	{
		const FVector Probe = Location + Ahead * Fraction;
		FHitResult FloorHit;
		const bool bFloor = World->LineTraceSingleByChannel(FloorHit, Probe,
			Probe - FVector::UpVector * GroundTraceDistance, ObstacleTraceChannel, Params, Responses)
			&& !FloorHit.bStartPenetrating && FloorHit.ImpactNormal.Z >= GetWalkableFloorZ();
		// 바닥부터 검사하면 현재 비행 높이보다 낮은 입구의 안쪽 천장도 발견한다.
		const float FloorZ = bFloor ? FloorHit.ImpactPoint.Z
			: (bHasGroundHeight ? SmoothedGroundHeight : Location.Z - Capsule->GetScaledCapsuleHalfHeight());
		const FVector Start(Probe.X, Probe.Y, FloorZ + Radius + FloorClearanceMargin);
		FHitResult Hit;
		if (World->SweepSingleByChannel(Hit, Start, Start + FVector::UpVector * CeilingTraceDistance,
			FQuat::Identity, ObstacleTraceChannel, FCollisionShape::MakeSphere(Radius), Params, Responses)
			&& !Hit.bStartPenetrating && Hit.ImpactNormal.Z < -0.1f)
		{
			// 구의 중심 높이에서 캡슐 반높이를 빼면 곡면 가장자리도 몸 폭으로 제한된다.
			LowestCeilingZ = FMath::Min(LowestCeilingZ, Hit.Location.Z + Radius);
			bFound = true;
		}
	}
	if (bFound)
	{
		// 낮은 제한은 즉시 적용하고, 높은 공간으로의 복귀는 잠시 유예한다.
		if (!bHasCeiling || LowestCeilingZ < CachedCeilingZ || Now - LastCeilingHitTime >= CeilingReleaseDelay)
		{
			CachedCeilingZ = LowestCeilingZ;
		}
		if (!bHasCeiling || LowestCeilingZ <= CachedCeilingZ + 1.0f) { LastCeilingHitTime = Now; }
		bHasCeiling = true;
	}
	else if (Now - LastCeilingHitTime >= CeilingReleaseDelay)
	{
		bHasCeiling = false;
	}
}

float UFlyingMonsterMovementComponent::LimitHeightByCeiling(const float WorldZ) const
{
	if (!bHasCeiling)
	{
		return WorldZ;
	}

	const ACharacter* Character = GetCharacterOwner();
	const UCapsuleComponent* Capsule = IsValid(Character) ? Character->GetCapsuleComponent() : nullptr;
	const float HalfHeight = IsValid(Capsule) ? Capsule->GetScaledCapsuleHalfHeight() : 0.0f;
	const float FloorMin = (bHasGroundHeight ? SmoothedGroundHeight : Character->GetActorLocation().Z - HalfHeight)
		+ HalfHeight + FloorClearanceMargin;
	const float HardMax = CachedCeilingZ - HalfHeight - CeilingClearance;
	const float ComfortableMax = CachedCeilingZ - HalfHeight - FMath::Max(CeilingClearance, ComfortableCeilingClearance);
	return FMath::Min(WorldZ, FMath::Min(HardMax, FMath::Max(FloorMin, ComfortableMax)));
}
