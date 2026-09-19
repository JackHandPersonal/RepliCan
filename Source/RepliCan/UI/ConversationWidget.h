// The conversation panel: the right third of the screen (the NPC is framed
// to the left by the controller's conversation camera) holding one
// scrolling transcript: the exchange so far with the player's numbered
// replies directly under the newest line. Click a reply or press its
// number; Tab/Esc leave. Driven by ABasePlayerController::StartConversation
// / ChooseConversationOption.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ConversationWidget.generated.h"

class ABasePlayerController;
class UHorizontalBox;
class UScrollBox;
class UTextBlock;
class UVerticalBox;

UCLASS()
class UConversationChoiceBinding : public UObject
{
	GENERATED_BODY()
public:
	TWeakObjectPtr<ABasePlayerController> Controller;
	TWeakObjectPtr<class UConversationWidget> Widget;
	int32 Index = 0;
	UFUNCTION() void OnClicked();
};

UCLASS()
class REPLICAN_API UConversationWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetOwnerController(ABasePlayerController* InController) { OwnerController = InController; }
	void SetSpeakerName(const FString& Name);
	// Name in false caps with the character's description under it.
	void SetSpeaker(const FString& Name, const FString& Description);
	void ClearHistory();
	// bPlayer: the player's own line (right-aligned, dimmer); else the NPC's.
	void AddLine(const FString& Speaker, const FString& Text, bool bPlayer);
	void SetChoices(const TArray<FString>& Choices);

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	float Clock = 0.0f;
	int32 PendingChoice = -1;
	int32 ScrollTicks = 0;   // ScrollToEnd only lands after layout, so it is repeated for a few ticks
	virtual void NativeOnInitialized() override;

private:
	void PlaceChoicesLast();
	UPROPERTY() TObjectPtr<ABasePlayerController> OwnerController;
	UPROPERTY() TObjectPtr<UHorizontalBox> TitleBox;
	UPROPERTY() TObjectPtr<UTextBlock> DescriptionText;
	FString SpeakerName;
	UPROPERTY() TObjectPtr<UScrollBox> History;
	UPROPERTY() TObjectPtr<UVerticalBox> ChoiceBox;
public:
	// Test hooks: a Slate mouse press and release on the Index-th reply; how many replies are on offer.
	void ClickChoice(int32 Index);
	int32 CountChoiceButtons() const;
	FString DescribeChoices() const;
	// A reply click is queued and taken on the next tick: rebuilding the reply
	// list from inside the button's own click dispatch left the new buttons unlaid-out.
	void QueueChoice(int32 Index) { PendingChoice = Index; }
private:
	UPROPERTY() TArray<TObjectPtr<UConversationChoiceBinding>> Bindings;
};
