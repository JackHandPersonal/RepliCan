// The transfer screen for a container: the container's squares above the
// player's, dressed exactly like the character sheet (same panel, rules, bag
// grid and info block, all from the sheet spec); clicking an item moves it
// to the other bag.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InventoryTransferWidget.generated.h"

class ABasePlayerController;
class ALootBoxActor;
class UInventoryGridWidget;
class UTextBlock;
class UVerticalBox;

UCLASS()
class REPLICAN_API UInventoryTransferWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Open(ABasePlayerController* InController, ALootBoxActor* InBox);
	// Rebuilds the layout from the sheet spec (every Open, so spec edits show live).
	void Rebuild();
	void Refresh();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	void OnBoxSlot(int32 Index);
	void OnPlayerSlot(int32 Index);
	UFUNCTION() void OnClose();
	void OnBoxHover(int32 Index);
	void OnPlayerHover(int32 Index);
	void ShowInfo(const FString& Item);
	UPROPERTY() TObjectPtr<UTextBlock> InfoName;
	UPROPERTY() TObjectPtr<UTextBlock> InfoText;

	UPROPERTY() TObjectPtr<ABasePlayerController> OwnerController;
	UPROPERTY() TObjectPtr<ALootBoxActor> Box;
	UPROPERTY() TObjectPtr<UVerticalBox> Outer;
	UPROPERTY() TObjectPtr<UTextBlock> Note;
	UPROPERTY() TObjectPtr<UInventoryGridWidget> BoxGrid;
	UPROPERTY() TObjectPtr<UInventoryGridWidget> PlayerGrid;
	float Clock = 0.0f;
};
