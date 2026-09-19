#include "UI/AppearanceWidget.h"
#include "Core/BasePlayerController.h"
#include "UI/CrtStyle.h"
#include "UI/CrtRuleWidget.h"
#include "UI/CrtTabsWidget.h"
#include "UI/SheetSpec.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void UAppearanceRowBinding::OnClicked()
{
	if (Controller.IsValid()) { Controller->AppearanceStep(Row, Delta); }
	if (Widget.IsValid()) { Widget->Refresh(); }
}

void UAppearanceNameBinding::OnChanged(const FText&)
{
	if (Widget.IsValid()) { Widget->PushName(); }
}

void UAppearanceWidget::PushName()
{
	if (OwnerController && FirstBox && LastBox) { OwnerController->SetAppearanceName(FirstBox->GetText().ToString(), LastBox->GetText().ToString()); }
}

UEditableTextBox* UAppearanceWidget::MakeNameBox(const FString& Hint)
{
	UEditableTextBox* Box = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
	Box->SetHintText(FText::FromString(Hint));
	Box->WidgetStyle.SetFont(Crt::Fixed(FSheetSpec::Get().CaptionSize));
	Box->WidgetStyle.SetForegroundColor(Crt::Green);
	Box->WidgetStyle.SetFocusedForegroundColor(Crt::Green);
	Box->WidgetStyle.SetBackgroundImageNormal(FSlateColorBrush(Crt::PanelSolid));
	Box->WidgetStyle.SetBackgroundImageHovered(FSlateColorBrush(Crt::PanelSolid));
	Box->WidgetStyle.SetBackgroundImageFocused(FSlateColorBrush(Crt::Faint));
	Box->WidgetStyle.SetPadding(FMargin(10.0f, 4.0f));
	Box->WidgetStyle.TextStyle.SetSelectedBackgroundColor(FSlateColor(Crt::DimGreen));   // green-coded selection, not the default lavender
	Box->WidgetStyle.TextStyle.SetHighlightColor(Crt::Green);
	Box->SetJustification(ETextJustify::Center);
	if (!NameBinding) { NameBinding = NewObject<UAppearanceNameBinding>(this); NameBinding->Controller = OwnerController; NameBinding->Widget = this; }
	Box->OnTextChanged.AddDynamic(NameBinding, &UAppearanceNameBinding::OnChanged);
	return Box;
}

// Every row's label sits in a column of one width, so the < and > buttons beside them line
// up down the page instead of stepping in and out with the length of each word. Defined up
// here because the name boxes, built in NativeOnInitialized, are sized from it.
static const float LabelColumnWidth = 210.0f;

void UAppearanceWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);   // keys for Tab navigation
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	Panel->SetBrushColor(Crt::Panel);
	Panel->SetPadding(FMargin(34.0f, 26.0f));
	WidgetTree->RootWidget = Panel;
	UVerticalBox* Outer = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Outer"));
	Panel->SetContent(Outer);

	// Header across the top, like the character sheet.
	const FSheetSpec& Spec = FSheetSpec::Get();
	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Header->AddChildToHorizontalBox(Crt::FixedText(WidgetTree, Spec.HeaderLeft, Spec.RuleSize, Crt::Faint))->SetVerticalAlignment(VAlign_Center);
	UCrtTabsWidget* Tabs = UCrtTabsWidget::Make(GetOwningPlayer(), UCrtTabsWidget::StandardTabs(OwnerController ? OwnerController->GetPlayerDisplayName() : FString()), UCrtTabsWidget::TabAppearance, Spec.NameSize);
	Tabs->OnTab.BindUObject(this, &UAppearanceWidget::OnTab);
	Header->AddChildToHorizontalBox(Tabs)->SetVerticalAlignment(VAlign_Center);
	UHorizontalBoxSlot* HeadRule = Header->AddChildToHorizontalBox(UCrtRuleWidget::Make(GetOwningPlayer(), Spec.HeaderRight, Spec.RuleSize, Crt::Faint));
	HeadRule->SetSize(ESlateSizeRule::Fill); HeadRule->SetVerticalAlignment(VAlign_Center);
	if (!Spec.HeaderEnd.IsEmpty()) { Header->AddChildToHorizontalBox(Crt::FixedText(WidgetTree, Spec.HeaderEnd, Spec.RuleSize, Crt::Faint))->SetVerticalAlignment(VAlign_Center); }
	UButton* Close = Crt::Button(WidgetTree, TEXT("[ X ]"), Spec.NameSize, Crt::Green);
	Close->OnClicked.AddDynamic(this, &UAppearanceWidget::OnDone);
	Header->AddChildToHorizontalBox(Close)->SetPadding(FMargin(12, 0, 0, 0));
	Outer->AddChildToVerticalBox(Header)->SetPadding(FMargin(0, 0, 0, 12));

	// Two columns: the mirror on the left, the rows on the right.
	Body = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Body"));
	bMirrorFitted = false;
	Outer->AddChildToVerticalBox(Body)->SetSize(ESlateSizeRule::Fill);

	UVerticalBox* Left = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Left"));
	USizeBox* Fit = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Fit"));
	Fit->SetMinAspectRatio(0.625f); Fit->SetMaxAspectRatio(0.625f);   // the portrait's 5:8, laid out in one pass
	Feed = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Feed"));
	Feed->SetColorAndOpacity(FLinearColor::White);   // a clean picture, not a CRT feed
	Fit->AddChild(Feed);
	UVerticalBoxSlot* FitSlot = Left->AddChildToVerticalBox(Fit);
	FitSlot->SetSize(ESlateSizeRule::Fill);
	FitSlot->SetHorizontalAlignment(HAlign_Fill);
	FitSlot->SetVerticalAlignment(VAlign_Fill);
	// The same viewer as the character sheet's, at the same width: the spec's rule on the body's
	// width. The viewport is only the first guess; NativeTick sets it from the body once laid out.
	const float Scale = FMath::Max(0.01f, UWidgetLayoutLibrary::GetViewportScale(this));
	const float Inner = UWidgetLayoutLibrary::GetViewportSize(this).X / Scale - Spec.Panel.Left - Spec.Panel.Right - 68.0f;
	MirrorBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("MirrorBox"));
	MirrorBox->SetWidthOverride(Spec.MirrorWidthFor(Inner));
	MirrorBox->AddChild(Left);
	UHorizontalBoxSlot* LeftSlot = Body->AddChildToHorizontalBox(MirrorBox);
	LeftSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic)); LeftSlot->SetPadding(FMargin(0, 0, Spec.ColumnGap, 0)); LeftSlot->SetVerticalAlignment(VAlign_Fill);
	// Vertical rules between the columns, as on the sheet.
	auto Divide = [&]() { if (Spec.Divider.IsEmpty()) { return; } UHorizontalBoxSlot* D = Body->AddChildToHorizontalBox(UCrtRuleWidget::MakeVertical(GetOwningPlayer(), Spec.Divider, Spec.RuleSize, Crt::Faint)); D->SetVerticalAlignment(VAlign_Fill); D->SetPadding(FMargin(0, 0, Spec.ColumnGap, 0)); };
	Divide();

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	// A FIXED width for the rows, not a fill. Filling handed this column everything the mirror
	// and the actions left over -- most of the screen -- so the name boxes and the value text
	// stretched across it and every row read as a label at one edge and buttons at the other.
	// One width, chosen so the page fits the smallest panel the game supports (a 1920 x 1080
	// logical screen leaves 1452 inside the frame: mirror, rows, actions and their gaps total
	// about 1250 of it) and no wider, because a row that stretches reads as a label at one edge
	// and buttons at the other.
	const float RowsWidth = 480.0f;   // narrower: the mirror takes the body's height and the width it needs comes from here
	USizeBox* RowsBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RowsBox"));
	RowsBox->SetWidthOverride(RowsWidth);
	RowsBox->AddChild(Column);
	UHorizontalBoxSlot* RightSlot = Body->AddChildToHorizontalBox(RowsBox);
	RightSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic)); RightSlot->SetPadding(FMargin(0, 0, Spec.ColumnGap, 0));
	// The name: first and last, typed.
	UHorizontalBox* NameLine = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	NameLine->AddChildToHorizontalBox(LabelColumn(TEXT("NAME")))->SetVerticalAlignment(VAlign_Bottom);
	FirstBox = MakeNameBox(TEXT("first"));
	LastBox = MakeNameBox(TEXT("last"));
	// Each box says what it is: the hint text vanishes the moment anything is typed.
	auto Field = [&](const TCHAR* Caption, UEditableTextBox* Box)
	{
		UVerticalBox* V = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		V->AddChildToVerticalBox(Crt::FixedText(WidgetTree, Caption, Spec.RowSize, Crt::DimGreen))->SetPadding(FMargin(2, 0, 0, 2));
		V->AddChildToVerticalBox(Box);
		return V;
	};
	// Two compact boxes side by side, each as wide as a name and no wider, splitting what the
	// label leaves of the rows column. They used to fill, which at a wide viewport made each one
	// a metre long with a word centred in the middle of it.
	const float NameW = FMath::Floor((RowsWidth - LabelColumnWidth - 12.0f) * 0.5f);
	auto Sized = [&](UWidget* W) { USizeBox* B = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass()); B->SetWidthOverride(NameW); B->AddChild(W); return B; };
	UHorizontalBoxSlot* FirstSlot = NameLine->AddChildToHorizontalBox(Sized(Field(TEXT("FIRST"), FirstBox))); FirstSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic)); FirstSlot->SetPadding(FMargin(0, 0, 12, 0));
	UHorizontalBoxSlot* LastSlot = NameLine->AddChildToHorizontalBox(Sized(Field(TEXT("LAST"), LastBox))); LastSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	Column->AddChildToVerticalBox(NameLine)->SetPadding(FMargin(0, 0, 0, 14));

	AddRow(Column, TEXT("Body"), TEXT("Body"));
	AddRow(Column, TEXT("Skin"), TEXT("Skin"));
	AddRow(Column, TEXT("Hair"), TEXT("Hair"));
	AddRow(Column, TEXT("Beard"), TEXT("Beard"));
	AddRow(Column, TEXT("HairColor"), TEXT("Hair color"));
	AddRow(Column, TEXT("Nose"), TEXT("Nose"));
	AddRow(Column, TEXT("Brows"), TEXT("Brows"));
	AddRow(Column, TEXT("Stubble"), TEXT("Stubble"));
	AddRow(Column, TEXT("HeadStubble"), TEXT("Head stubble"));
	AddRow(Column, TEXT("Height"), TEXT("Height"));
	AddRow(Column, TEXT("Build"), TEXT("Build"));

	Divide();
	UVerticalBox* Actions = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Actions"));
	Actions->AddChildToVerticalBox(Crt::FixedText(WidgetTree, TEXT("--[ ACTIONS ]-------"), Spec.CaptionSize, Crt::DimGreen))->SetPadding(FMargin(0, 0, 0, 10));
	// Every button the same width -- the widest label's, with room -- so the column reads as a
	// panel of controls rather than three labels of three lengths. The button fills its box and
	// centres its text.
	const float ActionW = 300.0f;
	auto Action = [&](const TCHAR* Label, const FLinearColor& Colour)
	{
		UButton* B = Crt::Button(WidgetTree, Label, Spec.CaptionSize, Colour); FocusButtons.Add(B);
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetWidthOverride(ActionW);
		Box->AddChild(B);
		if (USizeBoxSlot* BS = Cast<USizeBoxSlot>(B->Slot)) { BS->SetHorizontalAlignment(HAlign_Fill); }
		Actions->AddChildToVerticalBox(Box)->SetPadding(FMargin(0, 4));
		return B;
	};
	// One width of label: the face is fixed-pitch, so padding every caption to the longest
	// makes the three brackets line up exactly, which no size box round a text-only button can.
	auto Padded = [](const TCHAR* Caption) { FString S(Caption); const int32 W = 15; while (S.Len() < W) { S = ((W - S.Len()) % 2 == 0) ? TEXT(" ") + S : S + TEXT(" "); } return FString::Printf(TEXT("[ %s ]"), *S); };
	UButton* Done = Action(*Padded(TEXT("ACCEPT LIKENESS")), Crt::Green);
	Done->OnClicked.AddDynamic(this, &UAppearanceWidget::OnDone);
	UButton* Random = Action(*Padded(TEXT("RANDOMISE")), Crt::DimGreen);
	Random->OnClicked.AddDynamic(this, &UAppearanceWidget::OnRandom);
	UButton* Default = Action(*Padded(TEXT("DEFAULT")), Crt::DimGreen);
	Default->OnClicked.AddDynamic(this, &UAppearanceWidget::OnDefault);
	UHorizontalBoxSlot* ActionsSlot = Body->AddChildToHorizontalBox(Actions);
	ActionsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic)); ActionsSlot->SetVerticalAlignment(VAlign_Top);
}

USizeBox* UAppearanceWidget::LabelColumn(const FString& Label)
{
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Box->SetWidthOverride(LabelColumnWidth);
	UTextBlock* T = Crt::FixedText(WidgetTree, Label, FSheetSpec::Get().CaptionSize, Crt::DimGreen);
	Box->AddChild(T);
	return Box;
}

void UAppearanceWidget::AddRow(UVerticalBox* Into, const FString& Row, const FString& Label)
{
	UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	const FSheetSpec& Spec = FSheetSpec::Get();
	UHorizontalBoxSlot* LabelSlot = Line->AddChildToHorizontalBox(LabelColumn(Label.ToUpper()));
	LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	LabelSlot->SetVerticalAlignment(VAlign_Center);

	for (int32 Delta : { -1, 1 })
	{
		if (Delta == 1)
		{
			UTextBlock* Value = Crt::FixedText(WidgetTree, TEXT("-"), Spec.CaptionSize, Crt::Green, ETextJustify::Center);
			UHorizontalBoxSlot* ValueSlot = Line->AddChildToHorizontalBox(Value);
			ValueSlot->SetSize(ESlateSizeRule::Fill);
			ValueSlot->SetVerticalAlignment(VAlign_Center);
			Values.Add(Row, Value);
		}
		UButton* B = Crt::Button(WidgetTree, Delta < 0 ? TEXT("<") : TEXT(">"), Spec.CaptionSize, Crt::Green);
		UAppearanceRowBinding* Binding = NewObject<UAppearanceRowBinding>(this);
		Binding->Controller = OwnerController; Binding->Widget = this; Binding->Row = Row; Binding->Delta = Delta;
		Bindings.Add(Binding);
		B->OnClicked.AddDynamic(Binding, &UAppearanceRowBinding::OnClicked);
		Line->AddChildToHorizontalBox(B)->SetVerticalAlignment(VAlign_Center);
	}
	Into->AddChildToVerticalBox(Line)->SetPadding(FMargin(0, 5));
	RowLines.Add(Row, Line);
	RowOrder.Add(Row);
}

FReply UAppearanceWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Tab)
	{
		SetFocusIndex(FocusIndex + (InKeyEvent.IsShiftDown() ? -1 : 1));
		return FReply::Handled();
	}
	const int32 RowAt = FocusIndex - 2;
	if (RowOrder.IsValidIndex(RowAt) && (Key == EKeys::Left || Key == EKeys::Right) && OwnerController)
	{
		OwnerController->AppearanceStep(RowOrder[RowAt], Key == EKeys::Left ? -1 : 1);
		Refresh();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void UAppearanceWidget::SetFocusIndex(int32 Index)
{
	const int32 N = FocusCount();
	if (N == 0) { return; }
	FocusIndex = ((Index % N) + N) % N;
	if (FocusIndex == 0 && FirstBox) { FirstBox->SetKeyboardFocus(); }
	else if (FocusIndex == 1 && LastBox) { LastBox->SetKeyboardFocus(); }
	else if (FocusIndex < 2 + RowOrder.Num()) { SetKeyboardFocus(); }
	else if (UButton* B = FocusButtons[FocusIndex - 2 - RowOrder.Num()]) { B->SetKeyboardFocus(); }
	ApplyFocusLook();
}

void UAppearanceWidget::ApplyFocusLook()
{
	static const FLinearColor Lit(0.75f, 1.0f, 0.8f, 1.0f);
	for (int32 i = 0; i < RowOrder.Num(); ++i)
	{
		if (TObjectPtr<UTextBlock>* V = Values.Find(RowOrder[i])) { if (*V) { (*V)->SetColorAndOpacity(FSlateColor(FocusIndex == 2 + i ? Lit : Crt::Green)); } }
	}
	for (int32 i = 0; i < FocusButtons.Num(); ++i)
	{
		if (UTextBlock* T = FocusButtons[i] ? Cast<UTextBlock>(FocusButtons[i]->GetContent()) : nullptr)
		{
			const bool bAccept = i == FocusButtons.Num() - 1;
			T->SetColorAndOpacity(FSlateColor(FocusIndex == 2 + RowOrder.Num() + i ? Lit : (bAccept ? Crt::Green : Crt::DimGreen)));
		}
	}
}

void UAppearanceWidget::SetFeed(UTextureRenderTarget2D* Target)
{
	if (!Feed || !Target) { return; }
	FSlateBrush Brush;
	Brush.SetResourceObject(Target);
	Brush.ImageSize = FVector2D(Target->SizeX, Target->SizeY);
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Feed->SetBrush(Brush);
}

FReply UAppearanceWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && Feed && Feed->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
	{
		bDragging = true;
		DragLast = InMouseEvent.GetScreenSpacePosition();
		DragTravel = 0.0f;
		return FReply::Handled().CaptureMouse(TakeWidget());
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UAppearanceWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDragging && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bDragging = false;
		// A click, not a drag: on the face it frames the head, anywhere else it goes back to the figure.
		if (DragTravel < 6.0f && OwnerController && Feed)
		{
			const FGeometry& G = Feed->GetCachedGeometry();
			const FVector2D Local = G.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
			const float Frac = G.GetLocalSize().Y > 0.0f ? Local.Y / G.GetLocalSize().Y : 1.0f;
			if (OwnerController->IsAppearanceHeadView()) { OwnerController->SetAppearanceHeadView(false); }
			else if (Frac < FSheetSpec::Get().FaceClickFraction) { OwnerController->SetAppearanceHeadView(true); }
		}
		return FReply::Handled().ReleaseMouseCapture();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UAppearanceWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDragging)
	{
		const FVector2D Now = InMouseEvent.GetScreenSpacePosition();
		const FVector2D Delta = Now - DragLast;
		DragTravel += Delta.Size();
		DragLast = Now;
		// Dragging right swings the camera round to the right; up looks down on them.
		if (OwnerController) { OwnerController->OrbitAppearanceCamera(-Delta.X * 0.4f, Delta.Y * 0.25f); }
		return FReply::Handled();
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

void UAppearanceWidget::OnDefault()
{
	if (OwnerController) { OwnerController->DefaultAppearance(); }
	Refresh();
}

void UAppearanceWidget::Refresh()
{
	if (OwnerController)
	{
		if (FirstBox && !FirstBox->HasKeyboardFocus()) { FirstBox->SetText(FText::FromString(OwnerController->GetAppearanceValue(TEXT("First")))); }
		if (LastBox && !LastBox->HasKeyboardFocus()) { LastBox->SetText(FText::FromString(OwnerController->GetAppearanceValue(TEXT("Last")))); }
	}
	if (!OwnerController) { return; }
	for (TPair<FString, TObjectPtr<UTextBlock>>& Pair : Values)
	{
		if (Pair.Value) { Pair.Value->SetText(FText::FromString(OwnerController->GetAppearanceValue(Pair.Key))); }
	}
	for (TPair<FString, TObjectPtr<UWidget>>& Pair : RowLines)
	{
		if (!Pair.Value) { continue; }
		const bool bOn = OwnerController->IsAppearanceRowEnabled(Pair.Key);
		Pair.Value->SetIsEnabled(bOn);
		Pair.Value->SetRenderOpacity(bOn ? 1.0f : 0.35f);
	}
	// Bindings were made before the controller was set: keep them pointed at it.
	for (UAppearanceRowBinding* B : Bindings) { if (B) { B->Controller = OwnerController; } }
}

void UAppearanceWidget::OnRandom()
{
	if (OwnerController) { OwnerController->RandomiseAppearance(); }
	Refresh();
}

void UAppearanceWidget::OnDone()
{
	if (OwnerController) { OwnerController->FinishAppearance(); }
}

void UAppearanceWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// The mirror's width from the body's real width, once it has one -- the same rule as the
	// sheet's, so the portrait does not change size between the two tabs.
	if (bMirrorFitted && Body && FMath::Abs(Body->GetCachedGeometry().GetLocalSize().X - FittedBodyW) > 1.0f) { bMirrorFitted = false; }   // a resize: fit again
	if (!bMirrorFitted && Body && MirrorBox)
	{
		const float BodyW = Body->GetCachedGeometry().GetLocalSize().X;
		if (BodyW > 50.0f)
		{
			// The portrait fills the body's HEIGHT: its width is that height at 5:8, capped by what
			// the rows and the actions leave of the body's width.
			const float BodyH = Body->GetCachedGeometry().GetLocalSize().Y;
			const float Gap = FSheetSpec::Get().ColumnGap;
			MirrorBox->SetWidthOverride(FMath::Max(240.0f, FMath::Min(BodyH * 0.625f, BodyW - 480.0f - 300.0f - 3.0f * Gap - 2.0f * (Gap * 0.5f + 12.0f))));
			bMirrorFitted = true;
			FittedBodyW = BodyW;
			SettleTicks = FMath::Max(SettleTicks, 2);
		}
	}
	Clock += InDeltaTime;
	// Held blank while the rules fit themselves, then the page this one replaced is dropped.
	if (SettleTicks > 0) { if (--SettleTicks == 0) { SetRenderOpacity(1.0f); if (OwnerController) { OwnerController->PageSettled(this); } } }
}

int32 UAppearanceWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	Crt::PaintFrame(OutDrawElements, AllottedGeometry, FVector2f::ZeroVector, FVector2f(AllottedGeometry.GetLocalSize()), LayerId + 1, Clock);
	// The mirror is drawn again above the frame's scanlines so the picture
	// stays clean: the panel is a CRT, the mirror is a window.
	if (Feed && Feed->GetBrush().GetResourceObject())
	{
		const FGeometry& FeedGeo = Feed->GetCachedGeometry();
		if (FeedGeo.GetLocalSize().X > 1.0f)
		{
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 2, FeedGeo.ToPaintGeometry(), &Feed->GetBrush(), ESlateDrawEffect::None, FLinearColor::White);
			// A thin light rim so it reads as a set-in pane.
			const FVector2f Sz(FeedGeo.GetLocalSize());
			const TArray<FVector2f> Rim = { {0.0f, 0.0f}, {Sz.X, 0.0f}, Sz, {0.0f, Sz.Y}, {0.0f, 0.0f} };
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 3, FeedGeo.ToPaintGeometry(), Rim, ESlateDrawEffect::None, Crt::DimGreen, true, 1.0f);
		}
	}
	return LayerId + 4;
}

void UAppearanceWidget::OnTab(int32 Tab) { if (OwnerController) { OwnerController->ShowConsolePage(Tab); } }
