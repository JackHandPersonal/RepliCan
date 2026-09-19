// The appearance chooser: the player's basic look before any gear -- body
// (the cryo pair), head, skin tone, hair, beard, stubble, height and
// build -- as rows of "< value >" steppers on a CRT panel, with a live
// "mirror" feed of the preview character (the booth) beside it. The
// controller owns the state and the preview; this is the face of it.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AppearanceWidget.generated.h"

class ABasePlayerController;
class UEditableTextBox;
class UButton;
class UImage;
class UTextBlock;
class UTextureRenderTarget2D;
class UVerticalBox;

UCLASS()
class UAppearanceRowBinding : public UObject
{
	GENERATED_BODY()
public:
	TWeakObjectPtr<ABasePlayerController> Controller;
	TWeakObjectPtr<class UAppearanceWidget> Widget;
	FString Row;
	int32 Delta = 0;
	UFUNCTION() void OnClicked();
};

UCLASS()
class UAppearanceNameBinding : public UObject
{
	GENERATED_BODY()
public:
	TWeakObjectPtr<ABasePlayerController> Controller;
	TWeakObjectPtr<class UAppearanceWidget> Widget;
	UFUNCTION() void OnChanged(const FText& Text);
};

UCLASS()
class REPLICAN_API UAppearanceWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetOwnerController(ABasePlayerController* In) { OwnerController = In; }
	// Re-reads every row's value from the controller.
	void Refresh();
	// The mirror: the booth capture's render target.
	void SetFeed(UTextureRenderTarget2D* Target);
	// Sends the typed name to the controller (called by the text boxes).
	void PushName();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	// Drag on the mirror orbits the camera; the press is captured so the drag
	// may leave the picture.
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	// Tab / Shift+Tab walk the name boxes, the rows and the buttons; Left/Right
	// step a focused row; Enter presses a focused button.
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	void AddRow(UVerticalBox* Into, const FString& Row, const FString& Label);
	UFUNCTION() void OnRandom();
	UFUNCTION() void OnDefault();
	UFUNCTION() void OnDone();
	void OnTab(int32 Tab);
	class USizeBox* LabelColumn(const FString& Label);
	UEditableTextBox* MakeNameBox(const FString& Hint);
	void SetFocusIndex(int32 Index);
	void ApplyFocusLook();
	int32 FocusCount() const { return 2 + RowOrder.Num() + FocusButtons.Num(); }
	TArray<FString> RowOrder;
	int32 FocusIndex = -1;
	UPROPERTY() TArray<TObjectPtr<UButton>> FocusButtons;

	UPROPERTY() TObjectPtr<ABasePlayerController> OwnerController;
	UPROPERTY() TMap<FString, TObjectPtr<UTextBlock>> Values;
	UPROPERTY() TMap<FString, TObjectPtr<UWidget>> RowLines;   // whole rows, for dimming
	UPROPERTY() TObjectPtr<UImage> Feed;
	UPROPERTY() TObjectPtr<UEditableTextBox> FirstBox;
	UPROPERTY() TObjectPtr<UEditableTextBox> LastBox;
	UPROPERTY() TObjectPtr<UAppearanceNameBinding> NameBinding;
	bool bDragging = false;
	// Ticks of layout settling to hide after a rebuild: the ASCII rules measure themselves
	// against their own rendered text, so the first painted frame is not the final one.
	// The page it replaced is held on screen until this reaches zero. See ShowConsolePage.
	int32 SettleTicks = 0;
	// The body and the mirror box, so the tick can size the mirror from the body's real width.
	UPROPERTY() TObjectPtr<class UHorizontalBox> Body;
	UPROPERTY() TObjectPtr<class USizeBox> MirrorBox;
	bool bMirrorFitted = false;
	float FittedBodyW = 0.0f;
public:
	void BeginSettle() { SettleTicks = 2; SetRenderOpacity(0.0f); }
private:
	FVector2D DragLast = FVector2D::ZeroVector;
	float DragTravel = 0.0f;
	UPROPERTY() TArray<TObjectPtr<UAppearanceRowBinding>> Bindings;
	float Clock = 0.0f;
};
