#include "BossRingWave.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"
#include "GameCore/Ability/DamageSystemLibrary.h"
#include "GameCore/Character/BaseCharacter.h"
#include "GameCore/Combat/MonsterDamageLibrary.h"
#include "GameCore/Tags/GameplayTagSystem.h"
#include "GameFramework/Character.h"

ABossRingWave::ABossRingWave()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);
	RingRoot = CreateDefaultSubobject<USceneComponent>(TEXT("RingRoot"));
	SetRootComponent(RingRoot);
	RingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RingMesh"));
	RingMesh->SetupAttachment(RingRoot);
	RingMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ABossRingWave::InitializeRing(EBossRingWavePlane InPlane, float InChargeRadius, float InMaxRadius,
	float InChargeExpansionSpeed, float InLaunchExpansionSpeed, float InBandHalfWidth, float InPlaneHalfThickness,
	float InFacingYaw, const FGameplayTag& InAttackTag, TSubclassOf<UGameplayEffect> InDamageEffectClass)
{
	Plane = InPlane;
	FacingYaw = InFacingYaw;
	bUseExplicitRotation = false;
	UpdatePlaneRotation();
	CurrentRadius = 0.0f;
	ChargeRadius = FMath::Max(0.0f, InChargeRadius);
	MaxRadius = FMath::Max(ChargeRadius, InMaxRadius);
	ChargeExpansionSpeed = FMath::Max(0.0f, InChargeExpansionSpeed);
	LaunchExpansionSpeed = FMath::Max(0.0f, InLaunchExpansionSpeed);
	BandHalfWidth = FMath::Max(0.0f, InBandHalfWidth);
	PlaneHalfThickness = FMath::Max(0.0f, InPlaneHalfThickness);
	AttackTag = InAttackTag;
	DamageEffectClass = InDamageEffectClass;
	bCharging = true;
	bLaunched = false;
	bHoldAtMaxRadius = false;
	bRetracting = false;
	RetractSpeed = 0.0f;
	ExpandedDamagedActors.Reset();
	RetractedDamagedActors.Reset();
	UpdateVisual();
}

void ABossRingWave::InitializeRingWithRotation(const EBossRingWavePlane InPlane, const FRotator& InRotation,
	const float InChargeRadius, const float InMaxRadius, const float InChargeExpansionSpeed,
	const float InLaunchExpansionSpeed, const float InBandHalfWidth, const float InPlaneHalfThickness,
	const FGameplayTag& InAttackTag, TSubclassOf<UGameplayEffect> InDamageEffectClass)
{
	InitializeRing(InPlane, InChargeRadius, InMaxRadius, InChargeExpansionSpeed, InLaunchExpansionSpeed,
		InBandHalfWidth, InPlaneHalfThickness, 0.0f, InAttackTag, InDamageEffectClass);
	bUseExplicitRotation = true;
	ExplicitRotation = InRotation;
	UpdatePlaneRotation();
}

void ABossRingWave::InitializeGroundSlamRing(float InMaxRadius, float InExpandSpeed, float InRetractSpeed,
	float InBandHalfWidth, float InPlaneHalfThickness, const FGameplayTag& InAttackTag,
	TSubclassOf<UGameplayEffect> InDamageEffectClass)
{
	InitializeRing(EBossRingWavePlane::Horizontal, 0.0f, InMaxRadius, 0.0f, InExpandSpeed,
		InBandHalfWidth, InPlaneHalfThickness, 0.0f, InAttackTag, InDamageEffectClass);
	bCharging = false;
	bLaunched = true;
	bHoldAtMaxRadius = true;
	RetractSpeed = FMath::Max(0.0f, InRetractSpeed);
}

void ABossRingWave::LaunchRing() { bLaunched = true; }

void ABossRingWave::BeginRetract()
{
	if (!HasAuthority()) return;
	bCharging = false;
	bLaunched = false;
	bRetracting = RetractSpeed > 0.0f;
}

void ABossRingWave::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABossRingWave, bCharging);
	DOREPLIFETIME(ABossRingWave, bLaunched);
	DOREPLIFETIME(ABossRingWave, bHoldAtMaxRadius);
	DOREPLIFETIME(ABossRingWave, bRetracting);
	DOREPLIFETIME(ABossRingWave, Plane);
	DOREPLIFETIME(ABossRingWave, FacingYaw);
	DOREPLIFETIME(ABossRingWave, bUseExplicitRotation);
	DOREPLIFETIME(ABossRingWave, ExplicitRotation);
	DOREPLIFETIME(ABossRingWave, CurrentRadius);
	DOREPLIFETIME(ABossRingWave, ChargeRadius);
	DOREPLIFETIME(ABossRingWave, MaxRadius);
	DOREPLIFETIME(ABossRingWave, ChargeExpansionSpeed);
	DOREPLIFETIME(ABossRingWave, LaunchExpansionSpeed);
	DOREPLIFETIME(ABossRingWave, RetractSpeed);
	DOREPLIFETIME(ABossRingWave, BandHalfWidth);
	DOREPLIFETIME(ABossRingWave, PlaneHalfThickness);
}

void ABossRingWave::OnRep_RingState()
{
	UpdatePlaneRotation();
	UpdateVisual();
}

void ABossRingWave::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const bool bWasLaunched = bLaunched;
	const bool bWasRetracting = bRetracting;
	const float PreviousRadius = CurrentRadius;
	bool bFinishedRetracting = false;
	if (bCharging)
	{
		CurrentRadius = FMath::Min(CurrentRadius + ChargeExpansionSpeed * DeltaSeconds, ChargeRadius);
		if (CurrentRadius >= ChargeRadius) bCharging = false;
	}
	else if (bLaunched)
	{
		CurrentRadius += LaunchExpansionSpeed * DeltaSeconds;
		if (CurrentRadius >= MaxRadius)
		{
			CurrentRadius = MaxRadius;
			if (bHoldAtMaxRadius) bLaunched = false;
		}
	}
	else if (bRetracting)
	{
		CurrentRadius = FMath::Max(0.0f, CurrentRadius - RetractSpeed * DeltaSeconds);
		if (CurrentRadius <= 0.0f)
		{
			bFinishedRetracting = true;
		}
	}
	else return;
	UpdateVisual();
	// LaunchExpansionSpeed가 BandHalfWidth보다 큰 경우에도, 이전 프레임부터
	// 현재 프레임까지 지나간 모든 위치를 한 번의 판정으로 포함한다.
	if ((bWasLaunched || bWasRetracting) && HasAuthority()) ApplyRingDamage(PreviousRadius, bWasRetracting);
	if (bFinishedRetracting)
	{
		Destroy();
		return;
	}
	if (bLaunched && CurrentRadius >= MaxRadius && !bHoldAtMaxRadius) Destroy();
}

void ABossRingWave::UpdateVisual()
{
	const float Diameter = CurrentRadius * 2.0f;
	RingMesh->SetRelativeScale3D(FVector(Diameter / PlaneBaseSize, Diameter / PlaneBaseSize, 1.0f));
}

void ABossRingWave::UpdatePlaneRotation()
{
	if (bUseExplicitRotation)
	{
		SetActorRotation(ExplicitRotation);
		return;
	}
	// 기본 Plane은 XY 평면이다. 세로 링은 보스 정면(X)과 높이(Z)를 포함해야 한다.
	const FQuat FacingRotation(FVector::UpVector, FMath::DegreesToRadians(FacingYaw));
	const FQuat PlaneRotation = Plane == EBossRingWavePlane::Vertical
		? FQuat(FVector::ForwardVector, HALF_PI)
		: FQuat::Identity;
	SetActorRotation((FacingRotation * PlaneRotation).Rotator());
}

void ABossRingWave::ApplyRingDamage(const float PreviousRadius, const bool bRetractingPass)
{
	const float InnerRadius = FMath::Max(0.0f, FMath::Min(PreviousRadius, CurrentRadius) - BandHalfWidth);
	const float OuterRadius = FMath::Max(PreviousRadius, CurrentRadius) + BandHalfWidth;
	TSet<TObjectPtr<AActor>>& DamagedActors = bRetractingPass ? RetractedDamagedActors : ExpandedDamagedActors;

	// 대상 후보는 Ground Slam 등과 동일한 공용 TraceAttack Sweep으로 수집한다.
	// 원형 띠 자체는 기본 콜리전 프리미티브로 만들 수 없으므로, 아래에서만 띠의
	// 정밀 판정을 수행한다.
	TMap<AActor*, FHitResult> CandidateHits;
	MonsterDamageLibrary::GatherTargets(
		this,
		GetActorLocation(),
		GetActorLocation(),
		OuterRadius,
		{ this, GetOwner() },
		CandidateHits);
	if (bDebugDrawDamageQuery)
	{
		const FVector Origin = GetActorLocation();
		const FVector RingAxisX = GetActorForwardVector();
		const FVector RingAxisY = GetActorRightVector();
		// 노랑: TraceAttack 후보 수집 구체 / 하늘색: 실제 원형 띠의 안쪽·바깥쪽 경계.
		DrawDebugSphere(GetWorld(), Origin, OuterRadius, 24, FColor::Yellow, false, DebugDrawDuration, 0, 1.0f);
		DrawDebugCircle(GetWorld(), Origin, OuterRadius, 64, FColor::Cyan, false, DebugDrawDuration, 0, 2.0f, RingAxisX, RingAxisY);
		if (InnerRadius > 0.0f)
		{
			DrawDebugCircle(GetWorld(), Origin, InnerRadius, 64, FColor::Cyan, false, DebugDrawDuration, 0, 2.0f, RingAxisX, RingAxisY);
		}
	}

	int32 RingHitCount = 0;

	for (const TPair<AActor*, FHitResult>& Candidate : CandidateHits)
	{
		ACharacter* Target = Cast<ACharacter>(Candidate.Key);
		if (!IsValid(Target) || DamagedActors.Contains(Target)) continue;
		const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
		if (!IsValid(ASC) || !ASC->HasMatchingGameplayTag(GameplayTagSystem::Character_Player)) continue;

		const UCapsuleComponent* TargetCapsule = Target->GetCapsuleComponent();
		if (!IsValid(TargetCapsule)) continue;
		const FVector TargetLocation = TargetCapsule->GetComponentLocation();
		// RingMesh는 시각용으로 커지므로, 스케일이 섞인 Transform 역변환을 쓰지 않는다.
		// 루트의 위치와 회전만으로 월드 좌표를 링 평면 좌표로 바꾼다.
		const FQuat RingRotation = GetActorQuat();
		const FVector Local = RingRotation.UnrotateVector(TargetLocation - GetActorLocation());
		const float DistanceOnPlane = FVector2D(Local.X, Local.Y).Size();
		const FVector RadialDirection = DistanceOnPlane > KINDA_SMALL_NUMBER
			? FVector(Local.X / DistanceOnPlane, Local.Y / DistanceOnPlane, 0.0f)
			: FVector::ForwardVector;
		const FVector CapsuleAxisLocal = RingRotation.UnrotateVector(TargetCapsule->GetUpVector()).GetSafeNormal();
		float CapsuleRadius = 0.0f;
		float CapsuleHalfHeight = 0.0f;
		TargetCapsule->GetScaledCapsuleSize(CapsuleRadius, CapsuleHalfHeight);
		if (bDebugDrawDamageQuery)
		{
			// 초록: TraceAttack 및 Player 태그를 통과한 후보 캡슐.
			DrawDebugCapsule(GetWorld(), TargetLocation, CapsuleHalfHeight, CapsuleRadius,
				TargetCapsule->GetComponentQuat(), FColor::Green, false, DebugDrawDuration, 0, 1.5f);
		}

		const auto GetCapsuleExtentOnAxis = [CapsuleRadius, CapsuleHalfHeight, &CapsuleAxisLocal](const FVector& Axis)
		{
			const float AxisDot = FMath::Abs(FVector::DotProduct(CapsuleAxisLocal, Axis));
			return CapsuleHalfHeight * AxisDot + CapsuleRadius * FMath::Sqrt(FMath::Max(0.0f, 1.0f - AxisDot * AxisDot));
		};
		const float RadialCapsuleExtent = GetCapsuleExtentOnAxis(RadialDirection);
		const float PlaneNormalCapsuleExtent = GetCapsuleExtentOnAxis(FVector::UpVector);

		if (FMath::Abs(Local.Z) > PlaneHalfThickness + PlaneNormalCapsuleExtent
			|| DistanceOnPlane + RadialCapsuleExtent < InnerRadius
			|| DistanceOnPlane - RadialCapsuleExtent > OuterRadius) continue;
		if (bDebugDrawDamageQuery)
		{
			// 빨강: 실제 이번 프레임 원형 띠에 닿아 데미지를 적용할 캡슐.
			DrawDebugCapsule(GetWorld(), TargetLocation, CapsuleHalfHeight, CapsuleRadius,
				TargetCapsule->GetComponentQuat(), FColor::Red, false, DebugDrawDuration, 0, 2.5f);
		}
		DamagedActors.Add(Target);
		++RingHitCount;
		FDamagePayload Payload = MonsterDamageLibrary::MakeAttackPayload(AttackTag, DamageEffectClass);
		FHitResult Hit;
		Hit.HitObjectHandle = FActorInstanceHandle(Target);
		Hit.Location = TargetLocation;
		Hit.ImpactPoint = Hit.Location;
		Payload.HitResult = Hit;
		UDamageSystemLibrary::LogDamageApplied(GetOwner(), Target, TEXT("BossRingWave"));
		UDamageSystemLibrary::ApplyGameDamage(this, GetOwner(), Payload);
	}

	if (bDebugDrawDamageQuery)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BossRingDebug] TraceAttackPlayerCandidates=%d RingHits=%d PreviousRadius=%.1f CurrentRadius=%.1f Inner=%.1f Outer=%.1f"),
			CandidateHits.Num(), RingHitCount, PreviousRadius, CurrentRadius, InnerRadius, OuterRadius);
	}
}
