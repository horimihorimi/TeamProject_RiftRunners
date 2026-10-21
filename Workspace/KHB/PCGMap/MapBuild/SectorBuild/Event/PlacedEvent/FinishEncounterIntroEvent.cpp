#include "FinishEncounterIntroEvent.h"

#include "GameCore/Interface/EncounterIntroTarget.h"

void AFinishEncounterIntroEvent::Execute(const UObject* WorldContext)
{
	Super::Execute(WorldContext);

	// 시네마틱의 Post Events는 서버에서 실행된다. 클라이언트가 중복으로 전투를 열지 않게 방어한다.
	if (!HasAuthority() || !IsValid(EncounterIntroTarget))
	{
		return;
	}

	if (!EncounterIntroTarget->GetClass()->ImplementsInterface(UEncounterIntroTarget::StaticClass()))
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("[Encounter] FinishIntroEvent rejected | Event=%s | Target=%s | Reason=InterfaceMissing"),
		       *GetNameSafe(this), *GetNameSafe(EncounterIntroTarget));
		return;
	}

	IEncounterIntroTarget::Execute_FinishEncounterIntro(EncounterIntroTarget);

	// 대상이 실제 전투 상태로 들어간 뒤 후속 이벤트(예: 보스 애드 스폰)를 시작한다.
	RunEventGroups(PostEvents);
}
