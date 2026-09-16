// The game's look for menus and screens: phosphor-green monospaced text on
// a near-black ground, like an old terminal. Every code-built widget pulls
// its colors, font and button style from here so the theme changes in one
// place. PaintFrame draws the panel trim: chamfered outline, corner plates,
// a top rail with a seam, rivets and vent ticks -- the same vocabulary as
// the Sci-Fi Space wall panels in the bay, kept thin and dim so it reads
// as station architecture without competing with the text.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Components/TextBlock.h"
#include "Components/ScrollBox.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Fonts/CompositeFont.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "UObject/StrongObjectPtr.h"

namespace Crt
{
	inline const FLinearColor Green(0.32f, 1.0f, 0.45f, 1.0f);
	inline const FLinearColor DimGreen(0.16f, 0.55f, 0.24f, 1.0f);
	inline const FLinearColor Faint(0.06f, 0.22f, 0.09f, 1.0f);
	inline const FLinearColor Background(0.004f, 0.016f, 0.007f, 1.0f);
	inline const FLinearColor Panel(0.006f, 0.022f, 0.010f, 0.93f);      // translucent panel ground
	inline const FLinearColor PanelSolid(0.006f, 0.022f, 0.010f, 1.0f);
	inline const FLinearColor Amber(1.0f, 0.72f, 0.25f, 1.0f);   // warnings
	inline const FLinearColor Ink(0.02f, 0.05f, 0.03f, 1.0f);    // dark text on the few light controls

	// The terminal font: VT323 (SIL OFL), imported as the
	// /Game/RepliCan/UI/F_Crt_Face font face by Tools/import_crt_font.py;
	// Slate's bundled DroidSansMono when the face is missing.
	inline UFont* Font()
	{
		static TStrongObjectPtr<UFont> Cached;
		static bool bTried = false;
		if (!Cached.IsValid() && !bTried)
		{
			bTried = true;
			// A runtime composite font wrapped around the imported face; no
			// Font asset needed (the Typeface structs aren't scriptable).
			if (UFontFace* Face = LoadObject<UFontFace>(nullptr, TEXT("/Game/RepliCan/UI/F_Crt_Face.F_Crt_Face")))
			{
				UFont* F = NewObject<UFont>(GetTransientPackage(), TEXT("F_Crt_Runtime"));
				F->FontCacheType = EFontCacheType::Runtime;
				FTypefaceEntry Entry(FName(TEXT("Regular")));
				Entry.Font = FFontData(Face);
				F->CompositeFont.DefaultTypeface.Fonts.Add(Entry);
				F->LegacyFontSize = 24;
				Cached.Reset(F);
				UE_LOG(LogTemp, Log, TEXT("Crt: terminal font from %s"), *Face->GetPathName());
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("Crt: /Game/RepliCan/UI/F_Crt_Face missing, using Slate Mono"));
			}
		}
		return Cached.Get();
	}

	// Every UI draws in the old-terminal face (VT323, imported as /Game/RepliCan/UI/F_Crt_Face:
	// fixed pitch, so rules, leaders and columns line up); the engine's mono is only the fallback.
	inline FSlateFontInfo Mono(int32 Size)
	{
		if (UFont* F = Font())
		{
			// VT323 sits small in its em box; scale up so sizes read the same.
			return FSlateFontInfo(F, FMath::RoundToInt(Size * 1.25f));
		}
		return FCoreStyle::GetDefaultFontStyle("Mono", Size);
	}

	inline UTextBlock* Text(UWidgetTree* Tree, const FString& Str, int32 Size, const FLinearColor& Color = Green, ETextJustify::Type Justify = ETextJustify::Left)
	{
		UTextBlock* T = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		T->SetText(FText::FromString(Str));
		T->SetFont(Mono(Size));
		T->SetColorAndOpacity(FSlateColor(Color));
		T->SetJustification(Justify);
		return T;
	}

	// A true fixed-pitch face (the engine's mono) for screens that draw with ASCII rules and
	// dotted leaders; the CRT face above is proportional.
	inline FSlateFontInfo Fixed(int32 Size) { return Mono(Size); }
	inline UTextBlock* FixedText(UWidgetTree* Tree, const FString& Str, int32 Size, const FLinearColor& Color = Green, ETextJustify::Type Justify = ETextJustify::Left)
	{
		UTextBlock* T = Text(Tree, Str, Size, Color, Justify);
		T->SetFont(Fixed(Size));
		return T;
	}


	// A scroll box whose bar can actually be grabbed: the thumb is wide enough to hit, always
	// on show (a bar that appears only while scrolling cannot be clicked), and in the panel's
	// colours. Every scrolling list in the game uses this so they drag the same way.
	inline UScrollBox* ScrollBox(UWidgetTree* Tree, float Thickness = 12.0f)
	{
		UScrollBox* Box = Tree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		Box->SetOrientation(Orient_Vertical);
		Box->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
		Box->SetAnimateWheelScrolling(true);
		Box->SetAlwaysShowScrollbar(true);
		Box->SetScrollBarVisibility(ESlateVisibility::Visible);
		Box->SetScrollbarThickness(FVector2D(Thickness, Thickness));
		// Inboard of the panel's own trim. The frame paints an inner hairline and a column of
		// vent ticks a dozen pixels in from the right edge, which read as a scroll bar: aiming at
		// those missed the real one by about its own width. This clears them.
		// No inset: the bar and its grab area coincide. The "hit area is off to the left"
		// symptom this once compensated for was the software cursor's tip being drawn half a
		// widget up and left of the real hotspot -- fixed in UCrtCursorWidget, not here.
		Box->SetScrollbarPadding(FMargin(0.0f));
		Box->SetAllowRightClickDragScrolling(true);
		Box->SetAllowOverscroll(false);   // a list stops at its end; the rubber-band read as a glitch on a CRT panel
		// No shadow at the edges when the content runs past them: on a dark panel it read as a
		// smear over the first and last rows.
		{
			FScrollBoxStyle Plain = Box->GetWidgetStyle();
			Plain.SetTopShadowBrush(FSlateNoResource()); Plain.SetBottomShadowBrush(FSlateNoResource());
			Plain.SetLeftShadowBrush(FSlateNoResource()); Plain.SetRightShadowBrush(FSlateNoResource());
			Box->SetWidgetStyle(Plain);
		}
		const FSlateColorBrush Clear(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
		const FSlateColorBrush Track(FLinearColor(Faint.R, Faint.G, Faint.B, 0.5f));
		FScrollBarStyle Bar;
		Bar.SetVerticalBackgroundImage(Track);
		Bar.SetHorizontalBackgroundImage(Track);
		Bar.SetVerticalTopSlotImage(Clear);
		Bar.SetVerticalBottomSlotImage(Clear);
		Bar.SetHorizontalTopSlotImage(Clear);
		Bar.SetHorizontalBottomSlotImage(Clear);
		Bar.SetNormalThumbImage(FSlateColorBrush(DimGreen));
		Bar.SetHoveredThumbImage(FSlateColorBrush(Green));
		Bar.SetDraggedThumbImage(FSlateColorBrush(Green));
		Bar.SetThickness(Thickness);
		Box->SetWidgetBarStyle(Bar);
		return Box;
	}

	// Transparent at rest, faint green when hovered, brighter when pressed;
	// the label is the only thing you see until the mouse finds it.
	inline FButtonStyle ButtonStyle()
	{
		FButtonStyle Style;
		Style.SetNormal(FSlateColorBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f)));
		Style.SetHovered(FSlateColorBrush(Faint));
		Style.SetPressed(FSlateColorBrush(DimGreen));
		Style.SetDisabled(FSlateColorBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f)));
		Style.SetNormalPadding(FMargin(0.0f));
		Style.SetPressedPadding(FMargin(0.0f));
		return Style;
	}

	// A boxed variant for buttons that must read as buttons at rest (tool panels).
	inline FButtonStyle BoxedButtonStyle()
	{
		FButtonStyle Style = ButtonStyle();
		Style.SetNormal(FSlateColorBrush(FLinearColor(0.03f, 0.12f, 0.05f, 0.9f)));
		return Style;
	}

	inline UButton* Button(UWidgetTree* Tree, const FString& Label, int32 Size = 18, const FLinearColor& Color = Green)
	{
		UButton* B = Tree->ConstructWidget<UButton>(UButton::StaticClass());
		B->SetStyle(ButtonStyle());
		UTextBlock* T = Text(Tree, Label, Size, Color, ETextJustify::Center);
		B->AddChild(T);
		if (UButtonSlot* TextSlot = Cast<UButtonSlot>(T->Slot))
		{
			TextSlot->SetPadding(FMargin(18.0f, 6.0f));
			TextSlot->SetHorizontalAlignment(HAlign_Center);
		}
		return B;
	}

	// "False caps": the house style for names and headers. Capitals stay at
	// Size, lower-case letters become capitals at about three quarters of it,
	// all sitting on one baseline. Built from runs of text blocks in a
	// horizontal box (one text block cannot mix sizes).
	inline void FillSmallCaps(UWidgetTree* Tree, UHorizontalBox* Box, const FString& Str, int32 Size, const FLinearColor& Color = Green)
	{
		if (!Tree || !Box) { return; }
		Box->ClearChildren();
		const int32 SmallSize = FMath::Max(8, FMath::RoundToInt(Size * 0.74f));
		float Lift = 0.0f;
		if (FSlateApplication::IsInitialized())
		{
			const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
			Lift = FMath::Max(0.0f, static_cast<float>(FMath::Abs(Measure->GetBaseline(Mono(Size))) - FMath::Abs(Measure->GetBaseline(Mono(SmallSize)))));
		}
		FString Run; bool bRunSmall = false;
		auto Flush = [&]()
		{
			if (Run.IsEmpty()) { return; }
			UTextBlock* T = Text(Tree, bRunSmall ? Run.ToUpper() : Run, bRunSmall ? SmallSize : Size, Color);
			UHorizontalBoxSlot* TextSlot = Box->AddChildToHorizontalBox(T);
			TextSlot->SetVerticalAlignment(VAlign_Bottom);
			if (bRunSmall) { TextSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, Lift)); }
			Run.Reset();
		};
		for (int32 i = 0; i < Str.Len(); ++i)
		{
			const TCHAR C = Str[i];
			const bool bSmall = FChar::IsLower(C);
			if (!Run.IsEmpty() && bSmall != bRunSmall) { Flush(); }
			bRunSmall = bSmall;
			Run.AppendChar(C);
		}
		Flush();
	}

	inline UHorizontalBox* SmallCaps(UWidgetTree* Tree, const FString& Str, int32 Size, const FLinearColor& Color = Green)
	{
		UHorizontalBox* Box = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		FillSmallCaps(Tree, Box, Str, Size, Color);
		return Box;
	}

	inline UTextBlock* ButtonLabel(UButton* B)
	{
		return B ? Cast<UTextBlock>(B->GetChildAt(0)) : nullptr;
	}

	// ---- Panel trim ---------------------------------------------------------
	// Draws the station-panel frame around a rectangle (Pos/Size in the paint
	// geometry's local space). Alpha scales everything for faded panels.
	// SoftRect, when given, is a region the scanlines are dialled down over -- a live render
	// inside a CRT panel wants the frame around it without the panel's raster laid across the
	// picture itself. In panel-local coordinates, the same space as Pos and Size.
	inline void PaintFrame(FSlateWindowElementList& Out, const FGeometry& Geo, const FVector2f& Pos, const FVector2f& Size, int32 Layer, float Clock, float Alpha = 1.0f, const FBox2f* SoftRect = nullptr, float SoftScale = 0.18f)
	{
		const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
		if (!White || Size.X < 40.0f || Size.Y < 40.0f) { return; }
		auto Box = [&](float X, float Y, float W, float H, const FLinearColor& C)
		{
			FSlateDrawElement::MakeBox(Out, Layer, Geo.ToPaintGeometry(FVector2f(W, H), FSlateLayoutTransform(FVector2f(X, Y))), White, ESlateDrawEffect::None, C);
		};
		auto Tint = [&](float A) { return FLinearColor(Green.R, Green.G, Green.B, A * Alpha); };
		const float X = Pos.X, Y = Pos.Y, W = Size.X, H = Size.Y;
		// Faint scanlines over the panel ground, broken around SoftRect so a render inside the
		// panel keeps only a trace of the raster instead of the full set.
		const FLinearColor Scan(0, 0, 0, 0.13f * Alpha);
		const FLinearColor ScanSoft(0, 0, 0, 0.13f * Alpha * SoftScale);
		for (float Ly = Y + 1.0f; Ly < Y + H; Ly += 3.0f)
		{
			if (SoftRect && Ly >= SoftRect->Min.Y && Ly <= SoftRect->Max.Y)
			{
				const float L0 = FMath::Max(X, SoftRect->Min.X), L1 = FMath::Min(X + W, SoftRect->Max.X);
				if (L0 > X) { Box(X, Ly, L0 - X, 1.0f, Scan); }
				if (L1 > L0) { Box(L0, Ly, L1 - L0, 1.0f, ScanSoft); }
				if (X + W > L1) { Box(L1, Ly, X + W - L1, 1.0f, Scan); }
			}
			else { Box(X, Ly, W, 1.0f, Scan); }
		}
		// Outer outline with chamfered corners, inset a little.
		const float In = 5.0f, Ch = 13.0f;
		const float X0 = X + In, Y0 = Y + In, X1 = X + W - In, Y1 = Y + H - In;
		TArray<FVector2f> Pts = { {X0 + Ch, Y0}, {X1 - Ch, Y0}, {X1, Y0 + Ch}, {X1, Y1 - Ch}, {X1 - Ch, Y1}, {X0 + Ch, Y1}, {X0, Y1 - Ch}, {X0, Y0 + Ch}, {X0 + Ch, Y0} };
		FSlateDrawElement::MakeLines(Out, Layer, Geo.ToPaintGeometry(), Pts, ESlateDrawEffect::None, Tint(0.45f), true, 1.0f);
		// Inner hairline.
		const float In2 = 11.0f;
		TArray<FVector2f> Inner = { {X + In2, Y + In2}, {X + W - In2, Y + In2}, {X + W - In2, Y + H - In2}, {X + In2, Y + H - In2}, {X + In2, Y + In2} };
		FSlateDrawElement::MakeLines(Out, Layer, Geo.ToPaintGeometry(), Inner, ESlateDrawEffect::None, Tint(0.18f), true, 1.0f);
		// Corner plates: L brackets, the way the wall panels are braced.
		const float L = 26.0f, T = 2.0f;
		const FLinearColor Plate = Tint(0.75f);
		Box(X + 1, Y + 1, L, T, Plate);         Box(X + 1, Y + 1, T, L, Plate);
		Box(X + W - 1 - L, Y + 1, L, T, Plate); Box(X + W - 1 - T, Y + 1, T, L, Plate);
		Box(X + 1, Y + H - 1 - T, L, T, Plate); Box(X + 1, Y + H - 1 - L, T, L, Plate);
		Box(X + W - 1 - L, Y + H - 1 - T, L, T, Plate); Box(X + W - 1 - T, Y + H - 1 - L, T, L, Plate);
		// Top rail with a seam gap in the middle, like a panel join.
		const float RailY = Y0 + 3.0f, RailX0 = X0 + Ch + 6.0f, RailX1 = X1 - Ch - 6.0f, Gap = FMath::Min(70.0f, (RailX1 - RailX0) * 0.2f);
		const float Mid = (RailX0 + RailX1) * 0.5f;
		Box(RailX0, RailY, Mid - Gap * 0.5f - RailX0, 2.0f, Tint(0.28f));
		Box(Mid + Gap * 0.5f, RailY, RailX1 - Mid - Gap * 0.5f, 2.0f, Tint(0.28f));
		// Rivets along top and bottom.
		for (float Rx = RailX0 + 10.0f; Rx < RailX1 - 6.0f; Rx += 46.0f)
		{
			Box(Rx, Y0 + 8.0f, 3.0f, 3.0f, Tint(0.35f));
			Box(Rx, Y1 - 11.0f, 3.0f, 3.0f, Tint(0.35f));
		}
		// Vent ticks low on both sides.
		const float VentTop = Y + H * 0.62f, VentBot = Y1 - Ch - 4.0f;
		for (float Vy = VentTop; Vy < VentBot; Vy += 9.0f)
		{
			Box(X0 + 4.0f, Vy, 9.0f, 1.0f, Tint(0.2f));
			Box(X1 - 13.0f, Vy, 9.0f, 1.0f, Tint(0.2f));
		}
		// A slow phosphor flicker on the outline, barely there.
		const float Fl = 0.06f + 0.04f * FMath::Sin(Clock * 5.3f);
		FSlateDrawElement::MakeLines(Out, Layer, Geo.ToPaintGeometry(), Pts, ESlateDrawEffect::None, Tint(Fl), true, 3.0f);
	}

	// Frame around a child widget of the painting widget.
	inline void PaintFrameAround(FSlateWindowElementList& Out, const FGeometry& Geo, const UWidget* Child, int32 Layer, float Clock, float Alpha = 1.0f)
	{
		if (!Child) { return; }
		const FGeometry& Cg = Child->GetCachedGeometry();
		const FVector2f Pos = FVector2f(Geo.AbsoluteToLocal(Cg.GetAbsolutePosition()));
		PaintFrame(Out, Geo, Pos, FVector2f(Cg.GetLocalSize()), Layer, Clock, Alpha);
	}
}
