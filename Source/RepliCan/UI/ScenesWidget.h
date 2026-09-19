// THE NARRATIVE CONSOLE (the pause menu's NARRATIVE): one page in the same shell as the character
// sheet, two tabs. CHARACTERS: every character file the game has (Characters/*.json) as a table
// with a fixed header, and a viewer beside it -- the sheet's own booth showing whichever row was
// clicked, turnable with the mouse. SEQUENCES: every scripted scene (Sequences/*.json), read for
// what it holds -- when it last changed, its steps by kind, its cast, and about how long it runs
// -- with a button that jumps straight into it.
//
// A sequence is only reachable in play by arriving at whatever triggers it, which makes the
// tenth line of a five-minute scene expensive to look at and nearly impossible to iterate on.
// This is the way in: both lists are read off disk every time the page opens, so a file
// authored a minute ago is already here, and nothing needs registering in code. Playing one
// from here is the real thing, not a preview -- the same PlaySequence the game calls.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ScenesWidget.generated.h"

class UVerticalBox;
class UTextBlock;
class UButton;
class UImage;
class UCrtTabsWidget;
class ABasePlayerController;

DECLARE_DELEGATE(FOnScenesClosed);

UCLASS()
class REPLICAN_API UScenesWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(ABasePlayerController* Controller) { OwnerController = Controller; }
	// Re-reads the directories each time, so the lists are never stale.
	void Rebuild();
	FOnScenesClosed OnClose;
	enum { TabCharacters = 0, TabSequences = 1 };

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	struct FRow { FString Name, File, Type, Kit, Description; bool bTalks = false; bool bParses = true; };
	struct FSceneRow { FString Name, Title, Changed, Counts, Cast, Runtime; bool bUnskippable = false; bool bParses = true; };
	TArray<FSceneRow> ScanSequences(const TMap<FString, FString>& DisplayNames) const;
	TArray<FRow> ScanCharacters() const;
	void FillTab();
	void BuildCharacters(UVerticalBox* Into);
	void BuildSequences(UVerticalBox* Into);
	void OnTab(int32 Index);
	void ShowCharacter(const FRow& Row);

	UFUNCTION() void OnPlayClicked();
	UFUNCTION() void OnRowClicked();
	UFUNCTION() void OnCloseClicked();

	UPROPERTY() TObjectPtr<ABasePlayerController> OwnerController;
	UPROPERTY() TObjectPtr<UVerticalBox> Column;
	UPROPERTY() TObjectPtr<UVerticalBox> Body;
	UPROPERTY() TObjectPtr<UCrtTabsWidget> Tabs;
	UPROPERTY() TObjectPtr<UImage> Feed;
	UPROPERTY() TObjectPtr<UTextBlock> ViewerName;
	UPROPERTY() TObjectPtr<UTextBlock> ViewerText;
	int32 ActiveTab = TabCharacters;
	// Same trick the key bindings page uses: a dynamic delegate cannot carry which row was
	// clicked, so the buttons say who they are.
	UPROPERTY() TMap<TObjectPtr<UButton>, FString> ButtonScenes;
	UPROPERTY() TMap<TObjectPtr<UButton>, FString> ButtonCharacters;   // button -> the character file
	TArray<FRow> CharacterRows;
	FString Shown;   // the file in the viewer
	bool bDragging = false;
	FVector2D DragLast = FVector2D::ZeroVector;
};
