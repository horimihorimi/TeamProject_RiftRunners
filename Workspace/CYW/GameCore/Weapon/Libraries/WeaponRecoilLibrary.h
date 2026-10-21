#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WeaponRecoilLibrary.generated.h"

UCLASS()
class TEAMPROJECT_FIN_API UWeaponRecoilLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Weapon|Recoil")
	static FVector2D CalculateRecoilKick(float RecoilPitch, float RecoilYawRatio);

	UFUNCTION(BlueprintPure, Category = "Weapon|Recoil")
	static FVector2D CalculateRecoveryStep(const FVector2D& PendingRecovery, float RecoverySpeed, float DeltaTime);

	UFUNCTION(BlueprintPure, Category = "Weapon|Recoil")
	static FVector2D SnapSmallRecovery(const FVector2D& PendingRecovery, float SnapTolerance = 0.01f);
};
