#include "WeaponFireLibrary.h"

#include "../../Character/BaseCharacter.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

namespace WeaponFireLibrary
{
	namespace
	{
		void AddWeaponTraceIgnoredActors(FCollisionQueryParams& QueryParams, const AActor* SourceActor, AActor* InstigatorActor)
		{
			if (SourceActor)
			{
				QueryParams.AddIgnoredActor(SourceActor);
			}
			if (InstigatorActor)
			{
				QueryParams.AddIgnoredActor(InstigatorActor);
			}
			if (SourceActor)
			{
				if (AActor* OwnerActor = SourceActor->GetOwner())
				{
					QueryParams.AddIgnoredActor(OwnerActor);
				}
			}
		}
	}

	FVector CalculateAimDirection(
		UWorld* World,
		const AActor* SourceActor,
		AActor* InstigatorActor,
		const FVector& FireStart,
		const FVector& FallbackDirection,
		float Range)
	{
		if (!World || !InstigatorActor)
		{
			return FallbackDirection.GetSafeNormal();
		}

		FVector ViewLocation = FVector::ZeroVector;
		FRotator ViewRotation = FRotator::ZeroRotator;

		if (const APawn* InstigatorPawn = Cast<APawn>(InstigatorActor))
		{
			if (AController* Controller = InstigatorPawn->GetController())
			{
				Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
			}
			else
			{
				InstigatorPawn->GetActorEyesViewPoint(ViewLocation, ViewRotation);
			}
		}
		else
		{
			InstigatorActor->GetActorEyesViewPoint(ViewLocation, ViewRotation);
		}

		const FVector ViewDirection = ViewRotation.Vector();
		const FVector ViewEnd = ViewLocation + (ViewDirection * FMath::Max(Range, 0.0f));

		FHitResult AimHit;
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WeaponAimTrace), true, SourceActor);
		AddWeaponTraceIgnoredActors(QueryParams, SourceActor, InstigatorActor);

		const FVector TargetPoint = World->LineTraceSingleByChannel(AimHit, ViewLocation, ViewEnd, ECC_TraceAttack, QueryParams)
			? AimHit.ImpactPoint
			: ViewEnd;

		const FVector AimDirection = (TargetPoint - FireStart).GetSafeNormal();
		return AimDirection.IsNearlyZero() ? FallbackDirection.GetSafeNormal() : AimDirection;
	}

	bool PerformHitScan(
		UWorld* World,
		const AActor* SourceActor,
		AActor* InstigatorActor,
		const FVector& Start,
		const FVector& Direction,
		float Range,
		FHitResult& OutHit)
	{
		if (!World)
		{
			return false;
		}

		const FVector End = Start + (Direction.GetSafeNormal() * FMath::Max(Range, 0.0f));

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WeaponHitScan), true, SourceActor);
		QueryParams.bReturnPhysicalMaterial = true;
		AddWeaponTraceIgnoredActors(QueryParams, SourceActor, InstigatorActor);

		return World->LineTraceSingleByChannel(OutHit, Start, End, ECC_TraceAttack, QueryParams);
	}

	bool IsCharacterHit(const FHitResult& Hit)
	{
		return Hit.GetActor() && Hit.GetActor()->IsA<ACharacter>();
	}
}
