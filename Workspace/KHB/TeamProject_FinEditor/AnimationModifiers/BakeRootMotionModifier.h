// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimationModifier.h"
#include "BakeRootMotionModifier.generated.h"

/**
 * 본에 구워진 이동을 최상위 본(루트)으로 옮겨 루트 모션을 만든다.
 *
 * 게임에서 추출한 애니메이션은 루트 트랙이 비어 있고
 * 이동이 하위 본에 구워져 있는 경우가 많다.
 * 그대로 재생하면 메시만 움직이고 액터는 제자리에 남는다.
 *
 * 이동이 본 하나에만 있다고 가정하지 않는다.
 * 같은 계층의 형제 본 전부에 같은 양이 구워진 리그가 있어서,
 * 기준 본을 찾은 뒤 그 형제 전부에서 이동량을 빼고 루트에 더한다.
 *
 * 특정 몬스터에 종속되지 않는다.
 * 반복 입력을 줄이려면 이 클래스를 상속한 BP를 몬스터별 프리셋으로 만든다.
 */
UCLASS()
class TEAMPROJECT_FINEDITOR_API UBakeRootMotionModifier : public UAnimationModifier
{
	GENERATED_BODY()

public:
	virtual void OnApply_Implementation(UAnimSequence* AnimationSequence) override;

	virtual void OnRevert_Implementation(UAnimSequence* AnimationSequence) override;

	/*
	 * 이동량을 측정할 기준 본.
	 *
	 * 비워두면 계층 위쪽부터 훑어 처음으로 움직이는 본을 찾는다.
	 * 자동 탐색이 엉뚱한 본을 고르면 여기에 직접 지정한다.
	 */
	UPROPERTY(EditAnywhere, Category = "Root Motion")
	FName ReferenceBoneName = NAME_None;

	/*
	 * 수직 이동도 옮길지 여부.
	 *
	 * 부유형 캐릭터는 코드에서 높이를 따로 제어하는 경우가 있어,
	 * 그때는 꺼서 루트 모션과 충돌하지 않게 한다.
	 */
	UPROPERTY(EditAnywhere, Category = "Root Motion")
	bool bIncludeVertical = true;

	/*
	 * 원본 이동량에 적용할 배율.
	 *
	 * 1.0이면 원본 이동량 전체를 루트 모션으로 옮기고,
	 * 0.5이면 절반만 루트로 옮겨 이동 거리를 절반으로 줄인다.
	 * 배율을 바꿀 때는 수정된 결과에 다시 적용하지 말고,
	 * 별도로 보존한 원본 애니메이션 시퀀스에서 다시 베이크한다.
	 */
	UPROPERTY(EditAnywhere, Category = "Root Motion", meta = (ClampMin = "0.001"))
	float MotionTranslationScale = 1.0f;

	/*
	 * 수직 이동에만 추가로 적용할 배율.
	 *
	 * 1.0이면 원본 Z 이동을 그대로 사용한다. 부유 몬스터의 공격처럼
	 * 앞뒤 이동은 유지하되 과도한 상승을 막고 싶으면 0.1 등으로 낮춘다.
	 * 0은 Revert 시 원본 Z 이동량을 역산할 수 없으므로 0.001을 사실상
	 * 잠금값으로 사용한다.
	 */
	UPROPERTY(EditAnywhere, Category = "Root Motion", meta = (ClampMin = "0.001", ClampMax = "1.0"))
	float VerticalMotionScale = 1.0f;

	/*
	 * 기준 본의 첫 프레임 위치를 레퍼런스 포즈 위치에 맞춘다.
	 *
	 * 추출 애니메이션은 End 같은 중간 클립의 0프레임부터 기준 본에
	 * 누적 이동 오프셋이 들어 있는 경우가 있다. 그 값을 그대로 두면
	 * 루트 모션으로 캡슐은 이동해도 메시가 캡슐 중심에서 벗어난다.
	 *
	 * 켜면 첫 프레임의 고정 오프셋은 메시 계층에서 제거하고,
	 * 첫 프레임 이후의 변화량만 최상위 루트로 옮긴다.
	 */
	UPROPERTY(EditAnywhere, Category = "Root Motion")
	bool bNormalizeInitialOffset = true;

	/*
	 * 몽타주에서 이 클립 바로 앞에 재생되는 시퀀스.
	 *
	 * 하나로 이어지던 동작을 여러 시퀀스로 쪼갠 경우(Start→End 등),
	 * End의 0프레임은 실제로는 "이동량 0인 지점"이 아니라 Start가 끝난
	 * 지점이다. 이 알고리즘은 항상 0프레임을 기준점으로 잡기 때문에,
	 * 이 값을 지정하지 않으면 그 시작 오프셋이 보정되지 않고 남아서
	 * 몽타주 재생 시 클립 경계에서 위치가 튄다.
	 *
	 * 지정하면, 앞 시퀀스(이미 베이크되어 있어야 함)의 루트 본 총
	 * 이동거리를 자동으로 읽어서 시작 오프셋으로 보정한다. 거리는
	 * 직접 계산해서 넣는 게 아니라 항상 자동으로 구해진다.
	 *
	 * 체인의 첫 클립(예: Start)처럼 정지 상태에서 시작하면 비워둔다.
	 */
	UPROPERTY(EditAnywhere, Category = "Root Motion",
		meta = (EditCondition = "!bNormalizeInitialOffset"))
	TObjectPtr<UAnimSequence> PreviousSequenceInChain = nullptr;

	/*
	 * 적용 시 실제로 사용한 기준 본.
	 *
	 * 되돌릴 때는 이미 이동이 루트로 옮겨진 뒤라 자동 탐색이 불가능하다.
	 * 같은 계층을 다시 찾기 위해 남겨둔다.
	 */
	UPROPERTY()
	FName ResolvedReferenceBone = NAME_None;

	/*
	 * 적용 시 메시 계층에서 실제로 제거한 고정 오프셋.
	 *
	 * 적용 후에는 기준 본의 0프레임이 이미 정규화되어 원래 값을 다시
	 * 계산할 수 없으므로, Revert에서 정확히 복원할 수 있게 저장한다.
	 */
	UPROPERTY()
	FVector ResolvedInitialOffset = FVector::ZeroVector;

	// 새 정규화 로직으로 적용된 값인지 구분해 기존 베이크의 Revert도 지원한다.
	UPROPERTY()
	bool bHasResolvedInitialOffset = false;

	// Revert에서 스케일 적용 전의 원본 이동량을 복원하기 위해 적용 당시 배율을 저장한다.
	UPROPERTY()
	float ResolvedMotionTranslationScale = 1.0f;

	// Revert에서 적용 당시의 수직 배율을 사용해 원본 이동량을 복원한다.
	UPROPERTY()
	float ResolvedVerticalMotionScale = 1.0f;

	// 새 베이크에서 기준 본 회전도 루트로 이전했는지 구분한다.
	// 기존에 이동만 베이크된 애셋의 Revert 호환성을 유지한다.
	UPROPERTY()
	bool bHasResolvedRotationTransfer = false;

private:
	/*
	 * 실제 트랙 변형을 수행한다.
	 *
	 * bRevert가 참이면 루트에 쌓인 이동을 형제 본으로 되돌린다.
	 * 적용과 되돌리기가 완전히 대칭이므로 한 함수로 처리한다.
	 */
	void TransferMotion(UAnimSequence* AnimationSequence, bool bRevert);

	/*
	 * 루트에 가장 가까운 이동 본을 찾는다.
	 *
	 * 손이나 발은 공격 포즈 때문에 Pelvis 계열보다 훨씬 크게 움직일 수
	 * 있으므로 절대 이동량이 가장 큰 본을 고르면 안 된다. 이동량은
	 * 후보 판정과 같은 깊이 후보의 동률 해소에만 사용한다.
	 */
	static FName FindMotionBone(const UAnimSequence* AnimationSequence);

	/*
	 * 이미 베이크된 시퀀스의 루트 본 총 이동거리(마지막 프레임 - 첫 프레임)를
	 * 자동으로 읽는다. 사람이 계산해서 넣는 값이 아니라 항상 이 함수로 구한다.
	 *
	 * 루트 본에 트랙이 없으면(아직 베이크 안 됨) 경고를 남기고 0을 반환한다.
	 */
	static FVector GetBakedTotalDistance(const UAnimSequence* AnimationSequence, bool bIncludeVertical);

	// 지정한 본과 같은 부모를 공유하는 본을 모두 모은다. 자신도 포함한다.
	static void GatherSiblings(
		const UAnimSequence* AnimationSequence,
		FName BoneName,
		TArray<FName>& OutBoneNames);
};
