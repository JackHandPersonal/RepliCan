#include "UI/SaveLoadWidget.h"
#include "Core/BasePlayerController.h"
#include "UI/CrtStyle.h"
#include "UI/CrtRuleWidget.h"
#include "Core/SaveGameSubsystem.h"
#include "UI/SheetSpec.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

void USaveSlotBinding::OnClicked() { if (Page) { Page->Pick(Slot); } }

void USaveLoadWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	// The sheet's box, centred: an overlay (the page fills the screen) holding one sized panel.
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("SaveLoadRoot"));
	WidgetTree->RootWidget = Root;
	USizeBox* Fit = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SaveLoadFit"));
	Fit->SetWidthOverride(760.0f);
	UOverlaySlot* FitSlot = Root->AddChildToOverlay(Fit);
	FitSlot->SetHorizontalAlignment(HAlign_Center); FitSlot->SetVerticalAlignment(VAlign_Center);
	Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SaveLoadPanel"));
	Panel->SetBrushColor(Crt::Panel);
	Panel->SetPadding(FMargin(34.0f, 26.0f));
	Fit->AddChild(Panel);
	UVerticalBox* Outer = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SaveLoadOuter"));
	Panel->SetContent(Outer);
	HeaderBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SaveLoadHeader"));
	Outer->AddChildToVerticalBox(HeaderBox)->SetPadding(FMargin(0, 0, 0, 12));
	Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SaveLoadRows"));
	Outer->AddChildToVerticalBox(Rows);
	Note = Crt::Text(WidgetTree, TEXT(""), 13, Crt::DimGreen);
	Outer->AddChildToVerticalBox(Note)->SetPadding(FMargin(0, 12, 0, 0));
}

void USaveLoadWidget::Open(ABasePlayerController* InController, bool bInLoad)
{
	Controller = InController; bLoad = bInLoad;
	if (HeaderBox)
	{
		HeaderBox->ClearChildren();
		const FSheetSpec& S = FSheetSpec::Get();
		HeaderBox->AddChildToHorizontalBox(Crt::FixedText(WidgetTree, S.HeaderLeft, S.RuleSize, Crt::Faint))->SetVerticalAlignment(VAlign_Center);
		HeaderBox->AddChildToHorizontalBox(Crt::FixedText(WidgetTree, bLoad ? TEXT("LOAD GAME") : TEXT("SAVE GAME"), S.NameSize, Crt::Green))->SetVerticalAlignment(VAlign_Center);
		UHorizontalBoxSlot* RuleSlot = HeaderBox->AddChildToHorizontalBox(UCrtRuleWidget::Make(GetOwningPlayer(), S.HeaderRight, S.RuleSize, Crt::Faint));
		RuleSlot->SetSize(ESlateSizeRule::Fill); RuleSlot->SetVerticalAlignment(VAlign_Center);
		if (!S.HeaderEnd.IsEmpty()) { HeaderBox->AddChildToHorizontalBox(Crt::FixedText(WidgetTree, S.HeaderEnd, S.RuleSize, Crt::Faint))->SetVerticalAlignment(VAlign_Center); }
		UButton* Close = Crt::Button(WidgetTree, TEXT("[ X ]"), S.NameSize, Crt::Green);
		Close->OnClicked.AddDynamic(this, &USaveLoadWidget::OnBack);
		HeaderBox->AddChildToHorizontalBox(Close)->SetPadding(FMargin(12, 0, 0, 0));
	}
	if (Note) { Note->SetText(FText::FromString(bLoad ? TEXT("a placeholder: pick a slot to load. F9 loads the quick slot.") : TEXT("a placeholder: pick a slot to write. F5 writes the quick slot."))); }
	Refresh();
	SetKeyboardFocus();
}

void USaveLoadWidget::Refresh()
{
	if (!Rows) { return; }
	Rows->ClearChildren(); Bindings.Reset();
	USaveGameSubsystem* Saves = (Controller && Controller->GetGameInstance()) ? Controller->GetGameInstance()->GetSubsystem<USaveGameSubsystem>() : nullptr;
	static const TCHAR* Slots[] = { TEXT("Quick"), TEXT("Slot1"), TEXT("Slot2"), TEXT("Slot3"), TEXT("Slot4"), TEXT("Slot5"), TEXT("Slot6"), TEXT("Slot7") };
	for (const TCHAR* SlotName : Slots)
	{
		const bool bExists = Saves && Saves->SlotExists(SlotName);
		FString What = TEXT("-- empty --");
		if (bExists)
		{
			const FString File = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"), FString(SlotName) + TEXT(".sav"));
			const FDateTime Stamp = IFileManager::Get().GetTimeStamp(*File);
			FString Level = Saves->GetSlotLevel(SlotName);
			int32 SlashAt = INDEX_NONE; if (Level.FindLastChar(TEXT('/'), SlashAt)) { Level = Level.Mid(SlashAt + 1); }
			What = FString::Printf(TEXT("%s   %s"), *Level, Stamp == FDateTime::MinValue() ? TEXT("") : *Stamp.ToString(TEXT("%Y-%m-%d %H:%M")));
		}
		UHorizontalBox* R = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		const FString Label = FString(SlotName) == TEXT("Quick") ? FString(TEXT("QUICK")) : FString::Printf(TEXT("SLOT %s"), *FString(SlotName).Mid(4));
		UHorizontalBoxSlot* LS = R->AddChildToHorizontalBox(Crt::FixedText(WidgetTree, Label, 16, Crt::Green)); LS->SetVerticalAlignment(VAlign_Center); LS->SetPadding(FMargin(0, 0, 18, 0));
		UHorizontalBoxSlot* WS = R->AddChildToHorizontalBox(Crt::FixedText(WidgetTree, What, 15, bExists ? Crt::Green : Crt::DimGreen)); WS->SetSize(ESlateSizeRule::Fill); WS->SetVerticalAlignment(VAlign_Center);
		const bool bCan = !bLoad || bExists;
		UButton* B = Crt::Button(WidgetTree, bLoad ? TEXT("[ LOAD ]") : (bExists ? TEXT("[ OVERWRITE ]") : TEXT("[ SAVE ]")), 16, bCan ? Crt::Green : Crt::Faint);
		B->SetIsEnabled(bCan);
		USaveSlotBinding* Binding = NewObject<USaveSlotBinding>(this);
		Binding->Page = this; Binding->Slot = SlotName;
		B->OnClicked.AddDynamic(Binding, &USaveSlotBinding::OnClicked);
		Bindings.Add(Binding);
		UHorizontalBoxSlot* BS = R->AddChildToHorizontalBox(B); BS->SetVerticalAlignment(VAlign_Center);
		Rows->AddChildToVerticalBox(R)->SetPadding(FMargin(0, 3));
	}
}

void USaveLoadWidget::Pick(const FString& SlotName)
{
	if (!Controller) { return; }
	if (bLoad) { Controller->LoadFromSlot(SlotName); return; }   // the page goes with the world
	Controller->SaveToSlot(SlotName);
	Refresh();
	if (Note) { Note->SetText(FText::FromString(FString::Printf(TEXT("written: %s"), *SlotName.ToUpper()))); }
}

FReply USaveLoadWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape) { OnBack(); return FReply::Handled(); }
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void USaveLoadWidget::OnBack() { OnClose.ExecuteIfBound(); }
