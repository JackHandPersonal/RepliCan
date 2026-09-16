#include "CrtRuleWidget.h"
#include "CrtStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"

UCrtRuleWidget* UCrtRuleWidget::Make(APlayerController* Owner, const FString& InBase, int32 InSize, const FLinearColor& InColor, TCHAR InRuleChar)
{
	UCrtRuleWidget* W = CreateWidget<UCrtRuleWidget>(Owner, UCrtRuleWidget::StaticClass());
	if (W) { W->Configure(InBase, InSize, InColor, InRuleChar, false); }
	return W;
}

UCrtRuleWidget* UCrtRuleWidget::MakeVertical(APlayerController* Owner, const FString& Glyph, int32 InSize, const FLinearColor& InColor)
{
	UCrtRuleWidget* W = CreateWidget<UCrtRuleWidget>(Owner, UCrtRuleWidget::StaticClass());
	if (W) { W->Configure(Glyph.IsEmpty() ? TEXT("|") : Glyph.Left(1), InSize, InColor, 0, true); }
	return W;
}

void UCrtRuleWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	Text = Crt::FixedText(WidgetTree, Base, Size, Color);
	Text->SetRenderOpacity(0.0f);   // until the first fit
	WidgetTree->RootWidget = Text;
}

void UCrtRuleWidget::Configure(const FString& InBase, int32 InSize, const FLinearColor& InColor, TCHAR InRuleChar, bool bInVertical)
{
	Base = InBase; Size = InSize; Color = InColor; bVertical = bInVertical; Count = -1;
	RuleChar = InRuleChar ? InRuleChar : (Base.Len() ? Base[Base.Len() - 1] : TEXT('-'));
	if (Text)
	{
		Text->SetText(FText::FromString(Base));
		Text->SetFont(Crt::Fixed(Size));
		Text->SetColorAndOpacity(FSlateColor(Color));
	}
}

void UCrtRuleWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Fit();
}

void UCrtRuleWidget::Fit()
{
	if (!Text || !FSlateApplication::IsInitialized()) { return; }
	const FVector2D Local = GetCachedGeometry().GetLocalSize();
	const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	if (bVertical)
	{
		// A column of glyphs: one per line, as many lines as fit the height.
		if (Local.Y <= 1.0f) { return; }
		float LineH = Measure->Measure(Base, Text->GetFont()).Y;
		const float Drawn = Text->GetDesiredSize().Y;
		if (Count > 0 && Drawn > 0.0f) { LineH = Drawn / Count; }
		if (LineH <= 0.0f) { return; }
		const int32 NewCount = FMath::Max(1, FMath::FloorToInt(Local.Y / LineH));
		if (NewCount != Count)
		{
			Count = NewCount;
			Text->SetRenderOpacity(1.0f);
			FString Column;
			for (int32 i = 0; i < Count; ++i) { if (i) { Column += TEXT("\n"); } Column += Base; }
			Text->SetText(FText::FromString(Column));
		}
		return;
	}
	if (Local.X <= 1.0f) { return; }
	// First pass from the font measure; after that the text's own rendered width is the truth.
	float CharW = Measure->Measure(FString::ChrN(20, RuleChar), Text->GetFont()).X / 20.0f;
	const int32 Chars = Base.Len() + FMath::Max(0, Count);
	const float Drawn = Text->GetDesiredSize().X;
	if (Count >= 0 && Chars > 0 && Drawn > 0.0f) { CharW = Drawn / Chars; }
	if (CharW <= 0.0f) { return; }
	// Half a character spare so rounding never pushes the last character past the edge.
	const int32 NewCount = FMath::Max(0, FMath::FloorToInt((Local.X - CharW * 0.5f) / CharW) - Base.Len());
	if (NewCount != Count)
	{
		Count = NewCount;
		Text->SetRenderOpacity(1.0f);
		Text->SetText(FText::FromString(Base + FString::ChrN(Count, RuleChar)));
	}
}
