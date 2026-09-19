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
	// THE SELECTOR'S POSITIONS. Semi: one shot per pull. Burst: the pull and two more at the fire
	// rate. Auto: another shot every 1/fire_rate seconds while the trigger is held. Laser: a
	// continuous beam while the trigger is held, its damage done over time rather than per shot.
	// In the catalogue these are their lower-case names: "fire_modes": ["semi", "auto"].
	enum class EFireMode : uint8 { Semi, Burst, Auto, Laser };
	// WHAT A WEAPON FEEDS ON, one kind each: "ammo_kind": "medium". None for a blade or a prop.
	enum class EAmmoKind : uint8 { None, Light, Medium, Heavy, Shell, Cell, Rocket };
	REPLICAN_API const TCHAR* FireModeName(EFireMode Mode);
	REPLICAN_API bool ParseFireMode(const FString& Text, EFireMode& Out);
	REPLICAN_API const TArray<FString>& FireModeNames();   // every name in enum order; what the Reference offers
	REPLICAN_API const TCHAR* AmmoKindName(EAmmoKind Kind);
	REPLICAN_API bool ParseAmmoKind(const FString& Text, EAmmoKind& Out);
	REPLICAN_API const TArray<FString>& AmmoKindNames();

	struct FWeapon
	{
		FString Key;           // the catalogue key, "Space/Wep_Pistol_01": what a write-back addresses
		FString Make, Model;   // "make" / "model": what it is called; Name underneath stays the identity
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
		FString Skin;          // "skin": the lettered material variant it wears (WeaponSkins); empty = the mesh's own
		// "moa": the weapon's own dispersion in minutes of angle (the group it shoots, not the shooter): 5 for a combat rifle.
		float MechanicalMoa = 5.0f;
		FString Space;         // "hac1" once Tools/normalise_weapons.py has baked the mesh
		FVector Muzzle = FVector::ZeroVector;   // where a shot leaves it, in the mesh's own space
		// "grip": where the trigger hand closes, in the mesh's own space. HAC1 baked the meshes
		// with the grip at the origin, so this is usually zero; a grip moved on the Reference page
		// is honoured by holding the mesh offset so THIS point sits in the hand (nothing else moves).
		FVector Grip = FVector::ZeroVector;
		// "shoulder": where the stock meets the shoulder for recoil control; in space behind a weapon with no stock.
		FVector Shoulder = FVector::ZeroVector;
		// "attachments": accessory mounts (a light, a laser), HAC1 space; none, one or many.
		TArray<FVector> Attachments;
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
		// "fire_modes": the selector's positions, in order. Empty means semi only. "fire_rate" is
		// rounds per second: what auto and a burst cycle at.
		TArray<EFireMode> FireModes;
		EAmmoKind Ammo = EAmmoKind::None;   // "ammo_kind"
		float FireRate = 0.0f;
		float Recoil = -1.0f;   // "recoil": degrees of kick per shot; absent = the character's default, 0 = none
		float MassKg = 3.0f;    // "mass_kg": what the arms are holding up; the aim sway scales with it
		float Damage = 25.0f;   // "damage": vitality points per shot (per second for a beam), before armour
		// MELEE (bRanged false). "hands" 1 or 2; "attack_set" light | blade | heavy, the clip family the
		// swings come from; "blunt": no cuts, a shove and a heavier stagger instead. The edge that
		// hits runs from the grip (the origin) to the tip ("muzzle").
		int32 Hands = 1;
		FString AttackSet = TEXT("blade");
		bool bBlunt = false;
	};

	// Loads on first use and caches. Returns null when the name is not a weapon, which is the
	// normal answer for most inventory items.
	// An optic that can be fitted to a weapon: its mesh, and where the eye looks through it.
	struct FOptic
	{
		FString MeshPath;
		FVector Eye = FVector::ZeroVector;
		// "mount": the point of the optic that sits on the weapon's optic_mount; its origin unless set.
		FVector Mount = FVector::ZeroVector;
		FString Make, Model;
		FString Name;
	};
	REPLICAN_API const FOptic* FindOptic(const FString& OpticName);
	// Every optic the catalogue knows, by key, sorted: what the Reference page cycles through.
	REPLICAN_API TArray<FString> OpticNames();

	REPLICAN_API const FWeapon* Find(const FString& ItemName);
	// "Make Model" when the weapon has them, else its name; an item that is not a weapon returns its name.
	REPLICAN_API FString DisplayName(const FString& ItemName);
	REPLICAN_API FString OpticDisplayName(const FString& OpticKey);
	// Drops the cache so an edited Weapons.json is picked up without restarting.
	REPLICAN_API void Reload();
	// Writes one string field of one weapon back to UI/Weapons.json and re-reads the file.
	REPLICAN_API bool WriteStringField(const FString& Key, const TCHAR* Field, const FString& Value);
	REPLICAN_API int32 Num();

	// ---- Stances ------------------------------------------------------------------------
	// The clip vocabulary lives in Docs/HeldAssetStandard.md 3.2: Idle_Hipfire, Idle_ADS,
	// Fire, DryFire, Reload, Equip, Melee, Spawn, IdleBreak_Scan.

	// Which bones the stance's upper-body layer is rooted at -- spine_01 for a two-handed
	// weapon that turns the whole torso, clavicle_r for something held in one hand. Declared
	// per stance in Weapons.json so a new stance needs no code.
	REPLICAN_API const TArray<FName>& StanceRoots(const FString& Stance);
	REPLICAN_API bool StanceIsTwoHanded(const FString& Stance);
	// "elbow_down" 0..1: how far the hand IK pulls the elbows down and out from the pose's own bend (a pistol's arms out in front).
	REPLICAN_API float StanceElbowDown(const FString& Stance);
	// "grip_nudge": where the trigger hand closes, moved for every weapon on the stance (a pistol's hand sits high under the slide).
	REPLICAN_API FVector StanceGripNudge(const FString& Stance);
	// A stance's own hold offset for a posture ("shouldered", "hip", "low_ready", "low_ready_first_person"), when it has one.
	REPLICAN_API bool StanceCarry(const FString& Stance, const TCHAR* Which, FVector& Out);

	// Composes /Game/Characters/Animations/Lyra/<Stance>/<MM|MF>_<Stance>_<Clip> and follows
	// the stance's declared fallback chain until something loads. Returns null when no stance
	// in the chain ships that clip, which is a normal answer -- Shotgun has no Equip.
	REPLICAN_API UAnimSequence* StanceClip(const FString& Stance, const TCHAR* Clip, bool bFeminine);
}
