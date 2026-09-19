// SAVE / LOAD: a placeholder page in the character sheet's box. Eight slots down the page -- the
// quick slot and seven named ones -- each with what is in it (the level and when it was written)
// and one button: SAVE on the save page, LOAD on the load page (dim when the slot is empty).
// The real thing later gets a screenshot per slot and a name the player types; the plumbing
// (USaveGameSubsystem by slot name) is already what this drives.
#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SaveLoadWidget.generated.h"

class USaveLoadWidget;

UCLASS()
class USaveSlotBinding : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY() TObjectPtr<USaveLoadWidget> Page;
	FString Slot;
	UFUNCTION() void OnClicked();
};

UCLASS()
class REPLICAN_API USaveLoadWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void Open(class ABasePlayerController* InController, bool bInLoad);
	void Refresh();
	void Pick(const FString& SlotName);
	FSimpleDelegate OnClose;
	bool IsLoadPage() const { return bLoad; }

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	UFUNCTION() void OnBack();

private:
	UPROPERTY() TObjectPtr<class ABasePlayerController> Controller;
	UPROPERTY() TObjectPtr<class UBorder> Panel;
	UPROPERTY() TObjectPtr<class UHorizontalBox> HeaderBox;
	UPROPERTY() TObjectPtr<class UVerticalBox> Rows;
	UPROPERTY() TObjectPtr<class UTextBlock> Note;
	UPROPERTY() TArray<TObjectPtr<USaveSlotBinding>> Bindings;
	bool bLoad = false;
};
