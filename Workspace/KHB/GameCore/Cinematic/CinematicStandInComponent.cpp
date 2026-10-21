#include "CinematicStandInComponent.h"

#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameCore/Weapon/Core/WeaponBase.h"
#include "GameCore/Weapon/Core/WeaponComponent.h"

namespace
{
	void CopyMaterials(const UMeshComponent* Source, UMeshComponent* Target)
	{
		for (int32 Index = 0; Index < Source->GetNumMaterials(); ++Index)
		{
			Target->SetMaterial(Index, Source->GetMaterial(Index));
		}
	}
}

UCinematicStandInComponent::UCinematicStandInComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCinematicStandInComponent::BeginPlay()
{
	Super::BeginPlay();

	// 컷신 밖에서는 보이면 안 된다. 레벨에서 숨김 체크를 빠뜨려도 여기서 막는다
	if (AActor* Owner = GetOwner())
	{
		Owner->SetActorHiddenInGame(true);
	}
}

USkeletalMeshComponent* UCinematicStandInComponent::GetBodyMesh() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Cast<USkeletalMeshComponent>(Owner->GetRootComponent()) : nullptr;
}

void UCinematicStandInComponent::ApplyAppearance(const APawn* Source)
{
	ClearAppearance();

	const ACharacter* SourceCharacter = Cast<ACharacter>(Source);
	USkeletalMeshComponent* TargetBody = GetBodyMesh();
	const USkeletalMeshComponent* SourceBody = SourceCharacter ? SourceCharacter->GetMesh() : nullptr;
	if (!TargetBody || !SourceBody)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CinematicStandIn] 외형 적용 실패 | StandIn=%s | Source=%s"),
			*GetNameSafe(GetOwner()), *GetNameSafe(Source));
		return;
	}

	// 모든 플레이어 클래스가 같은 스켈레톤이라 시퀀스의 걷기 애니메이션이 메시를 바꿔도 그대로 붙는다
	TargetBody->SetSkeletalMeshAsset(SourceBody->GetSkeletalMeshAsset());
	CopyMaterials(SourceBody, TargetBody);

	CopyAttachedParts(SourceBody, TargetBody);
	CopyWeapon(Source, TargetBody);

	GetOwner()->SetActorHiddenInGame(false);
}

void UCinematicStandInComponent::CopyAttachedParts(const USkeletalMeshComponent* SourceBody, USkeletalMeshComponent* TargetBody)
{
	for (USceneComponent* Child : SourceBody->GetAttachChildren())
	{
		const USkeletalMeshComponent* Part = Cast<USkeletalMeshComponent>(Child);
		if (!Part || !Part->IsVisible() || !Part->GetSkeletalMeshAsset())
		{
			continue;
		}

		USkeletalMeshComponent* Copy = NewObject<USkeletalMeshComponent>(GetOwner());
		Copy->SetSkeletalMeshAsset(Part->GetSkeletalMeshAsset());
		CopyMaterials(Part, Copy);
		Copy->SetupAttachment(TargetBody, Part->GetAttachSocketName());
		Copy->SetRelativeTransform(Part->GetRelativeTransform());
		Copy->RegisterComponent();

		// 부위 메시는 몸체 포즈를 그대로 따라간다
		Copy->SetLeaderPoseComponent(TargetBody);
		CreatedComponents.Add(Copy);
	}
}

void UCinematicStandInComponent::CopyWeapon(const APawn* Source, USkeletalMeshComponent* TargetBody)
{
	const UWeaponComponent* WeaponComponent = Source->FindComponentByClass<UWeaponComponent>();
	const AWeaponBase* Weapon = WeaponComponent ? WeaponComponent->GetEquippedWeapon() : nullptr;
	const USceneComponent* WeaponRoot = Weapon ? Weapon->GetRootComponent() : nullptr;
	if (!WeaponRoot)
	{
		return;
	}

	// 실제 무기가 붙은 소켓과 오프셋을 그대로 읽어 쓴다. WeaponComponent 설정이 바뀌어도 따라간다
	USceneComponent* CopyRoot = NewObject<USceneComponent>(GetOwner());
	CopyRoot->SetupAttachment(TargetBody, WeaponRoot->GetAttachSocketName());
	CopyRoot->SetRelativeTransform(WeaponRoot->GetRelativeTransform());
	CopyRoot->RegisterComponent();
	CreatedComponents.Add(CopyRoot);

	// 생성형 무기라 파츠 조합이 무기마다 다르다. 보이는 스태틱 메시를 전부 옮긴다
	TArray<UStaticMeshComponent*> MeshComponents;
	Weapon->GetComponents<UStaticMeshComponent>(MeshComponents);

	const FTransform RootWorld = WeaponRoot->GetComponentTransform();
	for (const UStaticMeshComponent* Part : MeshComponents)
	{
		if (!Part->IsVisible() || !Part->GetStaticMesh())
		{
			continue;
		}

		UStaticMeshComponent* Copy = NewObject<UStaticMeshComponent>(GetOwner());
		Copy->SetStaticMesh(Part->GetStaticMesh());
		CopyMaterials(Part, Copy);
		Copy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Copy->SetupAttachment(CopyRoot);
		Copy->SetRelativeTransform(Part->GetComponentTransform().GetRelativeTransform(RootWorld));
		Copy->RegisterComponent();
		CreatedComponents.Add(Copy);
	}
}

void UCinematicStandInComponent::ClearAppearance()
{
	for (USceneComponent* Created : CreatedComponents)
	{
		if (IsValid(Created))
		{
			Created->DestroyComponent();
		}
	}
	CreatedComponents.Reset();

	if (AActor* Owner = GetOwner())
	{
		Owner->SetActorHiddenInGame(true);
	}
}

FTransform UCinematicStandInComponent::GetStandTransform() const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return FTransform::Identity;
	}

	const AActor* Anchor = Owner->GetAttachParentActor();
	const FRotator Facing = Anchor ? Anchor->GetActorRotation() : Owner->GetActorRotation();
	return FTransform(FRotator(0.0, Facing.Yaw, 0.0), Owner->GetActorLocation());
}
