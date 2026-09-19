#include "UI/CrtTabsWidget.h"
#include "UI/CrtStyle.h"
#include "Core/BasePlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"

void UCrtTabBinding::OnClicked() { if (Tabs.IsValid()) { Tabs->HandleClick(Index); } }

TArray<FString> UCrtTabsWidget::StandardTabs(const FString& PlayerName)
{
	return { PlayerName.IsEmpty() ? TEXT("REPLI CAN") : PlayerName.ToUpper(), TEXT("REFERENCE"), TEXT("APPEARANCE") };
}

UCrtTabsWidget* UCrtTabsWidget::Make(APlayerController* Owner, const TArray<FString>& Labels, int32 Active, int32 Size)
{
	UCrtTabsWidget* W = CreateWidget<UCrtTabsWidget>(Owner, UCrtTabsWidget::StaticClass());
	if (W) { W->Configure(Labels, Active, Size); }
	return W;
}

void UCrtTabsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Row"));
	WidgetTree->RootWidget = Row;
	if (PendingLabels.Num() > 0) { Configure(PendingLabels, ActiveIndex, FontSize); }
}

void UCrtTabsWidget::Configure(const TArray<FString>& Labels, int32 Active, int32 Size)
{
	PendingLabels = Labels; ActiveIndex = Active; FontSize = Size;
	if (!Row) { return; }
	Row->ClearChildren(); Texts.Reset(); Bindings.Reset();
	for (int32 i = 0; i < Labels.Num(); ++i)
	{
		if (i > 0) { Row->AddChildToHorizontalBox(Crt::FixedText(WidgetTree, TEXT(" | "), Size, Crt::Faint))->SetVerticalAlignment(VAlign_Center); }
		UButton* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		B->SetStyle(Crt::ButtonStyle());
		UTextBlock* T = Crt::FixedText(WidgetTree, Labels[i], Size, i == Active ? Crt::Green : Crt::DimGreen);
		B->AddChild(T);
		if (UButtonSlot* TS = Cast<UButtonSlot>(T->Slot)) { TS->SetPadding(FMargin(10.0f, 6.0f)); TS->SetHorizontalAlignment(HAlign_Center); TS->SetVerticalAlignment(VAlign_Center); }
		UCrtTabBinding* Binding = NewObject<UCrtTabBinding>(this);
		Binding->Tabs = this; Binding->Index = i;
		B->OnClicked.AddDynamic(Binding, &UCrtTabBinding::OnClicked);
		Bindings.Add(Binding); Texts.Add(T);
		Row->AddChildToHorizontalBox(B)->SetVerticalAlignment(VAlign_Fill);
	}
}

void UCrtTabsWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// Pages are often built before their controller is known: take the name from the player once it is.
	if (bNameResolved) { return; }
	if (ABasePlayerController* PC = Cast<ABasePlayerController>(GetOwningPlayer()))
	{
		const FString Name = PC->GetPlayerDisplayName();
		if (!Name.IsEmpty()) { SetLabel(TabSheet, Name.ToUpper()); bNameResolved = true; }
	}
}

void UCrtTabsWidget::SetLabel(int32 Index, const FString& Label)
{
	if (PendingLabels.IsValidIndex(Index)) { PendingLabels[Index] = Label; }
	if (Texts.IsValidIndex(Index) && Texts[Index]) { Texts[Index]->SetText(FText::FromString(Label)); }
}
