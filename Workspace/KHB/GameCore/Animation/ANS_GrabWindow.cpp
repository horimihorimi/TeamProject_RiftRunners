#include "GameCore/Animation/ANS_GrabWindow.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameCore/Tags/GameplayTagSystem.h"

UANS_GrabWindow::UANS_GrabWindow()
{
	CaughtEventTag = GameplayTagSystem::Event_AI_SoulRip_Caught;
}

void UANS_GrabWindow::OnTraceResolved(
	USkeletalMeshComponent* MeshComp,
	const FVector& Start,
	const FVector& End,
	const TArray<FHitResult>& HitResults)
{
	AActor* Owner = IsValid(MeshComp) ? MeshComp->GetOwner() : nullptr;

	if (!IsValid(Owner) || !CaughtEventTag.IsValid() || HitResults.IsEmpty())
	{
		return;
	}

	// 판정은 서버가 정한다. 클라이언트가 보낸 이벤트로 잡기가 성립하면 안 된다.
	if (!Owner->HasAuthority())
	{
		return;
	}

	UAbilitySystemComponent* OwnerASC =
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner);

	if (!IsValid(OwnerASC))
	{
		return;
	}

	for (const FHitResult& Hit : HitResults)
	{
		AActor* HitActor = Hit.GetActor();
		if (!IsValid(HitActor))
		{
			continue;
		}

		/*
		 * 플레이어만 잡는다. 벽이나 다른 몬스터에 걸려서 잡기가 성립하면
		 * 팬텀이 허공을 붙잡고 모션을 마친다.
		 */
		const UAbilitySystemComponent* TargetASC =
			UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(HitActor);

		if (!IsValid(TargetASC)
			|| !TargetASC->HasMatchingGameplayTag(GameplayTagSystem::Character_Player))
		{
			continue;
		}

		FGameplayEventData Payload;
		Payload.EventTag = CaughtEventTag;
		Payload.Instigator = Owner;
		Payload.Target = HitActor;

		OwnerASC->HandleGameplayEvent(CaughtEventTag, &Payload);

		// 한 번에 여러 명이 걸려도 잡는 것은 하나다.
		return;
	}
}
