#include "UI/CalloutWidget.h"
#include "UI/CrtStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"

void UCalloutWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::HitTestInvisible);

	// THE BOX MUST NOT BE THE ROOT. A UBorder paints across the whole of its slot, and a user
	// widget's slot in the viewport is the WHOLE SCREEN -- so this panel-coloured, seventy-percent
	// translucent box was washing the entire screen green every time anything called out, which is
	// what "the screen goes green when I pick something up" was. Sitting it in a vertical box as a
	// centred, auto-height child means it is only ever as big as the words inside it, whatever the
	// slot around it does.
	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UBorder* Box = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Box"));
	Box->SetBrushColor(FLinearColor(Crt::Panel.R, Crt::Panel.G, Crt::Panel.B, 0.7f));
	Box->SetPadding(FMargin(10.0f, 5.0f));
	if (UVerticalBoxSlot* BoxSlot = Root->AddChildToVerticalBox(Box))
	{
		BoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		BoxSlot->SetHorizontalAlignment(HAlign_Center);
		BoxSlot->SetVerticalAlignment(VAlign_Top);
	}

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
