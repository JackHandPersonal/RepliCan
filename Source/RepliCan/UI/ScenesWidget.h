// The cast-and-scenes console: one page in the same shell as the character sheet, with two
// tabs. CHARACTERS lists every character file the game has (Characters/*.json) with what it
// is; SEQUENCES lists every scripted scene (Sequences/*.json) with a button that jumps
// straight into it.
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

private:
	struct FRow { FString Name; FString Detail; FString Extra; };
	TArray<FRow> ScanSequences() const;
	TArray<FRow> ScanCharacters() const;
	void FillTab();
	void OnTab(int32 Index);

	UFUNCTION() void OnPlayClicked();
	UFUNCTION() void OnCloseClicked();

	UPROPERTY() TObjectPtr<ABasePlayerController> OwnerController;
	UPROPERTY() TObjectPtr<UVerticalBox> Column;
	UPROPERTY() TObjectPtr<UVerticalBox> Content;
	UPROPERTY() TObjectPtr<UCrtTabsWidget> Tabs;
	int32 ActiveTab = TabSequences;
	// Same trick the key bindings page uses: a dynamic delegate cannot carry which row was
	// clicked, so the buttons say who they are.
	UPROPERTY() TMap<TObjectPtr<UButton>, FString> ButtonScenes;
};
