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
		// Set aside in the Reference and kept out of the lists that are for choosing from. A third of
		// the catalogue is hidden -- shurikens, swords, shields, duplicates -- and stepping through
		// them one at a time to reach the weapon you wanted is most of the list wasted.
		bool bHidden = false;
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
		// HOW BIG THIS ONE IS DRAWN. Measured, the meshes are NOT upscaled -- ours match the pack's
		// own copies exactly (median ratio 1.000) and the character is 179 cm, the same height as
		// Synty's own, so a 105 cm heavy gun is simply a big gun drawn at its true size. What is
		// left is a judgement about how a particular weapon reads in the hand, and that is what this
		// is for: a multiplier per weapon, tuned by eye on the hand page.
		float Scale = 1.0f;
		FString Skin;          // "skin": the lettered material variant it wears (WeaponSkins); empty = the mesh's own
		// "moa": the weapon's own dispersion in minutes of angle (the group it shoots, not the shooter): 5 for a combat rifle.
		float MechanicalMoa = 5.0f;
		FString Space;         // "hac1" once Tools/normalise_weapons.py has baked the mesh
		FVector Muzzle = FVector::ZeroVector;   // where a shot leaves it, in the mesh's own space
		// "eject": where a spent casing leaves it (HAC1: right of the receiver, above the grip); zero = a default port.
		FVector Eject = FVector::ZeroVector;
		// "grip": where the main hand closes, in the mesh's own space. HAC1 baked the meshes
		// with the grip at the origin, so this is usually zero; a grip moved on the Reference page
		// is honoured by holding the mesh offset so THIS point sits in the hand (nothing else moves).
		FVector Grip = FVector::ZeroVector;
		// "hand_rot": pitch, yaw, roll (degrees) -- this weapon's own turn of the main hand on
		// its grip, added to the character's skeleton-wide correction (ABaseCharacter::
		// TriggerHandRotation). The per-weapon socket every shipped game has, as numbers: the
		// console's HandRotWeapon tunes it in play and saves it here. See Docs/HandAnchoring.md.
		FRotator HandRot = FRotator::ZeroRotator;
		// "fore_hand_rot": the same for the support hand on the fore grip (HandRotLWeapon tunes and saves it).
		FRotator ForeHandRot = FRotator::ZeroRotator;
		// "fingers_r" / "fingers_l": thumb, index, middle, ring, pinky -- degrees each phalanx closes on
		// top of the clip's hand (+ closes, - opens), trigger hand and support hand. The TUNE HANDS page sets them.
		TArray<float> FingersR, FingersL;
		// "hunch": centimetres of shrug AT THE SIGHTS -- shoulders up, head down between them.
		float Hunch = 0.0f;
		// "lean": degrees the torso leans forward at the waist AT THE SIGHTS.
		float LeanDeg = 0.0f;
		// "pull" and "lateral": how this weapon is held in each carry -- low ready, shouldered,
		// sights -- as [low, shouldered, ads]. PULL is centimetres along the aim (- brings the
		// weapon in towards the shoulder); LATERAL is centimetres off to the main-hand side, which at
		// the sights is normally zero because the optic has to be in the eye line. A single number
		// in the file is taken to mean all three. The defaults are the character's own carries.
		float PullCm[3] = { 0.0f, 0.0f, 0.0f };
		float LateralCm[3] = { 12.0f, 11.0f, 0.0f };
		// "low_ready": degrees the weapon is turned off the aim while at low ready -- muzzle down
		// and across. At low ready nothing has to stay on the eye line, so it need not sit parallel
		// to it; a weapon held ready points at the deck, off to the side.
		float LowReadyPitch = -30.0f, LowReadyYaw = -30.0f;
		// "elbow_main" / "elbow_support": degrees each elbow is swung about its own shoulder-to-hand
		// line, ONE PER CARRY [low ready, shouldered, sights] -- an arm folded in at the sights wants
		// a different elbow from the same arm hanging at low ready. Negative takes the elbow back
		// behind the ribs instead of flaring it outward. ("elbow", a pair, still reads as main and
		// support for every carry.)
		float ElbowMain[3] = { 0.0f, 0.0f, 0.0f };
		float ElbowSupport[3] = { 0.0f, 0.0f, 0.0f };
		// AND THE SAME ELBOW ACROSS THE AIM. The three above are one per carry; these are one per
		// AIM PITCH -- high, middle, low, at the tuning page's own three preview angles -- and they
		// are added to the carry's value. An arm that is right at the shoulder is wrong pointed at
		// the ceiling, and no single number fixes both. Zero everywhere means "as it was".
		float ElbowMainAim[3] = { 0.0f, 0.0f, 0.0f };
		float ElbowSupportAim[3] = { 0.0f, 0.0f, 0.0f };
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
		// SOME SIGHTS ARE PART OF THE GUN. A weapon with "optic_fixed": true has its sight built in
		// and it cannot be swapped. This is a fact about the WEAPON, not about the mount: the
		// BugBuster's red dot IS its own geometry -- the tube is body mesh and only the glass is a
		// separate piece -- so there is nothing to unclip and nothing a different sight could clamp
		// onto. Anything that offers the player a CHOICE of optic must ask this first and refuse when
		// it is set. Anything merely READING the fitted optic carries on unchanged: a fixed sight is
		// still an optic in every other respect, with a kind, a zoom, a reticle and an eye point.
		bool bOpticFixed = false;
		// Where the support hand wraps the handguard, in the weapon's own space. Derived by
		// Tools/derive_weapon_points.py. The main hand needs no field: HAC1 puts the origin
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
		float Magazine = 0.0f;  // "magazine": what a load holds; for a laser, the seconds of beam a cell gives
		// MELEE (bRanged false). "hands" 1 or 2; "attack_set" light | blade | heavy, the clip family the
		// swings come from; "blunt": no cuts, a shove and a heavier stagger instead. The edge that
		// hits runs from the grip (the origin) to the tip ("muzzle").
		int32 Hands = 1;
		FString AttackSet = TEXT("blade");
		bool bBlunt = false;
	};

	// Loads on first use and caches. Returns null when the name is not a weapon, which is the
	// normal answer for most inventory items.
	// WHAT KIND OF SIGHT THIS IS. The three are not points on one scale -- they are three different
	// answers to "what does the player see when the weapon comes up", and everything else (the mask,
	// hiding the tube from its owner, the field of view, mouse gain) follows from which one it is.
	//
	//   RedDot   No magnification, no overlay: a mark on glass that you look PAST. The gun stays in
	//            view and the world is unchanged. Honest at any eye position, because the reticle is
	//            drawn on the point of impact in screen space rather than sitting on the model.
	//   Zoomed   Magnified, but still looked past rather than through: no mask, no black surround,
	//            the weapon stays visible, the view simply narrows. The right answer at low power
	//            (1.5x-3x), where a tube would occlude more than the magnification is worth, and it
	//            keeps the peripheral vision a scope gives up.
	//   Scope    The full sight picture: the tube stops being drawn to its own shooter, a mask with
	//            a circular opening covers the view, and the world narrows by the magnification. The
	//            opening IS the picture. Needed once the tube is long enough that looking "through"
	//            it would show a pipe -- the eye sits ~14 cm behind ~30 cm of solid barrel.
	enum class EOpticKind : uint8 { RedDot, Zoomed, Scope };

	// An optic that can be fitted to a weapon: its mesh, and where the eye looks through it.
	struct FOptic
	{
		FString MeshPath;
		FVector Eye = FVector::ZeroVector;
		// "mount": the point of the optic that sits on the weapon's optic_mount; its origin unless set.
		FVector Mount = FVector::ZeroVector;
		// AND HOW THIS PARTICULAR OPTIC SITS ON THAT RAIL. The weapon owns where the rail is
		// (optic_mount); these two belong to the optic, because they are facts about the model and
		// not about the gun it happens to be bolted to. "rot" matters more than it sounds: measured
		// with Tools/optic_axes, every SM_Wep_Scope_* in the Synty pack is authored along Y -- across
		// the aim -- so all seven mount sideways until they are yawed back onto X.
		FVector Offset = FVector::ZeroVector;
		// MAGNIFICATION, applied only while the sights are up. 1 is a plain reflex sight: no
		// magnification at all, which is what a red dot actually gives you.
		float Zoom = 1.0f;
		// A SMART SIGHT puts figures on the glass -- range to whatever is under the reticle, and
		// what is left in the magazine. A property of the OPTIC, so fitting a better sight is what
		// earns you the readout.
		bool bSmart = false;
		// WHAT THE SIGHT PUTS IN FRONT OF YOUR EYE: "dot", "circle_dot", "crosshair", "chevron".
		// Drawn in SCREEN space by the HUD, not built into the mesh. Geometry inside a scope tube
		// has to be the right size, at the right depth, facing the right way, in front of the glass
		// and behind the near clip plane all at once -- it was wrong on every one of those at least
		// once -- and even when right it is only correct from one eye position. A screen-space mark
		// sits exactly on the point of impact by construction, at every zoom and every posture,
		// which is why every modern shooter draws it there.
		FString Reticle;
		// VARIABLE MAGNIFICATION. A scope with more than one power lists them; the sight steps
		// through them while it is up. One entry (or none) is a fixed scope.
		TArray<float> ZoomLevels;
		// WHICH OF THE THREE THIS IS -- see EOpticKind. Read from the catalogue's "kind"
		// ("red_dot" / "zoomed" / "scope"); an entry without one is classified from what it does
		// have, so older data keeps working: an "overlay" of true is a scope, and otherwise
		// magnification alone decides between zoomed and a red dot.
		EOpticKind Kind = EOpticKind::RedDot;
		// Whether the sight is looked through a TUBE -- i.e. whether the screen gets the scope
		// overlay. True for a scope and nothing else; kept as its own accessor because that is the
		// question the HUD and the owner-hide actually ask.
		bool bOverlay = false;
		FLinearColor ReticleColour = FLinearColor(0.45f, 1.0f, 0.65f, 1.0f);
		FRotator Rot = FRotator::ZeroRotator;
		FString Skin;
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
	// The same, into the OPTICS block rather than the weapons one, for numbers. An optic offset
	// belongs to the optic and not to whatever gun it happens to be bolted to, so it is saved once
	// and every weapon that fits that optic gets it.
	REPLICAN_API bool WriteOpticNumbers(const FString& OpticKey, const TCHAR* Field, const TArray<double>& Values);
	REPLICAN_API bool WriteOpticString(const FString& OpticKey, const TCHAR* Field, const FString& Value);
	REPLICAN_API int32 Num();
	// Every weapon a hand can actually be tuned on -- it has a mesh and a stance -- sorted by the
	// name shown on screen. What the hand-tuning page's arrows walk through.
	REPLICAN_API TArray<FString> TunableNames();

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
	// "grip_nudge": where the main hand closes, moved for every weapon on the stance (a pistol's hand sits high under the slide).
	REPLICAN_API FVector StanceGripNudge(const FString& Stance);
	// A stance's own hold offset for a posture ("shouldered", "hip", "low_ready", "low_ready_first_person"), when it has one.
	REPLICAN_API bool StanceCarry(const FString& Stance, const TCHAR* Which, FVector& Out);

	// A NAMED HAND SHAPE. A trigger hand is held much the same on every firearm -- thumb wrapped,
	// index along the guard, the rest closed -- so the shapes live once in UI/Weapons.json under
	// "finger_presets" (name -> [thumb, index, middle, ring, pinky], degrees per joint) and the
	// hand page applies them by name. Add or edit one in the file and it appears on the page.
	struct FFingerPreset { FString Name; TArray<float> Values; };
	REPLICAN_API const TArray<FFingerPreset>& FingerPresets();

	// Composes /Game/Characters/Animations/Lyra/<Stance>/<MM|MF>_<Stance>_<Clip> and follows
	// the stance's declared fallback chain until something loads. Returns null when no stance
	// in the chain ships that clip, which is a normal answer -- Shotgun has no Equip.
	REPLICAN_API UAnimSequence* StanceClip(const FString& Stance, const TCHAR* Clip, bool bFeminine);
}
