// A grid of item squares in the station-terminal style, shared by the
// character sheet (the player's 20 slots) and the transfer screen (a
// container's slots beside the player's). Each square shows the item's name;
// empty squares are dim. Clicks report the slot index to OnSlotClicked.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
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
	DECLARE_DELEGATE_OneParam(FSlotClicked, int32);
	FSlotClicked OnSlotClicked;
	int32 SelectedIndex = -1;
	// Hover: the slot index under the mouse, or -1 when the mouse leaves a slot.
	FSlotClicked OnSlotHovered;
	void HandleClick(int32 Index);
	void HandleHover(int32 Index);
	// /Game/RepliCan/Icons/T_Icon_<name with runs of non-alphanumerics as '_'>, or null.
	static class UTexture2D* FindIcon(const FString& ItemName);

protected:
	virtual void NativeOnInitialized() override;
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
	int32 Capacity = 0;
	int32 Columns = 5;
	float Gap = 1.0f;   // between cells, split across both neighbours
	float CellSize = 64.0f;
};
