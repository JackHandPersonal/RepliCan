// The translucent "about to drop a character here" preview the Character
// Manager shows while placing a new character (see ABaseCharacter::
// BeginPlacingCharacter). Built from a FCharacterConfig the same way the
// real character will be -- modular parts, or one base mesh -- but with no
// collision, no animation (reference pose) and one shared ghost material
// whose color says whether the spot under the cursor is valid.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CharacterGhostActor.generated.h"

struct FCharacterConfig;
class USkeletalMeshComponent;
class UMaterialInstanceDynamic;

UCLASS()
class REPLICAN_API ACharacterGhostActor : public AActor
{
	GENERATED_BODY()

public:

	ACharacterGhostActor();

	// Rebuilds the preview meshes from Config (parts or base mesh).
	void BuildFromConfig(const FCharacterConfig& Config);

	// Green-ish for a valid drop point, red-ish otherwise.
	void SetValid(bool bValid);

private:

	USkeletalMeshComponent* AddPreviewMesh(const FString& AssetPath);

	UPROPERTY()
	TArray<TObjectPtr<USkeletalMeshComponent>> PreviewMeshes;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> GhostMID;

	bool bCurrentValid = true;
};
