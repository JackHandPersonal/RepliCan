#include "TerminalWidget.h"
#include "BasePlayerController.h"
#include "CrtStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Framework/Application/SlateApplication.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

static const int32 TermFontSize = 18;   // on a 1024-wide screen texture: about 85 columns
static const FLinearColor TermGlass(0.008f, 0.026f, 0.012f, 0.97f);

void UTerminalLinkBinding::OnClicked() { if (Term) { Term->Run(Command); } }

void UTerminalWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	UBorder* Root = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Glass"));
	// Rounded, so the quad can be sized to cover a chamfered glass and its corners stay clear.
	FSlateBrush Glass;
	Glass.DrawAs = ESlateBrushDrawType::RoundedBox;
	Glass.OutlineSettings.CornerRadii = FVector4(90.0f, 90.0f, 90.0f, 90.0f);
	Glass.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	Glass.TintColor = FSlateColor(TermGlass);
	Root->SetBrush(Glass);
	Root->SetBrushColor(TermGlass);
	Root->SetPadding(FMargin(40.0f, 22.0f));
	WidgetTree->RootWidget = Root;
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	Root->SetContent(Column);
	Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("Scroll"));
	Scroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	UVerticalBoxSlot* SS = Column->AddChildToVerticalBox(Scroll);
	SS->SetSize(ESlateSizeRule::Fill);
	Lines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Lines"));
	Scroll->AddChild(Lines);
	// The prompt: a marker and a box in the same face.
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
	Column->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));
	Footer = Crt::FixedText(WidgetTree, TEXT("ESC leaves the terminal   TAB completes   ENTER runs   HELP lists commands"), 11, Crt::DimGreen);
	Column->AddChildToVerticalBox(Footer)->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));
}

void UTerminalWidget::LoadEntry(const FString& TerminalId)
{
	Banner.Reset(); Files.Reset();
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *FPaths::Combine(FPaths::ProjectDir(), TEXT("UI"), TEXT("Terminals.json")))) { Banner.Add(TEXT("NO TERMINAL TABLE (UI/Terminals.json)")); return; }
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) { Banner.Add(TEXT("TERMINAL TABLE UNREADABLE")); return; }
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

void UTerminalWidget::FocusPromptFor(int32 SlateUserIndex)
{
	if (!Prompt) { return; }
	FSlateApplication::Get().SetUserFocus(SlateUserIndex, Prompt->TakeWidget(), EFocusCause::SetDirectly);
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
