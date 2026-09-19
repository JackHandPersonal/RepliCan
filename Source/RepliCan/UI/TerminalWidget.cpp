#include "UI/TerminalWidget.h"
#include "Core/JsonDataFile.h"
#include "Core/BasePlayerController.h"
#include "UI/CrtStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Framework/Application/SlateApplication.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

static const int32 TermFontSize = 26;   // on a 1280-wide screen texture, inside the margins: about 70 columns
static const FLinearColor TermGlass(0.008f, 0.026f, 0.012f, 0.97f);

void UTerminalLinkBinding::OnClicked() { if (Term) { Term->Run(Command); Term->OnNeedFocus.ExecuteIfBound(); } }

void UTerminalWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	// The glass it is drawn on has the screen's own outline (the controller cuts that mesh from the
	// monitor), so the page is a plain box. Everything sits at the TOP and grows downward, the
	// prompt right under the last line: nothing along the bottom edge, where the chamfers are.
	UBorder* Root = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Glass"));
	Root->SetBrush(FSlateColorBrush(FLinearColor::White));
	Root->SetBrushColor(TermGlass);
	Root->SetPadding(FMargin(72.0f, 60.0f, 72.0f, 40.0f));   // margins clear of the bezel's corners
	WidgetTree->RootWidget = Root;
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	Root->SetContent(Column);
	Hints = Crt::FixedText(WidgetTree, TEXT("ESC leaves   TAB completes   ENTER runs   HELP lists commands"), 18, Crt::DimGreen);
	Column->AddChildToVerticalBox(Hints)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 14.0f));
	Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("Scroll"));
	Scroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	UVerticalBoxSlot* SS = Column->AddChildToVerticalBox(Scroll);
	SS->SetSize(ESlateSizeRule::Fill);
	Lines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Lines"));
	Scroll->AddChild(Lines);
	// The prompt: a marker and a box in the same face, inside the scrolling part right after the
	// last line, so it is always the next line and scrolls up with the rest.
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("PromptRow"));
	Row->AddChildToHorizontalBox(Crt::FixedText(WidgetTree, TEXT("> "), TermFontSize, Crt::Green))->SetVerticalAlignment(VAlign_Center);
	Prompt = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("Prompt"));
	Prompt->WidgetStyle.SetFont(Crt::Fixed(TermFontSize));
	Prompt->WidgetStyle.SetForegroundColor(Crt::Green);
	Prompt->WidgetStyle.SetFocusedForegroundColor(Crt::Green);
	Prompt->WidgetStyle.SetBackgroundImageNormal(FSlateColorBrush(FLinearColor::Transparent));
	Prompt->WidgetStyle.SetBackgroundImageHovered(FSlateColorBrush(FLinearColor::Transparent));
	Prompt->WidgetStyle.SetBackgroundImageFocused(FSlateColorBrush(FLinearColor::Transparent));
	Prompt->WidgetStyle.SetPadding(FMargin(0.0f, 2.0f));
	Prompt->WidgetStyle.TextStyle.SetSelectedBackgroundColor(FSlateColor(Crt::DimGreen));
	Prompt->WidgetStyle.TextStyle.SetHighlightColor(Crt::Green);
	Prompt->SetClearKeyboardFocusOnCommit(false);
	Prompt->OnTextCommitted.AddDynamic(this, &UTerminalWidget::OnPromptCommitted);
	UHorizontalBoxSlot* PS = Row->AddChildToHorizontalBox(Prompt);
	PS->SetSize(ESlateSizeRule::Fill); PS->SetVerticalAlignment(VAlign_Center);
	if (UScrollBoxSlot* RS = Cast<UScrollBoxSlot>(Scroll->AddChild(Row))) { RS->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f)); }
}

void UTerminalWidget::LoadEntry(const FString& TerminalId)
{
	Banner.Reset(); Files.Reset();
	TSharedPtr<FJsonObject> Root = JsonData::Load(TEXT("UI"), TEXT("Terminals.json"), TEXT("Terminals"));
	if (!Root.IsValid()) { Banner.Add(TEXT("NO TERMINAL TABLE (UI/Terminals.json)")); return; }
	// The entry by id, else "default"; a "files" map and a "banner" list on each.
	const TSharedPtr<FJsonObject>* Entry = nullptr;
	if (!Root->TryGetObjectField(TerminalId, Entry) || !Entry) { if (!Root->TryGetObjectField(TEXT("default"), Entry) || !Entry) { return; } }
	const TArray<TSharedPtr<FJsonValue>>* BannerLines = nullptr;
	if ((*Entry)->TryGetArrayField(TEXT("banner"), BannerLines) && BannerLines) { for (const TSharedPtr<FJsonValue>& V : *BannerLines) { Banner.Add(V->AsString()); } }
	const TSharedPtr<FJsonObject>* FilesObj = nullptr;
	if ((*Entry)->TryGetObjectField(TEXT("files"), FilesObj) && FilesObj) { for (const auto& Pair : (*FilesObj)->Values) { Files.Add(FString(Pair.Key.ToView()), Pair.Value->AsString()); } }
}

void UTerminalWidget::Open(ABasePlayerController* InController, const FString& TerminalId)
{
	Controller = InController; Id = TerminalId;
	LoadEntry(TerminalId);
	if (Lines) { Lines->ClearChildren(); } Bindings.Reset(); LineCount = 0;
	for (const FString& L : Banner) { Print(L, Crt::Green); }
	Print(TEXT(""), Crt::Green);
	if (Prompt) { Prompt->SetText(FText::GetEmpty()); }
}

void UTerminalWidget::FocusPrompt() { if (Prompt) { Prompt->SetKeyboardFocus(); } }

bool UTerminalWidget::FocusPromptFor(int32 SlateUserIndex)
{
	if (!Prompt || !FSlateApplication::IsInitialized()) { return false; }
	// Slate looks through its virtual windows too (a world widget's window is one), so this
	// reaches the prompt once the screen has been laid out; before that it says no.
	return FSlateApplication::Get().SetUserFocus(SlateUserIndex, Prompt->TakeWidget(), EFocusCause::SetDirectly);
}

bool UTerminalWidget::PromptCentrePx(FVector2D& Out) const
{
	if (!Prompt) { return false; }
	const FGeometry& G = Prompt->GetCachedGeometry();
	if (G.GetLocalSize().X < 1.0f || G.GetLocalSize().Y < 1.0f) { return false; }   // not laid out yet
	Out = FVector2D(G.GetAbsolutePositionAtCoordinates(FVector2f(0.5f, 0.5f)));
	return true;
}

void UTerminalWidget::Print(const FString& Text, const FLinearColor& Colour)
{
	if (!Lines) { return; }
	UTextBlock* T = Crt::FixedText(WidgetTree, Text, TermFontSize, Colour);
	T->SetAutoWrapText(true);
	Lines->AddChildToVerticalBox(T)->SetPadding(FMargin(0.0f, 1.0f));
	if (++LineCount > 400 && Lines->GetChildrenCount() > 0) { Lines->RemoveChildAt(0); }
	if (Scroll) { Scroll->ScrollToEnd(); }
}

void UTerminalWidget::PrintLink(const FString& Label, const FString& Command)
{
	if (!Lines) { return; }
	UButton* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	B->SetStyle(Crt::ButtonStyle());
	UTextBlock* T = Crt::FixedText(WidgetTree, TEXT("  ") + Label, TermFontSize, Crt::Green);
	B->AddChild(T);
	if (UButtonSlot* BS = Cast<UButtonSlot>(T->Slot)) { BS->SetPadding(FMargin(0.0f, 1.0f)); BS->SetHorizontalAlignment(HAlign_Left); }
	UTerminalLinkBinding* Binding = NewObject<UTerminalLinkBinding>(this);
	Binding->Term = this; Binding->Command = Command;
	B->OnClicked.AddDynamic(Binding, &UTerminalLinkBinding::OnClicked);
	Bindings.Add(Binding);
	UVerticalBoxSlot* S = Lines->AddChildToVerticalBox(B);
	S->SetHorizontalAlignment(HAlign_Left);
	++LineCount;
	if (Scroll) { Scroll->ScrollToEnd(); }
}

void UTerminalWidget::OnPromptCommitted(const FText& Text, ETextCommit::Type Method)
{
	if (Method != ETextCommit::OnEnter) { return; }
	const FString Line = Text.ToString().TrimStartAndEnd();
	if (Prompt) { Prompt->SetText(FText::GetEmpty()); }
	Run(Line);
	FocusPrompt();
}

void UTerminalWidget::Run(const FString& Line)
{
	if (Line.IsEmpty()) { Print(TEXT(">"), Crt::DimGreen); return; }
	Print(TEXT("> ") + Line, Crt::DimGreen);
	History.Add(Line); HistoryAt = History.Num();
	FString Cmd, Arg;
	if (!Line.Split(TEXT(" "), &Cmd, &Arg)) { Cmd = Line; }
	Cmd = Cmd.ToLower(); Arg = Arg.TrimStartAndEnd();
	if (Cmd == TEXT("help") || Cmd == TEXT("?"))
	{
		Print(TEXT("commands:  ls   cat <file>   clear   whoami   time   echo <text>   exit"), Crt::Green);
		Print(TEXT("           file names in a listing can be clicked"), Crt::DimGreen);
	}
	else if (Cmd == TEXT("ls") || Cmd == TEXT("dir") || Cmd == TEXT("files"))
	{
		if (Files.Num() == 0) { Print(TEXT("(no files)"), Crt::DimGreen); }
		TArray<FString> Names; Files.GetKeys(Names); Names.Sort();
		for (const FString& N : Names) { PrintLink(N, TEXT("cat ") + N); }
	}
	else if (Cmd == TEXT("cat") || Cmd == TEXT("read") || Cmd == TEXT("open") || Cmd == TEXT("type"))
	{
		if (const FString* Body = Files.Find(Arg))
		{
			TArray<FString> Rows; Body->ParseIntoArray(Rows, TEXT("\n"), false);
			if (Rows.Num() == 0) { Body->ParseIntoArrayLines(Rows, false); }
			Print(TEXT("---- ") + Arg + TEXT(" ----"), Crt::DimGreen);
			for (const FString& R : Rows) { Print(R, Crt::Green); }
			Print(TEXT("---- end ----"), Crt::DimGreen);
		}
		else { Print(Arg.IsEmpty() ? TEXT("cat: which file?") : FString::Printf(TEXT("cat: no such file: %s"), *Arg), Crt::DimGreen); }
	}
	else if (Cmd == TEXT("clear") || Cmd == TEXT("cls")) { if (Lines) { Lines->ClearChildren(); } Bindings.Reset(); LineCount = 0; }
	else if (Cmd == TEXT("whoami")) { Print(Controller ? Controller->GetPlayerDisplayName().ToUpper() : TEXT("UNKNOWN"), Crt::Green); }
	else if (Cmd == TEXT("time") || Cmd == TEXT("date")) { Print(FString::Printf(TEXT("station clock  %s"), *FDateTime::Now().ToString(TEXT("%H:%M:%S"))), Crt::Green); }
	else if (Cmd == TEXT("echo")) { Print(Arg, Crt::Green); }
	else if (Cmd == TEXT("exit") || Cmd == TEXT("quit") || Cmd == TEXT("logout") || Cmd == TEXT("bye")) { OnExit.ExecuteIfBound(); }
	else { Print(FString::Printf(TEXT("%s: not a command here (HELP lists them)"), *Cmd), Crt::DimGreen); }
}

void UTerminalWidget::Complete()
{
	if (!Prompt) { return; }
	const FString Line = Prompt->GetText().ToString();
	FString Head, Tail;
	const bool bTwo = Line.Split(TEXT(" "), &Head, &Tail);
	TArray<FString> Pool;
	if (bTwo) { Files.GetKeys(Pool); }
	else { Pool = { TEXT("help"), TEXT("ls"), TEXT("cat"), TEXT("clear"), TEXT("whoami"), TEXT("time"), TEXT("echo"), TEXT("exit") }; }
	const FString Partial = bTwo ? Tail : Line;
	TArray<FString> Hits;
	for (const FString& P : Pool) { if (P.StartsWith(Partial, ESearchCase::IgnoreCase)) { Hits.Add(P); } }
	if (Hits.Num() == 1) { Prompt->SetText(FText::FromString(bTwo ? Head + TEXT(" ") + Hits[0] : Hits[0] + TEXT(" "))); }
	else if (Hits.Num() > 1) { Hits.Sort(); Print(FString::Join(Hits, TEXT("   ")), Crt::DimGreen); }
}

FReply UTerminalWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape) { OnExit.ExecuteIfBound(); return FReply::Handled(); }
	if (InKeyEvent.GetKey() == EKeys::Tab) { Complete(); return FReply::Handled(); }
	if (InKeyEvent.GetKey() == EKeys::Up && History.Num() > 0 && Prompt) { HistoryAt = FMath::Max(0, HistoryAt - 1); Prompt->SetText(FText::FromString(History[HistoryAt])); return FReply::Handled(); }
	if (InKeyEvent.GetKey() == EKeys::Down && Prompt) { HistoryAt = FMath::Min(History.Num(), HistoryAt + 1); Prompt->SetText(FText::FromString(HistoryAt < History.Num() ? History[HistoryAt] : FString())); return FReply::Handled(); }
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}
