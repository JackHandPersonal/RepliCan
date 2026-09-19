// THE FIELDS EVERY ITEM CARRIES, in one table. The Reference page builds its editor from this
// (a row per field, typed), Tools/backfill_item_fields.py fills the same keys with best
// guesses, and gameplay reads them through ItemCatalog. Add a field here and it is editable,
// saved and shown everywhere at once.
//
// Scope: which catalogue entries carry it -- All, Weapons only, Armor only. Measured fields
// are shown but not edited: Tools/survey_items.py regenerates them from the mesh.
#pragma once

#include "CoreMinimal.h"

namespace ItemFields
{
	enum class EType : uint8 { Text, Number, Bool, List, Measured, Points, Enum };   // Enum: one of a fixed set of names, listed in the hint in order   // Points: a list of x, y, z triples, "x, y, z; x, y, z"
	enum class EScope : uint8 { All, Weapons, Armor, Items, Optics, Gear };   // Gear: weapons and optics both   // Items: everything that is not a weapon; Optics: the optics block's parts

	struct FField
	{
		const TCHAR* Key;      // the JSON key
		const TCHAR* Label;    // what the editor shows
		const TCHAR* Group;    // the section it sits under
		EType Type;
		EScope Scope;
		const TCHAR* Hint;     // one line on what the value means
	};

	inline const FField Table[] = {
		// the weapon's points, in HAC1 space (cm from the grip): the ones the Reference viewer draws
		// and lets you drag. Listed first: they are what gets tuned most.
		{ TEXT("grip"),          TEXT("GRIP"),          TEXT("POINTS"),   EType::List,     EScope::Weapons, TEXT("x, y, z: where the trigger hand closes (red); drag it to move the hold") },
		{ TEXT("sight"),         TEXT("SIGHT"),         TEXT("POINTS"),   EType::List,     EScope::Weapons, TEXT("x, y, z: the rear sight the eye lines up (yellow)") },
		{ TEXT("fore_grip"),     TEXT("FORE GRIP"),     TEXT("POINTS"),   EType::List,     EScope::Weapons, TEXT("x, y, z: where the support hand closes (green)") },
		{ TEXT("muzzle"),        TEXT("MUZZLE"),        TEXT("POINTS"),   EType::List,     EScope::Weapons, TEXT("x, y, z: where the shot leaves (white)") },
		{ TEXT("optic_mount"),   TEXT("OPTIC MOUNT"),   TEXT("POINTS"),   EType::List,     EScope::Weapons, TEXT("x, y, z: where a fitted optic bolts on (cyan)") },
		{ TEXT("shoulder"),      TEXT("SHOULDER"),      TEXT("POINTS"),   EType::List,     EScope::Weapons, TEXT("x, y, z: where the stock meets the shoulder (magenta); in space behind a stockless weapon") },
		{ TEXT("attachments"),   TEXT("ATTACHMENTS"),   TEXT("POINTS"),   EType::Points,   EScope::Weapons, TEXT("x, y, z; x, y, z ... accessory mounts (orange): where a light, laser or grip clamps on; [ + ATTACH ] in the legend adds one") },
		// An optic's own points, in ITS space (Tools/bake_optic_part.py sets a part's base on z 0, centred).
		{ TEXT("mount"),         TEXT("MOUNT"),         TEXT("POINTS"),   EType::List,     EScope::Optics,  TEXT("x, y, z: the point of the optic that sits on the weapon's OPTIC MOUNT (cyan); its origin unless moved") },
		{ TEXT("eye"),           TEXT("EYE"),           TEXT("POINTS"),   EType::List,     EScope::Optics,  TEXT("x, y, z: the window centre the aim line runs through (yellow); on the weapon it lands at OPTIC MOUNT + EYE - MOUNT") },
		{ TEXT("source"),        TEXT("SOURCE"),        TEXT("OPTIC"),    EType::Text,     EScope::Optics,  TEXT("the pack part this optic was baked from") },
		{ TEXT("sight_pitch"),   TEXT("SIGHT PITCH"),   TEXT("POINTS"),   EType::Number,   EScope::Weapons, TEXT("degrees the weapon is pitched so the sight line meets the shot") },
		{ TEXT("fore_grip_pitch"), TEXT("FORE GRIP PITCH"), TEXT("POINTS"), EType::Number, EScope::Weapons, TEXT("degrees the support palm is pitched onto the guard") },
		// identity
		// A weapon or an optic is known by MAKE and MODEL (Docs/Arms_Manufacturers.md); "name" stays underneath as the record's identity.
		{ TEXT("make"),          TEXT("MAKE"),          TEXT("IDENTITY"), EType::Text,     EScope::Gear,    TEXT("the manufacturer: see Docs/Arms_Manufacturers.md") },
		{ TEXT("model"),         TEXT("MODEL"),         TEXT("IDENTITY"), EType::Text,     EScope::Gear,    TEXT("the model, as it would read on the receiver") },
		{ TEXT("category"),      TEXT("CATEGORY"),      TEXT("IDENTITY"), EType::Text,     EScope::Items,   TEXT("the Reference tab it lives on: armor equipment consumables other (a weapon is always a weapon)") },
		{ TEXT("kind"),          TEXT("KIND"),          TEXT("IDENTITY"), EType::Text,     EScope::All,     TEXT("the type gameplay matches on: Pistol, Helmet, Medkit, Keycard, Drink") },
		{ TEXT("tags"),          TEXT("TAGS"),          TEXT("IDENTITY"), EType::List,     EScope::All,     TEXT("free labels, comma separated: medical, alien, contraband, quest") },
		// physical
		{ TEXT("size"),          TEXT("SIZE CM"),       TEXT("PHYSICAL"), EType::Measured, EScope::All,     TEXT("x y z, from the mesh") },
		{ TEXT("volume_l"),      TEXT("VOLUME L"),      TEXT("PHYSICAL"), EType::Measured, EScope::All,     TEXT("bounding box, litres") },
		{ TEXT("rest_offset"),   TEXT("REST OFFSET"),   TEXT("PHYSICAL"), EType::Measured, EScope::All,     TEXT("lowest point below the pivot, cm: how far to lift it onto a floor") },
		{ TEXT("mass_kg"),       TEXT("MASS KG"),       TEXT("PHYSICAL"), EType::Number,   EScope::All,     TEXT("drop and throw physics, encumbrance, the weight of its sound") },
		{ TEXT("material"),      TEXT("MATERIAL"),      TEXT("PHYSICAL"), EType::Text,     EScope::All,     TEXT("metal plastic glass ceramic cloth paper organic rubber alien: sounds, decals, sliding") },
		{ TEXT("hands"),         TEXT("HANDS"),         TEXT("PHYSICAL"), EType::Number,   EScope::All,     TEXT("1 or 2 to hold") },
		{ TEXT("hold"),          TEXT("HOLD"),          TEXT("PHYSICAL"), EType::Text,     EScope::All,     TEXT("the held pose: Weapon Handheld Bottle Tool Pad Worn") },
		{ TEXT("physics_on_drop"), TEXT("PHYSICS ON DROP"), TEXT("PHYSICAL"), EType::Bool, EScope::All,     TEXT("simulate when dropped") },
		{ TEXT("bounce"),        TEXT("BOUNCE"),        TEXT("PHYSICAL"), EType::Number,   EScope::All,     TEXT("0 dead .. 1 lively") },
		// inventory and economy
		{ TEXT("stack_max"),     TEXT("STACK MAX"),     TEXT("INVENTORY"), EType::Number,  EScope::All,     TEXT("1 for weapons and armour") },
		{ TEXT("bulk"),          TEXT("BULK"),          TEXT("INVENTORY"), EType::Number,  EScope::All,     TEXT("grid cells") },
		{ TEXT("equip_slot"),    TEXT("EQUIP SLOT"),    TEXT("INVENTORY"), EType::Text,    EScope::All,     TEXT("Slot1 Slot2 Head Chest Arms Hands Legs Feet Acc Aug Shield Healing, or empty") },
		{ TEXT("value"),         TEXT("VALUE"),         TEXT("INVENTORY"), EType::Number,  EScope::All,     TEXT("credits") },
		{ TEXT("rarity"),        TEXT("RARITY"),        TEXT("INVENTORY"), EType::Text,    EScope::All,     TEXT("common uncommon rare unique") },
		{ TEXT("droppable"),     TEXT("DROPPABLE"),     TEXT("INVENTORY"), EType::Bool,    EScope::All,     TEXT("") },
		{ TEXT("sellable"),      TEXT("SELLABLE"),      TEXT("INVENTORY"), EType::Bool,    EScope::All,     TEXT("") },
		{ TEXT("quest"),         TEXT("QUEST"),         TEXT("INVENTORY"), EType::Bool,    EScope::All,     TEXT("cannot be lost") },
		{ TEXT("unique"),        TEXT("UNIQUE"),        TEXT("INVENTORY"), EType::Bool,    EScope::All,     TEXT("one in the world") },
		// use
		{ TEXT("use"),           TEXT("USE"),           TEXT("USE"),      EType::Text,     EScope::All,     TEXT("none consume equip read activate throw") },
		{ TEXT("use_time_s"),    TEXT("USE TIME S"),    TEXT("USE"),      EType::Number,   EScope::All,     TEXT("") },
		{ TEXT("charges"),       TEXT("CHARGES"),       TEXT("USE"),      EType::Number,   EScope::All,     TEXT("0 = unlimited") },
		{ TEXT("cooldown_s"),    TEXT("COOLDOWN S"),    TEXT("USE"),      EType::Number,   EScope::All,     TEXT("") },
		{ TEXT("effects"),       TEXT("EFFECTS"),       TEXT("USE"),      EType::List,     EScope::All,     TEXT("heal 25, stamina 40, unlock KEY_LAB_3, explode 60") },
		{ TEXT("durability_max"), TEXT("DURABILITY"),   TEXT("USE"),      EType::Number,   EScope::All,     TEXT("0 = does not wear") },
		// weapon
		{ TEXT("damage"),        TEXT("DAMAGE"),        TEXT("WEAPON"),   EType::Number,   EScope::Weapons, TEXT("per shot or swing") },
		{ TEXT("fire_rate"),     TEXT("FIRE RATE"),     TEXT("WEAPON"),   EType::Number,   EScope::Weapons, TEXT("per second") },
		// melee
		{ TEXT("hands"),         TEXT("HANDS"),         TEXT("MELEE"),    EType::Number,   EScope::Weapons, TEXT("1 or 2 (2: the two-handed hold, when it exists)") },
		{ TEXT("attack_set"),    TEXT("ATTACK SET"),    TEXT("MELEE"),    EType::Enum,     EScope::Weapons, TEXT("blade light heavy") },
		{ TEXT("blunt"),         TEXT("BLUNT"),         TEXT("MELEE"),    EType::Bool,     EScope::Weapons, TEXT("no cuts: a shove and a stagger instead") },
		{ TEXT("fire_modes"),    TEXT("FIRE MODES"),    TEXT("WEAPON"),   EType::List,     EScope::Weapons, TEXT("semi, burst, auto, laser -- the selector's positions in order; empty is semi only") },
		{ TEXT("ammo_kind"),     TEXT("AMMO"),          TEXT("WEAPON"),   EType::Enum,     EScope::Weapons, TEXT("none light medium heavy shell cell rocket") },
		{ TEXT("magazine"),      TEXT("MAGAZINE"),      TEXT("WEAPON"),   EType::Number,   EScope::Weapons, TEXT("rounds") },
		{ TEXT("reload_s"),      TEXT("RELOAD S"),      TEXT("WEAPON"),   EType::Number,   EScope::Weapons, TEXT("") },
		{ TEXT("spread_hip"),    TEXT("SPREAD HIP"),    TEXT("WEAPON"),   EType::Number,   EScope::Weapons, TEXT("degrees") },
		{ TEXT("spread_aim"),    TEXT("SPREAD AIM"),    TEXT("WEAPON"),   EType::Number,   EScope::Weapons, TEXT("degrees") },
		{ TEXT("recoil"),        TEXT("RECOIL"),        TEXT("WEAPON"),   EType::Number,   EScope::Weapons, TEXT("degrees of kick") },
		{ TEXT("moa"),           TEXT("ACCURACY MOA"),  TEXT("WEAPON"),   EType::Number,   EScope::Weapons, TEXT("the weapon's own group in minutes of angle, on top of the stance spread: 5 is a combat rifle, 1 a match rifle") },
		{ TEXT("range_m"),       TEXT("RANGE M"),       TEXT("WEAPON"),   EType::Number,   EScope::Weapons, TEXT("") },
		{ TEXT("skin"),          TEXT("SKIN"),          TEXT("WEAPON"),   EType::Text,     EScope::Weapons, TEXT("the material variant it wears, e.g. M_PolygonSciFiSpace_02_C (the sheet's SKIN button cycles the pack's lettered paints); empty = the mesh's own") },
		{ TEXT("optic"),         TEXT("OPTIC"),         TEXT("WEAPON"),   EType::Text,     EScope::Weapons, TEXT("an optics key from Weapons.json (the OPTIC cycle above lists them), or empty for irons") },
		// armour
		{ TEXT("armor_value"),   TEXT("ARMOR"),         TEXT("ARMOUR"),   EType::Number,   EScope::Armor,   TEXT("") },
		{ TEXT("body_slot"),     TEXT("BODY SLOT"),     TEXT("ARMOUR"),   EType::Text,     EScope::Armor,   TEXT("Head Chest Arms Hands Legs Feet Back") },
		{ TEXT("hides_hair"),    TEXT("HIDES HAIR"),    TEXT("ARMOUR"),   EType::Bool,     EScope::Armor,   TEXT("") },
		// audio and presentation
		{ TEXT("sound_drop"),    TEXT("SOUND DROP"),    TEXT("AUDIO"),    EType::Text,     EScope::All,     TEXT("a cue name; defaults from material") },
		{ TEXT("sound_pickup"),  TEXT("SOUND PICKUP"),  TEXT("AUDIO"),    EType::Text,     EScope::All,     TEXT("") },
		{ TEXT("sound_use"),     TEXT("SOUND USE"),     TEXT("AUDIO"),    EType::Text,     EScope::All,     TEXT("") },
		{ TEXT("glow"),          TEXT("GLOW"),          TEXT("AUDIO"),    EType::Bool,     EScope::All,     TEXT("emissive: the pickup highlight steps back") },
		// review
		{ TEXT("hidden"),        TEXT("HIDDEN"),        TEXT("REVIEW"),   EType::Bool,     EScope::All,     TEXT("not listed in the Reference (SHOW HIDDEN reveals it)") },
		{ TEXT("reviewdate"),    TEXT("REVIEWED"),      TEXT("REVIEW"),   EType::Text,     EScope::All,     TEXT("when a person last went over this entry") },
	};

	// The groups in the order a person tunes them: what is dragged in the viewer first, then
	// the numbers that make a weapon a weapon, then how it is used, then everything else.
	inline const TCHAR* GroupOrder[] = { TEXT("POINTS"), TEXT("WEAPON"), TEXT("MELEE"), TEXT("OPTIC"), TEXT("USE"), TEXT("INVENTORY"), TEXT("PHYSICAL"), TEXT("ARMOUR"), TEXT("AUDIO"), TEXT("IDENTITY"), TEXT("REVIEW") };

	inline bool Applies(const FField& F, const FString& Category)
	{
		// An optic carries its own fields and the review pair only: it is a part, not an item.
		if (Category == TEXT("optics")) { return F.Scope == EScope::Optics || F.Scope == EScope::Gear || FCString::Strcmp(F.Key, TEXT("hidden")) == 0 || FCString::Strcmp(F.Key, TEXT("reviewdate")) == 0; }
		switch (F.Scope)
		{
		case EScope::Optics:  return false;
		case EScope::Gear:    return Category == TEXT("weapons");
		case EScope::Weapons: return Category == TEXT("weapons");
		case EScope::Armor:   return Category == TEXT("armor");
		case EScope::Items:   return Category != TEXT("weapons");
		default:              return true;
		}
	}
}
