// The texture variants a weapon can wear. Synty ships each pack's atlas in lettered variants
// (M_PolygonSciFiSpace_01_A and its Alternates 02_A..F, MI_PolygonCyberCity_01_A/B/C, ...): the
// same mesh, a different paint. A weapon's catalogue "skin" names the variant it wears; the
// character sheet's SKIN button cycles it, the Reference edits it as text. It is a property of
// the catalogue entry, so every copy of that weapon wears the same paint.
#pragma once
#include "CoreMinimal.h"
#include "WeaponCatalog.h"
class UStaticMeshComponent;
namespace WeaponSkins
{
	// The variant material names (asset names, sorted) that the weapon's own materials belong
	// to. Fewer than two means there is nothing to choose.
	REPLICAN_API TArray<FString> Variants(const WeaponCatalog::FWeapon& W);
	// The name the weapon wears now: its "skin" when that is one of the variants, else the
	// mesh's own material.
	REPLICAN_API FString Current(const WeaponCatalog::FWeapon& W);
	// Puts the chosen variant on every matching slot of the component (the mesh's own materials
	// back when nothing is chosen). Slots of other materials (glass, lights) are left alone.
	REPLICAN_API void Apply(UStaticMeshComponent* Comp, const WeaponCatalog::FWeapon& W);
}
