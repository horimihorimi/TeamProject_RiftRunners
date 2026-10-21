#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WindVolumeActor.generated.h"

class UBoxComponent;
class UNiagaraComponent;
class UAudioComponent;

UCLASS()
class TEAMPROJECT_FIN_API AWindVolumeActor : public AActor
{
	GENERATED_BODY()
	
public:	
	AWindVolumeActor();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override final;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnRep_WindReversed();

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* WindVolume;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UNiagaraComponent* WindParticle;

	// 지속적인 바람 소리를 재생할 오디오 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UAudioComponent* WindAudioComponent;

	// 바람 힘 범위와 별개로, 로컬 플레이어가 바람소리를 들을 수 있는 최대 거리
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind|Audio", meta = (ClampMin = "0.0"))
	float AudioRange = 3000.0f;

	// 여러 바람소리 범위가 겹칠 때 우선순위가 높은 볼륨을 먼저 재생합니다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind|Audio")
	float AudioPriority = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind|Audio", meta = (ClampMin = "0.0"))
	float AudioFadeInDuration = 0.15f;

	// 바람 구역의 크기 (OnConstruction에서 BoxExtent 갱신에 사용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind")
	FVector VolumeSize = FVector(500.f, 500.f, 1000.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind")
	FVector WindVelocity = FVector(0.f, 0.f, 1500.f);

	// The maximum Z velocity allowed when inside this volume. 
	// Helps simulate hovering near the top.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind")
	float TerminalVelocity = 1500.f;

	// 외부 스위치에 의해 바람이 역방향으로 불고 있는지 여부
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_WindReversed, Category = "Wind")
	bool bIsWindReversed = false;

	// 바람 방향을 동적으로 뒤집거나 원래대로 돌립니다 (체크포인트 스위치용)
	UFUNCTION(BlueprintCallable, Category = "Wind")
	void SetWindReversed(bool bReversed);

	// 현재 최종적으로 적용되어야 할 바람 방향 반환
	FVector GetEffectiveWindVelocity() const; 

private:
	void UpdateWindParticleDirection();
};
