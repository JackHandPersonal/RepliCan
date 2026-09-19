// Save / load of the running game (see USyntySaveGame for what a save
// holds). A UGameInstanceSubsystem so it outlives level loads and is
// reachable from anywhere (controller, menus, Python):
//   GetGameInstance()->GetSubsystem<USaveGameSubsystem>()->SaveGame("Quick")
// Loading applies onto the CURRENT level: placed actors are found by name,
// runtime-spawned characters are respawned, the player is moved back.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SaveGameSubsystem.generated.h"

class USyntySaveGame;

UCLASS()
class REPLICAN_API USaveGameSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Save Game") bool SaveGame(const FString& Slot);
	UFUNCTION(BlueprintCallable, Category = "Save Game") bool LoadGame(const FString& Slot);
	UFUNCTION(BlueprintPure, Category = "Save Game") bool SlotExists(const FString& Slot) const;
	// Slot names on disk, newest first.
	UFUNCTION(BlueprintPure, Category = "Save Game") TArray<FString> ListSlots() const;
	UFUNCTION(BlueprintPure, Category = "Save Game") FString GetLastSlot() const { return LastSlot; }
	// The level (long package name) a slot was saved in; empty if unknown.
	UFUNCTION(BlueprintPure, Category = "Save Game") FString GetSlotLevel(const FString& Slot) const;

	// Load Game from the title screen: the slot is remembered here, the
	// level is opened, and the gameplay controller applies it on arrival
	// (ABasePlayerController::BeginPlay -> ConsumePendingLoad).
	UFUNCTION(BlueprintCallable, Category = "Save Game") void SetPendingLoad(const FString& Slot) { PendingLoadSlot = Slot; }
	UFUNCTION(BlueprintCallable, Category = "Save Game") bool ConsumePendingLoad(FString& OutSlot);
	// New Game from the title: the gameplay level plays its intro on arrival.
	UFUNCTION(BlueprintCallable, Category = "Save Game") void SetPendingNewGame(bool bPending) { bPendingNewGame = bPending; }
	UFUNCTION(BlueprintCallable, Category = "Save Game") bool ConsumePendingNewGame() { const bool b = bPendingNewGame; bPendingNewGame = false; return b; }

private:
	FString PendingLoadSlot;
	bool bPendingNewGame = false;
	USyntySaveGame* Capture(UWorld* World) const;
	bool Apply(UWorld* World, const USyntySaveGame* Save) const;
	void WriteJsonMirror(const FString& Slot, const USyntySaveGame* Save) const;
	FString LastSlot;
};
