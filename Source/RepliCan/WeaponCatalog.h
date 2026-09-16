// UI/Weapons.json read once for the game to use, rather than only for the Reference screen to
// list. An inventory item whose name matches an entry here IS that weapon: it has a mesh to put
// in a hand, a family whose sound it inherits, a muzzle to fire from, a kind the equipment slots
// understand, and a STANCE that names how the body holds it.
//
// The file stays the single source of truth so a weapon can be added, renamed or re-described
// without touching code. Tools/derive_weapon_data.py fills in the muzzle and ranged fields from
// the meshes themselves; Tools/normalise_weapons.py stamps the mesh space.
//
// Nothing here converts anything. Where a weapon sits in a hand is baked into the mesh and the
// skeleton's WeaponGrip_R socket, and which animation plays is composed from the stance name.
// See Docs/HeldAssetStandard.md -- if you find yourself wanting a switch in this file, the
// answer belongs in the JSON or in an asset.
#pragma once

#include "CoreMinimal.h"

class UStaticMesh;
class UAnimSequence;

namespace WeaponCatalog
{
	struct FWeapon
	{
		FString Name;
		FString Kind;          // Pistol, Rifle, Sword... what the equipment slots match on
		// The rendered icon's name -- /Game/RepliCan/Icons/T_Icon_<Icon>. The inventory grid
		// derives its texture name from the ITEM'S name otherwise, and "Frontier Assault Rifle
		// 01" is not "Worlds_Wep_Assault_01", which is why no weapon ever showed one.
		FString Icon;
		FString Sound;         // the family's report, in RawAudio
		FString Pack;          // which Synty set it came from
		FString Stance;        // Rifle, Pistol, Shotgun, Blade -- an animation folder, not a clip
		FString MeshPath;
		FString BodyMeshPath;  // the mesh with its own scope stripped, used when an optic is fitted (Tools/strip_scopes.py)
		FString Space;         // "hac1" once Tools/normalise_weapons.py has baked the mesh
		FVector Muzzle = FVector::ZeroVector;   // where a shot leaves it, in the mesh's own space
		// "grip": where the trigger hand closes, in the mesh's own space. HAC1 baked the meshes
		// with the grip at the origin, so this is usually zero; a grip moved on the Reference page
		// is honoured by holding the mesh offset so THIS point sits in the hand (nothing else moves).
		FVector Grip = FVector::ZeroVector;
		// Where the eye goes: the rear sight, in the mesh's own space. Derived from the geometry
		// by Tools/derive_sights.py rather than authored -- in HAC1 space the sight line is the
		// top of the receiver on the centreline, which is a measurement, not an opinion.
		FVector Sight = FVector::ZeroVector;
		bool bHasSight = false;
		// Degrees to pitch the weapon up so its sight line is the line of the shot. Zero when
		// the weapon has no usable sights and is aimed down its bore instead.
		float SightPitch = 0.0f;
		// A bullpup puts its magazine BEHIND the trigger grip, which inverts the shape rule the
		// grip finder relies on. Declared per weapon; see Tools/reseat_grips.py.
		bool bBullpup = false;
		// A fitted optic, by name into the catalogue's "optics" block, and where it clamps on
		// this weapon. When one is fitted the weapon's Sight is ALREADY the lens centre -- see
		// Tools/fit_optics.py -- so the aiming code needs to know nothing about optics at all.
		FString Optic;
		FVector OpticMount = FVector::ZeroVector;
		// Where the support hand wraps the handguard, in the weapon's own space. Derived by
		// Tools/derive_weapon_points.py. The trigger hand needs no field: HAC1 puts the origin
		// there by definition.
		FVector ForeGrip = FVector::ZeroVector;
		bool bHasForeGrip = false;
		// The handguard's slope under the support hand, degrees, HAC1 pitch (nose up positive).
		// Derived by Tools/derive_weapon_points.py from the run the fore grip sits on, so a
		// hand on a drooping handguard droops with it instead of gripping it at one global angle.
		float ForeGripPitch = 0.0f;
		// Carried from the hip rather than the shoulder when not aiming. Reserved for a class of
		// special weapons that does not exist yet; "hip_fire": true in the catalogue opts in.
		bool bHipFire = false;
		bool bRanged = false;
		// "fire_modes": the selector's positions, in order: "semi", "auto" (a burst can come later).
		// Empty means semi only. "fire_rate" is rounds per second, what auto cycles at.
		TArray<FString> FireModes;
		float FireRate = 0.0f;
		float Recoil = -1.0f;   // "recoil": degrees of kick per shot; absent = the character's default, 0 = none
	};

	// Loads on first use and caches. Returns null when the name is not a weapon, which is the
	// normal answer for most inventory items.
	// An optic that can be fitted to a weapon: its mesh, and where the eye looks through it.
	struct FOptic
	{
		FString MeshPath;
		FVector Eye = FVector::ZeroVector;
		FString Name;
	};
	REPLICAN_API const FOptic* FindOptic(const FString& OpticName);
	// Every optic the catalogue knows, by key, sorted: what the Reference page cycles through.
	REPLICAN_API TArray<FString> OpticNames();

	REPLICAN_API const FWeapon* Find(const FString& ItemName);
	// Drops the cache so an edited Weapons.json is picked up without restarting.
	REPLICAN_API void Reload();
	REPLICAN_API int32 Num();

	// ---- Stances ------------------------------------------------------------------------
	// The clip vocabulary lives in Docs/HeldAssetStandard.md 3.2: Idle_Hipfire, Idle_ADS,
	// Fire, DryFire, Reload, Equip, Melee, Spawn, IdleBreak_Scan.

	// Which bones the stance's upper-body layer is rooted at -- spine_01 for a two-handed
	// weapon that turns the whole torso, clavicle_r for something held in one hand. Declared
	// per stance in Weapons.json so a new stance needs no code.
	REPLICAN_API const TArray<FName>& StanceRoots(const FString& Stance);
	REPLICAN_API bool StanceIsTwoHanded(const FString& Stance);

	// Composes /Game/Characters/Animations/Lyra/<Stance>/<MM|MF>_<Stance>_<Clip> and follows
	// the stance's declared fallback chain until something loads. Returns null when no stance
	// in the chain ships that clip, which is a normal answer -- Shotgun has no Equip.
	REPLICAN_API UAnimSequence* StanceClip(const FString& Stance, const TCHAR* Clip, bool bFeminine);
}
