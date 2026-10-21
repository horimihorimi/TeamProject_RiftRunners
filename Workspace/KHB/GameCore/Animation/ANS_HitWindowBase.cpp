#include "GameCore/Animation/ANS_HitWindowBase.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameCore/Ability/DamageSystemLibrary.h"
#include "GameCore/Character/BaseCharacter.h"
#include "GameCore/Combat/MonsterDamageLibrary.h"
#include "GameCore/Tags/GameplayTagSystem.h"

void UANS_HitWindowBase::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	const AActor* Owner = IsValid(MeshComp) ? MeshComp->GetOwner() : nullptr;
	if (!IsValid(Owner))
	{
		return;
	}

	FHitWindowRuntime& Runtime = ActiveWindows.FindOrAdd(MeshComp);
	Runtime.LastHitTimes.Reset();
	Runtime.AttackTag = ResolveAttackTag(Owner);

	/*
	 * 타수를 간격으로 바꾼다. 첫 타는 진입 즉시 들어가므로 구간 길이를 타수로
	 * 나누면 0, D/N, 2D/N ... 순서로 정확히 N번 맞는다.
	 *
	 * TotalDuration은 시퀀스 기준 길이다. 몽타주 재생 속도를 올리면 구간이 더
	 * 빨리 지나가 타수가 줄어든다.
	 */
	Runtime.ReHitInterval = (HitCount > 1 && TotalDuration > 0.0f)
		? TotalDuration / static_cast<float>(HitCount)
		: ReHitInterval;

	OnWindowBegin(MeshComp);
}

void UANS_HitWindowBase::NotifyTick(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const float FrameDeltaTime,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	AActor* Owner = IsValid(MeshComp) ? MeshComp->GetOwner() : nullptr;
	if (!IsValid(Owner))
	{
		return;
	}

	FHitWindowRuntime* Runtime = ActiveWindows.Find(MeshComp);
	if (Runtime == nullptr)
	{
		return;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(HitWindowTrace), false);
	QueryParams.AddIgnoredActor(Owner);

	TArray<FHitTraceSegment> Segments;
	GetTraceSegments(MeshComp, FrameDeltaTime, Segments);
	for (const FHitTraceSegment& Segment : Segments)
	{
		TArray<FHitResult> HitResults;
		const bool bHit = Owner->GetWorld()->SweepMultiByChannel(
			HitResults, Segment.Start, Segment.End, FQuat::Identity,
			ECC_TraceAttack, FCollisionShape::MakeSphere(TraceRadius), QueryParams);

		if (bDebugDrawTrace)
		{
			DrawDebugTrace(Owner, Segment.Start, Segment.End, HitResults, bHit);
		}

		// 맞지 않았을 때도 알린다. 빔 길이를 최대치로 되돌리는 것 같은 처리가 필요하다.
		OnTraceResolved(MeshComp, Segment.Start, Segment.End, HitResults);

		// 빔 등 연출의 끝점 갱신은 모든 클라이언트에서 수행한다.
		// 실제 충돌 판정과 GameplayEffect 적용만 서버가 권한을 가진다.
		if (Owner->HasAuthority() && bHit && DamageEffectClass != nullptr)
		{
			// Runtime이 대상별 타격 시간을 공유하므로 여러 부위에 닿아도 한 번만 맞는다.
			ApplyDamage(Owner, *Runtime, HitResults);
		}
	}
}

void UANS_HitWindowBase::GetTraceSegments(
	USkeletalMeshComponent* MeshComp,
	const float DeltaTime,
	TArray<FHitTraceSegment>& OutSegments)
{
	FVector StartLocation = FVector::ZeroVector;
	FVector EndLocation = FVector::ZeroVector;
	if (GetTraceSegment(MeshComp, DeltaTime, StartLocation, EndLocation))
	{
		OutSegments.Add({ StartLocation, EndLocation });
	}
}

void UANS_HitWindowBase::NotifyEnd(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	// 권한으로 거르지 않는다. Begin 때와 상태가 달라졌어도 스폰한 것은 치워야 한다.
	OnWindowEnd(MeshComp);
	ActiveWindows.Remove(MeshComp);

	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

void UANS_HitWindowBase::ApplyDamage(
	AActor* Owner,
	FHitWindowRuntime& Runtime,
	const TArray<FHitResult>& HitResults) const
{
	const float Now = Owner->GetWorld()->GetTimeSeconds();

	for (const FHitResult& Hit : HitResults)
	{
		AActor* HitActor = Hit.GetActor();
		if (!IsValid(HitActor))
		{
			continue;
		}

		/*
		 * 간격이 0이면 윈도우당 1회만 판정한다.
		 * 값이 있으면 그 간격이 지난 대상만 다시 맞는다.
		 */
		if (const float* LastHitTime = Runtime.LastHitTimes.Find(HitActor))
		{
			if (Runtime.ReHitInterval <= 0.0f || Now - *LastHitTime < Runtime.ReHitInterval)
			{
				continue;
			}
		}

		UAbilitySystemComponent* TargetASC =
			UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(HitActor);

		if (!IsValid(TargetASC) ||
			!TargetASC->HasMatchingGameplayTag(GameplayTagSystem::Character_Player))
		{
			continue;
		}

		// 벽 같은 무관한 액터로 쿨다운이 낭비되지 않도록 대상 판별 뒤에 기록한다.
		Runtime.LastHitTimes.Add(HitActor, Now);

		FDamagePayload Payload =
			MonsterDamageLibrary::MakeAttackPayload(Runtime.AttackTag, DamageEffectClass);

		// 대상은 이 HitResult에서 결정된다. 비워두면 데미지가 통째로 무시된다.
		Payload.HitResult = Hit;

		UDamageSystemLibrary::LogDamageApplied(Owner, HitActor, TEXT("MonsterHitWindow"));

		/*
		 * 가해자는 때린 몬스터다. 여기에 피격자를 넘기면 피격자의 공격력으로
		 * 스스로를 때리는 계산이 되고, 가해자와 대상이 같아져 자해 배수까지
		 * 곱해진다.
		 */
		UDamageSystemLibrary::ApplyGameDamage(Owner, Owner, Payload);
	}
}

FGameplayTag UANS_HitWindowBase::ResolveAttackTag(const AActor* Owner) const
{
	if (AttackTagOverride.IsValid())
	{
		return AttackTagOverride;
	}

	return MonsterDamageLibrary::ResolveAttackTag(Owner);
}

void UANS_HitWindowBase::DrawDebugTrace(
	const AActor* Owner,
	const FVector& Start,
	const FVector& End,
	const TArray<FHitResult>& HitResults,
	const bool bHit) const
{
	const FVector TraceVector = End - Start;
	const float TraceLength = TraceVector.Length();
	const FColor DebugColor = bHit ? FColor::Red : FColor::Green;

	if (TraceLength <= KINDA_SMALL_NUMBER)
	{
		DrawDebugSphere(
			Owner->GetWorld(), Start, TraceRadius, 12,
			DebugColor, false, DebugDrawDuration);
		return;
	}

	DrawDebugCapsule(
		Owner->GetWorld(),
		(Start + End) * 0.5f,
		TraceLength * 0.5f + TraceRadius,
		TraceRadius,
		FRotationMatrix::MakeFromZ(TraceVector.GetSafeNormal()).ToQuat(),
		DebugColor,
		false,
		DebugDrawDuration);
}
