#include "ConversationData.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

FString ConversationFile::GetDirectory()
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Conversations")));
}

FString ConversationFile::GetPath(const FString& CharacterName)
{
	return FPaths::Combine(GetDirectory(), CharacterName + TEXT(".json"));
}

bool ConversationFile::Exists(const FString& CharacterName)
{
	return !CharacterName.IsEmpty() && IFileManager::Get().FileExists(*GetPath(CharacterName));
}

FString ConversationFile::GetVoicePath(const FString& CharacterName, const FString& NodeId, int32 LineIndex)
{
	return FPaths::Combine(GetDirectory(), TEXT("Voice"), CharacterName, FString::Printf(TEXT("%s_%d.wav"), *NodeId, LineIndex));
}

FString ConversationFile::StripVoiceMarkup(const FString& Text)
{
	FString Out;
	Out.Reserve(Text.Len());
	for (int32 i = 0; i < Text.Len(); ++i)
	{
		const TCHAR C = Text[i];
		if (C == TEXT('[') && Text.Mid(i, 6).Equals(TEXT("[pause"), ESearchCase::IgnoreCase))
		{
			const int32 Close = Text.Find(TEXT("]"), ESearchCase::IgnoreCase, ESearchDir::FromStart, i);
			if (Close != INDEX_NONE) { i = Close; continue; }
		}
		if (C == TEXT('*') || C == TEXT('~')) { continue; }
		// Collapse the double spaces a removed [pause] leaves behind.
		if (C == TEXT(' ') && Out.Len() > 0 && Out[Out.Len() - 1] == TEXT(' ')) { continue; }
		Out.AppendChar(C);
	}
	return Out.TrimStartAndEnd();
}

bool ConversationFile::Load(const FString& CharacterName, FConversation& Out)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *GetPath(CharacterName))) { return false; }
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid()) { return false; }

	Out = FConversation();
	Out.Character = CharacterName;
	Out.StartNode = Root->GetStringField(TEXT("start"));
	Root->TryGetBoolField(TEXT("remote"), Out.bRemote);
	Root->TryGetBoolField(TEXT("unskippable"), Out.bUnskippable);
	const TSharedPtr<FJsonObject>* NodesObj = nullptr;
	if (!Root->TryGetObjectField(TEXT("nodes"), NodesObj) || !NodesObj) { return false; }

	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*NodesObj)->Values)
	{
		const TSharedPtr<FJsonObject> NodeObj = Pair.Value->AsObject();
		if (!NodeObj.IsValid()) { continue; }
		FConversationNode Node;
		Node.Id = Pair.Key;
		const TArray<TSharedPtr<FJsonValue>>* LineArray = nullptr;
		if (NodeObj->TryGetArrayField(TEXT("text"), LineArray))
		{
			for (const TSharedPtr<FJsonValue>& V : *LineArray) { Node.Lines.Add(StripVoiceMarkup(V->AsString())); }
		}
		else
		{
			Node.Lines.Add(StripVoiceMarkup(NodeObj->GetStringField(TEXT("text"))));
		}
		NodeObj->TryGetBoolField(TEXT("end"), Node.bEnd);
		const TArray<TSharedPtr<FJsonValue>>* ChoiceArray = nullptr;
		if (NodeObj->TryGetArrayField(TEXT("choices"), ChoiceArray))
		{
			for (const TSharedPtr<FJsonValue>& V : *ChoiceArray)
			{
				const TSharedPtr<FJsonObject> C = V->AsObject();
				if (!C.IsValid()) { continue; }
				FConversationChoice Choice;
				Choice.Text = C->GetStringField(TEXT("text"));
				C->TryGetStringField(TEXT("next"), Choice.Next);
				C->TryGetStringField(TEXT("set"), Choice.SetFlag);
				C->TryGetStringField(TEXT("if"), Choice.RequireFlag);
				C->TryGetBoolField(TEXT("once"), Choice.bOnce);
				C->TryGetBoolField(TEXT("end"), Choice.bEnd);
				Node.Choices.Add(MoveTemp(Choice));
			}
		}
		Out.Nodes.Add(Node.Id, MoveTemp(Node));
	}
	if (Out.StartNode.IsEmpty() && Out.Nodes.Num() > 0)
	{
		TArray<FString> Keys; Out.Nodes.GetKeys(Keys);
		Out.StartNode = Keys[0];
	}
	return Out.IsValid();
}
