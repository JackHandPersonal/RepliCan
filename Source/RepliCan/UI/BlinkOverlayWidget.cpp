#include "BlinkOverlayWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

void UBlinkOverlayWidget::Configure(const FBlinkParams& InParams)
{
	// Reconfiguring mid-scene keeps the lids and veil where they are and
	// moves on from there (a fresh overlay starts shut and dark).
	const bool bFresh = !IsInViewport();
	DarkFrom = bFresh ? InParams.DarkStart : GetDarkness();
	Params = InParams;
	Clock = 0.0f;
	if (bFresh)
	{
		Lid = ELid::Closed;
		PhaseLength = PhaseLeft = FMath::FRandRange(0.6f, 1.2f) * Params.ClosedSeconds;
		Openness = 0.0f;
		Blinks = 0;
	}
	else if (Params.OpenMax <= 0.001f && Lid != ELid::Closed)
	{
		// Asked to shut: close from wherever the lids are, now.
		Lid = ELid::Closing; OpenFrom = Openness; PhaseLength = PhaseLeft = 0.7f;
	}
	else if (Params.LiftSeconds > 0.0f && Lid == ELid::Closed)
	{
		PhaseLeft = 0.0f;   // the lift starts on the next tick
	}
	else
	{
		Lid = ELid::Opening;
		OpenFrom = Openness;
		OpenTarget = FMath::FRandRange(Params.OpenMin, Params.OpenMax);
		PhaseLength = PhaseLeft = 0.9f;
	}
	if (!WidgetTree->RootWidget)
	{
		// A transparent, hit-test-invisible root so clicks fall through.
		UBorder* Root = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Root"));
		Root->SetBrushColor(FLinearColor(0, 0, 0, 0));
		Root->SetVisibility(ESlateVisibility::HitTestInvisible);
		WidgetTree->RootWidget = Root;
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UBlinkOverlayWidget::Jolt(float Seconds)
{
	JoltTotal = JoltLeft = FMath::Max(0.1f, Seconds);
	JoltClock = 0.0f;
	// Eyes wide open at once; the next Configure eases them from here.
	Lid = ELid::Open;
	Openness = OpenFrom = OpenTarget = 1.0f;
	PhaseLength = PhaseLeft = 1.5f;
}

float UBlinkOverlayWidget::GetDarkness() const
{
	const float T = Params.DarkSeconds > 0.0f ? FMath::Clamp(Clock / Params.DarkSeconds, 0.0f, 1.0f) : 1.0f;
	return FMath::Lerp(DarkFrom, Params.DarkEnd, T);
}

void UBlinkOverlayWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Clock += InDeltaTime;
	PhaseLeft -= InDeltaTime;
	const float Progress = PhaseLength > 0.0f ? 1.0f - FMath::Clamp(PhaseLeft / PhaseLength, 0.0f, 1.0f) : 1.0f;
	switch (Lid)
	{
	case ELid::Closed:
		Openness = 0.0f;
		if (PhaseLeft <= 0.0f)
		{
			Lid = ELid::Opening;
			OpenFrom = 0.0f;
			OpenTarget = FMath::FRandRange(Params.OpenMin, Params.OpenMax);
			PhaseLength = PhaseLeft = Params.LiftSeconds > 0.0f ? Params.LiftSeconds : FMath::FRandRange(0.7f, 1.3f);   // a slow, heavy lift unless told to snap
		}
		break;
	case ELid::Opening:
		Openness = FMath::Lerp(OpenFrom, OpenTarget, FMath::InterpEaseOut(0.0f, 1.0f, Progress, 2.2f));
		if (PhaseLeft <= 0.0f)
		{
			Lid = ELid::Open;
			PhaseLength = PhaseLeft = FMath::FRandRange(0.6f, 1.4f) * Params.OpenSeconds;
		}
		break;
	case ELid::Open:
		// A little sag while open, as if fighting to keep them up.
		Openness = OpenTarget * (1.0f - 0.12f * Progress) + 0.02f * FMath::Sin(Clock * 3.1f);
		if (PhaseLeft <= 0.0f)
		{
			Lid = ELid::Closing;
			OpenFrom = Openness;
			PhaseLength = PhaseLeft = FMath::FRandRange(0.35f, 0.7f);
		}
		break;
	case ELid::Closing:
		Openness = FMath::Lerp(OpenFrom, 0.0f, FMath::InterpEaseIn(0.0f, 1.0f, Progress, 1.8f));
		if (PhaseLeft <= 0.0f)
		{
			Lid = ELid::Closed;
			++Blinks;
			PhaseLength = PhaseLeft = FMath::FRandRange(0.6f, 1.4f) * Params.ClosedSeconds;
		}
		break;
	}
	if (JoltLeft > 0.0f)
	{
		JoltLeft -= InDeltaTime;
		JoltClock += InDeltaTime;
		// Lid flutter: the muscles firing, strongest at the start.
		const float Decay = JoltLeft / JoltTotal;
		Openness = FMath::Max(Openness, 0.85f * Decay) + 0.16f * Decay * FMath::Sin(JoltClock * 58.0f);
	}
	Openness = FMath::Clamp(Openness, 0.0f, 1.0f);
}

int32 UBlinkOverlayWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
	if (!White) { return LayerId; }
	const FVector2f Size = FVector2f(AllottedGeometry.GetLocalSize());
	const int32 Layer = LayerId + 1;

	// The darkness veil over everything.
	FSlateDrawElement::MakeBox(OutDrawElements, Layer, AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform()), White, ESlateDrawEffect::None, FLinearColor(0, 0, 0, GetDarkness()));

	// The jolt: a green-white flash that burns off fast, then torn scanlines
	// flickering across the view while the spasm rings down.
	if (JoltLeft > 0.0f && JoltTotal > 0.0f)
	{
		const float Decay = JoltLeft / JoltTotal;
		const float Flash = 0.9f * FMath::Exp(-JoltClock * 9.0f);
		FSlateDrawElement::MakeBox(OutDrawElements, Layer + 3, AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform()), White, ESlateDrawEffect::None, FLinearColor(0.78f, 1.0f, 0.82f, Flash));
		const int32 Tears = 7;
		for (int32 i = 0; i < Tears; ++i)
		{
			const float Phase = FMath::Frac(FMath::Sin(JoltClock * (17.0f + i * 3.7f) + i * 12.9898f) * 43758.5453f);
			const float Y = Phase * Size.Y;
			const float H = 2.0f + 6.0f * FMath::Frac(Phase * 7.3f);
			const float Shift = (FMath::Frac(Phase * 13.1f) - 0.5f) * 80.0f * Decay;
			FSlateDrawElement::MakeBox(OutDrawElements, Layer + 3, AllottedGeometry.ToPaintGeometry(FVector2f(Size.X, H), FSlateLayoutTransform(FVector2f(Shift, Y))), White, ESlateDrawEffect::None, FLinearColor(0.6f, 1.0f, 0.7f, 0.55f * Decay));
		}
	}

	// Eyelids: an almond-shaped opening built from vertical strips. The
	// opening's height at each column follows an ellipse; each lid edge
	// gets a few translucent bands so it reads soft rather than cut.
	const int32 Strips = 64;
	const float StripW = Size.X / Strips;
	const float HalfH = Size.Y * 0.5f;
	const float MaxOpen = Openness * HalfH * 1.15f;   // wide open still leaves a hint of lid
	for (int32 i = 0; i < Strips; ++i)
	{
		const float X0 = i * StripW;
		const float Cx = (i + 0.5f) / Strips * 2.0f - 1.0f;          // -1 .. 1 across
		const float Ell = FMath::Sqrt(FMath::Max(0.0f, 1.0f - Cx * Cx));
		const float OpenHere = MaxOpen * (0.25f + 0.75f * Ell);      // almond, not a slit
		const float TopLid = HalfH - OpenHere;                        // lid bottom edge, from the top
		const float BotLid = HalfH + OpenHere;                        // lid top edge
		FSlateDrawElement::MakeBox(OutDrawElements, Layer + 1, AllottedGeometry.ToPaintGeometry(FVector2f(StripW + 1.0f, FMath::Max(0.0f, TopLid)), FSlateLayoutTransform(FVector2f(X0, 0.0f))), White, ESlateDrawEffect::None, FLinearColor::Black);
		FSlateDrawElement::MakeBox(OutDrawElements, Layer + 1, AllottedGeometry.ToPaintGeometry(FVector2f(StripW + 1.0f, FMath::Max(0.0f, Size.Y - BotLid)), FSlateLayoutTransform(FVector2f(X0, BotLid))), White, ESlateDrawEffect::None, FLinearColor::Black);
		// Soft edges.
		const int32 Bands = 6;
		const float BandH = FMath::Max(2.0f, OpenHere * 0.18f);
		for (int32 b = 0; b < Bands; ++b)
		{
			const FLinearColor C(0, 0, 0, 0.55f * (1.0f - static_cast<float>(b) / Bands));
			FSlateDrawElement::MakeBox(OutDrawElements, Layer + 1, AllottedGeometry.ToPaintGeometry(FVector2f(StripW + 1.0f, BandH / Bands + 1.0f), FSlateLayoutTransform(FVector2f(X0, TopLid + b * BandH / Bands))), White, ESlateDrawEffect::None, C);
			FSlateDrawElement::MakeBox(OutDrawElements, Layer + 1, AllottedGeometry.ToPaintGeometry(FVector2f(StripW + 1.0f, BandH / Bands + 1.0f), FSlateLayoutTransform(FVector2f(X0, BotLid - (b + 1) * BandH / Bands))), White, ESlateDrawEffect::None, C);
		}
	}
	return Layer + 2;
}
