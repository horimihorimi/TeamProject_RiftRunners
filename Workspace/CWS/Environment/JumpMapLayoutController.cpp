#include "JumpMapLayoutController.h"

#include "Engine/Level.h"
#include "Engine/LevelScriptActor.h"
#include "Engine/World.h"
#include "Environment/WindVolumeActor.h"
#include "GameCore/BasePlayerController.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "PCGMap/MapBuild/SectorBuild/Event/PlacedEvent/ActorVisibility.h"
#include "PCGMap/MapBuild/SectorBuild/Event/PlacedEvent/LevelTriggerEvent.h"
#include "PCGMap/MapBuild/SectorBuild/Event/PlacedEvent/WaitAllEvent.h"
#include "TimerManager.h"
#include "UObject/ScriptInterface.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogJumpMapLayout, Log, All);

namespace
{
	constexpr double PreparationTimeout = 30.0;
	constexpr float PreparationPollInterval = 0.05f;

	bool IsParticipant(const APlayerState* PlayerState)
	{
		return IsValid(PlayerState) && !PlayerState->IsInactive()
			&& !PlayerState->IsOnlyASpectator() && !PlayerState->IsABot()
			&& IsValid(Cast<ABasePlayerController>(PlayerState->GetPlayerController()));
	}

	// 편집 가능한 직접 참조를 검사한다. 보호된 이벤트 대상도 리플렉션으로 읽기만 한다.
	void VisitActorReferences(const FProperty* Property, const void* Value, TFunctionRef<void(AActor*)> Visit)
	{
		if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
		{
			if (AActor* Actor = Cast<AActor>(ObjectProperty->GetObjectPropertyValue(Value))) Visit(Actor);
		}
		else if (CastField<FInterfaceProperty>(Property))
		{
			if (AActor* Actor = Cast<AActor>(static_cast<const FScriptInterface*>(Value)->GetObject())) Visit(Actor);
		}
		else if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
		{
			FScriptArrayHelper Helper(ArrayProperty, Value);
			for (int32 Index = 0; Index < Helper.Num(); ++Index)
				VisitActorReferences(ArrayProperty->Inner, Helper.GetRawPtr(Index), Visit);
		}
		else if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			for (TFieldIterator<FProperty> It(StructProperty->Struct); It; ++It)
				VisitActorReferences(*It, It->ContainerPtrToValuePtr<void>(Value), Visit);
		}
		else if (const FMapProperty* MapProperty = CastField<FMapProperty>(Property))
		{
			FScriptMapHelper Helper(MapProperty, Value);
			for (int32 Index = 0; Index < Helper.GetMaxIndex(); ++Index)
			{
				if (!Helper.IsValidIndex(Index)) continue;
				VisitActorReferences(MapProperty->KeyProp, Helper.GetKeyPtr(Index), Visit);
				VisitActorReferences(MapProperty->ValueProp, Helper.GetValuePtr(Index), Visit);
			}
		}
		else if (const FSetProperty* SetProperty = CastField<FSetProperty>(Property))
		{
			FScriptSetHelper Helper(SetProperty, Value);
			for (int32 Index = 0; Index < Helper.GetMaxIndex(); ++Index)
				if (Helper.IsValidIndex(Index)) VisitActorReferences(SetProperty->ElementProp, Helper.GetElementPtr(Index), Visit);
		}
	}
}

AJumpMapLayoutController::AJumpMapLayoutController()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	for (int32 Count = 1; Count <= 4; ++Count)
		Profiles.AddDefaulted_GetRef().PlayerCount = Count;
}

void AJumpMapLayoutController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AJumpMapLayoutController, ReplicatedLayout);
}

void AJumpMapLayoutController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelPreparation();
	Super::EndPlay(EndPlayReason);
}

void AJumpMapLayoutController::PrepareLayout()
{
#if WITH_EDITOR
	if (!GetWorld() || GetWorld()->IsGameWorld()) return;
	FString Error;
	if (!ValidateLayout(false, false, Error))
	{
		UE_LOG(LogJumpMapLayout, Error, TEXT("배치 준비 실패: %s"), *Error);
		return;
	}

	Modify();
	for (FJumpMapLayoutProfile& Profile : Profiles)
	{
		for (FJumpMapLayoutActorEntry& Entry : Profile.Actors)
		{
			AActor* Actor = Entry.Actor;
			Actor->Modify();
			if (!Entry.bHasOriginalState)
			{
				Entry.OriginalActor = Actor;
				Entry.OriginalState.bHidden = Actor->IsHidden();
				Entry.OriginalState.bCollisionEnabled = Actor->GetActorEnableCollision();
				Entry.OriginalState.bStartWithTickEnabled = Actor->PrimaryActorTick.bStartWithTickEnabled;
				if (AWindVolumeActor* Wind = Cast<AWindVolumeActor>(Actor))
				{
					Entry.OriginalState.WindAudioRange = Wind->AudioRange;
				}
				TInlineComponentArray<UNiagaraComponent*> NiagaraComponents(Actor);
				for (UNiagaraComponent* Component : NiagaraComponents)
				{
					FJumpMapNiagaraState& State = Entry.OriginalState.NiagaraComponents.AddDefaulted_GetRef();
					State.Component = Component;
					State.bAutoActivate = Component->bAutoActivate;
				}
				Entry.bHasOriginalState = true;
			}
			Actor->SetActorHiddenInGame(true);
			Actor->SetActorEnableCollision(false);
			Actor->PrimaryActorTick.bStartWithTickEnabled = false;
			Actor->SetActorTickEnabled(false);
			if (AWindVolumeActor* Wind = Cast<AWindVolumeActor>(Actor))
				Wind->AudioRange = 0.0f;
			TInlineComponentArray<UNiagaraComponent*> NiagaraComponents(Actor);
			for (UNiagaraComponent* Component : NiagaraComponents)
			{
				Component->Modify();
				Component->bAutoActivate = false;
				Component->Deactivate();
				Component->MarkPackageDirty();
			}
			Actor->MarkPackageDirty();
		}
	}
	bPrepared = true;
	MarkPackageDirty();
	UE_LOG(LogJumpMapLayout, Log, TEXT("점프맵 배치를 준비했습니다. 맵을 저장하세요."));
#endif
}

void AJumpMapLayoutController::RestoreLayout()
{
#if WITH_EDITOR
	if (!GetWorld() || GetWorld()->IsGameWorld()) return;
	Modify();
	for (FJumpMapLayoutProfile& Profile : Profiles)
	{
		for (FJumpMapLayoutActorEntry& Entry : Profile.Actors)
		{
			if (Entry.bHasOriginalState && IsValid(Entry.OriginalActor))
			{
				Entry.OriginalActor->Modify();
				for (const FJumpMapNiagaraState& State : Entry.OriginalState.NiagaraComponents)
					if (IsValid(State.Component)) State.Component->Modify();
				RestoreActor(Entry.OriginalActor, Entry.OriginalState, true);
				Entry.OriginalActor->MarkPackageDirty();
			}
			Entry.bHasOriginalState = false;
			Entry.OriginalActor = nullptr;
			Entry.OriginalState = FJumpMapActorState();
		}
	}
	bPrepared = false;
	MarkPackageDirty();
#endif
}

#if WITH_EDITOR
bool AJumpMapLayoutController::AddActorsToProfile(int32 PlayerCount, const TArray<AActor*>& Actors,
	int32& OutAddedCount, FString& OutError)
{
	OutAddedCount = 0;
	OutError.Reset();
	if (!GetWorld() || GetWorld()->IsGameWorld() || bPrepared)
	{
		OutError = TEXT("에디터에서 RestoreLayout을 수행한 상태로 등록하세요.");
		return false;
	}
	if (PlayerCount < 1 || PlayerCount > 4 || Actors.IsEmpty())
	{
		OutError = TEXT("인원을 1~4로 지정하고 등록할 액터를 선택하세요.");
		return false;
	}

	// 후보 배열만 검사해서 실패한 등록이 원본 배열이나 실행 취소 기록에 남지 않게 한다.
	TArray<FJumpMapLayoutProfile> Candidates = Profiles;
	FJumpMapLayoutProfile* Target = Candidates.FindByPredicate(
		[PlayerCount](const FJumpMapLayoutProfile& Profile) { return Profile.PlayerCount == PlayerCount; });
	if (!Target)
	{
		OutError = TEXT("해당 인원의 프로필이 없습니다. Profiles의 1~4인 구성을 확인하세요.");
		return false;
	}
	TSet<AActor*> Registered;
	for (const FJumpMapLayoutActorEntry& Entry : Target->Actors)
	{
		Registered.Add(Entry.Actor.Get());
	}
	int32 AddedCount = 0;
	for (AActor* Actor : Actors)
	{
		if (!IsValid(Actor))
		{
			OutError = TEXT("선택 목록에 유효하지 않은 액터가 있습니다.");
			return false;
		}
		if (Registered.Contains(Actor))
		{
			continue;
		}
		Registered.Add(Actor);
		Target->Actors.AddDefaulted_GetRef().Actor = Actor;
		++AddedCount;
	}
	if (!ValidateLayout(false, false, OutError, &Candidates))
	{
		return false;
	}
	if (AddedCount > 0)
	{
		Modify();
		Profiles = MoveTemp(Candidates);
		MarkPackageDirty();
	}
	OutAddedCount = AddedCount;
	return true;
}
#endif

bool AJumpMapLayoutController::ValidateLayout(bool bRequirePrepared, bool bAllowRemovedActors, FString& OutError,
	const TArray<FJumpMapLayoutProfile>* CandidateProfiles) const
{
	const TArray<FJumpMapLayoutProfile>& LayoutProfiles = CandidateProfiles ? *CandidateProfiles : Profiles;
	if (LayoutProfiles.Num() != 4 || (bRequirePrepared && !bPrepared))
	{
		OutError = TEXT("1~4인 프로필을 준비한 뒤 맵을 저장해야 합니다.");
		return false;
	}
	TSet<int32> Counts;
	TMap<const AActor*, int32> Membership;
	for (const FJumpMapLayoutProfile& Profile : LayoutProfiles)
	{
		if (Profile.PlayerCount < 1 || Profile.PlayerCount > 4 || Counts.Contains(Profile.PlayerCount))
		{
			OutError = TEXT("프로필 인원은 1~4이며 중복될 수 없습니다.");
			return false;
		}
		Counts.Add(Profile.PlayerCount);
		for (const FJumpMapLayoutActorEntry& Entry : Profile.Actors)
		{
			const AActor* Actor = Entry.Actor;
			if (!IsValid(Actor))
			{
				if (bAllowRemovedActors && Profile.PlayerCount != ReplicatedLayout.PlayerCount) continue;
				OutError = TEXT("프로필에 유효하지 않은 액터가 있습니다.");
				return false;
			}
			if (Actor->GetLevel() != GetLevel() || Actor->IsA<APlayerStart>() || Actor->IsA<AController>()
				|| Actor->IsA<APawn>() || Actor->IsA<ALevelScriptActor>()
				|| Actor->IsA<AJumpMapLayoutController>() || Membership.Contains(Actor))
			{
				OutError = FString::Printf(TEXT("등록할 수 없는 액터 또는 중복 등록: %s"), *Actor->GetName());
				return false;
			}
			if (Actor->IsA<ALevelTriggerEvent>() || Actor->IsA<AWaitAllEvent>()
				|| Actor->GetClass()->IsFunctionImplementedInScript(TEXT("ReceiveBeginPlay"))
				|| (Actor->IsA<AActorVisibility>() && CastChecked<AActorVisibility>(Actor)->bApplyOnBeginPlay))
			{
				OutError = FString::Printf(TEXT("자동 BeginPlay 실행 액터는 인원별 코스에 등록할 수 없습니다: %s"), *Actor->GetName());
				return false;
			}
			if ((Entry.bHasOriginalState && Entry.OriginalActor != Actor) || (bRequirePrepared && !Entry.bHasOriginalState))
			{
				OutError = FString::Printf(TEXT("원본 상태가 없는 액터: %s"), *Actor->GetName());
				return false;
			}
			// 클라이언트 표시 상태는 서버의 복제 값이 먼저 도착할 수 있다.
			if (bRequirePrepared && !bAllowRemovedActors)
			{
				const AWindVolumeActor* Wind = Cast<AWindVolumeActor>(Actor);
				if (!Actor->IsHidden() || Actor->GetActorEnableCollision() || Actor->PrimaryActorTick.bStartWithTickEnabled
					|| (Wind && Wind->AudioRange != 0.0f))
				{
					OutError = FString::Printf(TEXT("초기 비활성 상태가 변경되었습니다: %s"), *Actor->GetName());
					return false;
				}
				TInlineComponentArray<UNiagaraComponent*> NiagaraComponents(Actor);
				if (NiagaraComponents.Num() != Entry.OriginalState.NiagaraComponents.Num())
				{
					OutError = FString::Printf(TEXT("준비 후 Niagara 구성이 변경되었습니다: %s"), *Actor->GetName());
					return false;
				}
				for (UNiagaraComponent* Component : NiagaraComponents)
				{
					const bool bCaptured = Entry.OriginalState.NiagaraComponents.ContainsByPredicate(
						[Component](const FJumpMapNiagaraState& State) { return State.Component == Component; });
					if (!bCaptured || Component->bAutoActivate)
					{
						OutError = FString::Printf(TEXT("Niagara 초기 상태가 변경되었습니다: %s"), *Actor->GetName());
						return false;
					}
				}
			}
			Membership.Add(Actor, Profile.PlayerCount);
		}
	}

	// 서버/에디터에서만 원본 레벨 전체의 직접 참조를 검사한다. 클라이언트의 삭제 복제 순서는 허용한다.
	if (!bAllowRemovedActors && GetLevel())
	{
		for (AActor* Source : GetLevel()->Actors)
		{
			if (!IsValid(Source) || Source == this || Source->IsEditorOnly()) continue;
			const int32* SourceCount = Membership.Find(Source);
			const auto CheckReference = [&](AActor* Target)
			{
				if (!OutError.IsEmpty() || !IsValid(Target)) return;
				const int32* TargetCount = Membership.Find(Target);
				if ((SourceCount && Target->GetLevel() != GetLevel())
					|| (TargetCount && (!SourceCount || *SourceCount != *TargetCount)))
					OutError = FString::Printf(TEXT("코스 밖 직접 참조: %s -> %s"), *Source->GetName(), *Target->GetName());
			};
			if (AActor* Parent = Source->GetAttachParentActor()) CheckReference(Parent);
			for (TFieldIterator<FProperty> It(Source->GetClass()); It; ++It)
			{
				if (It->HasAnyPropertyFlags(CPF_EditorOnly)
					|| (!Source->IsA<ALevelScriptActor>() && !It->HasAnyPropertyFlags(CPF_Edit))) continue;
				VisitActorReferences(*It, It->ContainerPtrToValuePtr<void>(Source), CheckReference);
			}
			if (!OutError.IsEmpty()) return false;
		}
	}
	return true;
}

void AJumpMapLayoutController::BeginLayout()
{
	if (bBeginRequested || bCancelled) return;
	bBeginRequested = true;
	PreparationStartedAt = GetWorld()->GetTimeSeconds();
	if (HasAuthority())
	{
		FString Error;
		if (bLayoutEnabled && !ValidateLayout(true, false, Error))
		{
			FailPreparation(Error);
			return;
		}
	}
	GetWorldTimerManager().SetTimer(PreparationTimer, this, &ThisClass::PollPreparation, PreparationPollInterval, true);
	PollPreparation();
}

void AJumpMapLayoutController::OnRep_Layout()
{
	if (bBeginRequested && !bCancelled) PollPreparation();
}

void AJumpMapLayoutController::PollPreparation()
{
	if (!bBeginRequested || bCancelled || bLocalFailed) return;
	if (ReplicatedLayout.bFailed)
	{
		FailPreparation(ReplicatedLayout.Failure);
		return;
	}
	if (GetWorld()->GetTimeSeconds() - PreparationStartedAt >= PreparationTimeout)
	{
		FailPreparation(TEXT("점프맵 준비 시간 초과(30초)."));
		return;
	}
	if (HasAuthority() && ReplicatedLayout.PlayerCount == 0)
	{
		TSet<TWeakObjectPtr<APlayerState>> Participants;
		const AGameStateBase* GameState = GetWorld()->GetGameState();
		if (GameState)
		{
			for (APlayerState* PlayerState : GameState->PlayerArray)
			{
				if (IsParticipant(PlayerState)) Participants.Add(PlayerState);
			}
		}
		if (Participants.IsEmpty() || Participants.Num() > 4)
		{
			FailPreparation(TEXT("참가 인원을 1~4명으로 확정할 수 없습니다."));
			return;
		}
		ExpectedParticipants = MoveTemp(Participants);
		ReplicatedLayout.PlayerCount = ExpectedParticipants.Num();
		ForceNetUpdate();
	}
	if (ReplicatedLayout.PlayerCount == 0)
	{
		return;
	}
	if (!bLayoutEnabled && !bApplied)
	{
		bApplied = true;
	}
	if (!bApplied)
	{
		FString Error;
		if (!ValidateLayout(true, !HasAuthority(), Error))
		{
			FailPreparation(Error);
			return;
		}
		// 모든 기믹의 초기화가 끝난 다음에만 Tick과 overlap을 켠다.
		for (const FJumpMapLayoutProfile& Profile : Profiles)
			for (const FJumpMapLayoutActorEntry& Entry : Profile.Actors)
				if (IsValid(Entry.Actor) && !Entry.Actor->HasActorBegunPlay())
				{
					return;
				}
		ApplyLayout();
		if (bLocalFailed || bCancelled) return;
	}
	if (!bLocalReady)
	{
		for (const FJumpMapLayoutProfile& Profile : Profiles)
		{
			if (!bLayoutEnabled || Profile.PlayerCount == ReplicatedLayout.PlayerCount) continue;
			for (const FJumpMapLayoutActorEntry& Entry : Profile.Actors)
				if (IsValid(Entry.Actor))
				{
					return;
				}
		}
		CompleteLocalPreparation();
	}
	if (HasAuthority()) CheckParticipants();
}

void AJumpMapLayoutController::DisableActor(AActor* Actor) const
{
	Actor->SetActorEnableCollision(false);
	Actor->SetActorHiddenInGame(true);
	Actor->SetActorTickEnabled(false);
	if (AWindVolumeActor* Wind = Cast<AWindVolumeActor>(Actor))
		Wind->AudioRange = 0.0f;
	TInlineComponentArray<UNiagaraComponent*> NiagaraComponents(Actor);
	for (UNiagaraComponent* Component : NiagaraComponents) Component->Deactivate();
}

void AJumpMapLayoutController::RestoreActor(AActor* Actor, const FJumpMapActorState& State, bool bInEditor) const
{
	Actor->PrimaryActorTick.bStartWithTickEnabled = State.bStartWithTickEnabled;
	Actor->SetActorTickEnabled(State.bStartWithTickEnabled);
	Actor->SetActorHiddenInGame(State.bHidden);
	if (AWindVolumeActor* Wind = Cast<AWindVolumeActor>(Actor))
		Wind->AudioRange = State.WindAudioRange;
	for (const FJumpMapNiagaraState& NiagaraState : State.NiagaraComponents)
	{
		if (!IsValid(NiagaraState.Component)) continue;
		NiagaraState.Component->bAutoActivate = NiagaraState.bAutoActivate;
		if (NiagaraState.bAutoActivate) NiagaraState.Component->Activate(true);
		else NiagaraState.Component->Deactivate();
#if WITH_EDITOR
		if (bInEditor) NiagaraState.Component->MarkPackageDirty();
#endif
	}
	Actor->SetActorEnableCollision(State.bCollisionEnabled);
}

void AJumpMapLayoutController::ApplyLayout()
{
	// 삭제 대상을 먼저 차단한다. 네트워크 파괴가 늦어도 충돌과 이펙트는 작동하지 않는다.
	for (const FJumpMapLayoutProfile& Profile : Profiles)
	{
		if (Profile.PlayerCount == ReplicatedLayout.PlayerCount) continue;
		for (const FJumpMapLayoutActorEntry& Entry : Profile.Actors)
		{
			AActor* Actor = Entry.Actor;
			if (!IsValid(Actor)) continue;
			DisableActor(Actor);
			if (HasAuthority() || !Actor->GetIsReplicated())
			{
				if (!Actor->Destroy(false, false))
				{
					FailPreparation(FString::Printf(TEXT("액터 삭제 실패: %s"), *Actor->GetName()));
					return;
				}
			}
		}
	}
	for (const FJumpMapLayoutProfile& Profile : Profiles)
		if (Profile.PlayerCount == ReplicatedLayout.PlayerCount)
			for (const FJumpMapLayoutActorEntry& Entry : Profile.Actors)
				if (IsValid(Entry.Actor))
				{
					RestoreActor(Entry.Actor, Entry.OriginalState, false);
				}
	bApplied = true;
}

void AJumpMapLayoutController::CompleteLocalPreparation()
{
	bLocalReady = true;
	if (!HasAuthority()) GetWorldTimerManager().ClearTimer(PreparationTimer);
	if (!bLocalResultSent)
	{
		bLocalResultSent = true;
		OnLayoutFinished.Broadcast(this, true);
	}
}

void AJumpMapLayoutController::ReportParticipantReady(ABasePlayerController* Participant, bool bSuccess)
{
	if (!HasAuthority() || !bBeginRequested || bCancelled || bParticipantResultSent || !IsValid(Participant)) return;
	APlayerState* PlayerState = Participant->PlayerState;
	if (!ExpectedParticipants.Contains(PlayerState) || !IsParticipant(PlayerState)) return;
	if (!bSuccess)
	{
		FailPreparation(TEXT("참가자의 점프맵 배치 준비가 실패했습니다."));
		return;
	}
	ReadyParticipants.Add(PlayerState);
	CheckParticipants();
}

void AJumpMapLayoutController::CheckParticipants()
{
	if (!HasAuthority() || bParticipantResultSent || bCancelled || bLocalFailed) return;
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	for (auto It = ExpectedParticipants.CreateIterator(); It; ++It)
	{
		APlayerState* PlayerState = It->Get();
		if (!IsParticipant(PlayerState) || !GameState || !GameState->PlayerArray.Contains(PlayerState)) It.RemoveCurrent();
	}
	if (!bLocalReady) return;
	for (const TWeakObjectPtr<APlayerState>& PlayerState : ExpectedParticipants)
		if (!ReadyParticipants.Contains(PlayerState))
		{
			return;
		}
	bParticipantsReady = true;
	bParticipantResultSent = true;
	GetWorldTimerManager().ClearTimer(PreparationTimer);
	OnParticipantsReady.Broadcast(this, true);
}

void AJumpMapLayoutController::FailPreparation(const FString& Reason)
{
	if (bLocalFailed || bCancelled) return;
	bLocalFailed = true;
	PreparationFailure = Reason;
	GetWorldTimerManager().ClearTimer(PreparationTimer);
	UE_LOG(LogJumpMapLayout, Error, TEXT("점프맵 준비 실패: %s"), *Reason);
	if (HasAuthority())
	{
		ReplicatedLayout.bFailed = true;
		ReplicatedLayout.Failure = Reason;
		ForceNetUpdate();
	}
	if (!bLocalResultSent)
	{
		bLocalResultSent = true;
		OnLayoutFinished.Broadcast(this, false);
	}
	if (HasAuthority() && !bParticipantResultSent)
	{
		bParticipantResultSent = true;
		OnParticipantsReady.Broadcast(this, false);
	}
}

void AJumpMapLayoutController::CancelPreparation()
{
	bCancelled = true;
	if (GetWorld()) GetWorldTimerManager().ClearTimer(PreparationTimer);
	ExpectedParticipants.Empty();
	ReadyParticipants.Empty();
}

