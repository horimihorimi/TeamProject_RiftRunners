#include "ActorVisibilitySubsystem.h"
#include "GameFramework/Actor.h"

bool UActorVisibilitySubsystem::Accept(AActor* Actor, int64 IncomingRevision)
{
	const TWeakObjectPtr<AActor> Key(Actor);
	if (const int64* Previous = AppliedRevisions.Find(Key); Previous && *Previous > IncomingRevision)
	{
		return false;
	}
	AppliedRevisions.Add(Key, IncomingRevision);
	return true;
}
