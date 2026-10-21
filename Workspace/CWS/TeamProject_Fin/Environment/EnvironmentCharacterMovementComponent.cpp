#include "EnvironmentCharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "RopeAnchorActor.h"
#include "GameCore/Character/PlayerCharacter.h"
#include "Kismet/GameplayStatics.h"

UEnvironmentCharacterMovementComponent::UEnvironmentCharacterMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CurrentRopeAnchor = nullptr;
	RopeLength = 0.0f;
	CurrentTerminalVelocity = 0.0f;
	CurrentTerminalVelocity = 0.0f;
	ActiveWindForce = FVector::ZeroVector;
}

void UEnvironmentCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	// The saved move flags can be used to synchronize custom states if needed.
	// For simple boolean flags, FLAG_Custom_0 to FLAG_Custom_3 are available.
	// bSavedIsRopeSwinging could correspond to FLAG_Custom_0
	bool bWasRopeSwinging = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
	
	if (bWasRopeSwinging && CustomMovementMode != CMOVE_RopeSwinging)
	{
		// [위험 방지] 앵커가 유효할 때만 스윙 모드로 복구합니다.
		if (CurrentRopeAnchor != nullptr)
		{
			SetMovementMode(MOVE_Custom, CMOVE_RopeSwinging);
		}
		else
		{
			// 앵커가 유효하지 않으면 롤백 과정에서 안전하게 공중 상태로 유지
			SetMovementMode(MOVE_Falling);
		}
	}
	// 클라이언트가 밧줄을 놨다(bWasRopeSwinging == false)고 서버에 패킷을 보냈는데, 서버는 아직 매달려 있는 경우
	else if (!bWasRopeSwinging && CustomMovementMode == CMOVE_RopeSwinging)
	{
		DismountRope(); // 서버도 클라이언트의 입력에 맞춰 밧줄을 놓습니다.
	}
}

class FNetworkPredictionData_Client* UEnvironmentCharacterMovementComponent::GetPredictionData_Client() const
{
	if (ClientPredictionData == nullptr)
	{
		UEnvironmentCharacterMovementComponent* MutableThis = const_cast<UEnvironmentCharacterMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_Environment(*this);
	}
	return ClientPredictionData;
}

void UEnvironmentCharacterMovementComponent::BeginRopeSwing(AActor* InAnchor, float InRopeLength)
{
	if (!InAnchor || !CharacterOwner) return;

	CurrentRopeAnchor = InAnchor;
	RopeLength = InRopeLength;

	// Reset velocity radially relative to anchor
	FVector ToAnchor = (CurrentRopeAnchor->GetActorLocation() - CharacterOwner->GetActorLocation()).GetSafeNormal();
	FVector RadialVel = ToAnchor * FVector::DotProduct(Velocity, ToAnchor);
	
	// Keep only tangential velocity upon grab
	if (FVector::DotProduct(Velocity, ToAnchor) < 0) 
	{
		Velocity -= RadialVel;
	}

	UpdateComponentVelocity();

	SetMovementMode(MOVE_Custom, CMOVE_RopeSwinging);

	// 시각적 로프 핀 고정 (베를레)
	if (ARopeAnchorActor* AnchorActor = Cast<ARopeAnchorActor>(CurrentRopeAnchor))
	{
		// 앵커와 캡슐 사이의 거리 비율을 기반으로 몇 번째 파티클(K)을 잡을지 계산
		float DistToAnchor = (CurrentRopeAnchor->GetActorLocation() - CharacterOwner->GetActorLocation()).Size();
		float GrabRatio = FMath::Clamp(DistToAnchor / RopeLength, 0.0f, 1.0f);
		
		AnchorActor->GrabRope(CharacterOwner, GrabRatio);

		// 서버일 경우 멀티플레이어 동기화를 위해 변수 갱신
		if (CharacterOwner->HasAuthority())
		{
			if (APlayerCharacter* PC = Cast<APlayerCharacter>(CharacterOwner))
			{
				PC->CurrentSwingingAnchor = AnchorActor;
			}
		}

		// 로컬 플레이어 및 서버인 경우 카메라 줌 변경 및 무기 숨김
		if (APlayerCharacter* PC = Cast<APlayerCharacter>(CharacterOwner))
		{
			PC->SetRopeCameraMode(true);
			PC->SetWeaponHidden(true);
		}
	}
}

void UEnvironmentCharacterMovementComponent::DismountRope()
{
	if (CustomMovementMode != CMOVE_RopeSwinging) return;

	FVector LaunchVelocity = Velocity;
	
	// 기획 의도에 맞게, 비정상적인 Z(위쪽) 보너스 점프를 완전히 제거하고 진행 방향(관성)으로만 밀어줍니다.
	FVector ForwardDir = Velocity.GetSafeNormal();
	FVector ForwardBonus = ForwardDir * DismountForwardBoost;
	
	LaunchVelocity += ForwardBonus;

	// 매달려있던 베를레 입자의 핀 고정을 해제합니다.
	if (ARopeAnchorActor* AnchorActor = Cast<ARopeAnchorActor>(CurrentRopeAnchor))
	{
		AnchorActor->ReleaseRope(CharacterOwner);

		// 서버일 경우 멀티플레이어 동기화 변수 초기화
		if (CharacterOwner->HasAuthority())
		{
			if (APlayerCharacter* PC = Cast<APlayerCharacter>(CharacterOwner))
			{
				PC->CurrentSwingingAnchor = nullptr;
			}
		}

		// 로컬 플레이어 및 서버인 경우 카메라 원상 복구 및 무기 다시 표시
		if (APlayerCharacter* PC = Cast<APlayerCharacter>(CharacterOwner))
		{
			PC->SetRopeCameraMode(false);
			PC->SetWeaponHidden(false);
		}
	}

	CurrentRopeAnchor = nullptr; // 앵커 참조 해제
	SetMovementMode(MOVE_Falling);
	Velocity = LaunchVelocity;
}

void UEnvironmentCharacterMovementComponent::AddActiveWindForce(const FVector& InWindVelocity, float InTerminalVelocity)
{
	ActiveWindForce += InWindVelocity;
	CurrentTerminalVelocity = FMath::Max(CurrentTerminalVelocity, InTerminalVelocity); // Use largest restriction
}

void UEnvironmentCharacterMovementComponent::RemoveActiveWindForce(const FVector& InWindVelocity, float InTerminalVelocity)
{
	ActiveWindForce -= InWindVelocity;
	// 만약 모든 바람 구역에서 벗어났다면(오차 허용) 터미널 벨로시티 초기화
	if (ActiveWindForce.IsNearlyZero(0.1f))
	{
		ActiveWindForce = FVector::ZeroVector;
		CurrentTerminalVelocity = 0.0f;
	}
}

float UEnvironmentCharacterMovementComponent::GetSwingPhase() const
{
	if (MovementMode != MOVE_Custom || CustomMovementMode != CMOVE_RopeSwinging || !CharacterOwner)
	{
		return 0.0f; // 기본 중립 반환
	}

	FVector ForwardVector = CharacterOwner->GetActorForwardVector();
	float ForwardVelocityMag = FVector::DotProduct(Velocity, ForwardVector);
	
	// -1.0 ~ 1.0 정규화
	return FMath::Clamp(ForwardVelocityMag / MaxSwingSpeed, -1.0f, 1.0f);
}

void UEnvironmentCharacterMovementComponent::PhysCustom(float deltaTime, int32 Iterations)
{
	if (CustomMovementMode == CMOVE_RopeSwinging)
	{
		PhysRopeSwinging(deltaTime, Iterations);
	}
	else
	{
		Super::PhysCustom(deltaTime, Iterations);
	}
}

void UEnvironmentCharacterMovementComponent::PerformMovement(float DeltaTime)
{
	// 일반 상태(걷기, 낙하 등)에서 활성화된 바람 가속도 적용
	if (MovementMode != MOVE_Custom && !ActiveWindForce.IsNearlyZero())
	{
		// 걷기 상태인데 위로 강하게 띄우는 바람(중력 극복)을 맞았다면 공중 상태로 강제 전환
		if (MovementMode == MOVE_Walking && ActiveWindForce.Z > 0.0f)
		{
			// 중력(음수)보다 더 큰 양수의 바람이라면 바닥에서 뗌
			if (ActiveWindForce.Z > FMath::Abs(GetGravityZ()) * 0.5f)
			{
				SetMovementMode(MOVE_Falling);
			}
		}

		Velocity += ActiveWindForce * DeltaTime;

		// Terminal velocity clamping (바람 상승 최대 속도 제한)
		if (CurrentTerminalVelocity > 0.0f && Velocity.Z > CurrentTerminalVelocity)
		{
			Velocity.Z = CurrentTerminalVelocity;
		}
	}

	Super::PerformMovement(DeltaTime);
	// 더이상 프레임 끝부분에서 바람을 초기화하지 않습니다! (Overlap 상태 기반으로 변경됨)
}

void UEnvironmentCharacterMovementComponent::PhysRopeSwinging(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}

	if (!CurrentRopeAnchor)
	{
		SetMovementMode(MOVE_Falling);
		return;
	}

	RestorePreAdditiveRootMotionVelocity();

	FVector AnchorLoc = CurrentRopeAnchor->GetActorLocation();
	FVector CurrentLoc = UpdatedComponent->GetComponentLocation();
	FVector ToAnchor = AnchorLoc - CurrentLoc;
	float Dist = ToAnchor.Size();
	FVector AnchorToChar = -ToAnchor.GetSafeNormal();
	
	// 최대 각도를 더 보수적으로 0(수평선)으로 잡아 시작점보다 절대 위로 가지 않게 막습니다.
	bool bIsOverAngleLimit = (AnchorToChar.Z > 0.0f);

	// Apply gravity
	float GravityZ = GetGravityZ();
	if (bIsOverAngleLimit)
	{
		GravityZ *= 2.0f; // 묵직한 추락 감각
	}
	Velocity.Z += GravityZ * deltaTime;

	// Move에서 W/S 축만 전달한 카메라 기준 입력을 사용한다.
	const FVector InputVector = Acceleration.GetSafeNormal2D();

	if (!InputVector.IsNearlyZero())
	{
		// 현재 속도 대신 입력을 로프의 접선 평면에 투영해 정지 출발과 방향 전환을 허용한다.
		// 투영 결과를 정규화하지 않아 로프와 나란한 입력의 추진력이 커지지 않게 한다.
		const FVector TangentialInput = FVector::VectorPlaneProject(InputVector, AnchorToChar);
		const bool bBlockPumping = bIsOverAngleLimit && (Velocity.Z > 0.f);

		if (!bBlockPumping)
		{
			Velocity += TangentialInput * (SwingPumpStrength * deltaTime);
		}
	}

	// Terminal velocity clamping (for wind upward limit)
	if (CurrentTerminalVelocity > 0.0f && Velocity.Z > CurrentTerminalVelocity)
	{
		Velocity.Z = CurrentTerminalVelocity;
	}

	// Compute step
	FVector Adjusted = Velocity * deltaTime;
	FHitResult Hit(1.f);
	SafeMoveUpdatedComponent(Adjusted, CharacterOwner->GetActorRotation(), true, Hit);

	if (Hit.Time < 1.f)
	{
		// Hit something, resolve penetration
		HandleImpact(Hit, deltaTime, Adjusted);
		SlideAlongSurface(Adjusted, (1.f - Hit.Time), Hit.Normal, Hit, true);
	}

	// Enforce Constraint (Length Clamping)
	if (Dist > RopeLength)
	{
		FVector Correction = ToAnchor.GetSafeNormal() * (Dist - RopeLength);
		SafeMoveUpdatedComponent(Correction, CharacterOwner->GetActorRotation(), true, Hit);
		
		// Remove radial velocity
		FVector NewToAnchor = (AnchorLoc - UpdatedComponent->GetComponentLocation()).GetSafeNormal();
		float RadialVelMag = FVector::DotProduct(Velocity, NewToAnchor);
		if (RadialVelMag < 0)
		{
			Velocity -= NewToAnchor * RadialVelMag;
		}
	}

	// 자연 감쇠 (Drag & Terminal Velocity)
	Velocity *= FMath::Max(0.f, 1.f - (SwingDamping * deltaTime));

	// 스윙 최대 속도 강제 제한 (Velocity Clamp)
	if (Velocity.SizeSquared() > FMath::Square(MaxSwingSpeed))
	{
		Velocity = Velocity.GetSafeNormal() * MaxSwingSpeed;
	}

	// 로프 스윙 방향 전환 감지 및 사운드 재생
	float CurrentSwingPhase = GetSwingPhase();
	// 부호가 달라졌다면(곱했을 때 음수) 방향이 바뀐 것
	if (PreviousSwingPhase * CurrentSwingPhase < 0.0f && RopeSwingSound)
	{
		if (CharacterOwner)
		{
			UGameplayStatics::PlaySoundAtLocation(this, RopeSwingSound, CharacterOwner->GetActorLocation());
		}
	}
	PreviousSwingPhase = CurrentSwingPhase;

	UpdateComponentVelocity();
}


// --------------------------------------------------------------------------------------
// Network Prediction Boilerplate
// --------------------------------------------------------------------------------------

void UEnvironmentCharacterMovementComponent::FSavedMove_Environment::Clear()
{
	Super::Clear();
	bSavedIsRopeSwinging = false;
}

uint8 UEnvironmentCharacterMovementComponent::FSavedMove_Environment::GetCompressedFlags() const
{
	uint8 Result = Super::GetCompressedFlags();
	if (bSavedIsRopeSwinging)
	{
		Result |= FLAG_Custom_0;
	}
	return Result;
}

bool UEnvironmentCharacterMovementComponent::FSavedMove_Environment::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const
{
	const FSavedMove_Environment* NewEnvironmentMove = static_cast<const FSavedMove_Environment*>(NewMove.Get());
	if (bSavedIsRopeSwinging != NewEnvironmentMove->bSavedIsRopeSwinging)
	{
		return false;
	}
	return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
}

void UEnvironmentCharacterMovementComponent::FSavedMove_Environment::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);

	UEnvironmentCharacterMovementComponent* CharacterMovement = Cast<UEnvironmentCharacterMovementComponent>(C->GetCharacterMovement());
	if (CharacterMovement)
	{
		bSavedIsRopeSwinging = (CharacterMovement->MovementMode == MOVE_Custom && CharacterMovement->CustomMovementMode == CMOVE_RopeSwinging);
	}
}

void UEnvironmentCharacterMovementComponent::FSavedMove_Environment::PrepMoveFor(class ACharacter* C)
{
	Super::PrepMoveFor(C);
}

UEnvironmentCharacterMovementComponent::FNetworkPredictionData_Client_Environment::FNetworkPredictionData_Client_Environment(const UCharacterMovementComponent& ClientMovement)
	: Super(ClientMovement)
{
}

FSavedMovePtr UEnvironmentCharacterMovementComponent::FNetworkPredictionData_Client_Environment::AllocateNewMove()
{
	return FSavedMovePtr(new FSavedMove_Environment());
}
