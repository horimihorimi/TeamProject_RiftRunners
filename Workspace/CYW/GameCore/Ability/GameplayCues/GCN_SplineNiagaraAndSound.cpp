#include "GCN_SplineNiagaraAndSound.h"

#include "../../Tags/GameplayTagSystem.h"
#include "GameplayCueFxLibrary.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "TimerManager.h"

AGCN_SplineNiagaraAndSound::AGCN_SplineNiagaraAndSound()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bUniqueInstancePerSourceObject = false;

	CueRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CueRoot"));
	SetRootComponent(CueRoot);

	SplineComponent = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
	SplineComponent->SetupAttachment(CueRoot);
}

void AGCN_SplineNiagaraAndSound::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateTrackedEndpoints();
}

bool AGCN_SplineNiagaraAndSound::OnExecute_Implementation(
	AActor* MyTarget,
	const FGameplayCueParameters& Parameters)
{
	const FVector Start = Parameters.Location;
	const FVector End = Start + Parameters.Normal.GetSafeNormal() * Parameters.RawMagnitude;
	if (Parameters.RawMagnitude <= KINDA_SMALL_NUMBER || Start.Equals(End))
	{
		GameplayCueFinishedCallback();
		return false;
	}

	StartTarget = Parameters.EffectCauser.Get();
	if (USceneComponent* EndComponent = Parameters.TargetAttachComponent.Get())
	{
		EndTarget = EndComponent->GetOwner();
	}
	StartRelativeLocation = StartTarget.IsValid()
		? StartTarget->GetActorTransform().InverseTransformPosition(Start)
		: FVector::ZeroVector;
	EndRelativeLocation = EndTarget.IsValid()
		? EndTarget->GetActorTransform().InverseTransformPosition(End)
		: FVector::ZeroVector;
	LastStartLocation = Start;
	LastEndLocation = End;
	bShowDebug = Parameters.AggregatedSourceTags.HasTagExact(GameplayTagSystem::CueParameter_ShowDebug);
	InitializeSplineJitter(Start, End);
	BuildSpline(Start, End);
	if (bShowDebug)
	{
		DrawSplineDebug(0.0f);
	}
	SetActorTickEnabled(StartTarget.IsValid() || EndTarget.IsValid() || bShowDebug);
	bEffectFinished = true;
	bSoundFinished = true;

	if (Effect)
	{
		EffectComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
			Effect,
			CueRoot,
			NAME_None,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::KeepRelativeOffset,
			false,
			false
		);
		if (EffectComponent)
		{
			bEffectFinished = false;
			EffectComponent->OnSystemFinished.AddUniqueDynamic(this, &AGCN_SplineNiagaraAndSound::OnEffectFinished);
			EffectComponent->SetVariableObject(SplineUserParameter, SplineComponent);
			EffectComponent->Activate(true);
		}
	}

	if (Parameters.AggregatedSourceTags.HasTagExact(GameplayTagSystem::CueParameter_PlaySound))
	{
		if (USoundBase* SelectedSound = GameplayCueFxLibrary::SelectRandomSound(RandomSounds, Sound))
		{
			SoundComponent = GameplayCueFxLibrary::SpawnSoundAtLocation(
				GetWorld(),
				SelectedSound,
				Start,
				(End - Start).Rotation(),
				SoundVolume,
				GameplayCueFxLibrary::GetRandomPitch(SoundPitchMin, SoundPitchMax),
				SoundAttenuation
			);
			if (SoundComponent)
			{
				bSoundFinished = false;
				SoundComponent->OnAudioFinished.AddUniqueDynamic(this, &AGCN_SplineNiagaraAndSound::OnSoundFinished);
			}
		}
	}

	if (!EffectComponent && !SoundComponent)
	{
		GameplayCueFinishedCallback();
		return false;
	}

	GetWorldTimerManager().SetTimer(
		FinishTimerHandle,
		this,
		&AGCN_SplineNiagaraAndSound::FinishCue,
		FMath::Max(CueLifetime, 0.01f),
		false
	);
	return true;
}

bool AGCN_SplineNiagaraAndSound::Recycle()
{
	ClearCueTimer(FinishTimerHandle);
	StopEffects();
	SetActorTickEnabled(false);
	StartTarget.Reset();
	EndTarget.Reset();
	JitterSamples.Reset();
	bShowDebug = false;
	if (SplineComponent)
	{
		SplineComponent->ClearSplinePoints(false);
	}
	return Super::Recycle();
}

void AGCN_SplineNiagaraAndSound::FinishCue()
{
	StopEffects();
	GameplayCueFinishedCallback();
}

void AGCN_SplineNiagaraAndSound::UpdateTrackedEndpoints()
{
	FVector CurrentStart = LastStartLocation;
	FVector CurrentEnd = LastEndLocation;
	if (StartTarget.IsValid())
	{
		CurrentStart = StartTarget->GetActorTransform().TransformPosition(StartRelativeLocation);
	}
	if (EndTarget.IsValid())
	{
		CurrentEnd = EndTarget->GetActorTransform().TransformPosition(EndRelativeLocation);
	}

	if (!CurrentStart.Equals(LastStartLocation, 0.1f) || !CurrentEnd.Equals(LastEndLocation, 0.1f))
	{
		LastStartLocation = CurrentStart;
		LastEndLocation = CurrentEnd;
		BuildSpline(CurrentStart, CurrentEnd);
	}
	if (bShowDebug)
	{
		DrawSplineDebug(0.0f);
	}
}

void AGCN_SplineNiagaraAndSound::OnEffectFinished(UNiagaraComponent* FinishedComponent)
{
	if (FinishedComponent != EffectComponent)
	{
		return;
	}

	EffectComponent->OnSystemFinished.RemoveDynamic(this, &AGCN_SplineNiagaraAndSound::OnEffectFinished);
	EffectComponent->DestroyComponent();
	EffectComponent = nullptr;
	bEffectFinished = true;
	TryFinishCue();
}

void AGCN_SplineNiagaraAndSound::OnSoundFinished()
{
	if (SoundComponent)
	{
		SoundComponent->OnAudioFinished.RemoveDynamic(this, &AGCN_SplineNiagaraAndSound::OnSoundFinished);
	}
	SoundComponent = nullptr;
	bSoundFinished = true;
	TryFinishCue();
}

void AGCN_SplineNiagaraAndSound::TryFinishCue()
{
	if (!bEffectFinished || !bSoundFinished)
	{
		return;
	}

	ClearCueTimer(FinishTimerHandle);
	GameplayCueFinishedCallback();
}

void AGCN_SplineNiagaraAndSound::InitializeSplineJitter(const FVector& Start, const FVector& End)
{
	JitterSamples.Reset();
	const uint32 Seed = HashCombine(GetTypeHash(Start), GetTypeHash(End));
	FRandomStream RandomStream(static_cast<int32>(Seed));
	const int32 PointCount = FMath::Max(IntermediatePointCount, 0);
	JitterSamples.Reserve(PointCount);
	for (int32 PointIndex = 0; PointIndex < PointCount; ++PointIndex)
	{
		JitterSamples.Emplace(
			RandomStream.FRandRange(-1.0f, 1.0f),
			RandomStream.FRandRange(-1.0f, 1.0f)
		);
	}
}

void AGCN_SplineNiagaraAndSound::BuildSpline(const FVector& Start, const FVector& End)
{
	if (!SplineComponent)
	{
		return;
	}

	SplineComponent->ClearSplinePoints(false);
	SplineComponent->AddSplinePoint(Start, ESplineCoordinateSpace::World, false);

	const FVector Direction = (End - Start).GetSafeNormal();
	FVector HorizontalAxis = FVector::CrossProduct(Direction, FVector::UpVector).GetSafeNormal();
	if (HorizontalAxis.IsNearlyZero())
	{
		HorizontalAxis = FVector::RightVector;
	}
	const FVector VerticalAxis = FVector::CrossProduct(HorizontalAxis, Direction).GetSafeNormal();

	const int32 PointCount = JitterSamples.Num();
	for (int32 PointIndex = 1; PointIndex <= PointCount; ++PointIndex)
	{
		const float Alpha = 1.0f * PointIndex / (PointCount + 1);
		const float Envelope = FMath::Sin(Alpha * UE_PI);
		const FVector2D Jitter = JitterSamples[PointIndex - 1];
		const FVector Offset =
			HorizontalAxis * Jitter.X * HorizontalJitter * Envelope
			+ VerticalAxis * Jitter.Y * VerticalJitter * Envelope;
		SplineComponent->AddSplinePoint(FMath::Lerp(Start, End, Alpha) + Offset, ESplineCoordinateSpace::World, false);
	}

	SplineComponent->AddSplinePoint(End, ESplineCoordinateSpace::World, false);
	for (int32 PointIndex = 0; PointIndex < SplineComponent->GetNumberOfSplinePoints(); ++PointIndex)
	{
		SplineComponent->SetSplinePointType(PointIndex, ESplinePointType::CurveClamped, false);
	}
	SplineComponent->UpdateSpline();
}

void AGCN_SplineNiagaraAndSound::DrawSplineDebug(float Duration) const
{
	UWorld* World = GetWorld();
	if (!World || !SplineComponent)
	{
		return;
	}

	constexpr int32 DebugSegmentCount = 32;
	const float SplineLength = SplineComponent->GetSplineLength();
	FVector PreviousPoint = SplineComponent->GetLocationAtDistanceAlongSpline(0.0f, ESplineCoordinateSpace::World);
	for (int32 SegmentIndex = 1; SegmentIndex <= DebugSegmentCount; ++SegmentIndex)
	{
		const float Distance = SplineLength * SegmentIndex / DebugSegmentCount;
		const FVector CurrentPoint = SplineComponent->GetLocationAtDistanceAlongSpline(
			Distance,
			ESplineCoordinateSpace::World
		);
		DrawDebugLine(World, PreviousPoint, CurrentPoint, FColor::Yellow, false, Duration, 0, 2.0f);
		PreviousPoint = CurrentPoint;
	}

	for (int32 PointIndex = 0; PointIndex < SplineComponent->GetNumberOfSplinePoints(); ++PointIndex)
	{
		DrawDebugSphere(
			World,
			SplineComponent->GetLocationAtSplinePoint(PointIndex, ESplineCoordinateSpace::World),
			8.0f,
			8,
			FColor::Magenta,
			false,
			Duration
		);
	}
}

void AGCN_SplineNiagaraAndSound::StopEffects()
{
	SetActorTickEnabled(false);
	if (IsValid(EffectComponent))
	{
		EffectComponent->OnSystemFinished.RemoveDynamic(this, &AGCN_SplineNiagaraAndSound::OnEffectFinished);
	}
	UNiagaraComponent* EffectComponentPtr = EffectComponent.Get();
	StopAndDestroyNiagara(EffectComponentPtr);
	EffectComponent = EffectComponentPtr;
	bEffectFinished = true;

	if (IsValid(SoundComponent))
	{
		SoundComponent->OnAudioFinished.RemoveDynamic(this, &AGCN_SplineNiagaraAndSound::OnSoundFinished);
	}
	UAudioComponent* SoundComponentPtr = SoundComponent.Get();
	StopAndDestroySound(SoundComponentPtr);
	SoundComponent = SoundComponentPtr;
	bSoundFinished = true;
}
