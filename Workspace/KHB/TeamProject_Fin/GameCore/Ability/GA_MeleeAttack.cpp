#include "GA_MeleeAttack.h"

#include "AbilitySystemComponent.h"
#include "GameCore/AI/MonsterAIController.h"
#include "GameCore/Tags/GameplayTagSystem.h"
#include "GameFramework/Character.h"
#include "MotionWarpingComponent.h"

UGA_MeleeAttack::UGA_MeleeAttack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// 식별 태그(Asset Tags)와 데미지 키는 BP 자식에서 지정한다. 기반 클래스는 어떤 패턴인지 모른다.
	ActivationOwnedTags.AddTag(GameplayTagSystem::State_AI_ActionCommitted);
}

void UGA_MeleeAttack::OnBeforePatternMontagePlay(const FGameplayAbilityActorInfo* ActorInfo)
{
	ACharacter* AvatarCharacter = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;

	if (!IsValid(AvatarCharacter))
	{
		return;
	}

	// 부유몹 관통형 근접 공격: 타깃을 지나쳐 그 뒤로 가는 지점을 워프 목표로 등록한다.
	if (UMotionWarpingComponent* WarpComp =
		AvatarCharacter->FindComponentByClass<UMotionWarpingComponent>())
	{
		const AMonsterAIController* Controller =
			Cast<AMonsterAIController>(AvatarCharacter->GetController());

		AActor* TargetActor =
			IsValid(Controller) ? Controller->GetPerceivedTarget() : nullptr;

		if (IsValid(TargetActor))
		{
			WarpComp->AddOrUpdateWarpTargetFromComponent(
				WarpTargetName,
				TargetActor->GetRootComponent(),
				NAME_None,
				true,
				EWarpTargetLocationOffsetDirection::VectorFromTargetToOwner,
				FVector(-WarpOvershootDistance, 0.0f, 0.0f),
				FRotator::ZeroRotator
			);
		}
	}
}
