#include "UI/ConfirmDialogWidget.h"
#include "UI/CrtStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"

void UConfirmDialogWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Dim"));
	Dim->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.35f));
	UOverlaySlot* DimSlot = Root->AddChildToOverlay(Dim);
	DimSlot->SetHorizontalAlignment(HAlign_Fill);
	DimSlot->SetVerticalAlignment(VAlign_Fill);

	Box = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Box"));
	Box->SetBrushColor(Crt::Panel);
	Box->SetPadding(FMargin(36.0f, 26.0f));
	UOverlaySlot* BoxSlot = Root->AddChildToOverlay(Box);
	BoxSlot->SetHorizontalAlignment(HAlign_Center);
	BoxSlot->SetVerticalAlignment(VAlign_Center);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	Box->SetContent(Column);

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
	TitleText->SetFont(Crt::Mono(16));
	TitleText->SetColorAndOpacity(FSlateColor(Crt::Green));
	Column->AddChildToVerticalBox(TitleText)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));

	MessageText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Message"));
	MessageText->SetFont(Crt::Mono(12));
	MessageText->SetColorAndOpacity(FSlateColor(Crt::DimGreen));
	Column->AddChildToVerticalBox(MessageText)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));

	UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Buttons"));
	UVerticalBoxSlot* ButtonsSlot = Column->AddChildToVerticalBox(Buttons);
	ButtonsSlot->SetHorizontalAlignment(HAlign_Center);

	UButton* Save = MakeButton(TEXT("Save"));
	Save->OnClicked.AddDynamic(this, &UConfirmDialogWidget::OnSaveClicked);
	Buttons->AddChildToHorizontalBox(Save)->SetPadding(FMargin(4.0f, 0.0f));
	UButton* Discard = MakeButton(TEXT("Discard"));
	Discard->OnClicked.AddDynamic(this, &UConfirmDialogWidget::OnDiscardClicked);
	Buttons->AddChildToHorizontalBox(Discard)->SetPadding(FMargin(4.0f, 0.0f));
	UButton* Cancel = MakeButton(TEXT("Cancel"));
	Cancel->OnClicked.AddDynamic(this, &UConfirmDialogWidget::OnCancelClicked);
	Buttons->AddChildToHorizontalBox(Cancel)->SetPadding(FMargin(4.0f, 0.0f));
}

UButton* UConfirmDialogWidget::MakeButton(const FString& Label)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	Button->SetStyle(Crt::ButtonStyle());
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text->SetText(FText::FromString(Label.ToUpper()));
	Text->SetFont(Crt::Mono(15));
	Text->SetColorAndOpacity(FSlateColor(Crt::Green));
	Button->AddChild(Text);
	if (UButtonSlot* TextSlot = Cast<UButtonSlot>(Text->Slot)) { TextSlot->SetPadding(FMargin(18.0f, 6.0f)); }
	return Button;
}

void UConfirmDialogWidget::Setup(const FString& Title, const FString& Message, TFunction<void()> InOnSave, TFunction<void()> InOnDiscard, TFunction<void()> InOnCancel)
{
	if (TitleText) { TitleText->SetText(FText::FromString(Title)); }
	if (MessageText) { MessageText->SetText(FText::FromString(Message)); }
	OnSave = MoveTemp(InOnSave);
	OnDiscard = MoveTemp(InOnDiscard);
	OnCancel = MoveTemp(InOnCancel);
}

void UConfirmDialogWidget::Finish(TFunction<void()>& Callback)
{
	// Take the callback first: it may open another page that tears this
	// dialog down.
	TFunction<void()> Run = MoveTemp(Callback);
	OnSave = nullptr; OnDiscard = nullptr; OnCancel = nullptr;
	RemoveFromParent();
	if (Run) { Run(); }
}

void UConfirmDialogWidget::OnSaveClicked() { Finish(OnSave); }
void UConfirmDialogWidget::OnDiscardClicked() { Finish(OnDiscard); }
void UConfirmDialogWidget::OnCancelClicked() { Finish(OnCancel); }

void UConfirmDialogWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Clock += InDeltaTime;
}

int32 UConfirmDialogWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	Crt::PaintFrameAround(OutDrawElements, AllottedGeometry, Box, LayerId + 1, Clock);
	return LayerId + 2;
}
