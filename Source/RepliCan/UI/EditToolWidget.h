// The root page of edit mode (F12), anchored bottom-left: a small station
// panel with the Character Manager button and a way back to the game.
// Entering edit mode lifts the player into the fly camera; leaving it (F12
// again, or RETURN TO GAME here) puts them back where they were. Pure C++
// UMG, same convention as the other panels.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EditToolWidget.generated.h"

class ABasePlayerController;
class UTextBlock;

UCLASS()
class REPLICAN_API UEditToolWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	void SetOwnerController(ABasePlayerController* InController) { OwnerController = InController; }

	// Kept as no-op hooks so the controller's calls stay valid if buttons
	// that depend on selection come back.
	void RefreshFromController();
	void SetStatus(const FString& Text);

protected:

	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:

	UTextBlock* MakeText(const FText& Text, float FontSize, const FLinearColor& Color);
	class UButton* MakeButton(const FString& Label);

	UFUNCTION() void OnNewCharacter();
	UFUNCTION() void OnReturnToGame();
	UFUNCTION() void OnClaudeAssist();
	UFUNCTION() void OnAppearance();
	UPROPERTY() TObjectPtr<UTextBlock> AssistLabel;

	UPROPERTY() TObjectPtr<ABasePlayerController> OwnerController;
	UPROPERTY() TObjectPtr<UTextBlock> StatusText;
	float Clock = 0.0f;
};
