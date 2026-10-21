#include "MonsterProjectileMovementComponent.h"

#include "Components/PrimitiveComponent.h"
#include "MonsterProjectile.h"

UMonsterProjectileMovementComponent::UMonsterProjectileMovementComponent()
{
	ProjectileGravityScale = 0.0f;
	bRotationFollowsVelocity = true;
	bShouldBounce = false;
}

void UMonsterProjectileMovementComponent::StartHomingToTarget(
	AActor* InTarget,
	const float InHomingDuration,
	const float InInitialSpeed)
{
	HomingTarget = InTarget;
	HomingRemainingTime = FMath::Max(InHomingDuration, 0.0f);
	bHomingActive = IsValid(InTarget) && HomingRemainingTime > 0.0f;

	if (InInitialSpeed > 0.0f)
	{
		Velocity = UpdatedComponent
			? UpdatedComponent->GetForwardVector() * InInitialSpeed
			: FVector::ForwardVector * InInitialSpeed;
	}
}

void UMonsterProjectileMovementComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	/*
	 * 추적 시간이 다하면 유도를 끈다. 이후에는 가속이 사라지므로
	 * 그 순간의 속도 그대로 등속 직진한다. 여기가 회피 창이 열리는 지점이다.
	 */
	if (bHomingActive)
	{
		HomingRemainingTime -= DeltaTime;

		if (HomingRemainingTime <= 0.0f || !HomingTarget.IsValid())
		{
			HomingRemainingTime = 0.0f;
			bHomingActive = false;
			HomingTarget = nullptr;
		}
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

FVector UMonsterProjectileMovementComponent::ComputeAcceleration(
	const FVector& InVelocity,
	const float DeltaTime) const
{
	FVector Acceleration = Super::ComputeAcceleration(InVelocity, DeltaTime);

	if (bHomingActive && HomingTarget.IsValid() && IsValid(UpdatedComponent))
	{
		// 매 틱 타깃의 현재 위치를 다시 읽는다. 플레이어가 움직이면 궤적이 휜다.
		const FVector ToTarget =
			HomingTarget->GetActorLocation() - UpdatedComponent->GetComponentLocation();

		Acceleration += ToTarget.GetSafeNormal() * HomingAccelerationMagnitude;

		if (const AMonsterProjectile* Projectile = Cast<AMonsterProjectile>(GetOwner()))
		{
			Acceleration += Projectile->GetHomingSeparationAcceleration();
		}
	}

	return Acceleration;
}

UProjectileMovementComponent::EHandleBlockingHitResult
UMonsterProjectileMovementComponent::HandleBlockingHit(
	const FHitResult& Hit,
	const float TimeTick,
	const FVector& MoveDelta,
	float& SubTickTimeRemaining)
{
	if (AMonsterProjectile* Projectile = Cast<AMonsterProjectile>(GetOwner()))
	{
		if (AActor* OtherActor = Hit.GetActor(); Projectile->ShouldPassThrough(OtherActor))
		{
			// HandleImpact만 생략하면 부모는 Deflect를 반환하고, 이어서
			// HandleDeflection/HandleSliding이 실행되어 바닥을 타고 흐른다.
			// 이 충돌은 완전히 무시하고 남은 서브스텝을 그대로 진행한다.
			if (UPrimitiveComponent* CollisionComponent = Cast<UPrimitiveComponent>(UpdatedComponent))
			{
				CollisionComponent->IgnoreActorWhenMoving(OtherActor, true);
			}
			return EHandleBlockingHitResult::AdvanceNextSubstep;
		}
	}

	return Super::HandleBlockingHit(Hit, TimeTick, MoveDelta, SubTickTimeRemaining);
}

void UMonsterProjectileMovementComponent::HandleImpact(
	const FHitResult& Hit, const float TimeSlice, const FVector& MoveDelta)
{
	if (AMonsterProjectile* Projectile = Cast<AMonsterProjectile>(GetOwner()))
	{
		if (AActor* OtherActor = Hit.GetActor(); Projectile->ShouldPassThrough(OtherActor))
		{
			if (UPrimitiveComponent* CollisionComponent = Cast<UPrimitiveComponent>(UpdatedComponent))
			{
				CollisionComponent->IgnoreActorWhenMoving(OtherActor, true);
			}
			return;
		}
	}

	Super::HandleImpact(Hit, TimeSlice, MoveDelta);
}
