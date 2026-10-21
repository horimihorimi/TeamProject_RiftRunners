#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Actor.h"
#include "GCN_SoulEffigy.generated.h"

class UAnimSequenceBase;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USkeletalMeshComponent;

/**
 * 잡힌 대상의 영혼 복제본 연출.
 *
 * 큐 파라미터의 SourceObject로 받은 캐릭터의 스켈레탈 메시를 그대로 복사해,
 * 모든 슬롯을 영혼 머티리얼로 덮고 시전자의 손 소켓에 붙인다. 큐가 제거되면
 * 디졸브 파라미터를 밀어 사라진다.
 *
 * 복제본은 대상 본인과 무관하게 움직인다. 대상은 잡힌 뒤에도 자유롭게
 * 이동하므로, 이 복제본이 대상을 따라다니면 손에서 떨어져 나간다.
 *
 * 순수 연출이라 각 클라이언트가 로컬에서 만든다. 서버가 액터를 스폰해
 * 복제하면 지연 때문에 손 위치와 어긋난다.
 */
UCLASS(Blueprintable)
class TEAMPROJECT_FIN_API AGCN_SoulEffigy : public AGameplayCueNotify_Actor
{
	GENERATED_BODY()

public:
	AGCN_SoulEffigy();

	virtual bool OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	virtual bool OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	virtual bool Recycle() override;

	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Effigy")
	TObjectPtr<USkeletalMeshComponent> EffigyMesh;

	/** 모든 슬롯을 이것으로 덮는다. 팬텀과 같은 색으로 맞춘다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effigy")
	TObjectPtr<UMaterialInterface> SoulMaterial;

	/**
	 * 복제본이 재생할 애니메이션.
	 *
	 * 비워두면 A 포즈로 굳어서 인형처럼 보인다. 끌려가는 동작을 하나 넣는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effigy")
	TObjectPtr<UAnimSequenceBase> DraggedAnimation;

	/** 시전자 메시에서 복제본을 붙일 소켓. 팬텀 손이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effigy")
	FName AttachSocketName = TEXT("Bone_PH_Hand_R");

	/**
	 * 복제본에서 잡히는 본. 이 본이 위 소켓 위치에 오도록 메시를 역보정한다.
	 *
	 * 스켈레탈 메시의 원점은 발밑이라, 보정하지 않으면 발을 잡고 있는 모양이
	 * 된다. 가슴을 잡게 하려면 흉부 본을 지정한다.
	 *
	 * 대상은 플레이어이므로 플레이어 스켈레톤의 본 이름을 넣는다.
	 * 비워두거나 찾지 못하면 원점을 그대로 붙인다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effigy")
	FName GrabBoneName = TEXT("spine_03");

	/** 소켓 기준 추가 위치 보정. 손바닥 안쪽으로 밀어 넣을 때 쓴다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effigy")
	FVector AttachLocationOffset = FVector::ZeroVector;

	/**
	 * 복제본의 방향 보정.
	 *
	 * 캐릭터 메시는 정면이 아니라 옆을 보도록 만들어져 있고, ACharacter는 그
	 * 보정을 메시 컴포넌트의 상대 회전으로 갖고 있다. 복제본은 메시 애셋만
	 * 가져오므로 그 보정이 딸려오지 않아 여기서 다시 준다.
	 *
	 * 부호는 메시가 어느 쪽을 보도록 제작됐는지에 달렸다. 이 프로젝트의
	 * 플레이어 메시는 +90이다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effigy")
	FRotator AttachRotationOffset = FRotator(0.0f, 90.0f, 0.0f);

	/**
	 * 복제본을 월드 Z 기준으로 세워 둘지.
	 *
	 * 손 소켓은 팔이 휘두르는 대로 뒤집히고 꺾인다. 그대로 따라가면 영혼이
	 * 물구나무를 서거나 옆으로 눕는다. 켜두면 시전자의 좌우 방향만 따르고
	 * 자세는 항상 서 있는 상태를 유지한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effigy")
	bool bKeepUpright = true;

	/**
	 * 매 프레임 정렬을 다시 맞출지.
	 *
	 * 끄면 스폰 순간의 자세로 한 번만 맞춘다. 끌려가는 애님이 흉부를 크게
	 * 움직이면 손에서 점점 떨어져 보이므로, 켜두는 편이 안전하다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effigy")
	bool bKeepGrabBoneAligned = true;

	/** 디졸브에 쓸 스칼라 파라미터 이름. 0에서 1로 민다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effigy|Dissolve")
	FName DissolveParameterName = TEXT("Dissolve");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effigy|Dissolve",
		meta = (ClampMin = "0.0"))
	float DissolveDuration = 0.5f;

private:
	void ApplySoulMaterial();
	void AlignGrabBone();
	void ResetEffigy();

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> SoulMaterialInstances;

	bool bDissolving = false;
	float DissolveElapsed = 0.0f;
};
