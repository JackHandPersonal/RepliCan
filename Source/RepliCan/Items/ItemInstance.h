#pragma once
#include "CoreMinimal.h"
#include "ItemInstance.generated.h"

// ONE PARTICULAR COPY OF AN ITEM, as opposed to the kind of thing it is.
//
// Inventory and Equipped hold strings, and until now that string was simply the catalogue name --
// so "the optic on this rifle" was really "the optic on every Spear 350 in the game", because the
// only place to put it was the catalogue entry that all of them share. Anything that belongs to one
// copy rather than to the type (what is bolted to it, what colour it was painted, how worn it is,
// what is in the magazine) had nowhere to live.
//
// An instance is that missing place. The arrays still hold strings, but an instanced item's string
// is a HANDLE -- "Spear 350#7" -- carrying the catalogue name and an id. Two consequences make this
// cheap rather than sweeping:
//
//   * every catalogue lookup strips the suffix at its own door (WeaponCatalog::Find,
//     ItemCatalog::FindRecord and the two display-name calls), so all 55 existing lookup sites keep
//     working untouched and a handle behaves exactly like a name everywhere that does not care;
//   * items that have nothing worth remembering -- ammo, a ration bar -- keep their bare name and
//     cost nothing. Nothing is forced to be unique just because something else is.
// BlueprintType, because the fields below are BlueprintReadOnly so the registry can be inspected
// from script -- UHT rejects exposed properties on a struct that is not itself a blueprint type.
USTRUCT(BlueprintType)
struct FItemInstance
{
	GENERATED_BODY()

	// BlueprintReadOnly throughout: these have to be readable from outside to be checkable at all.
	UPROPERTY(BlueprintReadOnly, Category = "Item") int32 Id = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Item") FString Name;   // the catalogue name this is a copy OF
	// What belongs to this copy. Free-form on purpose: "optic", "skin", "durability", "ammo" today,
	// and whatever the game needs next without a schema change or a save-format break. A key that is
	// absent means "whatever the catalogue says", which is what keeps old saves working.
	UPROPERTY(BlueprintReadOnly, Category = "Item") TMap<FString, FString> Props;
};

namespace ItemHandle
{
	// The separator is '#' because no catalogue name contains one; a name that somehow did would
	// simply never be treated as a handle, which fails safe.
	inline const TCHAR Sep = TEXT('#');

	inline bool IsInstance(const FString& Handle)
	{
		int32 At = INDEX_NONE;
		return Handle.FindLastChar(Sep, At) && At > 0 && At < Handle.Len() - 1;
	}

	/** "Spear 350#7" -> "Spear 350". A bare name is returned unchanged, so this is safe on anything. */
	inline FString NameOf(const FString& Handle)
	{
		int32 At = INDEX_NONE;
		if (Handle.FindLastChar(Sep, At) && At > 0) { return Handle.Left(At); }
		return Handle;
	}

	/** The id, or 0 for a bare name. */
	inline int32 IdOf(const FString& Handle)
	{
		int32 At = INDEX_NONE;
		if (Handle.FindLastChar(Sep, At) && At > 0 && At < Handle.Len() - 1)
		{
			return FCString::Atoi(*Handle.Mid(At + 1));
		}
		return 0;
	}

	inline FString Make(const FString& Name, int32 Id)
	{
		return (Id > 0) ? FString::Printf(TEXT("%s%c%d"), *Name, Sep, Id) : Name;
	}
}
