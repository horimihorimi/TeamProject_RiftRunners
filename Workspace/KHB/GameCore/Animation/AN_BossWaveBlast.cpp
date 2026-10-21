#include "AN_BossWaveBlast.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameCore/Tags/GameplayTagSystem.h"

FString UAN_BossWaveBlast::GetNotifyName_Implementation() const
{
	return TEXT("Boss Shattered Mirror Fire");
}

void UAN_BossWaveBlast::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	AActor* Owner = IsValid(MeshComp) ? MeshComp->GetOwner() : nullptr;
	if (!IsValid(Owner))
	{
		return;
	}

	/*
	 * 이벤트는 서버에서만 보낸다.
	 *
	 * 몽타주는 클라이언트에서도 재생되므로 노티파이가 양쪽에서 돈다. 걸러내지
	 * 않으면 벽이 두 번 스폰된다.
	 */
	if (!Owner->HasAuthority())
	{
		return;
	}

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		Owner,
		GameplayTagSystem::Event_AI_Boss_ShatteredMirror_Fire,
		FGameplayEventData());
}
