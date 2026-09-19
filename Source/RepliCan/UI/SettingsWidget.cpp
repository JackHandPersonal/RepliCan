#include "UI/SettingsWidget.h"
#include "UI/CrtStyle.h"
#include "UI/CrtRuleWidget.h"
#include "UI/SheetSpec.h"
#include "Components/Border.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Core/RepliCanUserSettings.h"
#include "World/EnvironmentDirector.h"
#include "Core/InputBindings.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "EngineUtils.h"
#include "Engine/World.h"

static const int32 SettingsRowSize = 15;   // dense: this game is not afraid of a full page

void USettingsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// The same box the character sheet sits in -- the panel colour and inset, a header rule with
	// the title set into it and the [ X ] at its end -- with the rows in THREE COLUMNS across the
	// whole width, and a scroll only if a column ever outgrows the box. Rebuild dresses or
	// undresses it for the context.
	Root = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SettingsRoot"));
	WidgetTree->RootWidget = Root;
	UVerticalBox* Outer = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SettingsOuter"));
	Root->SetContent(Outer);
	HeaderBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SettingsHeader"));
	Outer->AddChildToVerticalBox(HeaderBox)->SetPadding(FMargin(0, 0, 0, 10));
	TitleBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SettingsTitle"));
	Outer->AddChildToVerticalBox(TitleBox);
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("SettingsScroll"));
	Scroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	UVerticalBoxSlot* ScrollSlot = Outer->AddChildToVerticalBox(Scroll);
	ScrollSlot->SetSize(ESlateSizeRule::Fill);
	Columns = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SettingsColumns"));
	Scroll->AddChild(Columns);
	if (UScrollBoxSlot* CS = Cast<UScrollBoxSlot>(Columns->Slot)) { CS->SetHorizontalAlignment(HAlign_Fill); }
	for (int32 i = 0; i < 3; ++i)
	{
		UVerticalBox* C = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), *FString::Printf(TEXT("SettingsColumn%d"), i));
		UHorizontalBoxSlot* S = Columns->AddChildToHorizontalBox(C);
		S->SetSize(ESlateSizeRule::Fill);
		S->SetPadding(FMargin(i == 0 ? 0.0f : 22.0f, 0, 0, 0));
		S->SetVerticalAlignment(VAlign_Top);
		Cols.Add(C);
	}
	Column = Cols[0];
}

UVerticalBox* USettingsWidget::Col(int32 Index) const
{
	return Cols.IsValidIndex(Index) ? Cols[Index].Get() : Column.Get();
}

UWidget* USettingsWidget::Heading(UVerticalBox* Into, const FString& Title)
{
	// "-- Display --" in false caps: the section name capitalised, the rest small.
	FString Cased = Title;
	if (Cased.Len() > 0) { Cased[0] = FChar::ToUpper(Cased[0]); }
	UHorizontalBox* T = Crt::SmallCaps(WidgetTree, FString::Printf(TEXT("-- %s --"), *Cased), 14, Crt::DimGreen);
	Into->AddChildToVerticalBox(T)->SetPadding(FMargin(0, Into->GetChildrenCount() == 0 ? 0 : 9, 0, 3));
	return T;
}

// One settings line: label on the left, its control on the right. Rows that do nothing yet are
// dimmed, so it is obvious at a glance which of them are real.
UWidget* USettingsWidget::Row(UVerticalBox* Into, const FString& Label, UWidget* Value, bool bWired)
{
	UHorizontalBox* R = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UTextBlock* L = Crt::Text(WidgetTree, Label, SettingsRowSize, bWired ? Crt::Green : Crt::DimGreen);
	UHorizontalBoxSlot* LS = R->AddChildToHorizontalBox(L);
	LS->SetSize(ESlateSizeRule::Fill);
	LS->SetVerticalAlignment(VAlign_Center);
	UHorizontalBoxSlot* VS = R->AddChildToHorizontalBox(Value);
	VS->SetHorizontalAlignment(HAlign_Right);
	VS->SetVerticalAlignment(VAlign_Center);
	Into->AddChildToVerticalBox(R)->SetPadding(FMargin(0, 1));
	return R;
}

UTextBlock* USettingsWidget::Stub(const TCHAR* Value)
{
	return Crt::Text(WidgetTree, Value, SettingsRowSize, Crt::DimGreen);
}

void USettingsWidget::Rebuild(bool bTitleContext)
{
	if (Cols.Num() == 0) { return; }
	bTitle = bTitleContext;
	for (const TObjectPtr<UVerticalBox>& C : Cols) { if (C) { C->ClearChildren(); } }
	if (TitleBox) { TitleBox->ClearChildren(); }
	KeyRowLabels.Reset(); ButtonActions.Reset(); KeyHint = nullptr; CapturingAction = NAME_None;
	// Dressed as the character sheet's dialog in the pause context; bare inside the title screen's own frame.
	if (Root) { Root->SetBrushColor(bShowBack ? Crt::Panel : FLinearColor::Transparent); Root->SetPadding(bShowBack ? FMargin(34.0f, 26.0f) : FMargin(0.0f)); }
	if (HeaderBox)
	{
		HeaderBox->ClearChildren();
		HeaderBox->SetVisibility(bShowBack ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (bShowBack)
		{
			const FSheetSpec& S = FSheetSpec::Get();
			HeaderBox->AddChildToHorizontalBox(Crt::FixedText(WidgetTree, S.HeaderLeft, S.RuleSize, Crt::Faint))->SetVerticalAlignment(VAlign_Center);
			HeaderBox->AddChildToHorizontalBox(Crt::FixedText(WidgetTree, bKeysPage ? TEXT("SETTINGS / KEYS") : TEXT("SETTINGS"), S.NameSize, Crt::Green))->SetVerticalAlignment(VAlign_Center);
			UHorizontalBoxSlot* RuleSlot = HeaderBox->AddChildToHorizontalBox(UCrtRuleWidget::Make(GetOwningPlayer(), S.HeaderRight, S.RuleSize, Crt::Faint));
			RuleSlot->SetSize(ESlateSizeRule::Fill); RuleSlot->SetVerticalAlignment(VAlign_Center);
			if (!S.HeaderEnd.IsEmpty()) { HeaderBox->AddChildToHorizontalBox(Crt::FixedText(WidgetTree, S.HeaderEnd, S.RuleSize, Crt::Faint))->SetVerticalAlignment(VAlign_Center); }
			UButton* Close = Crt::Button(WidgetTree, TEXT("[ X ]"), S.NameSize, Crt::Green);
			Close->OnClicked.AddDynamic(this, &USettingsWidget::OnBack);
			HeaderBox->AddChildToHorizontalBox(Close)->SetPadding(FMargin(12, 0, 0, 0));
		}
	}
	if (bKeysPage) { BuildKeysPage(); return; }
	BuildMainPage();
}

void USettingsWidget::BuildMainPage()
{
	FullscreenLabel = nullptr; AlwaysIntroLabel = nullptr; FootstepVolumeLabel = nullptr;
	EnvGradeLabel = nullptr; EnvAOLabel = nullptr; EnvFogLabel = nullptr; EnvVolumetricLabel = nullptr;
	EnvDustLabel = nullptr; EnvFlickerLabel = nullptr; EnvFixedExposureLabel = nullptr;
	PestsLabel = nullptr; PestRateLabel = nullptr; AOStrengthLabel = nullptr;
	EnvExposureLabel = nullptr; EnvFogDensityLabel = nullptr; EnvDustDensityLabel = nullptr;

	if (!bShowBack && TitleBox) { TitleBox->AddChildToVerticalBox(Crt::Text(WidgetTree, TEXT("SETTINGS"), 30, Crt::Green, ETextJustify::Center))->SetPadding(FMargin(0, 0, 0, 6)); }   // in the pause context the header rule carries the title

	// Column one: the machine and the hands. Column two: the environment's switches. Column three: its levels.
	UVerticalBox* A = Col(0); UVerticalBox* B = Col(1); UVerticalBox* C = Col(2);
	auto Toggle = [&](UVerticalBox* Into, const TCHAR* Label, const TCHAR* Initial, void (USettingsWidget::*Fn)(), TObjectPtr<UTextBlock>& Out)
	{
		UButton* Btn = Crt::Button(WidgetTree, Initial, SettingsRowSize);
		FScriptDelegate D; D.BindUFunction(this, *FString());   // placeholder, replaced below
		(void)D;
		Out = Crt::ButtonLabel(Btn);
		Row(Into, Label, Btn, true);
		return Btn;
	};
	(void)Toggle;

	Heading(A, TEXT("display"));
	{
		UButton* Fullscreen = Crt::Button(WidgetTree, TEXT("< WINDOWED >"), SettingsRowSize);
		Fullscreen->OnClicked.AddDynamic(this, &USettingsWidget::OnToggleFullscreen);
		FullscreenLabel = Crt::ButtonLabel(Fullscreen);
		Row(A, TEXT("DISPLAY MODE"), Fullscreen, true);
	}
	Row(A, TEXT("RESOLUTION"), Stub(TEXT("< 1920 x 1080 >")), false);
	Row(A, TEXT("VSYNC"), Stub(TEXT("ON")), false);
	Row(A, TEXT("CRT SCANLINES"), Stub(TEXT("ON")), false);

	Heading(A, TEXT("audio"));
	Row(A, TEXT("MASTER VOLUME"), Stub(TEXT("[========  ]  80%")), false);
	Row(A, TEXT("VOICE VOLUME"), Stub(TEXT("[==========] 100%")), false);
	Row(A, TEXT("EFFECTS VOLUME"), Stub(TEXT("[========  ]  80%")), false);
	Row(A, TEXT("MUSIC VOLUME"), Stub(TEXT("[======    ]  60%")), false);
	{
		UButton* Btn = Crt::Button(WidgetTree, TEXT("< [==  ] >"), SettingsRowSize);
		Btn->OnClicked.AddDynamic(this, &USettingsWidget::OnStepFootstepVolume);
		FootstepVolumeLabel = Crt::ButtonLabel(Btn);
		Row(A, TEXT("FOOTSTEP VOLUME"), Btn, true);
	}

	Heading(A, TEXT("controls"));
	Row(A, TEXT("MOUSE SENSITIVITY"), Stub(TEXT("< 1.0 >")), false);
	Row(A, TEXT("INVERT LOOK"), Stub(TEXT("OFF")), false);
	{
		UButton* Btn = Crt::Button(WidgetTree, TEXT("[ EDIT ]"), SettingsRowSize);
		Btn->OnClicked.AddDynamic(this, &USettingsWidget::OnOpenKeys);
		Row(A, TEXT("KEY BINDINGS"), Btn, true);
	}
	if (bTitle)
	{
		// Only meaningful before a game is running.
		Heading(A, TEXT("game"));
		UButton* AlwaysIntro = Crt::Button(WidgetTree, TEXT("OFF"), SettingsRowSize);
		AlwaysIntro->OnClicked.AddDynamic(this, &USettingsWidget::OnToggleAlwaysIntro);
		AlwaysIntroLabel = Crt::ButtonLabel(AlwaysIntro);
		Row(A, TEXT("INTRO ON EVERY START"), AlwaysIntro, true);
	}
	A->AddChildToVerticalBox(Crt::Text(WidgetTree, TEXT("dim entries are not wired yet"), 12, Crt::Faint))->SetPadding(FMargin(0, 12, 0, 0));

	Heading(B, TEXT("environment"));
#define SETTINGS_ROW_TOGGLE(Into, Label, Initial, Fn, Out) { UButton* Btn = Crt::Button(WidgetTree, TEXT(Initial), SettingsRowSize); Btn->OnClicked.AddDynamic(this, &USettingsWidget::Fn); Out = Crt::ButtonLabel(Btn); Row(Into, TEXT(Label), Btn, true); }
	SETTINGS_ROW_TOGGLE(B, "COLOUR GRADE", "ON", OnToggleEnvGrade, EnvGradeLabel)
	SETTINGS_ROW_TOGGLE(B, "AMBIENT OCCLUSION", "ON", OnToggleEnvAO, EnvAOLabel)
	SETTINGS_ROW_TOGGLE(B, "ATMOSPHERE", "ON", OnToggleEnvFog, EnvFogLabel)
	SETTINGS_ROW_TOGGLE(B, "LIGHT SHAFTS", "ON", OnToggleEnvVolumetric, EnvVolumetricLabel)
	SETTINGS_ROW_TOGGLE(B, "DUST IN THE AIR", "ON", OnToggleEnvDust, EnvDustLabel)
	SETTINGS_ROW_TOGGLE(B, "LAMP FLICKER", "ON", OnToggleEnvFlicker, EnvFlickerLabel)
	SETTINGS_ROW_TOGGLE(B, "VERMIN", "ON", OnTogglePests, PestsLabel)
	SETTINGS_ROW_TOGGLE(B, "FIXED EXPOSURE", "OFF", OnToggleEnvFixedExposure, EnvFixedExposureLabel)

	Heading(C, TEXT("levels"));
	SETTINGS_ROW_TOGGLE(C, "CORNER SHADING", "< [====   ] >", OnStepAOStrength, AOStrengthLabel)
	SETTINGS_ROW_TOGGLE(C, "VERMIN FREQUENCY", "< [==  ] >", OnStepPestRate, PestRateLabel)
	SETTINGS_ROW_TOGGLE(C, "EXPOSURE LEVEL", "< [==  ] >", OnStepEnvExposure, EnvExposureLabel)
	SETTINGS_ROW_TOGGLE(C, "ATMOSPHERE DENSITY", "< [==  ] >", OnStepFogDensity, EnvFogDensityLabel)
	SETTINGS_ROW_TOGGLE(C, "DUST DENSITY", "< [==  ] >", OnStepDustDensity, EnvDustDensityLabel)
#undef SETTINGS_ROW_TOGGLE

	// The way out is the [ X ] in the header rule, as on the character sheet.
	RefreshRows();
}

void USettingsWidget::RefreshRows()
{
	const URepliCanUserSettings* S = URepliCanUserSettings::Get();
	auto OnOff = [](bool b) { return FText::FromString(b ? TEXT("ON") : TEXT("OFF")); };
	auto Bar = [](int32 Step)
	{
		FString B = TEXT("< [");
		for (int32 i = 0; i < 4; ++i) { B += (i < Step) ? TEXT("=") : TEXT(" "); }
		return FText::FromString(B + TEXT("] >"));
	};
	const bool bFull = S && S->GetFullscreenMode() != EWindowMode::Windowed;
	if (FullscreenLabel) { FullscreenLabel->SetText(FText::FromString(bFull ? TEXT("< FULLSCREEN >") : TEXT("< WINDOWED >"))); }
	if (AlwaysIntroLabel) { AlwaysIntroLabel->SetText(OnOff(S && S->bAlwaysShowIntro)); }
	if (FootstepVolumeLabel) { FootstepVolumeLabel->SetText(Bar(S ? S->FootstepVolumeStep : 2)); }
	if (EnvGradeLabel) { EnvGradeLabel->SetText(OnOff(!S || S->bEnvGrade)); }
	if (EnvAOLabel) { EnvAOLabel->SetText(OnOff(!S || S->bEnvAmbientOcclusion)); }
	if (EnvFogLabel) { EnvFogLabel->SetText(OnOff(!S || S->bEnvFog)); }
	if (EnvVolumetricLabel) { EnvVolumetricLabel->SetText(OnOff(!S || S->bEnvVolumetricFog)); }
	if (EnvDustLabel) { EnvDustLabel->SetText(OnOff(!S || S->bEnvDust)); }
	if (EnvFlickerLabel) { EnvFlickerLabel->SetText(OnOff(!S || S->bEnvLampFlicker)); }
	if (PestsLabel) { PestsLabel->SetText(OnOff(!S || S->bEnvPests)); }
	if (AOStrengthLabel)
	{
		// Ten stops, not eight: the last two are the overdriven demo settings and say so, because
		// a full bar that keeps getting darker is otherwise just confusing.
		static const TCHAR* Ten[10] = { TEXT("< [       ] >"), TEXT("< [=      ] >"), TEXT("< [==     ] >"), TEXT("< [===    ] >"),
		                                TEXT("< [====   ] >"), TEXT("< [=====  ] >"), TEXT("< [====== ] >"), TEXT("< [=======] >"),
		                                TEXT("< [ DEMO 1] >"), TEXT("< [ DEMO 2] >") };
		AOStrengthLabel->SetText(FText::FromString(Ten[FMath::Clamp(S ? S->EnvAOStrength : 4, 0, 9)]));
	}
	if (EnvFogDensityLabel)
	{
		static const TCHAR* Eight[8] = { TEXT("< [       ] >"), TEXT("< [=      ] >"), TEXT("< [==     ] >"), TEXT("< [===    ] >"),
		                                 TEXT("< [====   ] >"), TEXT("< [=====  ] >"), TEXT("< [====== ] >"), TEXT("< [=======] >") };
		EnvFogDensityLabel->SetText(FText::FromString(Eight[FMath::Clamp(S ? S->EnvFogDensity : 3, 0, 7)]));
	}
	if (PestRateLabel)
	{
		static const TCHAR* Bars[5] = { TEXT("< [     ] >"), TEXT("< [=    ] >"), TEXT("< [==   ] >"), TEXT("< [===  ] >"), TEXT("< [==== ] >") };
		PestRateLabel->SetText(FText::FromString(Bars[FMath::Clamp(S ? S->EnvPestRateStep : 2, 0, 4)]));
	}
	if (EnvFixedExposureLabel) { EnvFixedExposureLabel->SetText(OnOff(S && S->bEnvFixedExposure)); }
	if (EnvExposureLabel) { EnvExposureLabel->SetText(Bar(S ? S->EnvExposure : 2)); }
	if (EnvDustDensityLabel) { EnvDustDensityLabel->SetText(Bar(S ? S->EnvDustDensity : 2)); }
}

void USettingsWidget::PokeDirector()
{
	UWorld* W = GetWorld();
	if (!W) { return; }
	for (TActorIterator<AEnvironmentDirector> It(W); It; ++It) { It->ApplyEnvironment(); return; }
}

void USettingsWidget::BuildKeysPage()
{
	if (TitleBox)
	{
		if (!bShowBack) { TitleBox->AddChildToVerticalBox(Crt::Text(WidgetTree, TEXT("KEY BINDINGS"), 30, Crt::Green, ETextJustify::Center))->SetPadding(FMargin(0, 0, 0, 2)); }
		const FString Hint = PendingHint.IsEmpty() ? FString(TEXT("Click a binding, then press the key you want. Esc cancels.")) : PendingHint;
		PendingHint.Reset();
		KeyHint = Crt::Text(WidgetTree, Hint, 14, Crt::DimGreen, ETextJustify::Center);
		TitleBox->AddChildToVerticalBox(KeyHint)->SetPadding(FMargin(0, 0, 0, 6));
	}
	// The bindings in three columns of about equal length, each column re-saying the category it
	// starts in the middle of, so nothing is read out of context.
	const TArray<InputBindings::FAction>& All = InputBindings::All();
	const int32 PerColumn = FMath::Max(1, (All.Num() + 2) / 3);
	FString LastCategory;
	for (int32 i = 0; i < All.Num(); ++i)
	{
		const InputBindings::FAction& Action = All[i];
		UVerticalBox* Into = Col(FMath::Min(2, i / PerColumn));
		if (Action.Category != LastCategory || i % PerColumn == 0)
		{
			LastCategory = Action.Category;
			Heading(Into, LastCategory.ToLower());
		}
		UButton* Btn = Crt::Button(WidgetTree, InputBindings::Describe(Action.Id), SettingsRowSize);
		Btn->OnClicked.AddDynamic(this, &USettingsWidget::OnKeyRowClicked);
		KeyRowLabels.Add(Action.Id, Crt::ButtonLabel(Btn));
		ButtonActions.Add(Btn, Action.Id);
		Row(Into, Action.Label, Btn, true);
	}
	UVerticalBox* Last = Col(2);
	Heading(Last, TEXT("keys page"));
	UButton* Reset = Crt::Button(WidgetTree, TEXT("[ RESET ALL ]"), SettingsRowSize);
	Reset->OnClicked.AddDynamic(this, &USettingsWidget::OnResetKeys);
	Row(Last, TEXT("RESTORE DEFAULTS"), Reset, true);
	UButton* Back = Crt::Button(WidgetTree, TEXT("[ BACK ]"), SettingsRowSize);
	Back->OnClicked.AddDynamic(this, &USettingsWidget::OnBack);
	Row(Last, TEXT("DONE"), Back, true);
	SetKeyboardFocus();
}

void USettingsWidget::OnKeyRowClicked()
{
	// Which row: the one the pointer is over. A dynamic delegate carries no payload, and the
	// click that fired this necessarily happened on the hovered button.
	for (const TPair<TObjectPtr<UButton>, FName>& Pair : ButtonActions)
	{
		if (Pair.Key && Pair.Key->IsHovered()) { BeginCapture(Pair.Value); return; }
	}
}

void USettingsWidget::OnOpenKeys()
{
	bKeysPage = true;
	Rebuild(bTitle);
}

void USettingsWidget::OnResetKeys()
{
	InputBindings::ResetAll();
	Rebuild(bTitle);
}

void USettingsWidget::BeginCapture(FName ActionId)
{
	CapturingAction = ActionId;
	if (TObjectPtr<UTextBlock>* Found = KeyRowLabels.Find(ActionId))
	{
		if (*Found) { (*Found)->SetText(FText::FromString(TEXT("< PRESS A KEY >"))); }
	}
	if (KeyHint) { KeyHint->SetText(FText::FromString(TEXT("Listening. Press a key or mouse button. Esc cancels."))); }
	SetKeyboardFocus();
}

void USettingsWidget::FinishCapture(const FKey& Key, bool bShift, bool bCtrl, bool bAlt)
{
	const FName Id = CapturingAction;
	CapturingAction = NAME_None;
	if (Id.IsNone()) { Rebuild(bTitle); return; }
	// Esc is how you back out of the capture, so it can never be bound from here.
	if (Key != EKeys::Escape)
	{
		FKey Modifier = EKeys::Invalid;
		if (bCtrl) { Modifier = EKeys::LeftControl; }
		else if (bAlt) { Modifier = EKeys::LeftAlt; }
		else if (bShift) { Modifier = EKeys::LeftShift; }
		const FName Clash = InputBindings::ConflictWith(Id, Key, Modifier);
		InputBindings::SetKey(Id, Key, Modifier);
		// A clash is reported rather than refused: two commands that can never be live at the
		// same moment may safely share a key, and the player knows their own game better than
		// a validation rule does.
		if (const InputBindings::FAction* Other = Clash.IsNone() ? nullptr : InputBindings::Find(Clash))
		{
			PendingHint = FString::Printf(TEXT("Also bound to %s -- both will fire."), Other->Label);
		}
	}
	// Redraw, so every row shows what it is now -- including any row that just lost its key.
	Rebuild(bTitle);
}

FReply USettingsWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	if (!CapturingAction.IsNone())
	{
		const FKey Key = KeyEvent.GetKey();
		// A bare modifier is somebody reaching for a combination, not the binding itself.
		if (Key == EKeys::LeftControl || Key == EKeys::RightControl || Key == EKeys::LeftShift
			|| Key == EKeys::RightShift || Key == EKeys::LeftAlt || Key == EKeys::RightAlt)
		{
			return FReply::Handled();
		}
		FinishCapture(Key, KeyEvent.IsShiftDown(), KeyEvent.IsControlDown(), KeyEvent.IsAltDown());
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(Geometry, KeyEvent);
}

FReply USettingsWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent)
{
	if (!CapturingAction.IsNone())
	{
		FinishCapture(MouseEvent.GetEffectingButton(), MouseEvent.IsShiftDown(), MouseEvent.IsControlDown(), MouseEvent.IsAltDown());
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(Geometry, MouseEvent);
}

void USettingsWidget::OnBack()
{
	// The keys page backs out to the settings page, not out of settings entirely.
	if (bKeysPage) { bKeysPage = false; Rebuild(bTitle); return; }
	OnClose.ExecuteIfBound();
}

void USettingsWidget::OnToggleFullscreen()
{
	if (URepliCanUserSettings* S = URepliCanUserSettings::Get())
	{
		const bool bFull = S->GetFullscreenMode() != EWindowMode::Windowed;
		S->SetFullscreenMode(bFull ? EWindowMode::Windowed : EWindowMode::WindowedFullscreen);
		S->ApplySettings(false);
		S->SaveSettings();
	}
	RefreshRows();
}

void USettingsWidget::OnToggleAlwaysIntro()
{
	if (URepliCanUserSettings* S = URepliCanUserSettings::Get()) { S->bAlwaysShowIntro = !S->bAlwaysShowIntro; S->SaveSettings(); }
	RefreshRows();
}

void USettingsWidget::OnStepFootstepVolume()
{
	// Nothing to rebuild: the character reads this every time it plays a step.
	if (URepliCanUserSettings* S = URepliCanUserSettings::Get()) { S->FootstepVolumeStep = (S->FootstepVolumeStep + 1) % 5; S->SaveSettings(); }
	RefreshRows();
}

#define SETTINGS_TOGGLE(Fn, Field) \
	void USettingsWidget::Fn() \
	{ \
		if (URepliCanUserSettings* S = URepliCanUserSettings::Get()) { S->Field = !S->Field; S->SaveSettings(); } \
		PokeDirector(); \
		RefreshRows(); \
	}

SETTINGS_TOGGLE(OnTogglePests, bEnvPests)
SETTINGS_TOGGLE(OnToggleEnvGrade, bEnvGrade)
SETTINGS_TOGGLE(OnToggleEnvAO, bEnvAmbientOcclusion)
SETTINGS_TOGGLE(OnToggleEnvFog, bEnvFog)
SETTINGS_TOGGLE(OnToggleEnvVolumetric, bEnvVolumetricFog)
SETTINGS_TOGGLE(OnToggleEnvDust, bEnvDust)
SETTINGS_TOGGLE(OnToggleEnvFlicker, bEnvLampFlicker)
SETTINGS_TOGGLE(OnToggleEnvFixedExposure, bEnvFixedExposure)
#undef SETTINGS_TOGGLE

#define SETTINGS_STEP(Fn, Field) SETTINGS_STEP_N(Fn, Field, 5)
#define SETTINGS_STEP_N(Fn, Field, N) \
	void USettingsWidget::Fn() \
	{ \
		if (URepliCanUserSettings* S = URepliCanUserSettings::Get()) { S->Field = (S->Field + 1) % (N); S->SaveSettings(); } \
		PokeDirector(); \
		RefreshRows(); \
	}

SETTINGS_STEP(OnStepPestRate, EnvPestRateStep)
SETTINGS_STEP(OnStepEnvExposure, EnvExposure)
SETTINGS_STEP_N(OnStepFogDensity, EnvFogDensity, 8)
SETTINGS_STEP_N(OnStepAOStrength, EnvAOStrength, 10)
SETTINGS_STEP(OnStepDustDensity, EnvDustDensity)
#undef SETTINGS_STEP
#undef SETTINGS_STEP_N
