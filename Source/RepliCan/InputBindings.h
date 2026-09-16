// Every keyboard/mouse command the game handles itself, as data.
//
// The commands that are not Enhanced Input actions -- fire, aim, reload, draw, the panels, the
// debug tools -- used to be a run of `Key == EKeys::T` tests inside the Slate pre-processor.
// That made them unremappable by construction: the key WAS the code. Here each one is an entry
// in a table with an id, a label, a category and a default, the pre-processor asks
// InputBindings::Matches, and a rebind is a map entry saved to the user's config. No restart,
// because nothing was baked into an asset in the first place.
//
// Enhanced Input still owns movement, look, jump, crouch and sprint; those are remapped through
// UEnhancedInputUserSettings instead and appear in the same panel (see SettingsWidget).
#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

namespace InputBindings
{
	struct FAction
	{
		FName Id;
		const TCHAR* Label;
		const TCHAR* Category;
		FKey Default;
		// Modifier the default wants held, or EKeys::Invalid for none. Kept next to the key
		// because "Alt+T" is one command to a player, not a key plus a rule somewhere else.
		FKey DefaultModifier;
		const TCHAR* Hint;
	};

	// The table, in the order the settings panel should show it.
	REPLICAN_API const TArray<FAction>& All();
	REPLICAN_API const FAction* Find(FName Id);

	// The key currently bound: the player's override when they have one, the default otherwise.
	REPLICAN_API FKey KeyFor(FName Id);
	REPLICAN_API FKey ModifierFor(FName Id);

	// True when this key press is that command, modifiers included.
	REPLICAN_API bool Matches(FName Id, const FKey& Key, bool bShift, bool bCtrl, bool bAlt);

	// Rebind and persist. An empty/invalid key clears the override back to the default.
	REPLICAN_API void SetKey(FName Id, const FKey& Key, const FKey& Modifier);
	REPLICAN_API void ResetToDefault(FName Id);
	REPLICAN_API void ResetAll();

	// What to print on a row: "Alt + T", "Right Mouse", "Unbound".
	REPLICAN_API FString Describe(FName Id);

	// Another command already using this combination, or NAME_None. The panel warns rather
	// than refusing, because two commands that can never be active at once may safely share.
	REPLICAN_API FName ConflictWith(FName Id, const FKey& Key, const FKey& Modifier);
}
