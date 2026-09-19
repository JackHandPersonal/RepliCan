// The settings page, in one place, so the title screen and the in-game menu show the same thing
// rather than two copies that drift apart. Everything it changes lives in URepliCanUserSettings
// and is saved as it is set; features that can react immediately (the environment director) are
// asked to rebuild the moment their row is touched.
//
// Rows that are not wired yet are drawn dimmed and laid out in their final places, so the page
// reads as a whole rather than as a list of what happens to work.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SettingsWidget.generated.h"

class UVerticalBox;
class UHorizontalBox;
class UTextBlock;
class UButton;


DECLARE_DELEGATE(FOnSettingsClosed);

UCLASS()
class REPLICAN_API USettingsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// bTitleContext hides the rows that only mean something before a game is running (replaying
	// the intro), so the in-game menu does not offer them.
	void Rebuild(bool bTitleContext);
	void RefreshRows();
	// Fired by the BACK row. The title screen goes back to its menu; the pause menu closes the panel.
	FOnSettingsClosed OnClose;
	// The pause menu wants its own frame and back button; the title screen supplies its own.
	void SetShowBack(bool bShow) { bShowBack = bShow; }

protected:
	virtual void NativeOnInitialized() override;
	// Rebinding listens for the next press rather than offering a list of every key in
	// existence: the player presses the key they want, which is the only unambiguous way to
	// say "this one". Both handlers matter -- Fire and Aim are mouse buttons.
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent) override;


private:
	UWidget* Heading(UVerticalBox* Into, const FString& Title);
	UWidget* Row(UVerticalBox* Into, const FString& Label, UWidget* Value, bool bWired);
	UTextBlock* Stub(const TCHAR* Value);

	// The keys page is this same widget in a different mode, so the title screen and the pause
	// menu both get it for free, the way they already share everything else here.
	void BuildMainPage();
	void BuildKeysPage();
	void BeginCapture(FName ActionId);
	void FinishCapture(const FKey& Key, bool bShift, bool bCtrl, bool bAlt);

	UFUNCTION() void OnBack();
	UFUNCTION() void OnOpenKeys();
	UFUNCTION() void OnResetKeys();
	UFUNCTION() void OnKeyRowClicked();
	UFUNCTION() void OnToggleFullscreen();
	UFUNCTION() void OnToggleAlwaysIntro();
	UFUNCTION() void OnStepFootstepVolume();
	UFUNCTION() void OnToggleEnvGrade();
	UFUNCTION() void OnToggleEnvAO();
	UFUNCTION() void OnToggleEnvFog();
	UFUNCTION() void OnToggleEnvVolumetric();
	UFUNCTION() void OnToggleEnvDust();
	UFUNCTION() void OnToggleEnvFlicker();
	UFUNCTION() void OnTogglePests();
	UFUNCTION() void OnStepPestRate();
	UFUNCTION() void OnStepAOStrength();
	UFUNCTION() void OnToggleEnvFixedExposure();
	UFUNCTION() void OnStepEnvExposure();
	UFUNCTION() void OnStepFogDensity();
	UFUNCTION() void OnStepDustDensity();

	// Asks the live environment director to re-apply itself, when there is one. On the title
	// screen there is not, and the setting simply takes effect when the facility loads.
	void PokeDirector();

	UPROPERTY() TObjectPtr<UVerticalBox> Column;
	// The box the rows sit in: the character sheet's panel, header rule and [ X ] (pause context);
	// on the title screen, which frames the panel itself, these stay bare.
	UPROPERTY() TObjectPtr<class UBorder> Root;
	UPROPERTY() TObjectPtr<UHorizontalBox> HeaderBox;
	UPROPERTY() TObjectPtr<class USizeBox> ColumnFit;
	UPROPERTY() TObjectPtr<UTextBlock> FullscreenLabel;
	UPROPERTY() TObjectPtr<UTextBlock> AlwaysIntroLabel;
	UPROPERTY() TObjectPtr<UTextBlock> FootstepVolumeLabel;
	UPROPERTY() TObjectPtr<UTextBlock> EnvGradeLabel;
	UPROPERTY() TObjectPtr<UTextBlock> EnvAOLabel;
	UPROPERTY() TObjectPtr<UTextBlock> EnvFogLabel;
	UPROPERTY() TObjectPtr<UTextBlock> EnvVolumetricLabel;
	UPROPERTY() TObjectPtr<UTextBlock> EnvDustLabel;
	UPROPERTY() TObjectPtr<UTextBlock> EnvFlickerLabel;
	UPROPERTY() TObjectPtr<UTextBlock> PestsLabel;
	UPROPERTY() TObjectPtr<UTextBlock> PestRateLabel;
	UPROPERTY() TObjectPtr<UTextBlock> AOStrengthLabel;
	UPROPERTY() TObjectPtr<UTextBlock> EnvFixedExposureLabel;
	UPROPERTY() TObjectPtr<UTextBlock> EnvExposureLabel;
	UPROPERTY() TObjectPtr<UTextBlock> EnvFogDensityLabel;
	UPROPERTY() TObjectPtr<UTextBlock> EnvDustDensityLabel;

	bool bShowBack = true;
	bool bTitle = false;
	// Which page is showing, and which action is waiting for a key. None means nothing is
	// listening, so a key press falls through to whatever else wants it.
	bool bKeysPage = false;
	FName CapturingAction;
	// A dynamic delegate cannot carry which row was clicked, and one UFUNCTION per binding
	// would mean editing this file to add a command -- exactly what the table exists to avoid.
	// So every row shares one handler and this says which button means which action.
	UPROPERTY() TMap<TObjectPtr<UButton>, FName> ButtonActions;
	UPROPERTY() TMap<FName, TObjectPtr<UTextBlock>> KeyRowLabels;
	UPROPERTY() TObjectPtr<UTextBlock> KeyHint;
	// Survives the rebuild a rebind triggers, so the "also bound to X" warning is still there
	// to read once the rows have been redrawn.
	FString PendingHint;

};
