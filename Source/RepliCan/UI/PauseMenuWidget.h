// The in-game menu (Esc): pauses the game and shows a centred box titled
// "Main Menu" with Back (unpause, hide) and Quit (end the game / PIE
// session) stacked under it. Built in code like the other panels; the
// controller owns show/hide and the pause state (ABasePlayerController::
// ShowPauseMenu / HidePauseMenu).
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PauseMenuWidget.generated.h"

class ABasePlayerController;
class UButton;
class UTextBlock;

UCLASS()
class REPLICAN_API UPauseMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetOwnerController(ABasePlayerController* InController) { OwnerController = InController; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	UFUNCTION() void OnBack();
	UFUNCTION() void OnUnstuck();
	UFUNCTION() void OnQuit();
	UFUNCTION() void OnSave();
	UFUNCTION() void OnLoad();
	UFUNCTION() void OnSettings();
	UFUNCTION() void OnScenes();

	UButton* MakeButton(const FString& Label);

	UPROPERTY() TObjectPtr<ABasePlayerController> OwnerController;
	UPROPERTY() TObjectPtr<class UBorder> Box;
	float Clock = 0.0f;
};
