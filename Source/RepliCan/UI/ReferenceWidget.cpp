#include "UI/ReferenceWidget.h"
#include "UI/PaneShape.h"
#include "Weapons/WeaponSkins.h"
#include "UI/CrtStyle.h"
#include "UI/CrtRuleWidget.h"
#include "UI/CrtTabsWidget.h"
#include "Core/BasePlayerController.h"
#include "UI/InventoryGridWidget.h"
#include "UI/SheetSpec.h"
#include "Weapons/WeaponCatalog.h"
#include "Items/ItemCatalog.h"
#include "Items/ItemFields.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Dom/JsonObject.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Layout/Clipping.h"
#include "Framework/Application/SlateApplication.h"

static FString GroupHeader(const TCHAR* Name, bool bOpen);   // defined with the field form, below
static bool ParseVector(const FString& Text, FVector& Out);   // defined with the markers, below
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

static const float CardIcon = 160.0f;     // the card's render square
static const float DetailWidth = 980.0f;  // the detail column
static const float ViewerWidth = 700.0f;  // the render in it: smaller than the column, the rest is air

void UReferenceCardBinding::OnClicked()
{
	if (!Widget.IsValid()) { return; }
	const double Now = FPlatformTime::Seconds();
	const bool bCtrl = FSlateApplication::IsInitialized() && FSlateApplication::Get().GetModifierKeys().IsControlDown();
	if (bCtrl && Widget->LastCardIndex == Index && Now - Widget->LastCardClickSeconds < 0.45)
	{
		Widget->LastCardIndex = -1;
		Widget->HideEntry(Index);
		return;
	}
	Widget->LastCardIndex = Index; Widget->LastCardClickSeconds = Now;
	Widget->SelectEntry(Index);
}
void UReferenceFieldBinding::OnCycle() { if (Widget.IsValid()) { Widget->CycleEnumField(Key); } }

void UReferenceFieldBinding::OnToggle() { if (!Widget.IsValid()) { return; } if (Key.StartsWith(TEXT("group:"))) { Widget->ToggleGroup(Key.Mid(6)); } else if (Key.StartsWith(TEXT("arm:"))) { Widget->ArmMarker(Key.Mid(4)); } else if (Key == TEXT("attach:add")) { Widget->AddAttachment(); } else if (Key == TEXT("attach:del")) { Widget->RemoveArmedAttachment(); } else { Widget->ToggleBoolField(Key); } }

void UReferenceWidget::NativeOnInitialized()
{
	SetIsFocusable(true);   // keys (X, Y, Z during a point drag) come to this widget when it holds focus
	Super::NativeOnInitialized();
	UBorder* Root = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Root"));
	// OPAQUE, not a tint. This ground covers the whole screen, and at the panel colour's 93% it let
	// the level show faintly through -- which reads as a green wash laid over the game rather than
	// as a screen you have opened, and was reported three times as exactly that. Solid, it is a
	// screen. (Crt::Panel is the see-through one, if the world behind is ever wanted back.)
	Root->SetBrushColor(Crt::PanelSolid);
	Root->SetPadding(FMargin(34.0f, 26.0f));
	WidgetTree->RootWidget = Root;
	Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	Root->SetContent(Column);
	Rebuild();
}

void UReferenceWidget::NativeDestruct()
{
	if (OwnerController) { OwnerController->HideWeaponPreview(); }
	Super::NativeDestruct();
}

static FString CatalogueFile() { return FPaths::Combine(FPaths::ProjectDir(), TEXT("UI"), TEXT("Weapons.json")); }
static FString RefItemsFile() { return FPaths::Combine(FPaths::ProjectDir(), TEXT("UI"), TEXT("Items.json")); }
static const TCHAR* Categories[] = { TEXT("weapons"), TEXT("optics"), TEXT("armor"), TEXT("equipment"), TEXT("consumables"), TEXT("other") };

// UI/Weapons.json: { "weapons": { "<Pack>/<asset>": { name, kind, pack, description, icon, mesh } } }
void UReferenceWidget::LoadCatalogue()
{
	Entries.Reset();
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *CatalogueFile())) { UE_LOG(LogTemp, Warning, TEXT("Reference: %s missing"), *CatalogueFile()); return; }
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) { UE_LOG(LogTemp, Warning, TEXT("Reference: %s did not parse"), *CatalogueFile()); return; }
	StanceNames.Reset();
	const TSharedPtr<FJsonObject>* Stances = nullptr;
	if (Root->TryGetObjectField(TEXT("stances"), Stances) && Stances) { for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Stances)->Values) { StanceNames.Add(Pair.Key); } }
	const TSharedPtr<FJsonObject>* Weapons = nullptr;
	if (!Root->TryGetObjectField(TEXT("weapons"), Weapons) || !Weapons) { return; }
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Weapons)->Values)
	{
		if (Pair.Key.StartsWith(TEXT("_"))) { continue; }   // a documentation key, not an entry
		const TSharedPtr<FJsonObject>* O = nullptr;
		if (!Pair.Value->TryGetObject(O) || !O) { continue; }
		FReferenceEntry E; E.Key = Pair.Key;
		(*O)->TryGetStringField(TEXT("name"), E.Name); (*O)->TryGetStringField(TEXT("kind"), E.Kind); (*O)->TryGetStringField(TEXT("pack"), E.Pack);
		(*O)->TryGetStringField(TEXT("description"), E.Description); (*O)->TryGetStringField(TEXT("icon"), E.Icon); (*O)->TryGetStringField(TEXT("mesh"), E.Mesh);
		(*O)->TryGetStringField(TEXT("sound"), E.Sound);
		(*O)->TryGetBoolField(TEXT("hip_fire"), E.bHipFire);
		(*O)->TryGetStringField(TEXT("stance"), E.Stance);
		ItemCatalog::ReadFields(*O, E.Fields);
		if (E.Name.IsEmpty()) { E.Name = Pair.Key; }
		E.bWeaponsFile = true;
		E.Category = E.Kind == TEXT("Shield") ? TEXT("armor") : TEXT("weapons");   // a shield is carried like a weapon but read like armour: the ARMOR tab, armour's fields
		Entries.Add(E);
	}
	// The optics: parts with points of their own (MOUNT, EYE), on a tab of their own. Same
	// card, the part alone in the booth. Saved back into the same file's optics block.
	const TSharedPtr<FJsonObject>* OpticsObj = nullptr;
	if (Root->TryGetObjectField(TEXT("optics"), OpticsObj) && OpticsObj)
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*OpticsObj)->Values)
		{
			if (Pair.Key.StartsWith(TEXT("_"))) { continue; }   // a documentation key, not an entry
			const TSharedPtr<FJsonObject>* O = nullptr;
			if (!Pair.Value->TryGetObject(O) || !O) { continue; }
			FReferenceEntry E; E.Key = Pair.Key;
			(*O)->TryGetStringField(TEXT("name"), E.Name); (*O)->TryGetStringField(TEXT("mesh"), E.Mesh); (*O)->TryGetStringField(TEXT("icon"), E.Icon);
			(*O)->TryGetStringField(TEXT("description"), E.Description);
			ItemCatalog::ReadFields(*O, E.Fields);
			if (E.Name.IsEmpty()) { E.Name = Pair.Key; }
			E.Category = TEXT("optics"); E.Kind = TEXT("Optic");
			Entries.Add(E);
		}
	}
	// The other catalogue: everything a character can carry that is not a weapon, from the
	// packs (Tools/survey_items.py -> Tools/build_item_catalog.py). Same card, same booth; no
	// stance, no hip fire, no points to draw.
	FString ItemsJson;
	TSharedPtr<FJsonObject> ItemsRoot;
	if (FFileHelper::LoadFileToString(ItemsJson, *RefItemsFile()))
	{
		TSharedRef<TJsonReader<>> ItemsReader = TJsonReaderFactory<>::Create(ItemsJson);
		const TSharedPtr<FJsonObject>* Items = nullptr;
		if (FJsonSerializer::Deserialize(ItemsReader, ItemsRoot) && ItemsRoot.IsValid() && ItemsRoot->TryGetObjectField(TEXT("items"), Items) && Items)
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Items)->Values)
			{
				if (Pair.Key.StartsWith(TEXT("_"))) { continue; }   // a documentation key, not an entry
				const TSharedPtr<FJsonObject>* O = nullptr;
				if (!Pair.Value->TryGetObject(O) || !O) { continue; }
				FReferenceEntry E; E.Key = Pair.Key;
				(*O)->TryGetStringField(TEXT("name"), E.Name); (*O)->TryGetStringField(TEXT("category"), E.Category); (*O)->TryGetStringField(TEXT("pack"), E.Pack);
				(*O)->TryGetStringField(TEXT("description"), E.Description); (*O)->TryGetStringField(TEXT("icon"), E.Icon); (*O)->TryGetStringField(TEXT("mesh"), E.Mesh);
				E.Kind = E.Category;
				ItemCatalog::ReadFields(*O, E.Fields);
				if (!E.Fields.FindRef(TEXT("kind")).IsEmpty()) { E.Kind = E.Fields.FindRef(TEXT("kind")); }
				if (E.Name.IsEmpty()) { E.Name = Pair.Key; }
				if (E.Category.IsEmpty()) { E.Category = TEXT("other"); }
				Entries.Add(E);
			}
		}
	}
	// Weapons by name, one list regardless of kind (the tabs do the grouping); everything else by kind, then name.
	Entries.Sort([](const FReferenceEntry& A, const FReferenceEntry& B)
	{
		if (A.bWeaponsFile != B.bWeaponsFile) { return A.bWeaponsFile; }
		if (A.bWeaponsFile) { return A.Name < B.Name; }
		return A.Kind == B.Kind ? A.Name < B.Name : A.Kind < B.Kind;
	});
}

// Writes one entry's name and description back, marking it kept so the generator leaves it alone.
bool UReferenceWidget::SaveItemEntry(const FReferenceEntry& E)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *RefItemsFile())) { return false; }
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) { return false; }
	const TSharedPtr<FJsonObject>* Items = nullptr;
	if (!Root->TryGetObjectField(TEXT("items"), Items) || !Items) { return false; }
	const TSharedPtr<FJsonObject>* Entry = nullptr;
	if (!(*Items)->TryGetObjectField(E.Key, Entry) || !Entry) { return false; }
	(*Entry)->SetStringField(TEXT("name"), E.Name);
	(*Entry)->SetStringField(TEXT("description"), E.Description);
	(*Entry)->SetBoolField(TEXT("keep"), true);
	for (const ItemFields::FField& F : ItemFields::Table) { if (ItemFields::Applies(F, E.Category)) { if (const FString* V = E.Fields.Find(F.Key)) { ItemCatalog::StringToField(*Entry, F, *V); } } }
	FString Out;
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Out);
	if (!FJsonSerializer::Serialize(Root.ToSharedRef(), Writer)) { return false; }
	const bool bSaved = FFileHelper::SaveStringToFile(Out, *RefItemsFile(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	if (bSaved) { ItemCatalog::Reload(true); }
	return bSaved;
}

bool UReferenceWidget::SaveOpticEntry(const FReferenceEntry& E)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *CatalogueFile())) { return false; }
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) { return false; }
	const TSharedPtr<FJsonObject>* Optics = nullptr;
	if (!Root->TryGetObjectField(TEXT("optics"), Optics) || !Optics) { return false; }
	const TSharedPtr<FJsonObject>* Entry = nullptr;
	if (!(*Optics)->TryGetObjectField(E.Key, Entry) || !Entry) { return false; }
	(*Entry)->SetStringField(TEXT("name"), E.Name);
	(*Entry)->SetStringField(TEXT("description"), E.Description);
	for (const ItemFields::FField& F : ItemFields::Table) { if (ItemFields::Applies(F, E.Category)) { if (const FString* V = E.Fields.Find(F.Key)) { ItemCatalog::StringToField(*Entry, F, *V); } } }
	FString Out;
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Out);
	if (!FJsonSerializer::Serialize(Root.ToSharedRef(), Writer)) { return false; }
	const bool bSaved = FFileHelper::SaveStringToFile(Out, *CatalogueFile(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	if (bSaved) { WeaponCatalog::Reload(); ItemCatalog::Reload(true); }
	return bSaved;
}

bool UReferenceWidget::SaveEntry(const FReferenceEntry& E)
{
	if (E.Category == TEXT("optics")) { return SaveOpticEntry(E); }
	if (!E.bWeaponsFile) { return SaveItemEntry(E); }
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *CatalogueFile())) { return false; }
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) { return false; }
	const TSharedPtr<FJsonObject>* Weapons = nullptr;
	if (!Root->TryGetObjectField(TEXT("weapons"), Weapons) || !Weapons) { return false; }
	const TSharedPtr<FJsonObject>* Entry = nullptr;
	if (!(*Weapons)->TryGetObjectField(E.Key, Entry) || !Entry) { return false; }
	(*Entry)->SetStringField(TEXT("name"), E.Name);
	(*Entry)->SetStringField(TEXT("sound"), E.Sound);
	(*Entry)->SetStringField(TEXT("description"), E.Description);
	(*Entry)->SetBoolField(TEXT("hip_fire"), E.bHipFire);
	if (!E.Stance.IsEmpty()) { (*Entry)->SetStringField(TEXT("stance"), E.Stance); }
	(*Entry)->SetBoolField(TEXT("keep"), true);
	for (const ItemFields::FField& F : ItemFields::Table) { if (ItemFields::Applies(F, E.Category) || F.Scope == ItemFields::EScope::Gear) { if (const FString* V = E.Fields.Find(F.Key)) { ItemCatalog::StringToField(*Entry, F, *V); } } }
	FString Out;
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Out);
	if (!FJsonSerializer::Serialize(Root.ToSharedRef(), Writer)) { return false; }
	const bool bSaved = FFileHelper::SaveStringToFile(Out, *CatalogueFile(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	// The catalogue is what the game reads (hip fire decides the carry position when the weapon
	// is fired), so it is re-read here: the next equip of this weapon gets the flag as saved.
	if (bSaved) { WeaponCatalog::Reload(); ItemCatalog::Reload(true); }
	return bSaved;
}

// A bracketed label padded to Width characters: in a fixed-pitch face that is a fixed width,
// so a control whose value changes (YES/NO, Rifle/Shotgun, ON/OFF) never moves its neighbours.
static FString FixedLabel(const FString& Inner, int32 Width)
{
	FString S = Inner;
	while (S.Len() < Width) { S = ((Width - S.Len()) % 2 == 0) ? TEXT(" ") + S : S + TEXT(" "); }
	return FString::Printf(TEXT("[ %s ]"), *S);
}

static UEditableTextBox* StyledBox(UWidgetTree* Tree, int32 Size)
{
	UEditableTextBox* Box = Tree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
	Box->WidgetStyle.SetFont(Crt::Fixed(Size));
	Box->WidgetStyle.SetForegroundColor(Crt::Green);
	Box->WidgetStyle.SetFocusedForegroundColor(Crt::Green);
	Box->WidgetStyle.SetBackgroundImageNormal(FSlateColorBrush(Crt::PanelSolid));
	Box->WidgetStyle.SetBackgroundImageHovered(FSlateColorBrush(Crt::PanelSolid));
	Box->WidgetStyle.SetBackgroundImageFocused(FSlateColorBrush(Crt::Faint));
	Box->WidgetStyle.SetPadding(FMargin(8.0f, 2.0f));
	Box->WidgetStyle.TextStyle.SetSelectedBackgroundColor(FSlateColor(Crt::DimGreen));
	Box->WidgetStyle.TextStyle.SetHighlightColor(Crt::Green);
	return Box;
}

void UReferenceWidget::Rebuild()
{
	if (!Column) { return; }
	BeginSettle();
	const FSheetSpec& S = FSheetSpec::Get();
	// What was open last time, so reopening the page comes back to it.
	const FString PrevKey = Entries.IsValidIndex(SelectedIndex) ? Entries[SelectedIndex].Key : FString();
	Column->ClearChildren();
	CardBindings.Reset(); SelectedIndex = -1;
	if (OwnerController) { OwnerController->HideWeaponPreview(); }
	LoadCatalogue();

	// Header rule with the tab strip and the close box, as on the sheet.
	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Header->AddChildToHorizontalBox(Crt::FixedText(WidgetTree, S.HeaderLeft, S.RuleSize, Crt::Faint))->SetVerticalAlignment(VAlign_Center);
	UCrtTabsWidget* Tabs = UCrtTabsWidget::Make(GetOwningPlayer(), UCrtTabsWidget::StandardTabs(OwnerController ? OwnerController->GetPlayerDisplayName() : FString()), UCrtTabsWidget::TabReference, S.NameSize);
	Tabs->OnTab.BindUObject(this, &UReferenceWidget::OnTab);
	Header->AddChildToHorizontalBox(Tabs)->SetVerticalAlignment(VAlign_Center);
	UHorizontalBoxSlot* RuleSlot = Header->AddChildToHorizontalBox(UCrtRuleWidget::Make(GetOwningPlayer(), S.HeaderRight, S.RuleSize, Crt::Faint));
	RuleSlot->SetSize(ESlateSizeRule::Fill); RuleSlot->SetVerticalAlignment(VAlign_Center);
	if (!S.HeaderEnd.IsEmpty()) { Header->AddChildToHorizontalBox(Crt::FixedText(WidgetTree, S.HeaderEnd, S.RuleSize, Crt::Faint))->SetVerticalAlignment(VAlign_Center); }
	UButton* Close = Crt::Button(WidgetTree, TEXT("[ X ]"), S.NameSize, Crt::Green);
	Close->OnClicked.AddDynamic(this, &UReferenceWidget::OnCloseClicked);
	Header->AddChildToHorizontalBox(Close)->SetPadding(FMargin(12, 0, 0, 0));
	Column->AddChildToVerticalBox(Header)->SetPadding(FMargin(0, 0, 0, 12));

	// Category row (weapons only for now) and the search box.
	UHorizontalBox* Top = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	TabLabels.Reset();
	{
		using FTabFn = void (UReferenceWidget::*)();
		const FTabFn Fns[] = { &UReferenceWidget::OnTabWeapons, &UReferenceWidget::OnTabOptics, &UReferenceWidget::OnTabArmor, &UReferenceWidget::OnTabEquipment, &UReferenceWidget::OnTabConsumables, &UReferenceWidget::OnTabOther };
		for (int32 i = 0; i < UE_ARRAY_COUNT(Categories); ++i)
		{
			UButton* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
			B->SetStyle(Crt::ButtonStyle());
			UTextBlock* L = Crt::FixedText(WidgetTree, TEXT(""), S.CaptionSize, Crt::Faint);
			B->AddChild(L);
			if (UButtonSlot* BS = Cast<UButtonSlot>(L->Slot)) { BS->SetPadding(FMargin(6.0f, 4.0f)); }
			switch (i)
			{
			case 0: B->OnClicked.AddDynamic(this, &UReferenceWidget::OnTabWeapons); break;
			case 1: B->OnClicked.AddDynamic(this, &UReferenceWidget::OnTabOptics); break;
			case 2: B->OnClicked.AddDynamic(this, &UReferenceWidget::OnTabArmor); break;
			case 3: B->OnClicked.AddDynamic(this, &UReferenceWidget::OnTabEquipment); break;
			case 4: B->OnClicked.AddDynamic(this, &UReferenceWidget::OnTabConsumables); break;
			default: B->OnClicked.AddDynamic(this, &UReferenceWidget::OnTabOther); break;
			}
			(void)Fns;
			UHorizontalBoxSlot* TabSlot = Top->AddChildToHorizontalBox(B); TabSlot->SetVerticalAlignment(VAlign_Center); TabSlot->SetPadding(FMargin(0, 0, 6, 0));
			TabLabels.Add(L);
		}
	}
	ShowTabs();
	{
		UButton* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		B->SetStyle(Crt::ButtonStyle());
		ShowHiddenLabel = Crt::FixedText(WidgetTree, bShowHidden ? TEXT("[ X ] SHOW HIDDEN") : TEXT("[   ] SHOW HIDDEN"), S.RowSize, Crt::DimGreen);
		B->AddChild(ShowHiddenLabel);
		if (UButtonSlot* BS = Cast<UButtonSlot>(ShowHiddenLabel->Slot)) { BS->SetPadding(FMargin(6.0f, 4.0f)); }
		B->OnClicked.AddDynamic(this, &UReferenceWidget::OnToggleShowHidden);
		UHorizontalBoxSlot* HS = Top->AddChildToHorizontalBox(B); HS->SetVerticalAlignment(VAlign_Center); HS->SetPadding(FMargin(18, 0, 0, 0));
	}
	Top->AddChildToHorizontalBox(WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass()))->SetSize(ESlateSizeRule::Fill);
	Top->AddChildToHorizontalBox(Crt::FixedText(WidgetTree, TEXT("SEARCH > "), S.CaptionSize, Crt::DimGreen))->SetVerticalAlignment(VAlign_Center);
	SearchBox = StyledBox(WidgetTree, S.CaptionSize);
	SearchBox->SetHintText(FText::FromString(TEXT("name, kind or pack")));
	SearchBox->SetMinDesiredWidth(320.0f);
	SearchBox->OnTextChanged.AddDynamic(this, &UReferenceWidget::OnSearchChanged);
	if (!Filter.IsEmpty()) { SearchBox->SetText(FText::FromString(Filter)); }
	Top->AddChildToHorizontalBox(SearchBox)->SetVerticalAlignment(VAlign_Center);
	Column->AddChildToVerticalBox(Top)->SetPadding(FMargin(0, 0, 0, 8));
	Column->AddChildToVerticalBox(UCrtRuleWidget::Make(GetOwningPlayer(), TEXT("-"), S.RuleSize, Crt::Faint))->SetPadding(FMargin(0, 0, 0, 8));

	// The list on the left, the detail column on the right (collapsed until a card is picked).
	UHorizontalBox* Body = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Column->AddChildToVerticalBox(Body)->SetSize(ESlateSizeRule::Fill);
	List = Crt::ScrollBox(WidgetTree);
	UHorizontalBoxSlot* ListSlot = Body->AddChildToHorizontalBox(List);
	ListSlot->SetSize(ESlateSizeRule::Fill);
	DetailBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Detail"));
	DetailBox->SetWidthOverride(DetailWidth);
	UHorizontalBox* DetailRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (!S.Divider.IsEmpty()) { UHorizontalBoxSlot* D = DetailRow->AddChildToHorizontalBox(UCrtRuleWidget::MakeVertical(GetOwningPlayer(), S.Divider, S.RuleSize, Crt::Faint)); D->SetVerticalAlignment(VAlign_Fill); D->SetPadding(FMargin(S.ColumnGap * 0.5f, 0, S.ColumnGap * 0.5f, 0)); }
	// The column scrolls: with a render, four fields and the buttons it can run past the
	// bottom of the panel on a short screen, and everything below would be unreachable.
	// The header stays put; the render and the form scroll under it.
	UVerticalBox* DetailStack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	BuildDetailHeader(DetailStack);
	DetailScroll = Crt::ScrollBox(WidgetTree, 10.0f);
	DetailScroll->SetClipping(EWidgetClipping::ClipToBounds);
	UVerticalBox* DetailColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	BuildDetail(DetailColumn);
	DetailScroll->AddChild(DetailColumn);
	DetailStack->AddChildToVerticalBox(DetailScroll)->SetSize(ESlateSizeRule::Fill);
	DetailRow->AddChildToHorizontalBox(DetailStack)->SetSize(ESlateSizeRule::Fill);
	DetailBox->AddChild(DetailRow);
	DetailBox->SetVisibility(ESlateVisibility::Collapsed);
	UHorizontalBoxSlot* DetailSlot = Body->AddChildToHorizontalBox(DetailBox);
	DetailSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic)); DetailSlot->SetVerticalAlignment(VAlign_Fill);

	CountText = Crt::FixedText(WidgetTree, TEXT(""), S.RowSize, Crt::DimGreen);
	Column->AddChildToVerticalBox(CountText)->SetPadding(FMargin(0, 8, 0, 0));
	if (!S.FooterLeft.IsEmpty() || !S.FooterEnd.IsEmpty())
	{
		UHorizontalBox* Footer = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UHorizontalBoxSlot* FootRule = Footer->AddChildToHorizontalBox(UCrtRuleWidget::Make(GetOwningPlayer(), S.FooterLeft, S.RuleSize, Crt::Faint));
		FootRule->SetSize(ESlateSizeRule::Fill); FootRule->SetVerticalAlignment(VAlign_Center);
		if (!S.FooterEnd.IsEmpty()) { Footer->AddChildToHorizontalBox(Crt::FixedText(WidgetTree, S.FooterEnd, S.RuleSize, Crt::Faint))->SetVerticalAlignment(VAlign_Center); }
		Column->AddChildToVerticalBox(Footer)->SetPadding(FMargin(0, 12, 0, 0));
	}
	FillList();
	if (!PrevKey.IsEmpty())
	{
		for (int32 i = 0; i < Entries.Num(); ++i)
		{
			if (Entries[i].Key == PrevKey) { SelectEntry(i); break; }
		}
	}
}

// The detail column: the booth render (drag to turn), FIRE, then the editable name and
// description with SAVE, and a line for what happened.
void UReferenceWidget::BuildDetailHeader(UVerticalBox* Into)
{
	const FSheetSpec& S = FSheetSpec::Get();
	// Line one: the name as it is written, the asset path (minus the project prefix everything
	// shares) against the right edge, the review date beside it.
	UHorizontalBox* Head = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	DetailTitle = Crt::FixedText(WidgetTree, TEXT(""), S.InfoNameSize, Crt::Green);   // the same face and size as a card's name in the list
	UHorizontalBoxSlot* TitleSlot = Head->AddChildToHorizontalBox(DetailTitle);
	TitleSlot->SetSize(ESlateSizeRule::Fill); TitleSlot->SetVerticalAlignment(VAlign_Bottom);
	Head->SetClipping(EWidgetClipping::ClipToBounds);   // nothing on this line may paint over its neighbour
	ReviewLabel = Crt::FixedText(WidgetTree, TEXT("REVIEWED: never"), S.RowSize, Crt::DimGreen);
	UHorizontalBoxSlot* RL = Head->AddChildToHorizontalBox(ReviewLabel); RL->SetVerticalAlignment(VAlign_Bottom); RL->SetPadding(FMargin(16, 0, 16, 3));
	// THE PATH LIVES IN A FIXED CORNER AND IS CLIPPED TO IT. A text block does not clip itself: a
	// long one simply paints outside its slot and over whatever is next to it, which is how the name
	// and the path ended up on top of each other. The box gives it a width it cannot exceed, and
	// NativeTick slides the text inside when it is longer than that.
	MeshMarquee = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	MeshMarquee->SetWidthOverride(430.0f);
	MeshMarquee->SetClipping(EWidgetClipping::ClipToBounds);
	DetailMesh = Crt::FixedText(WidgetTree, TEXT(""), S.RowSize, Crt::DimGreen, ETextJustify::Left);
	MeshMarquee->AddChild(DetailMesh);
	UHorizontalBoxSlot* MeshSlot = Head->AddChildToHorizontalBox(MeshMarquee);
	MeshSlot->SetVerticalAlignment(VAlign_Bottom); MeshSlot->SetPadding(FMargin(0, 0, 28, 3));   // in from the edge
	Into->AddChildToVerticalBox(Head)->SetPadding(FMargin(0, 0, 0, 6));
	// Line two: what you can do to the entry. SAVE writes everything on the page -- the dragged
	// points, the fields, the flags -- REVERT reads it back from disk, REVIEWED stamps today,
	// HIDE keeps it out of the list, CLOSE puts the column away.
	UHorizontalBox* Bar = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	auto Add = [&](UButton* B) { Bar->AddChildToHorizontalBox(B)->SetPadding(FMargin(0, 0, 10, 0)); };
	UButton* Save = Crt::Button(WidgetTree, TEXT("[ SAVE ]"), S.CaptionSize, Crt::Green);
	Save->OnClicked.AddDynamic(this, &UReferenceWidget::OnSaveDetail); Add(Save);
	UButton* Revert = Crt::Button(WidgetTree, TEXT("[ REVERT ]"), S.CaptionSize, Crt::DimGreen);
	Revert->OnClicked.AddDynamic(this, &UReferenceWidget::OnRevert); Add(Revert);
	UButton* Reviewed = Crt::Button(WidgetTree, TEXT("[ REVIEWED ]"), S.CaptionSize, Crt::DimGreen);
	Reviewed->OnClicked.AddDynamic(this, &UReferenceWidget::OnReviewedToday); Add(Reviewed);
	UButton* Hide = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	Hide->SetStyle(Crt::ButtonStyle());
	HiddenLabel = Crt::FixedText(WidgetTree, FixedLabel(TEXT("HIDE"), 4), S.CaptionSize, Crt::DimGreen);
	Hide->AddChild(HiddenLabel);
	if (UButtonSlot* HS = Cast<UButtonSlot>(HiddenLabel->Slot)) { HS->SetPadding(FMargin(10.0f, 4.0f)); }
	Hide->OnClicked.AddDynamic(this, &UReferenceWidget::OnToggleHidden); Add(Hide);
	Bar->AddChildToHorizontalBox(WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass()))->SetSize(ESlateSizeRule::Fill);
	UButton* CloseDetail = Crt::Button(WidgetTree, TEXT("[ CLOSE ]"), S.CaptionSize, Crt::DimGreen);
	CloseDetail->OnClicked.AddDynamic(this, &UReferenceWidget::OnCloseDetail); Add(CloseDetail);
	Into->AddChildToVerticalBox(Bar)->SetPadding(FMargin(0, 0, 0, 0));   // straight onto the render: the gap under it was air
	DetailNote = Crt::FixedText(WidgetTree, TEXT(""), S.RowSize, Crt::DimGreen);
	Into->AddChildToVerticalBox(DetailNote)->SetPadding(FMargin(0, 0, 0, 6));
}

void UReferenceWidget::BuildDetail(UVerticalBox* Into)
{
	const FSheetSpec& S = FSheetSpec::Get();
	// The render, and the controls laid over its foot rather than under it: the same shape as
	// the booth's target, so nothing is stretched either way. Held to ViewerWidth at the left of
	// the column; the catalogue's points are drawn over it in NativePaint.
	USizeBox* FeedBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	FeedBox->SetMinAspectRatio(1.618f); FeedBox->SetMaxAspectRatio(1.618f);   // golden ratio, landscape: weapons are long
	FeedBox->SetWidthOverride(ViewerWidth);
	FeedBox->SetHeightOverride(FMath::RoundToFloat(PaneShape::HeightFor(PaneShape::WeaponPreview, ViewerWidth)));
	UOverlay* FeedStack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	WeaponFeed = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	WeaponFeed->SetColorAndOpacity(FLinearColor::White);
	if (UOverlaySlot* ImageSlot = FeedStack->AddChildToOverlay(WeaponFeed)) { ImageSlot->SetHorizontalAlignment(HAlign_Fill); ImageSlot->SetVerticalAlignment(VAlign_Fill); }
	UHorizontalBox* Controls = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Controls->AddChildToHorizontalBox(WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass()))->SetSize(ESlateSizeRule::Fill);
	// The metadata -- the points over the render and the table under it -- on or off.
	UButton* MetaButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	MetaWidget = MetaButton;
	MetaButton->SetStyle(Crt::ButtonStyle());
	MetaToggleLabel = Crt::FixedText(WidgetTree, FixedLabel(TEXT("META: ON"), 9), S.CaptionSize, Crt::DimGreen);
	MetaButton->AddChild(MetaToggleLabel);
	if (UButtonSlot* MS = Cast<UButtonSlot>(MetaToggleLabel->Slot)) { MS->SetPadding(FMargin(10.0f, 4.0f)); }
	MetaButton->OnClicked.AddDynamic(this, &UReferenceWidget::OnToggleMeta);
	Controls->AddChildToHorizontalBox(MetaButton)->SetPadding(FMargin(0, 0, 10, 0));
	UButton* Reset = Crt::Button(WidgetTree, TEXT("[ RESET VIEW ]"), S.CaptionSize, Crt::DimGreen);
	Reset->OnClicked.AddDynamic(this, &UReferenceWidget::OnResetView);
	Controls->AddChildToHorizontalBox(Reset)->SetPadding(FMargin(0, 0, 10, 0));
	UButton* ResetPts = Crt::Button(WidgetTree, TEXT("[ RESET POINTS ]"), S.CaptionSize, Crt::DimGreen);
	ResetPointsWidget = ResetPts;
	ResetPts->OnClicked.AddDynamic(this, &UReferenceWidget::OnResetPoints);
	Controls->AddChildToHorizontalBox(ResetPts)->SetPadding(FMargin(0, 0, 10, 0));
	// (the FIRE button that stood here was taken out at the user's request; OnFire stays wired for the console)
	if (UOverlaySlot* ControlSlot = FeedStack->AddChildToOverlay(Controls))
	{
		ControlSlot->SetHorizontalAlignment(HAlign_Fill);
		ControlSlot->SetVerticalAlignment(VAlign_Bottom);
		ControlSlot->SetPadding(FMargin(12.0f, 0.0f, 12.0f, 10.0f));
	}
	// The points' names over the render, top left, each in its dot's colour (FillMetaTable).
	MetaLegend = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	MetaLegend->SetVisibility(ESlateVisibility::Collapsed);
	if (UOverlaySlot* LegendSlot = FeedStack->AddChildToOverlay(MetaLegend))
	{
		LegendSlot->SetHorizontalAlignment(HAlign_Left);
		LegendSlot->SetVerticalAlignment(VAlign_Top);
		LegendSlot->SetPadding(FMargin(12.0f, 10.0f, 0.0f, 0.0f));
	}
	FeedBox->AddChild(FeedStack);
	// The render with a column of buttons to its right. GIVE puts one of the weapon in the bag.
	UHorizontalBox* FeedRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	FeedRow->AddChildToHorizontalBox(FeedBox);
	{
		UVerticalBox* Side = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		GiveWidget = Side;
		UButton* GiveBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		GiveBtn->SetStyle(Crt::ButtonStyle());
		UTextBlock* GiveText = Crt::FixedText(WidgetTree, TEXT("[ GIVE ]"), S.CaptionSize, Crt::Green);
		GiveBtn->AddChild(GiveText);
		if (UButtonSlot* GS = Cast<UButtonSlot>(GiveText->Slot)) { GS->SetPadding(FMargin(10.0f, 4.0f)); }
		GiveBtn->OnClicked.AddDynamic(this, &UReferenceWidget::OnGive);
		Side->AddChildToVerticalBox(GiveBtn)->SetHorizontalAlignment(HAlign_Left);
		// TUNE HANDS under it: the hand-tuning page for this weapon (the column hides with GIVE for anything that is not a weapon).
		UButton* TuneBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		TuneBtn->SetStyle(Crt::ButtonStyle());
		UTextBlock* TuneText = Crt::FixedText(WidgetTree, TEXT("[ TUNE HANDS ]"), S.CaptionSize, Crt::Green);
		TuneBtn->AddChild(TuneText);
		if (UButtonSlot* TS = Cast<UButtonSlot>(TuneText->Slot)) { TS->SetPadding(FMargin(10.0f, 4.0f)); }
		TuneBtn->OnClicked.AddDynamic(this, &UReferenceWidget::OnTuneHands);
		// THE PAINT, under the render. A Synty pack ships each atlas in lettered variants -- the same
		// mesh, a different colourway -- and until now the only way to choose one here was to type
		// the variant's name into the skin field. It steps on a click and says where it is.
		UButton* SkinBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		SkinBtn->SetStyle(Crt::ButtonStyle());
		SkinLabel = Crt::FixedText(WidgetTree, TEXT("[ SKIN ]"), S.CaptionSize, Crt::Green);
		SkinBtn->AddChild(SkinLabel);
		if (UButtonSlot* KS = Cast<UButtonSlot>(SkinLabel->Slot)) { KS->SetPadding(FMargin(10.0f, 4.0f)); }
		SkinBtn->OnClicked.AddDynamic(this, &UReferenceWidget::OnCycleSkin);
		SkinButton = SkinBtn;
		{ UVerticalBoxSlot* KS2 = Side->AddChildToVerticalBox(SkinBtn); KS2->SetPadding(FMargin(0, 8, 0, 0)); KS2->SetHorizontalAlignment(HAlign_Left); }
		// The same pair as the tuning page: the cycler is a fitting on the RENDER, and this is what
		// makes it the paint the weapon wears in the world and in its icon. Cycling used to write the
		// catalogue on every press, so flicking through to look repainted every copy in the level.
		UButton* DefBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		DefBtn->SetStyle(Crt::ButtonStyle());
		SkinDefaultLabel = Crt::FixedText(WidgetTree, TEXT("[ SET DEFAULT ]"), S.CaptionSize, Crt::DimGreen);
		DefBtn->AddChild(SkinDefaultLabel);
		if (UButtonSlot* DS = Cast<UButtonSlot>(SkinDefaultLabel->Slot)) { DS->SetPadding(FMargin(10.0f, 4.0f)); }
		DefBtn->OnClicked.AddDynamic(this, &UReferenceWidget::OnSetDefaultSkin);
		SkinDefaultButton = DefBtn;
		{ UVerticalBoxSlot* DS2 = Side->AddChildToVerticalBox(DefBtn); DS2->SetPadding(FMargin(0, 4, 0, 0)); DS2->SetHorizontalAlignment(HAlign_Left); }
		{ UVerticalBoxSlot* TS = Side->AddChildToVerticalBox(TuneBtn); TS->SetPadding(FMargin(0, 8, 0, 0)); TS->SetHorizontalAlignment(HAlign_Left); }
		// MOVE TO: one button per OTHER category, in the space under the side column. A button each
		// rather than a dropdown because recategorising is a rare, deliberate act and the whole point
		// is to see at a glance where a thing could go. Refresh hides the one you are already in.
		MoveBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		MoveBox->AddChildToVerticalBox(Crt::FixedText(WidgetTree, TEXT("MOVE TO"), S.CaptionSize, Crt::Faint))->SetPadding(FMargin(0, 0, 0, 4));
		{
			auto AddMove = [&](const TCHAR* Label) -> UButton*
			{
				UButton* B = Crt::Button(WidgetTree, Label, S.CaptionSize, Crt::DimGreen);
				MoveButtons.Add(B);
				UVerticalBoxSlot* MS = MoveBox->AddChildToVerticalBox(B);
				MS->SetPadding(FMargin(0, 0, 0, 4)); MS->SetHorizontalAlignment(HAlign_Left);
				return B;
			};
			// In the same order as Categories, which is what Refresh indexes by when it hides the
			// button for the tab this entry is already on.
			AddMove(TEXT("[ WEAPONS => ]"))->OnClicked.AddDynamic(this, &UReferenceWidget::OnMoveToWeapons);
			AddMove(TEXT("[ OPTICS => ]"))->OnClicked.AddDynamic(this, &UReferenceWidget::OnMoveToOptics);
			AddMove(TEXT("[ ARMOR => ]"))->OnClicked.AddDynamic(this, &UReferenceWidget::OnMoveToArmor);
			AddMove(TEXT("[ EQUIPMENT => ]"))->OnClicked.AddDynamic(this, &UReferenceWidget::OnMoveToEquipment);
			AddMove(TEXT("[ CONSUMABLES => ]"))->OnClicked.AddDynamic(this, &UReferenceWidget::OnMoveToConsumables);
			AddMove(TEXT("[ OTHER => ]"))->OnClicked.AddDynamic(this, &UReferenceWidget::OnMoveToOther);
		}
		// NOT IN Side. That column is what GiveWidget points at -- the name is misleading, it is the
		// whole column, not the button -- and the refresh collapses it outright for anything that is
		// not a weapon. Recategorising is for every tab, and armour and consumables are exactly the
		// entries most likely to be in the wrong one, so MOVE TO hangs off the row instead and is
		// never hidden with the weapon-only controls.
		// ONE COLUMN, NOT TWO. These were added to the row side by side, MOVE TO first, and on a panel
		// this narrow the second column -- GIVE, SKIN, SET DEFAULT and TUNE HANDS -- was pushed clean
		// off the right edge. Nothing had been removed and nothing was hidden; the buttons were simply
		// outside the picture, which from the other side of the screen is indistinguishable from having
		// deleted them. Stacked instead: the weapon controls on top, MOVE TO beneath, one column wide.
		// Side keeps its own collapse (GiveWidget) without taking MOVE TO down with it.
		UVerticalBox* RightCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		RightCol->AddChildToVerticalBox(Side)->SetHorizontalAlignment(HAlign_Left);
		{ UVerticalBoxSlot* MB = RightCol->AddChildToVerticalBox(MoveBox); MB->SetPadding(FMargin(0, 22, 0, 0)); MB->SetHorizontalAlignment(HAlign_Left); }
		UHorizontalBoxSlot* ColSlot = FeedRow->AddChildToHorizontalBox(RightCol);
		ColSlot->SetPadding(FMargin(18, 0, 0, 0)); ColSlot->SetVerticalAlignment(VAlign_Top); ColSlot->SetHorizontalAlignment(HAlign_Left);
	}
	UVerticalBoxSlot* FeedSlot = Into->AddChildToVerticalBox(FeedRow);
	FeedSlot->SetHorizontalAlignment(HAlign_Left);
	FeedSlot->SetPadding(FMargin(0, 0, 0, 6));
	Into->AddChildToVerticalBox(UCrtRuleWidget::Make(GetOwningPlayer(), TEXT("--[ DETAILS ]-"), S.CaptionSize, Crt::DimGreen))->SetPadding(FMargin(0, 0, 0, 4));
	// Label and value on one line, the label in the same 170 px column the field rows use.
	auto Labelled = [&](const TCHAR* Caption, UWidget* Value, EVerticalAlignment ValueAlign) -> UTextBlock*
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		LabelBox->SetWidthOverride(170.0f);
		UTextBlock* L = Crt::FixedText(WidgetTree, Caption, S.RowSize, Crt::DimGreen);
		LabelBox->AddChild(L);
		UHorizontalBoxSlot* LS = Row->AddChildToHorizontalBox(LabelBox); LS->SetVerticalAlignment(ValueAlign == VAlign_Top ? VAlign_Top : VAlign_Center); LS->SetPadding(FMargin(0, ValueAlign == VAlign_Top ? 5 : 0, 0, 0));
		UHorizontalBoxSlot* VS = Row->AddChildToHorizontalBox(Value); VS->SetSize(ESlateSizeRule::Fill); VS->SetVerticalAlignment(ValueAlign);
		Into->AddChildToVerticalBox(Row)->SetPadding(FMargin(0, 1));
		return L;
	};
	NameBox = StyledBox(WidgetTree, S.CaptionSize);
	NameLabel = Labelled(TEXT("NAME"), NameBox, VAlign_Center);
	// A weapon or an optic is a MAKE and a MODEL on one line (its name stays underneath as the
	// record's identity); the row stands in for NAME on those cards and folds away on the others.
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		USizeBox* MakeCell = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass()); MakeCell->SetWidthOverride(170.0f);
		MakeLabel = Crt::FixedText(WidgetTree, TEXT("MAKE"), S.RowSize, Crt::DimGreen); MakeCell->AddChild(MakeLabel);
		Row->AddChildToHorizontalBox(MakeCell)->SetVerticalAlignment(VAlign_Center);
		MakeBox = StyledBox(WidgetTree, S.CaptionSize);
		{ UHorizontalBoxSlot* MS = Row->AddChildToHorizontalBox(MakeBox); MS->SetSize(ESlateSizeRule::Fill); MS->SetVerticalAlignment(VAlign_Center); }
		ModelLabel = Crt::FixedText(WidgetTree, TEXT("MODEL"), S.RowSize, Crt::DimGreen);
		{ UHorizontalBoxSlot* ML = Row->AddChildToHorizontalBox(ModelLabel); ML->SetVerticalAlignment(VAlign_Center); ML->SetPadding(FMargin(16, 0, 10, 0)); }
		ModelBox = StyledBox(WidgetTree, S.CaptionSize);
		{ UHorizontalBoxSlot* MS = Row->AddChildToHorizontalBox(ModelBox); MS->SetSize(ESlateSizeRule::Fill); MS->SetVerticalAlignment(VAlign_Center); }
		Into->AddChildToVerticalBox(Row)->SetPadding(FMargin(0, 1));
	}
	DescBox = WidgetTree->ConstructWidget<UMultiLineEditableTextBox>(UMultiLineEditableTextBox::StaticClass());
	DescBox->WidgetStyle.SetFont(Crt::Fixed(S.InfoTextSize));
	DescBox->WidgetStyle.SetForegroundColor(Crt::Green);
	DescBox->WidgetStyle.SetFocusedForegroundColor(Crt::Green);
	DescBox->WidgetStyle.SetBackgroundImageNormal(FSlateColorBrush(Crt::PanelSolid));
	DescBox->WidgetStyle.SetBackgroundImageHovered(FSlateColorBrush(Crt::PanelSolid));
	DescBox->WidgetStyle.SetBackgroundImageFocused(FSlateColorBrush(Crt::Faint));
	DescBox->WidgetStyle.SetPadding(FMargin(8.0f, 4.0f));
	DescBox->WidgetStyle.TextStyle.SetSelectedBackgroundColor(FSlateColor(Crt::DimGreen));
	DescBox->WidgetStyle.TextStyle.SetHighlightColor(Crt::Green);
	USizeBox* DescSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	DescSize->SetMinDesiredHeight(96.0f);
	DescSize->AddChild(DescBox);
	Labelled(TEXT("DESCRIPTION"), DescSize, VAlign_Top);
	// SOUND, then the cycled controls, all on one line: HIP FIRE is the flag the carry logic
	// reads (fired without the sights, a hip-fire weapon comes up to the hip instead of the
	// shoulder); STANCE is the folder of clips the body holds it with, cycled rather than typed
	// because a misspelt one is a weapon nobody can hold; OPTIC is the sight fitted, or none.
	UHorizontalBox* Pair = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	{
		USizeBox* SoundCell = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass()); SoundCell->SetWidthOverride(170.0f);
		SoundCell->AddChild(Crt::FixedText(WidgetTree, TEXT("SOUND"), S.RowSize, Crt::DimGreen));
		Pair->AddChildToHorizontalBox(SoundCell)->SetVerticalAlignment(VAlign_Center);
		SoundBox = StyledBox(WidgetTree, S.CaptionSize);
		UHorizontalBoxSlot* SoundSlot = Pair->AddChildToHorizontalBox(SoundBox); SoundSlot->SetSize(ESlateSizeRule::Fill); SoundSlot->SetVerticalAlignment(VAlign_Center); SoundSlot->SetPadding(FMargin(0, 0, 16, 0));
	}
	auto Control = [&](const TCHAR* Caption, UButton* Button) -> UWidget*
	{
		UHorizontalBox* Cell = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Cell->AddChildToHorizontalBox(Crt::FixedText(WidgetTree, Caption, S.RowSize, Crt::DimGreen))->SetPadding(FMargin(0, 0, 8, 0));
		Cell->AddChildToHorizontalBox(Button);
		for (int32 k = 0; k < Cell->GetChildrenCount(); ++k) { if (UHorizontalBoxSlot* CS = Cast<UHorizontalBoxSlot>(Cell->GetChildAt(k)->Slot)) { CS->SetVerticalAlignment(VAlign_Center); } }
		UHorizontalBoxSlot* PS = Pair->AddChildToHorizontalBox(Cell); PS->SetSize(ESlateSizeRule::Automatic); PS->SetVerticalAlignment(VAlign_Center); PS->SetPadding(FMargin(0, 0, 16, 0));
		return Cell;
	};
	HipFireButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	HipFireButton->SetStyle(Crt::ButtonStyle());
	HipFireLabel = Crt::FixedText(WidgetTree, FixedLabel(TEXT("   NO"), 7), S.CaptionSize, Crt::DimGreen);
	HipFireButton->AddChild(HipFireLabel);
	if (UButtonSlot* HS = Cast<UButtonSlot>(HipFireLabel->Slot)) { HS->SetPadding(FMargin(8.0f, 2.0f)); }
	HipFireButton->OnClicked.AddDynamic(this, &UReferenceWidget::OnHipFire);
	HipWidget = Control(TEXT("HIP FIRE"), HipFireButton);
	UButton* StanceButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	StanceButton->SetStyle(Crt::ButtonStyle());
	StanceLabel = Crt::FixedText(WidgetTree, FixedLabel(TEXT("-"), 7), S.CaptionSize, Crt::Green);
	StanceButton->AddChild(StanceLabel);
	if (UButtonSlot* SS = Cast<UButtonSlot>(StanceLabel->Slot)) { SS->SetPadding(FMargin(8.0f, 2.0f)); }
	StanceButton->OnClicked.AddDynamic(this, &UReferenceWidget::OnCycleStance);
	StanceWidget = Control(TEXT("STANCE"), StanceButton);
	UButton* OpticButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	OpticButton->SetStyle(Crt::ButtonStyle());
	OpticLabel = Crt::FixedText(WidgetTree, FixedLabel(TEXT("NONE"), 10), S.CaptionSize, Crt::Green);
	OpticButton->AddChild(OpticLabel);
	if (UButtonSlot* OS = Cast<UButtonSlot>(OpticLabel->Slot)) { OS->SetPadding(FMargin(8.0f, 2.0f)); }
	OpticButton->OnClicked.AddDynamic(this, &UReferenceWidget::OnCycleOptic);
	OpticWidget = Control(TEXT("OPTIC"), OpticButton);
	Into->AddChildToVerticalBox(Pair)->SetPadding(FMargin(0, 3, 0, 4));
	// The form: filled per item (FillFields), every field that applies to it.
	FieldsBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Into->AddChildToVerticalBox(FieldsBox)->SetPadding(FMargin(0, 0, 0, 6));
}

// One card per entry: the render square on the left, then the name, a kind / pack line and
// the description; the whole card is a button that opens the detail column.
void UReferenceWidget::FillList()
{
	if (!List) { return; }
	const FSheetSpec& S = FSheetSpec::Get();
	List->ClearChildren();
	CardBindings.Reset(); ShownIndices.Reset();
	for (int32 i = 0; i < Entries.Num(); ++i)
	{
		const FReferenceEntry& E = Entries[i];
		if (E.Category != CurrentCategory) { continue; }
		if (!bShowHidden && E.Fields.FindRef(TEXT("hidden")) == TEXT("true")) { continue; }
		if (!Filter.IsEmpty() && !E.Name.Contains(Filter) && !E.Kind.Contains(Filter) && !E.Pack.Contains(Filter) && !E.Description.Contains(Filter) && !E.Fields.FindRef(TEXT("make")).Contains(Filter) && !E.Fields.FindRef(TEXT("model")).Contains(Filter)) { continue; }
		UButton* CardButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		CardButton->SetStyle(Crt::ButtonStyle());
		UReferenceCardBinding* Binding = NewObject<UReferenceCardBinding>(this);
		Binding->Widget = this; Binding->Index = i;
		CardButton->OnClicked.AddDynamic(Binding, &UReferenceCardBinding::OnClicked);
		CardBindings.Add(Binding);
		UHorizontalBox* Card = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UInventoryGridWidget* Square = CreateWidget<UInventoryGridWidget>(GetOwningPlayer(), UInventoryGridWidget::StaticClass());
		Square->SetGap(S.InventoryGap);
		Square->Configure(TEXT(""), 1, 1, CardIcon);
		Square->SetItems({ E.Icon.IsEmpty() ? E.Name : E.Icon });   // the render by key; the key as the fallback text
		Square->SetVisibility(ESlateVisibility::HitTestInvisible);   // the card button takes the click
		UHorizontalBoxSlot* SquareSlot = Card->AddChildToHorizontalBox(Square);
		SquareSlot->SetVerticalAlignment(VAlign_Top); SquareSlot->SetPadding(FMargin(0, 0, 16, 0));
		UVerticalBox* Text = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		{
			const FString Make = E.Fields.FindRef(TEXT("make")), Model = E.Fields.FindRef(TEXT("model"));
			if (!Make.IsEmpty()) { Text->AddChildToVerticalBox(Crt::FixedText(WidgetTree, Make.ToUpper(), S.CaptionSize, Crt::DimGreen))->SetPadding(FMargin(0, 0, 0, 2)); }
			Text->AddChildToVerticalBox(Crt::FixedText(WidgetTree, (Model.IsEmpty() ? E.Name : Model).ToUpper(), S.InfoNameSize, i == SelectedIndex ? FLinearColor(0.75f, 1.0f, 0.8f) : Crt::Green))->SetPadding(FMargin(0, 0, 0, 6));
		}
		UTextBlock* Desc = Crt::FixedText(WidgetTree, E.Description, S.InfoTextSize, Crt::DimGreen);
		Desc->SetAutoWrapText(true);
		Text->AddChildToVerticalBox(Desc);
		UHorizontalBoxSlot* TextSlot = Card->AddChildToHorizontalBox(Text);
		TextSlot->SetSize(ESlateSizeRule::Fill); TextSlot->SetVerticalAlignment(VAlign_Top);
		CardButton->AddChild(Card);
		if (UButtonSlot* BS = Cast<UButtonSlot>(Card->Slot)) { BS->SetPadding(FMargin(6.0f, 6.0f)); BS->SetHorizontalAlignment(HAlign_Fill); }
		if (UScrollBoxSlot* RS = Cast<UScrollBoxSlot>(List->AddChild(CardButton))) { RS->SetPadding(FMargin(0, 4, 18, 4)); }
		if (UScrollBoxSlot* RS = Cast<UScrollBoxSlot>(List->AddChild(UCrtRuleWidget::Make(GetOwningPlayer(), TEXT("-"), S.RuleSize, Crt::Faint)))) { RS->SetPadding(FMargin(0, 0, 18, 0)); }
		ShownIndices.Add(i);
	}
	int32 InCategory = 0;
	for (const FReferenceEntry& E : Entries) { if (E.Category == CurrentCategory) { ++InCategory; } }
	if (CountText) { CountText->SetText(FText::FromString(FString::Printf(TEXT("%d OF %d %s"), ShownIndices.Num(), InCategory, *CurrentCategory.ToUpper()))); }
}

void UReferenceWidget::SelectEntry(int32 Index)
{
	if (!Entries.IsValidIndex(Index) || !DetailBox) { return; }
	SelectedIndex = Index;
	const FReferenceEntry& E = Entries[Index];
	if (DetailTitle) { DetailTitle->SetText(FText::FromString(TitleOf(E))); }
	if (DetailMesh)
	{
		FString Shown = E.Mesh;
		Shown.RemoveFromStart(TEXT("/Game/RepliCan/"));
		Shown.RemoveFromStart(TEXT("/Game/"));
		DetailMesh->SetText(FText::FromString(Shown));
		DetailMesh->SetRenderTranslation(FVector2D::ZeroVector);   // a new entry reads from the start
		MeshScroll = 0.0f;
		MeshScrollWait = MeshScrollHoldSeconds;
	}
	if (HiddenLabel) { const bool bHid = E.Fields.FindRef(TEXT("hidden")) == TEXT("true"); HiddenLabel->SetText(FText::FromString(FixedLabel(bHid ? TEXT("SHOW") : TEXT("HIDE"), 4))); HiddenLabel->SetColorAndOpacity(FSlateColor(bHid ? Crt::Green : Crt::DimGreen)); }
	if (NameBox) { NameBox->SetText(FText::FromString(E.Name)); }
	{
		const bool bGear = E.bWeaponsFile || E.Category == TEXT("optics");
		if (MakeBox) { MakeBox->SetText(FText::FromString(E.Fields.FindRef(TEXT("make")))); }
		if (ModelBox) { ModelBox->SetText(FText::FromString(E.Fields.FindRef(TEXT("model")))); }
		if (UWidget* Row = MakeBox ? MakeBox->GetParent() : nullptr) { Row->SetVisibility(bGear ? ESlateVisibility::Visible : ESlateVisibility::Collapsed); }
		if (UWidget* Row = NameBox ? NameBox->GetParent() : nullptr) { Row->SetVisibility(bGear ? ESlateVisibility::Collapsed : ESlateVisibility::Visible); }
	}
	bHipFireValue = E.bHipFire;
	ShowHipFire();
	StanceValue = E.Stance;
	OpticValue = E.Fields.FindRef(TEXT("optic")); ShowOptic();
	ShowStance();
	FillFields(E);
	const bool bWeapon = E.Category == TEXT("weapons");
	const bool bOpticEntry = E.Category == TEXT("optics");
	if (ResetPointsWidget) { ResetPointsWidget->SetVisibility((bWeapon || bOpticEntry) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed); }   // nothing to reset on an item without points
	for (UWidget* W : { FireWidget.Get(), HipWidget.Get(), StanceWidget.Get(), OpticWidget.Get() }) { if (W) { W->SetVisibility(bWeapon ? ESlateVisibility::Visible : ESlateVisibility::Collapsed); } }
	if (GiveWidget) { GiveWidget->SetVisibility(E.bWeaponsFile ? ESlateVisibility::Visible : ESlateVisibility::Collapsed); }   // a shield can be given too
	// The MOVE TO list, less wherever this entry already is -- there is no sense offering to move a
	// rifle to WEAPONS. Set here, beside the other per-entry visibility, rather than in the skin
	// refresh where it used to sit: that one is about paint, and a reader fixing it would have no
	// reason to look there.
	{
		for (int32 i = 0; i < MoveButtons.Num() && i < UE_ARRAY_COUNT(Categories); ++i)
		{
			if (MoveButtons[i]) { MoveButtons[i]->SetVisibility(E.Category == Categories[i] ? ESlateVisibility::Collapsed : ESlateVisibility::Visible); }
		}
	}
	RefreshSkinLabel();
	if (MetaWidget) { MetaWidget->SetVisibility((bWeapon || bOpticEntry) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed); }
	FillMetaTable(bWeapon ? WeaponCatalog::Find(E.Name) : nullptr);
	if (SoundBox) { SoundBox->SetText(FText::FromString(E.Sound)); }
	if (DescBox) { DescBox->SetText(FText::FromString(E.Description)); }
	if (DetailNote) { DetailNote->SetText(FText::GetEmpty()); }
	DetailBox->SetVisibility(ESlateVisibility::Visible);
	if (OwnerController) { OwnerController->ShowWeaponPreview(E.Mesh, E.bWeaponsFile ? E.Name : FString()); }
	RefreshFeed();
	FillList();   // the picked card lights up
}

void UReferenceWidget::RefreshFeed()
{
	if (!WeaponFeed) { return; }
	UTextureRenderTarget2D* Target = OwnerController ? OwnerController->GetWeaponPreviewFeed() : nullptr;
	if (Target && WeaponFeed->GetBrush().GetResourceObject() != Target)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(Target);
		Brush.ImageSize = FVector2D(Target->SizeX, Target->SizeY);
		Brush.DrawAs = ESlateBrushDrawType::Image;
		WeaponFeed->SetBrush(Brush);
	}
	WeaponFeed->SetVisibility(Target ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
}

void UReferenceWidget::OnFire()
{
	if (OwnerController) { OwnerController->FireWeaponPreview(Entries.IsValidIndex(SelectedIndex) ? Entries[SelectedIndex].Sound : FString()); }
}

void UReferenceWidget::OnGive()
{
	if (!OwnerController || !Entries.IsValidIndex(SelectedIndex) || !Entries[SelectedIndex].bWeaponsFile) { return; }
	const FString& Name = Entries[SelectedIndex].Name;
	const bool bOk = OwnerController->AddToInventory(Name, true);
	if (DetailNote) { DetailNote->SetText(FText::FromString(bOk ? FString::Printf(TEXT("GIVEN: %s IS IN THE BAG"), *WeaponCatalog::DisplayName(Name).ToUpper()) : TEXT("NO ROOM IN THE BAG"))); }
}

void UReferenceWidget::OnResetView() { if (OwnerController) { OwnerController->ResetWeaponPreviewView(); } }

void UReferenceWidget::RefreshSkinLabel()
{
	const FReferenceEntry* E = Entries.IsValidIndex(SelectedIndex) ? &Entries[SelectedIndex] : nullptr;
	const WeaponCatalog::FWeapon* W = (E && E->bWeaponsFile) ? WeaponCatalog::Find(E->Name) : nullptr;
	const TArray<FString> V = W ? WeaponSkins::Variants(*W) : TArray<FString>();
	// Nothing to choose between is not a control: a weapon whose atlas ships in one colourway hides it.
	if (SkinButton) { SkinButton->SetVisibility(V.Num() >= 2 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed); }
	if (SkinDefaultButton) { SkinDefaultButton->SetVisibility(V.Num() >= 2 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed); }
	if (SkinDefaultLabel) { SkinDefaultLabel->SetColorAndOpacity(FSlateColor(TriedSkin.IsEmpty() ? Crt::Faint : Crt::Green)); }
	if (SkinLabel && V.Num() >= 2)
	{
		const FString Wearing = TriedSkin.IsEmpty() ? WeaponSkins::Current(*W) : TriedSkin;
		SkinLabel->SetText(FText::FromString(FString::Printf(TEXT("[ SKIN %d/%d ]"), V.IndexOfByKey(Wearing) + 1, V.Num())));
	}
}

void UReferenceWidget::OnSetDefaultSkin()
{
	if (!OwnerController || TriedSkin.IsEmpty() || !Entries.IsValidIndex(SelectedIndex)) { return; }
	const FString Name = Entries[SelectedIndex].Name;
	const WeaponCatalog::FWeapon* W = WeaponCatalog::Find(Name);
	if (!W) { return; }
	const FString Key = W->Key;   // the write re-reads the catalogue, so W is stale after it
	if (!WeaponCatalog::WriteStringField(Key, TEXT("skin"), TriedSkin)) { return; }
	TriedSkin.Empty();
	OwnerController->SetWeaponPreviewSkin(Name, FString());   // the render is showing the catalogue again
	OwnerController->RefreshHeldWeapon();
	OwnerController->RefreshTunedWeapon(Name);          // and everyone carrying one
	RefreshSkinLabel();
}

void UReferenceWidget::OnCycleSkin()
{
	if (!OwnerController || !Entries.IsValidIndex(SelectedIndex) || !Entries[SelectedIndex].bWeaponsFile) { return; }
	const WeaponCatalog::FWeapon* W = WeaponCatalog::Find(Entries[SelectedIndex].Name);
	if (!W) { return; }
	const TArray<FString> V = WeaponSkins::Variants(*W);
	if (V.Num() < 2) { return; }
	const FString Wearing = TriedSkin.IsEmpty() ? WeaponSkins::Current(*W) : TriedSkin;
	TriedSkin = V[(FMath::Max(0, V.IndexOfByKey(Wearing)) + 1) % V.Num()];
	OwnerController->SetWeaponPreviewSkin(Entries[SelectedIndex].Name, TriedSkin);
	// E.Mesh, the same field the selection path renders from. Looking it up in Fields instead
	// returned an empty string -- the entry keeps its mesh in its own member, not in the generic
	// map -- and an empty path is a preview with nothing in it, which is why cycling the paint
	// made the render disappear.
	RefreshSkinLabel();
}

void UReferenceWidget::OnTuneHands()
{
	if (!OwnerController || !Entries.IsValidIndex(SelectedIndex)) { return; }
	const FReferenceEntry& E = Entries[SelectedIndex];
	if (!E.bWeaponsFile) { if (DetailNote) { DetailNote->SetText(FText::FromString(TEXT("HANDS ARE TUNED ON WEAPONS"))); } return; }
	OwnerController->ShowHandTune(E.Name);   // the catalogue is looked up by item name, as GIVE does
}

void UReferenceWidget::OnMoveToWeapons()     { MoveSelectedTo(TEXT("weapons")); }
void UReferenceWidget::OnMoveToOptics()      { MoveSelectedTo(TEXT("optics")); }
void UReferenceWidget::OnMoveToArmor()       { MoveSelectedTo(TEXT("armor")); }
void UReferenceWidget::OnMoveToEquipment()   { MoveSelectedTo(TEXT("equipment")); }
void UReferenceWidget::OnMoveToConsumables() { MoveSelectedTo(TEXT("consumables")); }
void UReferenceWidget::OnMoveToOther()       { MoveSelectedTo(TEXT("other")); }

void UReferenceWidget::MoveSelectedTo(const FString& NewCategory)
{
	if (!Entries.IsValidIndex(SelectedIndex)) { return; }
	const FString Note = MoveEntryTo(Entries[SelectedIndex], NewCategory);
	if (DetailNote) { DetailNote->SetText(FText::FromString(Note)); }
}

// Where a category's records live: the weapons file keeps "weapons" and "optics" in two blocks of
// its own; the other four are one field inside the items file.
static bool InWeaponsFile(const FString& Cat) { return Cat == TEXT("weapons") || Cat == TEXT("optics"); }

FString UReferenceWidget::MoveEntryTo(FReferenceEntry& E, const FString& NewCategory)
{
	if (NewCategory == E.Category) { return TEXT("ALREADY THERE"); }
	const bool bFromWeapons = InWeaponsFile(E.Category);
	const bool bToWeapons = InWeaponsFile(NewCategory);

	// The easy case: both homes are the items file, where the category is only a field.
	if (!bFromWeapons && !bToWeapons)
	{
		const FString Was = E.Category;
		E.Category = NewCategory;
		E.Fields.Add(TEXT("category"), NewCategory);
		if (!SaveItemEntry(E)) { E.Category = Was; return TEXT("MOVE FAILED"); }
		Rebuild();
		return FString::Printf(TEXT("MOVED TO %s"), *NewCategory.ToUpper());
	}

	// Otherwise the record itself has to change container. Read both sides first.
	auto Load = [](const FString& Path, TSharedPtr<FJsonObject>& Out) -> bool
	{
		FString Json;
		if (!FFileHelper::LoadFileToString(Json, *Path)) { return false; }
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		return FJsonSerializer::Deserialize(Reader, Out) && Out.IsValid();
	};
	auto Save = [](const FString& Path, const TSharedPtr<FJsonObject>& Root) -> bool
	{
		FString Out;
		TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Out);
		if (!FJsonSerializer::Serialize(Root.ToSharedRef(), Writer)) { return false; }
		return FFileHelper::SaveStringToFile(Out, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	};
	const FString FromPath = bFromWeapons ? CatalogueFile() : RefItemsFile();
	const FString ToPath = bToWeapons ? CatalogueFile() : RefItemsFile();
	TSharedPtr<FJsonObject> FromRoot, ToRoot;
	if (!Load(FromPath, FromRoot)) { return TEXT("COULD NOT READ THE SOURCE FILE"); }
	const bool bSameFile = FromPath == ToPath;
	if (bSameFile) { ToRoot = FromRoot; }
	else if (!Load(ToPath, ToRoot)) { return TEXT("COULD NOT READ THE DESTINATION FILE"); }

	// The block each side lives in. In the items file every record sits in one "items" object and
	// the category is a field on it; in the weapons file the block IS the category.
	auto BlockName = [](const FString& Cat) { return InWeaponsFile(Cat) ? Cat : FString(TEXT("items")); };
	const TSharedPtr<FJsonObject>* FromBlock = nullptr;
	if (!FromRoot->TryGetObjectField(BlockName(E.Category), FromBlock) || !FromBlock) { return TEXT("THE SOURCE BLOCK IS MISSING"); }
	const TSharedPtr<FJsonObject>* Record = nullptr;
	if (!(*FromBlock)->TryGetObjectField(E.Key, Record) || !Record) { return TEXT("THE RECORD IS NOT WHERE IT SHOULD BE"); }
	TSharedPtr<FJsonObject> Copy = MakeShared<FJsonObject>(**Record);
	if (!bToWeapons) { Copy->SetStringField(TEXT("category"), NewCategory); }
	else { Copy->RemoveField(TEXT("category")); }

	const TSharedPtr<FJsonObject>* ToBlock = nullptr;
	if (!ToRoot->TryGetObjectField(BlockName(NewCategory), ToBlock) || !ToBlock)
	{
		ToRoot->SetObjectField(BlockName(NewCategory), MakeShared<FJsonObject>());
		ToRoot->TryGetObjectField(BlockName(NewCategory), ToBlock);
		if (!ToBlock) { return TEXT("COULD NOT MAKE THE DESTINATION BLOCK"); }
	}
	(*ToBlock)->SetObjectField(E.Key, Copy);

	// DESTINATION FIRST. If this fails nothing has been taken away yet; if the removal below fails
	// the record exists twice, which is a tidy-up rather than a loss.
	if (!bSameFile && !Save(ToPath, ToRoot)) { return TEXT("COULD NOT WRITE THE DESTINATION"); }
	(*FromBlock)->RemoveField(E.Key);
	if (!Save(FromPath, FromRoot))
	{
		return TEXT("WRITTEN TO THE NEW TAB BUT NOT REMOVED FROM THE OLD -- IT IS IN BOTH");
	}
	if (bSameFile && !Save(ToPath, ToRoot)) { return TEXT("COULD NOT WRITE THE FILE"); }

	WeaponCatalog::Reload();
	ItemCatalog::Reload(true);
	const FString Name = E.Name;
	Rebuild();
	OpenEntryByName(Name);   // the card has moved tabs; find it again and show it there
	return FString::Printf(TEXT("MOVED TO %s -- CHECK ITS FIELDS"), *NewCategory.ToUpper());
}

void UReferenceWidget::OnSaveDetail()
{
	if (!Entries.IsValidIndex(SelectedIndex)) { return; }
	FReferenceEntry& E = Entries[SelectedIndex];
	if (NameBox) { E.Name = NameBox->GetText().ToString().TrimStartAndEnd(); }
	if (SoundBox) { E.Sound = SoundBox->GetText().ToString().TrimStartAndEnd(); }
	if (DescBox) { E.Description = DescBox->GetText().ToString().TrimStartAndEnd(); }
	E.bHipFire = bHipFireValue;
	E.Stance = StanceValue;
	ReadFieldsFromForm(E);
	if (E.Category == TEXT("weapons")) { E.Fields.Add(TEXT("optic"), OpticValue); }
	// A changed CATEGORY moves the entry to another tab. The record is written with it, and the
	// list is rebuilt from the file so the card turns up where it now belongs; the tab follows.
	FString NewCat = E.Fields.FindRef(TEXT("category")).ToLower().TrimStartAndEnd();
	static const TCHAR* Tabs[] = { TEXT("armor"), TEXT("equipment"), TEXT("consumables"), TEXT("other") };
	bool bKnown = false; for (const TCHAR* T : Tabs) { bKnown |= NewCat == T; }
	const bool bMoved = !E.bWeaponsFile && bKnown && NewCat != E.Category;
	if (!E.bWeaponsFile && !bKnown && !NewCat.IsEmpty()) { E.Fields.Add(TEXT("category"), E.Category); if (DetailNote) { DetailNote->SetText(FText::FromString(TEXT("CATEGORY must be armor, equipment, consumables or other"))); } }
	if (bMoved) { E.Category = NewCat; }
	const bool bOk = SaveEntry(E);
	if (bOk && (E.bWeaponsFile || E.Category == TEXT("optics")) && OwnerController) { OwnerController->RefreshHeldWeapon(); }   // the weapon in hand picks up its new optic and points now
	if (DetailNote && !(!E.bWeaponsFile && !bKnown && !NewCat.IsEmpty())) { DetailNote->SetText(FText::FromString(bOk ? ((E.bWeaponsFile || E.Category == TEXT("optics")) ? TEXT("SAVED TO UI/WEAPONS.JSON") : TEXT("SAVED TO UI/ITEMS.JSON")) : TEXT("SAVE FAILED"))); }
	if (DetailTitle) { DetailTitle->SetText(FText::FromString(TitleOf(E))); }
	if (bMoved)
	{
		const FString Key = E.Key;
		CurrentCategory = NewCat;
		LoadCatalogue();
		for (int32 i = 0; i < Entries.Num(); ++i) { if (Entries[i].Key == Key) { SelectEntry(i); break; } }
		if (DetailNote) { DetailNote->SetText(FText::FromString(FString::Printf(TEXT("MOVED TO %s"), *NewCat.ToUpper()))); }
		return;
	}
	FillList();
}

void UReferenceWidget::OnHipFire()
{
	bHipFireValue = !bHipFireValue;
	ShowHipFire();
}

void UReferenceWidget::ShowHipFire()
{
	if (!HipFireLabel) { return; }
	HipFireLabel->SetText(FText::FromString(FixedLabel(bHipFireValue ? TEXT("X  YES") : TEXT("   NO"), 7)));
	HipFireLabel->SetColorAndOpacity(FSlateColor(bHipFireValue ? Crt::Green : Crt::DimGreen));
}

// SOME SIGHTS ARE PART OF THE GUN. The live edit wins over the saved catalogue, so clearing
// OPTIC FIXED and cycling the sight in one visit works, and setting it bites before the save.
bool UReferenceWidget::SelectedOpticFixed() const
{
	if (const bool* Live = FieldBoolValues.Find(TEXT("optic_fixed"))) { return *Live; }
	if (Entries.IsValidIndex(SelectedIndex))
	{
		if (const FString* F = Entries[SelectedIndex].Fields.Find(TEXT("optic_fixed"))) { return *F == TEXT("true"); }
		if (const WeaponCatalog::FWeapon* W = WeaponCatalog::Find(Entries[SelectedIndex].Name)) { return W->bOpticFixed; }
	}
	return false;
}

void UReferenceWidget::OnCycleOptic()
{
	// A BUILT-IN SIGHT IS NOT A CHOICE. Refuse rather than write a fitting the game will not
	// honour -- and say why, because a control that does nothing reads as a fault. Reading the
	// optic stays normal: it keeps its kind, zoom and eye point, and the weapon still aims through it.
	if (SelectedOpticFixed())
	{
		if (DetailNote) { DetailNote->SetText(FText::FromString(TEXT("OPTIC IS BUILT INTO THIS WEAPON -- CLEAR OPTIC FIXED TO CHANGE IT"))); }
		return;
	}
	OpticNames = WeaponCatalog::OpticNames();
	OpticNames.Insert(FString(), 0);   // none first
	const int32 At = OpticNames.IndexOfByKey(OpticValue);
	OpticValue = OpticNames[(At + 1) % OpticNames.Num()];
	if (Entries.IsValidIndex(SelectedIndex))
	{
		Entries[SelectedIndex].Fields.Add(TEXT("optic"), OpticValue);
		if (TObjectPtr<UEditableTextBox>* Box = FieldBoxes.Find(TEXT("optic"))) { if (*Box) { (*Box)->SetText(FText::FromString(OpticValue)); } }
	}
	ShowOptic();
	FillMetaTable(Entries.IsValidIndex(SelectedIndex) ? WeaponCatalog::Find(Entries[SelectedIndex].Name) : nullptr);
}

void UReferenceWidget::ShowOptic()
{
	int32 Widest = 4;
	for (const FString& N : WeaponCatalog::OpticNames()) { Widest = FMath::Max(Widest, N.Len()); }
	// Shown either way -- a fixed sight is still a sight and its name is worth reading. Dimmed and
	// marked so it reads as not-editable rather than as broken; the button stays live so clicking
	// it can say why. The blanks keep the field one width in a fixed-pitch face.
	const bool bFixed = SelectedOpticFixed();
	if (OpticLabel)
	{
		OpticLabel->SetText(FText::FromString(FixedLabel(OpticValue.IsEmpty() ? TEXT("NONE") : OpticValue, Widest) + (bFixed ? TEXT(" FIXED") : TEXT("      "))));
		OpticLabel->SetColorAndOpacity(FSlateColor(bFixed ? Crt::DimGreen : Crt::Green));
	}
}

void UReferenceWidget::OnCycleStance()
{
	if (StanceNames.Num() == 0) { return; }
	const int32 At = StanceNames.IndexOfByKey(StanceValue);
	StanceValue = StanceNames[(At + 1) % StanceNames.Num()];
	ShowStance();
}

void UReferenceWidget::ShowStance()
{
	int32 Widest = 1;
	for (const FString& N : StanceNames) { Widest = FMath::Max(Widest, N.Len()); }
	if (StanceLabel) { StanceLabel->SetText(FText::FromString(FixedLabel(StanceValue.IsEmpty() ? TEXT("-") : StanceValue, Widest))); }
}

void UReferenceWidget::OnTabWeapons() { SetCategory(TEXT("weapons")); }
void UReferenceWidget::OnTabOptics() { SetCategory(TEXT("optics")); }
void UReferenceWidget::OnTabArmor() { SetCategory(TEXT("armor")); }
void UReferenceWidget::OnTabEquipment() { SetCategory(TEXT("equipment")); }
void UReferenceWidget::OnTabConsumables() { SetCategory(TEXT("consumables")); }
void UReferenceWidget::OnTabOther() { SetCategory(TEXT("other")); }

void UReferenceWidget::SetCategory(const FString& Category)
{
	if (CurrentCategory == Category) { return; }
	CurrentCategory = Category;
	OnCloseDetail();   // the open card belongs to the old tab
	ShowTabs();
	FillList();
}

void UReferenceWidget::ShowTabs()
{
	for (int32 i = 0; i < TabLabels.Num() && i < UE_ARRAY_COUNT(Categories); ++i)
	{
		if (!TabLabels[i]) { continue; }
		const bool bOn = CurrentCategory == Categories[i];
		const FString Upper = FString(Categories[i]).ToUpper();
		TabLabels[i]->SetText(FText::FromString(bOn ? FString::Printf(TEXT("[ %s ]"), *Upper) : FString::Printf(TEXT("  %s  "), *Upper)));
		TabLabels[i]->SetColorAndOpacity(FSlateColor(bOn ? Crt::Green : Crt::Faint));
	}
}

void UReferenceWidget::OnToggleShowHidden()
{
	bShowHidden = !bShowHidden;
	if (ShowHiddenLabel) { ShowHiddenLabel->SetText(FText::FromString(bShowHidden ? TEXT("[ X ] SHOW HIDDEN") : TEXT("[   ] SHOW HIDDEN"))); }
	FillList();
}

void UReferenceWidget::OnReviewedToday()
{
	if (!Entries.IsValidIndex(SelectedIndex)) { return; }
	Entries[SelectedIndex].Fields.Add(TEXT("reviewdate"), FDateTime::Now().ToString(TEXT("%Y-%m-%d")));
	if (ReviewLabel) { ReviewLabel->SetText(FText::FromString(TEXT("REVIEWED: ") + Entries[SelectedIndex].Fields[TEXT("reviewdate")])); }
	OnSaveDetail();   // the stamp is part of the entry: saved with everything else on the form
}

void UReferenceWidget::CycleEnumField(const FString& Key)
{
	const TArray<FString>* Options = FieldEnumOptions.Find(Key);
	if (!Options || Options->Num() == 0) { return; }
	FString& V = FieldEnumValues.FindOrAdd(Key);
	const int32 At = Options->IndexOfByKey(V);
	V = (*Options)[(At + 1) % Options->Num()];
	if (TObjectPtr<UTextBlock>* L = FieldEnumLabels.Find(Key)) { if (*L) { (*L)->SetText(FText::FromString(FixedLabel(TEXT("< ") + V.ToUpper() + TEXT(" >"), 10))); } }
}

void UReferenceWidget::ToggleBoolField(const FString& Key)
{
	bool& V = FieldBoolValues.FindOrAdd(Key);
	V = !V;
	if (TObjectPtr<UTextBlock>* L = FieldBoolLabels.Find(Key)) { if (*L) { (*L)->SetText(FText::FromString(FixedLabel(V ? TEXT("X  YES") : TEXT("   NO"), 7))); (*L)->SetColorAndOpacity(FSlateColor(V ? Crt::Green : Crt::DimGreen)); } }
}

void UReferenceWidget::FillFields(const FReferenceEntry& E)
{
	if (!FieldsBox) { return; }
	FieldsBox->ClearChildren();
	FieldBoxes.Reset(); if (MakeBox) { FieldBoxes.Add(TEXT("make"), MakeBox); } if (ModelBox) { FieldBoxes.Add(TEXT("model"), ModelBox); } FieldBoolLabels.Reset(); FieldBoolValues.Reset(); FieldEnumLabels.Reset(); FieldEnumValues.Reset(); FieldEnumOptions.Reset(); FieldBindings.Reset();
	const FSheetSpec& S = FSheetSpec::Get();
	if (ReviewLabel) { const FString D = E.Fields.FindRef(TEXT("reviewdate")); ReviewLabel->SetText(FText::FromString(TEXT("REVIEWED: ") + (D.IsEmpty() ? TEXT("never") : D))); }
	GroupBoxes.Reset(); GroupLabels.Reset();
	if (ExpandedGroups.Num() == 0) { ExpandedGroups.Add(TEXT("POINTS")); ExpandedGroups.Add(TEXT("WEAPON")); ExpandedGroups.Add(TEXT("USE")); }
	for (const TCHAR* GroupName : ItemFields::GroupOrder)
	{
		UVerticalBox* Rows = nullptr;
		for (const ItemFields::FField& F : ItemFields::Table)
		{
		if (FCString::Strcmp(F.Group, GroupName) != 0) { continue; }
		if (!ItemFields::Applies(F, E.Category)) { continue; }
		if (FCString::Strcmp(F.Key, TEXT("reviewdate")) == 0) { continue; }   // in the header
		if (FCString::Strcmp(F.Key, TEXT("make")) == 0 || FCString::Strcmp(F.Key, TEXT("model")) == 0) { continue; }
		if (FCString::Strcmp(F.Key, TEXT("shoulder")) == 0 && E.Stance.StartsWith(TEXT("Pistol"))) { continue; }   // no stock, no shoulder: a pistol indexes off the hand   // in the header too
		if (!Rows)
		{
			// The header folds its rows: a button whose caption says which way it is.
			const bool bOpen = ExpandedGroups.Contains(GroupName);
			UButton* HB = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
			HB->SetStyle(Crt::ButtonStyle());
			UTextBlock* HL = Crt::FixedText(WidgetTree, GroupHeader(GroupName, bOpen), S.CaptionSize, Crt::DimGreen);
			HB->AddChild(HL);
			if (UButtonSlot* BS = Cast<UButtonSlot>(HL->Slot)) { BS->SetPadding(FMargin(0.0f, 3.0f)); BS->SetHorizontalAlignment(HAlign_Left); }
			UReferenceFieldBinding* GB = NewObject<UReferenceFieldBinding>(this);
			GB->Widget = this; GB->Key = FString(TEXT("group:")) + GroupName;
			HB->OnClicked.AddDynamic(GB, &UReferenceFieldBinding::OnToggle);
			FieldBindings.Add(GB); GroupLabels.Add(GroupName, HL);
			{ UVerticalBoxSlot* HS = FieldsBox->AddChildToVerticalBox(HB); HS->SetPadding(FMargin(0, 5, 0, 2)); HS->SetHorizontalAlignment(HAlign_Left); }   // left, like every other header in the column
			Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			Rows->SetVisibility(bOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			FieldsBox->AddChildToVerticalBox(Rows);
			GroupBoxes.Add(GroupName, Rows);
		}
		const FString Value = E.Fields.FindRef(F.Key);
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		LabelBox->SetWidthOverride(170.0f);
		LabelBox->AddChild(Crt::FixedText(WidgetTree, F.Label, S.RowSize, Crt::DimGreen));
		UHorizontalBoxSlot* LS = Row->AddChildToHorizontalBox(LabelBox); LS->SetVerticalAlignment(VAlign_Center);
		switch (F.Type)
		{
		case ItemFields::EType::Enum:
		{
			// One of a fixed set of names: a button that steps through them, in the hint's order.
			UButton* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
			B->SetStyle(Crt::ButtonStyle());
			TArray<FString> Options; FString(F.Hint).ParseIntoArray(Options, TEXT(" "), true);
			const FString Shown = (Value.IsEmpty() && Options.Num() > 0) ? Options[0] : Value;
			UTextBlock* L = Crt::FixedText(WidgetTree, FixedLabel(TEXT("< ") + Shown.ToUpper() + TEXT(" >"), 10), S.CaptionSize, Crt::Green);
			B->AddChild(L);
			if (UButtonSlot* BS = Cast<UButtonSlot>(L->Slot)) { BS->SetPadding(FMargin(8.0f, 2.0f)); }
			UReferenceFieldBinding* Binding = NewObject<UReferenceFieldBinding>(this);
			Binding->Widget = this; Binding->Key = F.Key;
			B->OnClicked.AddDynamic(Binding, &UReferenceFieldBinding::OnCycle);
			FieldBindings.Add(Binding); FieldEnumLabels.Add(F.Key, L); FieldEnumValues.Add(F.Key, Shown); FieldEnumOptions.Add(F.Key, Options);
			Row->AddChildToHorizontalBox(B)->SetVerticalAlignment(VAlign_Center);
			break;
		}
		case ItemFields::EType::Bool:
		{
			UButton* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
			B->SetStyle(Crt::ButtonStyle());
			const bool bOn = Value == TEXT("true");
			UTextBlock* L = Crt::FixedText(WidgetTree, FixedLabel(bOn ? TEXT("X  YES") : TEXT("   NO"), 7), S.CaptionSize, bOn ? Crt::Green : Crt::DimGreen);
			B->AddChild(L);
			if (UButtonSlot* BS = Cast<UButtonSlot>(L->Slot)) { BS->SetPadding(FMargin(8.0f, 2.0f)); }
			UReferenceFieldBinding* Binding = NewObject<UReferenceFieldBinding>(this);
			Binding->Widget = this; Binding->Key = F.Key;
			B->OnClicked.AddDynamic(Binding, &UReferenceFieldBinding::OnToggle);
			FieldBindings.Add(Binding); FieldBoolLabels.Add(F.Key, L); FieldBoolValues.Add(F.Key, bOn);
			Row->AddChildToHorizontalBox(B)->SetVerticalAlignment(VAlign_Center);
			break;
		}
		case ItemFields::EType::Measured:
			Row->AddChildToHorizontalBox(Crt::FixedText(WidgetTree, Value.IsEmpty() ? TEXT("-") : Value, S.CaptionSize, Crt::Green))->SetVerticalAlignment(VAlign_Center);
			break;
		default:
		{
			UEditableTextBox* Box = StyledBox(WidgetTree, S.CaptionSize);
			Box->SetText(FText::FromString(Value));
			FieldBoxes.Add(F.Key, Box);
			UHorizontalBoxSlot* BS = Row->AddChildToHorizontalBox(Box); BS->SetSize(ESlateSizeRule::Fill); BS->SetVerticalAlignment(VAlign_Center);
			break;
		}
		}
		if (F.Hint && *F.Hint) { Row->SetToolTipText(FText::FromString(F.Hint)); }
		Rows->AddChildToVerticalBox(Row)->SetPadding(FMargin(0, 1));
		}
	}
}

void UReferenceWidget::ToggleGroup(const FString& Group)
{
	if (ExpandedGroups.Contains(Group)) { ExpandedGroups.Remove(Group); } else { ExpandedGroups.Add(Group); }
	const bool bOpen = ExpandedGroups.Contains(Group);
	if (TObjectPtr<UVerticalBox>* Rows = GroupBoxes.Find(Group)) { if (*Rows) { (*Rows)->SetVisibility(bOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed); } }
	if (TObjectPtr<UTextBlock>* L = GroupLabels.Find(Group)) { if (*L) { (*L)->SetText(FText::FromString(GroupHeader(*Group, bOpen))); } }
}

void UReferenceWidget::HideEntry(int32 Index)
{
	if (!Entries.IsValidIndex(Index)) { return; }
	// Hidden and written, and if it was the one open on the right, the column goes with it.
	const bool bWasOpen = SelectedIndex == Index;
	const int32 Keep = SelectedIndex;
	SelectedIndex = Index;
	if (Entries.IsValidIndex(SelectedIndex)) { Entries[SelectedIndex].Fields.Add(TEXT("hidden"), TEXT("true")); }
	if (FieldBoolValues.Contains(TEXT("hidden")) && bWasOpen) { FieldBoolValues[TEXT("hidden")] = true; }
	OnSaveDetail();
	SelectedIndex = bWasOpen ? -1 : Keep;
	if (bWasOpen) { OnCloseDetail(); }
	FillList();
	if (DetailNote) { DetailNote->SetText(FText::FromString(FString::Printf(TEXT("HIDDEN: %s"), *Entries[Index].Name))); }
}

int32 UReferenceWidget::AxisHeld() const
{
	if (HeldAxis >= 0) { return HeldAxis; }
	if (const APlayerController* PC = GetOwningPlayer())
	{
		if (PC->IsInputKeyDown(EKeys::X)) { return 0; }
		if (PC->IsInputKeyDown(EKeys::Y)) { return 1; }
		if (PC->IsInputKeyDown(EKeys::Z)) { return 2; }
	}
	return -1;
}

static int32 AxisOfKey(const FKey& K) { return K == EKeys::X ? 0 : K == EKeys::Y ? 1 : K == EKeys::Z ? 2 : -1; }

FReply UReferenceWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const int32 A = AxisOfKey(InKeyEvent.GetKey());
	if (A >= 0 && bShowMeta)
	{
		HeldAxis = A;
		// Eaten only mid-drag, so an x typed into a field still lands in the field.
		if (bDragging && DragMarker >= 0) { return FReply::Handled(); }
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

FReply UReferenceWidget::NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const int32 A = AxisOfKey(InKeyEvent.GetKey());
	if (A >= 0 && HeldAxis == A) { HeldAxis = -1; }
	return Super::NativeOnKeyUp(InGeometry, InKeyEvent);
}

void UReferenceWidget::NativeOnFocusLost(const FFocusEvent& InFocusEvent)
{
	HeldAxis = -1;   // the key-up will go elsewhere now
	Super::NativeOnFocusLost(InFocusEvent);
}

bool UReferenceWidget::UnprojectToAxis(const FVector2D& FeedPx, const FVector& LineStartLocal, int32 Axis, FVector& OutLocal) const
{
	FTransform Camera, Piece; float Fov = 34.0f;
	if (!WeaponFeed || !OwnerController || !OwnerController->GetWeaponPreviewFrame(Camera, Fov, Piece)) { return false; }
	const FVector2D Sz = WeaponFeed->GetCachedGeometry().GetLocalSize();
	if (Sz.X <= 1.0 || Sz.Y <= 1.0) { return false; }
	const double TanH = FMath::Tan(FMath::DegreesToRadians(FMath::Max(1.0f, Fov) * 0.5f));
	const FVector DirCam(1.0, (FeedPx.X / Sz.X - 0.5) * 2.0 * TanH, -(FeedPx.Y / Sz.Y - 0.5) * 2.0 * TanH / (Sz.X / Sz.Y));
	const FVector D = Camera.TransformVectorNoScale(DirCam).GetSafeNormal();
	const FVector O = Camera.GetLocation();
	// The axis line in the world, through where the point was picked up.
	FVector Unit = FVector::ZeroVector; Unit[FMath::Clamp(Axis, 0, 2)] = 1.0;
	const FVector A = Piece.TransformVectorNoScale(Unit).GetSafeNormal();
	const FVector P = Piece.TransformPosition(LineStartLocal);
	// The point of the line nearest the pointer's ray: the two-lines closest-points solve with
	// both directions unit length.
	const FVector W0 = O - P;
	const double B = FVector::DotProduct(D, A), Dd = FVector::DotProduct(D, W0), E = FVector::DotProduct(A, W0);
	const double Denom = 1.0 - B * B;
	if (Denom < 1e-4) { return false; }   // looking straight down the axis: nowhere to drag it to
	const double T = (E - B * Dd) / Denom;
	const double Scale = FMath::Max(Piece.GetScale3D()[FMath::Clamp(Axis, 0, 2)], 1e-3);
	OutLocal = LineStartLocal + Unit * (T / Scale);
	return true;
}

void UReferenceWidget::ArmMarker(const FString& Key)
{
	ArmedKey = (ArmedKey == Key) ? FString() : Key;
	FillMetaTable(Entries.IsValidIndex(SelectedIndex) ? WeaponCatalog::Find(Entries[SelectedIndex].Name) : nullptr);
	if (DetailNote) { DetailNote->SetText(FText::FromString(ArmedKey.IsEmpty() ? TEXT("") : FString::Printf(TEXT("DRAG MOVES: %s"), *ArmedKey.ToUpper().Replace(TEXT("_"), TEXT(" "))))); }
}

void UReferenceWidget::OnResetPoints()
{
	// Every dragged or typed point goes back to the file's value (the catalogue re-reads the
	// file when it changes, so this is the state at the last SAVE).
	if (!Entries.IsValidIndex(SelectedIndex)) { return; }
	FReferenceEntry& E = Entries[SelectedIndex];
	if (E.Category == TEXT("optics"))
	{
		// Optics are not ItemCatalog records; the weapon catalogue's read of the file has their points.
		const WeaponCatalog::FOptic* O = WeaponCatalog::FindOptic(E.Key);
		const FString MountText = O ? FString::Printf(TEXT("%.2f, %.2f, %.2f"), O->Mount.X, O->Mount.Y, O->Mount.Z) : FString();
		const FString EyeText = O ? FString::Printf(TEXT("%.2f, %.2f, %.2f"), O->Eye.X, O->Eye.Y, O->Eye.Z) : FString();
		E.Fields.Add(TEXT("mount"), MountText); E.Fields.Add(TEXT("eye"), EyeText);
		if (TObjectPtr<UEditableTextBox>* Box = FieldBoxes.Find(TEXT("mount"))) { if (*Box) { (*Box)->SetText(FText::FromString(MountText)); } }
		if (TObjectPtr<UEditableTextBox>* Box = FieldBoxes.Find(TEXT("eye"))) { if (*Box) { (*Box)->SetText(FText::FromString(EyeText)); } }
		if (DetailNote) { DetailNote->SetText(FText::FromString(TEXT("POINTS RESET TO THE FILE"))); }
		return;
	}
	const ItemCatalog::FRecord* R = ItemCatalog::FindRecordByKey(E.Key);
	if (!R) { return; }
	static const TCHAR* Keys[] = { TEXT("grip"), TEXT("sight"), TEXT("fore_grip"), TEXT("muzzle"), TEXT("optic_mount"), TEXT("shoulder"), TEXT("attachments") };
	for (const TCHAR* K : Keys)
	{
		const FString V = R->Fields.FindRef(K);
		E.Fields.Add(K, V);
		if (TObjectPtr<UEditableTextBox>* Box = FieldBoxes.Find(K)) { if (*Box) { (*Box)->SetText(FText::FromString(V)); } }
	}
	if (DetailNote) { DetailNote->SetText(FText::FromString(TEXT("POINTS RESET TO THE FILE"))); }
}

void UReferenceWidget::OnRevert()
{
	if (!Entries.IsValidIndex(SelectedIndex)) { return; }
	const FString Key = Entries[SelectedIndex].Key;
	LoadCatalogue();
	for (int32 i = 0; i < Entries.Num(); ++i) { if (Entries[i].Key == Key) { SelectEntry(i); break; } }
	if (DetailNote) { DetailNote->SetText(FText::FromString(TEXT("REVERTED TO THE FILE"))); }
}

void UReferenceWidget::OnToggleHidden()
{
	if (!Entries.IsValidIndex(SelectedIndex)) { return; }
	FReferenceEntry& E = Entries[SelectedIndex];
	const bool bHid = E.Fields.FindRef(TEXT("hidden")) != TEXT("true");
	E.Fields.Add(TEXT("hidden"), bHid ? TEXT("true") : TEXT("false"));
	if (FieldBoolValues.Contains(TEXT("hidden"))) { FieldBoolValues[TEXT("hidden")] = bHid; ToggleBoolField(TEXT("hidden")); ToggleBoolField(TEXT("hidden")); }
	OnSaveDetail();
}

void UReferenceWidget::ReadFieldsFromForm(FReferenceEntry& E) const
{
	for (const TPair<FString, TObjectPtr<UEditableTextBox>>& Pair : FieldBoxes) { if (Pair.Value) { E.Fields.Add(Pair.Key, Pair.Value->GetText().ToString().TrimStartAndEnd()); } }
	for (const TPair<FString, bool>& Pair : FieldBoolValues) { E.Fields.Add(Pair.Key, Pair.Value ? TEXT("true") : TEXT("false")); }
	for (const TPair<FString, FString>& Pair : FieldEnumValues) { E.Fields.Add(Pair.Key, Pair.Value); }
}

// The render is on screen only while the detail pane is: the feed image keeps its own Visible
// flag inside a collapsed pane, and its cached rectangle stays where it last was, so painting
// on the image's flag alone left a ghost of the frame over the list.
bool UReferenceWidget::FeedShown() const
{
	return WeaponFeed && WeaponFeed->GetVisibility() == ESlateVisibility::Visible && DetailBox && DetailBox->GetVisibility() != ESlateVisibility::Collapsed;
}

FString UReferenceWidget::TitleOf(const FReferenceEntry& E)
{
	// THE MODEL ALONE. The maker used to be prepended, which made the longest titles on the page
	// ("Vestergaard Precision  Scope Sniper 03") long enough to run under the review date and the
	// asset path sharing that line. The maker is on the page anyway, in its own MAKE field a few
	// rows down, so the header was saying it twice and paying for it in width.
	const FString Model = E.Fields.FindRef(TEXT("model"));
	return Model.IsEmpty() ? E.Name : Model;
}

bool UReferenceWidget::OpenEntryByName(const FString& Name)
{
	for (int32 i = 0; i < Entries.Num(); ++i)
	{
		if (Entries[i].Name != Name) { continue; }
		SetCategory(Entries[i].Category);
		SelectEntry(i);
		return true;
	}
	return false;
}

void UReferenceWidget::OnCloseDetail()
{
	SelectedIndex = -1;
	if (DetailBox) { DetailBox->SetVisibility(ESlateVisibility::Collapsed); }
	if (WeaponFeed) { WeaponFeed->SetVisibility(ESlateVisibility::Hidden); }
	if (OwnerController) { OwnerController->HideWeaponPreview(); }
	FillList();
}

void UReferenceWidget::OnSearchChanged(const FText& Text)
{
	Filter = Text.ToString().TrimStartAndEnd();
	FillList();
}

void UReferenceWidget::OnCloseClicked() { OnClose.ExecuteIfBound(); }
void UReferenceWidget::OnTab(int32 Tab) { if (OwnerController) { OwnerController->ShowConsolePage(Tab); } }

FReply UReferenceWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && FeedShown() && WeaponFeed->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
	{
		// On a dot: pick it up. The point moves in the plane through it that faces the camera,
		// so a drag in the side view is a move in the weapon's X and Z, and the POINTS field
		// follows; SAVE writes it. The grip origin and the optic's eye are not points to move.
		DragMarker = -1;
		if (bShowMeta)
		{
			const FVector2D Local = WeaponFeed->GetCachedGeometry().AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
			const TArray<FMarkerHit> Marks = CurrentMarkers();
			double Best = 12.0;
			for (int32 i = 0; i < Marks.Num(); ++i)
			{
				FVector2D P;
				if (Marks[i].Key.IsEmpty() || !ProjectLocal(Marks[i].Local, P)) { continue; }
				const double D = FVector2D::Distance(P, Local);
				// The armed point takes the drag from anywhere within a generous reach; without
				// one, the single nearest dot within twelve pixels does.
				if (!ArmedKey.IsEmpty() && Marks[i].Key == ArmedKey && D < 60.0) { DragMarker = i; break; }
				if (ArmedKey.IsEmpty() && D < Best) { Best = D; DragMarker = i; }
			}
		}
		bDragging = true; DragLast = InMouseEvent.GetScreenSpacePosition(); DragTravel = 0.0f;
		DragAxis = -1;
		if (DragMarker >= 0) { DragStartLocal = CurrentMarkers()[DragMarker].Local; DragAxis = AxisHeld(); }
		// Focus comes with the drag, so a held X, Y or Z reaches this widget rather than a field.
		return FReply::Handled().CaptureMouse(TakeWidget()).SetUserFocus(TakeWidget(), EFocusCause::Mouse);
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UReferenceWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDragging && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bDragging = false;
		const bool bWasDot = DragMarker >= 0;
		DragMarker = -1;
		// A click rather than a drag: step between the fitted view and a closer one.
		if (!bWasDot && DragTravel < 6.0f && OwnerController) { OwnerController->ToggleWeaponPreviewZoom(); }
		if (bWasDot && DetailNote) { DetailNote->SetText(FText::FromString(TEXT("POINT MOVED -- SAVE to keep it"))); }
		DragAxis = -1;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UReferenceWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDragging)
	{
		const FVector2D Now = InMouseEvent.GetScreenSpacePosition();
		const FVector2D Delta = Now - DragLast; DragLast = Now; DragTravel += Delta.Size();
		if (DragMarker >= 0)
		{
			const TArray<FMarkerHit> Marks = CurrentMarkers();
			if (Marks.IsValidIndex(DragMarker) && WeaponFeed)
			{
				FTransform Camera, Piece; float Fov = 34.0f;
				if (OwnerController && OwnerController->GetWeaponPreviewFrame(Camera, Fov, Piece))
				{
					const FVector2D Local = WeaponFeed->GetCachedGeometry().AbsoluteToLocal(Now);
					FVector NewLocal;
					// The pin applies from where the drag BEGAN: press Z mid-drag and the point jumps back
					// to the start plus only the vertical part of the movement so far; release it and the
					// free drag resumes from the same start. Nothing re-anchors mid-drag.
					DragAxis = AxisHeld();
					const bool bOk = DragAxis >= 0 ? UnprojectToAxis(Local, DragStartLocal, DragAxis, NewLocal)
					                               : UnprojectToPlane(Local, Piece.TransformPosition(DragStartLocal), NewLocal);
					if (bOk) { SetPointField(Marks[DragMarker].Key, NewLocal); }
				}
			}
			return FReply::Handled();
		}
		// Same sense as the character sheet's portrait: drag left and the near face swings toward you.
		if (OwnerController) { OwnerController->OrbitWeaponPreview(-Delta.X * 0.5f, Delta.Y * 0.4f); }
		return FReply::Handled();
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

void UReferenceWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// THE PATH, SLIDING, only when it does not fit. It holds at the start for a moment so it can be
	// read, slides until its end is in view, holds again, then jumps back -- which is easier to read
	// than a continuous loop, where the one thing you want (the file name at the end) is never still.
	if (DetailMesh && MeshMarquee)
	{
		const float Width = MeshMarquee->GetCachedGeometry().GetLocalSize().X;
		const float Text = DetailMesh->GetDesiredSize().X;
		const float Over = Text - Width;
		if (Over > 1.0f && Width > 1.0f)
		{
			// Three states, no flags: holding (wait counts down), sliding, or parked at the far end
			// waiting to snap back. Whichever it is, MeshScroll is the offset to draw at.
			if (MeshScrollWait > 0.0f)
			{
				MeshScrollWait -= InDeltaTime;
			}
			else if (MeshScroll >= Over)
			{
				MeshScroll = 0.0f;                          // the hold at the end is over: back to the start
				MeshScrollWait = MeshScrollHoldSeconds;
			}
			else
			{
				MeshScroll = FMath::Min(Over, MeshScroll + InDeltaTime * MeshScrollSpeed);
				if (MeshScroll >= Over) { MeshScrollWait = MeshScrollHoldSeconds; }   // hold with the end in view
			}
			DetailMesh->SetRenderTranslation(FVector2D(-MeshScroll, 0.0f));
		}
		else if (MeshScroll != 0.0f)
		{
			MeshScroll = 0.0f;
			DetailMesh->SetRenderTranslation(FVector2D::ZeroVector);
		}
	}
	Clock += InDeltaTime;
	if (SettleTicks > 0) { if (--SettleTicks == 0) { SetRenderOpacity(1.0f); if (OwnerController) { OwnerController->PageSettled(this); } } }
	// The booth is driven from here: the game is paused under the screen, Slate is not.
	if (OwnerController && SelectedIndex >= 0) { OwnerController->TickWeaponPreview(InDeltaTime); RefreshFeed(); }
}

int32 UReferenceWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	// The render's own rectangle, in this panel's space, so the raster is dialled down over it.
	FBox2f Soft(ForceInit);
	bool bHaveSoft = false;
	if (FeedShown())
	{
		const FGeometry& G = WeaponFeed->GetCachedGeometry();
		if (G.GetLocalSize().X > 1.0f)
		{
			const FVector2f TopLeft(AllottedGeometry.AbsoluteToLocal(G.LocalToAbsolute(FVector2f::ZeroVector)));
			const FVector2f BottomRight(AllottedGeometry.AbsoluteToLocal(G.LocalToAbsolute(FVector2f(G.GetLocalSize()))));
			Soft = FBox2f(TopLeft, BottomRight);
			bHaveSoft = true;
		}
	}
	Crt::PaintFrame(OutDrawElements, AllottedGeometry, FVector2f::ZeroVector, FVector2f(AllottedGeometry.GetLocalSize()), LayerId + 1, Clock, 1.0f, bHaveSoft ? &Soft : nullptr);
	// The render keeps the panel's scanlines here: repainting it above the frame, the way the
	// character sheet does its portrait, would bury the controls sitting on top of it.
	if (FeedShown())
	{
		const FGeometry& FeedGeo = WeaponFeed->GetCachedGeometry();
		const FVector2f Sz(FeedGeo.GetLocalSize());
		if (Sz.X > 1.0f)
		{
			const bool bClip = DetailScroll && DetailScroll->GetCachedGeometry().GetLocalSize().X > 1.0f;
			if (bClip) { OutDrawElements.PushClip(FSlateClippingZone(DetailScroll->GetCachedGeometry())); }
			const TArray<FVector2f> Rim = { {0.0f, 0.0f}, {Sz.X, 0.0f}, Sz, {0.0f, Sz.Y}, {0.0f, 0.0f} };
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 2, FeedGeo.ToPaintGeometry(), Rim, ESlateDrawEffect::None, Crt::Faint, true, 1.0f);
			if (bClip) { OutDrawElements.PopClip(); }
		}
	}
	// Everything drawn over the render is clipped to the scroller, as the render itself is:
	// scrolled off, it goes with it instead of floating over the header.
	if (DetailScroll && DetailScroll->GetCachedGeometry().GetLocalSize().X > 1.0f) { OutDrawElements.PushClip(FSlateClippingZone(DetailScroll->GetCachedGeometry())); }
	PaintMarkers(OutDrawElements, LayerId + 3);
	if (DetailScroll && DetailScroll->GetCachedGeometry().GetLocalSize().X > 1.0f) { OutDrawElements.PopClip(); }
	return LayerId + 4;
}

// The catalogue's points for a weapon, in the order they are listed and drawn: the grip origin,
// the fore grip, the sight, the optic mount, the muzzle. One list feeds the dots and the table.
struct FRefMarker { FString Name; FVector Local; FLinearColor Colour; FString Note; };
static TArray<FRefMarker> MarkersFor(const WeaponCatalog::FWeapon& W)
{
	TArray<FRefMarker> M;
	M.Add({ TEXT("GRIP (origin)"), FVector::ZeroVector, FLinearColor(1.0f, 0.3f, 0.3f), TEXT("HAC1 origin, +X downrange") });
	if (W.bHasForeGrip) { M.Add({ TEXT("FORE GRIP"), W.ForeGrip, FLinearColor(0.35f, 1.0f, 0.4f), FString::Printf(TEXT("palm pitch %+.0f deg"), W.ForeGripPitch) }); }
	// What the eye is put behind: the optic's eye point when one is fitted (the weapon's own
	// sight is then unused and not shown), else the weapon's sight, else the bore.
	const WeaponCatalog::FOptic* Optic = W.Optic.IsEmpty() ? nullptr : WeaponCatalog::FindOptic(W.Optic);
	if (Optic && !Optic->Eye.IsNearlyZero())
	{
		M.Add({ TEXT("OPTIC MOUNT"), W.OpticMount, FLinearColor(0.3f, 1.0f, 1.0f), W.Optic });
		M.Add({ TEXT("AIM POINT (optic window)"), W.OpticMount + Optic->Eye, FLinearColor(1.0f, 0.9f, 0.2f), TEXT("the eye sits behind this") });
	}
	else
	{
		M.Add({ TEXT("SIGHT"), W.bHasSight ? W.Sight : FVector::ZeroVector, W.bHasSight ? FLinearColor(1.0f, 0.9f, 0.2f) : FLinearColor(0.6f, 0.55f, 0.15f),
		        W.bHasSight ? FString::Printf(TEXT("sight pitch %+.1f deg"), W.SightPitch) : FString(TEXT("none: aims down the bore")) });
		if (!W.Optic.IsEmpty()) { M.Add({ TEXT("OPTIC MOUNT"), W.OpticMount, FLinearColor(0.3f, 1.0f, 1.0f), W.Optic }); }
	}
	if (!W.Muzzle.IsNearlyZero()) { M.Add({ TEXT("MUZZLE"), W.Muzzle, FLinearColor::White, TEXT("shot origin") }); }
	return M;
}

void UReferenceWidget::FillMetaTable(const WeaponCatalog::FWeapon* W)
{
	if (!MetaLegend) { return; }
	MetaLegend->ClearChildren();
	const bool bOpticEntry = Entries.IsValidIndex(SelectedIndex) && Entries[SelectedIndex].Category == TEXT("optics");
	if (!W && !bOpticEntry) { MetaLegend->SetVisibility(ESlateVisibility::Collapsed); return; }
	MetaLegend->SetVisibility(bShowMeta ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	const FSheetSpec& S = FSheetSpec::Get();
	LegendBindings.Reset();
	// With a point armed: how to move it precisely. A plain drag rides the camera-facing plane;
	// a held X, Y or Z pins it to that weapon axis.
	if (!ArmedKey.IsEmpty())
	{
		MetaLegend->AddChildToVerticalBox(Crt::FixedText(WidgetTree, TEXT("DRAG THE POINT -- HOLD X, Y OR Z TO MOVE ALONG ONE AXIS"), S.RowSize, Crt::DimGreen))->SetPadding(FMargin(0, 6, 0, 2));
	}
	for (const FMarkerHit& M : CurrentMarkers())
	{
		// A row is a button: click it and a drag in the viewer moves THAT point. A point with
		// no key (the grip origin, an optic's eye) is derived and cannot be moved; it is drawn
		// hollow so the eye does not try to grab it.
		const bool bMovable = !M.Key.IsEmpty();
		const bool bArmed = bMovable && ArmedKey == M.Key;
		UHorizontalBox* R = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		USizeBox* Dot = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Dot->SetWidthOverride(9.0f); Dot->SetHeightOverride(9.0f);
		UBorder* B = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		B->SetBrushColor(M.Colour);
		if (!bMovable) { Dot->SetWidthOverride(5.0f); Dot->SetHeightOverride(5.0f); }   // a derived point: the small swatch
		Dot->AddChild(B);
		UHorizontalBoxSlot* DS = R->AddChildToHorizontalBox(Dot); DS->SetVerticalAlignment(VAlign_Center); DS->SetPadding(FMargin(0, 0, 7, 0));
		const FString Caption = bArmed ? FString::Printf(TEXT("> %s <"), *M.Name) : (bMovable ? TEXT("  ") + M.Name : TEXT("  ") + M.Name + TEXT(" (follows)"));
		R->AddChildToHorizontalBox(Crt::FixedText(WidgetTree, Caption, S.RowSize, bArmed ? Crt::Green : M.Colour))->SetVerticalAlignment(VAlign_Center);
		if (bMovable)
		{
			UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
			Btn->SetStyle(Crt::ButtonStyle());
			Btn->AddChild(R);
			if (UButtonSlot* BS = Cast<UButtonSlot>(R->Slot)) { BS->SetPadding(FMargin(2.0f, 0.0f)); BS->SetHorizontalAlignment(HAlign_Left); }
			UReferenceFieldBinding* LB = NewObject<UReferenceFieldBinding>(this);
			LB->Widget = this; LB->Key = TEXT("arm:") + M.Key;
			Btn->OnClicked.AddDynamic(LB, &UReferenceFieldBinding::OnToggle);
			LegendBindings.Add(LB);
			MetaLegend->AddChildToVerticalBox(Btn)->SetPadding(FMargin(0, 1));
		}
		else { MetaLegend->AddChildToVerticalBox(R)->SetPadding(FMargin(2, 1)); }
	}
	if (W)
	{
		// Accessory mounts come and go: one button adds a point under the fore-end, the other
		// removes the armed one.
		UHorizontalBox* AR = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		auto AddBtn = [&](const TCHAR* Caption, const TCHAR* Key)
		{
			UButton* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
			B->SetStyle(Crt::ButtonStyle());
			UTextBlock* T = Crt::FixedText(WidgetTree, Caption, S.RowSize, Crt::Green);
			B->AddChild(T);
			if (UButtonSlot* BS = Cast<UButtonSlot>(T->Slot)) { BS->SetPadding(FMargin(4.0f, 1.0f)); }
			UReferenceFieldBinding* NB = NewObject<UReferenceFieldBinding>(this);
			NB->Widget = this; NB->Key = Key;
			B->OnClicked.AddDynamic(NB, &UReferenceFieldBinding::OnToggle);
			LegendBindings.Add(NB);
			AR->AddChildToHorizontalBox(B)->SetPadding(FMargin(0, 0, 6, 0));
		};
		AddBtn(TEXT("[ + ATTACH ]"), TEXT("attach:add"));
		if (ArmedKey.StartsWith(TEXT("attach:"))) { AddBtn(TEXT("[ - ATTACH ]"), TEXT("attach:del")); }
		MetaLegend->AddChildToVerticalBox(AR)->SetPadding(FMargin(0, 6, 0, 2));
	}
}

void UReferenceWidget::OnToggleMeta()
{
	bShowMeta = !bShowMeta;
	if (MetaToggleLabel) { MetaToggleLabel->SetText(FText::FromString(FixedLabel(bShowMeta ? TEXT("META: ON") : TEXT("META: OFF"), 9))); }
	if (MetaLegend) { MetaLegend->SetVisibility(bShowMeta && MetaLegend->GetChildrenCount() > 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed); }
}

// The points over the render: each projected through the booth's own camera onto the feed and
// drawn as a filled dot in its colour, with the HAC1 frame at the grip. The names and numbers
// are in the table under the viewer, so the render stays clean.
// The rule that heads a group of fields: == when it is folded, -- when it is open.
static FString GroupHeader(const TCHAR* Name, bool bOpen)
{
	return bOpen ? FString::Printf(TEXT("--[ %s ]-------------"), Name) : FString::Printf(TEXT("==[ %s ]============="), Name);
}

static bool ParseVector(const FString& Text, FVector& Out)
{
	TArray<FString> Parts; Text.ParseIntoArray(Parts, TEXT(","), true);
	if (Parts.Num() < 3) { return false; }
	Out = FVector(FCString::Atod(*Parts[0].TrimStartAndEnd()), FCString::Atod(*Parts[1].TrimStartAndEnd()), FCString::Atod(*Parts[2].TrimStartAndEnd()));
	return true;
}

// "x, y, z; x, y, z" <-> points: the ATTACHMENTS field's form.
static void ParseVectors(const FString& Text, TArray<FVector>& Out)
{
	Out.Reset();
	TArray<FString> Parts; Text.ParseIntoArray(Parts, TEXT(";"), true);
	for (const FString& P : Parts) { FVector V; if (ParseVector(P, V)) { Out.Add(V); } }
}
static FString VectorsText(const TArray<FVector>& In)
{
	TArray<FString> Parts;
	for (const FVector& V : In) { Parts.Add(FString::Printf(TEXT("%.2f, %.2f, %.2f"), V.X, V.Y, V.Z)); }
	return FString::Join(Parts, TEXT("; "));
}

// The dots as the page currently has them: the entry's POINTS fields (which a drag edits)
// over the catalogue's numbers, so what is drawn is what SAVE will write.
TArray<UReferenceWidget::FMarkerHit> UReferenceWidget::CurrentMarkers() const
{
	TArray<FMarkerHit> Out;
	if (!Entries.IsValidIndex(SelectedIndex)) { return Out; }
	const FReferenceEntry& E = Entries[SelectedIndex];
	if (E.Category == TEXT("optics"))
	{
		// An optic alone: where it sits on a rail, and where the eye looks through it.
		const WeaponCatalog::FOptic* O = WeaponCatalog::FindOptic(E.Key);
		FVector V;
		Out.Add({ TEXT("mount"), ParseVector(E.Fields.FindRef(TEXT("mount")), V) ? V : (O ? O->Mount : FVector::ZeroVector), FLinearColor(0.3f, 1.0f, 1.0f), TEXT("MOUNT (on the rail)") });
		Out.Add({ TEXT("eye"), ParseVector(E.Fields.FindRef(TEXT("eye")), V) ? V : (O ? O->Eye : FVector::ZeroVector), FLinearColor(1.0f, 0.9f, 0.2f), TEXT("EYE (window centre)") });
		return Out;
	}
	const WeaponCatalog::FWeapon* W = WeaponCatalog::Find(E.Name);
	if (!W) { return Out; }
	auto Point = [&](const TCHAR* Key, const FVector& Fallback) { FVector V; return ParseVector(E.Fields.FindRef(Key), V) ? V : Fallback; };
	Out.Add({ TEXT("grip"), Point(TEXT("grip"), W->Grip), FLinearColor(1.0f, 0.3f, 0.3f), TEXT("GRIP") });
	if (W->bHasForeGrip) { Out.Add({ TEXT("fore_grip"), Point(TEXT("fore_grip"), W->ForeGrip), FLinearColor(0.35f, 1.0f, 0.4f), TEXT("FORE GRIP") }); }
	const WeaponCatalog::FOptic* Optic = W->Optic.IsEmpty() ? nullptr : WeaponCatalog::FindOptic(W->Optic);
	if (Optic && !Optic->Eye.IsNearlyZero())
	{
		const FVector Mount = Point(TEXT("optic_mount"), W->OpticMount);
		Out.Add({ TEXT("optic_mount"), Mount, FLinearColor(0.3f, 1.0f, 1.0f), TEXT("OPTIC MOUNT") });
		Out.Add({ TEXT(""), Mount + Optic->Eye - Optic->Mount, FLinearColor(1.0f, 0.9f, 0.2f), TEXT("AIM POINT (optic window)") });
	}
	else
	{
		Out.Add({ TEXT("sight"), Point(TEXT("sight"), W->bHasSight ? W->Sight : FVector::ZeroVector), W->bHasSight ? FLinearColor(1.0f, 0.9f, 0.2f) : FLinearColor(0.6f, 0.55f, 0.15f), TEXT("SIGHT") });
		if (!W->Optic.IsEmpty()) { Out.Add({ TEXT("optic_mount"), Point(TEXT("optic_mount"), W->OpticMount), FLinearColor(0.3f, 1.0f, 1.0f), TEXT("OPTIC MOUNT") }); }
	}
	const FVector Muzzle = Point(TEXT("muzzle"), W->Muzzle);
	if (!Muzzle.IsNearlyZero()) { Out.Add({ TEXT("muzzle"), Muzzle, FLinearColor::White, TEXT("MUZZLE") }); }
	const FVector Shoulder = Point(TEXT("shoulder"), W->Shoulder);
	if (!Shoulder.IsNearlyZero()) { Out.Add({ TEXT("shoulder"), Shoulder, FLinearColor(1.0f, 0.4f, 1.0f), TEXT("SHOULDER") }); }
	// Accessory mounts, as many as the weapon has: the field's text once edited, the file's list until then.
	TArray<FVector> Att;
	if (E.Fields.Contains(TEXT("attachments"))) { ParseVectors(E.Fields.FindRef(TEXT("attachments")), Att); } else { Att = W->Attachments; }
	for (int32 i = 0; i < Att.Num(); ++i) { Out.Add({ FString::Printf(TEXT("attach:%d"), i), Att[i], FLinearColor(1.0f, 0.55f, 0.15f), FString::Printf(TEXT("ATTACH %d"), i + 1) }); }
	return Out;
}

bool UReferenceWidget::ProjectLocal(const FVector& Local, FVector2D& OutFeedPx) const
{
	FTransform Camera, Piece; float Fov = 34.0f;
	if (!WeaponFeed || !OwnerController || !OwnerController->GetWeaponPreviewFrame(Camera, Fov, Piece)) { return false; }
	const FVector2D Sz = WeaponFeed->GetCachedGeometry().GetLocalSize();
	if (Sz.X <= 1.0 || Sz.Y <= 1.0) { return false; }
	const double TanH = FMath::Tan(FMath::DegreesToRadians(FMath::Max(1.0f, Fov) * 0.5f));
	const FVector V = Camera.InverseTransformPosition(Piece.TransformPosition(Local));
	if (V.X < 1.0) { return false; }
	OutFeedPx.X = Sz.X * (0.5 + (V.Y / V.X) / (2.0 * TanH));
	OutFeedPx.Y = Sz.Y * (0.5 - (V.Z / V.X) * (Sz.X / Sz.Y) / (2.0 * TanH));
	return true;
}

bool UReferenceWidget::UnprojectToPlane(const FVector2D& FeedPx, const FVector& PlanePointWorld, FVector& OutLocal) const
{
	FTransform Camera, Piece; float Fov = 34.0f;
	if (!WeaponFeed || !OwnerController || !OwnerController->GetWeaponPreviewFrame(Camera, Fov, Piece)) { return false; }
	const FVector2D Sz = WeaponFeed->GetCachedGeometry().GetLocalSize();
	if (Sz.X <= 1.0 || Sz.Y <= 1.0) { return false; }
	const double TanH = FMath::Tan(FMath::DegreesToRadians(FMath::Max(1.0f, Fov) * 0.5f));
	// The ray through that pixel, in the camera's frame, then the world's.
	const FVector DirCam(1.0, (FeedPx.X / Sz.X - 0.5) * 2.0 * TanH, -(FeedPx.Y / Sz.Y - 0.5) * 2.0 * TanH / (Sz.X / Sz.Y));
	const FVector Dir = Camera.TransformVectorNoScale(DirCam).GetSafeNormal();
	const FVector Origin = Camera.GetLocation();
	const FVector Normal = Camera.GetUnitAxis(EAxis::X);
	const double Denom = FVector::DotProduct(Dir, Normal);
	if (FMath::Abs(Denom) < 1e-6) { return false; }
	const double T = FVector::DotProduct(PlanePointWorld - Origin, Normal) / Denom;
	if (T <= 0.0) { return false; }
	OutLocal = Piece.InverseTransformPosition(Origin + Dir * T);
	return true;
}

void UReferenceWidget::SetPointField(const FString& Key, const FVector& Local)
{
	if (!Entries.IsValidIndex(SelectedIndex) || Key.IsEmpty()) { return; }
	if (Key.StartsWith(TEXT("attach:")))
	{
		FReferenceEntry& E = Entries[SelectedIndex];
		TArray<FVector> Att;
		if (E.Fields.Contains(TEXT("attachments"))) { ParseVectors(E.Fields.FindRef(TEXT("attachments")), Att); }
		else if (const WeaponCatalog::FWeapon* W = WeaponCatalog::Find(E.Name)) { Att = W->Attachments; }
		const int32 N = FCString::Atoi(*Key.Mid(7));
		if (!Att.IsValidIndex(N)) { return; }
		Att[N] = Local;
		const FString Text = VectorsText(Att);
		E.Fields.Add(TEXT("attachments"), Text);
		if (TObjectPtr<UEditableTextBox>* Box = FieldBoxes.Find(TEXT("attachments"))) { if (*Box) { (*Box)->SetText(FText::FromString(Text)); } }
		return;
	}
	const FString Text = FString::Printf(TEXT("%.2f, %.2f, %.2f"), Local.X, Local.Y, Local.Z);
	Entries[SelectedIndex].Fields.Add(Key, Text);
	if (TObjectPtr<UEditableTextBox>* Box = FieldBoxes.Find(Key)) { if (*Box) { (*Box)->SetText(FText::FromString(Text)); } }
}

void UReferenceWidget::AddAttachment()
{
	if (!Entries.IsValidIndex(SelectedIndex)) { return; }
	FReferenceEntry& E = Entries[SelectedIndex];
	const WeaponCatalog::FWeapon* W = WeaponCatalog::Find(E.Name);
	if (!W) { return; }
	TArray<FVector> Att;
	if (E.Fields.Contains(TEXT("attachments"))) { ParseVectors(E.Fields.FindRef(TEXT("attachments")), Att); } else { Att = W->Attachments; }
	// A first guess that is ON the weapon: under the fore-end, a light's usual place; short of
	// the muzzle on a weapon without one. Each further point a little further along.
	FVector V;
	const FVector Fore = ParseVector(E.Fields.FindRef(TEXT("fore_grip")), V) ? V : W->ForeGrip;
	const FVector Muzzle = ParseVector(E.Fields.FindRef(TEXT("muzzle")), V) ? V : W->Muzzle;
	FVector P = W->bHasForeGrip ? FVector(Fore.X + 4.0, 0.0, Fore.Z - 3.0) : FVector(Muzzle.X - 12.0, 0.0, Muzzle.Z - 3.0);
	P.X += 2.0 * Att.Num();
	Att.Add(P);
	const FString Text = VectorsText(Att);
	E.Fields.Add(TEXT("attachments"), Text);
	if (TObjectPtr<UEditableTextBox>* Box = FieldBoxes.Find(TEXT("attachments"))) { if (*Box) { (*Box)->SetText(FText::FromString(Text)); } }
	ArmedKey = FString::Printf(TEXT("attach:%d"), Att.Num() - 1);
	FillMetaTable(W);
	if (DetailNote) { DetailNote->SetText(FText::FromString(TEXT("ATTACH POINT ADDED -- drag it into place, SAVE to keep it"))); }
}

void UReferenceWidget::RemoveArmedAttachment()
{
	if (!Entries.IsValidIndex(SelectedIndex) || !ArmedKey.StartsWith(TEXT("attach:"))) { return; }
	FReferenceEntry& E = Entries[SelectedIndex];
	TArray<FVector> Att;
	if (E.Fields.Contains(TEXT("attachments"))) { ParseVectors(E.Fields.FindRef(TEXT("attachments")), Att); }
	else if (const WeaponCatalog::FWeapon* W = WeaponCatalog::Find(E.Name)) { Att = W->Attachments; }
	const int32 N = FCString::Atoi(*ArmedKey.Mid(7));
	if (!Att.IsValidIndex(N)) { return; }
	Att.RemoveAt(N);
	const FString Text = VectorsText(Att);
	E.Fields.Add(TEXT("attachments"), Text);
	if (TObjectPtr<UEditableTextBox>* Box = FieldBoxes.Find(TEXT("attachments"))) { if (*Box) { (*Box)->SetText(FText::FromString(Text)); } }
	ArmedKey.Reset();
	FillMetaTable(WeaponCatalog::Find(E.Name));
	if (DetailNote) { DetailNote->SetText(FText::FromString(TEXT("ATTACH POINT REMOVED -- SAVE to keep it"))); }
}

void UReferenceWidget::PaintMarkers(FSlateWindowElementList& OutDrawElements, int32 Layer) const
{
	if (!bShowMeta || !FeedShown() || !OwnerController || !Entries.IsValidIndex(SelectedIndex)) { return; }
	const WeaponCatalog::FWeapon* W = WeaponCatalog::Find(Entries[SelectedIndex].Name);
	FTransform Camera, Piece; float Fov = 34.0f;
	if ((!W && Entries[SelectedIndex].Category != TEXT("optics")) || !OwnerController->GetWeaponPreviewFrame(Camera, Fov, Piece)) { return; }
	const FGeometry& FeedGeo = WeaponFeed->GetCachedGeometry();
	const FVector2f Sz(FeedGeo.GetLocalSize());
	if (Sz.X <= 1.0f || Sz.Y <= 1.0f) { return; }
	// A pinhole: the capture's FOV is horizontal, the target is the feed's shape.
	const float TanH = FMath::Tan(FMath::DegreesToRadians(FMath::Max(1.0f, Fov) * 0.5f));
	const float Aspect = Sz.X / Sz.Y;
	auto Project = [&](const FVector& Local, FVector2f& Out) -> bool
	{
		const FVector V = Camera.InverseTransformPosition(Piece.TransformPosition(Local));
		if (V.X < 1.0f) { return false; }   // behind the lens
		Out.X = Sz.X * (0.5f + (V.Y / V.X) / (2.0f * TanH));
		Out.Y = Sz.Y * (0.5f - (V.Z / V.X) * Aspect / (2.0f * TanH));
		return true;
	};
	// The frame at the grip: +X downrange red, +Y green, +Z up blue, ten centimetres each.
	FVector2f O;
	if (Project(FVector::ZeroVector, O))
	{
		const FVector Axes[3] = { FVector(10, 0, 0), FVector(0, 10, 0), FVector(0, 0, 10) };
		const FLinearColor Colours[3] = { FLinearColor::Red, FLinearColor::Green, FLinearColor(0.3f, 0.5f, 1.0f) };
		for (int32 i = 0; i < 3; ++i)
		{
			FVector2f E;
			if (Project(Axes[i], E)) { FSlateDrawElement::MakeLines(OutDrawElements, Layer, FeedGeo.ToPaintGeometry(), TArray<FVector2f>{ O, E }, ESlateDrawEffect::None, Colours[i], true, 1.5f); }
		}
	}
	for (const FMarkerHit& M : CurrentMarkers())
	{
		FVector2f P;
		if (!Project(M.Local, P)) { continue; }
		// A dark ring under a filled dot, so the dot reads on a bright surface as well as a dark one.
		TArray<FVector2f> Ring;
		for (int32 i = 0; i <= 16; ++i) { const float A = 2.0f * PI * i / 16.0f; Ring.Add(P + FVector2f(7.0f * FMath::Cos(A), 7.0f * FMath::Sin(A))); }
		FSlateDrawElement::MakeLines(OutDrawElements, Layer, FeedGeo.ToPaintGeometry(), Ring, ESlateDrawEffect::None, FLinearColor(0.0f, 0.0f, 0.0f, 0.7f), true, 2.0f);
		if (M.Key.IsEmpty())
		{
			// Derived: a ring in its colour, not a dot. (A fully rounded box with a clear fill and a
			// coloured outline; the element copies the brush, so a local is fine.)
			const FSlateRoundedBoxBrush RingBrush(FLinearColor::Transparent, M.Colour, 2.0f, FVector2D(12, 12));
			FSlateDrawElement::MakeBox(OutDrawElements, Layer + 1, FeedGeo.ToPaintGeometry(FVector2D(12.0, 12.0), FSlateLayoutTransform(FVector2D(P.X - 6.0f, P.Y - 6.0f))), &RingBrush, ESlateDrawEffect::None, FLinearColor::White);
		}
		else
		{
			FSlateDrawElement::MakeBox(OutDrawElements, Layer + 1, FeedGeo.ToPaintGeometry(FVector2D(12.0, 12.0), FSlateLayoutTransform(FVector2D(P.X - 6.0f, P.Y - 6.0f))), &DotBrush, ESlateDrawEffect::None, M.Colour);
			if (M.Key == ArmedKey || (DragMarker >= 0 && CurrentMarkers().IsValidIndex(DragMarker) && CurrentMarkers()[DragMarker].Key == M.Key))
			{
				// Armed or in hand: a second, wider ring says which one the drag owns.
				const FSlateRoundedBoxBrush ArmRing(FLinearColor::Transparent, FLinearColor::White, 1.5f, FVector2D(22, 22));
				FSlateDrawElement::MakeBox(OutDrawElements, Layer + 2, FeedGeo.ToPaintGeometry(FVector2D(22.0, 22.0), FSlateLayoutTransform(FVector2D(P.X - 11.0f, P.Y - 11.0f))), &ArmRing, ESlateDrawEffect::None, FLinearColor::White);
			}
		}
	}
}
