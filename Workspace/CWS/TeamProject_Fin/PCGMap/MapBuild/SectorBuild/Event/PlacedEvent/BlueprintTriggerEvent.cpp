#include "BlueprintTriggerEvent.h"

void ABlueprintTriggerEvent::Execute(const UObject* WorldContext)
{
	// 기반 클래스의 Execute 호출 (연결된 PreEvents가 있다면 함께 실행)
	Super::Execute(WorldContext);
	
	// 블루프린트로 이벤트 발동 알림
	ReceiveExecute();
}
