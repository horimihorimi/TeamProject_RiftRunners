#include "ChangeMaterialColorEvent.h"
#include "Components/MeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	void ApplyMaterialColor(const FMaterialColorChangeTarget& ColorTarget)
	{
		if (!IsValid(ColorTarget.TargetActor) || ColorTarget.MaterialIndex < 0)
		{
			return;
		}

		TArray<UMeshComponent*> MeshComponents;
		ColorTarget.TargetActor->GetComponents<UMeshComponent>(MeshComponents);

		for (UMeshComponent* MeshComponent : MeshComponents)
		{
			if (!IsValid(MeshComponent)
				|| (!ColorTarget.TargetComponentName.IsNone()
					&& MeshComponent->GetFName() != ColorTarget.TargetComponentName)
				|| MeshComponent->GetNumMaterials() <= ColorTarget.MaterialIndex)
			{
				continue;
			}

			UMaterialInstanceDynamic* DynamicMaterial = MeshComponent->CreateAndSetMaterialInstanceDynamic(ColorTarget.MaterialIndex);
			if (DynamicMaterial)
			{
				DynamicMaterial->SetVectorParameterValue(ColorTarget.ColorParameterName, ColorTarget.NewColor);
			}
		}
	}
}

void AChangeMaterialColorEvent::Execute(const UObject* WorldContext)
{
	Super::Execute(WorldContext);
	ApplyMaterialColorChanges();
}

void AChangeMaterialColorEvent::ApplyMaterialColorChanges()
{
	for (const FMaterialColorChangeTarget& AdditionalTarget : AdditionalMaterialTargets)
	{
		ApplyMaterialColor(AdditionalTarget);
	}
}
