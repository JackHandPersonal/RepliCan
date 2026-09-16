// The Character / Inventory screen (Tab): a modal that fills most of the
// view with the game still visible around its edges. Three columns: stats on
// the left, a mirror of the unit in the middle (the saved likeness in the
// booth, dragged to turn, clicked on the face to zoom), inventory and the
// equipped gear on the right. Everything about the layout comes from
// UI/CharacterSheet.json (FSheetSpec) and is rebuilt each time the sheet opens.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CharacterSheetWidget.generated.h"

class ABasePlayerController;
class UTextBlock;
class UVerticalBox;
class UVerticalBoxSlot;
class UImage;

UCLASS()
class REPLICAN_API UCharacterSheetWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetOwnerController(ABasePlayerController* In) { OwnerController = In; }
	// Rebuilds the whole layout from the current spec (called when the sheet opens).
	void Rebuild();
	// Pulls the current unit, inventory and gear from the controller (and the mirror feed).
	void Refresh();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	void OnInventorySlot(int32 Index);
	void OnGearSlot(int32 Index);
	void OnInventoryHover(int32 Index);
	void OnGearHover(int32 Index);
	void ShowInfo(const FString& Item);
	UFUNCTION() void OnClose();
	void OnTab(int32 Tab);
	// A row of the sheet: plain fixed-pitch text, or a self-padding rule when it ends in the rule character.
	UVerticalBoxSlot* AddRow(UVerticalBox* Box, const FString& Text, int32 Size, const FLinearColor& Color);

	UPROPERTY() TObjectPtr<ABasePlayerController> OwnerController;
	UPROPERTY() TObjectPtr<UVerticalBox> Column;   // the root's content; Rebuild() refills it
	UPROPERTY() TObjectPtr<UImage> Feed;
	UPROPERTY() TObjectPtr<class UCrtTabsWidget> Tabs;
	UPROPERTY() TObjectPtr<class UInventoryGridWidget> InventoryGrid;
	UPROPERTY() TObjectPtr<class UInventoryGridWidget> GearGrid;
	// The three columns. Their widths are FITTED from the panel's own laid-out width on the
	// first settle tick (see NativeTick) -- not computed from the viewport, which is wider than
	// the panel and put the right column off the edge of the screen.
	UPROPERTY() TObjectPtr<class UHorizontalBox> Body;
	UPROPERTY() TObjectPtr<class USizeBox> StatsBox;
	UPROPERTY() TObjectPtr<class USizeBox> MirrorBox;
	UPROPERTY() TObjectPtr<class USizeBox> MirrorFit;   // the 5:8 box the portrait sits in, sized from the mirror's width
	// What a click picked: the bag slot or the gear slot whose details the info panel shows.
	int32 SelectedBag = -1, SelectedGear = -1;
	void ShowSelectedInfo();
	UPROPERTY() TObjectPtr<class USizeBox> RightBox;
	float MirrorWidth = 0.0f;
	bool bColumnsFitted = false;
	float FittedBodyW = 0.0f;   // the body width the columns were pinned for
	UPROPERTY() TObjectPtr<UTextBlock> InventoryCount;
	// Stat rows written with {Brawn} and the like: the template, and the block to fill.
	UPROPERTY() TArray<TObjectPtr<UTextBlock>> StatRowTexts;
	TArray<FString> StatRowTemplates;
	UPROPERTY() TObjectPtr<UTextBlock> InfoName;
	UPROPERTY() TObjectPtr<UTextBlock> InfoText;
	FString RuleChars = TEXT("-=");
	float FaceClickFraction = 0.3f;
	int32 SettleTicks = 0;
	bool bDragging = false;
	FVector2D DragLast = FVector2D::ZeroVector;
	FVector2D DragStart = FVector2D::ZeroVector;
	float DragTravel = 0.0f;
	float Clock = 0.0f;
};
