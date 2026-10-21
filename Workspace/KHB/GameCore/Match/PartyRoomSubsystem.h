#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PartyRoomSubsystem.generated.h"

// 월드가 교체되어도 유지되는 파티 복귀 목적지. 온라인 세션은 파괴하지 않는다.
UCLASS()
class TEAMPROJECT_FIN_API UPartyRoomSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	void RememberRoom(FName MapName) { RoomMap = MapName; bReturning = false; }
	FName GetRoomMap() const { return RoomMap; }
	void MarkReturning() { bReturning = true; }
	bool IsReturning() const { return bReturning; }
	void FinishReturn() { bReturning = false; }
private:
	FName RoomMap;
	bool bReturning = false;
};
