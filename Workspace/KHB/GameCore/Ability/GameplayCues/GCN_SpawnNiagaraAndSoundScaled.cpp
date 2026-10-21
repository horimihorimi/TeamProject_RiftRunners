#include "GCN_SpawnNiagaraAndSoundScaled.h"

#include "../../Tags/GameplayTagSystem.h"
#include "GameplayCueFxLibrary.h"
#include "NiagaraComponent.h"
#include "DrawDebugHelpers.h"

bool UGCN_SpawnNiagaraAndSoundScaled::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	UWorld* World = MyTarget ? MyTarget->GetWorld() : nullptr;
	if (!World) {
		return false;
	}

	// 요청 쪽이 넘긴 데이터가 있으면 그쪽이 우선하고, 없으면 이 큐의 기본값을 쓴다.
	const UNiagaraSoundCueData* CueData =
		Cast<UNiagaraSoundCueData>(Parameters.SourceObject.Get());

	UNiagaraSystem* SelectedEffect = CueData && CueData->Effect
		? CueData->Effect.Get()
		: Effect.Get();

	USoundBase* SelectedSound = CueData
		? GameplayCueFxLibrary::SelectRandomSound(CueData->RandomSounds, CueData->Sound.Get())
		: GameplayCueFxLibrary::SelectRandomSound(RandomSounds, Sound.Get());

	if (!SelectedEffect && !SelectedSound) {
		return false;
	}

	const FVector SpawnLocation = bUseParameterLocation
		? FVector(Parameters.Location)
		: MyTarget->GetActorLocation();
	const FRotator SpawnRotation = bUseTargetRotation && MyTarget
		? MyTarget->GetActorRotation()
		: Parameters.Normal.Rotation();

	if (SelectedEffect) {
		/*
		 * 부모는 스폰한 컴포넌트를 버리기 때문에 사용자 파라미터를 넣을 수 없다.
		 * 여기서는 반환값을 붙잡아 두고 값을 채운다.
		 */
		UNiagaraComponent* SpawnedEffect = GameplayCueFxLibrary::SpawnNiagaraAtLocation(
			World,
			SelectedEffect,
			SpawnLocation,
			SpawnRotation
		);

		ApplyUserParameters(SpawnedEffect, CueData);
	}

	if (SelectedSound) {
		GameplayCueFxLibrary::SpawnSoundAtLocation(
			World,
			SelectedSound,
			SpawnLocation,
			SpawnRotation,
			CueData ? CueData->SoundVolume : SoundVolume,
			CueData ? CueData->SoundPitch : SoundPitch,
			CueData ? CueData->SoundAttenuation.Get() : SoundAttenuation.Get()
		);
	}

#if ENABLE_DRAW_DEBUG
	if (Parameters.RawMagnitude > 0.0f
		&& Parameters.AggregatedSourceTags.HasTagExact(GameplayTagSystem::CueParameter_ShowDebug)) {
		DrawDebugSphere(World, SpawnLocation, Parameters.RawMagnitude, 32, FColor::Cyan, false, 2.0f, 0, 2.0f);
	}
#endif

	return true;
}

void UGCN_SpawnNiagaraAndSoundScaled::ApplyUserParameters(
	UNiagaraComponent* SpawnedEffect,
	const UNiagaraSoundCueData* CueData) const
{
	if (!SpawnedEffect) {
		return;
	}

	/*
	 * 없는 이름을 넘겨도 나이아가라는 에러 없이 무시한다. 그래서 크기가
	 * 안 변하면 이름 오타부터 의심해야 한다. 정확한 이름은 시스템 에디터의
	 * Parameters 패널에서 우클릭 -> Copy Reference로 확인할 수 있다.
	 */
	const FName ScaleName = CueData ? CueData->ScaleParameterName : ScaleParameterName;
	const float ScaleValue = CueData ? CueData->ScaleParameterValue : ScaleParameterValue;

	if (!ScaleName.IsNone()) {
		SpawnedEffect->SetVariableFloat(ScaleName, ScaleValue);
	}

	const TArray<FNiagaraUserFloatParameter>& Parameters =
		CueData ? CueData->FloatParameters : FloatParameters;

	for (const FNiagaraUserFloatParameter& FloatParameter : Parameters) {
		if (!FloatParameter.ParameterName.IsNone()) {
			SpawnedEffect->SetVariableFloat(FloatParameter.ParameterName, FloatParameter.Value);
		}
	}
}
