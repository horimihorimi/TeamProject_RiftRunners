#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ClearResultsWidget.generated.h"

class UButton;
class UTextBlock;

// 표시용 데이터. 실제 서버 통계 집계는 이후 이 구조체에 연결한다.
USTRUCT(BlueprintType)
struct FClearResultsDisplayData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double TotalDamage = 0.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double BossDamage = 0.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 KillCount = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double DamageTaken = 0.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ReviveCount = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ClearDurationSeconds = 0.0f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnClearReturnRequested);

// WBP_ClearResults의 부모. 배치/이미지는 UMG Designer에서 설정한다.
UCLASS(Abstract, Blueprintable)
class TEAMPROJECT_FIN_API UClearResultsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Clear Results")
	void SetResultsData(const FClearResultsDisplayData& Data);

	// 서버의 준비 인원 변경 알림이 이 함수를 호출하도록 연결한다.
	UFUNCTION(BlueprintCallable, Category = "Clear Results")
	void SetReturnReadyState(int32 ReadyCount, int32 PlayerCount, bool bLocalReady);

	// 서버 준비 요청을 연결할 지점. 클릭 자체를 서버 승인으로 취급하지 않는다.
	UPROPERTY(BlueprintAssignable, Category = "Clear Results")
	FOnClearReturnRequested OnReturnRequested;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_ClearTime;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_TotalDamage;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_BossDamage;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_KillCount;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_DamageTaken;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_ReviveCount;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_ReturnToRoom;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_ReturnButton;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_ReadyCount;

private:
	UFUNCTION()
	void HandleReturnClicked();
};
