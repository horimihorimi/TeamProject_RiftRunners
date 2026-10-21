#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AN_SpawnDroneSwarm.generated.h"

class ADroneSwarm;
class USkeletalMeshComponent;

/**
 * 몽타주의 이 지점에서 드론 무리를 사출한다.
 *
 * 소환 시점만 담당한다. 개수와 배치, 단계별 시간 같은 설정은 전부 ADroneSwarm의
 * BP 자식이 갖는다. 그래서 몽타주가 중단되어도 이미 나간 드론은 자기 시퀀스를
 * 끝낸다. 시전을 방해받았을 때 드론이 공중에서 사라지지 않게 하려는 것이다.
 *
 * 스켈레톤 지식은 여기까지다. 소켓 이름을 위치로 바꿔 넘기므로 ADroneSwarm은
 * 본이 몇 개인지, 이름이 무엇인지 알 필요가 없다.
 */
UCLASS(meta = (DisplayName = "Spawn Drone Swarm"))
class TEAMPROJECT_FIN_API UAN_SpawnDroneSwarm : public UAnimNotify
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Swarm")
	TSubclassOf<ADroneSwarm> SwarmClass;

	/**
	 * 드론이 튀어나오는 소켓들. 이 개수가 곧 드론 수가 된다.
	 *
	 * 좌우를 번갈아 넣으면(01_L, 01_R, 02_L, ...) 사출에 시간차를 줬을 때
	 * 양쪽에서 번갈아 나가 보인다. 비워두면 액터 위치 한 곳에서 한 기만 나간다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Swarm")
	TArray<FName> LaunchSocketNames;

	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
