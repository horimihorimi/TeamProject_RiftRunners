#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WindAudioManagerSubsystem.generated.h"

class AWindVolumeActor;

/**
 * 로컬 플레이어 주변의 바람 소리만 선별해 재생합니다.
 * 각 클라이언트가 독립적으로 동작하므로 오디오 상태를 네트워크로 복제하지 않습니다.
 */
UCLASS()
class TEAMPROJECT_FIN_API UWindAudioManagerSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;
	virtual bool IsTickable() const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	void RegisterWindVolume(AWindVolumeActor* WindVolume);
	void UnregisterWindVolume(AWindVolumeActor* WindVolume);

private:
	void UpdateActiveWindAudio();
	void StopAllWindAudio();

	static constexpr int32 MaxSimultaneousWindSounds = 2;
	static constexpr float UpdateInterval = 0.15f;

	TArray<TWeakObjectPtr<AWindVolumeActor>> RegisteredWindVolumes;
	TArray<TWeakObjectPtr<AWindVolumeActor>> ActiveWindVolumes;
	float UpdateAccumulator = 0.0f;
};
