#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CinematicStandInComponent.generated.h"

class APawn;
class USceneComponent;
class USkeletalMeshComponent;

/*
 * 컷신에서 플레이어 대신 걷는 대역. 레벨에 놓은 SkeletalMeshActor에 붙인다.
 * 로직·GAS·복제가 없는 인형이라 몬스터 타깃이나 네트워크와 얽히지 않는다. 움직임은 시퀀스 트랙이 맡는다.
 * 재생 직전에 맡은 플레이어의 겉모습(몸체 메시, 장착 무기)만 베껴 입는다
 */
UCLASS(ClassGroup = (Cinematic), meta = (BlueprintSpawnableComponent))
class TEAMPROJECT_FIN_API UCinematicStandInComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCinematicStandInComponent();

	// Source 플레이어의 몸체 메시·머티리얼과 장착 무기를 베껴 입고 보이게 한다
	void ApplyAppearance(const APawn* Source);

	// 베낀 무기를 지우고 숨긴다. 빈 슬롯과 컷신 종료 때 부른다
	void ClearAppearance();

	/*
	 * 컷신이 끝났을 때 실제 플레이어를 세울 자리.
	 * 방향은 대형 기준점(부착 부모)을 따른다. 메시는 보통 -90도 돌려 붙이므로 대역 자신의 회전을 쓰면 옆을 본다
	 */
	FTransform GetStandTransform() const;

protected:
	virtual void BeginPlay() override;

private:
	USkeletalMeshComponent* GetBodyMesh() const;

	// 몸체에 붙은 부위 메시(머리·장비 등)를 리더 포즈로 따라가게 베낀다
	void CopyAttachedParts(const USkeletalMeshComponent* SourceBody, USkeletalMeshComponent* TargetBody);

	void CopyWeapon(const APawn* Source, USkeletalMeshComponent* TargetBody);

	// 이번 재생 동안 만든 컴포넌트. ClearAppearance가 지운다
	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> CreatedComponents;
};
