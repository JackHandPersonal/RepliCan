#include "Core/JsonDataFile.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FString JsonData::DataDir()
{
	return FPaths::Combine(FPaths::ProjectContentDir(), TEXT("GameData"));
}

FString JsonData::Path(const TCHAR* SubDir, const TCHAR* FileName)
{
	return FPaths::Combine(DataDir(), SubDir, FileName);
}

TSharedPtr<FJsonObject> JsonData::LoadObject(const FString& FullPath, const TCHAR* Who)
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *FullPath))
	{
		// Naming the path matters more than it looks: the failure a reader will actually meet is a
		// catalogue that came back empty, which is indistinguishable from an empty catalogue unless
		// this line says which file could not be read.
		UE_LOG(LogTemp, Warning, TEXT("%s: could not read %s"), Who, *FullPath);
		return nullptr;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: %s is not valid JSON"), Who, *FullPath);
		return nullptr;
	}
	return Root;
}

TSharedPtr<FJsonObject> JsonData::Load(const TCHAR* SubDir, const TCHAR* FileName, const TCHAR* Who)
{
	return LoadObject(Path(SubDir, FileName), Who);
}

bool JsonData::ForEachEntry(const TSharedPtr<FJsonObject>& Root, const TCHAR* Field,
	TFunctionRef<void(const FString&, const TSharedPtr<FJsonObject>&)> Fn)
{
	if (!Root.IsValid()) { return false; }

	const TSharedPtr<FJsonObject>* Section = nullptr;
	if (!Root->TryGetObjectField(Field, Section) || !Section || !Section->IsValid()) { return false; }

	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Section)->Values)
	{
		if (IsDocKey(Pair.Key)) { continue; }
		const TSharedPtr<FJsonObject>* Entry = nullptr;
		if (!Pair.Value.IsValid() || !Pair.Value->TryGetObject(Entry) || !Entry) { continue; }
		Fn(Pair.Key, *Entry);
	}
	return true;
}

FDateTime JsonData::TimeStamp(const FString& FullPath)
{
	return IFileManager::Get().GetTimeStamp(*FullPath);
}
