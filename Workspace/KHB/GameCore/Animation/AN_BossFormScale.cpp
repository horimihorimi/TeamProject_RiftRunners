#include "AN_BossFormScale.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameCore/Character/BossMonsterCharacter.h"

FString UAN_BossFormScale::GetNotifyName_Implementation() const
{
	return TEXT("Boss Form Scale");
}

void UAN_BossFormScale::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	ABossMonsterCharacter* Boss = IsValid(MeshComp)
		? Cast<ABossMonsterCharacter>(MeshComp->GetOwner())
		: nullptr;
	if (IsValid(Boss) && IsValid(ScaleCurve))
	{
		Boss->BeginBossFormScaleTransition(ScaleCurve, TotalDuration);
	}
}

void UAN_BossFormScale::NotifyTick(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const float FrameDeltaTime,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	if (ABossMonsterCharacter* Boss = IsValid(MeshComp) ? Cast<ABossMonsterCharacter>(MeshComp->GetOwner()) : nullptr)
	{
		Boss->UpdateBossFormScaleTransition(FrameDeltaTime);
	}
}

void UAN_BossFormScale::NotifyEnd(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (ABossMonsterCharacter* Boss = IsValid(MeshComp) ? Cast<ABossMonsterCharacter>(MeshComp->GetOwner()) : nullptr)
	{
		Boss->EndBossFormScaleTransition();
	}
}
