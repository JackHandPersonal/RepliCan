#include "CalloutWidget.h"
#include "CrtStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"

void UCalloutWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::HitTestInvisible);

	UBorder* Box = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Box"));
	Box->SetBrushColor(FLinearColor(Crt::Panel.R, Crt::Panel.G, Crt::Panel.B, 0.7f));
	Box->SetPadding(FMargin(10.0f, 5.0f));
	WidgetTree->RootWidget = Box;

	Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text"));
	Text->SetFont(Crt::Mono(13));
	Text->SetColorAndOpacity(FSlateColor(Crt::Green));
	Text->SetJustification(ETextJustify::Center);
	Text->SetAutoWrapText(true);
	Text->SetWrapTextAt(320.0f);
	Box->SetContent(Text);
}

void UCalloutWidget::SetCalloutText(const FString& InText, bool bSpeech)
{
	if (!Text) { return; }
	// Speech in quotes and italics; notes plain.
	Text->SetText(FText::FromString(bSpeech ? FString::Printf(TEXT("“%s”"), *InText) : InText));
	Text->SetFont(Crt::Mono(13));
}
