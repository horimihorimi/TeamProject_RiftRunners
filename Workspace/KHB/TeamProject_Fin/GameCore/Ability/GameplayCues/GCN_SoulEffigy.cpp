#include "GameCore/Ability/GameplayCues/GCN_SoulEffigy.h"

#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInstanceDynamic.h"

AGCN_SoulEffigy::AGCN_SoulEffigy()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// 디졸브가 끝날 때까지 살아 있어야 하므로 제거 즉시 파괴하지 않는다.
	bAutoDestroyOnRemove = true;
	AutoDestroyDelay = 1.0f;

	EffigyMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("EffigyMesh"));
	SetRootComponent(EffigyMesh);

	/*
	 * 연출용이라 무엇과도 부딪히지 않는다. 프로파일까지 바꾸는 이유는,
	 * 채널 설정이 남아 있으면 다른 액터의 스윕이 이 메시에 걸려서 밀리거나
	 * 공격 판정이 여기서 멈추기 때문이다.
	 */
	EffigyMesh->SetCollisionProfileName(TEXT("NoCollision"));
	EffigyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EffigyMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	EffigyMesh->SetGenerateOverlapEvents(false);
	EffigyMesh->SetSimulatePhysics(false);
	EffigyMesh->SetEnableGravity(false);
	EffigyMesh->CanCharacterStepUpOn = ECB_No;
	EffigyMesh->SetCastShadow(false);

	SetActorEnableCollision(false);

	// 복제본은 시전자 손을 따라다니기만 한다. 물리도 애님 워크플로도 필요 없다.
	EffigyMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
}

bool AGCN_SoulEffigy::OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	/*
	 * 본뜰 대상은 큐 파라미터로 온다. MyTarget은 큐가 붙은 액터(시전자)라
	 * 여기서 쓰면 팬텀 모양의 영혼이 나온다.
	 */
	const ACharacter* SourceCharacter = Cast<ACharacter>(Parameters.SourceObject.Get());
	const ACharacter* CasterCharacter = Cast<ACharacter>(MyTarget);

	if (!IsValid(SourceCharacter) || !IsValid(CasterCharacter) || !IsValid(EffigyMesh))
	{
		return false;
	}

	const USkeletalMeshComponent* SourceMesh = SourceCharacter->GetMesh();
	USkeletalMeshComponent* CasterMesh = CasterCharacter->GetMesh();

	if (!IsValid(SourceMesh) || !IsValid(CasterMesh))
	{
		return false;
	}

	EffigyMesh->SetSkeletalMesh(SourceMesh->GetSkeletalMeshAsset());

	if (IsValid(DraggedAnimation))
	{
		EffigyMesh->PlayAnimation(DraggedAnimation, true);
	}

	ApplySoulMaterial();

	/*
	 * 액터 원점을 손 소켓에 붙이고, 메시는 그 안에서 다시 민다.
	 * 정렬 계산이 액터가 아니라 메시의 상대 트랜스폼을 건드리는 이유다.
	 */
	AttachToComponent(
		CasterMesh,
		FAttachmentTransformRules::SnapToTargetIncludingScale,
		AttachSocketName);

	AlignGrabBone();

	EffigyMesh->SetVisibility(true, true);

	bDissolving = false;
	DissolveElapsed = 0.0f;

	// 매 프레임 정렬을 맞추려면 디졸브 전에도 틱이 돌아야 한다.
	SetActorTickEnabled(bKeepGrabBoneAligned);

	return Super::OnActive_Implementation(MyTarget, Parameters);
}

void AGCN_SoulEffigy::ApplySoulMaterial()
{
	SoulMaterialInstances.Reset();

	if (!IsValid(SoulMaterial) || !IsValid(EffigyMesh))
	{
		return;
	}

	/*
	 * 슬롯마다 동적 인스턴스를 따로 만든다. 하나를 공유하면 디졸브 값이
	 * 같은 머티리얼을 쓰는 다른 슬롯까지 함께 움직인다.
	 */
	const int32 MaterialCount = EffigyMesh->GetNumMaterials();

	for (int32 SlotIndex = 0; SlotIndex < MaterialCount; ++SlotIndex)
	{
		if (UMaterialInstanceDynamic* DynamicMaterial =
			UMaterialInstanceDynamic::Create(SoulMaterial, this))
		{
			EffigyMesh->SetMaterial(SlotIndex, DynamicMaterial);
			SoulMaterialInstances.Add(DynamicMaterial);
		}
	}
}

void AGCN_SoulEffigy::AlignGrabBone()
{
	if (!IsValid(EffigyMesh))
	{
		return;
	}

	USceneComponent* AttachParent = EffigyMesh->GetAttachParent();
	if (!IsValid(AttachParent))
	{
		return;
	}

	/*
	 * 기준점은 손 소켓의 월드 위치다. 메시가 이 액터의 루트라서 상대 좌표로
	 * 계산하면 자기 위치를 바꾸는 순간 기준까지 같이 움직인다.
	 */
	const FVector SocketLocation =
		AttachParent->GetSocketLocation(EffigyMesh->GetAttachSocketName());

	FQuat DesiredRotation;

	if (bKeepUpright)
	{
		/*
		 * 손 소켓은 팔이 움직이는 대로 꺾이므로 그대로 따라가면 영혼이 눕는다.
		 * 시전자의 좌우 방향(Yaw)만 가져오고 나머지 축은 버려서 세워 둔다.
		 */
		const AActor* AttachActor = GetAttachParentActor();
		const float Yaw = IsValid(AttachActor)
			? AttachActor->GetActorRotation().Yaw
			: AttachParent->GetComponentRotation().Yaw;

		DesiredRotation = FRotator(0.0f, Yaw, 0.0f).Quaternion() * AttachRotationOffset.Quaternion();
	}
	else
	{
		DesiredRotation =
			AttachParent->GetSocketQuaternion(EffigyMesh->GetAttachSocketName())
			* AttachRotationOffset.Quaternion();
	}

	EffigyMesh->SetWorldRotation(DesiredRotation);

	const int32 BoneIndex = EffigyMesh->GetBoneIndex(GrabBoneName);

	if (BoneIndex == INDEX_NONE)
	{
		// 본을 못 찾으면 보정 없이 원점을 붙인다. 발을 잡은 모양이 되지만
		// 최소한 위치는 손에 있어서 이름이 틀렸다는 것이 바로 보인다.
		EffigyMesh->SetWorldLocation(SocketLocation + AttachLocationOffset);
		return;
	}

	/*
	 * 본의 컴포넌트 공간 위치를 지금 회전으로 돌린 만큼 반대로 밀면,
	 * 그 본이 정확히 소켓 위치에 온다.
	 */
	const FVector BoneLocalLocation =
		EffigyMesh->GetBoneTransform(BoneIndex, FTransform::Identity).GetLocation();

	EffigyMesh->SetWorldLocation(
		SocketLocation + AttachLocationOffset - DesiredRotation.RotateVector(BoneLocalLocation));
}

bool AGCN_SoulEffigy::OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	if (DissolveDuration > 0.0f && !SoulMaterialInstances.IsEmpty())
	{
		bDissolving = true;
		DissolveElapsed = 0.0f;
		SetActorTickEnabled(true);
	}
	else if (IsValid(EffigyMesh))
	{
		EffigyMesh->SetVisibility(false, true);
	}

	return Super::OnRemove_Implementation(MyTarget, Parameters);
}

void AGCN_SoulEffigy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	/*
	 * 애님이 흉부를 움직이면 잡힌 본도 같이 움직인다. 매 프레임 다시 맞춰야
	 * 손에 붙어 있는 것처럼 보인다.
	 */
	if (bKeepGrabBoneAligned)
	{
		AlignGrabBone();
	}

	if (!bDissolving)
	{
		return;
	}

	DissolveElapsed += DeltaSeconds;

	const float Alpha = FMath::Clamp(DissolveElapsed / DissolveDuration, 0.0f, 1.0f);

	for (UMaterialInstanceDynamic* DynamicMaterial : SoulMaterialInstances)
	{
		if (IsValid(DynamicMaterial))
		{
			DynamicMaterial->SetScalarParameterValue(DissolveParameterName, Alpha);
		}
	}

	if (Alpha >= 1.0f)
	{
		bDissolving = false;

		// 메시를 숨기고 나면 정렬을 유지할 이유도 없다.
		SetActorTickEnabled(false);

		if (IsValid(EffigyMesh))
		{
			EffigyMesh->SetVisibility(false, true);
		}
	}
}

void AGCN_SoulEffigy::ResetEffigy()
{
	bDissolving = false;
	DissolveElapsed = 0.0f;
	SoulMaterialInstances.Reset();

	DetachFromActor(FDetachmentTransformRules::KeepRelativeTransform);

	if (IsValid(EffigyMesh))
	{
		EffigyMesh->SetVisibility(false, true);
		EffigyMesh->SetSkeletalMesh(nullptr);
	}

	SetActorTickEnabled(false);
}

bool AGCN_SoulEffigy::Recycle()
{
	/*
	 * 큐 액터는 풀에서 재사용된다. 남은 메시와 머티리얼을 지우지 않으면
	 * 다음에 다른 캐릭터를 잡았을 때 이전 모습이 한 프레임 보인다.
	 */
	ResetEffigy();

	return Super::Recycle();
}
