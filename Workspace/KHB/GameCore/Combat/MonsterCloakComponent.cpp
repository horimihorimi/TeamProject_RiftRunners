#include "MonsterCloakComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	/*
	 * 인스턴스 체인을 따라 올라가며 실제로 지정된 텍스처를 찾는다.
	 *
	 * bOveriddenOnly를 켜지 않으면 아무도 지정하지 않은 파라미터까지 베이스
	 * 머티리얼의 DefaultTexture로 읽혀서 회색 체커가 발린다. 반대로 인스턴스
	 * 하나만 검사하면, 자식이 덮어쓰지 않고 부모 값을 그대로 쓰는 슬롯을 놓친다.
	 */
	UTexture* FindAssignedTexture(UMaterialInterface* Material, const FName ParameterName)
	{
		if (ParameterName.IsNone())
		{
			return nullptr;
		}

		UMaterialInterface* Current = Material;
		while (UMaterialInstance* Instance = Cast<UMaterialInstance>(Current))
		{
			UTexture* Texture = nullptr;
			if (Instance->GetTextureParameterValue(ParameterName, Texture, true) && IsValid(Texture))
			{
				return Texture;
			}

			Current = Instance->Parent;
		}

		return nullptr;
	}

	/*
	 * 런타임에 DMI로 꽂은 텍스처는 스트리밍이 화면 크기를 계산할 근거가 없어서
	 * 최저 밉에 머문다. 몬스터 텍스처는 개수가 적으니 상주시킨다.
	 */
	void ForceTextureResident(UTexture* Texture)
	{
		if (UTexture2D* Texture2D = Cast<UTexture2D>(Texture))
		{
			Texture2D->bForceMiplevelsToBeResident = true;
		}
	}
}

UMonsterCloakComponent::UMonsterCloakComponent()
{
	// 보간 중에만 틱한다.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true);
}

void UMonsterCloakComponent::BeginPlay()
{
	Super::BeginPlay();
	BuildCloakMaterials();
}

void UMonsterCloakComponent::BuildCloakMaterials()
{
	AActor* OwnerActor = GetOwner();

	/*
	 * FindComponentByClass는 무기 같은 부착 메시를 먼저 잡을 수 있다.
	 * 캐릭터면 본체 메시가 확정이므로 그쪽을 우선한다.
	 */
	if (const ACharacter* OwnerCharacter = Cast<ACharacter>(OwnerActor))
	{
		CachedMesh = OwnerCharacter->GetMesh();
	}
	if (!IsValid(CachedMesh) && IsValid(OwnerActor))
	{
		CachedMesh = OwnerActor->FindComponentByClass<USkeletalMeshComponent>();
	}

	if (!IsValid(CachedMesh) || !IsValid(CloakMaterial))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Cloak] BuildFailed | Owner=%s | Mesh=%s | CloakMaterial=%s"),
			*GetNameSafe(OwnerActor),
			*GetNameSafe(CachedMesh),
			*GetNameSafe(CloakMaterial));
		return;
	}

	OriginalMaterials.Reset();
	CloakMaterials.Reset();
	bCloakMaterialsApplied = false;

	const int32 MaterialCount = CachedMesh->GetNumMaterials();
	for (int32 Index = 0; Index < MaterialCount; ++Index)
	{
		UMaterialInterface* SourceMaterial = CachedMesh->GetMaterial(Index);
		OriginalMaterials.Add(SourceMaterial);

		UMaterialInstanceDynamic* Cloak = UMaterialInstanceDynamic::Create(CloakMaterial, this);
		if (!IsValid(Cloak))
		{
			CloakMaterials.Add(nullptr);
			continue;
		}

		UTexture* BaseColorTexture = FindAssignedTexture(SourceMaterial, SourceBaseColorParameter);
		if (IsValid(BaseColorTexture))
		{
			ForceTextureResident(BaseColorTexture);
			Cloak->SetTextureParameterValue(TEXT("BaseColorTex"), BaseColorTexture);
		}

		UTexture* EmissiveTexture = FindAssignedTexture(SourceMaterial, SourceEmissiveParameter);
		if (IsValid(EmissiveTexture))
		{
			ForceTextureResident(EmissiveTexture);
			Cloak->SetTextureParameterValue(TEXT("EmissiveTex"), EmissiveTexture);
		}
		else
		{
			// 발광 텍스처가 없는 슬롯은 이미시브를 꺼서 기본 텍스처가 빛나는 것을 막는다.
			Cloak->SetScalarParameterValue(TEXT("EmissiveIntensity"), 0.0f);
		}

		Cloak->SetScalarParameterValue(TEXT("CloakProgress"), 0.0f);
		CloakMaterials.Add(Cloak);

		UE_LOG(LogTemp, Log,
			TEXT("[Cloak] SlotPrepared | Owner=%s | Slot=%d | Source=%s | BaseColor=%s | Emissive=%s"),
			*GetNameSafe(OwnerActor),
			Index,
			*GetNameSafe(SourceMaterial),
			IsValid(BaseColorTexture) ? *GetNameSafe(BaseColorTexture) : TEXT("None"),
			IsValid(EmissiveTexture) ? *GetNameSafe(EmissiveTexture) : TEXT("None"));
	}

	// 평상시에는 원본을 그대로 쓴다. 교체는 StartCloak에서만 한다.
	UE_LOG(LogTemp, Log,
		TEXT("[Cloak] BuildCompleted | Owner=%s | Slots=%d | 평상시 원본 유지"),
		*GetNameSafe(OwnerActor),
		CloakMaterials.Num());
}

void UMonsterCloakComponent::ApplyCloakMaterials()
{
	if (bCloakMaterialsApplied || !IsValid(CachedMesh))
	{
		return;
	}

	for (int32 Index = 0; Index < CloakMaterials.Num(); ++Index)
	{
		if (IsValid(CloakMaterials[Index]))
		{
			CachedMesh->SetMaterial(Index, CloakMaterials[Index]);
		}
	}

	bCloakMaterialsApplied = true;

	UE_LOG(LogTemp, Log,
		TEXT("[Cloak] MaterialsApplied | Owner=%s | Slots=%d"),
		*GetNameSafe(GetOwner()),
		CloakMaterials.Num());
}

void UMonsterCloakComponent::RestoreOriginalMaterials()
{
	if (!bCloakMaterialsApplied || !IsValid(CachedMesh))
	{
		return;
	}

	for (int32 Index = 0; Index < OriginalMaterials.Num(); ++Index)
	{
		CachedMesh->SetMaterial(Index, OriginalMaterials[Index]);
	}

	bCloakMaterialsApplied = false;

	UE_LOG(LogTemp, Log,
		TEXT("[Cloak] MaterialsRestored | Owner=%s"),
		*GetNameSafe(GetOwner()));
}

void UMonsterCloakComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ElapsedTime += DeltaTime;
	const float Alpha = InterpolationDuration > KINDA_SMALL_NUMBER
		? FMath::Clamp(ElapsedTime / InterpolationDuration, 0.0f, 1.0f)
		: 1.0f;

	// 등속보다 가감속이 사라지고 나타나는 느낌을 살린다.
	CurrentProgress = FMath::InterpEaseInOut(StartProgress, TargetProgress, Alpha, 2.0f);
	ApplyProgressToMaterials();

	if (Alpha >= 1.0f)
	{
		CurrentProgress = TargetProgress;
		ApplyProgressToMaterials();
		SetComponentTickEnabled(false);

		UE_LOG(LogTemp, Log,
			TEXT("[Cloak] InterpFinished | Owner=%s | Progress=%.2f"),
			*GetNameSafe(GetOwner()),
			CurrentProgress);

		// 완전히 드러났으면 원본으로 되돌려 평상시 그림을 정확하게 만든다.
		if (CurrentProgress <= KINDA_SMALL_NUMBER)
		{
			RestoreOriginalMaterials();
		}
	}
}

void UMonsterCloakComponent::StartCloak(const float Duration)
{
	if (const AActor* OwnerActor = GetOwner(); IsValid(OwnerActor) && OwnerActor->HasAuthority())
	{
		MulticastStartCloak(Duration);
		return;
	}

	StartCloakLocal(Duration);
}

void UMonsterCloakComponent::StartCloakLocal(const float Duration)
{
	UE_LOG(LogTemp, Log,
		TEXT("[Cloak] StartCloak | Owner=%s | Duration=%.2f | Slots=%d"),
		*GetNameSafe(GetOwner()),
		Duration,
		CloakMaterials.Num());

	ApplyCloakMaterials();
	StartInterpolation(1.0f, Duration);
}

void UMonsterCloakComponent::StartReveal(const float Duration)
{
	if (const AActor* OwnerActor = GetOwner(); IsValid(OwnerActor) && OwnerActor->HasAuthority())
	{
		MulticastStartReveal(Duration);
		return;
	}

	StartRevealLocal(Duration);
}

void UMonsterCloakComponent::StartRevealLocal(const float Duration)
{
	UE_LOG(LogTemp, Log,
		TEXT("[Cloak] StartReveal | Owner=%s | Duration=%.2f | From=%.2f"),
		*GetNameSafe(GetOwner()),
		Duration,
		CurrentProgress);

	StartInterpolation(0.0f, Duration);
}

void UMonsterCloakComponent::StartInterpolation(const float InTargetProgress, const float Duration)
{
	if (CloakMaterials.Num() == 0)
	{
		return;
	}

	StartProgress = CurrentProgress;
	TargetProgress = FMath::Clamp(InTargetProgress, 0.0f, 1.0f);
	InterpolationDuration = FMath::Max(Duration, 0.0f);
	ElapsedTime = 0.0f;

	if (InterpolationDuration <= KINDA_SMALL_NUMBER)
	{
		SetCloakProgressLocal(TargetProgress);
		return;
	}

	SetComponentTickEnabled(true);
}

void UMonsterCloakComponent::SetCloakProgress(const float Progress)
{
	if (const AActor* OwnerActor = GetOwner(); IsValid(OwnerActor) && OwnerActor->HasAuthority())
	{
		MulticastSetCloakProgress(Progress);
		return;
	}

	SetCloakProgressLocal(Progress);
}

void UMonsterCloakComponent::SetCloakProgressLocal(const float Progress)
{
	SetComponentTickEnabled(false);
	CurrentProgress = FMath::Clamp(Progress, 0.0f, 1.0f);
	TargetProgress = CurrentProgress;

	if (CurrentProgress > KINDA_SMALL_NUMBER)
	{
		ApplyCloakMaterials();
		ApplyProgressToMaterials();
	}
	else
	{
		ApplyProgressToMaterials();
		RestoreOriginalMaterials();
	}
}

void UMonsterCloakComponent::MulticastStartCloak_Implementation(const float Duration)
{
	StartCloakLocal(Duration);
}

void UMonsterCloakComponent::MulticastStartReveal_Implementation(const float Duration)
{
	StartRevealLocal(Duration);
}

void UMonsterCloakComponent::MulticastSetCloakProgress_Implementation(const float Progress)
{
	SetCloakProgressLocal(Progress);
}

void UMonsterCloakComponent::ApplyProgressToMaterials()
{
	for (UMaterialInstanceDynamic* Cloak : CloakMaterials)
	{
		if (IsValid(Cloak))
		{
			Cloak->SetScalarParameterValue(TEXT("CloakProgress"), CurrentProgress);
		}
	}
}
