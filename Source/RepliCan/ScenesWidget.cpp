#include "ScenesWidget.h"
#include "CrtStyle.h"
#include "SequenceData.h"
#include "BasePlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

void UScenesWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ScenesColumn"));
	WidgetTree->RootWidget = Column;
}

TArray<UScenesWidget::FSceneRow> UScenesWidget::Scan() const
{
	TArray<FSceneRow> Rows;
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(SequenceFile::GetDirectory() / TEXT("*.json")), true, false);
	Files.Sort();
	for (const FString& File : Files)
	{
		FSceneRow Row;
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

void UScenesWidget::Rebuild()
{
	if (!Column) { return; }
	Column->ClearChildren();
	ButtonScenes.Reset();

	Column->AddChildToVerticalBox(Crt::Text(WidgetTree, TEXT("SEQUENCES"), 34, Crt::Green, ETextJustify::Center))->SetPadding(FMargin(0, 0, 0, 4));
	Column->AddChildToVerticalBox(Crt::Text(WidgetTree,
		TEXT("Jump to the start of a sequence and play it through. Read from Sequences/ each time this opens."),
		15, Crt::DimGreen, ETextJustify::Center))->SetPadding(FMargin(0, 0, 0, 14));

	const TArray<FSceneRow> Rows = Scan();
	if (Rows.Num() == 0)
	{
		Column->AddChildToVerticalBox(Crt::Text(WidgetTree, TEXT("  Nothing in Sequences/."), 17, Crt::DimGreen))->SetPadding(FMargin(0, 6));
	}
	for (const FSceneRow& Row : Rows)
	{
		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

		UVerticalBox* Names = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Names->AddChildToVerticalBox(Crt::Text(WidgetTree, TEXT("  ") + Row.Name.ToUpper(), 17, Crt::Green));
		Names->AddChildToVerticalBox(Crt::Text(WidgetTree, TEXT("  ") + Row.Detail, 13, Crt::DimGreen));
		UHorizontalBoxSlot* NameSlot = Line->AddChildToHorizontalBox(Names);
		NameSlot->SetSize(ESlateSizeRule::Fill);
		NameSlot->SetVerticalAlignment(VAlign_Center);

		UButton* Play = Crt::Button(WidgetTree, TEXT("[ PLAY ]"), 17);
		Play->OnClicked.AddDynamic(this, &UScenesWidget::OnPlayClicked);
		ButtonScenes.Add(Play, Row.Name);
		UHorizontalBoxSlot* PlaySlot = Line->AddChildToHorizontalBox(Play);
		PlaySlot->SetHorizontalAlignment(HAlign_Right);
		PlaySlot->SetVerticalAlignment(VAlign_Center);

		Column->AddChildToVerticalBox(Line)->SetPadding(FMargin(0, 5));
	}

	UButton* Back = Crt::Button(WidgetTree, TEXT("[ BACK ]"), 17);
	Back->OnClicked.AddDynamic(this, &UScenesWidget::OnBack);
	UVerticalBoxSlot* BackSlot = Column->AddChildToVerticalBox(Back);
	BackSlot->SetHorizontalAlignment(HAlign_Center);
	BackSlot->SetPadding(FMargin(0, 24, 0, 0));
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

void UScenesWidget::OnBack() { OnClose.ExecuteIfBound(); }
