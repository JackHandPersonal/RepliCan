// Drives the front end, in order:
//   Title      "RepliCan" + press any key
//   Intro      first-run terminal boot sequence (Data/Intro.json), typed out;
//              any key finishes the typing, the next key (or Esc) moves on.
//              Shown when the user settings say it hasn't been seen (or
//              "always show" is on). For debugging: console variable
//              RepliCan.SkipIntro=1 (Config/DefaultEngine.ini) or the
//              command line switch -SkipIntro; -ForceIntro does the opposite.
//   Menu       New Game / Load Game / Settings / Quit
//   Loading    the target map is streamed in the background behind a
//              progress screen, then opened.
// Everything is one UTitleScreenWidget with pages; this controller owns the
// state and the transitions.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TitlePlayerController.generated.h"

class UTitleScreenWidget;
class FTitleInputProcessor;

USTRUCT()
struct FIntroLine
{
	GENERATED_BODY()
	UPROPERTY() FString Text;
	UPROPERTY() float PauseAfter = 0.6f;   // seconds after the line finishes
	UPROPERTY() bool bBlank = false;       // an empty line (paragraph break)
};

UCLASS()
class REPLICAN_API ATitlePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ATitlePlayerController();

	// Screen transitions (also the test hooks).
	UFUNCTION(BlueprintCallable, Category = "Title") void ShowTitle();
	UFUNCTION(BlueprintCallable, Category = "Title") void AdvanceFromTitle();
	UFUNCTION(BlueprintCallable, Category = "Title") void ShowIntro();
	UFUNCTION(BlueprintCallable, Category = "Title") void FinishIntro();
	UFUNCTION(BlueprintCallable, Category = "Title") void ShowMainMenu();
	UFUNCTION(BlueprintCallable, Category = "Title") void ShowLoadGame();
	UFUNCTION(BlueprintCallable, Category = "Title") void ShowSettings();
	UFUNCTION(BlueprintCallable, Category = "Title") void StartNewGame();
	UFUNCTION(BlueprintCallable, Category = "Title") void LoadGameSlot(const FString& Slot);
	UFUNCTION(BlueprintCallable, Category = "Title") void QuitGame();
	UFUNCTION(BlueprintPure, Category = "Title") FString GetPageName() const;
	UFUNCTION(BlueprintPure, Category = "Title") bool ShouldShowIntro() const;
	UFUNCTION(BlueprintPure, Category = "Title") bool IsIntroFinished() const;
	UFUNCTION(BlueprintPure, Category = "Title") float GetLoadingProgress() const { return LoadingProgress; }

	// Settings page actions.
	UFUNCTION(BlueprintCallable, Category = "Title") void ToggleFullscreen();
	UFUNCTION(BlueprintCallable, Category = "Title") void ToggleAlwaysShowIntro();
	UFUNCTION(BlueprintCallable, Category = "Title") void ResetIntroSeen();

	// Any key / click from the Slate preprocessor.
	void OnAnyKey(const FKey& Key);
	// Faint keyboard clack for menu navigation.
	UFUNCTION(BlueprintCallable, Category = "Title") void PlayUiClack();
	UFUNCTION(BlueprintPure, Category = "Title") FString GetAmbientState() const;
	UFUNCTION(BlueprintPure, Category = "Title") int32 GetMenuSelection() const;

	// The map New Game opens (long package name).
	UPROPERTY(EditAnywhere, Category = "Title") FString NewGameMap = TEXT("/Game/RepliCan/Maps/Lvl_AsteroidFacility");
	// Keep the loading screen up at least this long so it can be read.
	UPROPERTY(EditAnywhere, Category = "Title") float MinimumLoadingSeconds = 1.8f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void BeginLoading(const FString& MapPackage, const FString& PendingSlot);
	void OnMapPackageLoaded(const FName& PackageName, UPackage* Package, EAsyncLoadingResult::Type Result);
	bool LoadIntroLines(TArray<FIntroLine>& Out, float& CharsPerSecond) const;

	UPROPERTY() TObjectPtr<UTitleScreenWidget> Screen;
	UPROPERTY() TObjectPtr<class UAmbientPlayer> Ambient;
	TSharedPtr<FTitleInputProcessor> InputProcessor;

	FString LoadingMap;
	float LoadingStartedAt = 0.0f;
	float LoadingProgress = 0.0f;
	bool bLoadingPackageReady = false;
	bool bLevelOpened = false;
};
