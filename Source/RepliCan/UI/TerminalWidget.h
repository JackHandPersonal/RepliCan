// A COMPUTER TERMINAL. A monospaced text area with a prompt, laid over a monitor prop's screen
// while the camera looks at it squarely (BasePlayerController::OpenTerminal). Keyboard types
// at the prompt, Enter runs, Tab completes, Esc leaves; the mouse clicks file names and scrolls.
// What a terminal knows -- its banner and its files -- comes from UI/Terminals.json by id, with
// "default" for any terminal without an entry of its own.
#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TerminalWidget.generated.h"

class UTerminalWidget;

UCLASS()
class UTerminalLinkBinding : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY() TObjectPtr<UTerminalWidget> Term;
	FString Command;
	UFUNCTION() void OnClicked();
};

UCLASS()
class REPLICAN_API UTerminalWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Open(class ABasePlayerController* InController, const FString& TerminalId);
	void FocusPrompt();
	bool FocusPromptFor(int32 SlateUserIndex);   // Slate user (0 = the real keyboard, or the pointer's virtual user) onto the prompt; false if the widget could not be found
	bool PromptCentrePx(FVector2D& Out) const;   // the prompt's centre in the picture's pixels, once laid out
	void Run(const FString& Line);
	FSimpleDelegate OnExit;
	FSimpleDelegate OnNeedFocus;   // a link was clicked (the pointer's user took the focus): the prompt wants the keyboard back

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	UFUNCTION() void OnPromptCommitted(const FText& Text, ETextCommit::Type Method);

private:
	void Print(const FString& Text, const FLinearColor& Colour);
	void PrintLink(const FString& Label, const FString& Command);
	void Complete();
	void LoadEntry(const FString& TerminalId);
	UPROPERTY() TObjectPtr<class ABasePlayerController> Controller;
	UPROPERTY() TObjectPtr<class UScrollBox> Scroll;
	UPROPERTY() TObjectPtr<class UVerticalBox> Lines;
	UPROPERTY() TObjectPtr<class UEditableTextBox> Prompt;
	UPROPERTY() TObjectPtr<class UTextBlock> Hints;
	UPROPERTY() TArray<TObjectPtr<UTerminalLinkBinding>> Bindings;
	TArray<FString> Banner;
	TMap<FString, FString> Files;
	TArray<FString> History;
	int32 HistoryAt = 0;
	FString Id;
	int32 LineCount = 0;
};
