#include "UI/ScenesWidget.h"
#include "UI/CrtStyle.h"
#include "UI/CrtTabsWidget.h"
#include "UI/CrtRuleWidget.h"
#include "UI/SheetSpec.h"
#include "Narrative/SequenceData.h"
#include "Core/BasePlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Dom/JsonObject.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	const float ColName = 210.0f, ColType = 90.0f, ColKit = 120.0f, ColFile = 170.0f, ColTalk = 110.0f;
	const float ViewerWidth = 300.0f;

	FString Clock(float Seconds)
	{
		const int32 S = FMath::RoundToInt(FMath::Max(0.0f, Seconds));
		return FString::Printf(TEXT("%d:%02d"), S / 60, S % 60);
	}
	FString LocalStamp(const FString& Path)
	{
		const FDateTime Utc = IFileManager::Get().GetTimeStamp(*Path);
		if (Utc == FDateTime::MinValue()) { return TEXT("never"); }
		return (Utc + (FDateTime::Now() - FDateTime::UtcNow())).ToString(TEXT("%Y-%m-%d %H:%M"));
	}
	// The baked voice lengths, by "Sequences/<Name>/<step>", from the voice bake's manifest.
	TMap<FString, float> VoiceSeconds()
	{
		TMap<FString, float> Out;
		FString Json;
		if (!FFileHelper::LoadFileToString(Json, *FPaths::Combine(FPaths::ProjectDir(), TEXT("Conversations"), TEXT("Voice"), TEXT("manifest.json")))) { return Out; }
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) { return Out; }
		for (const auto& Pair : Root->Values)
		{
			const TSharedPtr<FJsonObject>* O = nullptr;
			double Seconds = 0.0;
			if (Pair.Value->TryGetObject(O) && O && (*O)->TryGetNumberField(TEXT("seconds"), Seconds)) { Out.Add(FString(Pair.Key.ToView()), (float)Seconds); }
		}
		return Out;
	}
}

void UScenesWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	// The same shell as the character sheet: the panel border, the same padding, one column.
	// PlaceConsolePage gives it the same rectangle, so the two read as pages of one console.
	UBorder* Root = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Root"));
	Root->SetBrushColor(Crt::Panel);
	Root->SetPadding(FMargin(34.0f, 26.0f));
	WidgetTree->RootWidget = Root;
	Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ScenesColumn"));
	Root->SetContent(Column);
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
		Row.File = FPaths::GetBaseFilename(File);
		Row.Name = Row.File;
		FString Json;
		TSharedPtr<FJsonObject> Obj;
		if (FFileHelper::LoadFileToString(Json, *(Dir / File)))
		{
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
			FJsonSerializer::Deserialize(Reader, Obj);
		}
		if (Obj.IsValid())
		{
			FString Name;
			Obj->TryGetStringField(TEXT("displayName"), Name);
			if (Name.IsEmpty()) { Obj->TryGetStringField(TEXT("name"), Name); }
			if (!Name.IsEmpty()) { Row.Name = Name; }
			Obj->TryGetStringField(TEXT("type"), Row.Type);
			Obj->TryGetStringField(TEXT("kit"), Row.Kit);
			Obj->TryGetStringField(TEXT("description"), Row.Description);
			Row.bTalks = FPaths::FileExists(FPaths::Combine(FPaths::ProjectDir(), TEXT("Conversations"), Row.Name + TEXT(".json")))
				|| FPaths::FileExists(FPaths::Combine(FPaths::ProjectDir(), TEXT("Conversations"), Row.File + TEXT(".json")));
		}
		else { Row.bParses = false; Row.Description = TEXT("will not parse"); }
		Rows.Add(MoveTemp(Row));
	}
	return Rows;
}

TArray<UScenesWidget::FSceneRow> UScenesWidget::ScanSequences(const TMap<FString, FString>& DisplayNames) const
{
	TArray<FSceneRow> Rows;
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(SequenceFile::GetDirectory() / TEXT("*.json")), true, false);
	Files.Sort();
	const TMap<FString, float> Voiced = VoiceSeconds();
	for (const FString& File : Files)
	{
		FSceneRow Row;
		Row.Name = FPaths::GetBaseFilename(File);
		Row.Changed = LocalStamp(SequenceFile::GetPath(Row.Name));
		// Load it to describe it. These files are small and the panel opens rarely, so reading
		// them beats keeping a second index that can disagree with the folder.
		FSequence Seq;
		if (!SequenceFile::Load(Row.Name, Seq)) { Row.bParses = false; Rows.Add(MoveTemp(Row)); continue; }
		Row.Title = (!Seq.Name.IsEmpty() && Seq.Name != Row.Name) ? Seq.Name : FString();
		Row.bUnskippable = Seq.bUnskippable;
		// The steps by kind, and about how long they take: waits and fades as written, a line as
		// long as its baked voice (or its words at a reading pace), a camera move as its blend,
		// a choice as a few seconds of thought. Branches can skip some of this, so it is "about".
		int32 Lines = 0, VoicedLines = 0, Shots = 0, Fades = 0, Choices = 0, Options = 0, Branches = 0, Waits = 0, Looks = 0;
		float Speech = 0.0f, Waiting = 0.0f, Blends = 0.0f, Other = 0.0f;
		for (int32 i = 0; i < Seq.Steps.Num(); ++i)
		{
			const FSequenceStep& St = Seq.Steps[i];
			switch (St.Type)
			{
			case ESequenceStepType::Say:
			{
				++Lines;
				const float* Baked = Voiced.Find(FString::Printf(TEXT("Sequences/%s/%d"), *Row.Name, i));
				if (Baked) { ++VoicedLines; Speech += *Baked; }
				else
				{
					int32 Words = 1; for (const TCHAR C : St.Text) { if (C == TEXT(' ')) { ++Words; } }
					Speech += Words / 2.5f + 0.5f;   // a reading pace
				}
				if (St.bWaitForInput) { Other += 1.5f; }
				break;
			}
			case ESequenceStepType::Wait: ++Waits; Waiting += St.Seconds; break;
			case ESequenceStepType::Fade: ++Fades; Blends += St.Seconds; break;
			case ESequenceStepType::End: Blends += St.Seconds; break;
			case ESequenceStepType::Camera: ++Shots; Blends += St.Shot.Blend; break;
			case ESequenceStepType::Look: ++Looks; Waiting += FMath::Max(0.0f, St.Seconds); break;
			case ESequenceStepType::Choice: ++Choices; Options += St.Options.Num(); Other += 3.0f; break;
			case ESequenceStepType::Goto: ++Branches; break;
			case ESequenceStepType::Ambient: Other += St.Seconds; break;
			case ESequenceStepType::WaitBlinks: Other += 4.0f; break;
			case ESequenceStepType::Shock: case ESequenceStepType::Remote: case ESequenceStepType::Panel: case ESequenceStepType::Move: Other += 3.0f; break;
			case ESequenceStepType::Turn: Other += 1.0f; break;
			default: break;
			}
		}
		TArray<FString> Bits;
		Bits.Add(FString::Printf(TEXT("%d lines%s"), Lines, VoicedLines > 0 ? *FString::Printf(TEXT(" (%d voiced)"), VoicedLines) : TEXT("")));
		if (Shots) { Bits.Add(FString::Printf(TEXT("%d shots"), Shots)); }
		if (Looks) { Bits.Add(FString::Printf(TEXT("%d looks"), Looks)); }
		if (Fades) { Bits.Add(FString::Printf(TEXT("%d fades"), Fades)); }
		if (Waits) { Bits.Add(FString::Printf(TEXT("%d waits"), Waits)); }
		if (Choices) { Bits.Add(FString::Printf(TEXT("%d choice%s (%d options)"), Choices, Choices == 1 ? TEXT("") : TEXT("s"), Options)); }
		if (Branches) { Bits.Add(FString::Printf(TEXT("%d branch%s"), Branches, Branches == 1 ? TEXT("") : TEXT("es"))); }
		Row.Counts = FString::Printf(TEXT("%d steps: %s"), Seq.Steps.Num(), *FString::Join(Bits, TEXT(", ")));
		TArray<FString> Cast;
		for (const FSequenceActor& A : Seq.Actors)
		{
			const FString* Disp = DisplayNames.Find(A.Character);
			Cast.Add(FString::Printf(TEXT("%s = %s"), *A.Id, Disp ? **Disp : *A.Character));
		}
		Row.Cast = Cast.Num() > 0 ? FString::Printf(TEXT("cast %d: %s"), Cast.Num(), *FString::Join(Cast, TEXT(", "))) : TEXT("no cast (the player alone)");
		const float Total = Speech + Waiting + Blends + Other;
		Row.Runtime = FString::Printf(TEXT("runtime about %s  (speech %s, waits %s, fades and blends %s%s)"), *Clock(Total), *Clock(Speech), *Clock(Waiting), *Clock(Blends), Other > 0.5f ? *FString::Printf(TEXT(", other %s"), *Clock(Other)) : TEXT(""));
		Rows.Add(MoveTemp(Row));
	}
	return Rows;
}

void UScenesWidget::Rebuild()
{
	if (!Column) { return; }
	const FSheetSpec& S = FSheetSpec::Get();
	Column->ClearChildren();
	ButtonScenes.Reset(); ButtonCharacters.Reset(); Feed = nullptr; ViewerName = nullptr; ViewerText = nullptr;

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

	Body = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Column->AddChildToVerticalBox(Body)->SetSize(ESlateSizeRule::Fill);
	FillTab();
}

void UScenesWidget::FillTab()
{
	if (!Body) { return; }
	Body->ClearChildren();
	ButtonScenes.Reset(); ButtonCharacters.Reset();
	if (ActiveTab == TabCharacters) { BuildCharacters(Body); } else { BuildSequences(Body); }
}

void UScenesWidget::BuildCharacters(UVerticalBox* Into)
{
	const FSheetSpec& S = FSheetSpec::Get();
	CharacterRows = ScanCharacters();
	UHorizontalBox* Split = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Into->AddChildToVerticalBox(Split)->SetSize(ESlateSizeRule::Fill);

	// THE VIEWER, on the left in its own column: the sheet's booth, 5:8 like the sheet's mirror,
	// with the name and the file's description under it. Drag on it to turn the figure.
	USizeBox* ViewerBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	ViewerBox->SetWidthOverride(ViewerWidth);
	UVerticalBox* Viewer = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	ViewerBox->AddChild(Viewer);
	USizeBox* Fit = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Fit->SetWidthOverride(ViewerWidth); Fit->SetHeightOverride(FMath::Floor(ViewerWidth / 0.625f));
	Feed = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	Feed->SetColorAndOpacity(FLinearColor::White);
	Feed->SetVisibility(ESlateVisibility::Hidden);
	Fit->AddChild(Feed);
	Viewer->AddChildToVerticalBox(Fit)->SetHorizontalAlignment(HAlign_Fill);
	ViewerName = Crt::Text(WidgetTree, TEXT(""), S.InfoNameSize, Crt::Green);
	Viewer->AddChildToVerticalBox(ViewerName)->SetPadding(FMargin(0, 8, 0, 2));
	ViewerText = Crt::Text(WidgetTree, TEXT("click a row to see the character; drag the picture to turn them"), S.RowSize, Crt::DimGreen);
	ViewerText->SetAutoWrapText(true);
	Viewer->AddChildToVerticalBox(ViewerText);
	UHorizontalBoxSlot* VS = Split->AddChildToHorizontalBox(ViewerBox);
	VS->SetSize(ESlateSizeRule::Automatic); VS->SetVerticalAlignment(VAlign_Top); VS->SetPadding(FMargin(0, 0, S.ColumnGap, 0));

	// THE TABLE, on the right: a header that stays put, and the rows scrolling under it.
	UVerticalBox* Table = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UHorizontalBoxSlot* TS = Split->AddChildToHorizontalBox(Table);
	TS->SetSize(ESlateSizeRule::Fill); TS->SetVerticalAlignment(VAlign_Fill);
	auto Cell = [&](UHorizontalBox* Line, float Width, const FString& Text, const FLinearColor& Colour)
	{
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetWidthOverride(Width);
		Box->AddChild(Crt::Text(WidgetTree, Text, S.RowSize, Colour));
		UHorizontalBoxSlot* CS = Line->AddChildToHorizontalBox(Box);
		CS->SetVerticalAlignment(VAlign_Center);
		CS->SetPadding(FMargin(0, 0, 10, 0));
	};
	UHorizontalBox* Head = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Cell(Head, ColName, TEXT("NAME"), Crt::DimGreen); Cell(Head, ColType, TEXT("TYPE"), Crt::DimGreen); Cell(Head, ColKit, TEXT("KIT"), Crt::DimGreen);
	Cell(Head, ColFile, TEXT("FILE"), Crt::DimGreen); Cell(Head, ColTalk, TEXT("TALKS"), Crt::DimGreen);
	Table->AddChildToVerticalBox(Head)->SetPadding(FMargin(8, 0, 0, 2));
	Table->AddChildToVerticalBox(UCrtRuleWidget::Make(GetOwningPlayer(), TEXT("-"), S.RuleSize, Crt::Faint))->SetPadding(FMargin(0, 0, 0, 4));
	UScrollBox* Scroll = Crt::ScrollBox(WidgetTree, 10.0f);
	Table->AddChildToVerticalBox(Scroll)->SetSize(ESlateSizeRule::Fill);
	if (CharacterRows.Num() == 0) { Scroll->AddChild(Crt::Text(WidgetTree, TEXT("  Nothing in Characters/."), S.InfoNameSize, Crt::DimGreen)); }
	for (const FRow& Row : CharacterRows)
	{
		UButton* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		B->SetStyle(Crt::ButtonStyle());
		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		const FLinearColor Ink = Row.bParses ? Crt::Green : Crt::DimGreen;
		Cell(Line, ColName, Row.Name, Ink); Cell(Line, ColType, Row.Type, Crt::DimGreen); Cell(Line, ColKit, Row.Kit, Crt::DimGreen);
		Cell(Line, ColFile, Row.File, Crt::DimGreen); Cell(Line, ColTalk, Row.bTalks ? TEXT("yes") : TEXT("--"), Crt::DimGreen);
		B->AddChild(Line);
		if (UButtonSlot* BS = Cast<UButtonSlot>(Line->Slot)) { BS->SetPadding(FMargin(8, 3)); BS->SetHorizontalAlignment(HAlign_Fill); }
		B->OnClicked.AddDynamic(this, &UScenesWidget::OnRowClicked);
		ButtonCharacters.Add(B, Row.File);
		Scroll->AddChild(B);
	}
	// The one in the viewer: what was there, else the player, else the first row.
	const FRow* First = nullptr;
	for (const FRow& Row : CharacterRows) { if (Row.File == Shown) { First = &Row; break; } }
	if (!First) { for (const FRow& Row : CharacterRows) { if (Row.File == TEXT("Player")) { First = &Row; break; } } }
	if (!First && CharacterRows.Num() > 0) { First = &CharacterRows[0]; }
	if (First) { ShowCharacter(*First); }
}

void UScenesWidget::BuildSequences(UVerticalBox* Into)
{
	const FSheetSpec& S = FSheetSpec::Get();
	TMap<FString, FString> DisplayNames;
	for (const FRow& Row : ScanCharacters()) { DisplayNames.Add(Row.File, Row.Name); }
	UScrollBox* Scroll = Crt::ScrollBox(WidgetTree, 10.0f);
	Into->AddChildToVerticalBox(Scroll)->SetSize(ESlateSizeRule::Fill);
	const TArray<FSceneRow> Rows = ScanSequences(DisplayNames);
	if (Rows.Num() == 0) { Scroll->AddChild(Crt::Text(WidgetTree, TEXT("  Nothing in Sequences/."), S.InfoNameSize, Crt::DimGreen)); }
	for (const FSceneRow& Row : Rows)
	{
		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UVerticalBox* Names = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UHorizontalBox* Top = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Top->AddChildToHorizontalBox(Crt::Text(WidgetTree, TEXT("  ") + Row.Name.ToUpper(), S.InfoNameSize, Crt::Green))->SetVerticalAlignment(VAlign_Center);
		if (!Row.Title.IsEmpty()) { Top->AddChildToHorizontalBox(Crt::Text(WidgetTree, TEXT("  \"") + Row.Title + TEXT("\""), S.RowSize, Crt::Green))->SetVerticalAlignment(VAlign_Center); }
		Names->AddChildToVerticalBox(Top);
		if (!Row.bParses) { Names->AddChildToVerticalBox(Crt::Text(WidgetTree, TEXT("  will not parse  --  changed ") + Row.Changed, S.RowSize, Crt::Amber)); }
		else
		{
			Names->AddChildToVerticalBox(Crt::Text(WidgetTree, FString::Printf(TEXT("  changed %s%s"), *Row.Changed, Row.bUnskippable ? TEXT("  --  unskippable") : TEXT("")), S.RowSize, Crt::DimGreen));
			Names->AddChildToVerticalBox(Crt::Text(WidgetTree, TEXT("  ") + Row.Counts, S.RowSize, Crt::DimGreen));
			Names->AddChildToVerticalBox(Crt::Text(WidgetTree, TEXT("  ") + Row.Cast, S.RowSize, Crt::DimGreen));
			Names->AddChildToVerticalBox(Crt::Text(WidgetTree, TEXT("  ") + Row.Runtime, S.RowSize, Crt::DimGreen));
		}
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
		Scroll->AddChild(Line);
		if (UScrollBoxSlot* LS = Cast<UScrollBoxSlot>(Line->Slot)) { LS->SetPadding(FMargin(0, 6)); }
	}
}

void UScenesWidget::ShowCharacter(const FRow& Row)
{
	Shown = Row.File;
	if (ViewerName) { ViewerName->SetText(FText::FromString(Row.Name.ToUpper())); }
	if (ViewerText)
	{
		TArray<FString> Bits;
		if (!Row.Type.IsEmpty()) { Bits.Add(Row.Type); }
		if (!Row.Kit.IsEmpty()) { Bits.Add(Row.Kit + TEXT(" kit")); }
		Bits.Add(TEXT("file ") + Row.File);
		Bits.Add(Row.bTalks ? TEXT("has a conversation") : TEXT("no conversation"));
		ViewerText->SetText(FText::FromString(FString::Join(Bits, TEXT("  --  ")) + (Row.Description.IsEmpty() ? FString() : TEXT("\n") + Row.Description)));
	}
	if (OwnerController && !OwnerController->ShowSheetMirrorFor(Row.File))
	{
		if (ViewerText) { ViewerText->SetText(FText::FromString(TEXT("no likeness: the file did not load"))); }
	}
}

void UScenesWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!Feed) { return; }
	if (UTextureRenderTarget2D* Target = OwnerController ? OwnerController->GetSheetFeed() : nullptr)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(Target);
		Brush.ImageSize = FVector2D(Target->SizeX, Target->SizeY);
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Feed->SetBrush(Brush);
		Feed->SetVisibility(ESlateVisibility::Visible);
	}
	else { Feed->SetVisibility(ESlateVisibility::Hidden); }
}

FReply UScenesWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && Feed && Feed->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
	{
		bDragging = true; DragLast = InMouseEvent.GetScreenSpacePosition();
		return FReply::Handled().CaptureMouse(TakeWidget());
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UScenesWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDragging && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton) { bDragging = false; return FReply::Handled().ReleaseMouseCapture(); }
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UScenesWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDragging)
	{
		const FVector2D Now = InMouseEvent.GetScreenSpacePosition();
		const FVector2D Delta = Now - DragLast; DragLast = Now;
		if (OwnerController) { OwnerController->OrbitSheetMirror(-Delta.X * 0.4f, Delta.Y * 0.25f); }   // as on the sheet: drag right turns them right
		return FReply::Handled();
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

void UScenesWidget::OnTab(int32 Index)
{
	if (Index != TabCharacters && OwnerController) { OwnerController->HideCharacterPreview(); }
	ActiveTab = Index;
	Rebuild();   // the strip is rebuilt with the new active tab, the body with the new list
}

void UScenesWidget::OnRowClicked()
{
	for (const TPair<TObjectPtr<UButton>, FString>& Pair : ButtonCharacters)
	{
		if (!Pair.Key || !Pair.Key->IsHovered()) { continue; }
		for (const FRow& Row : CharacterRows) { if (Row.File == Pair.Value) { ShowCharacter(Row); return; } }
		return;
	}
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
