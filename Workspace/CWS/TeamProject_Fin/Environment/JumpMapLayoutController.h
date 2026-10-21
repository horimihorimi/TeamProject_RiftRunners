#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JumpMapLayoutController.generated.h"

class ABasePlayerController;
class APlayerState;
class AJumpMapLayoutController;
class UNiagaraComponent;

DECLARE_MULTICAST_DELEGATE_TwoParams(FJumpMapLayoutResult, AJumpMapLayoutController*, bool);

USTRUCT()
struct FJumpMapNiagaraState
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> Component;
	UPROPERTY()
	bool bAutoActivate = false;
};

USTRUCT()
struct FJumpMapActorState
{
	GENERATED_BODY()

	UPROPERTY()
	bool bHidden = false;
	UPROPERTY()
	bool bCollisionEnabled = true;
	UPROPERTY()
	bool bStartWithTickEnabled = false;
	UPROPERTY()
	float WindAudioRange = 0.0f;
	UPROPERTY()
	TArray<FJumpMapNiagaraState> NiagaraComponents;
};

USTRUCT(BlueprintType)
struct FJumpMapLayoutActorEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Jump Map")
	TObjectPtr<AActor> Actor;

	// 준비 버튼을 반복해서 눌러도 처음 저장한 활성 상태를 덮어쓰지 않는다.
	UPROPERTY(VisibleAnywhere, Category = "Jump Map")
	bool bHasOriginalState = false;
	UPROPERTY()
	TObjectPtr<AActor> OriginalActor;
	UPROPERTY()
	FJumpMapActorState OriginalState;
};

USTRUCT(BlueprintType)
struct FJumpMapLayoutProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Jump Map", meta = (ClampMin = "1", ClampMax = "4"))
	int32 PlayerCount = 1;
	UPROPERTY(EditAnywhere, Category = "Jump Map")
	TArray<FJumpMapLayoutActorEntry> Actors;
};

USTRUCT()
struct FJumpMapReplicatedLayout
{
	GENERATED_BODY()

	UPROPERTY()
	int32 PlayerCount = 0;
	UPROPERTY()
	bool bFailed = false;
	UPROPERTY()
	FString Failure;
};

/** 배치된 코스 중 이번 입장 인원에 맞는 코스만 남긴다. */
UCLASS(Blueprintable)
class TEAMPROJECT_FIN_API AJumpMapLayoutController : public AActor
{
	GENERATED_BODY()

public:
	AJumpMapLayoutController();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 기존 맵은 명시적으로 켜기 전까지 아무 것도 변경하지 않는다.
	UPROPERTY(EditInstanceOnly, Category = "Jump Map")
	bool bLayoutEnabled = false;
	UPROPERTY(EditInstanceOnly, Category = "Jump Map", meta = (EditCondition = "!bPrepared", EditConditionHides))
	TArray<FJumpMapLayoutProfile> Profiles;
	UPROPERTY(VisibleInstanceOnly, Category = "Jump Map")
	bool bPrepared = false;

	UFUNCTION(CallInEditor, Category = "Jump Map")
	void PrepareLayout();
	UFUNCTION(CallInEditor, Category = "Jump Map")
	void RestoreLayout();

#if WITH_EDITOR
	// 선택 액터를 한 번에 등록한다. 검사 실패 시 기존 배열을 유지한다.
	bool AddActorsToProfile(int32 PlayerCount, const TArray<AActor*>& Actors, int32& OutAddedCount, FString& OutError);
#endif

	// 스트리밍 입장 흐름이 준비 결과를 받을 수 있도록 먼저 delegate를 연결한다.
	void BeginLayout();
	void ReportParticipantReady(ABasePlayerController* Participant, bool bSuccess);
	void CancelPreparation();
	bool IsLocalLayoutReady() const { return bLocalReady; }
	bool HasLocalLayoutFailed() const { return bLocalFailed; }
	bool AreParticipantsReady() const { return bParticipantsReady; }
	bool HasPreparationFailed() const { return bLocalFailed || ReplicatedLayout.bFailed; }
	FString GetPreparationFailure() const { return PreparationFailure; }

	FJumpMapLayoutResult OnLayoutFinished;
	FJumpMapLayoutResult OnParticipantsReady;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(ReplicatedUsing = OnRep_Layout)
	FJumpMapReplicatedLayout ReplicatedLayout;
	UFUNCTION()
	void OnRep_Layout();

	bool ValidateLayout(bool bRequirePrepared, bool bAllowRemovedActors, FString& OutError,
		const TArray<FJumpMapLayoutProfile>* CandidateProfiles = nullptr) const;
	void PollPreparation();
	void ApplyLayout();
	void CheckParticipants();
	void CompleteLocalPreparation();
	void FailPreparation(const FString& Reason);
	void DisableActor(AActor* Actor) const;
	void RestoreActor(AActor* Actor, const FJumpMapActorState& State, bool bInEditor) const;

	FTimerHandle PreparationTimer;
	double PreparationStartedAt = 0.0;
	bool bBeginRequested = false;
	bool bCancelled = false;
	bool bApplied = false;
	bool bLocalReady = false;
	bool bLocalFailed = false;
	bool bLocalResultSent = false;
	bool bParticipantResultSent = false;
	bool bParticipantsReady = false;
	FString PreparationFailure;
	TSet<TWeakObjectPtr<APlayerState>> ExpectedParticipants;
	TSet<TWeakObjectPtr<APlayerState>> ReadyParticipants;
};
