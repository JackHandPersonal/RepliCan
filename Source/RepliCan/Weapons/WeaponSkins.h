// The texture variants a weapon can wear. Synty ships each pack's atlas in lettered variants
// (M_PolygonSciFiSpace_01_A and its Alternates 02_A..F, MI_PolygonCyberCity_01_A/B/C, ...): the
// same mesh, a different paint. A weapon's catalogue "skin" names the variant it wears; the
// character sheet's SKIN button cycles it, the Reference edits it as text. It is a property of
// the catalogue entry, so every copy of that weapon wears the same paint.
#pragma once
#include "CoreMinimal.h"
#include "Weapons/WeaponCatalog.h"
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
	// The same, wearing a named variant instead of the catalogue's. This is what lets a paint be
	// TRIED on one figure without every copy of that weapon in the world changing colour: the
	// tuning page cycles through these on its stand-in and only writes one to the catalogue when
	// SET DEFAULT is pressed. An empty name means the catalogue's own choice.
	REPLICAN_API void ApplyNamed(UStaticMeshComponent* Comp, const WeaponCatalog::FWeapon& W, const FString& Variant);

	// THE SAME THREE, FOR ANY MESH. A weapon is not the only thing with a paint on it: an optic is
	// its own model with its own atlas material, and it is bolted to guns of every colour. These
	// take the mesh directly so the sight can be painted without pretending to be a weapon.
	// (ApplyNamed already ignored its FWeapon entirely -- it reads the component own mesh -- so
	// ApplyVariant is that function under an honest name.)
	REPLICAN_API TArray<FString> VariantsOfMesh(class UStaticMesh* Mesh);
	REPLICAN_API FString CurrentOfMesh(class UStaticMesh* Mesh, const FString& Chosen);
	REPLICAN_API void ApplyVariant(UStaticMeshComponent* Comp, const FString& Variant);
}
