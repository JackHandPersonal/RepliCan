// Name-driven index of the Synty Sword Combat clips, built from the clips
// themselves (UCharacterAnimInstance::AllCombatAnims) rather than hand-
// curated, so it always reflects what the pack actually ships.
//
// The pack's naming is regular enough to read usage straight off the file:
//
//   A_<Category>_<Name>[_<Dir>][_<Phase>][_RootMotion[Horizontal|Vertical]]_Sword[_Masc|_Femn]
//
//   Attack_<Style><Family>[<ComboStep>]        e.g. Attack_LightCombo01A, Attack_HeavyStab01
//     ... and for EVERY attack a sibling  ..._ReturnToIdle : the recovery from
//     that attack's end pose back to the ready idle. Combo steps A/B/C chain
//     into each other (A's end pose is B's start), each with its own recovery.
//   Block_Begin / Block_Loop / Block_End         a held state
//   Parry_<F|L|R>, Parry_Break, Parry_F_ReturnToBlock,
//   Parry_F_CounterShove, Parry_F_PommelStrike  counters that follow a front parry
//   Dodge_<Dir>, DodgeRoll_<Dir>
//   Hit_<Dir>_React (light) / Hit_<Dir>_Stagger (heavy)
//   KnockDown_Begin/Loop/End, Stun_Begin/Loop/End
//   Death_<Dir>_01  + ..._Pose (the corpse hold)
//   Idle_Base (ready), Idle_Base_Sheathed, Idle_Base_ToIdle_<G> / Idle_<G>_ToBase
//   (ready <-> relaxed), Idle_EnergeticStance01 / Idle_Flourish01 (fidgets),
//   Idle_Menacing01_Begin / Idle_Menacing01 / Idle_Menacing01_End (taunt),
//   Idle_Sheathed_Sword_<G>, Draw_Sword_<G>, Sheathe_Sword_<G>
//
// _RootMotion variants move the capsule via the clip; this project drives the
// capsule from CharacterMovement and plays the in-place clips, so the
// capabilities on ABaseCharacter resolve to the non-RootMotion keys.
#pragma once

#include "CoreMinimal.h"

class UAnimSequence;

struct FCombatClipInfo
{
	// The lookup key: the asset name minus the "A_" prefix and any trailing
	// "_Sword" (e.g. "Attack_HeavyStab01_ReturnToIdle", "Block_Loop",
	// "Draw_Sword_Masc"). RootMotion variants keep their token in the key.
	FString Key;
	FString Category;     // Attack, Block, Parry, Dodge, DodgeRoll, Hit, KnockDown, Stun, Death, Idle, Draw, Sheathe
	FString Direction;    // F/B/L/R when the name carries one
	FString Phase;        // Begin/Loop/End/ReturnToIdle/Pose/React/Stagger/ReturnToBlock when present
	FString Gender;       // Masc/Femn when present
	bool bRootMotion = false;
	UAnimSequence* Clip = nullptr;
};

class REPLICAN_API FCombatAnimLibrary
{
public:
	void Build(const TArray<UAnimSequence*>& Clips);
	bool IsBuilt() const { return Infos.Num() > 0; }

	UAnimSequence* Find(const FString& Key) const;
	bool Has(const FString& Key) const { return Find(Key) != nullptr; }
	TArray<FString> Keys() const;
	const TArray<FCombatClipInfo>& All() const { return Infos; }

	// Human-readable grouping of what the pack provides, derived from the
	// names -- what an NPC/gameplay system can rely on being invokable.
	FString Describe() const;

private:
	TArray<FCombatClipInfo> Infos;
	TMap<FString, int32> KeyToIndex;
};
