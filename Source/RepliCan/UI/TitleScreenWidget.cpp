#include "TitleScreenWidget.h"
#include "CrtStyle.h"
#include "RepliCanUserSettings.h"
#include "EnvironmentDirector.h"
#include "SettingsWidget.h"
#include "EngineUtils.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Misc/ConfigCacheIni.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

namespace
{
	const TCHAR* LoadingLogLines[] = {
		TEXT("> mounting memory core ............ ok"),
		TEXT("> checking limbs (4/4) ............ ok"),
		TEXT("> loading personality template .... ok"),
		TEXT("> verifying likeness .............. close enough"),
		TEXT("> suppressing existential doubt ... retrying"),
		TEXT("> calibrating optimism ............ ok"),
	};
}

void UTitleSlotBinding::OnClicked()
{
	if (Controller.IsValid()) { Controller->LoadGameSlot(Slot); }
}

// ---- Build -----------------------------------------------------------------

void UTitleScreenWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	UBorder* Root = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Root"));
	Root->SetBrushColor(Crt::Background);
	Root->SetPadding(FMargin(0.0f));
	WidgetTree->RootWidget = Root;

	Pages = WidgetTree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass(), TEXT("Pages"));
	Root->SetContent(Pages);

	// Page index == ETitlePage value.
	Pages->AddChild(BuildTitlePage());
	Pages->AddChild(BuildIntroPage());
	Pages->AddChild(BuildMenuPage());
	Pages->AddChild(BuildLoadPage());
	Pages->AddChild(BuildSettingsPage());
	Pages->AddChild(BuildLoadingPage());
	ShowPage(ETitlePage::Title);
}

UBorder* UTitleScreenWidget::Frame(UWidget* Content, float MaxWidth)
{
	// Centred column of at most MaxWidth, inset from the screen edges.
	UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Size->SetMaxDesiredWidth(MaxWidth);
	Size->SetContent(Content);
	UOverlaySlot* SizeSlot = Overlay->AddChildToOverlay(Size);
	SizeSlot->SetHorizontalAlignment(HAlign_Center);
	SizeSlot->SetVerticalAlignment(VAlign_Center);
	UBorder* Page = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Page->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
	Page->SetPadding(FMargin(64.0f, 48.0f));
	Page->SetContent(Overlay);
	return Page;
}

namespace
{
	// ProjectVersion from Config/DefaultGame.ini, shown as "v0.0.1".
	FString ProjectVersion()
	{
		FString Version;
		GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("ProjectVersion"), Version, GGameIni);
		return Version.IsEmpty() ? TEXT("v?.?.?") : (Version.StartsWith(TEXT("v")) ? Version : TEXT("v") + Version);
	}

	FString AsciiBar(int32 Width)
	{
		FString Bar = TEXT("[");
		for (int32 i = 0; i < Width; ++i) { Bar.AppendChar(TEXT('=')); }
		return Bar + TEXT("]");
	}
}

UWidget* UTitleScreenWidget::BuildLogo(int32 BodySize, bool bFrame)
{
	// REPLICAN with the R and the C an eighth larger, every letter on one
	// measured baseline (the bigger font has a deeper descender), the
	// version right under it, and an ASCII frame drawn around both. The
	// frame is text in the readout font, sized from measurements of the
	// word so it always encloses it.
	const int32 BigSize = BodySize + BodySize / 8;
	const int32 VersionSize = FMath::Max(12, BodySize / 5);
	const int32 FrameSize = FMath::Max(12, BodySize / 6);
	const FSlateFontInfo Small = Crt::Mono(BodySize), Big = Crt::Mono(BigSize), VerFont = Crt::Mono(VersionSize), FrameFont = Crt::Mono(FrameSize);

	float Lift = 0.0f, WordW = 0.0f, WordH = 0.0f, VerH = 0.0f, CharW = 10.0f, LineH = 16.0f;
	if (FSlateApplication::IsInitialized())
	{
		const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		Lift = FMath::Max(0.0f, static_cast<float>(FMath::Abs(Measure->GetBaseline(Big)) - FMath::Abs(Measure->GetBaseline(Small))));
		WordW = Measure->Measure(TEXT("R"), Big).X + Measure->Measure(TEXT("EPLI"), Small).X + Measure->Measure(TEXT("C"), Big).X + Measure->Measure(TEXT("AN"), Small).X;
		WordH = Measure->Measure(TEXT("R"), Big).Y;
		VerH = Measure->Measure(TEXT("v"), VerFont).Y;
		const FVector2D M = Measure->Measure(TEXT("M"), FrameFont);
		CharW = FMath::Max(1.0f, static_cast<float>(M.X));
		LineH = FMath::Max(1.0f, static_cast<float>(M.Y));
	}

	// The word.
	UHorizontalBox* Word = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	struct FPiece { const TCHAR* Str; bool bBig; };
	for (const FPiece& P : { FPiece{ TEXT("R"), true }, FPiece{ TEXT("EPLI"), false }, FPiece{ TEXT("C"), true }, FPiece{ TEXT("AN"), false } })
	{
		UTextBlock* T = Crt::Text(WidgetTree, P.Str, P.bBig ? BigSize : BodySize, Crt::Green);
		UHorizontalBoxSlot* S = Word->AddChildToHorizontalBox(T);
		S->SetVerticalAlignment(VAlign_Bottom);
		S->SetPadding(FMargin(0.0f, 0.0f, 0.0f, P.bBig ? 0.0f : Lift));
	}
	UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Stack->AddChildToVerticalBox(Word)->SetHorizontalAlignment(HAlign_Center);
	// Version flush with the word's right edge (the stack is exactly as wide as the word).
	UVerticalBoxSlot* VerSlot = Stack->AddChildToVerticalBox(Crt::Text(WidgetTree, ProjectVersion(), VersionSize, Crt::DimGreen, ETextJustify::Right));
	VerSlot->SetHorizontalAlignment(HAlign_Right);
	VerSlot->SetPadding(FMargin(0.0f, -BodySize * 0.08f, BodySize * 0.06f, 0.0f));
	if (!bFrame) { return Stack; }

	// The frame: wide enough for the word plus a margin of a few columns,
	// tall enough for word + version plus a row above and below.
	const int32 Inner = FMath::CeilToInt(WordW / CharW) + 8;
	const int32 Rows = FMath::CeilToInt((WordH + VerH) / LineH) + 2;
	auto Line = [Inner](const FString& Left, const FString& Right)
	{
		FString Row = TEXT("|") + Left;
		const int32 Pad = FMath::Max(0, Inner - Left.Len() - Right.Len());
		for (int32 i = 0; i < Pad; ++i) { Row.AppendChar(TEXT(' ')); }
		return Row + Right + TEXT("|");
	};
	FString Edge = TEXT("+");
	for (int32 i = 0; i < Inner; ++i) { Edge.AppendChar(TEXT('-')); }
	Edge += TEXT("+");
	FString FrameText = Edge + TEXT("\n");
	for (int32 r = 0; r < Rows; ++r)
	{
		// Subtle ornaments: corner ticks on the first rows, a faint pulse
		// trace along the bottom row.
		FString L, R;
		if (r == 0)             { L = TEXT(" .:"); R = TEXT(":. "); }
		else if (r == 1)        { L = TEXT(" '");  R = TEXT("' "); }
		else if (r == Rows - 1) { L = TEXT(" _/\\_/\\_/\\_"); R = TEXT(":. "); }
		FrameText += Line(L, R) + TEXT("\n");
	}
	FrameText += Edge;
	UTextBlock* FrameBlock = Crt::Text(WidgetTree, FrameText, FrameSize, Crt::Faint);
	FrameBlock->SetLineHeightPercentage(1.0f);

	UOverlay* Logo = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	UOverlaySlot* FrameSlot = Logo->AddChildToOverlay(FrameBlock);
	FrameSlot->SetHorizontalAlignment(HAlign_Center);
	FrameSlot->SetVerticalAlignment(VAlign_Center);
	UOverlaySlot* StackSlot = Logo->AddChildToOverlay(Stack);
	StackSlot->SetHorizontalAlignment(HAlign_Center);
	StackSlot->SetVerticalAlignment(VAlign_Center);
	return Logo;
}

UWidget* UTitleScreenWidget::BuildTitlePage()
{
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TitleColumn"));
	Column->AddChildToVerticalBox(BuildLogo(104, true))->SetHorizontalAlignment(HAlign_Center);

	// The reptech readout, static; the last line carries a blinking cursor.
	const FString Bar = AsciiBar(24);
	struct FLine { FString Text; FLinearColor Color; };
	const FLine Lines[] = {
		{ TEXT("reptech >  EMULSION PROGRESS ") + Bar + TEXT("...DONE"), Crt::Green },
		{ TEXT("reptech >  CELLULAR DECONFLICTION PROGRESS ") + Bar + TEXT("...DONE"), Crt::Green },
		{ TEXT("reptech >  warn::neuroburn.nb2400.logger:  139/477 sectors fails check 2321_CSF_NEUROMIN024"), Crt::Amber },
		{ TEXT("reptech >  EFFLUVIUM DECANTATION PROGRESS ") + Bar + TEXT("...DONE"), Crt::Green },
		{ TEXT("reptech >  REPLICATION COMPLETE.  11/11 COMPLETE.  49 INFO.  6 WARNING.  0 ERROR.  0 FATAL."), Crt::Green },
	};
	UVerticalBox* Readout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Readout"));
	for (const FLine& L : Lines)
	{
		Readout->AddChildToVerticalBox(Crt::Text(WidgetTree, L.Text, 17, L.Color))->SetPadding(FMargin(0, 3));
	}
	TitlePrompt = Crt::Text(WidgetTree, TEXT("reptech >  ADMINISTER VOLTAGE?  Y/n "), 17, Crt::Green);
	Readout->AddChildToVerticalBox(TitlePrompt)->SetPadding(FMargin(0, 3));
	UVerticalBoxSlot* ReadoutSlot = Column->AddChildToVerticalBox(Readout);
	ReadoutSlot->SetHorizontalAlignment(HAlign_Center);
	ReadoutSlot->SetPadding(FMargin(0, 34, 0, 0));
	return Frame(Column, 1500.0f);
}

UWidget* UTitleScreenWidget::BuildIntroPage()
{
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("IntroColumn"));
	IntroText = Crt::Text(WidgetTree, TEXT(""), 19);
	IntroText->SetAutoWrapText(true);
	IntroText->SetLineHeightPercentage(1.35f);
	Column->AddChildToVerticalBox(IntroText);
	IntroPrompt = Crt::Text(WidgetTree, TEXT("[ ANY KEY: continue ]   [ ESC: skip ]"), 13, Crt::DimGreen);
	Column->AddChildToVerticalBox(IntroPrompt)->SetPadding(FMargin(0, 36, 0, 0));
	UBorder* Page = Frame(Column, 980.0f);
	return Page;
}

UWidget* UTitleScreenWidget::BuildMenuPage()
{
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MenuColumn"));
	UVerticalBoxSlot* LogoSlot = Column->AddChildToVerticalBox(BuildLogo(64, false));
	LogoSlot->SetHorizontalAlignment(HAlign_Center);
	LogoSlot->SetPadding(FMargin(0, 0, 0, 40));
	// Labels are padded to one width so the "> <" selection marker doesn't
	// shift them; hovering or arrowing onto one selects it (with a clack).
	MenuLabels = { TEXT("NEW GAME"), TEXT("LOAD GAME"), TEXT("SETTINGS"), TEXT("QUIT") };
	UButton* NewGame = Crt::Button(WidgetTree, TEXT(""), 22);  NewGame->OnClicked.AddDynamic(this, &UTitleScreenWidget::OnNewGame);
	UButton* LoadGame = Crt::Button(WidgetTree, TEXT(""), 22); LoadGame->OnClicked.AddDynamic(this, &UTitleScreenWidget::OnLoadGame);
	UButton* Settings = Crt::Button(WidgetTree, TEXT(""), 22); Settings->OnClicked.AddDynamic(this, &UTitleScreenWidget::OnSettings);
	UButton* Quit = Crt::Button(WidgetTree, TEXT(""), 22);     Quit->OnClicked.AddDynamic(this, &UTitleScreenWidget::OnQuit);
	MenuButtons = { NewGame, LoadGame, Settings, Quit };
	for (int32 i = 0; i < MenuButtons.Num(); ++i)
	{
		UTitleMenuBinding* Binding = NewObject<UTitleMenuBinding>(this);
		Binding->Widget = this;
		Binding->Index = i;
		MenuBindings.Add(Binding);
		MenuButtons[i]->OnHovered.AddDynamic(Binding, &UTitleMenuBinding::OnHovered);
		UVerticalBoxSlot* S = Column->AddChildToVerticalBox(MenuButtons[i]);
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0, 5));
	}
	SetMenuSelection(0, false);
	return Frame(Column, 700.0f);
}

void UTitleMenuBinding::OnHovered()
{
	if (Widget.IsValid()) { Widget->SetMenuSelection(Index, true); }
}

void UTitleScreenWidget::SetMenuSelection(int32 Index, bool bClack)
{
	if (MenuButtons.Num() == 0) { return; }
	Index = (Index % MenuButtons.Num() + MenuButtons.Num()) % MenuButtons.Num();
	const bool bChanged = Index != MenuSelection;
	MenuSelection = Index;
	int32 Width = 0;
	for (const FString& L : MenuLabels) { Width = FMath::Max(Width, L.Len()); }
	for (int32 i = 0; i < MenuButtons.Num(); ++i)
	{
		FString Label = MenuLabels[i];
		while (Label.Len() < Width) { Label = (Label.Len() % 2 == 0) ? Label + TEXT(" ") : TEXT(" ") + Label; }
		Label = (i == MenuSelection) ? TEXT("> ") + Label + TEXT(" <") : TEXT("  ") + Label + TEXT("  ");
		if (UTextBlock* T = Crt::ButtonLabel(MenuButtons[i]))
		{
			T->SetText(FText::FromString(Label));
			T->SetColorAndOpacity(FSlateColor(i == MenuSelection ? Crt::Green : Crt::DimGreen));
		}
	}
	if (bChanged && bClack && OwnerController) { OwnerController->PlayUiClack(); }
}

void UTitleScreenWidget::MoveMenuSelection(int32 Delta)
{
	SetMenuSelection(MenuSelection + Delta, true);
}

void UTitleScreenWidget::ActivateMenuSelection()
{
	switch (MenuSelection)
	{
	case 0: OnNewGame(); break;
	case 1: OnLoadGame(); break;
	case 2: OnSettings(); break;
	case 3: OnQuit(); break;
	default: break;
	}
}

UWidget* UTitleScreenWidget::BuildLoadPage()
{
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LoadColumn"));
	Column->AddChildToVerticalBox(Crt::Text(WidgetTree, TEXT("LOAD GAME"), 34, Crt::Green, ETextJustify::Center))->SetPadding(FMargin(0, 0, 0, 24));
	SlotList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SlotList"));
	Column->AddChildToVerticalBox(SlotList);
	UButton* Back = Crt::Button(WidgetTree, TEXT("BACK"), 18, Crt::DimGreen);
	Back->OnClicked.AddDynamic(this, &UTitleScreenWidget::OnBack);
	UVerticalBoxSlot* BackSlot = Column->AddChildToVerticalBox(Back);
	BackSlot->SetHorizontalAlignment(HAlign_Center);
	BackSlot->SetPadding(FMargin(0, 28, 0, 0));
	return Frame(Column, 700.0f);
}

UWidget* UTitleScreenWidget::SettingsHeading(UVerticalBox* Into, const FString& Title)
{
	// "-- Display --" in false caps: the section name capitalised, the rest small.
	FString Cased = Title;
	if (Cased.Len() > 0) { Cased[0] = FChar::ToUpper(Cased[0]); }
	UHorizontalBox* T = Crt::SmallCaps(WidgetTree, FString::Printf(TEXT("-- %s --"), *Cased), 15, Crt::DimGreen);
	Into->AddChildToVerticalBox(T)->SetPadding(FMargin(0, 18, 0, 6));
	return T;
}

// One settings line: label on the left, its control/value on the right.
// Unwired (stub) rows are dimmed so it is obvious they do nothing yet.
UWidget* UTitleScreenWidget::SettingsRow(UVerticalBox* Into, const FString& Label, UWidget* Value, bool bWired)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UTextBlock* L = Crt::Text(WidgetTree, TEXT("  ") + Label, 17, bWired ? Crt::Green : Crt::DimGreen);
	UHorizontalBoxSlot* LS = Row->AddChildToHorizontalBox(L);
	LS->SetSize(ESlateSizeRule::Fill);
	LS->SetVerticalAlignment(VAlign_Center);
	UHorizontalBoxSlot* VS = Row->AddChildToHorizontalBox(Value);
	VS->SetHorizontalAlignment(HAlign_Right);
	VS->SetVerticalAlignment(VAlign_Center);
	Into->AddChildToVerticalBox(Row)->SetPadding(FMargin(0, 2));
	return Row;
}

UWidget* UTitleScreenWidget::BuildSettingsPage()
{
	// The page itself lives in USettingsWidget so the in-game menu shows exactly the same rows.
	// This screen supplies only the frame and the way back to its own menu.
	// CreateWidget only accepts a controller, a game instance or a world, not a UObject*, so the
	// player controller is passed when there is one and the world otherwise.
	USettingsWidget* Panel = GetOwningPlayer()
		? CreateWidget<USettingsWidget>(GetOwningPlayer(), USettingsWidget::StaticClass())
		: CreateWidget<USettingsWidget>(GetWorld(), USettingsWidget::StaticClass());
	if (!Panel) { return Frame(WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass()), 760.0f); }
	SettingsPanel = Panel;
	Panel->SetShowBack(true);
	Panel->OnClose.BindUObject(this, &UTitleScreenWidget::OnBack);
	Panel->Rebuild(/*bTitleContext=*/true);
	return Frame(Panel, 760.0f);
}

UWidget* UTitleScreenWidget::BuildLoadingPage()
{
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LoadingColumn"));
	LoadingStatus = Crt::Text(WidgetTree, TEXT("FABRICATING"), 30, Crt::Green, ETextJustify::Center);
	Column->AddChildToVerticalBox(LoadingStatus)->SetPadding(FMargin(0, 0, 0, 18));
	LoadingBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("LoadingBar"));
	LoadingBar->SetFillColorAndOpacity(Crt::Green);
	FProgressBarStyle BarStyle = LoadingBar->GetWidgetStyle();
	BarStyle.SetBackgroundImage(FSlateColorBrush(Crt::Faint));
	BarStyle.SetFillImage(FSlateColorBrush(FLinearColor::White));
	LoadingBar->SetWidgetStyle(BarStyle);
	LoadingBar->SetPercent(0.0f);
	USizeBox* BarBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	BarBox->SetHeightOverride(14.0f);
	BarBox->SetContent(LoadingBar);
	Column->AddChildToVerticalBox(BarBox)->SetPadding(FMargin(0, 0, 0, 24));
	LoadingLog = Crt::Text(WidgetTree, TEXT(""), 14, Crt::DimGreen);
	LoadingLog->SetLineHeightPercentage(1.3f);
	Column->AddChildToVerticalBox(LoadingLog);
	return Frame(Column, 720.0f);
}

// ---- Pages -----------------------------------------------------------------

void UTitleScreenWidget::ShowPage(ETitlePage Page)
{
	CurrentPage = Page;
	if (Pages) { Pages->SetActiveWidgetIndex(static_cast<int32>(Page)); }
	if (Page == ETitlePage::Loading) { LoadingLogShown = 0; LoadingLogTimer = 0.0f; if (LoadingLog) { LoadingLog->SetText(FText::GetEmpty()); } }
}

void UTitleScreenWidget::SetLoadSlots(const TArray<FString>& Slots)
{
	if (!SlotList) { return; }
	SlotList->ClearChildren();
	SlotBindings.Reset();
	if (Slots.Num() == 0)
	{
		UVerticalBoxSlot* S = SlotList->AddChildToVerticalBox(Crt::Text(WidgetTree, TEXT("-- no saved games --"), 16, Crt::DimGreen, ETextJustify::Center));
		S->SetHorizontalAlignment(HAlign_Center);
		return;
	}
	for (const FString& SlotName : Slots)
	{
		UButton* B = Crt::Button(WidgetTree, SlotName.ToUpper(), 18);
		UTitleSlotBinding* Binding = NewObject<UTitleSlotBinding>(this);
		Binding->Controller = OwnerController;
		Binding->Slot = SlotName;
		SlotBindings.Add(Binding);
		B->OnClicked.AddDynamic(Binding, &UTitleSlotBinding::OnClicked);
		UVerticalBoxSlot* S = SlotList->AddChildToVerticalBox(B);
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0, 4));
	}
}

void UTitleScreenWidget::RefreshSettings()
{
	if (SettingsPanel) { SettingsPanel->RefreshRows(); }
}

void UTitleScreenWidget::SetLoadingProgress(float Fraction, const FString& StatusOrEmpty)
{
	if (LoadingBar) { LoadingBar->SetPercent(FMath::Clamp(Fraction, 0.0f, 1.0f)); }
	if (LoadingStatus && !StatusOrEmpty.IsEmpty()) { LoadingStatus->SetText(FText::FromString(StatusOrEmpty)); }
}

// ---- Buttons ---------------------------------------------------------------

void UTitleScreenWidget::OnNewGame()          { if (OwnerController) { OwnerController->StartNewGame(); } }
void UTitleScreenWidget::OnLoadGame()         { if (OwnerController) { OwnerController->ShowLoadGame(); } }
void UTitleScreenWidget::OnSettings()         { if (OwnerController) { OwnerController->ShowSettings(); } }
void UTitleScreenWidget::OnQuit()             { if (OwnerController) { OwnerController->QuitGame(); } }
void UTitleScreenWidget::OnBack()             { if (OwnerController) { OwnerController->ShowMainMenu(); } }
void UTitleScreenWidget::OnToggleFullscreen() { if (OwnerController) { OwnerController->ToggleFullscreen(); } }
void UTitleScreenWidget::OnToggleAlwaysIntro(){ if (OwnerController) { OwnerController->ToggleAlwaysShowIntro(); } }
void UTitleScreenWidget::OnReplayIntro()      { if (OwnerController) { OwnerController->ResetIntroSeen(); } }

// ---- Intro typewriter ------------------------------------------------------

void UTitleScreenWidget::StartIntro(const TArray<FIntroLine>& Lines, float CharsPerSecond)
{
	IntroLines = Lines;
	IntroCharsPerSecond = FMath::Max(1.0f, CharsPerSecond);
	IntroLineIndex = 0;
	IntroCharIndex = 0;
	IntroAccumulator = 0.0f;
	IntroPauseLeft = 0.0f;
	IntroRevealed.Reset();
	bIntroFinished = false;
	if (IntroText) { IntroText->SetText(FText::GetEmpty()); }
}

void UTitleScreenWidget::RevealIntro()
{
	if (bIntroFinished) { return; }
	IntroRevealed.Reset();
	for (const FIntroLine& L : IntroLines) { IntroRevealed += L.Text + TEXT("\n"); }
	bIntroFinished = true;
	if (IntroText) { IntroText->SetText(FText::FromString(IntroRevealed)); }
}

void UTitleScreenWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Clock += InDeltaTime;
	const bool bBlinkOn = FMath::Fmod(Clock, 1.0f) < 0.6f;

	if (CurrentPage == ETitlePage::Title && TitlePrompt)
	{
		TitlePrompt->SetText(FText::FromString(FString(TEXT("reptech >  ADMINISTER VOLTAGE?  Y/n ")) + (bBlinkOn ? TEXT("_") : TEXT(" "))));
	}
	else if (CurrentPage == ETitlePage::Intro && IntroText)
	{
		if (!bIntroFinished)
		{
			if (IntroPauseLeft > 0.0f)
			{
				IntroPauseLeft -= InDeltaTime;
			}
			else
			{
				IntroAccumulator += InDeltaTime * IntroCharsPerSecond;
				while (IntroAccumulator >= 1.0f && !bIntroFinished)
				{
					IntroAccumulator -= 1.0f;
					if (!IntroLines.IsValidIndex(IntroLineIndex)) { bIntroFinished = true; break; }
					const FIntroLine& Line = IntroLines[IntroLineIndex];
					if (IntroCharIndex < Line.Text.Len())
					{
						IntroRevealed.AppendChar(Line.Text[IntroCharIndex++]);
					}
					else
					{
						IntroRevealed += TEXT("\n");
						IntroPauseLeft = Line.PauseAfter;
						IntroCharIndex = 0;
						++IntroLineIndex;
						if (IntroLineIndex >= IntroLines.Num()) { bIntroFinished = true; }
						break;   // one line ends per tick at most; the pause follows
					}
				}
			}
		}
		// Block cursor at the end of what has been typed so far.
		IntroText->SetText(FText::FromString(IntroRevealed + (bBlinkOn ? TEXT("█") : TEXT(" "))));
		if (IntroPrompt) { IntroPrompt->SetRenderOpacity(bIntroFinished ? (bBlinkOn ? 1.0f : 0.35f) : 0.6f); }
	}
	else if (CurrentPage == ETitlePage::Loading && LoadingLog)
	{
		LoadingLogTimer += InDeltaTime;
		const int32 Want = FMath::Min(static_cast<int32>(UE_ARRAY_COUNT(LoadingLogLines)), static_cast<int32>(LoadingLogTimer / 0.28f));
		if (Want != LoadingLogShown)
		{
			LoadingLogShown = Want;
			FString Log;
			for (int32 i = 0; i < Want; ++i) { Log += LoadingLogLines[i]; Log += TEXT("\n"); }
			LoadingLog->SetText(FText::FromString(Log));
		}
	}
}

// ---- CRT overlay -----------------------------------------------------------

int32 UTitleScreenWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
	if (!White) { return LayerId; }
	const FVector2f Size = FVector2f(AllottedGeometry.GetLocalSize());
	const int32 Layer = LayerId + 1;

	// Scanlines: a dark one-unit line every third row, flickering a touch.
	const float Flicker = 0.32f + 0.03f * FMath::Sin(Clock * 41.0f) + 0.02f * FMath::Sin(Clock * 7.3f);
	const FLinearColor LineColor(0.0f, 0.0f, 0.0f, Flicker);
	for (float Y = 0.0f; Y < Size.Y; Y += 3.0f)
	{
		FSlateDrawElement::MakeBox(OutDrawElements, Layer, AllottedGeometry.ToPaintGeometry(FVector2f(Size.X, 1.0f), FSlateLayoutTransform(FVector2f(0.0f, Y))), White, ESlateDrawEffect::None, LineColor);
	}

	// Vignette: bands fading in toward each edge.
	const int32 Bands = 22;
	const float Depth = FMath::Min(Size.X, Size.Y) * 0.22f;
	for (int32 i = 0; i < Bands; ++i)
	{
		const float T = static_cast<float>(i) / Bands;             // 0 at the edge
		const float Alpha = 0.55f * FMath::Square(1.0f - T) / Bands * 2.0f;
		const float Thick = Depth / Bands;
		const FLinearColor C(0.0f, 0.0f, 0.0f, Alpha);
		const float Off = i * Thick;
		FSlateDrawElement::MakeBox(OutDrawElements, Layer + 1, AllottedGeometry.ToPaintGeometry(FVector2f(Size.X, Thick), FSlateLayoutTransform(FVector2f(0.0f, Off))), White, ESlateDrawEffect::None, C);
		FSlateDrawElement::MakeBox(OutDrawElements, Layer + 1, AllottedGeometry.ToPaintGeometry(FVector2f(Size.X, Thick), FSlateLayoutTransform(FVector2f(0.0f, Size.Y - Off - Thick))), White, ESlateDrawEffect::None, C);
		FSlateDrawElement::MakeBox(OutDrawElements, Layer + 1, AllottedGeometry.ToPaintGeometry(FVector2f(Thick, Size.Y), FSlateLayoutTransform(FVector2f(Off, 0.0f))), White, ESlateDrawEffect::None, C);
		FSlateDrawElement::MakeBox(OutDrawElements, Layer + 1, AllottedGeometry.ToPaintGeometry(FVector2f(Thick, Size.Y), FSlateLayoutTransform(FVector2f(Size.X - Off - Thick, 0.0f))), White, ESlateDrawEffect::None, C);
	}
	return Layer + 2;
}

// ---- Environment switches --------------------------------------------------
// Each writes the setting, saves it so it survives the session, and rebuilds that one feature
// on the live director when there is one. On the title screen there is no director and nothing
// to rebuild, so the setting simply takes effect when the facility loads.
static AEnvironmentDirector* LiveDirector(UObject* Ctx)
{
	UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
	if (!W) { return nullptr; }
	for (TActorIterator<AEnvironmentDirector> It(W); It; ++It) { return *It; }
	return nullptr;
}

void UTitleScreenWidget::OnToggleEnvGrade()
{
	if (URepliCanUserSettings* S = URepliCanUserSettings::Get()) { S->bEnvGrade = !S->bEnvGrade; S->SaveSettings(); }
	if (AEnvironmentDirector* D = LiveDirector(this)) { D->ApplyEnvironment(); }
	RefreshSettings();
}
void UTitleScreenWidget::OnToggleEnvAO()
{
	if (URepliCanUserSettings* S = URepliCanUserSettings::Get()) { S->bEnvAmbientOcclusion = !S->bEnvAmbientOcclusion; S->SaveSettings(); }
	if (AEnvironmentDirector* D = LiveDirector(this)) { D->ApplyEnvironment(); }
	RefreshSettings();
}
void UTitleScreenWidget::OnToggleEnvFog()
{
	if (URepliCanUserSettings* S = URepliCanUserSettings::Get()) { S->bEnvFog = !S->bEnvFog; S->SaveSettings(); }
	if (AEnvironmentDirector* D = LiveDirector(this)) { D->ApplyEnvironment(); }
	RefreshSettings();
}
void UTitleScreenWidget::OnToggleEnvVolumetric()
{
	if (URepliCanUserSettings* S = URepliCanUserSettings::Get()) { S->bEnvVolumetricFog = !S->bEnvVolumetricFog; S->SaveSettings(); }
	if (AEnvironmentDirector* D = LiveDirector(this)) { D->ApplyEnvironment(); }
	RefreshSettings();
}
void UTitleScreenWidget::OnToggleEnvDust()
{
	if (URepliCanUserSettings* S = URepliCanUserSettings::Get()) { S->bEnvDust = !S->bEnvDust; S->SaveSettings(); }
	if (AEnvironmentDirector* D = LiveDirector(this)) { D->ApplyEnvironment(); }
	RefreshSettings();
}
void UTitleScreenWidget::OnToggleEnvFlicker()
{
	if (URepliCanUserSettings* S = URepliCanUserSettings::Get()) { S->bEnvLampFlicker = !S->bEnvLampFlicker; S->SaveSettings(); }
	RefreshSettings();
}
void UTitleScreenWidget::OnStepFogDensity()
{
	if (URepliCanUserSettings* S = URepliCanUserSettings::Get()) { S->EnvFogDensity = (S->EnvFogDensity + 1) % 5; S->SaveSettings(); }
	if (AEnvironmentDirector* D = LiveDirector(this)) { D->ApplyEnvironment(); }
	RefreshSettings();
}
void UTitleScreenWidget::OnStepDustDensity()
{
	if (URepliCanUserSettings* S = URepliCanUserSettings::Get()) { S->EnvDustDensity = (S->EnvDustDensity + 1) % 5; S->SaveSettings(); }
	if (AEnvironmentDirector* D = LiveDirector(this)) { D->ApplyEnvironment(); }
	RefreshSettings();
}

void UTitleScreenWidget::OnToggleEnvFixedExposure()
{
	if (URepliCanUserSettings* S = URepliCanUserSettings::Get()) { S->bEnvFixedExposure = !S->bEnvFixedExposure; S->SaveSettings(); }
	if (AEnvironmentDirector* D = LiveDirector(this)) { D->ApplyEnvironment(); }
	RefreshSettings();
}
void UTitleScreenWidget::OnStepEnvExposure()
{
	if (URepliCanUserSettings* S = URepliCanUserSettings::Get()) { S->EnvExposure = (S->EnvExposure + 1) % 5; S->SaveSettings(); }
	if (AEnvironmentDirector* D = LiveDirector(this)) { D->ApplyEnvironment(); }
	RefreshSettings();
}

void UTitleScreenWidget::OnStepFootstepVolume()
{
	// Nothing to rebuild: the character reads this every time it plays a step.
	if (URepliCanUserSettings* S = URepliCanUserSettings::Get()) { S->FootstepVolumeStep = (S->FootstepVolumeStep + 1) % 5; S->SaveSettings(); }
	RefreshSettings();
}
