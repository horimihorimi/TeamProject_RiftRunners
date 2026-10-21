#include "GameCore/Animation/ANS_BeamHit.h"

#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "GameCore/AI/MonsterAIController.h"
#include "GameCore/Combat/EnemyBeamAimReplicationComponent.h"
#include "GameFramework/Pawn.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

void UANS_BeamHit::OnWindowBegin(USkeletalMeshComponent* MeshComp)
{
	/*
	 * 이전 빔이 남아 있으면 먼저 치운다. NotifyEnd는 몽타주가 비정상 종료되면
	 * 건너뛸 수 있고, 그때 남은 빔은 영영 안 꺼진다.
	 */
	DestroyBeam(MeshComp);

	/*
	 * 조준은 몬스터가 보고 있던 방향에서 출발한다. 타깃 방향에서 시작하면
	 * 빔이 켜지자마자 겨눠져 있어서, 훑고 지나가는 스윕 자체가 사라진다.
	 */
	const AActor* Owner = MeshComp->GetOwner();
	AimDirections.Add(
		MeshComp,
		IsValid(Owner) ? Owner->GetActorForwardVector() : FVector::ForwardVector);

	if (IsValid(Owner) && Owner->HasAuthority())
	{
		if (UEnemyBeamAimReplicationComponent* BeamAim = Owner->FindComponentByClass<UEnemyBeamAimReplicationComponent>())
		{
			BeamAim->SetServerAimDirection(Owner->GetActorForwardVector());
		}
	}

	if (!IsValid(BeamSystem))
	{
		return;
	}

	/*
	 * 소켓에 부착하면 시작점이 따라오므로 매 틱 위치를 맞출 필요가 없다.
	 * 메시가 파괴되면 같이 정리되는 것도 부착의 이점이다.
	 * 방향은 컴포넌트 회전이 아니라 BeamEnd 파라미터가 정한다.
	 */
	UNiagaraComponent* Beam = UNiagaraFunctionLibrary::SpawnSystemAttached(
		BeamSystem,
		MeshComp,
		BeamSocketName,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		EAttachLocation::SnapToTarget,
		/*bAutoDestroy=*/false);

	if (IsValid(Beam))
	{
		ActiveBeams.Add(MeshComp, Beam);
	}
}

void UANS_BeamHit::OnWindowEnd(USkeletalMeshComponent* MeshComp)
{
	if (AActor* Owner = IsValid(MeshComp) ? MeshComp->GetOwner() : nullptr; IsValid(Owner) && Owner->HasAuthority())
	{
		if (UEnemyBeamAimReplicationComponent* BeamAim = Owner->FindComponentByClass<UEnemyBeamAimReplicationComponent>())
		{
			BeamAim->StopServerAim();
		}
	}

	DestroyBeam(MeshComp);
	AimDirections.Remove(MeshComp);
}

void UANS_BeamHit::DestroyBeam(USkeletalMeshComponent* MeshComp)
{
	if (TWeakObjectPtr<UNiagaraComponent>* Found = ActiveBeams.Find(MeshComp))
	{
		if (UNiagaraComponent* Beam = Found->Get())
		{
			Beam->DestroyComponent();
		}
		ActiveBeams.Remove(MeshComp);
	}
}

FVector UANS_BeamHit::GetDesiredDirection(
	const USkeletalMeshComponent* MeshComp,
	const FVector& Origin) const
{
	const AActor* Owner = MeshComp->GetOwner();
	const FVector Fallback = IsValid(Owner)
		? Owner->GetActorForwardVector()
		: FVector::ForwardVector;

	switch (AimMode)
	{
	case EBeamAimMode::SocketForward:
		return MeshComp->GetSocketQuaternion(BeamSocketName).GetForwardVector();

	case EBeamAimMode::Target:
	{
		// AIController의 타깃은 서버 전용이다. 클라이언트는 서버가 HitWindow에서
		// 이미 보간해 확정한 방향만 받아 연출을 같은 축으로 그린다.
		if (IsValid(Owner) && !Owner->HasAuthority())
		{
			if (const UEnemyBeamAimReplicationComponent* BeamAim = Owner->FindComponentByClass<UEnemyBeamAimReplicationComponent>())
			{
				FVector ReplicatedDirection;
				if (BeamAim->GetReplicatedAimDirection(ReplicatedDirection))
				{
					return ReplicatedDirection;
				}
			}
		}

		const APawn* OwnerPawn = Cast<APawn>(Owner);
		const AMonsterAIController* Controller =
			IsValid(OwnerPawn) ? Cast<AMonsterAIController>(OwnerPawn->GetController()) : nullptr;
		AActor* TargetActor =
			IsValid(Controller) ? Controller->GetPerceivedTarget() : nullptr;

		if (!IsValid(TargetActor))
		{
			// 타깃을 잃어도 빔은 계속 쏜다. 방향만 소유자 전방으로 떨어진다.
			return Fallback;
		}

		FVector ToTarget = TargetActor->GetActorLocation() - Origin;
		if (!bAimPitch)
		{
			ToTarget.Z = 0.0f;
		}

		return ToTarget.IsNearlyZero() ? Fallback : ToTarget.GetSafeNormal();
	}

	case EBeamAimMode::OwnerForward:
	default:
		return Fallback;
	}
}

bool UANS_BeamHit::GetTraceSegment(
	USkeletalMeshComponent* MeshComp,
	const float DeltaTime,
	FVector& OutStart,
	FVector& OutEnd)
{
	AActor* Owner = IsValid(MeshComp) ? MeshComp->GetOwner() : nullptr;
	if (!IsValid(Owner))
	{
		return false;
	}

	FVector* CurrentDirection = AimDirections.Find(MeshComp);
	if (CurrentDirection == nullptr)
	{
		// 윈도우가 열리지 않은 상태로 틱이 들어오면 판정하지 않는다.
		return false;
	}

	OutStart = MeshComp->GetSocketLocation(BeamSocketName);

	const FVector DesiredDirection = GetDesiredDirection(MeshComp, OutStart);

	/*
	 * 추적할 때만 보간한다. 즉시 조준하면 옆으로 뛰어도 빔이 붙어 다녀 회피가
	 * 성립하지 않는다. TurnSpeed만큼씩 좁혀야 각을 벌려 떨궈내는 대응이 생긴다.
	 *
	 * 나머지 모드는 이미 다른 것이 방향을 정하고 있으므로 그대로 따라간다.
	 * 거기서 또 보간하면 연출보다 판정이 뒤처진다.
	 */
	const bool bShouldInterpolate = AimMode == EBeamAimMode::Target && TurnSpeed > 0.0f;

	*CurrentDirection = bShouldInterpolate
		? FMath::VInterpNormalRotationTo(*CurrentDirection, DesiredDirection, DeltaTime, TurnSpeed)
		: DesiredDirection;

	if (Owner->HasAuthority())
	{
		if (UEnemyBeamAimReplicationComponent* BeamAim = Owner->FindComponentByClass<UEnemyBeamAimReplicationComponent>())
		{
			BeamAim->SetServerAimDirection(*CurrentDirection);
		}
	}

	OutEnd = OutStart + *CurrentDirection * BeamLength;

	return true;
}

void UANS_BeamHit::OnTraceResolved(
	USkeletalMeshComponent* MeshComp,
	const FVector& Start,
	const FVector& End,
	const TArray<FHitResult>& HitResults)
{
	if (BeamEndParameterName.IsNone())
	{
		return;
	}

	const TWeakObjectPtr<UNiagaraComponent>* Found = ActiveBeams.Find(MeshComp);
	UNiagaraComponent* Beam = Found != nullptr ? Found->Get() : nullptr;
	if (!IsValid(Beam))
	{
		return;
	}

	/*
	 * 막힌 지점까지만 그린다. SweepMulti는 거리순 정렬을 보장하지 않으므로
	 * 가장 가까운 블로킹 히트를 직접 고른다. 아무것도 막지 않았으면 끝까지 뻗는다.
	 */
	FVector WorldEnd = End;
	float ClosestDistance = TNumericLimits<float>::Max();
	for (const FHitResult& Hit : HitResults)
	{
		if (Hit.bBlockingHit && Hit.Distance < ClosestDistance)
		{
			ClosestDistance = Hit.Distance;
			WorldEnd = Hit.ImpactPoint;
		}
	}

	/*
	 * 로컬을 요구하는 이미터라도 같은 월드 끝점에서 역변환해 넘긴다.
	 * 방향을 따로 구하지 않으므로 소켓 축이 어느 쪽이든 연출이 트레이스를 따라간다.
	 */
	const FVector ParameterValue = bBeamEndIsWorldSpace
		? WorldEnd
		: Beam->GetComponentTransform().InverseTransformPosition(WorldEnd);

	Beam->SetVectorParameter(BeamEndParameterName, ParameterValue);
}

void UANS_BeamHit::DrawDebugTrace(
	const AActor* Owner,
	const FVector& Start,
	const FVector& End,
	const TArray<FHitResult>& HitResults,
	const bool bHit) const
{
	const UWorld* World = Owner->GetWorld();
	const FColor LineColor = bHit ? FColor::Red : FColor::Green;

	// 트레이스 축. 나이아가라 빔이 이 선 위에 놓여야 위치가 맞는 것이다.
	DrawDebugLine(World, Start, End, LineColor, false, DebugDrawDuration, 0, 3.0f);

	// 시작점 = 빔이 나와야 할 소켓. 눈이 아닌 곳에 있으면 소켓 지정이 틀린 것이다.
	DrawDebugSphere(World, Start, TraceRadius, 12, FColor::Cyan, false, DebugDrawDuration);
	DrawDebugSphere(World, End, TraceRadius, 12, LineColor, false, DebugDrawDuration);

	for (const FHitResult& Hit : HitResults)
	{
		if (Hit.bBlockingHit)
		{
			DrawDebugSphere(
				World, Hit.ImpactPoint, TraceRadius * 0.5f, 12,
				FColor::Yellow, false, DebugDrawDuration);
		}
	}
}
