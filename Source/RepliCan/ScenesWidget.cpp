#include "ScenesWidget.h"
#include "CrtStyle.h"
#include "CrtTabsWidget.h"
#include "CrtRuleWidget.h"
#include "SheetSpec.h"
#include "SequenceData.h"
#include "BasePlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

void UScenesWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// The same shell as the character sheet: the panel border, the same padding, one column.
	// PlaceConsolePage gives it the same rectangle, so the two read as pages of one console.
	UBorder* Root = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Root"));
	Root->SetBrushColor(Crt::Panel);
	Root->SetPadding(FMargin(34.0f, 26.0f));
	WidgetTree->RootWidget = Root;
	Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ScenesColumn"));
	Root->SetContent(Column);
}

TArray<UScenesWidget::FRow> UScenesWidget::ScanSequences() const
{
	TArray<FRow> Rows;
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(SequenceFile::GetDirectory() / TEXT("*.json")), true, false);
	Files.Sort();
	for (const FString& File : Files)
	{
		FRow Row;
		Row.Name = FPaths::GetBaseFilename(File);
		// Load it to describe it. These files are small and the panel opens rarely, so reading
		// them beats keeping a second index that can disagree with the folder.
		FSequence Seq;
		if (SequenceFile::Load(Row.Name, Seq))
		{
			int32 Lines = 0;
			for (const FSequenceStep& Step : Seq.Steps) { if (!Step.Text.IsEmpty()) { ++Lines; } }
			Row.Detail = FString::Printf(TEXT("%d steps, %d lines, %d cast"), Seq.Steps.Num(), Lines, Seq.Actors.Num());
			if (!Seq.Name.IsEmpty() && Seq.Name != Row.Name) { Row.Detail = Seq.Name + TEXT("  --  ") + Row.Detail; }
		}
		else
		{
			Row.Detail = TEXT("will not parse");
		}
		Rows.Add(MoveTemp(Row));
	}
	return Rows;
}

TArray<UScenesWidget::FRow> UScenesWidget::ScanCharacters() const
{
	// Every character file: its name as written, its type and kit, and whether a conversation
	// file exists for it. Read straight from the JSON so a field added by hand shows up.
	TArray<FRow> Rows;
	const FString Dir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Characters"));
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(Dir / TEXT("*.json")), true, false);
	Files.Sort();
	for (const FString& File : Files)
	{
		FRow Row;
		Row.Name = FPaths::GetBaseFilename(File);
		FString Json;
		TSharedPtr<FJsonObject> Obj;
		if (FFileHelper::LoadFileToString(Json, *(Dir / File)))
		{
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
			FJsonSerializer::Deserialize(Reader, Obj);
		}
		if (Obj.IsValid())
		{
			FString Name, Type, Kit, Desc;
			Obj->TryGetStringField(TEXT("displayName"), Name);
			if (Name.IsEmpty()) { Obj->TryGetStringField(TEXT("name"), Name); }
			Obj->TryGetStringField(TEXT("type"), Type);
			Obj->TryGetStringField(TEXT("kit"), Kit);
			Obj->TryGetStringField(TEXT("description"), Desc);
			if (!Name.IsEmpty()) { Row.Name = Name; }
			TArray<FString> Bits;
			if (!Type.IsEmpty()) { Bits.Add(Type); }
			if (!Kit.IsEmpty()) { Bits.Add(Kit + TEXT(" kit")); }
			Bits.Add(TEXT("file ") + FPaths::GetBaseFilename(File));
			const FString Conv = FPaths::Combine(FPaths::ProjectDir(), TEXT("Conversations"), Row.Name + TEXT(".json"));
			Bits.Add(FPaths::FileExists(Conv) ? TEXT("has a conversation") : TEXT("no conversation"));
			Row.Detail = FString::Join(Bits, TEXT("  --  "));
			Row.Extra = Desc;
		}
		else
		{
			Row.Detail = TEXT("will not parse");
		}
		Rows.Add(MoveTemp(Row));
	}
	return Rows;
}

void UScenesWidget::Rebuild()
{
	if (!Column) { return; }
	const FSheetSpec& S = FSheetSpec::Get();
	Column->ClearChildren();
	ButtonScenes.Reset();

	// The header rule with the tab strip in it, exactly as the sheet builds its own.
	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Header->AddChildToHorizontalBox(Crt::FixedText(WidgetTree, S.HeaderLeft, S.RuleSize, Crt::Faint))->SetVerticalAlignment(VAlign_Center);
	Tabs = UCrtTabsWidget::Make(GetOwningPlayer(), { TEXT("CHARACTERS"), TEXT("SEQUENCES") }, ActiveTab, S.NameSize);
	Tabs->OnTab.BindUObject(this, &UScenesWidget::OnTab);
	Header->AddChildToHorizontalBox(Tabs)->SetVerticalAlignment(VAlign_Center);
	UHorizontalBoxSlot* RuleSlot = Header->AddChildToHorizontalBox(UCrtRuleWidget::Make(GetOwningPlayer(), S.HeaderRight, S.RuleSize, Crt::Faint));
	RuleSlot->SetSize(ESlateSizeRule::Fill); RuleSlot->SetVerticalAlignment(VAlign_Center);
	if (!S.HeaderEnd.IsEmpty()) { Header->AddChildToHorizontalBox(Crt::FixedText(WidgetTree, S.HeaderEnd, S.RuleSize, Crt::Faint))->SetVerticalAlignment(VAlign_Center); }
	UButton* Close = Crt::Button(WidgetTree, TEXT("[ X ]"), S.NameSize, Crt::Green);
	Close->OnClicked.AddDynamic(this, &UScenesWidget::OnCloseClicked);
	Header->AddChildToHorizontalBox(Close)->SetPadding(FMargin(12, 0, 0, 0));
	Column->AddChildToVerticalBox(Header)->SetPadding(FMargin(0, 0, 0, 12));

	// The page body scrolls; the tab decides what is in it.
	UScrollBox* Scroll = Crt::ScrollBox(WidgetTree, 10.0f);
	Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Scroll->AddChild(Content);
	Column->AddChildToVerticalBox(Scroll)->SetSize(ESlateSizeRule::Fill);
	FillTab();
}

void UScenesWidget::FillTab()
{
	if (!Content) { return; }
	const FSheetSpec& S = FSheetSpec::Get();
	Content->ClearChildren();
	ButtonScenes.Reset();
	if (ActiveTab == TabCharacters)
	{
		Content->AddChildToVerticalBox(Crt::Text(WidgetTree, TEXT("Every character file in Characters/, read each time this opens. Place and edit them from the Character Manager in edit mode."), S.RowSize, Crt::DimGreen))->SetPadding(FMargin(0, 0, 0, 14));
		const TArray<FRow> Rows = ScanCharacters();
		if (Rows.Num() == 0) { Content->AddChildToVerticalBox(Crt::Text(WidgetTree, TEXT("  Nothing in Characters/."), S.InfoNameSize, Crt::DimGreen))->SetPadding(FMargin(0, 6)); }
		for (const FRow& Row : Rows)
		{
			UVerticalBox* Names = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			Names->AddChildToVerticalBox(Crt::Text(WidgetTree, TEXT("  ") + Row.Name.ToUpper(), S.InfoNameSize, Crt::Green));
			Names->AddChildToVerticalBox(Crt::Text(WidgetTree, TEXT("  ") + Row.Detail, S.RowSize, Crt::DimGreen));
			if (!Row.Extra.IsEmpty()) { Names->AddChildToVerticalBox(Crt::Text(WidgetTree, TEXT("  ") + Row.Extra, S.RowSize, Crt::DimGreen)); }
			Content->AddChildToVerticalBox(Names)->SetPadding(FMargin(0, 5));
		}
		return;
	}
	Content->AddChildToVerticalBox(Crt::Text(WidgetTree, TEXT("Jump to the start of a sequence and play it through. Read from Sequences/ each time this opens."), S.RowSize, Crt::DimGreen))->SetPadding(FMargin(0, 0, 0, 14));
	const TArray<FRow> Rows = ScanSequences();
	if (Rows.Num() == 0) { Content->AddChildToVerticalBox(Crt::Text(WidgetTree, TEXT("  Nothing in Sequences/."), S.InfoNameSize, Crt::DimGreen))->SetPadding(FMargin(0, 6)); }
	for (const FRow& Row : Rows)
	{
		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UVerticalBox* Names = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Names->AddChildToVerticalBox(Crt::Text(WidgetTree, TEXT("  ") + Row.Name.ToUpper(), S.InfoNameSize, Crt::Green));
		Names->AddChildToVerticalBox(Crt::Text(WidgetTree, TEXT("  ") + Row.Detail, S.RowSize, Crt::DimGreen));
		UHorizontalBoxSlot* NameSlot = Line->AddChildToHorizontalBox(Names);
		NameSlot->SetSize(ESlateSizeRule::Fill);
		NameSlot->SetVerticalAlignment(VAlign_Center);
		UButton* Play = Crt::Button(WidgetTree, TEXT("[ PLAY ]"), S.InfoNameSize);
		Play->OnClicked.AddDynamic(this, &UScenesWidget::OnPlayClicked);
		ButtonScenes.Add(Play, Row.Name);
		UHorizontalBoxSlot* PlaySlot = Line->AddChildToHorizontalBox(Play);
		PlaySlot->SetHorizontalAlignment(HAlign_Right);
		PlaySlot->SetVerticalAlignment(VAlign_Center);
		PlaySlot->SetPadding(FMargin(0, 0, 28, 0));
		Content->AddChildToVerticalBox(Line)->SetPadding(FMargin(0, 5));
	}
}

void UScenesWidget::OnTab(int32 Index)
{
	ActiveTab = Index;
	Rebuild();   // the strip is rebuilt with the new active tab, the body with the new list
}

void UScenesWidget::OnPlayClicked()
{
	// The hovered button is the one that was clicked; see ButtonScenes.
	for (const TPair<TObjectPtr<UButton>, FString>& Pair : ButtonScenes)
	{
		if (!Pair.Key || !Pair.Key->IsHovered()) { continue; }
		const FString Scene = Pair.Value;
		// Close everything first: a scene that starts behind a panel looks broken and cannot be
		// watched. OnClose unwinds the menu, then the sequence takes the screen.
		OnClose.ExecuteIfBound();
		if (OwnerController) { OwnerController->PlaySequence(Scene); }
		return;
	}
}

void UScenesWidget::OnCloseClicked()
{
	OnClose.ExecuteIfBound();
}
