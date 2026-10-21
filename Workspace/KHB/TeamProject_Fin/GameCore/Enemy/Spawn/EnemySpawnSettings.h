#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "EnemySpawnSettings.generated.h"

class UEnemySpawnTable;

/*
 * Project Settings > Enemy Spawn. 대응표 에셋을 경로 문자열 대신 직접 참조한다.
 * 에셋을 옮기거나 이름을 바꿔도 에디터가 따라가서 조용히 깨지지 않는다
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Enemy Spawn"))
class TEAMPROJECT_FIN_API UEnemySpawnSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category = "Spawn")
	TSoftObjectPtr<UEnemySpawnTable> SpawnTable;

	/*
	 * 풀에서 쉬는 몹을 세워 둘 곳. 숨김·충돌 끔 상태라 어디든 상관없지만,
	 * 섹터와 겹치면 디버깅할 때 헷갈리고 KillZ 아래면 엔진이 지워 버리므로 위쪽 먼 곳에 둔다
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Pool")
	FVector StagingLocation = FVector(0.0, 0.0, 50000.0);

	/*
	 * 플레이어의 NavigationInvokerComponent 반경 안쪽으로 물러서는 여유. 활성화·비활성화 거리는
	 * 그 컴포넌트의 생성·제거 반경에서 이 값을 뺀 것이라, 반경은 컴포넌트에서만 정하고 여기서는 여유만 둔다.
	 * 네비메시가 아직 만들어지는 중이거나 이미 지워지는 경계에서 몹이 나오거나 움직이는 것을 피한다
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Activation", meta = (ClampMin = "0.0", Units = "cm"))
	float ActivationMargin = 500.0f;

	// 멀어진 상태가 이만큼 이어져야 처리한다. 경계에서 들락날락해도 깜빡이지 않게
	UPROPERTY(Config, EditAnywhere, Category = "Activation", meta = (ClampMin = "0.0", Units = "s"))
	float DeactivationGraceTime = 3.0f;

	// 거리 판정 주기. 매 프레임이 아니라 이 간격으로만 잰다
	UPROPERTY(Config, EditAnywhere, Category = "Activation", meta = (ClampMin = "0.05", Units = "s"))
	float ActivationCheckInterval = 0.5f;
};
