// A centred three-way prompt ("Save changes to X?": Save / Discard /
// Cancel) over a dimmed screen. Built in code like the other panels; the
// controller creates one when leaving the character pages with unsaved
// changes (ABasePlayerController::LeaveCharacterPagesThen).
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ConfirmDialogWidget.generated.h"

class UButton;
class UTextBlock;

UCLASS()
class REPLICAN_API UConfirmDialogWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// The callbacks run AFTER the dialog has removed itself.
	void Setup(const FString& Title, const FString& Message, TFunction<void()> InOnSave, TFunction<void()> InOnDiscard, TFunction<void()> InOnCancel);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	UPROPERTY() TObjectPtr<class UBorder> Box;
	float Clock = 0.0f;

private:
	UFUNCTION() void OnSaveClicked();
	UFUNCTION() void OnDiscardClicked();
	UFUNCTION() void OnCancelClicked();
	void Finish(TFunction<void()>& Callback);
	UButton* MakeButton(const FString& Label);

	UPROPERTY() TObjectPtr<UTextBlock> TitleText;
	UPROPERTY() TObjectPtr<UTextBlock> MessageText;
	TFunction<void()> OnSave, OnDiscard, OnCancel;
};
