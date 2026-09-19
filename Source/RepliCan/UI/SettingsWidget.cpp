#include "SettingsWidget.h"
#include "CrtStyle.h"
#include "CrtRuleWidget.h"
#include "SheetSpec.h"
#include "Components/Border.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "RepliCanUserSettings.h"
#include "EnvironmentDirector.h"
#include "InputBindings.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "EngineUtils.h"
#include "Engine/World.h"

void USettingsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// The same box the character sheet sits in -- the panel colour and inset, a header rule with
	// the title set into it and the [ X ] at its end -- with the rows in a column down the middle
	// that scrolls if it ever outgrows the box. Rebuild dresses or undresses it for the context.
	Root = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SettingsRoot"));
	WidgetTree->RootWidget = Root;
	UVerticalBox* Outer = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SettingsOuter"));
	Root->SetContent(Outer);
	HeaderBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SettingsHeader"));
	Outer->AddChildToVerticalBox(HeaderBox)->SetPadding(FMargin(0, 0, 0, 12));
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("SettingsScroll"));
	Scroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	UVerticalBoxSlot* ScrollSlot = Outer->AddChildToVerticalBox(Scroll);
	ScrollSlot->SetSize(ESlateSizeRule::Fill);
	ColumnFit = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SettingsFit"));
	Scroll->AddChild(ColumnFit);
	if (UScrollBoxSlot* FitSlot = Cast<UScrollBoxSlot>(ColumnFit->Slot)) { FitSlot->SetHorizontalAlignment(HAlign_Center); }
	Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SettingsColumn"));
	ColumnFit->AddChild(Column);
}

UWidget* USettingsWidget::Heading(UVerticalBox* Into, const FString& Title)
{
	// "-- Display --" in false caps: the section name capitalised, the rest small.
	FString Cased = Title;
	if (Cased.Len() > 0) { Cased[0] = FChar::ToUpper(Cased[0]); }
	UHorizontalBox* T = Crt::SmallCaps(WidgetTree, FString::Printf(TEXT("-- %s --"), *Cased), 15, Crt::DimGreen);
	Into->AddChildToVerticalBox(T)->SetPadding(FMargin(0, 18, 0, 6));
	return T;
}

// One settings line: label on the left, its control on the right. Rows that do nothing yet are
// dimmed, so it is obvious at a glance which of them are real.
UWidget* USettingsWidget::Row(UVerticalBox* Into, const FString& Label, UWidget* Value, bool bWired)
{
	UHorizontalBox* R = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UTextBlock* L = Crt::Text(WidgetTree, TEXT("  ") + Label, 17, bWired ? Crt::Green : Crt::DimGreen);
	UHorizontalBoxSlot* LS = R->AddChildToHorizontalBox(L);
	LS->SetSize(ESlateSizeRule::Fill);
	LS->SetVerticalAlignment(VAlign_Center);
	UHorizontalBoxSlot* VS = R->AddChildToHorizontalBox(Value);
	VS->SetHorizontalAlignment(HAlign_Right);
	VS->SetVerticalAlignment(VAlign_Center);
	Into->AddChildToVerticalBox(R)->SetPadding(FMargin(0, 2));
	return R;
}

UTextBlock* USettingsWidget::Stub(const TCHAR* Value)
{
	return Crt::Text(WidgetTree, Value, 17, Crt::DimGreen);
}

void USettingsWidget::Rebuild(bool bTitleContext)
{
	if (!Column) { return; }
	bTitle = bTitleContext;
	Column->ClearChildren();
	KeyRowLabels.Reset(); ButtonActions.Reset(); KeyHint = nullptr; CapturingAction = NAME_None;
	// Dressed as the character sheet's dialog in the pause context; bare inside the title screen's own frame.
	if (Root) { Root->SetBrushColor(bShowBack ? Crt::Panel : FLinearColor::Transparent); Root->SetPadding(bShowBack ? FMargin(34.0f, 26.0f) : FMargin(0.0f)); }
	if (ColumnFit) { if (bShowBack) { ColumnFit->SetWidthOverride(760.0f); } else { ColumnFit->ClearWidthOverride(); } }
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

	if (!bShowBack) { Column->AddChildToVerticalBox(Crt::Text(WidgetTree, TEXT("SETTINGS"), 34, Crt::Green, ETextJustify::Center))->SetPadding(FMargin(0, 0, 0, 8)); }   // in the pause context the header rule carries the title

	Heading(Column, TEXT("display"));
	UButton* Fullscreen = Crt::Button(WidgetTree, TEXT("< WINDOWED >"), 17);
	Fullscreen->OnClicked.AddDynamic(this, &USettingsWidget::OnToggleFullscreen);
	FullscreenLabel = Crt::ButtonLabel(Fullscreen);
	Row(Column, TEXT("DISPLAY MODE"), Fullscreen, true);
	Row(Column, TEXT("RESOLUTION"), Stub(TEXT("< 1920 x 1080 >")), false);
	Row(Column, TEXT("VSYNC"), Stub(TEXT("ON")), false);
	Row(Column, TEXT("CRT SCANLINES"), Stub(TEXT("ON")), false);

	Heading(Column, TEXT("audio"));
	Row(Column, TEXT("MASTER VOLUME"), Stub(TEXT("[========  ]  80%")), false);
	Row(Column, TEXT("VOICE VOLUME"), Stub(TEXT("[==========] 100%")), false);
	Row(Column, TEXT("EFFECTS VOLUME"), Stub(TEXT("[========  ]  80%")), false);
	Row(Column, TEXT("MUSIC VOLUME"), Stub(TEXT("[======    ]  60%")), false);
	{
		UButton* B = Crt::Button(WidgetTree, TEXT("< [==  ] >"), 17);
		B->OnClicked.AddDynamic(this, &USettingsWidget::OnStepFootstepVolume);
		FootstepVolumeLabel = Crt::ButtonLabel(B);
		Row(Column, TEXT("FOOTSTEP VOLUME"), B, true);
	}

	Heading(Column, TEXT("controls"));
	Row(Column, TEXT("MOUSE SENSITIVITY"), Stub(TEXT("< 1.0 >")), false);
	Row(Column, TEXT("INVERT LOOK"), Stub(TEXT("OFF")), false);
	{
		UButton* B = Crt::Button(WidgetTree, TEXT("[ EDIT ]"), 17);
		B->OnClicked.AddDynamic(this, &USettingsWidget::OnOpenKeys);
		Row(Column, TEXT("KEY BINDINGS"), B, true);
	}

	Heading(Column, TEXT("environment"));
	{
		UButton* B = Crt::Button(WidgetTree, TEXT("ON"), 17);
		B->OnClicked.AddDynamic(this, &USettingsWidget::OnToggleEnvGrade);
		EnvGradeLabel = Crt::ButtonLabel(B);
		Row(Column, TEXT("COLOUR GRADE"), B, true);
	}
	{
		UButton* B = Crt::Button(WidgetTree, TEXT("ON"), 17);
		B->OnClicked.AddDynamic(this, &USettingsWidget::OnToggleEnvAO);
		EnvAOLabel = Crt::ButtonLabel(B);
		Row(Column, TEXT("AMBIENT OCCLUSION"), B, true);
	}
	{
		UButton* B = Crt::Button(WidgetTree, TEXT("ON"), 17);
		B->OnClicked.AddDynamic(this, &USettingsWidget::OnToggleEnvFog);
		EnvFogLabel = Crt::ButtonLabel(B);
		Row(Column, TEXT("ATMOSPHERE"), B, true);
	}
	{
		UButton* B = Crt::Button(WidgetTree, TEXT("ON"), 17);
		B->OnClicked.AddDynamic(this, &USettingsWidget::OnToggleEnvVolumetric);
		EnvVolumetricLabel = Crt::ButtonLabel(B);
		Row(Column, TEXT("LIGHT SHAFTS"), B, true);
	}
	{
		UButton* B = Crt::Button(WidgetTree, TEXT("ON"), 17);
		B->OnClicked.AddDynamic(this, &USettingsWidget::OnToggleEnvDust);
		EnvDustLabel = Crt::ButtonLabel(B);
		Row(Column, TEXT("DUST IN THE AIR"), B, true);
	}
	{
		UButton* B = Crt::Button(WidgetTree, TEXT("ON"), 17);
		B->OnClicked.AddDynamic(this, &USettingsWidget::OnToggleEnvFlicker);
		EnvFlickerLabel = Crt::ButtonLabel(B);
		Row(Column, TEXT("LAMP FLICKER"), B, true);
	}

	{
		UButton* B = Crt::Button(WidgetTree, TEXT("< [====   ] >"), 17);
		B->OnClicked.AddDynamic(this, &USettingsWidget::OnStepAOStrength);
		AOStrengthLabel = Crt::ButtonLabel(B);
		Row(Column, TEXT("CORNER SHADING"), B, true);
	}
	{
		UButton* B = Crt::Button(WidgetTree, TEXT("ON"), 17);
		B->OnClicked.AddDynamic(this, &USettingsWidget::OnTogglePests);
		PestsLabel = Crt::ButtonLabel(B);
		Row(Column, TEXT("VERMIN"), B, true);
	}
	{
		UButton* B = Crt::Button(WidgetTree, TEXT("< [==  ] >"), 17);
		B->OnClicked.AddDynamic(this, &USettingsWidget::OnStepPestRate);
		PestRateLabel = Crt::ButtonLabel(B);
		Row(Column, TEXT("VERMIN FREQUENCY"), B, true);
	}
	{
		UButton* B = Crt::Button(WidgetTree, TEXT("OFF"), 17);
		B->OnClicked.AddDynamic(this, &USettingsWidget::OnToggleEnvFixedExposure);
		EnvFixedExposureLabel = Crt::ButtonLabel(B);
		Row(Column, TEXT("FIXED EXPOSURE"), B, true);
	}
	{
		UButton* B = Crt::Button(WidgetTree, TEXT("< [==  ] >"), 17);
		B->OnClicked.AddDynamic(this, &USettingsWidget::OnStepEnvExposure);
		EnvExposureLabel = Crt::ButtonLabel(B);
		Row(Column, TEXT("EXPOSURE LEVEL"), B, true);
	}
	{
		UButton* B = Crt::Button(WidgetTree, TEXT("< [==  ] >"), 17);
		B->OnClicked.AddDynamic(this, &USettingsWidget::OnStepFogDensity);
		EnvFogDensityLabel = Crt::ButtonLabel(B);
		Row(Column, TEXT("ATMOSPHERE DENSITY"), B, true);
	}
	{
		UButton* B = Crt::Button(WidgetTree, TEXT("< [==  ] >"), 17);
		B->OnClicked.AddDynamic(this, &USettingsWidget::OnStepDustDensity);
		EnvDustDensityLabel = Crt::ButtonLabel(B);
		Row(Column, TEXT("DUST DENSITY"), B, true);
	}

	if (bTitle)
	{
		// Only meaningful before a game is running.
		Heading(Column, TEXT("game"));
		UButton* AlwaysIntro = Crt::Button(WidgetTree, TEXT("OFF"), 17);
		AlwaysIntro->OnClicked.AddDynamic(this, &USettingsWidget::OnToggleAlwaysIntro);
		AlwaysIntroLabel = Crt::ButtonLabel(AlwaysIntro);
		Row(Column, TEXT("INTRO ON EVERY START"), AlwaysIntro, true);
	}

	Column->AddChildToVerticalBox(Crt::Text(WidgetTree, TEXT("dim entries are not wired yet"), 12, Crt::Faint))->SetPadding(FMargin(0, 16, 0, 0));
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
	if (EnvFogDensityLabel) { EnvFogDensityLabel->SetText(Bar(S ? S->EnvFogDensity : 2)); }
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
	Column->AddChildToVerticalBox(Crt::Text(WidgetTree, TEXT("KEY BINDINGS"), 34, Crt::Green, ETextJustify::Center))->SetPadding(FMargin(0, 0, 0, 4));
	const FString Hint = PendingHint.IsEmpty() ? FString(TEXT("Click a binding, then press the key you want. Esc cancels.")) : PendingHint;
	PendingHint.Reset();
	KeyHint = Crt::Text(WidgetTree, Hint, 15, Crt::DimGreen, ETextJustify::Center);
	Column->AddChildToVerticalBox(KeyHint)->SetPadding(FMargin(0, 0, 0, 8));

	FString LastCategory;
	for (const InputBindings::FAction& Action : InputBindings::All())
	{
		if (Action.Category != LastCategory)
		{
			LastCategory = Action.Category;
			Heading(Column, LastCategory.ToLower());
		}
		UButton* B = Crt::Button(WidgetTree, InputBindings::Describe(Action.Id), 17);
		B->OnClicked.AddDynamic(this, &USettingsWidget::OnKeyRowClicked);
		KeyRowLabels.Add(Action.Id, Crt::ButtonLabel(B));
		ButtonActions.Add(B, Action.Id);
		Row(Column, Action.Label, B, true);
	}

	UButton* Reset = Crt::Button(WidgetTree, TEXT("[ RESET ALL ]"), 17);
	Reset->OnClicked.AddDynamic(this, &USettingsWidget::OnResetKeys);
	Row(Column, TEXT("RESTORE DEFAULTS"), Reset, true);

	UButton* Back = Crt::Button(WidgetTree, TEXT("[ BACK ]"), 17);
	Back->OnClicked.AddDynamic(this, &USettingsWidget::OnBack);
	Row(Column, TEXT("DONE"), Back, true);
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
