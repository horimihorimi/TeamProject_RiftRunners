// Copyright Epic Games, Inc. All Rights Reserved.

#include "BakeRootMotionModifier.h"

#include "AnimationBlueprintLibrary.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"

#define LOCTEXT_NAMESPACE "BakeRootMotionModifier"

namespace
{
	// 이동으로 간주할 최소 거리(cm). 부동소수 오차를 걸러낸다.
	constexpr double MotionThreshold = 0.01;
	// 회전으로 간주할 최소 각도(degree).
	constexpr double RotationThresholdDegrees = 0.1;
}

void UBakeRootMotionModifier::OnApply_Implementation(UAnimSequence* AnimationSequence)
{
	TransferMotion(AnimationSequence, false);
}

void UBakeRootMotionModifier::OnRevert_Implementation(UAnimSequence* AnimationSequence)
{
	TransferMotion(AnimationSequence, true);
}

FName UBakeRootMotionModifier::FindMotionBone(const UAnimSequence* AnimationSequence)
{
	const TScriptInterface<IAnimationDataModel> DataModel =
		AnimationSequence->GetDataModelInterface();

	if (!DataModel)
	{
		return NAME_None;
	}

	const FReferenceSkeleton& ReferenceSkeleton =
		AnimationSequence->GetSkeleton()->GetReferenceSkeleton();

	TArray<FName> TrackNames;
	DataModel->GetBoneTrackNames(TrackNames);

	TArray<FName> Candidates;
	TArray<int32> CandidateBoneIndices;

	for (int32 BoneIndex = 1; BoneIndex < ReferenceSkeleton.GetNum(); ++BoneIndex)
	{
		const FName BoneName = ReferenceSkeleton.GetBoneName(BoneIndex);

		if (TrackNames.Contains(BoneName))
		{
			Candidates.Add(BoneName);
			CandidateBoneIndices.Add(BoneIndex);
		}
	}

	const int32 NumKeys = DataModel->GetNumberOfKeys();

	if (Candidates.Num() == 0 || NumKeys <= 1)
	{
		return NAME_None;
	}

	TArray<FTransform> FirstPoses;

	UAnimationBlueprintLibrary::GetBonePosesForFrame(
		AnimationSequence, Candidates, 0, false, FirstPoses);

	if (FirstPoses.Num() != Candidates.Num())
	{
		return NAME_None;
	}

	TArray<double> MaxDeltas;
	MaxDeltas.SetNumZeroed(Candidates.Num());
	TArray<double> MaxRotationDeltas;
	MaxRotationDeltas.SetNumZeroed(Candidates.Num());

	for (int32 KeyIndex = 1; KeyIndex < NumKeys; ++KeyIndex)
	{
		TArray<FTransform> Poses;

		UAnimationBlueprintLibrary::GetBonePosesForFrame(
			AnimationSequence, Candidates, KeyIndex, false, Poses);

		if (Poses.Num() != Candidates.Num())
		{
			return NAME_None;
		}

		for (int32 Index = 0; Index < Candidates.Num(); ++Index)
		{
			const double Distance =
				(Poses[Index].GetTranslation()
					- FirstPoses[Index].GetTranslation()).Size();
			const double AngleDegrees = FMath::RadiansToDegrees(
				FirstPoses[Index].GetRotation().AngularDistance(
					Poses[Index].GetRotation()));

			MaxDeltas[Index] = FMath::Max(MaxDeltas[Index], Distance);
			MaxRotationDeltas[Index] = FMath::Max(
				MaxRotationDeltas[Index], AngleDegrees);
		}
	}

	/*
	 * 루트에 가장 가까운 이동 본을 고른다.
	 *
	 * 공격 애니메이션은 손/발의 이동량이 Pelvis_Align보다 훨씬 클 수
	 * 있다. 절대 이동량으로 고르면 팔다리가 루트 모션 기준이 되므로,
	 * 이동 임계값을 넘은 후보 중 계층 깊이가 가장 얕은 본을 선택한다.
	 * 같은 깊이에 후보가 여러 개일 때만 이동량이 큰 본을 사용한다.
	 */
	int32 BestIndex = INDEX_NONE;
	int32 BestDepth = MAX_int32;
	double BestDistance = 0.0;
	double BestRotation = 0.0;

	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		if (MaxDeltas[Index] <= MotionThreshold
			&& MaxRotationDeltas[Index] <= RotationThresholdDegrees)
		{
			continue;
		}

		int32 Depth = 0;
		int32 BoneIndex = CandidateBoneIndices[Index];

		while (BoneIndex != INDEX_NONE)
		{
			++Depth;
			BoneIndex = ReferenceSkeleton.GetParentIndex(BoneIndex);
		}

		if (Depth < BestDepth
			|| (Depth == BestDepth && MaxDeltas[Index] > BestDistance))
		{
			BestDepth = Depth;
			BestDistance = MaxDeltas[Index];
			BestRotation = MaxRotationDeltas[Index];
			BestIndex = Index;
		}
	}

	if (BestIndex != INDEX_NONE)
	{
		UE_LOG(LogTemp, Log,
			TEXT("[BakeRootMotion] %s: 자동 기준 본 %s (깊이 %d, 이동량 %.2f cm, 회전량 %.2f 도)."),
			*AnimationSequence->GetName(),
			*Candidates[BestIndex].ToString(),
			BestDepth,
			BestDistance,
			BestRotation);
	}

	return BestIndex != INDEX_NONE ? Candidates[BestIndex] : NAME_None;
}

FVector UBakeRootMotionModifier::GetBakedTotalDistance(
	const UAnimSequence* AnimationSequence,
	const bool bIncludeVertical)
{
	if (!IsValid(AnimationSequence))
	{
		return FVector::ZeroVector;
	}

	const USkeleton* Skeleton = AnimationSequence->GetSkeleton();

	if (!IsValid(Skeleton))
	{
		return FVector::ZeroVector;
	}

	const FReferenceSkeleton& ReferenceSkeleton = Skeleton->GetReferenceSkeleton();

	if (ReferenceSkeleton.GetNum() == 0)
	{
		return FVector::ZeroVector;
	}

	const TScriptInterface<IAnimationDataModel> DataModel =
		AnimationSequence->GetDataModelInterface();

	if (!DataModel)
	{
		return FVector::ZeroVector;
	}

	const int32 NumKeys = DataModel->GetNumberOfKeys();

	if (NumKeys <= 1)
	{
		return FVector::ZeroVector;
	}

	const FName RootBoneName = ReferenceSkeleton.GetBoneName(0);

	TArray<FName> TrackNames;
	DataModel->GetBoneTrackNames(TrackNames);

	if (!TrackNames.Contains(RootBoneName))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BakeRootMotion] %s: 체인 앞 시퀀스의 루트 본에 트랙이 없습니다. 먼저 그 시퀀스부터 베이크하세요."),
			*AnimationSequence->GetName());

		return FVector::ZeroVector;
	}

	const TArray<FName> QueryNames = { RootBoneName };

	TArray<FTransform> FirstPose;
	TArray<FTransform> LastPose;

	UAnimationBlueprintLibrary::GetBonePosesForFrame(
		AnimationSequence, QueryNames, 0, false, FirstPose);

	UAnimationBlueprintLibrary::GetBonePosesForFrame(
		AnimationSequence, QueryNames, NumKeys - 1, false, LastPose);

	if (FirstPose.Num() != 1 || LastPose.Num() != 1)
	{
		return FVector::ZeroVector;
	}

	FVector TotalDistance = LastPose[0].GetTranslation() - FirstPose[0].GetTranslation();

	if (!bIncludeVertical)
	{
		TotalDistance.Z = 0.0;
	}

	return TotalDistance;
}

void UBakeRootMotionModifier::GatherSiblings(
	const UAnimSequence* AnimationSequence,
	const FName BoneName,
	TArray<FName>& OutBoneNames)
{
	OutBoneNames.Reset();

	const TScriptInterface<IAnimationDataModel> DataModel =
		AnimationSequence->GetDataModelInterface();

	if (!DataModel)
	{
		return;
	}

	const FReferenceSkeleton& ReferenceSkeleton =
		AnimationSequence->GetSkeleton()->GetReferenceSkeleton();

	const int32 BoneIndex = ReferenceSkeleton.FindBoneIndex(BoneName);

	if (BoneIndex == INDEX_NONE)
	{
		return;
	}

	const int32 ParentIndex = ReferenceSkeleton.GetParentIndex(BoneIndex);

	TArray<FName> TrackNames;
	DataModel->GetBoneTrackNames(TrackNames);

	for (int32 Index = 1; Index < ReferenceSkeleton.GetNum(); ++Index)
	{
		if (ReferenceSkeleton.GetParentIndex(Index) != ParentIndex)
		{
			continue;
		}

		const FName SiblingName = ReferenceSkeleton.GetBoneName(Index);

		// 트랙이 없는 본은 변형할 것도 없다.
		if (TrackNames.Contains(SiblingName))
		{
			OutBoneNames.Add(SiblingName);
		}
	}
}

void UBakeRootMotionModifier::TransferMotion(
	UAnimSequence* AnimationSequence,
	const bool bRevert)
{
	if (!IsValid(AnimationSequence))
	{
		return;
	}

	const USkeleton* Skeleton = AnimationSequence->GetSkeleton();

	if (!IsValid(Skeleton))
	{
		return;
	}

	const FReferenceSkeleton& ReferenceSkeleton =
		Skeleton->GetReferenceSkeleton();

	if (ReferenceSkeleton.GetNum() == 0)
	{
		return;
	}

	const TScriptInterface<IAnimationDataModel> DataModel =
		AnimationSequence->GetDataModelInterface();

	if (!DataModel)
	{
		return;
	}

	const int32 NumKeys = DataModel->GetNumberOfKeys();

	if (NumKeys <= 0)
	{
		return;
	}

	if (!bRevert && (MotionTranslationScale < UE_KINDA_SMALL_NUMBER
		|| VerticalMotionScale < UE_KINDA_SMALL_NUMBER))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BakeRootMotion] %s: 이동 배율은 0보다 커야 합니다."),
			*AnimationSequence->GetName());

		return;
	}

	/*
	 * 루트 본은 이름으로 받지 않는다.
	 *
	 * 언리얼은 스켈레톤 최상위 본(인덱스 0)에서만 루트 모션을 추출하므로
	 * 이름을 지정하게 두면 잘못된 값이 들어올 여지만 생긴다.
	 */
	const FName RootBoneName = ReferenceSkeleton.GetBoneName(0);

	/*
	 * 되돌릴 때는 이미 이동이 루트로 옮겨진 뒤라
	 * 자동 탐색이 아무것도 찾지 못한다.
	 * 적용 시 기록해둔 본을 그대로 쓴다.
	 */
	const FName ReferenceName =
		bRevert
		? ResolvedReferenceBone
		: (ReferenceBoneName.IsNone()
			? FindMotionBone(AnimationSequence)
			: ReferenceBoneName);

	if (ReferenceName.IsNone())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BakeRootMotion] %s: 이동하는 본을 찾지 못했습니다."),
			*AnimationSequence->GetName());

		return;
	}

	/*
	 * 이동이 본 하나에만 있는 리그도 있고,
	 * 같은 계층의 형제 전부에 같은 양이 구워진 리그도 있다.
	 * 후자에서 한 본만 보정하면 나머지가 이중으로 이동한다.
	 */
	TArray<FName> SiblingBoneNames;
	GatherSiblings(AnimationSequence, ReferenceName, SiblingBoneNames);

	if (SiblingBoneNames.Num() == 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BakeRootMotion] %s: 본 %s 의 형제 트랙이 없습니다."),
			*AnimationSequence->GetName(),
			*ReferenceName.ToString());

		return;
	}

	// 평가 대상은 루트 + 형제 전부. 인덱스 0이 항상 루트다.
	TArray<FName> QueryNames;
	QueryNames.Add(RootBoneName);
	QueryNames.Append(SiblingBoneNames);

	const int32 ReferenceIndex = QueryNames.IndexOfByKey(ReferenceName);

	if (ReferenceIndex == INDEX_NONE)
	{
		return;
	}

	// 되돌릴 때는 루트에 쌓인 이동량을 기준으로 삼는다.
	const int32 DeltaIndex = bRevert ? 0 : ReferenceIndex;

	TArray<FTransform> FirstPoses;

	UAnimationBlueprintLibrary::GetBonePosesForFrame(
		AnimationSequence, QueryNames, 0, false, FirstPoses);

	if (FirstPoses.Num() != QueryNames.Num())
	{
		return;
	}

	const FVector BasePosition = FirstPoses[DeltaIndex].GetTranslation();
	const FQuat BaseRotation = FirstPoses[DeltaIndex].GetRotation();
	const bool bTransferRotation = !bRevert || bHasResolvedRotationTransfer;

	/*
	 * 루트에는 프레임 사이의 변화량만 넣고, 이 고정 오프셋은 메시 계층에서만
	 * 제거한다. 그래야 재생 시작 순간 캡슐이 순간이동하지 않으면서 메시가
	 * 캡슐 중심에 정렬된다.
	 */
	FVector InitialOffset = FVector::ZeroVector;

	if (bRevert)
	{
		if (bHasResolvedInitialOffset)
		{
			InitialOffset = ResolvedInitialOffset;
		}
		else if (IsValid(PreviousSequenceInChain))
		{
			/*
			 * 새 저장 필드가 생기기 전에 적용된 Modifier의 호환 경로.
			 * 예전 로직이 PreviousSequenceInChain 이동량을 메시에서 뺐으므로
			 * 같은 값을 사용해야 정확히 원본으로 되돌아간다.
			 */
			InitialOffset =
				GetBakedTotalDistance(PreviousSequenceInChain, bIncludeVertical);
		}
	}
	else if (bNormalizeInitialOffset)
	{
		const int32 ReferenceBoneIndex =
			ReferenceSkeleton.FindBoneIndex(ReferenceName);

		if (ReferenceBoneIndex == INDEX_NONE)
		{
			return;
		}

		const FVector ReferencePosition =
			ReferenceSkeleton.GetRefBonePose()[ReferenceBoneIndex].GetTranslation();

		InitialOffset =
			FirstPoses[ReferenceIndex].GetTranslation() - ReferencePosition;

		if (!bIncludeVertical)
		{
			InitialOffset.Z = 0.0;
		}
	}
	else if (IsValid(PreviousSequenceInChain))
	{
		// 초기 위치 정규화를 끈 기존 애셋만 이전 체인 방식으로 보정한다.
		InitialOffset =
			GetBakedTotalDistance(PreviousSequenceInChain, bIncludeVertical);
	}

	TArray<TArray<FVector>> Positions;
	TArray<TArray<FQuat>> Rotations;
	TArray<TArray<FVector>> Scales;

	Positions.SetNum(QueryNames.Num());
	Rotations.SetNum(QueryNames.Num());
	Scales.SetNum(QueryNames.Num());

	double MaxDeltaSize = 0.0;
	double MaxRotationDeltaDegrees = 0.0;

	for (int32 KeyIndex = 0; KeyIndex < NumKeys; ++KeyIndex)
	{
		TArray<FTransform> Poses;

		/*
		 * 본 단위로 호출하면 매번 전체 포즈를 평가한다.
		 * 프레임당 한 번만 평가하도록 복수형을 쓴다.
		 */
		UAnimationBlueprintLibrary::GetBonePosesForFrame(
			AnimationSequence, QueryNames, KeyIndex, false, Poses);

		if (Poses.Num() != QueryNames.Num())
		{
			return;
		}

		/*
		 * 첫 프레임을 기준으로 한 상대 이동량.
		 *
		 * 절대 위치를 그대로 옮기면 재생 시작 순간
		 * 캐릭터가 본 오프셋만큼 튄다.
		 */
		FVector RawDelta = Poses[DeltaIndex].GetTranslation() - BasePosition;
		const FQuat RawRotationDelta =
			bTransferRotation
			? (Poses[DeltaIndex].GetRotation() * BaseRotation.Inverse()).GetNormalized()
			: FQuat::Identity;
		const double RotationDeltaDegrees = FMath::RadiansToDegrees(
			RawRotationDelta.GetAngle());
		MaxRotationDeltaDegrees = FMath::Max(
			MaxRotationDeltaDegrees,
			RotationDeltaDegrees);

		if (!bIncludeVertical)
		{
			RawDelta.Z = 0.0;
		}

		/*
		 * 적용할 때만 원본 이동량에 배율을 적용한다.
		 * 되돌릴 때는 현재 루트에 실제로 들어 있는 이동량을 그대로 사용한다.
		 */
		const float AppliedTranslationScale =
			bRevert ? ResolvedMotionTranslationScale : MotionTranslationScale;
		const float AppliedVerticalScale =
			bRevert ? ResolvedVerticalMotionScale : VerticalMotionScale;

		FVector OriginalMotionDelta = RawDelta;
		if (bRevert)
		{
			OriginalMotionDelta /= AppliedTranslationScale;
			OriginalMotionDelta.Z /= AppliedVerticalScale;
		}

		FVector MotionDelta =
			bRevert ? RawDelta : OriginalMotionDelta * AppliedTranslationScale;
		if (!bRevert)
		{
			MotionDelta.Z *= AppliedVerticalScale;
		}

		MaxDeltaSize = FMath::Max(
			MaxDeltaSize,
			(MotionDelta + InitialOffset).Size());

		/*
		 * 루트와 형제 본에 다른 양을 적용한다.
		 *
		 * 루트 모션은 프레임 간 "성장분"만 실제 이동으로 소비되므로
		 * InitialOffset을 더해도 안 더해도 결과 이동에는 차이가 없어야
		 * 정상이지만, 0프레임 절대값 자체가 커지면 엔진이 그 순간을
		 * 별도 이동으로 처리해 튀는 현상이 있었다. 그래서 루트에는
		 * InitialOffset을 넣지 않고, 시각적 포즈를 담당하는 형제 본에만
		 * 넣어서 "0프레임인데 이미 중간 동작인 포즈"만 보정한다.
		 */
		const FVector RootShift = bRevert ? -RawDelta : MotionDelta;
		const FVector SiblingShift =
			bRevert
			? -(OriginalMotionDelta + InitialOffset)
			: (OriginalMotionDelta + InitialOffset);

		Positions[0].Add(Poses[0].GetTranslation() + RootShift);
		Rotations[0].Add(
			bTransferRotation
				? (bRevert
					? (RawRotationDelta.Inverse() * Poses[0].GetRotation()).GetNormalized()
					: (RawRotationDelta * Poses[0].GetRotation()).GetNormalized())
				: Poses[0].GetRotation());
		Scales[0].Add(Poses[0].GetScale3D());

		for (int32 Index = 1; Index < QueryNames.Num(); ++Index)
		{
			Positions[Index].Add(Poses[Index].GetTranslation() - SiblingShift);
			Rotations[Index].Add(
				bTransferRotation
					? (bRevert
						? (RawRotationDelta * Poses[Index].GetRotation()).GetNormalized()
						: (RawRotationDelta.Inverse() * Poses[Index].GetRotation()).GetNormalized())
					: Poses[Index].GetRotation());
			Scales[Index].Add(Poses[Index].GetScale3D());
		}
	}

	/*
	 * 아무것도 옮기지 못했는데 성공한 것처럼 보이는 상황을 막는다.
	 * 기준 본을 잘못 고르면 델타가 0으로 나온다.
	 */
	if (MaxDeltaSize < MotionThreshold
		&& MaxRotationDeltaDegrees < RotationThresholdDegrees)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BakeRootMotion] %s: 본 %s 에 이동량이 없습니다. ReferenceBoneName 을 직접 지정하세요."),
			*AnimationSequence->GetName(),
			*ReferenceName.ToString());

		return;
	}

	IAnimationDataController& Controller = AnimationSequence->GetController();

	Controller.OpenBracket(LOCTEXT("BakeRootMotion", "Bake Root Motion"));

	// 루트가 한 번도 애니메이션되지 않았다면 트랙 자체가 없다.
	TArray<FName> TrackNames;
	DataModel->GetBoneTrackNames(TrackNames);

	if (!TrackNames.Contains(RootBoneName))
	{
		Controller.AddBoneCurve(RootBoneName);
	}

	for (int32 Index = 0; Index < QueryNames.Num(); ++Index)
	{
		Controller.SetBoneTrackKeys(
			QueryNames[Index],
			Positions[Index],
			Rotations[Index],
			Scales[Index]);
	}

	Controller.CloseBracket();

	/*
	 * 적용 후에는 루트 모션을 켜고, 되돌릴 때는 끈다.
	 * 트랙만 만들어두면 엔진이 루트 모션으로 취급하지 않는다.
	 *
	 * 이 플래그는 컨트롤러 브래킷 밖이라 직접 더티 처리해야
	 * 저장 시 함께 기록된다.
	 */
	AnimationSequence->Modify();

	AnimationSequence->bEnableRootMotion = !bRevert;

	// 되돌리기에서 같은 계층을 다시 찾을 수 있도록 남긴다.
	ResolvedReferenceBone = bRevert ? NAME_None : ReferenceName;
	ResolvedInitialOffset =
		bRevert ? FVector::ZeroVector : InitialOffset;
	bHasResolvedInitialOffset = !bRevert;
	ResolvedMotionTranslationScale =
		bRevert ? 1.0f : MotionTranslationScale;
	ResolvedVerticalMotionScale =
		bRevert ? 1.0f : VerticalMotionScale;
	bHasResolvedRotationTransfer = bRevert ? false : true;

	AnimationSequence->MarkPackageDirty();

	UE_LOG(LogTemp, Log,
		TEXT("[BakeRootMotion] %s: 본 %s 기준, 형제 %d 개, 초기 오프셋 %s, 최대 이동량 %.2f cm, 최대 회전량 %.2f 도."),
		*AnimationSequence->GetName(),
		*ReferenceName.ToString(),
		SiblingBoneNames.Num(),
		*InitialOffset.ToCompactString(),
		MaxDeltaSize,
		MaxRotationDeltaDegrees);
}

#undef LOCTEXT_NAMESPACE
