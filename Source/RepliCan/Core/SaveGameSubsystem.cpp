#include "Core/SaveGameSubsystem.h"
#include "Core/SyntySaveGame.h"
#include "Characters/BaseCharacter.h"
#include "Core/BasePlayerController.h"
#include "Characters/CharacterConfig.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "HAL/FileManager.h"
#include "JsonObjectConverter.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	FString SaveDir() { return FPaths::ProjectSavedDir() / TEXT("SaveGames"); }
}

bool USaveGameSubsystem::SlotExists(const FString& Slot) const
{
	return UGameplayStatics::DoesSaveGameExist(Slot, 0);
}

TArray<FString> USaveGameSubsystem::ListSlots() const
{
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(SaveDir() / TEXT("*.sav")), true, false);
	Files.Sort([](const FString& A, const FString& B)
	{
		return IFileManager::Get().GetTimeStamp(*(SaveDir() / A)) > IFileManager::Get().GetTimeStamp(*(SaveDir() / B));
	});
	for (FString& F : Files) { F = FPaths::GetBaseFilename(F); }
	return Files;
}

FString USaveGameSubsystem::GetSlotLevel(const FString& Slot) const
{
	const USyntySaveGame* Save = Cast<USyntySaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
	return Save ? Save->LevelName : FString();
}

bool USaveGameSubsystem::ConsumePendingLoad(FString& OutSlot)
{
	if (PendingLoadSlot.IsEmpty()) { return false; }
	OutSlot = PendingLoadSlot;
	PendingLoadSlot.Reset();
	return true;
}

USyntySaveGame* USaveGameSubsystem::Capture(UWorld* World) const
{
	USyntySaveGame* Save = Cast<USyntySaveGame>(UGameplayStatics::CreateSaveGameObject(USyntySaveGame::StaticClass()));
	if (!Save || !World) { return nullptr; }
	// Long package name without any PIE prefix, so it can be reopened.
	Save->LevelName = UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
	Save->SavedAt = FDateTime::Now();

	ABasePlayerController* PC = Cast<ABasePlayerController>(UGameplayStatics::GetPlayerController(World, 0));
	APawn* Player = PC ? PC->GetPawn() : nullptr;
	if (PC)
	{
		Save->ControlRotation = PC->GetControlRotation();
		Save->Inventory = PC->Inventory;
		Save->ItemInstances = PC->ItemInstances;
		Save->NextItemInstanceId = PC->NextItemInstanceId;
		Save->ConversationFlags = PC->ConversationFlags.Array();
		Save->ConversationChoicesTaken = PC->ConversationChoicesTaken.Array();
	}
	if (Player)
	{
		Save->PlayerTransform = Player->GetActorTransform();
		Save->PlayerActorName = Player->GetFName().ToString();
	}

	for (TActorIterator<ABaseCharacter> It(World); It; ++It)
	{
		ABaseCharacter* C = *It;
		FSavedCharacter Rec;
		Rec.ActorName = C->GetFName().ToString();
		Rec.Label = C->GetActorLabel();
		Rec.bRuntimeSpawned = C->bRuntimeSpawned;
		Rec.Transform = C->GetActorTransform();
		Rec.bWasPlayer = (C == Player);
		FJsonObjectConverter::UStructToJsonObjectString(C->GetLiveCharacterConfig(), Rec.ConfigJson);
		Save->Characters.Add(MoveTemp(Rec));
	}

	// Props: anything inspectable/taken (tags carry the inspect metadata).
	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		AStaticMeshActor* A = *It;
		if (A->Tags.Num() == 0 && !A->IsHidden()) { continue; }
		FSavedProp Rec;
		Rec.ActorName = A->GetFName().ToString();
		Rec.bHidden = A->IsHidden();
		for (const FName& T : A->Tags) { Rec.Tags.Add(T.ToString()); }
		Save->Props.Add(MoveTemp(Rec));
	}
	return Save;
}

bool USaveGameSubsystem::SaveGame(const FString& Slot)
{
	UWorld* World = GetWorld();
	USyntySaveGame* Save = Capture(World);
	if (!Save) { return false; }
	const bool bOk = UGameplayStatics::SaveGameToSlot(Save, Slot, 0);
	if (bOk)
	{
		LastSlot = Slot;
		WriteJsonMirror(Slot, Save);
		UE_LOG(LogTemp, Log, TEXT("SaveGame: wrote slot '%s' (%d characters, %d props)"), *Slot, Save->Characters.Num(), Save->Props.Num());
	}
	return bOk;
}

bool USaveGameSubsystem::LoadGame(const FString& Slot)
{
	UWorld* World = GetWorld();
	USyntySaveGame* Save = Cast<USyntySaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
	if (!Save || !World) { return false; }
	const FString Here = UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
	if (Save->LevelName != Here)
	{
		UE_LOG(LogTemp, Warning, TEXT("SaveGame: slot '%s' is for level '%s', current is '%s' -- applying anyway"), *Slot, *Save->LevelName, *Here);
	}
	const bool bOk = Apply(World, Save);
	if (bOk) { LastSlot = Slot; }
	return bOk;
}

bool USaveGameSubsystem::Apply(UWorld* World, const USyntySaveGame* Save) const
{
	ABasePlayerController* PC = Cast<ABasePlayerController>(UGameplayStatics::GetPlayerController(World, 0));
	if (PC)
	{
		PC->EndConversation();
		PC->Inventory = Save->Inventory;
		PC->ItemInstances = Save->ItemInstances;
		// Never let a fresh id collide with one already in the file.
		PC->NextItemInstanceId = FMath::Max(Save->NextItemInstanceId, 1);
		for (const FItemInstance& I : PC->ItemInstances) { PC->NextItemInstanceId = FMath::Max(PC->NextItemInstanceId, I.Id + 1); }
		PC->ConversationFlags = TSet<FString>(Save->ConversationFlags);
		PC->ConversationChoicesTaken = TSet<FString>(Save->ConversationChoicesTaken);
	}

	// Characters: match placed actors by name; respawn the runtime ones.
	TMap<FString, ABaseCharacter*> ByName;
	for (TActorIterator<ABaseCharacter> It(World); It; ++It) { ByName.Add(It->GetFName().ToString(), *It); }
	TSet<FString> Seen;
	ABaseCharacter* PlayerCharacter = nullptr;
	for (const FSavedCharacter& Rec : Save->Characters)
	{
		ABaseCharacter* C = ByName.FindRef(Rec.ActorName);
		if (!C && Rec.bRuntimeSpawned)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			C = World->SpawnActor<ABaseCharacter>(ABaseCharacter::StaticClass(), Rec.Transform, Params);
			if (C) { C->bRuntimeSpawned = true; C->SetActorLabel(Rec.Label); }
		}
		if (!C) { UE_LOG(LogTemp, Warning, TEXT("SaveGame: character '%s' not in level, skipped"), *Rec.Label); continue; }
		Seen.Add(Rec.ActorName);
		C->SetActorTransform(Rec.Transform, false, nullptr, ETeleportType::TeleportPhysics);
		FCharacterConfig Config;
		if (!Rec.ConfigJson.IsEmpty() && FJsonObjectConverter::JsonObjectStringToUStruct(Rec.ConfigJson, &Config))
		{
			C->ApplyCharacterConfig(Config);
			C->MarkConfigSaved();
		}
		if (Rec.bWasPlayer) { PlayerCharacter = C; }
	}
	// Runtime-spawned characters that aren't in the save go away.
	for (const TPair<FString, ABaseCharacter*>& Pair : ByName)
	{
		if (Pair.Value->bRuntimeSpawned && !Seen.Contains(Pair.Key)) { Pair.Value->Destroy(); }
	}

	for (const FSavedProp& Rec : Save->Props)
	{
		for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
		{
			if (It->GetFName().ToString() != Rec.ActorName) { continue; }
			It->SetActorHiddenInGame(Rec.bHidden);
			It->SetActorEnableCollision(!Rec.bHidden);
			It->Tags.Reset();
			for (const FString& T : Rec.Tags) { It->Tags.Add(FName(*T)); }
			break;
		}
	}

	if (PC)
	{
		if (PlayerCharacter && PC->GetPawn() != PlayerCharacter) { PC->ControlCharacter(PlayerCharacter); }
		if (APawn* P = PC->GetPawn())
		{
			P->SetActorTransform(Save->PlayerTransform, false, nullptr, ETeleportType::TeleportPhysics);
		}
		PC->SetControlRotation(Save->ControlRotation);
	}
	UE_LOG(LogTemp, Log, TEXT("SaveGame: applied (%d characters, %d props)"), Save->Characters.Num(), Save->Props.Num());
	return true;
}

void USaveGameSubsystem::WriteJsonMirror(const FString& Slot, const USyntySaveGame* Save) const
{
	// A readable copy next to the .sav (not what gets loaded).
	TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
	if (FJsonObjectConverter::UStructToJsonObject(USyntySaveGame::StaticClass(), Save, Obj, 0, 0))
	{
		FString Json;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
		FJsonSerializer::Serialize(Obj, Writer);
		FFileHelper::SaveStringToFile(Json, *(SaveDir() / (Slot + TEXT(".json"))));
	}
}
