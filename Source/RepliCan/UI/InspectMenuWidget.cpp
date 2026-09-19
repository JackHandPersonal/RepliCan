#include "UI/InspectMenuWidget.h"
#include "UI/CrtStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"

namespace
{
	const FLinearColor NameColor = Crt::Green;
	const FLinearColor DescriptionColor = Crt::DimGreen;
	const FLinearColor OptionColor = Crt::DimGreen;
	const FLinearColor SelectedColor = Crt::Green;
}

void UInspectMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::HitTestInvisible);

	UBorder* Box = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Box"));
	// A plain rounded panel: no corner brackets, no frame.
	FSlateBrush Rounded;
	Rounded.DrawAs = ESlateBrushDrawType::RoundedBox;
	Rounded.OutlineSettings = FSlateBrushOutlineSettings(FVector4(10.0f, 10.0f, 10.0f, 10.0f), FLinearColor(Crt::DimGreen.R, Crt::DimGreen.G, Crt::DimGreen.B, 0.5f), 1.0f);
	Rounded.TintColor = FSlateColor(Crt::Panel);
	Box->SetBrush(Rounded);
	Box->SetPadding(FMargin(18.0f, 12.0f));
	WidgetTree->RootWidget = Box;

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	Box->SetContent(Column);

	NameBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Name"));
	Column->AddChildToVerticalBox(NameBox);

	DescriptionText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Description"));
	DescriptionText->SetFont(Crt::Mono(10));
	DescriptionText->SetColorAndOpacity(FSlateColor(DescriptionColor));
	DescriptionText->SetAutoWrapText(true);
	DescriptionText->SetWrapTextAt(260.0f);
	Column->AddChildToVerticalBox(DescriptionText)->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 6.0f));

	ActionBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Actions"));
	Column->AddChildToVerticalBox(ActionBox);
}

void UInspectMenuWidget::SetContent(const FString& Name, const FString& Description, const TArray<FString>& Actions, int32 SelectedIndex)
{
	if (NameBox) { Crt::FillSmallCaps(WidgetTree, NameBox, Name, 16, NameColor); }
	if (DescriptionText)
	{
		DescriptionText->SetText(FText::FromString(Description));
		DescriptionText->SetVisibility(Description.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	ActionLabels = Actions;
	if (ActionBox)
	{
		ActionBox->ClearChildren();
		ActionTexts.Reset();
		for (const FString& Label : Actions)
		{
			UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			Text->SetFont(Crt::Mono(12));
			ActionBox->AddChildToVerticalBox(Text)->SetPadding(FMargin(0.0f, 1.0f));
			ActionTexts.Add(Text);
		}
	}
	Selected = SelectedIndex;
	RefreshSelection();
}

void UInspectMenuWidget::SetSelectedIndex(int32 SelectedIndex)
{
	Selected = SelectedIndex;
	RefreshSelection();
}

void UInspectMenuWidget::RefreshSelection()
{
	for (int32 i = 0; i < ActionTexts.Num(); ++i)
	{
		const bool bSel = (i == Selected);
		ActionTexts[i]->SetText(FText::FromString(FString::Printf(TEXT("%s%d %s"), bSel ? TEXT("> ") : TEXT("  "), i + 1, *ActionLabels[i])));   // numbered: the digit keys pick a row
		ActionTexts[i]->SetColorAndOpacity(FSlateColor(bSel ? SelectedColor : OptionColor));
		ActionTexts[i]->SetFont(Crt::Mono(12));
	}
}

int32 UInspectMenuWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	if (bLeader)
	{
		// The panel's top-left in viewport space, then the anchor in panel-local space; the
		// leader runs to the nearest point on the panel's edge, with a small square at the thing.
		const FVector2D Size(AllottedGeometry.GetLocalSize());
		const FVector2D TopLeft = LeaderPanelPos - LeaderAlign * Size;
		const FVector2D A = LeaderAnchor - TopLeft;
		const FVector2D Edge(FMath::Clamp(A.X, 0.0, Size.X), FMath::Clamp(A.Y, 0.0, Size.Y));
		const FLinearColor Col(Crt::Green.R, Crt::Green.G, Crt::Green.B, 0.75f);
		TArray<FVector2f> Pts; Pts.Add(FVector2f(A)); Pts.Add(FVector2f(Edge));
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 2, AllottedGeometry.ToPaintGeometry(), Pts, ESlateDrawEffect::None, Col, true, 1.0f);
		TArray<FVector2f> Box; const float R = 3.0f;
		Box.Add(FVector2f(A.X - R, A.Y - R)); Box.Add(FVector2f(A.X + R, A.Y - R)); Box.Add(FVector2f(A.X + R, A.Y + R)); Box.Add(FVector2f(A.X - R, A.Y + R)); Box.Add(FVector2f(A.X - R, A.Y - R));
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 2, AllottedGeometry.ToPaintGeometry(), Box, ESlateDrawEffect::None, Col, true, 1.5f);
	}
	return LayerId + 3;
}
