#include "InputBindings.h"
#include "RepliCanUserSettings.h"

namespace
{
	// The table. Adding a command is one row here plus one InputBindings::Matches call at the
	// place that handles it -- never a new EKeys literal buried in a branch.
	const TArray<InputBindings::FAction>& Table()
	{
		static const TArray<InputBindings::FAction> Actions = {
			{ TEXT("Fire"),          TEXT("Fire"),                 TEXT("Combat"),  EKeys::LeftMouseButton,  EKeys::Invalid, TEXT("Pull the trigger on whatever is in hand") },
			{ TEXT("Aim"),           TEXT("Aim down sights"),      TEXT("Combat"),  EKeys::RightMouseButton, EKeys::Invalid, TEXT("Held. Pulls the camera in and tightens the spread") },
			{ TEXT("FireMode"),      TEXT("Fire mode"),            TEXT("Combat"),  EKeys::MiddleMouseButton, EKeys::Invalid, TEXT("Cycle semi / auto on the weapon in hand") },
			{ TEXT("Reload"),        TEXT("Reload"),               TEXT("Combat"),  EKeys::R,                EKeys::Invalid, TEXT("Change the magazine") },
			{ TEXT("NextWeapon"),    TEXT("Next weapon"),          TEXT("Combat"),  EKeys::G,                EKeys::Invalid, TEXT("Also the mouse wheel") },
			{ TEXT("Holster"),       TEXT("Holster weapon"),       TEXT("Combat"),  EKeys::H,                EKeys::Invalid, TEXT("Put it away, or draw it again") },
			{ TEXT("MeleeBash"),     TEXT("Melee"),                TEXT("Combat"),  EKeys::V,                EKeys::Invalid, TEXT("Hit something with the weapon rather than shoot it") },

			{ TEXT("Zoom"),          TEXT("Camera distance"),      TEXT("Camera"),  EKeys::C,                EKeys::Invalid, TEXT("Steps through the zoom levels; the first is first person") },
			{ TEXT("ShoulderSide"),  TEXT("Swap shoulder"),        TEXT("Camera"),  EKeys::C,                EKeys::LeftControl, TEXT("Which side the third-person camera sits on") },
			{ TEXT("Freelook"),      TEXT("Free look"),            TEXT("Camera"),  EKeys::LeftAlt,          EKeys::Invalid, TEXT("Held. Look around without turning; the aim stays where it was") },

			{ TEXT("CharacterSheet"),TEXT("Character sheet"),      TEXT("Screens"), EKeys::Tab,              EKeys::Invalid, TEXT("Inventory, equipment and attributes") },
			{ TEXT("PauseMenu"),     TEXT("Menu"),                 TEXT("Screens"), EKeys::Escape,           EKeys::Invalid, TEXT("Also backs out of whatever is open") },

			{ TEXT("Headlamp"),      TEXT("Headlamp"),             TEXT("World"),   EKeys::L,                EKeys::Invalid, TEXT("") },

			{ TEXT("InspectSurface"),TEXT("Identify surface"),     TEXT("Tools"),   EKeys::T,                EKeys::Invalid, TEXT("Reports the mesh and material under the reticle") },
			{ TEXT("CyclePalette"),  TEXT("Cycle palette variant"),TEXT("Tools"),   EKeys::T,                EKeys::LeftAlt, TEXT("Steps a Synty material through its lettered variants") },
			{ TEXT("ClaudeAssist"),  TEXT("Claude assist"),        TEXT("Tools"),   EKeys::F12,              EKeys::Invalid, TEXT("Marking reticle; every click is sent to Claude") },
			{ TEXT("EditMode"),      TEXT("Edit mode"),            TEXT("Tools"),   EKeys::F7,               EKeys::Invalid, TEXT("Scene tuning without the assist reticle") },
		};
		return Actions;
	}

	FString KeyString(const FKey& Key)
	{
		return Key.IsValid() ? Key.ToString() : FString();
	}

	// One config entry per action, "Key|Modifier", because a TMap<FName, FKey> does not
	// round-trip through an ini and a pair of strings does.
	FString OverrideFor(FName Id)
	{
		const URepliCanUserSettings* S = URepliCanUserSettings::Get();
		if (!S) { return FString(); }
		const FString* Found = S->KeyBindings.Find(Id.ToString());
		return Found ? *Found : FString();
	}

	void SplitOverride(const FString& Raw, FKey& OutKey, FKey& OutMod)
	{
		OutKey = EKeys::Invalid;
		OutMod = EKeys::Invalid;
		if (Raw.IsEmpty()) { return; }
		FString KeyPart, ModPart;
		if (!Raw.Split(TEXT("|"), &KeyPart, &ModPart)) { KeyPart = Raw; }
		if (!KeyPart.IsEmpty()) { OutKey = FKey(*KeyPart); }
		if (!ModPart.IsEmpty()) { OutMod = FKey(*ModPart); }
	}

	// A modifier the player bound as "Ctrl" should accept either Ctrl key; the same for
	// Shift and Alt. Anything else is matched exactly.
	bool ModifierHeld(const FKey& Mod, bool bShift, bool bCtrl, bool bAlt)
	{
		if (!Mod.IsValid()) { return true; }
		if (Mod == EKeys::LeftControl || Mod == EKeys::RightControl) { return bCtrl; }
		if (Mod == EKeys::LeftShift || Mod == EKeys::RightShift) { return bShift; }
		if (Mod == EKeys::LeftAlt || Mod == EKeys::RightAlt) { return bAlt; }
		return true;
	}
}

const TArray<InputBindings::FAction>& InputBindings::All() { return Table(); }

const InputBindings::FAction* InputBindings::Find(FName Id)
{
	return Table().FindByPredicate([Id](const FAction& A) { return A.Id == Id; });
}

FKey InputBindings::KeyFor(FName Id)
{
	FKey Key, Mod;
	SplitOverride(OverrideFor(Id), Key, Mod);
	if (Key.IsValid()) { return Key; }
	const FAction* A = Find(Id);
	return A ? A->Default : EKeys::Invalid;
}

FKey InputBindings::ModifierFor(FName Id)
{
	const FString Raw = OverrideFor(Id);
	if (!Raw.IsEmpty())
	{
		FKey Key, Mod;
		SplitOverride(Raw, Key, Mod);
		// An override is the whole binding, modifier included -- clearing the modifier is a
		// legitimate rebind, so an empty half means "no modifier", not "use the default".
		if (Key.IsValid()) { return Mod; }
	}
	const FAction* A = Find(Id);
	return A ? A->DefaultModifier : EKeys::Invalid;
}

bool InputBindings::Matches(FName Id, const FKey& Key, bool bShift, bool bCtrl, bool bAlt)
{
	const FKey Bound = KeyFor(Id);
	if (!Bound.IsValid() || Bound != Key) { return false; }
	const FKey Mod = ModifierFor(Id);
	if (!ModifierHeld(Mod, bShift, bCtrl, bAlt)) { return false; }
	// A command with NO modifier must not fire while one is held, or Alt+T would trigger both
	// "identify surface" and "cycle palette" -- which is exactly the bug the table replaced.
	if (!Mod.IsValid() && (bShift || bCtrl || bAlt)) { return false; }
	return true;
}

void InputBindings::SetKey(FName Id, const FKey& Key, const FKey& Modifier)
{
	URepliCanUserSettings* S = URepliCanUserSettings::Get();
	if (!S) { return; }
	if (!Key.IsValid()) { S->KeyBindings.Remove(Id.ToString()); }
	else { S->KeyBindings.Add(Id.ToString(), KeyString(Key) + TEXT("|") + KeyString(Modifier)); }
	S->SaveConfig();
}

void InputBindings::ResetToDefault(FName Id)
{
	if (URepliCanUserSettings* S = URepliCanUserSettings::Get())
	{
		S->KeyBindings.Remove(Id.ToString());
		S->SaveConfig();
	}
}

void InputBindings::ResetAll()
{
	if (URepliCanUserSettings* S = URepliCanUserSettings::Get())
	{
		S->KeyBindings.Reset();
		S->SaveConfig();
	}
}

FString InputBindings::Describe(FName Id)
{
	const FKey Key = KeyFor(Id);
	if (!Key.IsValid()) { return TEXT("Unbound"); }
	const FKey Mod = ModifierFor(Id);
	FString Out;
	if (Mod.IsValid())
	{
		if (Mod == EKeys::LeftControl || Mod == EKeys::RightControl) { Out = TEXT("Ctrl + "); }
		else if (Mod == EKeys::LeftShift || Mod == EKeys::RightShift) { Out = TEXT("Shift + "); }
		else if (Mod == EKeys::LeftAlt || Mod == EKeys::RightAlt) { Out = TEXT("Alt + "); }
		else { Out = Mod.GetDisplayName().ToString() + TEXT(" + "); }
	}
	return Out + Key.GetDisplayName().ToString();
}

FName InputBindings::ConflictWith(FName Id, const FKey& Key, const FKey& Modifier)
{
	auto Same = [](const FKey& A, const FKey& B)
	{
		if (A == B) { return true; }
		// Left and right modifiers are the same binding as far as a player is concerned.
		const bool bCtrl = (A == EKeys::LeftControl || A == EKeys::RightControl) && (B == EKeys::LeftControl || B == EKeys::RightControl);
		const bool bShift = (A == EKeys::LeftShift || A == EKeys::RightShift) && (B == EKeys::LeftShift || B == EKeys::RightShift);
		const bool bAlt = (A == EKeys::LeftAlt || A == EKeys::RightAlt) && (B == EKeys::LeftAlt || B == EKeys::RightAlt);
		return bCtrl || bShift || bAlt;
	};
	for (const FAction& A : Table())
	{
		if (A.Id == Id) { continue; }
		if (KeyFor(A.Id) == Key && Same(ModifierFor(A.Id), Modifier)) { return A.Id; }
	}
	return NAME_None;
}
