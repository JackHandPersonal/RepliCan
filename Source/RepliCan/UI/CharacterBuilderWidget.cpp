#include "UI/CharacterBuilderWidget.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Animation/AnimSequence.h"
#include "Modules/ModuleManager.h"
#include "UI/CrtStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Slider.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ScrollBox.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/ARFilter.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Styling/CoreStyle.h"
#include "Characters/BaseCharacter.h"
#include "Core/BasePlayerController.h"
#include "Characters/CharacterConfig.h"
#include "Characters/CharacterAnimInstance.h"
#include "Characters/FaceController.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
	constexpr float TitleFontSize = 12.0f;
	constexpr float BodyFontSize = 9.0f;
	constexpr float WeaponHalfRangeCm = 15.0f;
	constexpr float FaceHalfRangeCm = 8.0f;
	constexpr float LabelWidth = 96.0f;
	const FString NoneOption = TEXT("None");

	FString DisplayName(const FAssetData& Data)
	{
		FString Name = Data.AssetName.ToString();
		Name.RemoveFromStart(TEXT("SK_Chr_"));
		return Name;
	}
}

// ---- UBuilderControlBinding ---------------------------------------------

void UBuilderControlBinding::OnCombo(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (Owner.IsValid()) { Owner->HandleCombo(Key, SelectedItem); }
}

void UBuilderControlBinding::OnSlider(float NewValue)
{
	if (Owner.IsValid()) { Owner->HandleSlider(Key, NewValue); }
}

void UBuilderControlBinding::OnButton()
{
	if (Owner.IsValid()) { Owner->HandleButton(Key); }
}

void UBuilderControlBinding::OnCheck(bool bIsChecked)
{
	if (Owner.IsValid()) { Owner->HandleCheck(Key, bIsChecked); }
}

void UBuilderControlBinding::OnTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (Owner.IsValid()) { Owner->HandleText(Key, Text.ToString()); }
}

UWidget* UBuilderControlBinding::GenerateComboItem(FString Item)
{
	return Owner.IsValid() ? Owner->MakeComboItem(Item) : nullptr;
}

// ---- Construction helpers -----------------------------------------------

UCharacterAnimInstance* UCharacterBuilderWidget::GetTargetAnimInstance() const
{
	return TargetCharacter ? TargetCharacter->GetCharacterAnimInstance() : nullptr;
}

UTextBlock* UCharacterBuilderWidget::MakeText(const FText& Text, float FontSize, const FLinearColor& Color)
{
	// (themed: see below)
	UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Block->SetText(Text);
	Block->SetColorAndOpacity(FSlateColor(Color));
	Block->SetFont(Crt::Mono(static_cast<int32>(FontSize)));   // the UI's terminal face
	return Block;
}

UWidget* UCharacterBuilderWidget::MakeComboItem(const FString& Item)
{
	// Dark text: the combo button and its dropdown both use the light
	// default Slate brushes (white text was unreadable on them).
	return MakeText(FText::FromString(Item), BodyFontSize, FLinearColor(0.06f, 0.06f, 0.06f));
}

UBuilderControlBinding* UCharacterBuilderWidget::MakeBinding(const FString& Key)
{
	UBuilderControlBinding* Binding = NewObject<UBuilderControlBinding>(this);
	Binding->Owner = this;
	Binding->Key = Key;
	Bindings.Add(Binding);
	return Binding;
}

UVerticalBox* UCharacterBuilderWidget::AddSection(const FString& Key, const FText& Title)
{
	FSection Section;
	Section.Title = Title.ToString();
	Section.Wrapper = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	BodyBox->AddChildToVerticalBox(Section.Wrapper);

	// Header: a full-width button whose text carries the collapse glyph.
	UButton* Header = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	Header->SetBackgroundColor(FLinearColor(0.35f, 0.3f, 0.18f, 1.0f));
	Section.HeaderText = MakeText(FText::FromString(FString::Chr(0x25BE) + TEXT(" ") + Section.Title), BodyFontSize + 1.0f, FLinearColor::White);
	Header->AddChild(Section.HeaderText);
	Header->OnClicked.AddDynamic(MakeBinding(TEXT("Section.") + Key), &UBuilderControlBinding::OnButton);
	Section.Wrapper->AddChildToVerticalBox(Header)->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 2.0f));

	Section.Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Section.Wrapper->AddChildToVerticalBox(Section.Content);
	Sections.Add(Key, Section);
	return Section.Content;
}

void UCharacterBuilderWidget::SetSectionCollapsed(const FString& Key, bool bCollapsed)
{
	FSection* Section = Sections.Find(Key);
	if (!Section) { return; }
	Section->bCollapsed = bCollapsed;
	Section->Content->SetVisibility(bCollapsed ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	// U+25B8 right-pointing (collapsed), U+25BE down-pointing (expanded).
	Section->HeaderText->SetText(FText::FromString(FString::Chr(bCollapsed ? 0x25B8 : 0x25BE) + TEXT(" ") + Section->Title));
}

void UCharacterBuilderWidget::SetTargetCharacter(ABaseCharacter* InTargetCharacter)
{
	TargetCharacter = InTargetCharacter;
	if (RootBox && IsConstructed()) { RefreshFromTarget(); }
}

void UCharacterBuilderWidget::AddSectionHeader(UVerticalBox* Into, const FText& Text)
{
	UVerticalBoxSlot* SectionSlot = Into->AddChildToVerticalBox(MakeText(Text, BodyFontSize + 1.0f, FLinearColor(0.85f, 0.75f, 0.4f)));
	SectionSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 2.0f));
}

UButton* UCharacterBuilderWidget::AddButton(UHorizontalBox* Row, const FString& Key, const FText& Label)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass()); Button->SetStyle(Crt::BoxedButtonStyle());
	Button->AddChild(MakeText(Label, BodyFontSize, Crt::Green));
	Button->OnClicked.AddDynamic(MakeBinding(Key), &UBuilderControlBinding::OnButton);
	UHorizontalBoxSlot* ButtonSlot = Row->AddChildToHorizontalBox(Button);
	ButtonSlot->SetPadding(FMargin(3.0f, 0.0f));
	ButtonSlot->SetVerticalAlignment(VAlign_Center);
	return Button;
}

// A labeled row; the reset button (when ResetKey is set) is added LAST by
// the callers after their control, so it sits at the right edge.
UHorizontalBox* UCharacterBuilderWidget::AddRow(UVerticalBox* Into, const FText& Label, const FString& ResetKey)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Into->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.0f, 1.0f));
	UTextBlock* LabelText = MakeText(Label, BodyFontSize, FLinearColor::White);
	LabelText->SetMinDesiredWidth(LabelWidth);
	Row->AddChildToHorizontalBox(LabelText)->SetVerticalAlignment(VAlign_Center);
	return Row;
}

namespace
{
	// Small "reset to default" button at the end of a row. Plain glyph
	// rather than an icon asset so it stays pure C++; "0" reads as "zero it".
	void AddResetButtonTo(UCharacterBuilderWidget* Panel, UHorizontalBox* Row, const FString& ResetKey,
		TFunctionRef<UButton*(UHorizontalBox*, const FString&, const FText&)> MakeButton)
	{
		if (ResetKey.IsEmpty()) { return; }
		UButton* Button = MakeButton(Row, TEXT("Reset.") + ResetKey, FText::FromString(FString::Chr(0x21BA)));   // U+21BA anticlockwise arrow
		Button->SetToolTipText(FText::FromString(TEXT("Reset to default")));
	}
}

UComboBoxString* UCharacterBuilderWidget::AddComboRow(UVerticalBox* Into, const FString& Key, const FText& Label, bool bResettable)
{
	UHorizontalBox* Row = AddRow(Into, Label, Key);
	UComboBoxString* Combo = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass());
	UBuilderControlBinding* Binding = MakeBinding(Key);
	Combo->OnGenerateWidgetEvent.BindDynamic(Binding, &UBuilderControlBinding::GenerateComboItem);
	Combo->OnSelectionChanged.AddDynamic(Binding, &UBuilderControlBinding::OnCombo);
	Combo->SetContentPadding(FMargin(4.0f, 1.0f));
	UHorizontalBoxSlot* ComboSlot = Row->AddChildToHorizontalBox(Combo);
	ComboSlot->SetSize(ESlateSizeRule::Fill);
	ComboSlot->SetPadding(FMargin(6.0f, 0.0f, 0.0f, 0.0f));
	if (bResettable) { AddResetButtonTo(this, Row, Key, [this](UHorizontalBox* R, const FString& K, const FText& L) { return AddButton(R, K, L); }); }
	return Combo;
}

UCharacterBuilderWidget::FSliderRow UCharacterBuilderWidget::AddSliderRow(UVerticalBox* Into, const FString& Key, const FText& Label, float Min, float Max, bool bResettable)
{
	UHorizontalBox* Row = AddRow(Into, Label, Key);
	USlider* Slider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass());
	Slider->SetMinValue(Min);
	Slider->SetMaxValue(Max);
	Slider->OnValueChanged.AddDynamic(MakeBinding(Key), &UBuilderControlBinding::OnSlider);
	UHorizontalBoxSlot* SliderSlot = Row->AddChildToHorizontalBox(Slider);
	SliderSlot->SetSize(ESlateSizeRule::Fill);
	SliderSlot->SetPadding(FMargin(6.0f, 0.0f));
	SliderSlot->SetVerticalAlignment(VAlign_Center);

	FSliderRow Result;
	Result.Slider = Slider;
	Result.ValueText = MakeText(FText::GetEmpty(), BodyFontSize, FLinearColor::White);
	Result.ValueText->SetMinDesiredWidth(40.0f);
	Row->AddChildToHorizontalBox(Result.ValueText)->SetVerticalAlignment(VAlign_Center);
	if (bResettable) { AddResetButtonTo(this, Row, Key, [this](UHorizontalBox* R, const FString& K, const FText& L) { return AddButton(R, K, L); }); }
	return Result;
}

UCheckBox* UCharacterBuilderWidget::AddCheckRow(UVerticalBox* Into, const FString& Key, const FText& Label, bool bResettable)
{
	UHorizontalBox* Row = AddRow(Into, Label, Key);
	UCheckBox* Check = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass());
	Check->OnCheckStateChanged.AddDynamic(MakeBinding(Key), &UBuilderControlBinding::OnCheck);
	UHorizontalBoxSlot* CheckSlot = Row->AddChildToHorizontalBox(Check);
	CheckSlot->SetSize(ESlateSizeRule::Fill);
	CheckSlot->SetPadding(FMargin(6.0f, 0.0f));
	CheckSlot->SetVerticalAlignment(VAlign_Center);
	if (bResettable) { AddResetButtonTo(this, Row, Key, [this](UHorizontalBox* R, const FString& K, const FText& L) { return AddButton(R, K, L); }); }
	return Check;
}

void UCharacterBuilderWidget::AddColorRow(UVerticalBox* Into, const FString& RowKey, const FText& Label)
{
	UHorizontalBox* Row = AddRow(Into, Label, RowKey);

	UBorder* Swatch = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Swatch->SetPadding(FMargin(9.0f, 6.0f));
	Swatch->SetContent(MakeText(FText::FromString(TEXT(" ")), BodyFontSize, FLinearColor::White));
	UHorizontalBoxSlot* SwatchSlot = Row->AddChildToHorizontalBox(Swatch);
	SwatchSlot->SetPadding(FMargin(4.0f, 0.0f));
	SwatchSlot->SetVerticalAlignment(VAlign_Center);
	ColorSwatches.Add(RowKey, Swatch);

	static const FLinearColor ChannelTint[3] = { FLinearColor(1, 0.4f, 0.4f), FLinearColor(0.4f, 1, 0.4f), FLinearColor(0.5f, 0.6f, 1) };
	for (int32 Channel = 0; Channel < 3; ++Channel)
	{
		const FString Key = FString::Printf(TEXT("%s.%d"), *RowKey, Channel);
		USlider* Slider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass());
		Slider->SetMinValue(0.0f);
		Slider->SetMaxValue(1.0f);
		Slider->SetSliderBarColor(ChannelTint[Channel]);
		Slider->OnValueChanged.AddDynamic(MakeBinding(Key), &UBuilderControlBinding::OnSlider);
		UHorizontalBoxSlot* SliderSlot = Row->AddChildToHorizontalBox(Slider);
		SliderSlot->SetSize(ESlateSizeRule::Fill);
		SliderSlot->SetPadding(FMargin(3.0f, 0.0f));
		SliderSlot->SetVerticalAlignment(VAlign_Center);
		FSliderRow SliderRow;
		SliderRow.Slider = Slider;
		ColorSliders.Add(Key, SliderRow);
	}
	AddResetButtonTo(this, Row, RowKey, [this](UHorizontalBox* R, const FString& K, const FText& L) { return AddButton(R, K, L); });
}

void UCharacterBuilderWidget::SetColorRow(const FString& RowKey, const FLinearColor& Color)
{
	if (UBorder* Swatch = ColorSwatches.FindRef(RowKey)) { Swatch->SetBrushColor(Color); }
	for (int32 Channel = 0; Channel < 3; ++Channel)
	{
		if (const FSliderRow* Row = ColorSliders.Find(FString::Printf(TEXT("%s.%d"), *RowKey, Channel))) { Row->Slider->SetValue(Color.Component(Channel)); }
	}
}

// ---- Layout ----------------------------------------------------------------

void UCharacterBuilderWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Background"));
	Background->SetBrushColor(Crt::Panel);
	Background->SetPadding(FMargin(10.0f));
	WidgetTree->RootWidget = Background;

	UScrollBox* Scroll = Crt::ScrollBox(WidgetTree);
	Background->SetContent(Scroll);

	RootBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RootBox"));
	Scroll->AddChild(RootBox);

	// -- Top row: Back sits above everything else (Edit Tool for the
	// Character Manager, Character Manager for the Face Manager).
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		RootBox->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
		const bool bSubPage = bFaceManager || bAnimBrowser;
		AddButton(Row, bSubPage ? TEXT("BackToManager") : TEXT("Back"), FText::FromString(TEXT("< Back")));
		AddButton(Row, TEXT("SaveClose"), FText::FromString(TEXT("Save & Close")));
		UHorizontalBoxSlot* TitleSlot = Row->AddChildToHorizontalBox(TitleText = MakeText(FText::FromString(bFaceManager ? TEXT("Face Manager") : bAnimBrowser ? TEXT("Animation Browser") : TEXT("Character Manager")), TitleFontSize, FLinearColor::White));
		TitleSlot->SetPadding(FMargin(10.0f, 0.0f));
		TitleSlot->SetVerticalAlignment(VAlign_Center);
	}

	// -- Manager row: place a new character / control / remove the selected
	// one / open its face. The Face Manager only carries the hint text.
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		RootBox->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 2.0f));
		if (!bFaceManager && !bAnimBrowser)
		{
			AddButton(Row, TEXT("New"), FText::FromString(TEXT("New")));
			AddButton(Row, TEXT("Control"), FText::FromString(TEXT("Control")));
			AddButton(Row, TEXT("Remove"), FText::FromString(TEXT("Remove")));
		}
		ManagerHintText = MakeText(FText::GetEmpty(), BodyFontSize, FLinearColor(0.7f, 0.7f, 0.7f));
		UHorizontalBoxSlot* HintSlot = Row->AddChildToHorizontalBox(ManagerHintText);
		HintSlot->SetPadding(FMargin(8.0f, 0.0f));
		HintSlot->SetVerticalAlignment(VAlign_Center);
	}

	BodyBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BodyBox"));
	RootBox->AddChildToVerticalBox(BodyBox);

	if (bFaceManager)
	{
		BuildFaceSection();
		return;
	}
	if (bAnimBrowser)
	{
		BuildAnimBrowserSection();
		return;
	}

	// -- Character: name / file / type / scale / speed
	UVerticalBox* Character = AddSection(TEXT("Character"), FText::FromString(TEXT("Character")));
	{
		UHorizontalBox* Row = AddRow(Character, FText::FromString(TEXT("Name")), FString());
		NameBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
		FEditableTextBoxStyle Style = NameBox->GetWidgetStyle();
		Style.SetFont(Crt::Mono(static_cast<int32>(BodyFontSize)));
		Style.SetForegroundColor(FSlateColor(FLinearColor(0.06f, 0.06f, 0.06f)));
		Style.SetBackgroundColor(FSlateColor(FLinearColor(0.88f, 0.88f, 0.88f)));
		Style.TextStyle.SetSelectedBackgroundColor(FSlateColor(FLinearColor(0.45f, 0.85f, 0.55f)));
		NameBox->SetWidgetStyle(Style);
		NameBox->OnTextCommitted.AddDynamic(MakeBinding(TEXT("Name")), &UBuilderControlBinding::OnTextCommitted);
		UHorizontalBoxSlot* NameSlot = Row->AddChildToHorizontalBox(NameBox);
		NameSlot->SetSize(ESlateSizeRule::Fill);
		NameSlot->SetPadding(FMargin(6.0f, 0.0f));
		AddButton(Row, TEXT("Save"), FText::FromString(TEXT("Save")));
	}
	{
		UHorizontalBox* Row = AddRow(Character, FText::FromString(TEXT("Tags")), FString());
		TagsBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
		FEditableTextBoxStyle Style = TagsBox->GetWidgetStyle();
		Style.SetFont(Crt::Mono(static_cast<int32>(BodyFontSize)));
		Style.SetForegroundColor(FSlateColor(FLinearColor(0.06f, 0.06f, 0.06f)));
		Style.SetBackgroundColor(FSlateColor(FLinearColor(0.88f, 0.88f, 0.88f)));
		TagsBox->SetWidgetStyle(Style);
		TagsBox->SetHintText(FText::FromString(TEXT("comma, separated, tags")));
		TagsBox->OnTextCommitted.AddDynamic(MakeBinding(TEXT("Tags")), &UBuilderControlBinding::OnTextCommitted);
		UHorizontalBoxSlot* TagsSlot = Row->AddChildToHorizontalBox(TagsBox);
		TagsSlot->SetSize(ESlateSizeRule::Fill);
		TagsSlot->SetPadding(FMargin(6.0f, 0.0f));
	}
	// Inspect-menu text: the line under the name, and the stub Talk remark.
	for (const TPair<FString, FString>& Def : TArray<TPair<FString, FString>>{ { TEXT("Description"), TEXT("Description") }, { TEXT("Comment"), TEXT("Talk remark") } })
	{
		UHorizontalBox* Row = AddRow(Character, FText::FromString(Def.Value), FString());
		UEditableTextBox* Box = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
		FEditableTextBoxStyle Style = Box->GetWidgetStyle();
		Style.SetFont(Crt::Mono(static_cast<int32>(BodyFontSize)));
		Style.SetForegroundColor(FSlateColor(FLinearColor(0.06f, 0.06f, 0.06f)));
		Style.SetBackgroundColor(FSlateColor(FLinearColor(0.88f, 0.88f, 0.88f)));
		Box->SetWidgetStyle(Style);
		Box->SetHintText(FText::FromString(Def.Key == TEXT("Comment") ? TEXT("what they say when talked to") : TEXT("shown under the name when inspected")));
		Box->OnTextCommitted.AddDynamic(MakeBinding(Def.Key), &UBuilderControlBinding::OnTextCommitted);
		UHorizontalBoxSlot* BoxSlot = Row->AddChildToHorizontalBox(Box);
		BoxSlot->SetSize(ESlateSizeRule::Fill);
		BoxSlot->SetPadding(FMargin(6.0f, 0.0f));
		if (Def.Key == TEXT("Comment")) { CommentBox = Box; } else { DescriptionBox = Box; }
	}
	{
		UHorizontalBox* Row = AddRow(Character, FText::FromString(TEXT("Saved")), FString());
		SavedCombo = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass());
		UBuilderControlBinding* Binding = MakeBinding(TEXT("Saved"));
		SavedCombo->OnGenerateWidgetEvent.BindDynamic(Binding, &UBuilderControlBinding::GenerateComboItem);
		SavedCombo->OnSelectionChanged.AddDynamic(Binding, &UBuilderControlBinding::OnCombo);
		SavedCombo->SetContentPadding(FMargin(4.0f, 1.0f));
		UHorizontalBoxSlot* ComboSlot = Row->AddChildToHorizontalBox(SavedCombo);
		ComboSlot->SetSize(ESlateSizeRule::Fill);
		ComboSlot->SetPadding(FMargin(6.0f, 0.0f));
		AddButton(Row, TEXT("Load"), FText::FromString(TEXT("Load")));
	}
	StatusText = MakeText(FText::GetEmpty(), BodyFontSize, FLinearColor(0.6f, 0.9f, 0.6f));
	Character->AddChildToVerticalBox(StatusText);

	TypeCombo = AddComboRow(Character, TEXT("Type"), FText::FromString(TEXT("Kind")), false);
	ScaleRows[0] = AddSliderRow(Character, TEXT("Scale.0"), FText::FromString(TEXT("Scale X (width)")), 0.5f, 2.0f);
	ScaleRows[1] = AddSliderRow(Character, TEXT("Scale.1"), FText::FromString(TEXT("Scale Y (depth)")), 0.5f, 2.0f);
	ScaleRows[2] = AddSliderRow(Character, TEXT("Scale.2"), FText::FromString(TEXT("Scale Z (height)")), 0.5f, 2.0f);
	SpeedRow = AddSliderRow(Character, TEXT("Speed"), FText::FromString(TEXT("Speed x")), 0.25f, 3.0f);
	LocomotionCombo = AddComboRow(Character, TEXT("Locomotion"), FText::FromString(TEXT("Locomotion")));

	// -- Single-mesh body
	UVerticalBox* Single = AddSection(TEXT("Single"), FText::FromString(TEXT("Body (Synty character)")));
	SingleSection = Sections[TEXT("Single")].Wrapper;
	BaseMeshCombo = AddComboRow(Single, TEXT("BaseMesh"), FText::FromString(TEXT("Mesh")));
	PaletteCombo = AddComboRow(Single, TEXT("Palette"), FText::FromString(TEXT("Palette")));

	// -- Modular body (parts) and its colors, as two collapsible sections
	// inside one type-dependent wrapper.
	ModularSection = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	BodyBox->AddChildToVerticalBox(ModularSection);
	{
		// AddSection appends to BodyBox; re-parent the two wrappers under
		// ModularSection so the type switch hides both at once.
		UVerticalBox* Parts = AddSection(TEXT("Parts"), FText::FromString(TEXT("Body (Modular Fantasy Hero)")));
		UVerticalBox* Colors = AddSection(TEXT("Colors"), FText::FromString(TEXT("Colors")));
		for (const TCHAR* Key : { TEXT("Parts"), TEXT("Colors") })
		{
			UVerticalBox* Wrapper = Sections[Key].Wrapper;
			Wrapper->RemoveFromParent();
			ModularSection->AddChildToVerticalBox(Wrapper);
		}
		GenderCombo = AddComboRow(Parts, TEXT("Gender"), FText::FromString(TEXT("Gender")));
		for (const ModularHero::FSlotDef& SlotDef : ModularHero::Slots())
		{
			PartCombos.Add(SlotDef.Slot, AddComboRow(Parts, TEXT("Part.") + SlotDef.Slot, FText::FromString(SlotDef.Slot)));
		}
		// The sci-fi cut library (Kit "SciFi"): any pack's part in any slot.
		for (const FString& CutSlot : CutLibrary::Slots())
		{
			PartCombos.Add(CutSlot, AddComboRow(Parts, TEXT("Part.") + CutSlot, FText::FromString(TEXT("SciFi ") + CutSlot.RightChop(3))));
		}
		for (const FString& Parameter : ModularHero::ColorParameters())
		{
			FString Label = Parameter;
			Label.RemoveFromStart(TEXT("Color_"));
			Label.ReplaceInline(TEXT("_"), TEXT(" "));
			AddColorRow(Colors, TEXT("Color.") + Parameter, FText::FromString(Label));
		}
	}

	// -- Face: just a button (the controls live on the Face Manager page).
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		BodyBox->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 2.0f));
		AddButton(Row, TEXT("Face"), FText::FromString(TEXT("Face...")));
		AddButton(Row, TEXT("Anims"), FText::FromString(TEXT("Animations...")));
		UHorizontalBoxSlot* HintSlot = Row->AddChildToHorizontalBox(MakeText(FText::FromString(TEXT("face systems / try out clips")), BodyFontSize, FLinearColor(0.7f, 0.7f, 0.7f)));
		HintSlot->SetPadding(FMargin(8.0f, 0.0f));
		HintSlot->SetVerticalAlignment(VAlign_Center);
	}

	// -- Weapon
	UVerticalBox* Weapon = AddSection(TEXT("Weapon"), FText::FromString(TEXT("Weapon")));
	WeaponCombo = AddComboRow(Weapon, TEXT("Weapon"), FText::FromString(TEXT("Mesh")));
	ArmPoseCombo = AddComboRow(Weapon, TEXT("ArmPose"), FText::FromString(TEXT("Arm pose")));
	ArmWeightRow = AddSliderRow(Weapon, TEXT("ArmWeight"), FText::FromString(TEXT("Arm weight")), 0.0f, 1.0f);
	FingerCurlRow = AddSliderRow(Weapon, TEXT("FingerCurl"), FText::FromString(TEXT("Finger curl")), -120.0f, 120.0f);
	ThumbCurlRow = AddSliderRow(Weapon, TEXT("ThumbCurl"), FText::FromString(TEXT("Thumb curl")), -120.0f, 120.0f);
	static const TCHAR* OffsetLabels[3] = { TEXT("Offset X"), TEXT("Offset Y"), TEXT("Offset Z") };
	static const TCHAR* RotLabels[3] = { TEXT("Pitch"), TEXT("Yaw"), TEXT("Roll") };
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		WeaponLocRows[Axis] = AddSliderRow(Weapon, FString::Printf(TEXT("WeaponLoc.%d"), Axis), FText::FromString(OffsetLabels[Axis]), -WeaponHalfRangeCm, WeaponHalfRangeCm);
	}
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		WeaponRotRows[Axis] = AddSliderRow(Weapon, FString::Printf(TEXT("WeaponRot.%d"), Axis), FText::FromString(RotLabels[Axis]), -180.0f, 180.0f);
	}

	// -- Gait (runtime posture/timing layer, see GaitAdjustments.h)
	UVerticalBox* Gait = AddSection(TEXT("Gait"), FText::FromString(TEXT("Gait")));
	struct FGaitRowDef { const TCHAR* Field; const TCHAR* Label; float Min; float Max; };
	static const FGaitRowDef GaitRowDefs[] = {
		{ TEXT("StanceWidthDegrees"), TEXT("Stance width"), -10.0f, 25.0f },
		{ TEXT("MovingStanceWidthDegrees"), TEXT("Stride width"), -10.0f, 25.0f },
		{ TEXT("ToeOutDegrees"),      TEXT("Toe out"),      -20.0f, 30.0f },
		{ TEXT("HunchDegrees"),       TEXT("Hunch"),        -15.0f, 40.0f },
		{ TEXT("LeanDegrees"),        TEXT("Lean"),         -20.0f, 20.0f },
		{ TEXT("ArmSwingScale"),      TEXT("Arm swing"),      0.0f,  2.0f },
		{ TEXT("BounceScale"),        TEXT("Bounce"),         0.0f,  3.0f },
		{ TEXT("Sway"),               TEXT("Hip sway"),       0.0f,  1.0f },
		{ TEXT("CadenceScale"),       TEXT("Cadence"),        0.5f,  2.0f },
		{ TEXT("Stumble"),            TEXT("Stumble"),        0.0f,  1.0f },
		{ TEXT("LimpAmount"),         TEXT("Limp"),           0.0f,  1.0f },
	};
	for (const FGaitRowDef& Def : GaitRowDefs)
	{
		GaitRows.Add(Def.Field, AddSliderRow(Gait, FString(TEXT("Gait.")) + Def.Field, FText::FromString(Def.Label), Def.Min, Def.Max));
	}
	LimpSideCombo = AddComboRow(Gait, TEXT("Gait.LimpSide"), FText::FromString(TEXT("Limp side")));
}

void UCharacterBuilderWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshFromTarget();
}

void UCharacterBuilderWidget::BuildFaceSection()
{
	// The face systems (see FCharacterFaceConfig) -- the Face Manager's
	// only content.
	UVerticalBox* Face = AddSection(TEXT("Face"), FText::FromString(TEXT("Face (nose, mouth, eyes)")));
	FaceEnabledCheck = AddCheckRow(Face, TEXT("Face.Enabled"), FText::FromString(TEXT("Enabled")));
	NoseCombo = AddComboRow(Face, TEXT("Face.Nose"), FText::FromString(TEXT("Nose mesh")));
	HairCombo = AddComboRow(Face, TEXT("Face.Hair"), FText::FromString(TEXT("Hair")));
	HeadGearCombo = AddComboRow(Face, TEXT("Face.HeadGear"), FText::FromString(TEXT("Hat")));
	HatLiftRow = AddSliderRow(Face, TEXT("Face.HatLift"), FText::FromString(TEXT("Hat lift (cm)")), -10.0f, 20.0f);
	HatSizeRow = AddSliderRow(Face, TEXT("Face.HatSize"), FText::FromString(TEXT("Hat size")), 0.7f, 1.4f);
	static const TCHAR* AxisNames[3] = { TEXT("X"), TEXT("Y"), TEXT("Z") };
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		NoseLocRows[Axis] = AddSliderRow(Face, FString::Printf(TEXT("Face.NoseLoc.%d"), Axis), FText::FromString(FString::Printf(TEXT("Nose %s"), AxisNames[Axis])), -FaceHalfRangeCm, FaceHalfRangeCm);
	}
	MouthDecalCheck = AddCheckRow(Face, TEXT("Face.Mouth"), FText::FromString(TEXT("Mouth decal")));
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		MouthLocRows[Axis] = AddSliderRow(Face, FString::Printf(TEXT("Face.MouthLoc.%d"), Axis), FText::FromString(FString::Printf(TEXT("Mouth %s"), AxisNames[Axis])), -FaceHalfRangeCm, FaceHalfRangeCm);
	}
	MouthScaleRow = AddSliderRow(Face, TEXT("Face.MouthScale"), FText::FromString(TEXT("Mouth scale")), 0.25f, 1.5f);
	AddColorRow(Face, TEXT("Face.MouthTint"), FText::FromString(TEXT("Mouth tint")));
	ExpressionCombo = AddComboRow(Face, TEXT("Face.Expression"), FText::FromString(TEXT("Expression")));
	AutoBlinkCheck = AddCheckRow(Face, TEXT("Face.Blink"), FText::FromString(TEXT("Auto blink")));
}

namespace
{
	// The Animation Browser's categories: label -> content folders scanned
	// (recursively) for AnimSequences.
	struct FAnimCategory { const TCHAR* Label; TArray<FString> Paths; };
	const TArray<FAnimCategory>& AnimCategories()
	{
		static const TArray<FAnimCategory> Categories = {
			{ TEXT("Idles (Synty Idles pack)"), { TEXT("/Game/Characters/Animations/SyntyIdles") } },
			{ TEXT("Emotes & taunts"),          { TEXT("/Game/Characters/Animations/SyntyEmotes") } },
			{ TEXT("Lyra: pistol"),             { TEXT("/Game/Characters/Animations/Lyra/Pistol") } },
			{ TEXT("Lyra: rifle"),              { TEXT("/Game/Characters/Animations/Lyra/Rifle") } },
			{ TEXT("Lyra: shotgun"),            { TEXT("/Game/Characters/Animations/Lyra/shotgun") } },
			{ TEXT("Lyra: hits & deaths"),      { TEXT("/Game/Characters/Animations/Lyra/HitReactions"), TEXT("/Game/Characters/Animations/Lyra/death") } },
			{ TEXT("Lyra: bench (sitting)"),    { TEXT("/Game/Characters/Animations/Lyra/Bench") } },
			{ TEXT("Lyra: emotes"),             { TEXT("/Game/Characters/Animations/Lyra/Emotes") } },
			{ TEXT("Locomotion: Male"),         { TEXT("/Game/Characters/Animations/SyntyBaseLocomotion/Masculine") } },
			{ TEXT("Locomotion: Female"),       { TEXT("/Game/Characters/Animations/SyntyBaseLocomotion/Feminine") } },
			{ TEXT("Dash / dodge / roll"),      { TEXT("/Game/Characters/Animations/Dash"), TEXT("/Game/Characters/Animations/DashDodgeRoll") } },
			{ TEXT("Falling"),                  { TEXT("/Game/Characters/Animations/DynamicFalling") } },
			{ TEXT("Idle variety (Lyra)"),      { TEXT("/Game/Characters/Animations/IdleVariety") } },
			{ TEXT("Poses"),                    { TEXT("/Game/Characters/Animations/Poses") } },
		};
		return Categories;
	}
}

void UCharacterBuilderWidget::BuildAnimBrowserSection()
{
	// Plain rows straight into the body -- no collapsible section here.
	UVerticalBox* Box = BodyBox;
	AnimCategoryCombo = AddComboRow(Box, TEXT("Anim.Category"), FText::FromString(TEXT("Category")), false);
	AnimClipCombo = AddComboRow(Box, TEXT("Anim.Clip"), FText::FromString(TEXT("Clip")), false);
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Box->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.0f, 4.0f));
		AddButton(Row, TEXT("Anim.Play"), FText::FromString(TEXT("Play once")));
		AddButton(Row, TEXT("Anim.Loop"), FText::FromString(TEXT("Loop")));
		AddButton(Row, TEXT("Anim.Stop"), FText::FromString(TEXT("Stop")));
		AnimCountText = MakeText(FText::GetEmpty(), BodyFontSize, FLinearColor(0.7f, 0.7f, 0.7f));
		UHorizontalBoxSlot* CountSlot = Row->AddChildToHorizontalBox(AnimCountText);
		CountSlot->SetPadding(FMargin(8.0f, 0.0f));
		CountSlot->SetVerticalAlignment(VAlign_Center);
	}
	TArray<FString> Labels;
	for (const FAnimCategory& Category : AnimCategories()) { Labels.Add(Category.Label); }
	{
		TGuardValue<bool> RefreshGuard(bRefreshing, true);
		SetCombo(AnimCategoryCombo, Labels, 0);
	}
}

void UCharacterBuilderWidget::RefreshAnimClips()
{
	if (!AnimClipCombo) { return; }
	AnimClipAssets.Reset();
	const TArray<FAnimCategory>& Categories = AnimCategories();
	if (Categories.IsValidIndex(AnimCategoryIndex))
	{
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		for (const FString& Path : Categories[AnimCategoryIndex].Paths)
		{
			TArray<FAssetData> Found;
			Registry.GetAssetsByPath(FName(*Path), Found, /*bRecursive=*/true);
			for (const FAssetData& Data : Found)
			{
				if (Data.AssetClassPath == UAnimSequence::StaticClass()->GetClassPathName()) { AnimClipAssets.Add(Data); }
			}
		}
		AnimClipAssets.Sort([](const FAssetData& A, const FAssetData& B) { return A.AssetName.LexicalLess(B.AssetName); });
	}
	TArray<FString> Names;
	for (const FAssetData& Data : AnimClipAssets) { Names.Add(Data.AssetName.ToString()); }
	if (Names.Num() == 0) { Names.Add(TEXT("(no clips found)")); }
	TGuardValue<bool> RefreshGuard(bRefreshing, true);
	SetCombo(AnimClipCombo, Names, 0);
	if (AnimCountText) { AnimCountText->SetText(FText::FromString(FString::Printf(TEXT("%d clips"), AnimClipAssets.Num()))); }
}

// ---- Refresh (state -> controls) ------------------------------------------

void UCharacterBuilderWidget::SetCombo(UComboBoxString* Combo, const TArray<FString>& Options, int32 SelectedIndex)
{
	Combo->ClearOptions();
	for (const FString& Option : Options) { Combo->AddOption(Option); }
	if (Options.Num() > 0) { Combo->SetSelectedIndex(FMath::Clamp(SelectedIndex, 0, Options.Num() - 1)); }
}

void UCharacterBuilderWidget::SetSlider(const FSliderRow& Row, float Value, int32 Decimals)
{
	Row.Slider->SetValue(Value);
	if (Row.ValueText) { Row.ValueText->SetText(FText::FromString(FString::Printf(TEXT("%.*f"), Decimals, Value))); }
}

void UCharacterBuilderWidget::SetSliderCentered(const FSliderRow& Row, float Value, float HalfRange, int32 Decimals)
{
	// Offsets are nudged relative to wherever they are, so the range
	// re-centers on the current value each time the panel opens.
	Row.Slider->SetMinValue(Value - HalfRange);
	Row.Slider->SetMaxValue(Value + HalfRange);
	SetSlider(Row, Value, Decimals);
}

int32 UCharacterBuilderWidget::IndexOfPath(const TArray<FAssetData>& Assets, const FString& ObjectPath)
{
	return Assets.IndexOfByPredicate([&ObjectPath](const FAssetData& D) { return D.GetObjectPathString() == ObjectPath; });
}

void UCharacterBuilderWidget::RefreshFromTarget()
{
	// The combo/slider "set" calls below fire the same delegates a user
	// interaction would; bRefreshing makes the handlers ignore those so
	// pushing state INTO the controls never re-applies it.
	TGuardValue<bool> RefreshGuard(bRefreshing, true);
	BodyBox->SetVisibility(TargetCharacter ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	const TCHAR* PageTitle = bFaceManager ? TEXT("Face Manager") : bAnimBrowser ? TEXT("Animation Browser") : TEXT("Character Manager");
	if (!TargetCharacter)
	{
		TitleText->SetText(FText::FromString(PageTitle));
		ManagerHintText->SetText(FText::FromString(TEXT("Nothing selected.")));
		return;
	}

	const FCharacterConfig& Config = TargetCharacter->GetCharacterConfig();
	TitleText->SetText(FText::FromString(FString::Printf(TEXT("%s  -  %s"), PageTitle, *Config.Name)));
	ManagerHintText->SetText(FText::GetEmpty());
	if (bFaceManager)
	{
		RefreshFaceControls();
		return;
	}
	if (bAnimBrowser)
	{
		RefreshAnimClips();
		return;
	}
	NameBox->SetText(FText::FromString(Config.Name));
	if (TagsBox) { TagsBox->SetText(FText::FromString(TargetCharacter ? TargetCharacter->GetCharacterTagsString() : FString())); }
	if (DescriptionBox) { DescriptionBox->SetText(FText::FromString(Config.Description)); }
	if (CommentBox) { CommentBox->SetText(FText::FromString(Config.Comment)); }
	RefreshSavedList();
	RefreshTypeSections();
	RefreshSingleOptions();
	RefreshPartCombos();
	RefreshColors();
	RefreshWeaponOptions();
	RefreshArmPoseOptions();
	RefreshSliders();
}

void UCharacterBuilderWidget::RefreshSavedList()
{
	const TArray<FString> Names = CharacterConfigFile::List();
	const int32 Current = TargetCharacter ? Names.IndexOfByKey(TargetCharacter->GetCharacterConfig().Name) : INDEX_NONE;
	if (Names.Num() == 0)
	{
		SetCombo(SavedCombo, { TEXT("(nothing saved yet)") }, 0);
	}
	else
	{
		SetCombo(SavedCombo, Names, FMath::Max(Current, 0));
	}
}

void UCharacterBuilderWidget::RefreshTypeSections()
{
	const bool bModular = TargetCharacter->GetCharacterConfig().Type == CharacterType::Modular;
	SetCombo(TypeCombo, { TEXT("Modular Fantasy Hero"), TEXT("Synty character (single mesh)") }, bModular ? 0 : 1);
	ModularSection->SetVisibility(bModular ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	SingleSection->SetVisibility(bModular ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
}

void UCharacterBuilderWidget::RefreshSingleOptions()
{
	const FCharacterConfig& Config = TargetCharacter->GetCharacterConfig();
	BaseMeshAssets = SyntyCharacters::BaseMeshOptions();
	TArray<FString> Names;
	for (const FAssetData& Data : BaseMeshAssets) { Names.Add(DisplayName(Data)); }
	if (Names.Num() == 0) { Names.Add(TEXT("(no SK_Chr_* meshes found)")); }
	SetCombo(BaseMeshCombo, Names, FMath::Max(IndexOfPath(BaseMeshAssets, Config.BaseMesh), 0));

	// Palette family follows the mesh currently shown.
	const USkeletalMesh* Mesh = TargetCharacter->GetMesh()->GetSkeletalMeshAsset();
	PaletteAssets = (Config.Type == CharacterType::Single) ? SyntyCharacters::PaletteOptions(Mesh) : TArray<FAssetData>();
	TArray<FString> PaletteNames;
	PaletteNames.Add(TEXT("(mesh default)"));
	for (const FAssetData& Data : PaletteAssets) { PaletteNames.Add(Data.AssetName.ToString()); }
	SetCombo(PaletteCombo, PaletteNames, IndexOfPath(PaletteAssets, Config.Material) + 1);
}

void UCharacterBuilderWidget::RefreshPartCombos()
{
	const FCharacterConfig& Config = TargetCharacter->GetCharacterConfig();
	SetCombo(GenderCombo, { TEXT("Male"), TEXT("Female") }, Config.Gender == TEXT("Female") ? 1 : 0);
	for (const ModularHero::FSlotDef& SlotDef : ModularHero::Slots())
	{
		TArray<FAssetData>& Options = PartOptions.FindOrAdd(SlotDef.Slot);
		Options = ModularHero::PartOptions(SlotDef, Config.Gender);

		TArray<FString> Names;
		Names.Add(NoneOption);
		for (const FAssetData& Data : Options) { Names.Add(DisplayName(Data)); }
		const FString* CurrentPath = Config.Parts.Find(SlotDef.Slot);
		SetCombo(PartCombos[SlotDef.Slot], Names, CurrentPath ? IndexOfPath(Options, *CurrentPath) + 1 : 0);
	}
	for (const FString& CutSlot : CutLibrary::Slots())
	{
		TArray<FAssetData>& Options = PartOptions.FindOrAdd(CutSlot);
		Options = CutLibrary::PartOptions(CutSlot);
		TArray<FString> Names;
		Names.Add(NoneOption);
		for (const FAssetData& Data : Options) { Names.Add(DisplayName(Data)); }
		const FString* CurrentPath = Config.Parts.Find(CutSlot);
		SetCombo(PartCombos[CutSlot], Names, CurrentPath ? IndexOfPath(Options, *CurrentPath) + 1 : 0);
	}
}

void UCharacterBuilderWidget::RefreshColors()
{
	for (const FString& Parameter : ModularHero::ColorParameters())
	{
		SetColorRow(TEXT("Color.") + Parameter, TargetCharacter->GetPartColor(Parameter));
	}
}

void UCharacterBuilderWidget::RefreshFaceControls()
{
	const FCharacterFaceConfig& Face = TargetCharacter->GetCharacterConfig().Face;
	FaceEnabledCheck->SetIsChecked(Face.bEnabled);
	MouthDecalCheck->SetIsChecked(Face.bMouthDecal);
	AutoBlinkCheck->SetIsChecked(Face.bAutoBlink);

	NoseAssets = SyntyCharacters::NoseOptions();
	TArray<FString> NoseNames;
	NoseNames.Add(NoneOption);
	for (const FAssetData& Data : NoseAssets) { NoseNames.Add(Data.AssetName.ToString()); }
	SetCombo(NoseCombo, NoseNames, IndexOfPath(NoseAssets, Face.NoseMesh) + 1);

	HairAssets = SyntyCharacters::HairOptions();
	TArray<FString> HairNames;
	HairNames.Add(NoneOption);
	for (const FAssetData& Data : HairAssets) { HairNames.Add(Data.AssetName.ToString()); }
	SetCombo(HairCombo, HairNames, IndexOfPath(HairAssets, Face.HairMesh) + 1);
	HeadGearAssets = SyntyCharacters::HeadGearOptions();
	TArray<FString> HeadGearNames;
	HeadGearNames.Add(NoneOption);
	for (const FAssetData& Data : HeadGearAssets) { HeadGearNames.Add(Data.AssetName.ToString()); }
	SetCombo(HeadGearCombo, HeadGearNames, IndexOfPath(HeadGearAssets, Face.HeadGearMesh) + 1);
	HeadGearCombo->SetIsEnabled(TargetCharacter->GetCharacterConfig().Type != CharacterType::Modular);
	SetSlider(HatLiftRow, static_cast<float>(Face.HeadGearLocation.Z), 1);
	SetSlider(HatSizeRow, Face.HeadGearScale, 2);
	HairCombo->SetIsEnabled(TargetCharacter->GetCharacterConfig().Type != CharacterType::Modular);

	ExpressionNames.Reset();
	if (const AFaceController* Controller = TargetCharacter->GetFaceController())
	{
		ExpressionNames = Controller->GetMouthStateNames();
		ExpressionNames.Sort([](const FName& A, const FName& B) { return A.ToString() < B.ToString(); });
	}
	TArray<FString> ExpressionStrings;
	for (const FName& Name : ExpressionNames) { ExpressionStrings.Add(Name.ToString()); }
	if (ExpressionStrings.Num() == 0) { ExpressionStrings.Add(Face.Expression.IsEmpty() ? TEXT("Neutral") : Face.Expression); }
	SetCombo(ExpressionCombo, ExpressionStrings, FMath::Max(ExpressionStrings.IndexOfByKey(Face.Expression), 0));

	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		SetSliderCentered(NoseLocRows[Axis], static_cast<float>(Face.NoseLocation[Axis]), FaceHalfRangeCm);
		SetSliderCentered(MouthLocRows[Axis], static_cast<float>(Face.MouthLocation[Axis]), FaceHalfRangeCm);
	}
	SetSlider(MouthScaleRow, Face.MouthScale);
	SetColorRow(TEXT("Face.MouthTint"), Face.MouthTint);
}

void UCharacterBuilderWidget::RefreshWeaponOptions()
{
	if (WeaponAssets.Num() == 0)
	{
		// Every *_Wep_* static mesh in the project -- the two Dungeon/War
		// Camp packs use SM_Wep_, the Modular Hero pack's StaticMeshes folder
		// uses SK_Wep_ for its static variants. Scanned once.
		const IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		FARFilter Filter;
		Filter.ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());
		Filter.PackagePaths.Add(TEXT("/Game/RepliCan/Weapons"));   // the baked, hand-held set (Tools: scratchpad convert_all_weapons.py); the packs' loose parts stay out
		Filter.bRecursivePaths = true;
		TArray<FAssetData> AllStaticMeshes;
		AssetRegistry.GetAssets(Filter, AllStaticMeshes);
		for (const FAssetData& Data : AllStaticMeshes)
		{
			if (Data.AssetName.ToString().Contains(TEXT("_Wep_"))) { WeaponAssets.Add(Data); }
		}
		// String order, not FName::LexicalLess -- see ModularHero::PartOptions.
		WeaponAssets.Sort([](const FAssetData& A, const FAssetData& B) { return A.AssetName.ToString() < B.AssetName.ToString(); });
	}

	TArray<FString> Names;
	Names.Add(NoneOption);
	for (const FAssetData& Data : WeaponAssets) { Names.Add(Data.AssetName.ToString()); }
	SetCombo(WeaponCombo, Names, IndexOfPath(WeaponAssets, TargetCharacter->GetCharacterConfig().WeaponMesh) + 1);
}

void UCharacterBuilderWidget::RefreshArmPoseOptions()
{
	ArmPoseAnims.Reset();
	UCharacterAnimInstance* AnimInst = GetTargetAnimInstance();
	UAnimSequence* Current = AnimInst ? AnimInst->ArmOverridePose.Get() : nullptr;
	if (AnimInst)
	{
		// The Sword Combat set's idles are the natural "carrying a sword" arm
		// poses; the full 118-clip set would be noise here.
		for (int32 i = 0; i < AnimInst->GetNumAllCombatAnims(); ++i)
		{
			UAnimSequence* Anim = AnimInst->GetAllCombatAnim(i);
			if (Anim && Anim->GetName().Contains(TEXT("Idle"))) { ArmPoseAnims.Add(Anim); }
		}
	}
	if (Current && !ArmPoseAnims.Contains(Current)) { ArmPoseAnims.Insert(Current, 0); }

	TArray<FString> Names;
	Names.Add(NoneOption);
	int32 Selected = 0;
	for (int32 i = 0; i < ArmPoseAnims.Num(); ++i)
	{
		Names.Add(ArmPoseAnims[i]->GetName());
		if (ArmPoseAnims[i] == Current) { Selected = i + 1; }
	}
	SetCombo(ArmPoseCombo, Names, Selected);
}

void UCharacterBuilderWidget::RefreshSliders()
{
	const FCharacterConfig& Config = TargetCharacter->GetCharacterConfig();
	for (int32 Axis = 0; Axis < 3; ++Axis) { SetSlider(ScaleRows[Axis], static_cast<float>(Config.Scale[Axis])); }
	SetSlider(SpeedRow, Config.SpeedMultiplier);
	{
		static const TArray<FString> Sets = { TEXT("Male"), TEXT("Female"), TEXT("Goblin") };
		SetCombo(LocomotionCombo, Sets, FMath::Max(Sets.IndexOfByKey(TargetCharacter->GetLocomotionSetName()), 0));
	}
	SetSlider(ArmWeightRow, Config.ArmPoseWeight);
	SetSlider(FingerCurlRow, Config.FingerCurlDegrees, 0);
	SetSlider(ThumbCurlRow, Config.ThumbCurlDegrees, 0);
	for (TPair<FString, FSliderRow>& Pair : GaitRows)
	{
		SetSlider(Pair.Value, TargetCharacter->GetGaitAdjustment(Pair.Key));
	}
	SetCombo(LimpSideCombo, { TEXT("Left"), TEXT("Right") }, Config.Gait.LimpSide == TEXT("Right") ? 1 : 0);
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		SetSliderCentered(WeaponLocRows[Axis], static_cast<float>(Config.WeaponLocation[Axis]), WeaponHalfRangeCm);
	}
	SetSliderCentered(WeaponRotRows[0], static_cast<float>(Config.WeaponRotation.Pitch), 180.0f);
	SetSliderCentered(WeaponRotRows[1], static_cast<float>(Config.WeaponRotation.Yaw), 180.0f);
	SetSliderCentered(WeaponRotRows[2], static_cast<float>(Config.WeaponRotation.Roll), 180.0f);
}

// ---- Handlers (controls -> state) -----------------------------------------

void UCharacterBuilderWidget::HandleCombo(const FString& Key, const FString& SelectedItem)
{
	if (bRefreshing || !TargetCharacter) { return; }

	if (Key == TEXT("Saved")) { return; }   // selection only; Load acts on it

	if (Key == TEXT("Type"))
	{
		TargetCharacter->SetCharacterType(TypeCombo->FindOptionIndex(SelectedItem) == 0 ? CharacterType::Modular : CharacterType::Single);
		RefreshFromTarget();
	}
	else if (Key == TEXT("BaseMesh"))
	{
		const int32 Index = BaseMeshCombo->FindOptionIndex(SelectedItem);
		if (BaseMeshAssets.IsValidIndex(Index)) { TargetCharacter->SetBaseMesh(BaseMeshAssets[Index].GetObjectPathString()); }
		TGuardValue<bool> RefreshGuard(bRefreshing, true);
		RefreshSingleOptions();
	}
	else if (Key == TEXT("Palette"))
	{
		const int32 Index = PaletteCombo->FindOptionIndex(SelectedItem) - 1;
		TargetCharacter->SetPaletteMaterial(PaletteAssets.IsValidIndex(Index) ? PaletteAssets[Index].GetObjectPathString() : FString());
	}
	else if (Key == TEXT("Gender"))
	{
		TargetCharacter->SetGender(SelectedItem);
		TGuardValue<bool> RefreshGuard(bRefreshing, true);
		RefreshPartCombos();
	}
	else if (Key == TEXT("Gait.LimpSide"))
	{
		TargetCharacter->SetLimpSide(SelectedItem);
	}
	else if (Key == TEXT("Locomotion"))
	{
		TargetCharacter->SetLocomotionSet(SelectedItem);
	}
	else if (Key == TEXT("Anim.Category"))
	{
		AnimCategoryIndex = AnimCategoryCombo ? AnimCategoryCombo->FindOptionIndex(SelectedItem) : 0;
		RefreshAnimClips();
	}
	else if (Key == TEXT("Anim.Clip"))
	{
		// A new pick while a loop is playing swaps the loop over to it.
		if (TargetCharacter->IsAnimationClipLooping())
		{
			const int32 Index = AnimClipCombo ? AnimClipCombo->FindOptionIndex(SelectedItem) : INDEX_NONE;
			if (AnimClipAssets.IsValidIndex(Index))
			{
				if (UAnimSequence* Clip = Cast<UAnimSequence>(AnimClipAssets[Index].GetAsset())) { TargetCharacter->PlayAnimationClip(Clip, true); }
			}
		}
	}
	else if (Key == TEXT("Weapon"))
	{
		const int32 Index = WeaponCombo->FindOptionIndex(SelectedItem) - 1;
		TargetCharacter->SetWeaponMesh(WeaponAssets.IsValidIndex(Index) ? Cast<UStaticMesh>(WeaponAssets[Index].GetAsset()) : nullptr);
	}
	else if (Key == TEXT("ArmPose"))
	{
		const int32 Index = ArmPoseCombo->FindOptionIndex(SelectedItem) - 1;
		TargetCharacter->SetArmPose(ArmPoseAnims.IsValidIndex(Index) ? ArmPoseAnims[Index].Get() : nullptr);
	}
	else if (Key == TEXT("Face.Nose"))
	{
		FCharacterFaceConfig Face = TargetCharacter->GetCharacterConfig().Face;
		const int32 Index = NoseCombo->FindOptionIndex(SelectedItem) - 1;
		Face.NoseMesh = NoseAssets.IsValidIndex(Index) ? NoseAssets[Index].GetObjectPathString() : FString();
		TargetCharacter->SetFaceConfig(Face);
	}
	else if (Key == TEXT("Face.Hair"))
	{
		FCharacterFaceConfig Face = TargetCharacter->GetCharacterConfig().Face;
		const int32 Index = HairCombo->FindOptionIndex(SelectedItem) - 1;
		Face.HairMesh = HairAssets.IsValidIndex(Index) ? HairAssets[Index].GetObjectPathString() : FString();
		TargetCharacter->SetFaceConfig(Face);
	}
	else if (Key == TEXT("Face.HeadGear"))
	{
		FCharacterFaceConfig Face = TargetCharacter->GetCharacterConfig().Face;
		const int32 Index = HeadGearCombo->FindOptionIndex(SelectedItem) - 1;
		Face.HeadGearMesh = HeadGearAssets.IsValidIndex(Index) ? HeadGearAssets[Index].GetObjectPathString() : FString();
		TargetCharacter->SetFaceConfig(Face);
	}
	else if (Key == TEXT("Face.Expression"))
	{
		FCharacterFaceConfig Face = TargetCharacter->GetCharacterConfig().Face;
		Face.Expression = SelectedItem;
		TargetCharacter->SetFaceConfig(Face);
	}
	else if (Key.StartsWith(TEXT("Part.")))
	{
		const FString SlotName = Key.RightChop(5);
		const TArray<FAssetData>* Options = PartOptions.Find(SlotName);
		UComboBoxString* Combo = PartCombos.FindRef(SlotName);
		if (!Options || !Combo) { return; }
		const int32 Index = Combo->FindOptionIndex(SelectedItem) - 1;
		TargetCharacter->SetPart(SlotName, Options->IsValidIndex(Index) ? (*Options)[Index].GetObjectPathString() : FString());
	}
}

void UCharacterBuilderWidget::HandleCheck(const FString& Key, bool bChecked)
{
	if (bRefreshing || !TargetCharacter) { return; }
	FCharacterFaceConfig Face = TargetCharacter->GetCharacterConfig().Face;
	if (Key == TEXT("Face.Enabled")) { Face.bEnabled = bChecked; }
	else if (Key == TEXT("Face.Mouth")) { Face.bMouthDecal = bChecked; }
	else if (Key == TEXT("Face.Blink")) { Face.bAutoBlink = bChecked; }
	else { return; }
	TargetCharacter->SetFaceConfig(Face);
	if (Key == TEXT("Face.Enabled"))
	{
		// A freshly spawned controller brings the expression list with it.
		TGuardValue<bool> RefreshGuard(bRefreshing, true);
		RefreshFaceControls();
	}
}

void UCharacterBuilderWidget::HandleSlider(const FString& Key, float NewValue)
{
	if (bRefreshing || !TargetCharacter) { return; }
	const FCharacterConfig& Config = TargetCharacter->GetCharacterConfig();

	if (Key.StartsWith(TEXT("Scale.")))
	{
		const int32 Axis = FCString::Atoi(*Key.RightChop(6));
		FVector Scale = Config.Scale;
		Scale[Axis] = NewValue;
		TargetCharacter->SetCharacterScale(Scale);
		SetSlider(ScaleRows[Axis], NewValue);
	}
	else if (Key == TEXT("Speed"))
	{
		TargetCharacter->SetSpeedMultiplier(NewValue);
		SetSlider(SpeedRow, NewValue);
	}
	else if (Key == TEXT("ArmWeight"))
	{
		TargetCharacter->SetArmPoseWeight(NewValue);
		SetSlider(ArmWeightRow, NewValue);
	}
	else if (Key.StartsWith(TEXT("Gait.")))
	{
		const FString Field = Key.RightChop(5);
		TargetCharacter->SetGaitAdjustment(Field, NewValue);
		if (FSliderRow* Row = GaitRows.Find(Field)) { SetSlider(*Row, NewValue); }
	}
	else if (Key == TEXT("FingerCurl") || Key == TEXT("ThumbCurl"))
	{
		const bool bFinger = Key == TEXT("FingerCurl");
		TargetCharacter->SetGripCurl(bFinger ? NewValue : Config.FingerCurlDegrees, bFinger ? Config.ThumbCurlDegrees : NewValue);
		SetSlider(bFinger ? FingerCurlRow : ThumbCurlRow, NewValue, 0);
	}
	else if (Key == TEXT("Face.HatLift"))
	{
		FCharacterFaceConfig Face = TargetCharacter->GetCharacterConfig().Face;
		Face.HeadGearLocation.Z = NewValue;
		TargetCharacter->SetFaceConfig(Face);
		SetSlider(HatLiftRow, NewValue, 1);
	}
	else if (Key == TEXT("Face.HatSize"))
	{
		FCharacterFaceConfig Face = TargetCharacter->GetCharacterConfig().Face;
		Face.HeadGearScale = NewValue;
		TargetCharacter->SetFaceConfig(Face);
		SetSlider(HatSizeRow, NewValue, 2);
	}
	else if (Key.StartsWith(TEXT("WeaponLoc.")))
	{
		const int32 Axis = FCString::Atoi(*Key.RightChop(10));
		FVector Loc = Config.WeaponLocation;
		Loc[Axis] = NewValue;
		TargetCharacter->SetWeaponRelativeLocation(Loc);
		SetSlider(WeaponLocRows[Axis], NewValue, 1);
	}
	else if (Key.StartsWith(TEXT("WeaponRot.")))
	{
		const int32 Axis = FCString::Atoi(*Key.RightChop(10));
		FRotator Rot = Config.WeaponRotation;
		if (Axis == 0) { Rot.Pitch = NewValue; } else if (Axis == 1) { Rot.Yaw = NewValue; } else { Rot.Roll = NewValue; }
		TargetCharacter->SetWeaponRelativeRotation(Rot);
		SetSlider(WeaponRotRows[Axis], NewValue, 1);
	}
	else if (Key.StartsWith(TEXT("Face.NoseLoc.")) || Key.StartsWith(TEXT("Face.MouthLoc.")))
	{
		const bool bNose = Key.StartsWith(TEXT("Face.NoseLoc."));
		const int32 Axis = FCString::Atoi(*Key.RightChop(bNose ? 13 : 14));
		FVector NoseLoc = Config.Face.NoseLocation;
		FVector MouthLoc = Config.Face.MouthLocation;
		(bNose ? NoseLoc : MouthLoc)[Axis] = NewValue;
		// Placement only -- no re-attach per slider tick.
		TargetCharacter->SetFaceOffsets(NoseLoc, MouthLoc);
		SetSlider(bNose ? NoseLocRows[Axis] : MouthLocRows[Axis], NewValue, 1);
	}
	else if (Key == TEXT("Face.MouthScale"))
	{
		TargetCharacter->SetMouthLook(NewValue, Config.Face.MouthTint);
		SetSlider(MouthScaleRow, NewValue);
	}
	else if (Key.StartsWith(TEXT("Face.MouthTint.")))
	{
		FLinearColor Tint = Config.Face.MouthTint;
		Tint.Component(FCString::Atoi(*Key.RightChop(15))) = NewValue;
		TargetCharacter->SetMouthLook(Config.Face.MouthScale, Tint);
		if (UBorder* Swatch = ColorSwatches.FindRef(TEXT("Face.MouthTint"))) { Swatch->SetBrushColor(Tint); }
	}
	else if (Key.StartsWith(TEXT("Color.")))
	{
		// "Color.<Param>.<Channel>"
		FString Parameter, ChannelString;
		Key.RightChop(6).Split(TEXT("."), &Parameter, &ChannelString, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		const int32 Channel = FCString::Atoi(*ChannelString);
		FLinearColor Color = TargetCharacter->GetPartColor(Parameter);
		Color.Component(Channel) = NewValue;
		TargetCharacter->SetPartColor(Parameter, Color);
		if (UBorder* Swatch = ColorSwatches.FindRef(TEXT("Color.") + Parameter)) { Swatch->SetBrushColor(Color); }
	}
}

void UCharacterBuilderWidget::HandleButton(const FString& Key)
{
	if (Key.StartsWith(TEXT("Section.")))
	{
		const FString SectionKey = Key.RightChop(8);
		if (const FSection* Section = Sections.Find(SectionKey)) { SetSectionCollapsed(SectionKey, !Section->bCollapsed); }
		return;
	}
	if (Key == TEXT("Back"))
	{
		if (OwnerController) { OwnerController->ShowEditTool(); }
		return;
	}
	if (Key == TEXT("New"))
	{
		// Place a new character (the Edit Tool used to offer this directly).
		if (OwnerController) { OwnerController->BeginPlacingCharacter(); }
		return;
	}
	if (Key == TEXT("Face"))
	{
		if (OwnerController) { OwnerController->ShowFaceManager(); }
		return;
	}
	if (Key == TEXT("Anims"))
	{
		if (OwnerController) { OwnerController->ShowAnimationBrowser(); }
		return;
	}
	if (Key == TEXT("Anim.Play") || Key == TEXT("Anim.Loop") || Key == TEXT("Anim.Stop"))
	{
		if (!TargetCharacter) { return; }
		if (Key == TEXT("Anim.Stop")) { TargetCharacter->StopAnimationClip(); return; }
		const int32 Index = AnimClipCombo ? AnimClipCombo->GetSelectedIndex() : INDEX_NONE;
		if (AnimClipAssets.IsValidIndex(Index))
		{
			if (UAnimSequence* Clip = Cast<UAnimSequence>(AnimClipAssets[Index].GetAsset())) { TargetCharacter->PlayAnimationClip(Clip, Key == TEXT("Anim.Loop")); }
		}
		return;
	}
	if (Key == TEXT("BackToManager"))
	{
		if (OwnerController) { OwnerController->ShowCharacterManager(); }
		return;
	}
	if (Key == TEXT("SaveClose"))
	{
		// Save (taking the Name box on the manager page) and close every
		// character page back to the Edit Tool -- no prompt, nothing is dirty.
		if (TargetCharacter && NameBox) { TargetCharacter->SetCharacterName(NameBox->GetText().ToString().TrimStartAndEnd()); }
		if (OwnerController) { OwnerController->SaveSelectedAndClose(); }
		return;
	}
	if (Key == TEXT("Control"))
	{
		// Hand the player's inputs to this character (it becomes the one
		// being played); selection/editing stays on it.
		if (OwnerController && TargetCharacter) { OwnerController->ControlCharacter(TargetCharacter); }
		return;
	}
	if (Key == TEXT("Remove"))
	{
		if (OwnerController) { OwnerController->RemoveSelectedCharacter(); }
		return;
	}
	if (Key == TEXT("Fly"))
	{
		if (OwnerController) { OwnerController->EnterFlyMode(); }
		return;
	}
	if (!TargetCharacter) { return; }

	if (Key.StartsWith(TEXT("Reset.")))
	{
		ResetControl(Key.RightChop(6));
	}
	else if (Key == TEXT("Save"))
	{
		TargetCharacter->SetCharacterName(NameBox->GetText().ToString().TrimStartAndEnd());
		const bool bOk = TargetCharacter->SaveCharacterConfig();
		StatusText->SetText(FText::FromString(bOk
			? FString::Printf(TEXT("Saved %s"), *CharacterConfigFile::GetPath(TargetCharacter->GetCharacterConfig().Name))
			: TEXT("Save FAILED (see log)")));
		TGuardValue<bool> RefreshGuard(bRefreshing, true);
		RefreshSavedList();
	}
	else if (Key == TEXT("Load"))
	{
		const FString Name = SavedCombo->GetSelectedOption();
		const bool bOk = TargetCharacter->LoadCharacterConfig(Name);
		StatusText->SetText(FText::FromString(bOk ? FString::Printf(TEXT("Loaded %s"), *Name) : FString::Printf(TEXT("Could not load %s"), *Name)));
		RefreshFromTarget();
	}
}

void UCharacterBuilderWidget::HandleText(const FString& Key, const FString& Text)
{
	if (bRefreshing || !TargetCharacter) { return; }
	if (Key == TEXT("Name")) { TargetCharacter->SetCharacterName(Text.TrimStartAndEnd()); }
	else if (Key == TEXT("Tags")) { TargetCharacter->SetCharacterTags(Text); }
	else if (Key == TEXT("Description")) { TargetCharacter->SetDescription(Text); }
	else if (Key == TEXT("Comment")) { TargetCharacter->SetComment(Text); }
}

// ---- Reset -------------------------------------------------------------------

void UCharacterBuilderWidget::ResetControl(const FString& Key)
{
	const FCharacterConfig Def = TargetCharacter->GetDefaultCharacterConfig();
	const FCharacterConfig& Cur = TargetCharacter->GetCharacterConfig();

	if (Key == TEXT("BaseMesh")) { TargetCharacter->SetBaseMesh(Def.BaseMesh); TargetCharacter->SetPaletteMaterial(Def.Material); }
	else if (Key == TEXT("Palette")) { TargetCharacter->SetPaletteMaterial(Def.Material); }
	else if (Key == TEXT("Gender")) { TargetCharacter->SetGender(Def.Gender); }
	else if (Key.StartsWith(TEXT("Part."))) { const FString SlotName = Key.RightChop(5); TargetCharacter->SetPart(SlotName, Def.Parts.FindRef(SlotName)); }
	else if (Key.StartsWith(TEXT("Color."))) { const FString Param = Key.RightChop(6); TargetCharacter->SetPartColor(Param, TargetCharacter->GetDefaultPartColor(Param)); }
	else if (Key.StartsWith(TEXT("Scale."))) { FVector Scale = Cur.Scale; Scale[FCString::Atoi(*Key.RightChop(6))] = 1.0f; TargetCharacter->SetCharacterScale(Scale); }
	else if (Key == TEXT("Speed")) { TargetCharacter->SetSpeedMultiplier(Def.SpeedMultiplier); }
	else if (Key == TEXT("Locomotion")) { TargetCharacter->SetLocomotionSet(Def.Locomotion); }
	else if (Key == TEXT("Weapon")) { TargetCharacter->SetWeaponMesh(Def.WeaponMesh.IsEmpty() ? nullptr : LoadObject<UStaticMesh>(nullptr, *Def.WeaponMesh)); }
	else if (Key == TEXT("ArmPose")) { TargetCharacter->SetArmPose(Def.ArmPose.IsEmpty() || Def.ArmPose == NoneOption ? nullptr : LoadObject<UAnimSequence>(nullptr, *Def.ArmPose)); }
	else if (Key == TEXT("ArmWeight")) { TargetCharacter->SetArmPoseWeight(Def.ArmPoseWeight); }
	else if (Key == TEXT("FingerCurl")) { TargetCharacter->SetGripCurl(Def.FingerCurlDegrees, Cur.ThumbCurlDegrees); }
	else if (Key == TEXT("ThumbCurl")) { TargetCharacter->SetGripCurl(Cur.FingerCurlDegrees, Def.ThumbCurlDegrees); }
	else if (Key == TEXT("Gait.LimpSide")) { TargetCharacter->SetLimpSide(Def.Gait.LimpSide); }
	else if (Key.StartsWith(TEXT("Gait."))) { const FString Field = Key.RightChop(5); FGaitAdjustments Defaults; if (const FFloatProperty* P = FindFProperty<FFloatProperty>(FGaitAdjustments::StaticStruct(), *Field)) { TargetCharacter->SetGaitAdjustment(Field, P->GetPropertyValue_InContainer(&Defaults)); } }
	else if (Key.StartsWith(TEXT("WeaponLoc."))) { FVector Loc = Cur.WeaponLocation; const int32 A = FCString::Atoi(*Key.RightChop(10)); Loc[A] = Def.WeaponLocation[A]; TargetCharacter->SetWeaponRelativeLocation(Loc); }
	else if (Key.StartsWith(TEXT("WeaponRot."))) { FRotator R = Cur.WeaponRotation; const int32 A = FCString::Atoi(*Key.RightChop(10)); if (A == 0) { R.Pitch = Def.WeaponRotation.Pitch; } else if (A == 1) { R.Yaw = Def.WeaponRotation.Yaw; } else { R.Roll = Def.WeaponRotation.Roll; } TargetCharacter->SetWeaponRelativeRotation(R); }
	else if (Key.StartsWith(TEXT("Face.")))
	{
		FCharacterFaceConfig Face = Cur.Face;
		if (Key == TEXT("Face.Enabled")) { Face.bEnabled = Def.Face.bEnabled; }
		else if (Key == TEXT("Face.Nose")) { Face.NoseMesh = Def.Face.NoseMesh; }
		else if (Key == TEXT("Face.Hair")) { Face.HairMesh = Def.Face.HairMesh; }
		else if (Key == TEXT("Face.HeadGear")) { Face.HeadGearMesh = Def.Face.HeadGearMesh; }
		else if (Key == TEXT("Face.HatLift")) { Face.HeadGearLocation = Def.Face.HeadGearLocation; }
		else if (Key == TEXT("Face.HatSize")) { Face.HeadGearScale = Def.Face.HeadGearScale; }
		else if (Key == TEXT("Face.Mouth")) { Face.bMouthDecal = Def.Face.bMouthDecal; }
		else if (Key == TEXT("Face.Expression")) { Face.Expression = Def.Face.Expression; }
		else if (Key == TEXT("Face.Blink")) { Face.bAutoBlink = Def.Face.bAutoBlink; }
		else if (Key == TEXT("Face.MouthScale")) { Face.MouthScale = Def.Face.MouthScale; }
		else if (Key == TEXT("Face.MouthTint")) { Face.MouthTint = Def.Face.MouthTint; }
		else if (Key.StartsWith(TEXT("Face.NoseLoc."))) { const int32 A = FCString::Atoi(*Key.RightChop(13)); Face.NoseLocation[A] = Def.Face.NoseLocation[A]; }
		else if (Key.StartsWith(TEXT("Face.MouthLoc."))) { const int32 A = FCString::Atoi(*Key.RightChop(14)); Face.MouthLocation[A] = Def.Face.MouthLocation[A]; }
		TargetCharacter->SetFaceConfig(Face);
	}
	else
	{
		return;
	}
	RefreshFromTarget();
}
