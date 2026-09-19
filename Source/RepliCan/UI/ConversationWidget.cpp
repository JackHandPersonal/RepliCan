#include "UI/ConversationWidget.h"
#include "Core/BasePlayerController.h"
#include "UI/CrtStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/ButtonSlot.h"
#include "Components/ScrollBox.h"
#include "Components/Spacer.h"
#include "Components/ScrollBoxSlot.h"
#include "Framework/Application/SlateApplication.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"

void UConversationChoiceBinding::OnClicked()
{
	if (Widget.IsValid()) { Widget->QueueChoice(Index); }
	else if (Controller.IsValid()) { Controller->ChooseConversationOption(Index); }
}

FString UConversationWidget::DescribeChoices() const
{
	if (!ChoiceBox || !History) { return TEXT("no boxes"); }
	FString Out = FString::Printf(TEXT("parent=%s idx=%d/%d vis=%d geo=%s"), ChoiceBox->GetParent() ? *ChoiceBox->GetParent()->GetName() : TEXT("none"), History->GetChildIndex(ChoiceBox), History->GetChildrenCount(), (int32)ChoiceBox->GetVisibility(), *ChoiceBox->GetCachedGeometry().GetLocalSize().ToString());
	for (int32 i = 0; i < ChoiceBox->GetChildrenCount(); ++i)
	{
		UWidget* C = ChoiceBox->GetChildAt(i);
		Out += FString::Printf(TEXT(" | child %d %s vis=%d geo=%s"), i, *C->GetClass()->GetName(), (int32)C->GetVisibility(), *C->GetCachedGeometry().GetLocalSize().ToString());
	}
	return Out;
}

int32 UConversationWidget::CountChoiceButtons() const
{
	if (!ChoiceBox) { return 0; }
	int32 N = 0;
	for (int32 i = 0; i < ChoiceBox->GetChildrenCount(); ++i) { if (Cast<UButton>(ChoiceBox->GetChildAt(i))) { ++N; } }
	return N;
}

void UConversationWidget::ClickChoice(int32 Index)
{
	if (!ChoiceBox || !FSlateApplication::IsInitialized()) { return; }
	int32 N = 0;
	for (int32 i = 0; i < ChoiceBox->GetChildrenCount(); ++i)
	{
		UButton* Button = Cast<UButton>(ChoiceBox->GetChildAt(i));
		if (!Button) { continue; }
		if (N++ != Index) { continue; }
		const FGeometry& Geo = Button->GetCachedGeometry();
		const FVector2D Centre = Geo.GetAbsolutePosition() + Geo.GetAbsoluteSize() * 0.5;
		FSlateApplication& App = FSlateApplication::Get();
		App.SetCursorPos(Centre);
		const FPointerEvent Move(0, Centre, Centre, FVector2D::ZeroVector, TSet<FKey>(), FModifierKeysState());
		App.ProcessMouseMoveEvent(Move);
		const FPointerEvent Down(0, Centre, Centre, TSet<FKey>{ EKeys::LeftMouseButton }, EKeys::LeftMouseButton, 0.0f, FModifierKeysState());
		App.ProcessMouseButtonDownEvent(nullptr, Down);
		const FPointerEvent Up(0, Centre, Centre, TSet<FKey>(), EKeys::LeftMouseButton, 0.0f, FModifierKeysState());
		App.ProcessMouseButtonUpEvent(Up);
		return;
	}
}

void UConversationWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	Panel->SetBrushColor(Crt::Panel);
	Panel->SetPadding(FMargin(30.0f, 26.0f));
	WidgetTree->RootWidget = Panel;

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	Panel->SetContent(Column);

	TitleBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Title"));
	Column->AddChildToVerticalBox(TitleBox)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 2.0f));
	DescriptionText = Crt::Text(WidgetTree, TEXT(""), 11, Crt::DimGreen);
	DescriptionText->SetAutoWrapText(true);
	Column->AddChildToVerticalBox(DescriptionText)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
	// A hairline rule under the header; the transcript (and its bar) start below it.
	UBorder* Rule = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Rule"));
	Rule->SetBrushColor(FLinearColor(Crt::DimGreen.R, Crt::DimGreen.G, Crt::DimGreen.B, 0.6f));
	Rule->SetPadding(FMargin(0.0f));
	USpacer* RuleHeight = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	RuleHeight->SetSize(FVector2D(1.0f, 1.0f));
	Rule->SetContent(RuleHeight);
	Column->AddChildToVerticalBox(Rule)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));

	// One scrolling transcript: the exchange so far, and the player's
	// replies directly under the latest line. The wheel scrolls it while
	// the cursor is over the panel; a thin bar shows when it overflows.
	History = Crt::ScrollBox(WidgetTree, 10.0f);
	UVerticalBoxSlot* HistorySlot = Column->AddChildToVerticalBox(History);
	HistorySlot->SetSize(ESlateSizeRule::Fill);

	ChoiceBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Choices"));
}

void UConversationWidget::SetSpeakerName(const FString& Name)
{
	SetSpeaker(Name, DescriptionText ? DescriptionText->GetText().ToString() : FString());
}

void UConversationWidget::SetSpeaker(const FString& Name, const FString& Description)
{
	if (Name != SpeakerName && TitleBox) { Crt::FillSmallCaps(WidgetTree, TitleBox, Name, 20, Crt::Green); SpeakerName = Name; }
	if (DescriptionText)
	{
		DescriptionText->SetText(FText::FromString(Description));
		DescriptionText->SetVisibility(Description.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void UConversationWidget::ClearHistory()
{
	if (History) { History->ClearChildren(); }
	if (ChoiceBox) { ChoiceBox->ClearChildren(); }
	Bindings.Reset();
}

void UConversationWidget::PlaceChoicesLast()
{
	// The reply block always sits right under the newest line.
	if (!History || !ChoiceBox) { return; }
	if (ChoiceBox->GetParent() == History) { History->RemoveChild(ChoiceBox); }
	if (UScrollBoxSlot* ChoiceSlot = Cast<UScrollBoxSlot>(History->AddChild(ChoiceBox))) { ChoiceSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 4.0f)); }
	History->ScrollToEnd();
	ScrollTicks = 3;   // and again once the new line has been measured
}

void UConversationWidget::AddLine(const FString& Speaker, const FString& Text, bool bPlayer)
{
	if (!History) { return; }
	UTextBlock* Line = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Line->SetText(FText::FromString(bPlayer ? FString::Printf(TEXT("You: %s"), *Text) : FString::Printf(TEXT("%s: %s"), *Speaker, *Text)));
	Line->SetFont(Crt::Mono(13));
	Line->SetColorAndOpacity(FSlateColor(bPlayer ? Crt::DimGreen : Crt::Green));
	Line->SetAutoWrapText(true);
	UScrollBoxSlot* LineSlot = Cast<UScrollBoxSlot>(History->AddChild(Line));
	if (LineSlot) { LineSlot->SetPadding(FMargin(bPlayer ? 24.0f : 0.0f, 3.0f, 12.0f, 3.0f)); }
	// A new line retires the replies that were on offer: the player's own
	// line means one was taken, the character's means new ones are coming.
	if (ChoiceBox) { ChoiceBox->ClearChildren(); }
	Bindings.Reset();
	PlaceChoicesLast();
}

void UConversationWidget::SetChoices(const TArray<FString>& Choices)
{
	if (!ChoiceBox) { return; }
	ChoiceBox->ClearChildren();
	Bindings.Reset();
	for (int32 i = 0; i < Choices.Num(); ++i)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		Button->SetStyle(Crt::ButtonStyle());
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Text->SetText(FText::FromString(FString::Printf(TEXT("%d >  %s"), i + 1, *Choices[i])));
		Text->SetFont(Crt::Mono(13));
		Text->SetColorAndOpacity(FSlateColor(Crt::Green));
		Text->SetAutoWrapText(true);
		Button->AddChild(Text);
		Text->SetJustification(ETextJustify::Left);
		// Fill, not Left: a left-aligned auto-wrap block measures from its own last wrap and collapses to a word per line.
		if (UButtonSlot* TextSlot = Cast<UButtonSlot>(Text->Slot)) { TextSlot->SetPadding(FMargin(12.0f, 5.0f)); TextSlot->SetHorizontalAlignment(HAlign_Fill); }
		UConversationChoiceBinding* Binding = NewObject<UConversationChoiceBinding>(this);
		Binding->Controller = OwnerController;
		Binding->Widget = this;
		Binding->Index = i;
		Bindings.Add(Binding);
		Button->OnClicked.AddDynamic(Binding, &UConversationChoiceBinding::OnClicked);
		UVerticalBoxSlot* ButtonSlot = ChoiceBox->AddChildToVerticalBox(Button);
		ButtonSlot->SetPadding(FMargin(0.0f, 2.0f, 12.0f, 2.0f));
		ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
	}
	PlaceChoicesLast();
}

void UConversationWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Clock += InDeltaTime;
	if (ScrollTicks > 0 && History) { History->ScrollToEnd(); --ScrollTicks; }
	if (PendingChoice >= 0)
	{
		const int32 Taken = PendingChoice; PendingChoice = -1;
		if (OwnerController) { OwnerController->ChooseConversationOption(Taken); }
	}
}

int32 UConversationWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	Crt::PaintFrame(OutDrawElements, AllottedGeometry, FVector2f::ZeroVector, FVector2f(AllottedGeometry.GetLocalSize()), LayerId + 1, Clock);
	return LayerId + 2;
}
