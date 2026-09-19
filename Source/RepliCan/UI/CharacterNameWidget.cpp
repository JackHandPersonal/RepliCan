#include "UI/CharacterNameWidget.h"
#include "UI/CrtStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"

void UCharacterNameWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Never intercept the mouse: the label floats over the character the
	// user is about to click (see FEditClickProcessor's viewport check).
	SetVisibility(ESlateVisibility::HitTestInvisible);

	// Text only -- no backing plate. Bold, antialiased (Slate's font
	// rendering), clearly translucent with a faint drop shadow -- a soft
	// tag hovering over the character, not a HUD element.
	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Frame"));
	Frame->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
	Frame->SetPadding(FMargin(4.0f, 2.0f));
	Frame->SetHorizontalAlignment(HAlign_Center);
	Frame->SetVerticalAlignment(VAlign_Center);
	WidgetTree->RootWidget = Frame;

	NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("NameText"));
	NameText->SetFont(Crt::Mono(14));
	NameText->SetColorAndOpacity(FSlateColor(FLinearColor(0.96f, 0.94f, 0.88f, 0.55f)));
	NameText->SetShadowOffset(FVector2D(1.0f, 1.0f));
	NameText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.3f));
	NameText->SetJustification(ETextJustify::Center);
	Frame->SetContent(NameText);
}

void UCharacterNameWidget::SetName(const FString& Name)
{
	if (NameText) { NameText->SetText(FText::FromString(Name)); }
}

void UCharacterNameWidget::SetFontSize(int32 Size)
{
	Size = FMath::Max(Size, 4);
	if (!NameText || Size == CurrentFontSize) { return; }
	CurrentFontSize = Size;
	NameText->SetFont(Crt::Mono(Size));
}
