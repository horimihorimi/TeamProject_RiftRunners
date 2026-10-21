// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PCGMap/MapBuild/SectorBuild/Event/SectorEvent.h"
#include "WindSwitchButton.generated.h"

class UBoxComponent;
class USkeletalMeshComponent;
class AWindVolumeActor;
class AChangeMaterialColorEvent;

UCLASS()
class TEAMPROJECT_FIN_API AWindSwitchButton : public AActor
{
	GENERATED_BODY()
	
public:
	AWindSwitchButton();

	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_IsPressed();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlaySwitchSound(bool bPressed);

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

public:
	// 플레이어가 밟는 판정 구역
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* TriggerBox;

	// 버튼을 감싸는 구조물 (Object_9)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USkeletalMeshComponent* SwitchBaseMesh;

	// 실제로 눌리는 버튼 (Object_8)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USkeletalMeshComponent* SwitchButtonMesh;

	// 이 버튼을 눌렀을 때 조작할 바람 구역들 (에디터에서 맵에 배치된 WindVolume들을 스포이드로 연결)
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Wind Switch")
	TArray<AWindVolumeActor*> TargetWindVolumes;

	// 이 버튼을 눌렀을 때 실행할 레벨 배치 머티리얼 색상 변경 이벤트
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Wind Switch")
	AChangeMaterialColorEvent* TargetMaterialColorEvent;

	// 이 버튼에서 마지막 플레이어가 내려왔을 때 실행할 레벨 배치 머티리얼 색상 변경 이벤트
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Wind Switch")
	AChangeMaterialColorEvent* ReleaseMaterialColorEvent;

	// 처음 눌림 상태로 전환될 때 실행한다. 그룹별 지연과 데이터 에셋/레벨 배치 이벤트를 지원한다.
	UPROPERTY(EditInstanceOnly, Category = "Wind Switch|Events")
	TArray<FTimedEventGroup> PressEvents;

	// 현재 스위치가 눌려있는지 여부 (중복 실행 방지)
	UPROPERTY(ReplicatedUsing = OnRep_IsPressed)
	bool bIsPressed = false;

	// 버튼을 밟았을 때 나는 소리
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Switch|Audio")
	USoundBase* PressSound;

	// 버튼에서 내려왔을 때 나는 소리
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Switch|Audio")
	USoundBase* ReleaseSound;

	// === 비주얼 애니메이션 설정 ===
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Switch|Visual")
	float PressedZOffset = -15.f; // 눌렸을 때 아래로 얼마나 내려갈지

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Switch|Visual")
	float EmissiveOnStrength = 500.f; // 활성화 시 발광 세기

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Switch|Visual")
	float EmissiveOffStrength = 0.f; // 비활성화 시 발광 세기

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Switch|Visual")
	FName EmissiveParamName = "EmissiveStrength";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Switch|Visual")
	float AnimationSpeed = 10.f;

private:
	void RunPressEvents();
	void ExecutePressEventGroup(TArray<TScriptInterface<ISectorEvent>> Events);

	UPROPERTY()
	UMaterialInstanceDynamic* BaseDynamicMaterial;

	float CurrentPressAlpha = 0.0f;
	FVector ButtonOriginalLocation;
};
