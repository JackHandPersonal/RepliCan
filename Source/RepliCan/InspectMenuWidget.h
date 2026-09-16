// The inspect context menu: while the reticle rests on an inspectable
// within reach, a small panel beside the reticle shows the thing's friendly
// name (lightly emphasized), its description in small italics, and its
// actions; the mouse wheel moves the selection and E uses it (see
// ABasePlayerController::UpdateInspectTarget / OnInspectWheel / OnInspectUse).
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InspectMenuWidget.generated.h"

class UTextBlock;
class UVerticalBox;

UCLASS()
class REPLICAN_API UInspectMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetContent(const FString& Name, const FString& Description, const TArray<FString>& Actions, int32 SelectedIndex);
	void SetSelectedIndex(int32 SelectedIndex);
	int32 GetNumActions() const { return ActionTexts.Num(); }
	// Corner placement: a thin leader from the thing's screen point to the panel's nearest edge.
	// All three in the viewport space SetPositionInViewport uses; Align is the panel's alignment.
	void SetLeader(bool bOn, FVector2D Anchor, FVector2D PanelPos, FVector2D Align) { bLeader = bOn; LeaderAnchor = Anchor; LeaderPanelPos = PanelPos; LeaderAlign = Align; }

protected:
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual void NativeOnInitialized() override;

private:
	void RefreshSelection();

	UPROPERTY() TObjectPtr<class UHorizontalBox> NameBox;
	UPROPERTY() TObjectPtr<UTextBlock> DescriptionText;
	UPROPERTY() TObjectPtr<UVerticalBox> ActionBox;
	UPROPERTY() TArray<TObjectPtr<UTextBlock>> ActionTexts;
	TArray<FString> ActionLabels;
	int32 Selected = 0;
	bool bLeader = false;
	FVector2D LeaderAnchor = FVector2D::ZeroVector;
	FVector2D LeaderPanelPos = FVector2D::ZeroVector;
	FVector2D LeaderAlign = FVector2D::ZeroVector;
};
