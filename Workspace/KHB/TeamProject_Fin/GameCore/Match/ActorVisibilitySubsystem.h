#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ActorVisibilitySubsystem.generated.h"

// 서로 다른 표시 이벤트가 같은 액터를 다뤄도 최신 실행 상태를 유지한다.
UCLASS()
class TEAMPROJECT_FIN_API UActorVisibilitySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	int64 NextRevision() { return ++Revision; }
	bool Accept(AActor* Actor, int64 IncomingRevision);
private:
	int64 Revision = 0;
	TMap<TWeakObjectPtr<AActor>, int64> AppliedRevisions;
};
