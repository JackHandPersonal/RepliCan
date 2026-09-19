// The tab strip that sits inside the header rule of the full-screen panels:
// "[ NAME | REFERENCE | APPEARANCE ]" with the active tab bright and the others dim and
// clickable. Shared by the character sheet, the Reference screen and the Appearance
// screen so they read as pages of one console.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CrtTabsWidget.generated.h"

class UHorizontalBox;
class UButtonSlot;
class UTextBlock;
class UButton;

DECLARE_DELEGATE_OneParam(FOnCrtTab, int32);

UCLASS()
class REPLICAN_API UCrtTabBinding : public UObject
{
	GENERATED_BODY()
public:
	TWeakObjectPtr<class UCrtTabsWidget> Tabs;
	int32 Index = 0;
	UFUNCTION() void OnClicked();
};

UCLASS()
class REPLICAN_API UCrtTabsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// The tab names in order; the standard set for the console pages.
	static TArray<FString> StandardTabs(const FString& PlayerName);
	enum { TabSheet = 0, TabReference = 1, TabAppearance = 2 };
	void Configure(const TArray<FString>& Labels, int32 Active, int32 Size);
	void SetLabel(int32 Index, const FString& Label);
	static UCrtTabsWidget* Make(APlayerController* Owner, const TArray<FString>& Labels, int32 Active, int32 Size);
	FOnCrtTab OnTab;
	void HandleClick(int32 Index) { if (Index != ActiveIndex) { OnTab.ExecuteIfBound(Index); } }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY() TObjectPtr<UHorizontalBox> Row;
	UPROPERTY() TArray<TObjectPtr<UTextBlock>> Texts;
	UPROPERTY() TArray<TObjectPtr<UCrtTabBinding>> Bindings;
	TArray<FString> PendingLabels;
	int32 ActiveIndex = 0;
	int32 FontSize = 15;
	bool bNameResolved = false;
};
