// A small menu at the mouse: a title, then a row per option, in the sheet's own panel style.
// It covers the screen while it is up, so a click anywhere else closes it; Escape closes it too.
// Whoever shows it hands over what to do with the pick.
#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ContextMenuWidget.generated.h"

class UContextMenuWidget;

UCLASS()
class UContextMenuRowBinding : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY() TObjectPtr<UContextMenuWidget> Menu;
	FString Option;
	UFUNCTION() void OnClicked();
};

UCLASS()
class REPLICAN_API UContextMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ScreenPos is Slate's absolute position (a pointer event's GetScreenSpacePosition); the menu
	// opens just below and right of it. One menu at a time: showing another closes the last.
	static UContextMenuWidget* Show(APlayerController* Owner, const FVector2D& ScreenPos, const FString& Title, const TArray<FString>& Options, TFunction<void(const FString&)> OnPick);
	void Close();
	void Pick(const FString& Option);

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	void Build(const FString& Title, const TArray<FString>& Options);
	TFunction<void(const FString&)> OnPickFn;
	UPROPERTY() TObjectPtr<class UCanvasPanel> Canvas;
	UPROPERTY() TObjectPtr<class UBorder> Panel;
	UPROPERTY() TObjectPtr<class UVerticalBox> Rows;
	UPROPERTY() TArray<TObjectPtr<UContextMenuRowBinding>> Bindings;
};
