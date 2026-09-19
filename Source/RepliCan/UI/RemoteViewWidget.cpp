#include "RemoteViewWidget.h"
#include "CrtStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/TextureRenderTarget2D.h"

void URemoteViewWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UBorder* Root = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Root"));
	Root->SetBrushColor(Crt::Panel);
	Root->SetPadding(FMargin(16.0f, 14.0f));
	WidgetTree->RootWidget = Root;

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Root->SetContent(Column);

	// Header: REMOTE on the left, the blinking live mark on the right.
	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Header->AddChildToHorizontalBox(Crt::SmallCaps(WidgetTree, TEXT("Remote"), 15, Crt::DimGreen))->SetSize(ESlateSizeRule::Fill);
	LiveText = Crt::Text(WidgetTree, TEXT("* LIVE"), 12, Crt::Amber);
	Header->AddChildToHorizontalBox(LiveText)->SetVerticalAlignment(VAlign_Bottom);
	Column->AddChildToVerticalBox(Header)->SetPadding(FMargin(0, 0, 0, 6));

	// The feed itself, tinted toward phosphor green; the frame is painted over it.
	Feed = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Feed"));
	Feed->SetColorAndOpacity(FLinearColor(0.62f, 1.0f, 0.72f, 1.0f));
	UVerticalBoxSlot* FeedSlot = Column->AddChildToVerticalBox(Feed);
	FeedSlot->SetSize(ESlateSizeRule::Fill);
	FeedSlot->SetHorizontalAlignment(HAlign_Fill);
	FeedSlot->SetVerticalAlignment(VAlign_Fill);

	CaptionText = Crt::Text(WidgetTree, TEXT(""), 12, Crt::DimGreen);
	Column->AddChildToVerticalBox(CaptionText)->SetPadding(FMargin(0, 6, 0, 0));
}

void URemoteViewWidget::SetFeed(UTextureRenderTarget2D* Target, const FString& Caption)
{
	if (Feed && Target)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(Target);
		Brush.ImageSize = FVector2D(Target->SizeX, Target->SizeY);
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Feed->SetBrush(Brush);
	}
	if (CaptionText) { CaptionText->SetText(FText::FromString(Caption)); }
}

void URemoteViewWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Clock += InDeltaTime;
	if (LiveText) { LiveText->SetRenderOpacity(FMath::Fmod(Clock, 1.2f) < 0.8f ? 1.0f : 0.25f); }
}

int32 URemoteViewWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	// Scanlines and a soft vignette over the feed only, then the panel frame.
	if (Feed)
	{
		const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
		const FGeometry& Fg = Feed->GetCachedGeometry();
		const FVector2f Pos = FVector2f(AllottedGeometry.AbsoluteToLocal(Fg.GetAbsolutePosition()));
		const FVector2f Size = FVector2f(Fg.GetLocalSize());
		if (White && Size.X > 4.0f && Size.Y > 4.0f)
		{
			for (float Y = Pos.Y; Y < Pos.Y + Size.Y; Y += 3.0f)
			{
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(FVector2f(Size.X, 1.0f), FSlateLayoutTransform(FVector2f(Pos.X, Y))), White, ESlateDrawEffect::None, FLinearColor(0, 0, 0, 0.28f));
			}
			// A rolling bar, the way a badly synced monitor drifts.
			const float Bar = Pos.Y + FMath::Fmod(Clock * 38.0f, Size.Y + 40.0f) - 20.0f;
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(FVector2f(Size.X, 18.0f), FSlateLayoutTransform(FVector2f(Pos.X, FMath::Clamp(Bar, Pos.Y, Pos.Y + Size.Y - 18.0f)))), White, ESlateDrawEffect::None, FLinearColor(0.5f, 1.0f, 0.6f, 0.05f));
			// Edge darkening.
			const FLinearColor Edge(0, 0, 0, 0.35f);
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(FVector2f(Size.X, 10.0f), FSlateLayoutTransform(Pos)), White, ESlateDrawEffect::None, Edge);
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(FVector2f(Size.X, 10.0f), FSlateLayoutTransform(FVector2f(Pos.X, Pos.Y + Size.Y - 10.0f))), White, ESlateDrawEffect::None, Edge);
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(FVector2f(10.0f, Size.Y), FSlateLayoutTransform(Pos)), White, ESlateDrawEffect::None, Edge);
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(FVector2f(10.0f, Size.Y), FSlateLayoutTransform(FVector2f(Pos.X + Size.X - 10.0f, Pos.Y))), White, ESlateDrawEffect::None, Edge);
		}
	}
	Crt::PaintFrame(OutDrawElements, AllottedGeometry, FVector2f::ZeroVector, FVector2f(AllottedGeometry.GetLocalSize()), LayerId + 2, Clock);
	return LayerId + 3;
}
