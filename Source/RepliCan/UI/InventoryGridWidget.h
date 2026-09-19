// A grid of item squares in the station-terminal style, shared by the
// character sheet (the player's 20 slots) and the transfer screen (a
// container's slots beside the player's). Each square shows the item's name;
// empty squares are dim. Clicks report the slot index to OnSlotClicked.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/DragDropOperation.h"
#include "InventoryGridWidget.generated.h"

class UGridPanel;
class UButton;
class UTextBlock;
class UInventoryGridWidget;

UCLASS()
class UInventorySlotBinding : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY() TObjectPtr<UInventoryGridWidget> Grid;
	int32 Index = 0;
	UFUNCTION() void OnClicked();
	UFUNCTION() void OnHovered();
	UFUNCTION() void OnUnhovered();
};

// What a drag carries: which grid it started on (0 bag, 1 gear, 2 quickbar) and which square.
UCLASS()
class UInventoryDragOperation : public UDragDropOperation
{
	GENERATED_BODY()
public:
	UPROPERTY() int32 SourceGrid = 0;
	UPROPERTY() int32 SourceIndex = -1;
};

UCLASS()
class REPLICAN_API UInventoryGridWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// A bag grid the way the character sheet spec says (columns, cell, gap): the sheet and the
	// container screen call this so their inventories look identical.
	static UInventoryGridWidget* MakeBag(APlayerController* Owner, int32 Capacity);
	// Builds the squares: Capacity of them, Columns per row, each Cell px square.
	void Configure(const FString& Title, int32 Capacity, int32 Columns, float Cell);
	// Like Configure, but each cell sits at the given (row, column); gaps are left empty.
	void ConfigureCells(const FString& Title, const TArray<FIntPoint>& Cells, float Cell);
	// Faint captions shown in empty cells (slot names), and cells that cannot be used yet.
	void SetSlotNames(const TArray<FString>& Names);
	void SetSlotEnabled(int32 Index, bool bEnabled);
	// A caption in a grid cell of its own beside a slot (the body slots' names sit next to their squares).
	// Align: 0 left, 1 right, 2 centred.
	void AddCaption(int32 Row, int32 Col, const FString& Text, int32 Align);
	// An empty cell that holds a column open by Width px.
	void AddSpacer(int32 Row, int32 Col, float Width);
	// Space between cells in px (set before Configure).
	void SetGap(float Gap);
	// Fills the squares from the front; the rest read empty.
	void SetItems(const TArray<FString>& Items);
	// The cell a click has picked out (-1 for none): drawn brighter, restored by SetItems.
	void SetSelected(int32 Index);
	int32 GetSelected() const { return SelectedIndex; }
	int32 GetCapacity() const { return Capacity; }
	// DRAG AND DROP between squares, on this grid or another: the sheet answers whether a drop is
	// valid (the target square lights up while it is) and performs it. Grids are told apart by id.
	DECLARE_DELEGATE_RetVal_FourParams(bool, FDropQuery, int32 /*SrcGrid*/, int32 /*SrcIndex*/, int32 /*DstGrid*/, int32 /*DstIndex*/);
	DECLARE_DELEGATE_FourParams(FDropped, int32, int32, int32, int32);
	FDropQuery OnCanDrop;
	FDropped OnDropped;
	void SetGridId(int32 Id) { GridId = Id; }
	int32 GetGridId() const { return GridId; }
	// THE PICTURE UNDER THE POINTER while dragging is the owner's (the sheet draws it in its own
	// overlay): the engine's decorator window appears at the dragged widget's corner and slides
	// to the cursor, which for a whole grid is a slide in from the left. Began carries the square's
	// icon brush; Ended fires however the drag finished. Refused fires on a drop the owner said no to.
	DECLARE_DELEGATE_OneParam(FDragBegan, const FSlateBrush&);
	FDragBegan OnDragBegan;
	FSimpleDelegate OnDragEnded;
	DECLARE_DELEGATE_FourParams(FDropRefused, int32, int32, int32, int32);
	FDropRefused OnDropRefused;
	UFUNCTION() void HandleDragOpEnded(UDragDropOperation* Operation);
	DECLARE_DELEGATE_OneParam(FSlotClicked, int32);
	FSlotClicked OnSlotClicked;
	int32 SelectedIndex = -1;
	// Hover: the slot index under the mouse, or -1 when the mouse leaves a slot.
	FSlotClicked OnSlotHovered;
	// A right click on a filled square: its index and the pointer's absolute screen position, for a menu.
	DECLARE_DELEGATE_TwoParams(FSlotRightClicked, int32, FVector2D);
	FSlotRightClicked OnSlotRightClicked;
	void HandleClick(int32 Index);
	void HandleHover(int32 Index);
	// /Game/RepliCan/Icons/T_Icon_<name with runs of non-alphanumerics as '_'>, or null.
	static class UTexture2D* FindIcon(const FString& ItemName);

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual bool NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	int32 CellAt(const FVector2D& ScreenPos) const;   // the enabled square under a screen point, or -1
	void SetHighlight(int32 Index);                    // the drop target's glow; -1 clears it
	FLinearColor RestColour(int32 Index) const;        // a square's colour with nothing going on
	// Disabled cells are cross-hatched so they read as "not yet" rather than empty.
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	UPROPERTY() TObjectPtr<class UVerticalBox> Column;
	UPROPERTY() TObjectPtr<UTextBlock> TitleText;
	UPROPERTY() TObjectPtr<UGridPanel> Grid;
	UPROPERTY() TArray<TObjectPtr<UButton>> Cells;
	UPROPERTY() TArray<TObjectPtr<UTextBlock>> Labels;
	UPROPERTY() TArray<TObjectPtr<class UImage>> Icons;
	UPROPERTY() TArray<TObjectPtr<UInventorySlotBinding>> Bindings;
	TArray<FString> SlotNames;
	TArray<bool> SlotEnabled;
	TArray<bool> Filled;        // which squares hold something, as of the last SetItems
	int32 GridId = 0;
	int32 PressedIndex = -1;    // the square the mouse went down on, a drag's source
	int32 HighlightIndex = -1;
	int32 Capacity = 0;
	int32 Columns = 5;
	float Gap = 1.0f;   // between cells, split across both neighbours
	float CellSize = 64.0f;
};
