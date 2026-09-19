#include "UI/CrtCursorWidget.h"
#include "UI/CrtStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/SizeBox.h"
#include "Core/BasePlayerController.h"
#include "Engine/World.h"

void UCrtCursorWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// A fixed footprint; everything is painted, nothing is hit-testable.
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Box"));
	// Twice the art's reach in each direction, because the art is anchored at the box's
	// CENTRE (the hotspot) rather than its corner -- see the header comment.
	Box->SetWidthOverride(56.0f);
	Box->SetHeightOverride(56.0f);
	Box->SetVisibility(ESlateVisibility::HitTestInvisible);
	WidgetTree->RootWidget = Box;
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UCrtCursorWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Clock += InDeltaTime;
}

int32 UCrtCursorWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	// Slate draws a software cursor centred on the real hotspot: it shifts the widget by half
	// its own size before painting. The art below is authored with the arrow's tip at local
	// (1,1), so painting it straight into the allotted geometry put the visible tip half a
	// widget up and to the left of the point clicks are actually measured at -- which is why
	// every hitbox on screen read as shifted, scroll bars worst of all because they are narrow.
	// Shifting the paint space by the box's centre puts the tip exactly on the hotspot.
	const FVector2f Hotspot = AllottedGeometry.GetLocalSize() * 0.5f - FVector2f(1.0f, 1.0f);
	const FPaintGeometry PG = AllottedGeometry.ToPaintGeometry(AllottedGeometry.GetLocalSize(), FSlateLayoutTransform(Hotspot));
	// Claude Assist armed: the pointer becomes a marking reticle, amber so it cannot be mistaken
	// for the arrow: bracketed corners around the hotspot, cross ticks, a slow pulse.
	const ABasePlayerController* PC = GetWorld() ? Cast<ABasePlayerController>(GetWorld()->GetFirstPlayerController()) : nullptr;
	if (PC && PC->IsClaudeAssistActive())
	{
		const float R = 11.0f + 1.5f * FMath::Sin(Clock * 3.0f), C = 1.0f, L = 5.0f;
		const FLinearColor Amber(1.0f, 0.72f, 0.2f, 1.0f), Glow(1.0f, 0.72f, 0.2f, 0.25f);
		auto Corner = [&](float Sx, float Sy) { const TArray<FVector2f> P = { {C + Sx * R, C + Sy * (R - L)}, {C + Sx * R, C + Sy * R}, {C + Sx * (R - L), C + Sy * R} }; FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 2, PG, P, ESlateDrawEffect::None, Amber, true, 1.5f); FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, PG, P, ESlateDrawEffect::None, Glow, true, 5.0f); };
		Corner(-1, -1); Corner(1, -1); Corner(-1, 1); Corner(1, 1);
		for (const TArray<FVector2f>& T : { TArray<FVector2f>{ {C - 4.0f, C}, {C + 4.0f, C} }, TArray<FVector2f>{ {C, C - 4.0f}, {C, C + 4.0f} } })
		{
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 2, PG, T, ESlateDrawEffect::None, Amber, true, 1.0f);
		}
		return LayerId + 3;
	}
	// The arrow: tip at (1,1), a classic pointer outline, filled dark so it
	// reads over any panel, rimmed green, with a soft green halo behind.
	const TArray<FVector2f> Arrow = { {1.0f, 1.0f}, {1.0f, 22.0f}, {6.5f, 17.5f}, {10.5f, 26.0f}, {14.0f, 24.5f}, {10.0f, 16.0f}, {17.0f, 16.0f}, {1.0f, 1.0f} };
	const float Pulse = 0.85f + 0.15f * FMath::Sin(Clock * 4.0f);
	// Halo: the same outline drawn thick and faint.
	FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, PG, Arrow, ESlateDrawEffect::None, FLinearColor(Crt::Green.R, Crt::Green.G, Crt::Green.B, 0.18f * Pulse), true, 6.0f);
	// Dark fill: approximate with a few thick dark strokes down the spine.
	const TArray<FVector2f> Spine1 = { {3.0f, 4.0f}, {3.0f, 18.0f} };
	const TArray<FVector2f> Spine2 = { {5.0f, 6.0f}, {10.0f, 13.0f} };
	const TArray<FVector2f> Spine3 = { {7.0f, 15.0f}, {11.0f, 23.0f} };
	for (const TArray<FVector2f>* S : { &Spine1, &Spine2, &Spine3 })
	{
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 2, PG, *S, ESlateDrawEffect::None, Crt::Background, true, 5.0f);
	}
	// The green rim.
	FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 3, PG, Arrow, ESlateDrawEffect::None, FLinearColor(Crt::Green.R, Crt::Green.G, Crt::Green.B, Pulse), true, 1.5f);
	// A scanline notch across the arrow, the way a CRT would slice it.
	const float Ny = 6.0f + FMath::Fmod(Clock * 9.0f, 16.0f);
	const TArray<FVector2f> Scan = { {1.0f, Ny}, {1.0f + Ny * 0.75f, Ny} };
	FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 4, PG, Scan, ESlateDrawEffect::None, FLinearColor(0, 0, 0, 0.5f), true, 1.0f);
	return LayerId + 5;
}
