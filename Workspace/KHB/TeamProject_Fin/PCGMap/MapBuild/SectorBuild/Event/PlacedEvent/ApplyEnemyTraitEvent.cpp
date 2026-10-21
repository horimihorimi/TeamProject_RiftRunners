#include "ApplyEnemyTraitEvent.h"
#include "GameCore/Character/CommonEnemyCharacter.h"
#include "TimerManager.h"

void AApplyEnemyTraitEvent::Execute(const UObject* WorldContext)
{
	if (!HasAuthority() || !IsValid(TargetEnemy))
	{
		return;
	}
	// 레벨 진입 이벤트가 대상의 스탯과 AI 초기화보다 먼저 실행될 수 있습니다.
	if (bExecutionDeferred)
	{
		return;
	}
	if (!TargetEnemy->HasActorBegunPlay())
	{
		bExecutionDeferred = true;
		const TWeakObjectPtr<UObject> Context = const_cast<UObject*>(WorldContext);
		GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,
		                                                                            [this, Context]()
		                                                                            {
			                                                                            bExecutionDeferred = false;
			                                                                            Execute(Context.Get());
		                                                                            }));
		return;
	}
	Super::Execute(WorldContext);
	if (!TargetEnemy->ApplyAdditionalTrait(Trait))
	{
		return;
	}
	if (bShowBeforeBattle)
	{
		TargetEnemy->PreparePlacedEncounter();
	}
	RunEventGroups(PostEvents);
}
