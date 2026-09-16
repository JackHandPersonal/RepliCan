// Every scripted scene in the game, listed, with a button that jumps straight into it.
//
// A sequence is only reachable in play by arriving at whatever triggers it, which makes the
// tenth line of a five-minute scene expensive to look at and nearly impossible to iterate on.
// This is the way in: the list is read off disk (Sequences/*.json) every time the panel opens,
// so a scene authored a minute ago is already here, and nothing needs registering in code.
//
// Playing one from here is the real thing, not a preview -- the same PlaySequence the game
// calls -- so what is seen is what ships.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ScenesWidget.generated.h"

class UVerticalBox;
class UTextBlock;
class UButton;
class ABasePlayerController;

DECLARE_DELEGATE(FOnScenesClosed);

UCLASS()
class REPLICAN_API UScenesWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(ABasePlayerController* Controller) { OwnerController = Controller; }
	// Re-reads the directory each time, so the list is never stale.
	void Rebuild();
	FOnScenesClosed OnClose;

protected:
	virtual void NativeOnInitialized() override;

private:
	// One row per scene: its name, how many steps and speaking parts it has, and PLAY. The
	// counts are there because they are the cheapest possible answer to "is this the scene I
	// mean" without opening the file.
	struct FSceneRow
	{
		FString Name;
		FString Detail;
		bool bIsSequence = true;
	};
	TArray<FSceneRow> Scan() const;

	UFUNCTION() void OnPlayClicked();
	UFUNCTION() void OnBack();

	UPROPERTY() TObjectPtr<ABasePlayerController> OwnerController;
	UPROPERTY() TObjectPtr<UVerticalBox> Column;
	// Same trick the key bindings page uses: a dynamic delegate cannot carry which row was
	// clicked, so the buttons say who they are.
	UPROPERTY() TMap<TObjectPtr<UButton>, FString> ButtonScenes;
};
