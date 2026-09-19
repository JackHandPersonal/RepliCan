// The character manager panel: create, configure, review and save a
// character so it can be instantiated at runtime with every setting in
// place. Two Synty character kinds -- an assembled Modular Fantasy Hero, or
// a single SK_Chr_* mesh with a palette -- plus everything shared: face
// systems (nose, decal mouth, blink, expression), weapon and carry pose,
// scale, per-character speed, and New / Load / Save of the whole thing as
// <Project>/Characters/<Name>.json (see CharacterConfig.h).
//
// Built entirely in C++ via WidgetTree (no UMG Designer/Blueprint layout
// needed) to stay within this project's "everything scriptable through the
// same C++ + remote-exec workflow" convention -- see
// feedback_prefer_cpp_over_blueprint. Every control is a thin view over
// ABaseCharacter's FCharacterConfig: the panel never owns configuration
// itself, so closing/reopening it, loading a file, or changing the same
// thing via Python all stay consistent. Every row has a small reset button
// that puts that one field back to ABaseCharacter::GetDefaultCharacterConfig.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AssetRegistry/AssetData.h"
#include "CharacterBuilderWidget.generated.h"

class ABaseCharacter;
class ABasePlayerController;
class UCharacterAnimInstance;
class UVerticalBox;
class UHorizontalBox;
class UTextBlock;
class USlider;
class UComboBoxString;
class UEditableTextBox;
class UCheckBox;
class UBorder;
class UButton;
class UCharacterBuilderWidget;
struct FCharacterConfig;

// One of these per control. Dynamic delegates need a UFUNCTION and hand
// back no "which control fired" information, so each control gets its own
// tiny receiver that knows its key and forwards to the panel -- a handful
// of handlers on the panel instead of one UFUNCTION per part slot / slider.
UCLASS()
class UBuilderControlBinding : public UObject
{
	GENERATED_BODY()

public:

	TWeakObjectPtr<UCharacterBuilderWidget> Owner;
	FString Key;

	UFUNCTION() void OnCombo(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION() void OnSlider(float NewValue);
	UFUNCTION() void OnButton();
	UFUNCTION() void OnCheck(bool bIsChecked);
	UFUNCTION() void OnTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);
	UFUNCTION() UWidget* GenerateComboItem(FString Item);
};

UCLASS()
class REPLICAN_API UCharacterBuilderWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	// The character this panel currently edits (the "selected" character;
	// may be null after Remove). Re-syncs the controls if the panel is up.
	void SetTargetCharacter(ABaseCharacter* InTargetCharacter);
	ABaseCharacter* GetTargetCharacter() const { return TargetCharacter; }

	// The controller that owns the panel -- it runs placement / selection /
	// removal / fly mode (see ABasePlayerController).
	void SetOwnerController(ABasePlayerController* InController) { OwnerController = InController; }

	// Called by the bindings.
	void HandleCombo(const FString& Key, const FString& SelectedItem);
	void HandleSlider(const FString& Key, float NewValue);
	void HandleButton(const FString& Key);
	void HandleCheck(const FString& Key, bool bChecked);
	void HandleText(const FString& Key, const FString& Text);
	UWidget* MakeComboItem(const FString& Item);

protected:

	// Layout is built here, NOT in NativeConstruct: Slate builds its
	// hierarchy from WidgetTree BEFORE NativeConstruct runs, so a tree
	// assembled there is invisible on the first AddToViewport.
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

	// True for the Face Manager page (UFaceManagerWidget): the same panel
	// machinery building only the face controls, with its own title and a
	// Back that returns to the Character Manager. The Character Manager
	// itself then shows a "Face" button where those controls used to be.
	bool bFaceManager = false;
	// True for the Animation Browser page (UAnimationBrowserWidget): a
	// category + clip picker with Play / Loop / Stop, for trying any
	// imported animation on the selected character.
	bool bAnimBrowser = false;

private:

	void BuildFaceSection();
	void BuildAnimBrowserSection();
	void RefreshAnimClips();
	UPROPERTY() TObjectPtr<UComboBoxString> AnimCategoryCombo;
	UPROPERTY() TObjectPtr<UComboBoxString> AnimClipCombo;
	UPROPERTY() TObjectPtr<UTextBlock> AnimCountText;
	TArray<FAssetData> AnimClipAssets;
	int32 AnimCategoryIndex = 0;

	struct FSliderRow
	{
		USlider* Slider = nullptr;
		UTextBlock* ValueText = nullptr;
	};

	// ---- construction
	UTextBlock* MakeText(const FText& Text, float FontSize, const FLinearColor& Color);
	UBuilderControlBinding* MakeBinding(const FString& Key);
	UHorizontalBox* AddRow(UVerticalBox* Into, const FText& Label, const FString& ResetKey);
	void AddSectionHeader(UVerticalBox* Into, const FText& Text);
	UComboBoxString* AddComboRow(UVerticalBox* Into, const FString& Key, const FText& Label, bool bResettable = true);
	FSliderRow AddSliderRow(UVerticalBox* Into, const FString& Key, const FText& Label, float Min, float Max, bool bResettable = true);
	UCheckBox* AddCheckRow(UVerticalBox* Into, const FString& Key, const FText& Label, bool bResettable = true);
	// A swatch plus R/G/B sliders keyed "<Key>.0/1/2", reset key "<Key>".
	void AddColorRow(UVerticalBox* Into, const FString& Key, const FText& Label);
	void SetColorRow(const FString& Key, const FLinearColor& Color);
	UButton* AddButton(UHorizontalBox* Row, const FString& Key, const FText& Label);
	// Returns the section's content box (children go there); the header is
	// a button that collapses/expands it (key "Section.<Key>").
	UVerticalBox* AddSection(const FString& Key, const FText& Title);

	// ---- state -> controls
	void RefreshFromTarget();
	void RefreshSavedList();
	void RefreshTypeSections();
	void RefreshSingleOptions();
	void RefreshPartCombos();
	void RefreshColors();
	void RefreshFaceControls();
	void RefreshWeaponOptions();
	void RefreshArmPoseOptions();
	void RefreshSliders();

	// ---- reset
	void ResetControl(const FString& Key);

	static void SetCombo(UComboBoxString* Combo, const TArray<FString>& Options, int32 SelectedIndex);
	static void SetSlider(const FSliderRow& Row, float Value, int32 Decimals = 2);
	static void SetSliderCentered(const FSliderRow& Row, float Value, float HalfRange, int32 Decimals = 1);
	static int32 IndexOfPath(const TArray<FAssetData>& Assets, const FString& ObjectPath);

	UCharacterAnimInstance* GetTargetAnimInstance() const;

	UPROPERTY() TObjectPtr<ABaseCharacter> TargetCharacter;
	UPROPERTY() TObjectPtr<ABasePlayerController> OwnerController;
	UPROPERTY() TObjectPtr<UVerticalBox> RootBox;
	// Everything below the manager row; collapsed when nothing is selected.
	UPROPERTY() TObjectPtr<UVerticalBox> BodyBox;

	// Collapsible sections: a clickable header over a content box. The
	// wrapper is what type-dependent show/hide toggles; the content box is
	// what the header collapses.
	struct FSection
	{
		UVerticalBox* Wrapper = nullptr;
		UVerticalBox* Content = nullptr;
		UTextBlock* HeaderText = nullptr;
		FString Title;
		bool bCollapsed = false;
	};
	TMap<FString, FSection> Sections;
	void SetSectionCollapsed(const FString& Key, bool bCollapsed);
	UPROPERTY() TObjectPtr<UTextBlock> TitleText;
	UPROPERTY() TObjectPtr<UTextBlock> ManagerHintText;
	FSliderRow FingerCurlRow;
	FSliderRow ThumbCurlRow;
	// Gait section: one slider per float field of FGaitAdjustments, keyed
	// "Gait.<FieldName>", plus the limp-side combo.
	TMap<FString, FSliderRow> GaitRows;
	UPROPERTY() TObjectPtr<UComboBoxString> LimpSideCombo;
	UPROPERTY() TArray<TObjectPtr<UBuilderControlBinding>> Bindings;

	// Sections that only apply to one character type.
	UPROPERTY() TObjectPtr<UVerticalBox> ModularSection;
	UPROPERTY() TObjectPtr<UVerticalBox> SingleSection;

	UPROPERTY() TObjectPtr<UEditableTextBox> NameBox;
	UPROPERTY() TObjectPtr<UEditableTextBox> TagsBox;
	UPROPERTY() TObjectPtr<UEditableTextBox> DescriptionBox;
	UPROPERTY() TObjectPtr<UEditableTextBox> CommentBox;
	UPROPERTY() TObjectPtr<UComboBoxString> SavedCombo;
	UPROPERTY() TObjectPtr<UComboBoxString> TypeCombo;
	UPROPERTY() TObjectPtr<UComboBoxString> BaseMeshCombo;
	UPROPERTY() TObjectPtr<UComboBoxString> PaletteCombo;
	UPROPERTY() TObjectPtr<UComboBoxString> GenderCombo;
	UPROPERTY() TObjectPtr<UComboBoxString> LocomotionCombo;
	UPROPERTY() TObjectPtr<UComboBoxString> WeaponCombo;
	UPROPERTY() TObjectPtr<UComboBoxString> ArmPoseCombo;
	UPROPERTY() TObjectPtr<UComboBoxString> NoseCombo;
	UPROPERTY() TObjectPtr<UComboBoxString> HairCombo;
	UPROPERTY() TObjectPtr<UComboBoxString> HeadGearCombo;
	UPROPERTY() TObjectPtr<UComboBoxString> ExpressionCombo;
	UPROPERTY() TObjectPtr<UCheckBox> FaceEnabledCheck;
	UPROPERTY() TObjectPtr<UCheckBox> MouthDecalCheck;
	UPROPERTY() TObjectPtr<UCheckBox> AutoBlinkCheck;
	UPROPERTY() TObjectPtr<UTextBlock> StatusText;

	// Slot name -> its combo, and the option list behind it (index-aligned
	// after the leading "None").
	UPROPERTY() TMap<FString, TObjectPtr<UComboBoxString>> PartCombos;
	TMap<FString, TArray<FAssetData>> PartOptions;

	UPROPERTY() TMap<FString, TObjectPtr<UBorder>> ColorSwatches;
	TMap<FString, FSliderRow> ColorSliders;   // "<Param>.0/1/2"

	FSliderRow ScaleRows[3];
	FSliderRow SpeedRow;
	FSliderRow ArmWeightRow;
	FSliderRow WeaponLocRows[3];
	FSliderRow WeaponRotRows[3];
	FSliderRow NoseLocRows[3];
	FSliderRow HatLiftRow;
	FSliderRow HatSizeRow;
	FSliderRow MouthLocRows[3];
	FSliderRow MouthScaleRow;

	TArray<FAssetData> BaseMeshAssets;
	TArray<FAssetData> PaletteAssets;
	TArray<FAssetData> WeaponAssets;
	TArray<FAssetData> NoseAssets;
	TArray<FAssetData> HairAssets;
	TArray<FAssetData> HeadGearAssets;
	TArray<FName> ExpressionNames;
	UPROPERTY() TArray<TObjectPtr<UAnimSequence>> ArmPoseAnims;

	// Guards against the handlers re-applying while RefreshFromTarget is
	// pushing the current state INTO the controls.
	bool bRefreshing = false;
};
