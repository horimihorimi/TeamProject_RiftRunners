#include "AbilityTask_FollowDashSpline.h"

#include "Components/SplineComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Curves/CurveFloat.h"
#include "DashSplinePath.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UAbilityTask_FollowDashSpline* UAbilityTask_FollowDashSpline::PerformDash(
	UGameplayAbility* OwningAbility,
	const FDashPatternChoice& Pattern,
	AActor* InTargetActor,
	float InRotationInterpSpeed,
	bool bInDebugDraw)
{
	UAbilityTask_FollowDashSpline* Task = NewAbilityTask<UAbilityTask_FollowDashSpline>(OwningAbility);
	Task->DashPattern = Pattern;
	Task->DashPathClass = Pattern.SplinePath;
	Task->TargetActor = InTargetActor;
	Task->RotationInterpSpeed = FMath::Max(0.0f, InRotationInterpSpeed);
	Task->bRotateAlongMovement = true;
	Task->bFaceTargetWhileMoving = Pattern.bFaceTargetWhileMoving;
	Task->bDebugDraw = bInDebugDraw;
	Task->bTickingTask = true;
	return Task;
}

UAbilityTask_FollowDashSpline* UAbilityTask_FollowDashSpline::FollowDashSpline(
	UGameplayAbility* OwningAbility,
	const TSubclassOf<ADashSplinePath> ChosenPath,
	float InMoveSpeed,
	float InRotationInterpSpeed,
	bool bInRotateAlongSpline,
	bool bInDebugDraw)
{
	FDashPatternChoice Pattern;
	Pattern.SplinePath = ChosenPath;
	Pattern.DashSpeed = FMath::Max(0.0f, InMoveSpeed);
	UAbilityTask_FollowDashSpline* Task = PerformDash(
		OwningAbility,
		Pattern,
		nullptr,
		InRotationInterpSpeed,
		bInDebugDraw);
	Task->bRotateAlongMovement = bInRotateAlongSpline;
	return Task;
}

void UAbilityTask_FollowDashSpline::Activate()
{
	const FGameplayAbilityActorInfo* ActorInfo = Ability ? Ability->GetCurrentActorInfo() : nullptr;
	AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	Character = Cast<ACharacter>(AvatarActor.Get());
	MovementComponent = IsValid(Character.Get()) ? Character->GetCharacterMovement() : nullptr;

	if (!IsValid(AvatarActor.Get()) || !IsValid(Character.Get())
		|| !IsValid(MovementComponent.Get())
		|| !IsValid(MovementComponent->UpdatedComponent)
		|| DashPattern.DashSpeed <= KINDA_SMALL_NUMBER)
	{
		EndTask();
		return;
	}

	DashStartLocation = AvatarActor->GetActorLocation();
	const FRotator YawOnlyRotation(0.0f, AvatarActor->GetActorRotation().Yaw, 0.0f);
	DashStartTransform = FTransform(YawOnlyRotation, DashStartLocation);
	ElapsedTime = 0.0f;
	AccumulatedDistance = 0.0f;
	CurrentDashSpeed = 0.0f;
	FVector InitialMovementDirection = FVector::ZeroVector;

	// 동적 경로는 그려둔 스플라인을 쓰지 않으므로 경로 클래스도 필요 없다.
	if (!DashPattern.bBuildDynamicPath && !IsValid(DashPathClass.Get()))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[DashSpline] 경로가 없어 대시를 시작할 수 없다 | Avatar=%s | ")
			TEXT("bBuildDynamicPath를 켜거나 SplinePath를 지정하라."),
			*GetNameSafe(AvatarActor.Get()));

		EndTask();
		return;
	}

	// 대시 중에는 Pawn만 통과하고 월드/지형 충돌은 기존 설정을 유지한다.
	DashCollisionComponent = Cast<UPrimitiveComponent>(MovementComponent->UpdatedComponent);
	if (DashCollisionComponent.IsValid())
	{
		PreviousPawnCollisionResponse = DashCollisionComponent->GetCollisionResponseToChannel(ECC_Pawn);
		DashCollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		bPawnCollisionOverridden = true;
	}

	if (DashPattern.bBuildDynamicPath)
	{
		if (!BuildRuntimeSplineToTarget())
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[DashSpline] 동적 경로 생성 실패 | Avatar=%s | Target=%s | ")
				TEXT("타깃이 없거나 타깃과 수직으로 겹쳐 있다."),
				*GetNameSafe(AvatarActor.Get()),
				*GetNameSafe(TargetActor.Get()));

			EndTask();
			return;
		}
		SelectedSpline = RuntimeSpline;
	}
	else
	{
		const ADashSplinePath* PathTemplate = DashPathClass->GetDefaultObject<ADashSplinePath>();
		const USplineComponent* SourceSpline = IsValid(PathTemplate) ? PathTemplate->Spline : nullptr;
		if (!IsValid(SourceSpline) || SourceSpline->GetSplineLength() <= KINDA_SMALL_NUMBER)
		{
			EndTask();
			return;
		}

		if (DashPattern.bUseSplineVariation)
		{
			if (!BuildRuntimeSplineVariation(SourceSpline))
			{
				EndTask();
				return;
			}
			SelectedSpline = RuntimeSpline;
		}
		else
		{
			SelectedSpline = const_cast<USplineComponent*>(SourceSpline);
		}
	}

	if (!SelectedSpline.IsValid() || SelectedSpline->GetSplineLength() <= KINDA_SMALL_NUMBER)
	{
		EndTask();
		return;
	}

	TotalDistance = SelectedSpline->GetSplineLength();
	InitialMovementDirection = DashStartTransform.TransformVectorNoScale(
		SelectedSpline->GetDirectionAtDistanceAlongSpline(
			0.0f,
			ESplineCoordinateSpace::Local)).GetSafeNormal();

	// Carry only the component of the existing velocity that already points along
	// the newly selected dash path. Sideways or opposing velocity is not inherited.
	const float InitialSpeed = FMath::Max(
		FVector::DotProduct(MovementComponent->Velocity, InitialMovementDirection),
		0.0f);
	InitialSpeedMultiplier = InitialSpeed / DashPattern.DashSpeed;
	CurrentDashSpeed = InitialSpeed;

	const float AverageSpeedMultiplier = CalculateAverageSpeedMultiplier();
	DashDuration = TotalDistance / (DashPattern.DashSpeed * AverageSpeedMultiplier);
	if (DashDuration <= KINDA_SMALL_NUMBER)
	{
		EndTask();
		return;
	}

	if (bDebugDraw)
	{
		if (UWorld* World = GetWorld())
		{
			constexpr int32 SampleCount = 32;
			FVector PreviousPosition = DashStartTransform.TransformPosition(
				SelectedSpline->GetLocationAtDistanceAlongSpline(0.0f, ESplineCoordinateSpace::Local));
			for (int32 SampleIndex = 1; SampleIndex <= SampleCount; ++SampleIndex)
			{
				const float Distance = TotalDistance * static_cast<float>(SampleIndex) / SampleCount;
				const FVector CurrentPosition = DashStartTransform.TransformPosition(
					SelectedSpline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::Local));
				DrawDebugLine(World, PreviousPosition, CurrentPosition, FColor::Green, false, 5.0f, 0, 3.0f);
				PreviousPosition = CurrentPosition;
			}
		}
	}
}

void UAbilityTask_FollowDashSpline::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);

	if (!IsValid(AvatarActor.Get()) || !IsValid(Character.Get()) || !IsValid(MovementComponent.Get())
		|| !IsValid(MovementComponent->UpdatedComponent))
	{
		EndTask();
		return;
	}

	const float SafeDeltaTime = FMath::Max(0.0f, DeltaTime);
	ElapsedTime = FMath::Min(ElapsedTime + SafeDeltaTime, DashDuration);
	const float TimeAlpha = FMath::Clamp(ElapsedTime / DashDuration, 0.0f, 1.0f);

	const float CurrentSpeedMultiplier = EvaluateSpeedMultiplier(TimeAlpha);
	const float RemainingDistance = FMath::Max(0.0f, TotalDistance - AccumulatedDistance);
	const float Deceleration = FMath::Max(DashPattern.DashSpeed * 10.0f, 1.0f);
	const float StoppingSpeed = FMath::Sqrt(2.0f * Deceleration * RemainingDistance);

	float DesiredSpeed = DashPattern.DashSpeed * CurrentSpeedMultiplier;
	if (TimeAlpha >= 1.0f)
	{
		// A speed curve may end at zero. Continue braking to the endpoint
		// instead of freezing before the travelled distance is complete.
		DesiredSpeed = StoppingSpeed;
	}
	else
	{
		DesiredSpeed = FMath::Min(DesiredSpeed, StoppingSpeed);
	}

	const float InterpSpeed = DesiredSpeed >= CurrentDashSpeed ? 12.0f : 18.0f;
	CurrentDashSpeed = FMath::FInterpTo(
		CurrentDashSpeed,
		DesiredSpeed,
		SafeDeltaTime,
		InterpSpeed);

	float FrameDistance = FMath::Min(
		CurrentDashSpeed * SafeDeltaTime,
		RemainingDistance);
	if (RemainingDistance <= 1.0f)
	{
		FrameDistance = RemainingDistance;
	}
	AccumulatedDistance += FrameDistance;

	const FVector CurrentPosition =
		MovementComponent->UpdatedComponent->GetComponentLocation();
	if (!SelectedSpline.IsValid())
	{
		EndTask();
		return;
	}

	const float SplineDistance = AccumulatedDistance;
	const FVector TargetPosition = DashStartTransform.TransformPosition(
		SelectedSpline->GetLocationAtDistanceAlongSpline(
			SplineDistance,
			ESplineCoordinateSpace::Local));
	const FVector WorldDirection = DashStartTransform.TransformVectorNoScale(
		SelectedSpline->GetDirectionAtDistanceAlongSpline(
			SplineDistance,
			ESplineCoordinateSpace::Local));

	const FQuat CurrentRotation =
		MovementComponent->UpdatedComponent->GetComponentQuat();

	FQuat AppliedRotation = CurrentRotation;
	if (bRotateAlongMovement)
	{
		FVector DesiredDirection = WorldDirection;
		if (bFaceTargetWhileMoving && TargetActor.IsValid())
		{
			DesiredDirection = TargetActor->GetActorLocation() - CurrentPosition;
			DesiredDirection.Z = 0.0f;
		}

		if (!DesiredDirection.IsNearlyZero())
		{
			const FQuat DesiredRotation = DesiredDirection.ToOrientationQuat();
			const float Alpha = 1.0f - FMath::Exp(-RotationInterpSpeed * DeltaTime);
			AppliedRotation = FQuat::Slerp(CurrentRotation, DesiredRotation, Alpha).GetNormalized();
		}
	}

	const FVector Delta = TargetPosition - CurrentPosition;

	if (!Delta.IsNearlyZero())
	{
		FHitResult Hit;
		MovementComponent->SafeMoveUpdatedComponent(
			Delta,
			AppliedRotation,
			true,
			Hit);

		const FVector ActualDelta =
			MovementComponent->UpdatedComponent->GetComponentLocation() - CurrentPosition;
		MovementComponent->Velocity = DeltaTime > SMALL_NUMBER
			? ActualDelta / DeltaTime
			: FVector::ZeroVector;

		// 벽/플레이어 등에 막히면 이번 대쉬는 중단.
		if (Hit.IsValidBlockingHit())
		{
			/*
			 * OnFinished를 안 부르고 그냥 끝내면 GA_DashFollowSpline이
			 * 몽타주를 End로 점프시키지도, 스스로 EndAbility를 부르지도
			 * 못한다. 그러면 GA_Dash도 몽타주가 안 끝나서 영원히 안
			 * 끝나고, ActionCommitted 태그가 안 빠져서 이동이 영구히
			 * 잠긴다. 막혀서 중단되는 경우도 정상 종료와 동일하게
			 * 마무리해야 한다.
			 */
			FinishDash();
			return;
		}
	}
	else if (bRotateAlongMovement && !WorldDirection.IsNearlyZero())
	{
		MovementComponent->UpdatedComponent->SetWorldRotation(AppliedRotation);
	}

	if (AccumulatedDistance >= TotalDistance - KINDA_SMALL_NUMBER)
	{
		FinishDash();
	}
}

void UAbilityTask_FollowDashSpline::OnDestroy(bool AbilityEnding)
{
	if (bPawnCollisionOverridden && DashCollisionComponent.IsValid())
	{
		DashCollisionComponent->SetCollisionResponseToChannel(
			ECC_Pawn,
			PreviousPawnCollisionResponse);
	}
	bPawnCollisionOverridden = false;
	DashCollisionComponent = nullptr;

	if (MovementComponent.IsValid())
	{
		MovementComponent->Velocity = FVector::ZeroVector;
	}

	Super::OnDestroy(AbilityEnding);
}

bool UAbilityTask_FollowDashSpline::BuildRuntimeSplineVariation(const USplineComponent* SourceSpline)
{
	if (!IsValid(SourceSpline))
	{
		return false;
	}

	const float SourceLength = SourceSpline->GetSplineLength();
	if (SourceLength <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	RuntimeSpline = NewObject<USplineComponent>(this, NAME_None, RF_Transient);
	if (!IsValid(RuntimeSpline))
	{
		return false;
	}

	RuntimeSpline->ClearSplinePoints(false);

	const int32 SampleCount = FMath::Clamp(DashPattern.VariationSampleCount, 4, 64);
	const float Frequency = FMath::Max(DashPattern.VariationFrequency, 0.25f);
	const float SideAmplitude = FMath::FRandRange(
		-DashPattern.HorizontalVariation,
		DashPattern.HorizontalVariation);
	const float VerticalAmplitude = FMath::FRandRange(
		-DashPattern.VerticalVariation,
		DashPattern.VerticalVariation);
	const float SideNoiseSeed = FMath::FRandRange(-10000.0f, 10000.0f);
	const float VerticalNoiseSeed = FMath::FRandRange(-10000.0f, 10000.0f);

	for (int32 PointIndex = 0; PointIndex < SampleCount; ++PointIndex)
	{
		const float Alpha = static_cast<float>(PointIndex) / static_cast<float>(SampleCount - 1);
		const float SourceDistance = SourceLength * Alpha;
		FVector LocalPosition = SourceSpline->GetLocationAtDistanceAlongSpline(
			SourceDistance,
			ESplineCoordinateSpace::Local);

		// The start is always fixed. Optionally fix the end as well so deformation
		// changes the route shape without changing its authored destination.
		const float Envelope = DashPattern.bPreserveSplineEnd
			? FMath::Sin(PI * Alpha)
			: FMath::Sin(0.5f * PI * Alpha);
		const float NoisePosition = Frequency * Alpha;
		const float SideNoise = FMath::PerlinNoise1D(SideNoiseSeed + NoisePosition);
		const float VerticalNoise = FMath::PerlinNoise1D(VerticalNoiseSeed + NoisePosition);
		LocalPosition.Y += Envelope * SideAmplitude * SideNoise;
		LocalPosition.Z += Envelope * VerticalAmplitude * VerticalNoise;

		RuntimeSpline->AddSplinePoint(LocalPosition, ESplineCoordinateSpace::Local, false);
		RuntimeSpline->SetSplinePointType(PointIndex, ESplinePointType::Curve, false);
	}

	RuntimeSpline->UpdateSpline();
	return RuntimeSpline->GetSplineLength() > KINDA_SMALL_NUMBER;
}

bool UAbilityTask_FollowDashSpline::BuildRuntimeSplineToTarget()
{
	const AActor* Target = TargetActor.Get();
	if (!IsValid(Target))
	{
		return false;
	}

	/*
	 * 도착 지점은 발동 시점의 타깃 위치로 한 번 정하고 끝이다.
	 *
	 * 이동 중에 갱신하면 타깃이 도망쳐도 계속 따라붙어서, 회피로 벗어날
	 * 방법이 사라진다. 빗나가는 것이 이 패턴의 카운터플레이다.
	 */
	const FVector TargetLocation = Target->GetActorLocation();

	FVector ToTarget = TargetLocation - DashStartLocation;
	ToTarget.Z = 0.0f;

	const FVector ApproachDirection = ToTarget.GetSafeNormal();
	if (ApproachDirection.IsNearlyZero())
	{
		// 타깃과 수직으로 겹쳐 있으면 접근 방향을 정할 수 없다.
		return false;
	}

	FVector EndWorld = TargetLocation - ApproachDirection * FMath::Max(DashPattern.StopDistance, 0.0f);
	EndWorld.Z = TargetLocation.Z + DashPattern.ArrivalHeightOffset;

	/*
	 * 이동 코드가 스플라인 포인트를 DashStartTransform으로 다시 월드화하므로
	 * 여기서는 로컬 공간으로 넣는다. 시작점은 항상 원점이다.
	 */
	const FVector LocalStart = FVector::ZeroVector;
	const FVector LocalEnd = DashStartTransform.InverseTransformPosition(EndWorld);

	const float TravelDistance = FVector::Dist(LocalStart, LocalEnd);
	if (TravelDistance <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	/*
	 * 아치는 거리에 비례하되 상한을 둔다. 짧은 거리에서 비율만 따르면
	 * 앞으로 나가는 것이 아니라 제자리에서 튀어오르는 모양이 된다.
	 */
	const float ArcHeight = FMath::Min(
		TravelDistance * FMath::Max(DashPattern.ArcHeightRatio, 0.0f),
		FMath::Max(DashPattern.MaxArcHeight, 0.0f));

	FVector LocalMid = (LocalStart + LocalEnd) * 0.5f;
	LocalMid.Z += ArcHeight;

	// 렌더링하지 않고 위치 계산에만 쓰므로 등록하지 않는다.
	RuntimeSpline = NewObject<USplineComponent>(this, NAME_None, RF_Transient);
	if (!IsValid(RuntimeSpline))
	{
		return false;
	}

	RuntimeSpline->ClearSplinePoints(false);
	RuntimeSpline->AddSplinePoint(LocalStart, ESplineCoordinateSpace::Local, false);
	RuntimeSpline->AddSplinePoint(LocalMid, ESplineCoordinateSpace::Local, false);
	RuntimeSpline->AddSplinePoint(LocalEnd, ESplineCoordinateSpace::Local, false);

	// 시작과 정점은 자동 탄젠트로 부드럽게 잇는다.
	RuntimeSpline->SetSplinePointType(0, ESplinePointType::CurveClamped, false);
	RuntimeSpline->SetSplinePointType(1, ESplinePointType::CurveClamped, false);

	/*
	 * 도착 탄젠트를 타깃 쪽으로 눕힌다. 자동 탄젠트에 맡기면 아치 정점에서
	 * 내려오는 각 그대로 꽂혀서, 정면을 가정한 공격 모션과 방향이 어긋난다.
	 *
	 * 지정한 탄젠트가 UpdateSpline에서 다시 계산되지 않도록 이 점만
	 * CurveCustomTangent로 둔다.
	 */
	RuntimeSpline->SetSplinePointType(2, ESplinePointType::CurveCustomTangent, false);

	const FVector LocalApproachDirection =
		DashStartTransform.InverseTransformVectorNoScale(ApproachDirection);

	RuntimeSpline->SetTangentAtSplinePoint(
		2,
		LocalApproachDirection * TravelDistance * DashPattern.ArrivalTangentScale,
		ESplineCoordinateSpace::Local,
		false);

	RuntimeSpline->UpdateSpline();
	return RuntimeSpline->GetSplineLength() > KINDA_SMALL_NUMBER;
}

float UAbilityTask_FollowDashSpline::EvaluateSpeedMultiplier(const float TimeAlpha) const
{
	const float ClampedTimeAlpha = FMath::Clamp(TimeAlpha, 0.0f, 1.0f);
	const float CurveSpeedMultiplier = IsValid(DashPattern.MovementCurve)
		? FMath::Max(DashPattern.MovementCurve->GetFloatValue(ClampedTimeAlpha), 0.0f)
		: 1.0f;

	// Preserve the incoming speed at the first frame, then fade that contribution
	// out while the authored speed curve takes control.
	const float InheritedSpeedMultiplier = InitialSpeedMultiplier * (1.0f - ClampedTimeAlpha);
	return FMath::Max(CurveSpeedMultiplier, InheritedSpeedMultiplier);
}

float UAbilityTask_FollowDashSpline::CalculateAverageSpeedMultiplier() const
{
	constexpr int32 SampleCount = 64;
	float IntegratedMultiplier = 0.0f;
	float PreviousMultiplier = EvaluateSpeedMultiplier(0.0f);

	for (int32 SampleIndex = 1; SampleIndex <= SampleCount; ++SampleIndex)
	{
		const float TimeAlpha = static_cast<float>(SampleIndex) / SampleCount;
		const float CurrentMultiplier = EvaluateSpeedMultiplier(TimeAlpha);
		IntegratedMultiplier += 0.5f * (PreviousMultiplier + CurrentMultiplier) / SampleCount;
		PreviousMultiplier = CurrentMultiplier;
	}

	return FMath::Max(IntegratedMultiplier, KINDA_SMALL_NUMBER);
}

void UAbilityTask_FollowDashSpline::FinishDash()
{
	if (MovementComponent.IsValid())
	{
		MovementComponent->Velocity = FVector::ZeroVector;
	}

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnFinished.Broadcast();
	}

	EndTask();
}

