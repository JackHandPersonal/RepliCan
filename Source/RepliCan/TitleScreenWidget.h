// The front-end screen: one full-screen widget whose pages (title, intro,
// main menu, load game, settings, loading) are swapped by
// ATitlePlayerController. Draws the CRT treatment itself in NativePaint --
// scanlines, a soft vignette and a faint flicker -- over whatever page is
// up, so no material or texture asset is needed.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TitlePlayerController.h"
#include "TitleScreenWidget.generated.h"

class UBorder;
class UButton;
class UProgressBar;
class UTextBlock;
class UVerticalBox;
class UWidgetSwitcher;

UENUM()
enum class ETitlePage : uint8 { Title, Intro, Menu, LoadGame, Settings, Loading };

// A load-slot button knows which slot it is.
UCLASS()
class UTitleSlotBinding : public UObject
{
	GENERATED_BODY()
public:
	TWeakObjectPtr<ATitlePlayerController> Controller;
	FString Slot;
	UFUNCTION() void OnClicked();
};

// A main-menu entry: hovering it selects it (with a key clack).
UCLASS()
class UTitleMenuBinding : public UObject
{
	GENERATED_BODY()
public:
	TWeakObjectPtr<class UTitleScreenWidget> Widget;
	int32 Index = 0;
	UFUNCTION() void OnHovered();
};

UCLASS()
class REPLICAN_API UTitleScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetOwnerController(ATitlePlayerController* In) { OwnerController = In; }
	void ShowPage(ETitlePage Page);
	ETitlePage GetPage() const { return CurrentPage; }

	// Main menu keyboard navigation (arrows / W S, Enter / Space) and the
	// mouse-hover highlight share one selection; every change clacks.
	void SetMenuSelection(int32 Index, bool bClack);
	void MoveMenuSelection(int32 Delta);
	void ActivateMenuSelection();
	int32 GetMenuSelection() const { return MenuSelection; }

	// Intro typewriter.
	void StartIntro(const TArray<FIntroLine>& Lines, float CharsPerSecond);
	void RevealIntro();            // show everything at once
	bool IsIntroFinished() const { return bIntroFinished; }

	void SetLoadSlots(const TArray<FString>& Slots);
	void SetLoadingProgress(float Fraction, const FString& StatusOrEmpty);
	void RefreshSettings();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	UFUNCTION() void OnNewGame();
	UFUNCTION() void OnLoadGame();
	UFUNCTION() void OnSettings();
	UFUNCTION() void OnQuit();
	UFUNCTION() void OnBack();
	UFUNCTION() void OnToggleFullscreen();
	UFUNCTION() void OnToggleAlwaysIntro();
	// Environment switches. Each writes the setting, saves it, and asks the director to rebuild
	// that feature if one is live, so a toggle made in game takes effect immediately.
	UFUNCTION() void OnToggleEnvGrade();
	UFUNCTION() void OnToggleEnvAO();
	UFUNCTION() void OnToggleEnvFog();
	UFUNCTION() void OnToggleEnvVolumetric();
	UFUNCTION() void OnToggleEnvDust();
	UFUNCTION() void OnToggleEnvFlicker();
	UFUNCTION() void OnToggleEnvFixedExposure();
	UFUNCTION() void OnStepEnvExposure();
	UFUNCTION() void OnStepFootstepVolume();
	UFUNCTION() void OnStepFogDensity();
	UFUNCTION() void OnStepDustDensity();
	UFUNCTION() void OnReplayIntro();

	UWidget* BuildLogo(int32 BodySize, bool bFrame);   // REPLICAN + version, optionally in an ASCII frame
	UWidget* SettingsRow(UVerticalBox* Into, const FString& Label, UWidget* Value, bool bWired);
	UWidget* SettingsHeading(UVerticalBox* Into, const FString& Title);
	UWidget* BuildTitlePage();
	UWidget* BuildIntroPage();
	UWidget* BuildMenuPage();
	UWidget* BuildLoadPage();
	UWidget* BuildSettingsPage();
	UWidget* BuildLoadingPage();
	UBorder* Frame(UWidget* Content, float MaxWidth);

	UPROPERTY() TObjectPtr<ATitlePlayerController> OwnerController;
	UPROPERTY() TObjectPtr<UWidgetSwitcher> Pages;
	UPROPERTY() TObjectPtr<UTextBlock> TitlePrompt;
	UPROPERTY() TObjectPtr<UTextBlock> IntroText;
	UPROPERTY() TObjectPtr<UTextBlock> IntroPrompt;
	UPROPERTY() TObjectPtr<UVerticalBox> SlotList;
	UPROPERTY() TObjectPtr<class USettingsWidget> SettingsPanel;
	UPROPERTY() TObjectPtr<UTextBlock> FullscreenLabel;
	UPROPERTY() TObjectPtr<UTextBlock> AlwaysIntroLabel;
	UPROPERTY() TObjectPtr<UTextBlock> IntroSeenLabel;
	UPROPERTY() TObjectPtr<UTextBlock> EnvGradeLabel;
	UPROPERTY() TObjectPtr<UTextBlock> EnvAOLabel;
	UPROPERTY() TObjectPtr<UTextBlock> EnvFogLabel;
	UPROPERTY() TObjectPtr<UTextBlock> EnvVolumetricLabel;
	UPROPERTY() TObjectPtr<UTextBlock> EnvDustLabel;
	UPROPERTY() TObjectPtr<UTextBlock> EnvFlickerLabel;
	UPROPERTY() TObjectPtr<UTextBlock> EnvFixedExposureLabel;
	UPROPERTY() TObjectPtr<UTextBlock> EnvExposureLabel;
	UPROPERTY() TObjectPtr<UTextBlock> FootstepVolumeLabel;
	UPROPERTY() TObjectPtr<UTextBlock> EnvFogDensityLabel;
	UPROPERTY() TObjectPtr<UTextBlock> EnvDustDensityLabel;
	UPROPERTY() TObjectPtr<UProgressBar> LoadingBar;
	UPROPERTY() TObjectPtr<UTextBlock> LoadingStatus;
	UPROPERTY() TObjectPtr<UTextBlock> LoadingLog;
	UPROPERTY() TArray<TObjectPtr<UTitleSlotBinding>> SlotBindings;
	UPROPERTY() TArray<TObjectPtr<UButton>> MenuButtons;
	UPROPERTY() TArray<TObjectPtr<UTitleMenuBinding>> MenuBindings;
	TArray<FString> MenuLabels;
	int32 MenuSelection = -1;

	ETitlePage CurrentPage = ETitlePage::Title;
	float Clock = 0.0f;

	// Typewriter state.
	TArray<FIntroLine> IntroLines;
	float IntroCharsPerSecond = 38.0f;
	int32 IntroLineIndex = 0;
	int32 IntroCharIndex = 0;
	float IntroAccumulator = 0.0f;
	float IntroPauseLeft = 0.0f;
	FString IntroRevealed;
	bool bIntroFinished = false;

	// Loading "boot log" lines cycle while the bar fills.
	int32 LoadingLogShown = 0;
	float LoadingLogTimer = 0.0f;
};
