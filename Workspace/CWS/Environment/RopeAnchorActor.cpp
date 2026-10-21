#include "RopeAnchorActor.h"
#include "EnvironmentCharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "Components/BoxComponent.h"
#include "ProceduralMeshComponent.h"

ARopeAnchorActor::ARopeAnchorActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics; // Very important for 1-frame lag prevention
	bReplicates = false;
	DefaultRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultRoot"));
	RootComponent = DefaultRoot;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	TriggerBox->SetupAttachment(RootComponent);
	TriggerBox->SetBoxExtent(FVector(100.f, 100.f, 200.f));
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));

	ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralMesh"));
	ProceduralMesh->SetupAttachment(RootComponent);
	ProceduralMesh->bUseAsyncCooking = false;
	ProceduralMesh->SetCastShadow(true);
	
	GrabbingCharacter = nullptr;
	GrabIndexK = -1;
}

void ARopeAnchorActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (TriggerBox)
	{
		// Z Extent is half of RopeLength.
		float HalfHeight = RopeLength * 0.5f;
		TriggerBox->SetBoxExtent(FVector(TriggerThickness.X, TriggerThickness.Y, HalfHeight));

		// 트리거 중심을 아래로 HalfHeight만큼 내려서 시작점이 앵커 원점과 정확히 일치하게 함
		TriggerBox->SetRelativeLocation(FVector(0.f, 0.f, -HalfHeight));
	}
}

void ARopeAnchorActor::BeginPlay()
{
	Super::BeginPlay();

	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ARopeAnchorActor::OnOverlapBegin);

	// Initialize Verlet Particles
	NumSegments = FMath::Max(2, NumSegments);
	SegmentLength = RopeLength / (NumSegments - 1);
	
	Particles.SetNumUninitialized(NumSegments);
	OldParticles.SetNumUninitialized(NumSegments);
	
	FVector StartPos = GetActorLocation();
	for (int32 i = 0; i < NumSegments; i++)
	{
		Particles[i] = StartPos - FVector(0, 0, i * SegmentLength);
		OldParticles[i] = Particles[i];
	}

	// Initialize PMC Buffers
	Sides = FMath::Max(3, Sides);
	int32 TotalVertices = NumSegments * Sides;
	int32 TotalTriangles = (NumSegments - 1) * Sides * 6;
	
	Vertices.SetNumUninitialized(TotalVertices);
	Normals.SetNumUninitialized(TotalVertices);
	UVs.SetNumUninitialized(TotalVertices);
	Triangles.SetNumUninitialized(TotalTriangles);

	// Precalculate Triangles (Index Buffer) & UVs
	int32 TriIndex = 0;
	for (int32 i = 0; i < TotalVertices; i++)
	{
		Vertices[i] = FVector::ZeroVector; // 초기 프레임 쓰레기값 방지
		Normals[i] = FVector::UpVector;
	}

	for (int32 i = 0; i < NumSegments; i++)
	{
		float V = (float)i / (NumSegments - 1);
		for (int32 s = 0; s < Sides; s++)
		{
			int32 VIndex = i * Sides + s;
			float U = (float)s / Sides;
			UVs[VIndex] = FVector2D(U, V);
			
			if (i < NumSegments - 1)
			{
				int32 NextS = (s + 1) % Sides;
				
				int32 V1 = VIndex;
				int32 V2 = i * Sides + NextS;
				int32 V3 = (i + 1) * Sides + s;
				int32 V4 = (i + 1) * Sides + NextS;

				// Triangle 1
				Triangles[TriIndex++] = V1;
				Triangles[TriIndex++] = V3;
				Triangles[TriIndex++] = V4;
				
				// Triangle 2
				Triangles[TriIndex++] = V1;
				Triangles[TriIndex++] = V4;
				Triangles[TriIndex++] = V2;
			}
		}
	}
	
	// Create the initial mesh section so UpdateMeshSection works later
	ProceduralMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, TArray<FLinearColor>(), TArray<FProcMeshTangent>(), false);

	if (RopeMaterial)
	{
		ProceduralMesh->SetMaterial(0, RopeMaterial);
	}
}

void ARopeAnchorActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	SimulateVerletPhysics(DeltaTime);
	UpdateMeshExtrusion();
}

void ARopeAnchorActor::SimulateVerletPhysics(float DeltaTime)
{
	float FixedDT = FMath::Min(DeltaTime, 0.0333f);
	FVector Gravity = FVector(0, 0, -980.f);
	float Damping = 0.99f;
	
	// 1. Integration
	for (int32 i = 0; i < NumSegments; i++)
	{
		// Pin 0 (Ceiling)
		if (i == 0)
		{
			Particles[i] = GetActorLocation();
			OldParticles[i] = Particles[i];
			continue;
		}
		
		// Pin K (Hand)
		if (GrabbingCharacter && i == GrabIndexK)
		{
			// Character's capsule location + local offset for hand (이제 HandOffset으로 앞뒤/좌우/높이 모두 조절 가능)
			FVector TargetLoc = GrabbingCharacter->GetActorLocation() 
								+ (GrabbingCharacter->GetActorForwardVector() * HandOffset.X)
								+ (GrabbingCharacter->GetActorRightVector() * HandOffset.Y)
								+ (GrabbingCharacter->GetActorUpVector() * HandOffset.Z);
			Particles[i] = TargetLoc;
			OldParticles[i] = Particles[i];
			continue;
		}

		FVector Velocity = (Particles[i] - OldParticles[i]) * Damping;
		OldParticles[i] = Particles[i];
		Particles[i] += Velocity + (Gravity * FixedDT * FixedDT);
	}

	// 2. Constraints (Jacobian Relaxation)
	for (int32 Iter = 0; Iter < ConstraintIterations; Iter++)
	{
		for (int32 i = 0; i < NumSegments - 1; i++)
		{
			FVector Delta = Particles[i + 1] - Particles[i];
			float Dist = Delta.Size();
			if (Dist > KINDA_SMALL_NUMBER)
			{
				float Error = (Dist - SegmentLength) / Dist;
				FVector Correction = Delta * 0.5f * Error;
				
				if (i != 0 && !(GrabbingCharacter && i == GrabIndexK))
				{
					Particles[i] += Correction;
				}
				if (!(GrabbingCharacter && (i + 1) == GrabIndexK))
				{
					Particles[i + 1] -= Correction;
				}
			}
		}
	}
}

void ARopeAnchorActor::UpdateMeshExtrusion()
{
	const float AngleStep = 2.0f * PI / Sides;
	const FTransform& WorldToLocal = ProceduralMesh->GetComponentTransform().Inverse();

	for (int32 i = 0; i < NumSegments; i++)
	{
		FVector Direction;
		if (i < NumSegments - 1)
		{
			Direction = (Particles[i + 1] - Particles[i]).GetSafeNormal();
		}
		else
		{
			Direction = (Particles[i] - Particles[i - 1]).GetSafeNormal();
		}

		// Prevent twisting
		FVector Up = FVector::UpVector;
		if (FMath::Abs(FVector::DotProduct(Direction, Up)) > 0.99f)
		{
			Up = FVector::ForwardVector;
		}
		FVector Right = FVector::CrossProduct(Up, Direction).GetSafeNormal();
		Up = FVector::CrossProduct(Direction, Right).GetSafeNormal();

		for (int32 s = 0; s < Sides; s++)
		{
			float Angle = s * AngleStep;
			float CosA = FMath::Cos(Angle);
			float SinA = FMath::Sin(Angle);

			// Radial vector outward from center (Smooth Shading Normal)
			FVector RadialDir = (Right * CosA + Up * SinA).GetSafeNormal();
			
			int32 VIndex = i * Sides + s;
			FVector WorldPos = Particles[i] + RadialDir * RopeThickness;
			
			// ProceduralMeshComponent는 로컬 좌표계를 사용하므로 월드 좌표를 로컬로 변환
			Vertices[VIndex] = WorldToLocal.TransformPosition(WorldPos);
			Normals[VIndex] = WorldToLocal.TransformVector(RadialDir);
		}
	}
	
	ProceduralMesh->UpdateMeshSection_LinearColor(0, Vertices, Normals, UVs, TArray<FLinearColor>(), TArray<FProcMeshTangent>(), false);
}

void ARopeAnchorActor::GrabRope(ACharacter* InCharacter, float InGrabRatio)
{
	GrabbingCharacter = InCharacter;
	
	if (GrabbingCharacter)
	{
		AddTickPrerequisiteActor(GrabbingCharacter);
		
		float ClampedRatio = FMath::Clamp(InGrabRatio, 0.0f, 1.0f);
		GrabIndexK = FMath::RoundToInt(ClampedRatio * (NumSegments - 1));
	}
}

void ARopeAnchorActor::ReleaseRope(ACharacter* InCharacter)
{
	if (GrabbingCharacter == InCharacter)
	{
		RemoveTickPrerequisiteActor(GrabbingCharacter);
		GrabbingCharacter = nullptr;
		GrabIndexK = -1;
	}
}

void ARopeAnchorActor::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
									  UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, 
									  bool bFromSweep, const FHitResult& SweepResult)
{
	if (ACharacter* Character = Cast<ACharacter>(OtherActor))
	{
		if (UEnvironmentCharacterMovementComponent* EnvMovement = Cast<UEnvironmentCharacterMovementComponent>(Character->GetCharacterMovement()))
		{
			if (EnvMovement->MovementMode != MOVE_Custom || EnvMovement->CustomMovementMode != CMOVE_RopeSwinging)
			{
				EnvMovement->BeginRopeSwing(this, RopeLength);
			}
		}
	}
}

FVector ARopeAnchorActor::GetGrabbedParticleLocation() const
{
	if (Particles.IsValidIndex(GrabIndexK))
	{
		return Particles[GrabIndexK];
	}
	return GetActorLocation(); // 기본값
}
