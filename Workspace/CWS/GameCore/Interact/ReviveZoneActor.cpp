#include "ReviveZoneActor.h"
#include "Components/CapsuleComponent.h"
#include "NiagaraComponent.h"
#include "Components/AudioComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameCore/Character/BaseCharacter.h"
#include "GameCore/Character/PlayerCharacter.h"
#include "AbilitySystemComponent.h"
#include "GameCore/Tags/GameplayTagSystem.h"
#include "GameCore/BasePlayerController.h"
#include "GameCore/BasePlayerState.h"
#include "GameCore/UI/ReviveGaugeWidget.h"
#include "GameCore/Attribute/CharacterAttributeSet.h"

AReviveZoneActor::AReviveZoneActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true; // 중요도가 항상 높도록 유지

	ReviveZoneCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("ReviveZoneCapsule"));
	RootComponent = ReviveZoneCapsule;
	ReviveZoneCapsule->SetCollisionProfileName(TEXT("Trigger"));
	ReviveZoneCapsule->SetCapsuleSize(ZoneRadius, ZoneHalfHeight);
	
	ReviveEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("ReviveEffect"));
	ReviveEffect->SetupAttachment(RootComponent);

	ReviveAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("ReviveAudio"));
	ReviveAudio->SetupAttachment(RootComponent);
	ReviveAudio->bAutoActivate = false; // 스폰되자마자 스스로 켜지는 것 방지

	CurrentState = EReviveZoneState::Idle;
	CurrentProgress = 0.0f;
	VisualProgress = 0.0f;
}

void AReviveZoneActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AReviveZoneActor, CurrentState);
	DOREPLIFETIME(AReviveZoneActor, CurrentProgress);
	DOREPLIFETIME(AReviveZoneActor, DeadPlayer);
}

void AReviveZoneActor::BeginPlay()
{
	Super::BeginPlay();

	// 블루프린트에서 수정한 크기를 캡슐에 반영
	ReviveZoneCapsule->SetCapsuleSize(ZoneRadius, ZoneHalfHeight);

	if (HasAuthority())
	{
		ReviveZoneCapsule->OnComponentBeginOverlap.AddDynamic(this, &AReviveZoneActor::OnOverlapBegin);
		ReviveZoneCapsule->OnComponentEndOverlap.AddDynamic(this, &AReviveZoneActor::OnOverlapEnd);
	}

	// 틱 대신 초당 20번(0.05초) 도는 가벼운 타이머 실행
	GetWorldTimerManager().SetTimer(TH_UpdateLoop, this, &AReviveZoneActor::UpdateLoop, 0.05f, true);
}

void AReviveZoneActor::InitializeZone(ABaseCharacter* InDeadPlayer)
{
	DeadPlayer = InDeadPlayer;

	if (HasAuthority() && ReviveZoneCapsule)
	{
		// 스폰 되기 전부터 이미 구역 안에 서 있던 플레이어들을 강제로 수동 감지 (최적화를 위해 PlayerCharacter만 가져옵니다)
		TArray<AActor*> OverlappingActors;
		ReviveZoneCapsule->GetOverlappingActors(OverlappingActors, APlayerCharacter::StaticClass());
		
		for (AActor* Actor : OverlappingActors)
		{
			// 수동으로 오버랩 이벤트를 한 번 실행시켜서 배열에 넣음
			OnOverlapBegin(ReviveZoneCapsule, Actor, nullptr, 0, false, FHitResult());
		}
	}
}

void AReviveZoneActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(TH_UpdateLoop);
	
	// 액터 파괴 시 떠 있는 UI 강제 정리 (클라이언트)
	if (LocalReviveWidget)
	{
		LocalReviveWidget->UpdateGauge(0.0f);
		LocalReviveWidget->RemoveFromParent();
		LocalReviveWidget = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void AReviveZoneActor::UpdateLoop()
{
	float DeltaTime = 0.05f;

	if (HasAuthority())
	{
		// 유효하지 않은 구조자(사망했거나 접속 종료 등) 배열 정리
		for (int32 i = ActiveRescuers.Num() - 1; i >= 0; --i)
		{
			ABaseCharacter* Rescuer = ActiveRescuers[i];
			if (!IsValid(Rescuer) || Rescuer->GetCharacterAttributeSet()->GetHealth() <= 0.0f || Rescuer->GetAbilitySystemComponent()->HasMatchingGameplayTag(GameplayTagSystem::State_Dead))
			{
				ActiveRescuers.RemoveAt(i);
			}
		}

		EReviveZoneState OldState = CurrentState;

		if (ActiveRescuers.Num() > 0 && CurrentState != EReviveZoneState::Reviving)
		{
			CurrentState = EReviveZoneState::Reviving;
		}
		else if (ActiveRescuers.Num() == 0 && CurrentState == EReviveZoneState::Reviving)
		{
			CurrentState = EReviveZoneState::GracePeriod;
			GraceTimer = 0.0f;
		}

		switch (CurrentState)
		{
		case EReviveZoneState::Reviving:
			CurrentProgress += DeltaTime;
			if (CurrentProgress >= MaxReviveTime)
			{
				CurrentProgress = MaxReviveTime;
				CurrentState = EReviveZoneState::Idle;
				FinishRevive();
			}
			break;

		case EReviveZoneState::GracePeriod:
			GraceTimer += DeltaTime;
			if (GraceTimer >= GracePeriodTime)
			{
				CurrentState = EReviveZoneState::Decaying;
			}
			break;

		case EReviveZoneState::Decaying:
			CurrentProgress -= (DeltaTime * DecayRate);
			if (CurrentProgress <= 0.0f)
			{
				CurrentProgress = 0.0f;
				CurrentState = EReviveZoneState::Idle;
			}
			break;
		case EReviveZoneState::Idle:
		default:
			break;
		}

		// 호스트(리슨 서버) 플레이어의 경우 OnRep가 자동으로 불리지 않으므로 수동 호출
		if (OldState != CurrentState && GetNetMode() != NM_DedicatedServer)
		{
			OnRep_CurrentState();
		}
	}

	// 리슨 서버(호스트) 플레이어와 클라이언트 모두 시각/UI 처리가 필요함
	if (GetNetMode() != NM_DedicatedServer)
	{
		// FinishRevive() 내부에서 타이머가 해제(Clear)되었거나, 파괴 중이라면 
		// 하단의 UI 재생성 로직을 건너뜁니다.
		if (IsActorBeingDestroyed() || !IsValid(this) || !GetWorldTimerManager().IsTimerActive(TH_UpdateLoop))
		{
			return;
		}

		UpdateAudioAndVisuals(DeltaTime);
		UpdateClientUI();
	}
}

void AReviveZoneActor::UpdateAudioAndVisuals(float DeltaTime)
{
	// 1. UI 시각적 진행률 부드럽게 보간
	VisualProgress = FMath::FInterpTo(VisualProgress, CurrentProgress, DeltaTime, 10.0f);

	// 2. MetaSound 진행률 전달
	ReviveAudio->SetFloatParameter(FName("Progress"), VisualProgress);

	// 3. 컴포넌트 전체 피치 배수(Pitch Multiplier) 실시간 조절
	// MetaSound 내부 노드를 거치지 않고 C++에서 컴포넌트 자체의 재생 속도를 올려 피치를 완벽하게 올립니다.
	float ClampedPitch = FMath::GetMappedRangeValueClamped(FVector2D(0.0f, MaxReviveTime), FVector2D(MinPitch, MaxPitch), VisualProgress);
	ReviveAudio->SetPitchMultiplier(ClampedPitch);
}

void AReviveZoneActor::UpdateClientUI()
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC) return;

	ABaseCharacter* LocalPlayer = Cast<ABaseCharacter>(PC->GetPawn());
	if (!LocalPlayer) return;

	bool bShouldShowUI = false;

	if (LocalPlayer == DeadPlayer)
	{
		// 내가 부활을 기다리는 당사자라면, 누군가 살려줘서 게이지가 0보다 클 때만 UI 표시
		bShouldShowUI = (CurrentProgress > 0.0f);
	}
	else if (LocalPlayer->GetCharacterAttributeSet()->GetHealth() > 0.0f)
	{
		// 살아있는 구조자라면 장판 영역 안에 들어왔을 때만 게이지를 볼 수 있음
		bShouldShowUI = ReviveZoneCapsule->IsOverlappingActor(LocalPlayer);
	}
	else
	{
		// 내가 당사자도 아니고 살아있는 것도 아니라면(구조 중 사망 등), UI를 즉시 가려야 함
		bShouldShowUI = false;
	}

	// 1. 위젯이 없으면 딱 한 번만 생성해서 저장해둔다.
	if (!LocalReviveWidget && ReviveGaugeClass)
	{
		LocalReviveWidget = CreateWidget<UReviveGaugeWidget>(PC, ReviveGaugeClass);
		if (LocalReviveWidget)
		{
			LocalReviveWidget->AddToViewport();
			LocalReviveWidget->SetVisibility(ESlateVisibility::Collapsed); // 일단 숨겨둠
		}
	}

	// 2. 화면에 띄울지 말지 결정하여 가시성(Visibility)만 껐다 켰다 한다.
	if (LocalReviveWidget)
	{
		if (bShouldShowUI)
		{
			if (LocalReviveWidget->GetVisibility() != ESlateVisibility::Visible)
			{
				LocalReviveWidget->SetVisibility(ESlateVisibility::Visible);
			}

			// 게이지 상태 업데이트
			LocalReviveWidget->UpdateGaugeFromValues(VisualProgress, MaxReviveTime);
		}
		else
		{
			if (LocalReviveWidget->GetVisibility() != ESlateVisibility::Collapsed)
			{
				LocalReviveWidget->SetVisibility(ESlateVisibility::Collapsed);
				LocalReviveWidget->UpdateGauge(0.0f);
			}
		}
	}
}

void AReviveZoneActor::OnRep_CurrentState()
{
	UE_LOG(LogTemp, Warning, TEXT("[ReviveZoneActor] OnRep_CurrentState Called. New State: %d"), (int32)CurrentState);
	
	// 사운드 컴포넌트가 대기(Sleep) 상태로 꺼져있다면 명시적으로 깨워줌
	if (!ReviveAudio->IsPlaying())
	{
		ReviveAudio->Play();
	}

	// 클라이언트 측에서 상태 전환 감지 후 MetaSound Trigger 전송
	switch (CurrentState)
	{
	case EReviveZoneState::Reviving:
		UE_LOG(LogTemp, Warning, TEXT("[ReviveZoneAudio] Firing Trigger -> ResumeProgress"));
		ReviveAudio->SetTriggerParameter(FName("ResumeProgress"));
		break;
	case EReviveZoneState::GracePeriod:
		UE_LOG(LogTemp, Warning, TEXT("[ReviveZoneAudio] Firing Trigger -> StopProgress (GracePeriod)"));
		// 밖으로 나가면 즉시 충전(게이지) 소리를 끕니다. (경고음 제거됨)
		ReviveAudio->SetTriggerParameter(FName("StopProgress")); 
		break;
	case EReviveZoneState::Decaying:
		UE_LOG(LogTemp, Warning, TEXT("[ReviveZoneAudio] Firing Trigger -> StopProgress"));
		ReviveAudio->SetTriggerParameter(FName("StopProgress"));
		break;
	case EReviveZoneState::Idle:
		UE_LOG(LogTemp, Warning, TEXT("[ReviveZoneAudio] State is Idle."));
		break;
	}
}

void AReviveZoneActor::OnRep_CurrentProgress()
{
	// Reserved
}

void AReviveZoneActor::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 몬스터가 밟았을 때 부활 장판이 작동하는 것을 막기 위해 APlayerCharacter로 캐스팅하여 '플레이어'인지 확인합니다.
	APlayerCharacter* Rescuer = Cast<APlayerCharacter>(OtherActor);
	// 자기 자신(시체)은 제외하고, 살아있는 다른 플레이어만 허용
	if (Rescuer && Rescuer != DeadPlayer && Rescuer->GetCharacterAttributeSet()->GetHealth() > 0.0f)
	{
		ActiveRescuers.AddUnique(Rescuer);
	}
}

void AReviveZoneActor::OnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	APlayerCharacter* Rescuer = Cast<APlayerCharacter>(OtherActor);
	if (Rescuer)
	{
		ActiveRescuers.Remove(Rescuer);
	}
}

void AReviveZoneActor::FinishRevive()
{
	if (HasAuthority() && DeadPlayer && !bReviveCompleted)
	{
		bReviveCompleted = true;
		GetWorldTimerManager().ClearTimer(TH_UpdateLoop);
		FGameplayEventData Payload;
		Payload.Instigator = ActiveRescuers.Num() > 0 ? ActiveRescuers[0] : nullptr;
		Payload.Target = DeadPlayer;
		
		// 시체에게 부활 어빌리티 실행 지시 (서버에서만 발생, GA_Revive 실행 후 GE_Revive가 리플리케이트됨)
		if (UAbilitySystemComponent* ASC = DeadPlayer->GetAbilitySystemComponent())
		{
			const int32 Triggered = ASC->HandleGameplayEvent(GameplayTagSystem::Event_Revive, &Payload);
			if (Triggered > 0)
			{
				// 완료 순간 참여 중인 살아 있는 팀원 모두에게 한 번씩 인정한다.
				for (ABaseCharacter* Rescuer : ActiveRescuers)
				{
					if (IsValid(Rescuer) && Rescuer != DeadPlayer && Rescuer->GetCharacterAttributeSet()
						&& Rescuer->GetCharacterAttributeSet()->GetHealth() > 0.0f)
					{
						if (ABasePlayerState* RescuerPS = Rescuer->GetPlayerState<ABasePlayerState>())
						{
							RescuerPS->RecordTeamRevive();
						}
					}
				}
			}
		}

		// 사운드가 끝까지 재생될 수 있도록 즉시 파괴 대신 수명 설정 및 상호작용 비활성화
		ReviveZoneCapsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SetLifeSpan(3.0f); // 3초 뒤 자동 파괴

		// 클라이언트들에게 즉시 시각/청각적 종료 처리를 명령합니다.
		Multicast_OnReviveFinished();
	}
}

void AReviveZoneActor::Multicast_OnReviveFinished_Implementation()
{
	// 1. UI 강제 닫기
	if (LocalReviveWidget)
	{
		LocalReviveWidget->UpdateGauge(0.0f);
		LocalReviveWidget->RemoveFromParent();
		LocalReviveWidget = nullptr;
	}

	// 2. 이펙트 비활성화 및 화면에서 즉시 숨김
	if (ReviveEffect)
	{
		ReviveEffect->SetVisibility(false, true); // 파티클 잔해가 남지 않도록 강제 숨김
		ReviveEffect->Deactivate();
	}

	// 3. 클라이언트 타이머 중단 (더 이상 UI 업데이트 안함)
	GetWorldTimerManager().ClearTimer(TH_UpdateLoop);

	// 4. 오디오 마무리 (게이지 소리 끄고 성공 사운드 재생)
	if (ReviveAudio)
	{
		// 밖으로 나갔을 때와 마찬가지로 게이지 사운드 정지
		ReviveAudio->SetTriggerParameter(FName("StopProgress"));

		if (!ReviveAudio->IsPlaying())
		{
			ReviveAudio->Play();
		}
		ReviveAudio->SetTriggerParameter(FName("PlaySuccess"));
	}
}
