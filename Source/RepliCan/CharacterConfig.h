// The complete, serializable description of one configured character --
// what the CharacterBuilder panel edits and what gets written to
// <Project>/Characters/<Name>.json (see CharacterConfigFile). Plain data:
// asset references are stored as object paths (strings) so the file stays
// readable/hand-editable and never pins a loaded object.
//
// Also home to the Synty POLYGON Modular Fantasy Hero slot table
// (ModularHero namespace): the pack ships ~1400 skeletal-mesh parts, all
// on one rig, named SK_Chr_<Slot>[_Male|_Female]_<NN>. Everything the
// panel lists is discovered from the Asset Registry by that naming, so
// nothing here has to be updated when the pack does.
#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "GaitAdjustments.h"
#include "CharacterConfig.generated.h"

// One entry of an NPC's gaze schedule: "player" (the camera), "prop" (the
// held prop), "character:<ConfigName>" or "none", with a relative weight.
// Where the "remote communication" camera sits for this character: an
// offset from the head socket in the character's frame (X forward, Y right,
// Z up), what it looks at (offset from the head), and its field of view.
USTRUCT()
struct FRemoteCameraConfig
{
	GENERATED_BODY()
	UPROPERTY() FVector Offset = FVector(58.0f, 10.0f, 14.0f);
	UPROPERTY() FVector LookOffset = FVector(0.0f, 0.0f, 1.0f);
	UPROPERTY() float Fov = 36.0f;
	UPROPERTY() float Roll = 0.0f;
};

USTRUCT()
struct FGazeTarget
{
	GENERATED_BODY()
	UPROPERTY() FString Target;
	UPROPERTY() float Weight = 1.0f;
};

// The face systems (AFaceController: nose mesh, decal mouth, auto-blink,
// eye/brow poses) wired onto the character. Offsets are in Mannequin head-
// bone space; rigs with other head axes are converted automatically (see
// AFaceController::SetHeadFrameConversion).
USTRUCT()
struct REPLICAN_API FCharacterFaceConfig
{
	GENERATED_BODY()

	UPROPERTY() bool bEnabled = true;
	UPROPERTY() FString NoseMesh = TEXT("/Game/Characters/KnightDemo/SM_Nose.SM_Nose");
	UPROPERTY() FVector NoseLocation = FVector(6.89f, 13.0f, 0.0f);
	UPROPERTY() FRotator NoseRotation = FRotator(-90.0f, 0.0f, 0.0f);
	UPROPERTY() FVector NoseScale = FVector(1.0f, 1.0f, 1.0f);
	// Brows: one mesh mirrored to both sides. Location is head-bone space for the
	// right brow (x up, y forward, z lateral); the left is the z mirror. Tilt is
	// degrees about the forward axis, opposite on each side (positive = angry).
	UPROPERTY() FString BrowMesh;
	UPROPERTY() FVector BrowLocation = FVector(12.0f, 12.3f, 4.3f);
	UPROPERTY() float BrowTilt = 0.0f;
	UPROPERTY() FVector BrowScale = FVector(1.0f, 1.0f, 1.0f);
	// Hair color for the hero pieces (Color_Hair on their material); alpha 0 = as painted.
	UPROPERTY() FLinearColor HairColor = FLinearColor(0, 0, 0, 0);
	UPROPERTY() bool bMouthDecal = true;
	UPROPERTY() FVector MouthLocation = FVector(-1.06f, 13.70f, 0.0f);
	UPROPERTY() FRotator MouthRotation = FRotator(0.0f, -90.0f, -90.0f);
	UPROPERTY() float MouthScale = 0.5f;
	UPROPERTY() FLinearColor MouthTint = FLinearColor::White;
	UPROPERTY() FString Expression = TEXT("Neutral");
	UPROPERTY() bool bAutoBlink = true;
	// A Synty SM_Chr_Attach_Hair* mesh snapped onto the SOC_head socket
	// (single-mesh characters only; empty = no hair attachment). It takes the
	// character's palette material, so its color follows the palette.
	UPROPERTY() FString HairMesh;
	// A beard attachment on SOC_head, same rules as HairMesh.
	UPROPERTY() FString FacialHairMesh;
	// A Synty hat/helmet attachment on SOC_head, same rules as HairMesh.
	UPROPERTY() FString HeadGearMesh;
	// Nudge for the headgear from its socket (cm, socket space; +Z lifts it).
	UPROPERTY() FVector HeadGearLocation = FVector::ZeroVector;
	UPROPERTY() float HeadGearScale = 1.0f;
	// A small plate on the headgear (a cap badge): a plane in the headgear
	// mesh's own space carrying this material; empty = none. Size in cm.
	UPROPERTY() FString HeadGearBadgeMaterial;
	UPROPERTY() FVector HeadGearBadgeLocation = FVector::ZeroVector;
	UPROPERTY() FRotator HeadGearBadgeRotation = FRotator::ZeroRotator;
	UPROPERTY() FVector2D HeadGearBadgeSize = FVector2D(5.0f, 4.0f);
};

namespace CharacterType
{
	// Assembled from the Modular Fantasy Hero part slots.
	inline const TCHAR* Modular = TEXT("Modular");
	// One ordinary Synty skeletal mesh (SK_Chr_*) plus a palette material.
	inline const TCHAR* Single = TEXT("Single");
}

// What a body and a mind are good for. Every character and NPC carries these; they are saved
// in the character's own JSON, so an NPC's sheet is a text edit. The working range is 1 to 10
// with 5 as an unremarkable adult: 1 is barely functional, 8 is the best in the room, 10 is
// the best on the station.
USTRUCT()
struct REPLICAN_API FAttributes
{
	GENERATED_BODY()

	// Raw muscle power, heavy lifting, and melee force.
	UPROPERTY() int32 Brawn = 50;
	// Coordination, speed, dodging, and fine motor skills.
	UPROPERTY() int32 Agility = 50;
	// Immune system strength, stamina, and resistance to toxins or radiation.
	UPROPERTY() int32 Endurance = 50;
	// Memory, logical deduction, and academic knowledge.
	UPROPERTY() int32 Cognition = 50;
	// Repairing, hacking, and operating futuristic machinery or starship systems.
	UPROPERTY() int32 Tech = 50;
	// Leadership, intimidation, and social engineering.
	UPROPERTY() int32 Presence = 50;

	// A hundred-point scale for humans: 50 is an ordinary person, 100 the best a body does.
	static constexpr int32 Min = 1;
	static constexpr int32 Max = 100;
	// The six in the order they are shown, so the UI and any roll code agree on it.
	static const TArray<FName>& Names();
	int32 Get(FName Which) const;
	void Set(FName Which, int32 Value);
	// "Cognition" -> "the mind's reach", for the line under a score on a sheet.
	static FString Describe(FName Which);

	// The numbers the six attributes imply: how much damage a body absorbs, how hard it is to
	// hit, how well it shrugs off heat or poison, how hard its implants are to talk into
	// opening. They live here rather than in the sheet widget because a defence is a game
	// quantity that combat will want, not a caption -- the sheet just happens to be the first
	// thing that reads them. Ordered for display; the names are the sheet's {Tokens}.
	static TArray<TPair<FName, int32>> Derived(const FAttributes& A);
};

USTRUCT()
struct REPLICAN_API FCharacterConfig
{
	GENERATED_BODY()

	UPROPERTY() FString Name = TEXT("Hero");

	// Brawn, Agility, Endurance, Cognition, Tech, Presence.
	UPROPERTY() FAttributes Attributes;

	// CharacterType::Modular or CharacterType::Single -- which of the two
	// Synty character kinds this is; decides whether Parts or BaseMesh/
	// Material describe the body.
	UPROPERTY() FString Type = TEXT("Modular");

	// Single only: the skeletal mesh and (optional) palette material
	// instance for slot 0. Empty Material = the mesh's own default.
	UPROPERTY() FString BaseMesh;
	UPROPERTY() FString Material;

	// Modular only. "Male" / "Female" selects which gendered part set the
	// slots draw from (see ModularHero::FSlotDef::bGendered).
	UPROPERTY() FString Gender = TEXT("Male");

	// Which Synty locomotion set the character moves with: "Male", "Female"
	// or "Goblin". Empty = follow Gender (older files).
	UPROPERTY() FString Locomotion;

	// Modular only. Slot name (ModularHero::FSlotDef::Slot) -> skeletal
	// mesh object path. A slot that's absent or empty shows nothing.
	UPROPERTY() TMap<FString, FString> Parts;
	// Which modular kit Parts belongs to: "" / "FantasyHero" (the Modular
	// Fantasy Hero pack) or "SciFi" (the cut library: CutHead, CutTorso,
	// CutArms, CutLegs slots holding /Game/RepliCan/CutLibrary parts from any
	// pack, all on the UE4 Mannequin rig).
	UPROPERTY() FString Kit;
	bool IsSciFiKit() const { return Kit == TEXT("SciFi"); }

	UPROPERTY() FCharacterFaceConfig Face;

	// Material vector parameter name -> color, for the pack's shared
	// character material (Color_Primary, Color_Skin, ...). Parameters not
	// listed keep the material instance's default.
	UPROPERTY() TMap<FString, FLinearColor> Colors;

	UPROPERTY() FString WeaponMesh;
	UPROPERTY() FVector WeaponLocation = FVector(-7.0f, 2.0f, 0.0f);
	UPROPERTY() FRotator WeaponRotation = FRotator(0.0f, -90.0f, 0.0f);
	UPROPERTY() FVector WeaponScale = FVector::OneVector;

	// See UCharacterAnimInstance::ArmOverridePose.
	UPROPERTY() FString ArmPose;
	UPROPERTY() float ArmPoseWeight = 1.0f;

	// Additive grip curl on the weapon hand's finger/thumb bones (see
	// UCharacterAnimInstance::FingerCurlDegrees / ThumbCurlDegrees). The
	// curl axis is each bone's local Z, whose sense differs between rigs,
	// so these are per-character: the modular hero's thumb needs the
	// opposite sign from the Mannequin-rig characters.
	// Runtime posture/timing adjustments over the locomotion clips (see
	// GaitAdjustments.h) -- all "no change" by default.
	UPROPERTY() FGaitAdjustments Gait;

	UPROPERTY() float FingerCurlDegrees = -60.0f;
	UPROPERTY() float ThumbCurlDegrees = 40.0f;

	// Applied to the mesh (not the capsule), about the feet, so a scaled
	// character still stands on the ground. Locomotion play rates divide by
	// the scale along the direction of travel so stride length keeps
	// matching distance covered (see FLocomotionInputs::StrideScale).
	UPROPERTY() FVector Scale = FVector::OneVector;

	// Multiplies every ground-speed tier (Walk/Jog/Run/crouch) for this
	// character specifically; the tiers themselves stay the shared base.
	UPROPERTY() float SpeedMultiplier = 1.0f;

	// Freeform string labels for gameplay to key off (e.g. "npc", "merchant",
	// "hostile"). Mirrored onto the actor's own Tags array so anything that
	// queries AActor::ActorHasTag sees them too. Edited as a comma-separated
	// list in the Character Manager.
	UPROPERTY() TArray<FString> Tags;

	// Inspect context menu: a line under the name, and the stub "Talk"
	// remark the character says (Talk is offered only when non-empty).
	UPROPERTY() FString Description;
	// What the world calls them when it differs from the file name (the player: Name stays "Player", the file).
	UPROPERTY() FString DisplayName;
	UPROPERTY() FString Comment;
	// Ambient behaviour: AnimSequence paths played one after another at
	// random (with short gaps) whenever the character is otherwise idle.
	UPROPERTY() TArray<FString> IdleAnimations;
	// Where the eyes go while idle: a weighted target is picked every
	// GazeHoldMin..Max seconds and tracked. Empty = glance at the player
	// on alternate idle clips (the old behaviour).
	UPROPERTY() TArray<FGazeTarget> GazeTargets;
	UPROPERTY() float GazeHoldMin = 1.8f;
	UPROPERTY() float GazeHoldMax = 4.5f;
	// Head turn speed between gaze targets (0 = the anim instance default).
	UPROPERTY() float GazeTurnSpeed = 0.0f;
	UPROPERTY() FRemoteCameraConfig RemoteCamera;
	// Skin recolor for atlas characters: alpha 0 = off; otherwise the skin
	// cells are re-toned to this color (their shading kept), and the stubble
	// cells are faded toward the skin by StubbleFade (0 = as painted, 1 = gone).
	UPROPERTY() FLinearColor SkinTint = FLinearColor(0, 0, 0, 0);
	UPROPERTY() float StubbleFade = 0.0f;
	// The scalp shadow on a hero head (its second material section), same scale.
	UPROPERTY() float HeadStubbleFade = 0.0f;
};

namespace CharacterConfigFile
{
	// <Project>/Characters -- checked in alongside the project, not Saved/.
	REPLICAN_API FString GetDirectory();
	REPLICAN_API FString GetPath(const FString& Name);
	REPLICAN_API bool Save(const FCharacterConfig& Config);
	REPLICAN_API bool Load(const FString& Name, FCharacterConfig& OutConfig);
	// Names (file stems) of every saved config, sorted.
	REPLICAN_API TArray<FString> List();
}

namespace ModularHero
{
	// Base rig every part follows (leader pose) and the material instance
	// all parts share. The skeleton has UE4_Mannequin_Skeleton registered as
	// a compatible skeleton (done once, saved in the asset), which is what
	// lets this project's Mannequin-retargeted animations play on it.
	REPLICAN_API const TCHAR* BaseMeshPath();
	REPLICAN_API const TCHAR* MaterialInstancePath();

	struct FSlotDef
	{
		FString Slot;
		// Asset-name prefix; "{G}" is replaced with the gender for gendered
		// slots. Match is on the prefix followed by digits (so "Head_{G}_"
		// doesn't also claim "Head_No_Elements_{G}_").
		TArray<FString> Prefixes;
		bool bGendered = false;
		// Attach to a socket instead of following the leader pose -- the
		// pack's BackAttachments folder has its own skeletons.
		FName AttachSocket;
	};

	REPLICAN_API const TArray<FSlotDef>& Slots();
	REPLICAN_API const FSlotDef* FindSlot(const FString& SlotName);

	// Every part asset that belongs to Slot for Gender, sorted by name.
	REPLICAN_API TArray<FAssetData> PartOptions(const FSlotDef& Slot, const FString& Gender);

	// The 12 color parameters on the shared material, in display order.
	REPLICAN_API const TArray<FString>& ColorParameters();

	// A sensible starting character: a plainly clothed body (the pack's
	// "_00" parts are the bare body), hair 01, no attachments, material
	// defaults. Mouth decal off -- these heads have painted mouths.
	REPLICAN_API FCharacterConfig MakeDefaultConfig(const FString& Name);
}

// The appearance chooser's palette: what the player picks their basic
// look from. Bodies are the bare cryo pair; heads are every human head in
// the cut library; hair and beards are the packs' head attachments; skin
// tones are presets for FCharacterConfig::SkinTint.
namespace Appearance
{
	struct FOption { FString Label; FString Path; };
	struct FBody { FString Label; FString Torso; FString Arms; FString Legs; FString Head; FVector Brow; };   // Brow: right-brow rest position in head-bone space
	struct FBrow { FString Label; FString Mesh; float Tilt; float Raise; float Spread; FVector Scale; };
	struct FSkin { FString Label; FLinearColor Tint; };
	struct FNose { FString Label; FString Mesh; FVector Scale; };
	REPLICAN_API const TArray<FBody>& Bodies();
	REPLICAN_API const TArray<FOption>& Heads();
	// Heads Synty labels Female / Male; unlabelled ones appear in both lists.
	REPLICAN_API const TArray<FOption>& HeadsFor(bool bFemale);
	// The three SM_Nose meshes at a few scales; the last entry is no nose.
	REPLICAN_API const TArray<FNose>& Noses();
	// Per-sex lists: pieces Synty or the hero pack label Female/Male are filtered, unlabelled ones appear in both;
	// labels are plain numbers so the chooser can show "3 / 34". Beards: none for female.
	REPLICAN_API const TArray<FOption>& HairFor(bool bFemale);
	REPLICAN_API const TArray<FOption>& BeardsFor(bool bFemale);
	REPLICAN_API const TArray<FBrow>& BrowsFor(bool bFemale);
	// Brow presets on the generated brow bars; the last entry is no brows.
	REPLICAN_API const TArray<FBrow>& Brows();
	struct FHairColor { FString Label; FLinearColor Color; };
	REPLICAN_API const TArray<FHairColor>& HairColors();
	// Static pieces baked from the Modular Fantasy Hero, in head-bone space.
	REPLICAN_API TArray<FOption> HeroPieces(const TCHAR* Prefix);
	REPLICAN_API const TArray<FOption>& Hair();
	REPLICAN_API const TArray<FOption>& Beards();
	REPLICAN_API const TArray<FSkin>& Skins();
}

// The cut library: Synty sci-fi characters split by bone into Head / Torso /
// Arms / Legs (Tools/cut_library.py), assembled by leader pose on a hidden
// Mannequin-rigged base so any pack's legs go with any pack's torso.
namespace CutLibrary
{
	REPLICAN_API const TArray<FString>& Slots();          // CutHead, CutTorso, CutArms, CutLegs
	REPLICAN_API FString PartSuffix(const FString& Slot);  // "_Head" ...
	REPLICAN_API const TCHAR* BaseMeshPath();              // the hidden leader rig
	// Every cut part for Slot across all packs, sorted by name.
	REPLICAN_API TArray<FAssetData> PartOptions(const FString& Slot);
}

// Ordinary (single-mesh) Synty characters and the assets a Single config
// can pick from.
namespace SyntyCharacters
{
	// Every SK_Chr_* skeletal mesh in the project that isn't a modular part
	// or an attachment, sorted by name.
	REPLICAN_API TArray<FAssetData> BaseMeshOptions();

	// Material instances sitting next to the mesh's own slot-0 material --
	// Synty ships each pack's recolors as sibling MI_<Pack>_XX_Y assets.
	REPLICAN_API TArray<FAssetData> PaletteOptions(const class USkeletalMesh* Mesh);

	// SM_Nose* static meshes, for the face's nose picker.
	REPLICAN_API TArray<FAssetData> NoseOptions();
	// Hair attachments authored for the UE4-Mannequin Synty heads (SOC_head).
	REPLICAN_API TArray<FAssetData> HairOptions();
	// Hats, helmets, caps, masks for the same heads.
	REPLICAN_API TArray<FAssetData> HeadGearOptions();

	// The stock single-mesh character when nothing better is known.
	REPLICAN_API const TCHAR* DefaultBaseMeshPath();

	// Empty BaseMeshPath = DefaultBaseMeshPath().
	REPLICAN_API FCharacterConfig MakeDefaultConfig(const FString& Name, const FString& BaseMeshPath, const FString& MaterialPath);
}
