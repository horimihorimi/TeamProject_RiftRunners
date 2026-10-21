// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HealthBarFrostVisualWidget.generated.h"

class ABaseCharacter;
class UImage;
class UTexture2D;

/** 최대 체력 감소 디버프에 따른 체력바 프레임 연출을 담당하는 부모 위젯입니다. */
UCLASS()
class TEAMPROJECT_FIN_API UHealthBarFrostVisualWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** 동결 단계별 체력바 테두리 이미지입니다. WBP에서 Is Variable을 활성화해야 합니다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Frost Visual")
	TObjectPtr<UImage> FrameImage;

	/** 정상 상태(0)와 동결 단계(1~4)에 대응하는 프레임 텍스처 배열입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Frost Visual", meta = (TitleProperty = "ObjectName"))
	TArray<TObjectPtr<UTexture2D>> FrameTextures;

	UFUNCTION()
	void HandleHealthCapMultiplierChanged(float NewMultiplier);

	/** 블루프린트 미리보기나 수동 갱신이 필요할 때 호출할 수 있습니다. */
	UFUNCTION(BlueprintCallable, Category = "Frost Visual")
	void UpdateFrostVisuals(float Multiplier);

private:
	TWeakObjectPtr<ABaseCharacter> OwningCharacter;
	int32 LastFrameIndex = INDEX_NONE;
};
