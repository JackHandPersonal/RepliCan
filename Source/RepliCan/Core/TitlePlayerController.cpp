#include "Core/TitlePlayerController.h"
#include "UI/TitleScreenWidget.h"
#include "World/AmbientPlayer.h"
#include "Core/RepliCanUserSettings.h"
#include "Core/SaveGameSubsystem.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/UObjectGlobals.h"

static TAutoConsoleVariable<int32> CVarSkipIntro(
	TEXT("RepliCan.SkipIntro"), 0,
	TEXT("1 = never play the first-run intro (debugging); 0 = follow the user settings."));

// Sees every key and click before Slate routes them, so "press any key"
// works whatever has focus. Never consumes, so the menu buttons still work.
class FTitleInputProcessor : public IInputProcessor
{
public:
	explicit FTitleInputProcessor(ATitlePlayerController* InOwner) : Owner(InOwner) {}
	virtual void Tick(const float, FSlateApplication&, TSharedRef<ICursor>) override {}
	virtual bool HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& Event) override
	{
		if (Owner.IsValid()) { Owner->OnAnyKey(Event.GetKey()); }
		return false;
	}
	virtual bool HandleMouseButtonDownEvent(FSlateApplication&, const FPointerEvent& Event) override
	{
		if (Owner.IsValid()) { Owner->OnAnyKey(Event.GetEffectingButton()); }
		return false;
	}
private:
	TWeakObjectPtr<ATitlePlayerController> Owner;
};

ATitlePlayerController::ATitlePlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	bShowMouseCursor = true;
}

void ATitlePlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController()) { return; }

	Screen = CreateWidget<UTitleScreenWidget>(this, UTitleScreenWidget::StaticClass());
	Screen->SetOwnerController(this);
	Screen->AddToViewport(10);

	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
	SetShowMouseCursor(true);

	if (FSlateApplication::IsInitialized())
	{
		InputProcessor = MakeShared<FTitleInputProcessor>(this);
		FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor);
	}
	ShowTitle();
}

void ATitlePlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Ambient) { Ambient->Stop(); }
	if (InputProcessor.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
	}
	InputProcessor.Reset();
	Super::EndPlay(Reason);
}

// ---- Pages -----------------------------------------------------------------

void ATitlePlayerController::ShowTitle()
{
	if (Screen) { Screen->ShowPage(ETitlePage::Title); }
}

bool ATitlePlayerController::ShouldShowIntro() const
{
	if (FParse::Param(FCommandLine::Get(), TEXT("ForceIntro"))) { return true; }
	if (FParse::Param(FCommandLine::Get(), TEXT("SkipIntro"))) { return false; }
	if (CVarSkipIntro.GetValueOnGameThread() != 0) { return false; }
	const URepliCanUserSettings* Settings = URepliCanUserSettings::Get();
	return !Settings || Settings->bAlwaysShowIntro || !Settings->bIntroSeen;
}

void ATitlePlayerController::AdvanceFromTitle()
{
	if (ShouldShowIntro()) { ShowIntro(); }
	else { ShowMainMenu(); }
}

void ATitlePlayerController::ShowIntro()
{
	if (!Screen) { return; }
	TArray<FIntroLine> Lines;
	float CharsPerSecond = 38.0f;
	if (!LoadIntroLines(Lines, CharsPerSecond) || Lines.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Title: Data/Intro.json missing or empty, skipping the intro"));
		FinishIntro();
		return;
	}
	Screen->ShowPage(ETitlePage::Intro);
	Screen->StartIntro(Lines, CharsPerSecond);
}

bool ATitlePlayerController::IsIntroFinished() const
{
	return Screen && Screen->IsIntroFinished();
}

void ATitlePlayerController::FinishIntro()
{
	if (URepliCanUserSettings* Settings = URepliCanUserSettings::Get())
	{
		Settings->bIntroSeen = true;
		Settings->SaveSettings();
	}
	ShowMainMenu();
}

void ATitlePlayerController::ShowMainMenu()
{
	if (Screen) { Screen->ShowPage(ETitlePage::Menu); }
	// The facility's own sound bed, faint, from the menu onward (it carries
	// through the loading screen; the level starts the same bed on arrival).
	if (!Ambient) { Ambient = NewObject<UAmbientPlayer>(this); }
	if (!Ambient->IsPlaying()) { Ambient->Start(GetWorld(), TEXT("facility")); }
}

void ATitlePlayerController::PlayUiClack()
{
	static const TCHAR* Clacks[] = { TEXT("key_clack_01.wav"), TEXT("key_clack_02.wav"), TEXT("key_clack_03.wav"), TEXT("key_clack_04.wav") };
	UAmbientPlayer::PlayOneShot(this, GetWorld(), Clacks[FMath::RandRange(0, 3)], 0.22f, FMath::FRandRange(0.92f, 1.08f));
}

FString ATitlePlayerController::GetAmbientState() const
{
	return Ambient ? Ambient->Describe() : TEXT("none");
}

void ATitlePlayerController::ShowLoadGame()
{
	if (!Screen) { return; }
	TArray<FString> Slots;
	if (USaveGameSubsystem* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<USaveGameSubsystem>() : nullptr)
	{
		Slots = Saves->ListSlots();
	}
	Screen->SetLoadSlots(Slots);
	Screen->ShowPage(ETitlePage::LoadGame);
}

void ATitlePlayerController::ShowSettings()
{
	if (Screen) { Screen->RefreshSettings(); Screen->ShowPage(ETitlePage::Settings); }
}

void ATitlePlayerController::StartNewGame()
{
	BeginLoading(NewGameMap, FString());
}

void ATitlePlayerController::LoadGameSlot(const FString& Slot)
{
	USaveGameSubsystem* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<USaveGameSubsystem>() : nullptr;
	if (!Saves || !Saves->SlotExists(Slot)) { ShowLoadGame(); return; }
	FString Map = Saves->GetSlotLevel(Slot);
	if (Map.IsEmpty()) { Map = NewGameMap; }
	BeginLoading(Map, Slot);
}

void ATitlePlayerController::QuitGame()
{
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}

int32 ATitlePlayerController::GetMenuSelection() const
{
	return Screen ? Screen->GetMenuSelection() : -1;
}

FString ATitlePlayerController::GetPageName() const
{
	return Screen ? UEnum::GetValueAsString(Screen->GetPage()).RightChop(FString(TEXT("ETitlePage::")).Len()) : TEXT("None");
}

// ---- Settings --------------------------------------------------------------

void ATitlePlayerController::ToggleFullscreen()
{
	if (URepliCanUserSettings* Settings = URepliCanUserSettings::Get())
	{
		const bool bFull = Settings->GetFullscreenMode() != EWindowMode::Windowed;
		Settings->SetFullscreenMode(bFull ? EWindowMode::Windowed : EWindowMode::WindowedFullscreen);
		Settings->ApplySettings(false);
		Settings->SaveSettings();
	}
	if (Screen) { Screen->RefreshSettings(); }
}

void ATitlePlayerController::ToggleAlwaysShowIntro()
{
	if (URepliCanUserSettings* Settings = URepliCanUserSettings::Get())
	{
		Settings->bAlwaysShowIntro = !Settings->bAlwaysShowIntro;
		Settings->SaveSettings();
	}
	if (Screen) { Screen->RefreshSettings(); }
}

void ATitlePlayerController::ResetIntroSeen()
{
	if (URepliCanUserSettings* Settings = URepliCanUserSettings::Get())
	{
		Settings->bIntroSeen = false;
		Settings->SaveSettings();
	}
	if (Screen) { Screen->RefreshSettings(); }
}

// ---- Input -----------------------------------------------------------------

void ATitlePlayerController::OnAnyKey(const FKey& Key)
{
	if (!Screen) { return; }
	switch (Screen->GetPage())
	{
	case ETitlePage::Title:
		AdvanceFromTitle();
		break;
	case ETitlePage::Intro:
		if (Key == EKeys::Escape || Screen->IsIntroFinished()) { FinishIntro(); }
		else { Screen->RevealIntro(); }
		break;
	case ETitlePage::Menu:
		if (Key == EKeys::Up || Key == EKeys::W || Key == EKeys::Gamepad_DPad_Up) { Screen->MoveMenuSelection(-1); }
		else if (Key == EKeys::Down || Key == EKeys::S || Key == EKeys::Gamepad_DPad_Down) { Screen->MoveMenuSelection(1); }
		else if (Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom) { Screen->ActivateMenuSelection(); }
		break;
	case ETitlePage::LoadGame:
	case ETitlePage::Settings:
		if (Key == EKeys::Escape) { ShowMainMenu(); }
		break;
	default:
		break;
	}
}

// ---- Loading ---------------------------------------------------------------

void ATitlePlayerController::BeginLoading(const FString& MapPackage, const FString& PendingSlot)
{
	if (!Screen || bLevelOpened) { return; }
	if (USaveGameSubsystem* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<USaveGameSubsystem>() : nullptr)
	{
		Saves->SetPendingLoad(PendingSlot);
		Saves->SetPendingNewGame(PendingSlot.IsEmpty());
	}
	LoadingMap = MapPackage;
	LoadingStartedAt = GetWorld()->GetTimeSeconds();
	LoadingProgress = 0.0f;
	bLoadingPackageReady = false;
	Screen->ShowPage(ETitlePage::Loading);
	Screen->SetLoadingProgress(0.0f, PendingSlot.IsEmpty() ? TEXT("FABRICATING") : FString::Printf(TEXT("RESTORING '%s'"), *PendingSlot));
	// Pull the map into memory behind the screen; OpenLevel is then quick.
	LoadPackageAsync(MapPackage, FLoadPackageAsyncDelegate::CreateUObject(this, &ATitlePlayerController::OnMapPackageLoaded));
}

void ATitlePlayerController::OnMapPackageLoaded(const FName& PackageName, UPackage* Package, EAsyncLoadingResult::Type Result)
{
	if (Result != EAsyncLoadingResult::Succeeded)
	{
		UE_LOG(LogTemp, Warning, TEXT("Title: could not preload %s (%d); opening it directly"), *PackageName.ToString(), static_cast<int32>(Result));
	}
	bLoadingPackageReady = true;
}

void ATitlePlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Screen || Screen->GetPage() != ETitlePage::Loading || bLevelOpened) { return; }

	const float Elapsed = GetWorld()->GetTimeSeconds() - LoadingStartedAt;
	const float Async = FMath::Max(0.0f, GetAsyncLoadPercentage(FName(*LoadingMap)) / 100.0f);
	const float TimeShare = FMath::Clamp(Elapsed / MinimumLoadingSeconds, 0.0f, 1.0f);
	LoadingProgress = bLoadingPackageReady ? TimeShare : FMath::Min(0.9f, FMath::Max(Async, TimeShare * 0.9f));
	Screen->SetLoadingProgress(LoadingProgress, FString());

	if (bLoadingPackageReady && Elapsed >= MinimumLoadingSeconds)
	{
		bLevelOpened = true;
		UGameplayStatics::OpenLevel(this, FName(*LoadingMap));
	}
}

// ---- Intro data ------------------------------------------------------------

bool ATitlePlayerController::LoadIntroLines(TArray<FIntroLine>& Out, float& CharsPerSecond) const
{
	const FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("Data"), TEXT("Intro.json"));
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *Path)) { return false; }
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid()) { return false; }
	Root->TryGetNumberField(TEXT("charsPerSecond"), CharsPerSecond);
	const TArray<TSharedPtr<FJsonValue>>* Lines = nullptr;
	if (!Root->TryGetArrayField(TEXT("lines"), Lines)) { return false; }
	for (const TSharedPtr<FJsonValue>& V : *Lines)
	{
		FIntroLine Line;
		if (V->Type == EJson::String)
		{
			Line.Text = V->AsString();
		}
		else if (const TSharedPtr<FJsonObject> Obj = V->AsObject())
		{
			Obj->TryGetStringField(TEXT("text"), Line.Text);
			double Pause = Line.PauseAfter;
			if (Obj->TryGetNumberField(TEXT("pause"), Pause)) { Line.PauseAfter = static_cast<float>(Pause); }
		}
		Line.bBlank = Line.Text.IsEmpty();
		Out.Add(MoveTemp(Line));
	}
	return true;
}
