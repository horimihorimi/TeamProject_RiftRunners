#include "WeaponRecoilLibrary.h"

FVector2D UWeaponRecoilLibrary::CalculateRecoilKick(float RecoilPitch, float RecoilYawRatio)
{
	const float SafePitch = FMath::Max(RecoilPitch, 0.0f);
	const float SafeYawRatio = FMath::Max(RecoilYawRatio, 0.0f);
	return FVector2D(-SafePitch, FMath::FRandRange(-SafePitch * SafeYawRatio, SafePitch * SafeYawRatio));
}

FVector2D UWeaponRecoilLibrary::CalculateRecoveryStep(const FVector2D& PendingRecovery, float RecoverySpeed, float DeltaTime)
{
	const float RecoveryAlpha = FMath::Clamp(FMath::Max(RecoverySpeed, 0.0f) * DeltaTime, 0.0f, 1.0f);
	return PendingRecovery * RecoveryAlpha;
}

FVector2D UWeaponRecoilLibrary::SnapSmallRecovery(const FVector2D& PendingRecovery, float SnapTolerance)
{
	const float SafeTolerance = FMath::Max(SnapTolerance, 0.0f);
	return FVector2D(
		FMath::Abs(PendingRecovery.X) < SafeTolerance ? 0.0f : PendingRecovery.X,
		FMath::Abs(PendingRecovery.Y) < SafeTolerance ? 0.0f : PendingRecovery.Y
	);
}
