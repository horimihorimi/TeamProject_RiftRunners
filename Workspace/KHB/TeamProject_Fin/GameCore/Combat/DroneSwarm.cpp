#include "GameCore/Combat/DroneSwarm.h"

#include "GameCore/Combat/MonsterDamageLibrary.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffectTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameCore/Ability/DamageSystemLibrary.h"
#include "GameCore/Attribute/CharacterAttributeSet.h"
#include "GameCore/Character/BaseCharacter.h"
#include "GameCore/Projectile/MonsterProjectile.h"
#include "GameCore/Tags/GameplayTagSystem.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Net/UnrealNetwork.h"

namespace
{
	/*
	 * 제어점을 흩을 오프셋. 아래로 파고들면 경로가 바닥을 뚫고 지나가므로
	 * 수직 성분은 위쪽으로만 둔다. 수평은 어느 쪽이든 상관없다.
	 */
	FVector ScatterOffset(const float Radius)
	{
		FVector Direction = FMath::VRand();
		Direction.Z = FMath::Abs(Direction.Z);
		return Direction * Radius;
	}
}

ADroneSwarm::ADroneSwarm()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

	bReplicates = true;

	/*
	 * 캐리어가 아니라 스스로가 데미지 출처가 된다. 몬스터는 사망 0.1초 뒤
	 * 파괴되므로 캐리어에 기대면 그 뒤에 터지는 드론이 무해해진다.
	 */
	AbilitySystemComponent =
		CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);

	AttributeSet = CreateDefaultSubobject<UCharacterAttributeSet>(TEXT("AttributeSet"));
}

UAbilitySystemComponent* ADroneSwarm::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ADroneSwarm::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADroneSwarm, Drones);
	DOREPLIFETIME(ADroneSwarm, PlacementScale);
}

FVector ADroneSwarm::EvaluateBezier(
	const FVector& P0, const FVector& P1,
	const FVector& P2, const FVector& P3, const float T)
{
	// 3차 베지어. T=1이면 반드시 P3에 도착하므로 배치 지점이 흔들리지 않는다.
	const float U = 1.0f - T;
	return U * U * U * P0
		+ 3.0f * U * U * T * P1
		+ 3.0f * U * T * T * P2
		+ T * T * T * P3;
}

void ADroneSwarm::Deploy(
	AActor* InInstigator,
	const TArray<FVector>& InLaunchLocations,
	const AActor* InTarget,
	const FGameplayTag& InAttackTag)
{
	if (InLaunchLocations.Num() == 0)
	{
		Destroy();
		return;
	}

	// 공격력과 같은 이유로 지금 복사한다. 자폭 시점에는 캐리어가 없을 수 있다.
	AttackTag = InAttackTag;

	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	// 드론을 쏴서 떨어뜨릴 수 있게 한다. 판정은 서버만 한다.
	AbilitySystemComponent->OnGameplayEffectAppliedDelegateToSelf.AddUObject(
		this, &ADroneSwarm::OnDamageReceived);

	/*
	 * 캐리어의 공격력을 지금 복사해 둔다. 나중에 참조하면 이미 파괴된 뒤일 수
	 * 있다. 투사체가 발사 시점의 위력을 그대로 들고 날아가는 것과 같다.
	 */
	if (const UAbilitySystemComponent* SourceASC =
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(InInstigator))
	{
		const float AttackPower =
			SourceASC->GetNumericAttribute(UCharacterAttributeSet::GetAttackPowerAttribute());

		AbilitySystemComponent->SetNumericAttributeBase(
			UCharacterAttributeSet::GetAttackPowerAttribute(), AttackPower);
	}

	PlacementScale = IsValid(InInstigator)
		? FMath::Max(FMath::Abs(InInstigator->GetActorScale3D().Z), KINDA_SMALL_NUMBER)
		: 1.0f;
	BuildPlacement(InLaunchLocations, InTarget);

	if (Drones.Num() == 0)
	{
		// 지면을 찾지 못해 배치가 전멸했다. 빈 껍데기를 남길 이유가 없다.
		Destroy();
		return;
	}

	SpawnVisuals();

	Phase = EDroneSwarmPhase::Deploy;
	PhaseElapsed = 0.0f;
}

void ADroneSwarm::OnRep_Drones()
{
	// 서버가 확정한 배치가 도착했다. 클라이언트는 이걸로 같은 진행을 재생한다.
	for (FSwarmDrone& Drone : Drones)
	{
		Drone.CurrentLocation = Drone.LaunchLocation;
		Drone.PhaseElapsed = 0.0f;
	}

	SpawnVisuals();

	Phase = EDroneSwarmPhase::Deploy;
	PhaseElapsed = 0.0f;
}

void ADroneSwarm::BuildPlacement(
	const TArray<FVector>& InLaunchLocations, const AActor* InTarget)
{
	Drones.Reset();

	// 사출 지점이 여럿이므로 벽의 기준은 그 무게중심으로 잡는다.
	FVector Centroid = FVector::ZeroVector;
	for (const FVector& Location : InLaunchLocations)
	{
		Centroid += Location;
	}
	Centroid /= InLaunchLocations.Num();

	SetActorLocation(Centroid);

	const FVector TargetLocation = IsValid(InTarget)
		? InTarget->GetActorLocation()
		: Centroid + FVector::ForwardVector * MaxWallDistance;

	const FVector ToTarget = TargetLocation - Centroid;
	FVector Axis = ToTarget.GetSafeNormal2D();
	if (Axis.IsNearlyZero())
	{
		Axis = FVector::ForwardVector;
	}

	/*
	 * 벽은 캐리어와 타깃 사이에 선다. 고정 거리로 두면 플레이어가 가까울 때
	 * 등 뒤에 생겨 아무것도 막지 못한다. 비율로 잡고 양쪽을 clamp한다.
	 */
	const float DistanceToTarget = ToTarget.Size2D();
	float WallDistance =
		FMath::Clamp(DistanceToTarget * WallRatio, MinWallDistance, MaxWallDistance);

	// 발밑에 벽이 생기면 피할 방법이 없다.
	WallDistance = FMath::Min(WallDistance, DistanceToTarget - MinTargetClearance);
	WallDistance = FMath::Max(WallDistance, 0.0f);

	const FVector Right = FVector::CrossProduct(FVector::UpVector, Axis).GetSafeNormal();
	const FVector LineCenter = Centroid + Axis * WallDistance;

	const int32 PlannedCount = InLaunchLocations.Num();

	for (int32 Index = 0; Index < PlannedCount; ++Index)
	{
		const float LateralOffset =
			(static_cast<float>(Index) - (PlannedCount - 1) * 0.5f) * Spacing;

		FVector Candidate = LineCenter
			+ Right * LateralOffset
			+ Axis * FMath::FRandRange(-DepthJitter, DepthJitter);

		float GroundZ = 0.0f;
		if (!ResolveGroundZ(Candidate, GroundZ))
		{
			// 허공이나 낭떠러지다. 억지로 옮기면 벽이 뭉치므로 이 기만 뺀다.
			continue;
		}

		FSwarmDrone Drone;
		Drone.LaunchLocation = InLaunchLocations[Index];
		Drone.CurrentLocation = Drone.LaunchLocation;

		Drone.HoverLocation = FVector(
			Candidate.X,
			Candidate.Y,
			GroundZ + FMath::FRandRange(MinHoverHeight, MaxHoverHeight) * PlacementScale);

		// 바닥에서 올려 검사해야 천장 위에 이미 생성된 목적지를 허용하지 않는다.
		const float Clearance = GetDroneClearance();
		const FVector FloorStart(Candidate.X, Candidate.Y, GroundZ + Clearance + 1.0f);
		Drone.HoverLocation.Z = FMath::Max(Drone.HoverLocation.Z, FloorStart.Z);
		Drone.HoverLocation = ConstrainMovement(FloorStart, Drone.HoverLocation);
		if (Drone.HoverLocation.Z <= FloorStart.Z)
		{
			continue;
		}

		/*
		 * 제어점 둘을 직선 위 1/3, 2/3 지점에서 각자 다른 방향으로 밀어낸다.
		 * 출발과 도착 양쪽이 개체마다 다르므로 소켓을 떠나는 순간부터 경로가 갈린다.
		 * CurveAmount가 0이면 둘 다 직선 위에 남아 경로가 정확히 직선이 된다.
		 */
		const FVector Straight = Drone.HoverLocation - Drone.LaunchLocation;
		const FVector StraightA = Drone.LaunchLocation + Straight / 3.0f;
		const FVector StraightB = Drone.LaunchLocation + Straight * (2.0f / 3.0f);

		Drone.ControlA = StraightA + ScatterOffset(DepartureRadius) * CurveAmount * PlacementScale;
		Drone.ControlB = StraightB + ScatterOffset(ApproachRadius) * CurveAmount * PlacementScale;
		if (!IsFlightPathClear(Drone))
		{
			// 호가 천장을 넘으면 먼저 직선 경로를 시도한다.
			Drone.ControlA = StraightA;
			Drone.ControlB = StraightB;
			if (!IsFlightPathClear(Drone))
			{
				continue;
			}
		}

		Drone.LaunchDelay = Index * StaggerInterval;

		// 위상과 속도가 같으면 무리가 한 덩어리로 움직여 오히려 더 기계적으로 보인다.
		Drone.HoverPhase = FVector(
			FMath::FRandRange(0.0f, 2.0f * PI),
			FMath::FRandRange(0.0f, 2.0f * PI),
			FMath::FRandRange(0.0f, 2.0f * PI));

		Drone.HoverSpeedScale =
			1.0f + FMath::FRandRange(-HoverSpeedVariance, HoverSpeedVariance);

		Drones.Add(MoveTemp(Drone));
	}
}

bool ADroneSwarm::ResolveGroundZ(const FVector& Location, float& OutGroundZ) const
{
	/*
	 * 떠 있는 높이를 지면에서 재려고 한 번 훑는다. 월드 Z 절대값으로 잡으면
	 * 경사나 단차에서 드론이 땅에 박히거나 너무 높이 뜬다.
	 */
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DroneSwarmGround), false);
	QueryParams.AddIgnoredActor(this);
	if (const AActor* OwnerActor = GetOwner())
	{
		QueryParams.AddIgnoredActor(OwnerActor);
	}

	FHitResult GroundHit;
	FCollisionObjectQueryParams GroundObjects;
	GroundObjects.AddObjectTypesToQuery(ECC_WorldStatic);
	GroundObjects.AddObjectTypesToQuery(ECC_WorldDynamic);
	FVector GroundStart = Location;
	if (const AActor* OwnerActor = GetOwner())
	{
		GroundStart.Z = FMath::Min(GroundStart.Z, OwnerActor->GetActorLocation().Z);
	}
	const bool bHit = GetWorld()->LineTraceSingleByObjectType(
		GroundHit,
		// 실내 높이에서 아래로 찾고 Pawn은 제외한다.
		GroundStart,
		GroundStart - FVector(0.0f, 0.0f, 1500.0f),
		GroundObjects,
		QueryParams);

	if (!bHit || GroundHit.ImpactNormal.Z < 0.7f)
	{
		return false;
	}

	OutGroundZ = GroundHit.ImpactPoint.Z;
	return true;
}

float ADroneSwarm::GetDroneClearance() const
{
	return IsValid(DroneMesh)
		? FMath::Max((DroneMesh->GetBounds().BoxExtent * DroneMeshScale.GetAbs()).Size(), 1.0f)
		: 24.0f;
}

FVector ADroneSwarm::ConstrainMovement(const FVector& Start, const FVector& End) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DroneSwarmFlight), false);
	Params.AddIgnoredActor(this);
	if (const AActor* OwnerActor = GetOwner())
	{
		Params.AddIgnoredActor(OwnerActor);
	}
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	FHitResult Hit;
	if (GetWorld()->SweepSingleByObjectType(Hit, Start, End, FQuat::Identity,
		Objects, FCollisionShape::MakeSphere(GetDroneClearance()), Params))
	{
		return Hit.bStartPenetrating ? Start : Hit.Location;
	}
	return End;
}

bool ADroneSwarm::IsFlightPathClear(const FSwarmDrone& Drone) const
{
	FVector Previous = Drone.LaunchLocation;
	for (int32 Step = 1; Step <= 32; ++Step)
	{
		const FVector Next = EvaluateBezier(Drone.LaunchLocation, Drone.ControlA,
			Drone.ControlB, Drone.HoverLocation, Step / 32.0f);
		if (!ConstrainMovement(Previous, Next).Equals(Next, 0.1f))
		{
			return false;
		}
		Previous = Next;
	}
	return true;
}

void ADroneSwarm::SpawnVisuals()
{
	if (IsValid(DroneMesh))
	{
		for (FSwarmDrone& Drone : Drones)
		{
			UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(this);
			if (!IsValid(Mesh))
			{
				continue;
			}

			Mesh->SetStaticMesh(DroneMesh);
			Mesh->SetWorldScale3D(DroneMeshScale);

			/*
			 * 총알만 받는다. 물리로 밀거나 이동을 막으면 안 되므로 나머지 채널은
			 * 전부 무시하고 공격 트레이스에만 반응한다.
			 */
			Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Mesh->SetCollisionObjectType(ECC_WorldDynamic);
			Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
			Mesh->SetCollisionResponseToChannel(ECC_TraceAttack, ECR_Block);

			// 런타임 생성이라 등록한 뒤에 붙인다. 생성자에서 쓰는 SetupAttachment와 다르다.
			Mesh->RegisterComponent();
			Mesh->AttachToComponent(
				GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
			Mesh->SetWorldLocation(Drone.LaunchLocation);

			Drone.Visual = Mesh;
		}
	}

	if (!IsValid(LinkEffect))
	{
		return;
	}

	/*
	 * 인접한 쌍만 잇는다. 전부 연결하면 그물이 되어 한 줄로 읽히지 않는다.
	 * 자폭 순간에 켜야 하지만 미리 만들어 둔다. 그때 한꺼번에 스폰하면 튄다.
	 */
	for (int32 Index = 0; Index + 1 < Drones.Num(); ++Index)
	{
		UNiagaraComponent* Link = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(), LinkEffect, Drones[Index].HoverLocation,
			FRotator::ZeroRotator, FVector::OneVector,
			/*bAutoDestroy=*/false);

		if (IsValid(Link))
		{
			/*
			 * 스폰하면 곧바로 재생이 시작된다. 자폭까지 몇 초가 걸리므로 그냥
			 * 숨겨두면 켤 때쯤엔 이미 수명이 다해 아무것도 안 보인다.
			 * 시뮬레이션 자체를 멈춰 두었다가 그때 되감아 켠다.
			 */
			Link->Deactivate();
			Link->SetVisibility(false);
			Links.Add(Link);
		}
	}
}

void ADroneSwarm::SyncVisuals()
{
	// 연출은 로직이 정한 위치를 따라간다. 반대가 되면 메시 없이 검증할 수 없다.
	for (const FSwarmDrone& Drone : Drones)
	{
		if (!IsValid(Drone.Visual))
		{
			continue;
		}

		/*
		 * 이동 방향을 바라보게 한다. 직전 위치를 따로 들고 있지 않아도
		 * 컴포넌트가 아직 옛 위치에 있으므로 그 차이가 곧 이번 프레임의 이동이다.
		 */
		if (bOrientToVelocity)
		{
			const FVector Delta = Drone.CurrentLocation - Drone.Visual->GetComponentLocation();
			if (!Delta.IsNearlyZero())
			{
				Drone.Visual->SetWorldRotation(Delta.Rotation());
			}
		}

		Drone.Visual->SetWorldLocation(Drone.CurrentLocation);
	}
}

void ADroneSwarm::DestroyLinks()
{
	for (UNiagaraComponent* Link : Links)
	{
		if (IsValid(Link))
		{
			Link->DestroyComponent();
		}
	}
	Links.Reset();
}

void ADroneSwarm::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (Drones.Num() == 0)
	{
		// 클라이언트는 배치가 복제되기 전까지 할 일이 없다.
		return;
	}

	PhaseElapsed += DeltaSeconds;

	switch (Phase)
	{
	case EDroneSwarmPhase::Deploy:
		TickDeploy(DeltaSeconds);
		break;

	case EDroneSwarmPhase::Hover:
		TickHover(DeltaSeconds);
		break;

	case EDroneSwarmPhase::Settle:
		/*
		 * 위치를 갱신하지 않는다. 마지막 맴돌기 좌표에 그대로 굳고, 그 자리가
		 * 곧 판정 위치가 된다. 움직임이 멈추는 것 자체가 예고다.
		 */
		if (PhaseElapsed >= SettleDuration)
		{
			BeginDischarge();
		}
		break;

	case EDroneSwarmPhase::Discharge:
		// 판정은 진입 프레임에 끝났다. 남은 시간은 연출을 보여주는 데 쓴다.
		if (PhaseElapsed >= DischargeDuration)
		{
			Phase = EDroneSwarmPhase::Finished;
			PhaseElapsed = 0.0f;
		}
		break;

	case EDroneSwarmPhase::Finished:
		DestroyLinks();

		// 액터 제거는 서버가 결정한다. 클라이언트는 복제로 따라 사라진다.
		if (HasAuthority())
		{
			Destroy();
		}
		return;
	}

	SyncVisuals();

	if (bDebugDrawSequence)
	{
		DrawSequence();
	}
}

void ADroneSwarm::TickDeploy(const float DeltaSeconds)
{
	bool bAllArrived = true;

	for (FSwarmDrone& Drone : Drones)
	{
		if (Drone.bDestroyed)
		{
			continue;
		}

		Drone.PhaseElapsed += DeltaSeconds;

		const float FlightElapsed = Drone.PhaseElapsed - Drone.LaunchDelay;
		if (FlightElapsed < 0.0f)
		{
			bAllArrived = false;
			continue;   // 아직 출발 전이다.
		}

		const float RawAlpha = FMath::Clamp(FlightElapsed / FlightDuration, 0.0f, 1.0f);
		if (RawAlpha < 1.0f)
		{
			bAllArrived = false;
		}

		// 곡선은 베지어가, 가감속은 이징이 맡는다. 따로 조절하려고 분리해 둔다.
		const float T = FMath::InterpEaseOut(0.0f, 1.0f, RawAlpha, 2.0f);
		Drone.CurrentLocation = ConstrainMovement(Drone.CurrentLocation,
			EvaluateBezier(Drone.LaunchLocation, Drone.ControlA, Drone.ControlB, Drone.HoverLocation, T));
	}

	if (bAllArrived)
	{
		Phase = EDroneSwarmPhase::Hover;
		PhaseElapsed = 0.0f;
		for (FSwarmDrone& Drone : Drones)
		{
			Drone.PhaseElapsed = 0.0f;
		}
	}
}

void ADroneSwarm::TickHover(const float DeltaSeconds)
{
	const float Time = GetWorld()->GetTimeSeconds();

	for (FSwarmDrone& Drone : Drones)
	{
		if (Drone.bDestroyed)
		{
			continue;
		}

		/*
		 * 축마다 다른 주파수로 흔들어 배치 지점 주변을 떠돌게 한다. 비율을
		 * 정수배로 두면 경로가 금세 반복되어 눈에 띄므로 어긋난 값을 쓴다.
		 *
		 * 위상과 속도가 개체마다 다르므로 같은 자리에서 시작해도 전부 다르게 논다.
		 */
		const float Phase2 = Time * HoverSpeed * Drone.HoverSpeedScale;

		const FVector Offset(
			FMath::Sin(Phase2 * 1.00f + Drone.HoverPhase.X) * HoverRadius,
			FMath::Sin(Phase2 * 0.73f + Drone.HoverPhase.Y) * HoverRadius,
			FMath::Sin(Phase2 * 1.31f + Drone.HoverPhase.Z) * HoverHeightRadius * PlacementScale);

		Drone.CurrentLocation = ConstrainMovement(Drone.CurrentLocation, Drone.HoverLocation + Offset);
	}

	if (PhaseElapsed >= HoverDuration)
	{
		Phase = EDroneSwarmPhase::Settle;
		PhaseElapsed = 0.0f;
	}
}

void ADroneSwarm::BeginDischarge()
{
	Phase = EDroneSwarmPhase::Discharge;
	PhaseElapsed = 0.0f;

	// 서버가 연출을 실행하면 클라이언트로 복제된다. 판정도 서버만 한다.
	if (HasAuthority())
	{
		ExecuteDischargeCues();
		ApplyDischargeDamage();
	}

	for (int32 Index = 0; Index < Drones.Num(); ++Index)
	{
		FSwarmDrone& Drone = Drones[Index];
		if (Drone.bDestroyed)
		{
			continue;   // 이미 떨어뜨린 드론은 터지지 않는다.
		}

		if (bDebugDrawSequence)
		{
			/*
			 * 판정 반경을 남겨 둔다. 나이아가라 폭발이 이 구 안에 들어차야
			 * 보이는 것과 맞는 것이 일치한다. 한 프레임만 그리면 이펙트가
			 * 재생되는 동안 비교할 수가 없어서 수명을 준다.
			 */
			DrawDebugSphere(
				GetWorld(), Drone.CurrentLocation, ExplosionRadius, 24,
				FColor::Red, false, DebugExplosionDrawDuration, 0, 2.0f);
		}

		// 자폭했으니 본체는 사라진다. 남겨두면 터진 뒤에도 떠 있는 그림이 된다.
		if (IsValid(Drone.Visual))
		{
			Drone.Visual->DestroyComponent();
			Drone.Visual = nullptr;
		}

		if (!Drones.IsValidIndex(Index + 1) || !Links.IsValidIndex(Index)
			|| Drones[Index + 1].bDestroyed)
		{
			continue;   // 한쪽이라도 떨어졌으면 전기가 이어지지 않는다.
		}

		UNiagaraComponent* Link = Links[Index];
		if (!IsValid(Link))
		{
			continue;
		}

		Link->SetWorldLocation(Drone.CurrentLocation);

		// 끝점을 먼저 넣고 켠다. 활성화 뒤에 넣으면 첫 프레임이 엉뚱한 곳을 가리킨다.
		if (!LinkEndParameterName.IsNone())
		{
			Link->SetVectorParameter(LinkEndParameterName, Drones[Index + 1].CurrentLocation);
		}

		Link->SetVisibility(true);
		Link->Activate(/*bReset=*/true);
	}
}

void ADroneSwarm::ExecuteDischargeCues()
{
	if (!DischargeCueTag.IsValid() || !IsValid(AbilitySystemComponent))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[DroneSwarm] CueSkipped | CueTag=%s | ASC=%s"),
			*DischargeCueTag.ToString(),
			IsValid(AbilitySystemComponent) ? TEXT("Valid") : TEXT("None"));
		return;
	}

	for (const FSwarmDrone& Drone : Drones)
	{
		if (Drone.bDestroyed)
		{
			continue;
		}

		/*
		 * 위치는 반드시 넣는다. 넣지 않으면 큐가 스웜 액터의 원점을 쓰기 때문에
		 * 흩어져 있는 드론이 전부 한자리에서 터지는 그림이 된다.
		 */
		FGameplayCueParameters CueParameters;
		CueParameters.Location = Drone.CurrentLocation;
		CueParameters.SourceObject = DischargeCueData.Get();

		AbilitySystemComponent->ExecuteGameplayCue(DischargeCueTag, CueParameters);
	}

	UE_LOG(LogTemp, Log,
		TEXT("[DroneSwarm] CueExecuted | Tag=%s | Drones=%d | CueData=%s"),
		*DischargeCueTag.ToString(),
		Drones.Num(),
		*GetNameSafe(DischargeCueData));
}

void ADroneSwarm::ApplyDischargeDamage()
{
	if (DamageEffectClass == nullptr)
	{
		return;
	}

	/*
	 * 전깃줄과 폭발이 같은 순간에 터진다. 대상을 한 곳에 모아 두어 선과 폭발
	 * 범위가 겹치는 자리에 선 대상이 두 번 맞지 않게 한다.
	 */
	TMap<AActor*, FHitResult> DamagedHits;

	for (int32 Index = 0; Index < Drones.Num(); ++Index)
	{
		const FSwarmDrone& Drone = Drones[Index];
		if (Drone.bDestroyed)
		{
			continue;
		}

		// 드론 각자의 자폭 반경
		MonsterDamageLibrary::GatherTargets(
			this,
			Drone.CurrentLocation,
			Drone.CurrentLocation,
			ExplosionRadius,
			{ this, GetOwner() },
			DamagedHits);

		if (!Drones.IsValidIndex(Index + 1) || Drones[Index + 1].bDestroyed)
		{
			continue;
		}

		// 두 드론을 잇는 전깃줄
		MonsterDamageLibrary::GatherTargets(
			this,
			Drone.CurrentLocation,
			Drones[Index + 1].CurrentLocation,
			LinkDamageRadius,
			{ this, GetOwner() },
			DamagedHits);
	}

	/*
	 * 가해자는 스웜 자신이다. 캐리어를 넘기면 그가 먼저 죽었을 때 데미지가
	 * 통째로 사라진다. 공격력은 Deploy에서 이미 복사해 두었다.
	 */
	MonsterDamageLibrary::ApplyDamageToTargets(
		this,
		MonsterDamageLibrary::MakeAttackPayload(AttackTag, DamageEffectClass),
		DamagedHits,
		TEXT("DroneSwarm"));
}

void ADroneSwarm::OnDamageReceived(
	UAbilitySystemComponent* TargetASC,
	const FGameplayEffectSpec& Spec,
	FActiveGameplayEffectHandle Handle)
{
	if (!HasAuthority() || Phase == EDroneSwarmPhase::Discharge
		|| Phase == EDroneSwarmPhase::Finished)
	{
		return;
	}

	/*
	 * 드론은 액터가 아니라 컴포넌트다. 히트 액터는 늘 이 스웜이므로 어느 기를
	 * 맞혔는지는 컴포넌트로만 알 수 있다. 데미지 경로가 이펙트 컨텍스트에
	 * 히트 결과를 실어 보내 준다.
	 */
	const FHitResult* HitResult = Spec.GetContext().GetHitResult();
	if (HitResult == nullptr)
	{
		return;
	}

	const UPrimitiveComponent* HitComponent = HitResult->GetComponent();
	if (HitComponent == nullptr)
	{
		return;
	}

	for (int32 Index = 0; Index < Drones.Num(); ++Index)
	{
		if (!Drones[Index].bDestroyed && Drones[Index].Visual == HitComponent)
		{
			MulticastDestroyDrone(Index);
			return;
		}
	}
}

void ADroneSwarm::MulticastDestroyDrone_Implementation(const int32 DroneIndex)
{
	if (!Drones.IsValidIndex(DroneIndex) || Drones[DroneIndex].bDestroyed)
	{
		return;
	}

	FSwarmDrone& Drone = Drones[DroneIndex];
	Drone.bDestroyed = true;

	if (IsValid(Drone.Visual))
	{
		Drone.Visual->DestroyComponent();
		Drone.Visual = nullptr;
	}

	/*
	 * 남은 드론이 없으면 자폭할 것도 없다. 빈 액터가 맴도는 시간을 다 채우고
	 * 나서야 사라지면 플레이어는 아직 위협이 남았다고 오해한다.
	 */
	for (const FSwarmDrone& Other : Drones)
	{
		if (!Other.bDestroyed)
		{
			return;
		}
	}

	Phase = EDroneSwarmPhase::Finished;
	PhaseElapsed = 0.0f;
}

void ADroneSwarm::DrawSequence() const
{
	const UWorld* World = GetWorld();
	const bool bDischarging = Phase == EDroneSwarmPhase::Discharge;

	for (int32 Index = 0; Index < Drones.Num(); ++Index)
	{
		const FSwarmDrone& Drone = Drones[Index];
		if (Drone.bDestroyed)
		{
			continue;
		}

		// 드론 본체
		DrawDebugSphere(World, Drone.CurrentLocation, 24.0f, 8, FColor::Cyan, false, -1.0f);

		// 자폭 반경. 배치 단계부터 보여서 Spacing과의 관계를 눈으로 맞출 수 있다.
		DrawDebugSphere(
			World, Drone.CurrentLocation, ExplosionRadius, 16,
			bDischarging ? FColor::Orange : FColor(90, 90, 90), false, -1.0f);

		if (!Drones.IsValidIndex(Index + 1) || Drones[Index + 1].bDestroyed)
		{
			continue;
		}

		// 전깃줄. 자폭 순간에만 실제로 판정된다.
		DrawDebugLine(
			World, Drone.CurrentLocation, Drones[Index + 1].CurrentLocation,
			bDischarging ? FColor::Yellow : FColor(70, 70, 70),
			false, -1.0f, 0, bDischarging ? 5.0f : 1.0f);
	}
}
