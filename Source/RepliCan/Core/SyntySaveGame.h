// The save-game record. Standard Unreal pattern: a USaveGame subclass
// serialised by UGameplayStatics::SaveGameToSlot into
// <Project>/Saved/SaveGames/<Slot>.sav (binary, versioned by the engine's
// save header + our own SaveVersion), with a readable <Slot>.json mirror
// written beside it. Character configs go in as the same JSON text the
// Characters/*.json authoring files use, so a save carries every edit made
// to a character; the world part records where everyone stands, what was
// taken, and the conversation flags. See USaveGameSubsystem.
#pragma once

#include "CoreMinimal.h"
#include "Items/ItemInstance.h"
#include "GameFramework/SaveGame.h"
#include "SyntySaveGame.generated.h"

USTRUCT(BlueprintType)
struct FSavedCharacter
{
	GENERATED_BODY()
	UPROPERTY() FString ActorName;      // level actor FName (stable for placed actors)
	UPROPERTY() FString Label;
	UPROPERTY() bool bRuntimeSpawned = false;   // placed by the Character Manager at runtime
	UPROPERTY() FTransform Transform;
	UPROPERTY() FString ConfigJson;     // FCharacterConfig as JSON
	UPROPERTY() bool bWasPlayer = false;
};

USTRUCT(BlueprintType)
struct FSavedProp
{
	GENERATED_BODY()
	UPROPERTY() FString ActorName;
	UPROPERTY() bool bHidden = false;   // taken
	UPROPERTY() TArray<FString> Tags;
};

UCLASS()
class REPLICAN_API USyntySaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY() int32 SaveVersion = 1;
	UPROPERTY() FString LevelName;
	UPROPERTY() FDateTime SavedAt;
	UPROPERTY() FTransform PlayerTransform;
	UPROPERTY() FRotator ControlRotation;
	UPROPERTY() FString PlayerActorName;
	UPROPERTY() TArray<FString> Inventory;
	// The copies those handles refer to. An old save has none, and every plain name in its inventory
	// still resolves through the catalogue, so it loads unchanged.
	UPROPERTY() TArray<FItemInstance> ItemInstances;
	UPROPERTY() int32 NextItemInstanceId = 1;
	UPROPERTY() TArray<FString> ConversationFlags;
	UPROPERTY() TArray<FString> ConversationChoicesTaken;
	UPROPERTY() TArray<FSavedCharacter> Characters;
	UPROPERTY() TArray<FSavedProp> Props;
};
