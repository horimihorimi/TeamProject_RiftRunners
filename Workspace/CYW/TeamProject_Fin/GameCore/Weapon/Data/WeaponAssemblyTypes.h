#pragma once

#include "CoreMinimal.h"
#include "WeaponAssemblyTypes.generated.h"

class UStaticMesh;

USTRUCT(BlueprintType)
struct TEAMPROJECT_FIN_API FWeaponAssemblyData
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Assembly")
	TObjectPtr<UStaticMesh> FrameMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Assembly")
	TObjectPtr<UStaticMesh> BarrelMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Assembly")
	FName BarrelSocketName = TEXT("BarrelSocket");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Assembly")
	FTransform BarrelRelativeTransform = FTransform::Identity;

	bool HasAssemblyMeshes() const
	{
		return FrameMesh && BarrelMesh;
	}
};
