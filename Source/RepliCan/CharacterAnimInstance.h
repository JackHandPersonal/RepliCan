// The single native AnimInstance every character (player, and any future
// NPC) uses as its ONE main AnimInstance -- locomotion blending
// (idle/walk/run), jump, crouch, head look-at, and facial expression
// (jaw/eyes/brow) all live here, entirely in C++, with no AnimGraph/
// Blueprint graph involved anywhere. This replaces what used to be split
// across three separate systems (ThirdPerson_AnimBP for locomotion, a
// hand-rolled crossfade instance for crouch/jump, and ABP_KnightFace as a
// post-process AnimBP for facial expression) -- consolidated because (a)
// this project's remote-exec Python workflow cannot edit AnimGraphs at all
// (confirmed: EdGraph's own Nodes array is Python-protected, can't even be
// read), which made ThirdPerson_AnimBP and ABP_KnightFace both effectively
// frozen, unmodifiable assets, and (b) the user directly prefers native C++
// over touching Unreal's Blueprint/AnimGraph editor wherever there's a
// viable alternative. Doing everything in one instance also sidesteps the
// post-process AnimBP's "Input Pose" requirement entirely (a post-process
// slot needs a Linked Input Pose node wired up to receive the main pose, or
// it silently discards it and shows a broken/static body -- confirmed live,
// this is exactly what routing through ABP_KnightFace as a post-process
// broke) -- since this is the ONLY AnimInstance on the mesh, there's no
// second pose to receive at all, just one pose this class builds and
// modifies directly.
//
// Owns its own animation asset references and bone names (via
// ConstructorHelpers::FObjectFinder in its constructor, the same pattern
// AFaceController now uses) rather than relying on the owning Character to
// supply them, so any character gets full locomotion/crouch/look-at/face
// behavior for free just by using this as its AnimClass -- no per-actor
// Python wiring required.
//
// Pose generation/blending works by overriding FAnimInstanceProxy::
// Evaluate() directly -- the same mechanism UAnimSingleNodeInstance itself
// uses internally to produce a pose with no compiled AnimGraph at all.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "GaitAdjustments.h"
#include "CharacterAnimInstance.generated.h"

class UAnimSequence;
class UCharacterAnimInstance;

UENUM()
enum class ECharacterLocomotionState : uint8
{
	// Walk/Jog/Run are three real speed tiers structured around what the
	// Synty pack actually ships (Walk ~160, "Run" ~285 = a jog, Sprint ~800
	// cm/s natively) -- Run is the sprint tier. Lyra's set only has two
	// clips (Walk, Jog) so its Run tier reuses the jog clip.
	Idle, Walk, Jog, Run, JumpRise, JumpFall, JumpEnd, CrouchIdle, CrouchWalk, CrouchRun, Roll, Dash, Pose, Attack,
	// Synty transition clips (see the matching FLocomotionAnimSet slots):
	// a start turn out of idle, a settle into idle, a turn on the spot,
	// stand<->crouch, and the sprint->crouch slide.
	StartMove, StopMove, TurnInPlace, CrouchDown, CrouchUp, SprintSlide
};

// Which of the 4 real Lyra Dash clips to play -- picked by ABaseCharacter::
// StartDash from whatever WASD is currently held (diagonals resolve to
// whichever axis has the larger magnitude), not from a per-direction input
// binding the way an 8-way roll once tried to work.
// Which Synty locomotion set a character moves with (see
// UCharacterAnimInstance::GetActiveSyntySet). Male/Female are the Base
// Locomotion pack's Masculine/Feminine sets; Goblin is the Goblin
// Locomotion pack (a hunched, scurrying gait on the same Polygon rig).
UENUM(BlueprintType)
enum class ESyntyLocomotionChoice : uint8
{
	Male, Female, Goblin
};

UENUM()
enum class ECharacterDashDirection : uint8
{
	Forward, Backward, Left, Right
};

// Which whole moveset UCharacterAnimInstance's core Idle/Walk/Run/Crouch/
// Jump states pull their clips from -- toggled with 'O' (see
// UCharacterAnimInstance::ToggleLocomotionSet). Lyra is this project's
// original, already-tuned set; Synty is the pack retargeted this session
// (see project notes on the PolygonSource -> Mannequin IK Retargeter).
// Deliberately NOT the same concept as IdleVariants/CycleIdleVariant ('I')
// -- that cycles WITHIN whichever set is currently active.
UENUM()
enum class ECharacterLocomotionSet : uint8
{
	Lyra, Synty
};

// One movement tier's 8-way strafe clips, as Synty ships them
// (FwdStrafe F/FL/FR/L/R, BckStrafe B/BL/BR). Pick() selects by the
// direction of travel RELATIVE TO THE CHARACTER'S FACING -- with
// bOrientRotationToMovement (third person) that's almost always F, so the
// strafes only come into play where facing and movement genuinely differ
// (first person, or a future aim/strafe stance). Any unset slot falls back
// to F, so a partially-filled set still works.
USTRUCT()
struct FDirectionalAnimSet
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> F;
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> FL;
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> FR;
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> L;
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> R;
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> B;
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> BL;
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> BR;

	// MoveAngleDeg: direction of travel relative to actor forward, -180..180
	// (positive = to the character's right).
	UAnimSequence* Pick(float MoveAngleDeg) const;
};

// The Synty moveset, structured around exactly what the Base Locomotion
// pack ships (see the folder inventory in the session notes): three
// standing speed tiers with measured native speeds (Walk ~160, Jog ~285,
// Sprint ~800 cm/s), an 8-way crouch tier, jump arcs per takeoff tier,
// short/long fall loops, and soft/medium/hard landings. The standalone
// IdleAnim/WalkAnim/RunAnim/etc. properties on UCharacterAnimInstance
// remain untouched as "the Lyra set" and are swapped against this as a
// whole via ECharacterLocomotionSet.
//
// The *AnimSpeed values are each clip's own real baked travel speed,
// measured directly off its "_RootMotion" sibling's root-bone translation
// (Synty ships one per clip). Play rate is ActualSpeed / AnimSpeed, so the
// stride cadence always matches distance actually covered, whatever the
// character's configured tier speeds are -- and when a tier speed EQUALS
// the clip's native speed (the defaults on ABaseCharacter are set that
// way) the clip simply plays at 1.0x. Lyra's clips have no equivalent:
// their root motion was deliberately zeroed (see clean_to_ue4_mannequin.py),
// so those keep the current-speed-cap ratio instead.
USTRUCT()
struct FLocomotionAnimSet
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	TObjectPtr<UAnimSequence> IdleAnim;

	UPROPERTY(EditAnywhere)
	FDirectionalAnimSet Walk;

	UPROPERTY(EditAnywhere, meta = (ClampMin = "1.0"))
	float WalkAnimSpeed = 160.27f;

	// Synty's own "Run" folder -- a jog by measurement (285 cm/s).
	UPROPERTY(EditAnywhere)
	FDirectionalAnimSet Jog;

	UPROPERTY(EditAnywhere, meta = (ClampMin = "1.0"))
	float JogAnimSpeed = 285.44f;

	// Forward only -- Synty ships no sprint strafes.
	UPROPERTY(EditAnywhere)
	TObjectPtr<UAnimSequence> SprintAnim;

	UPROPERTY(EditAnywhere, meta = (ClampMin = "1.0"))
	float SprintAnimSpeed = 799.7f;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UAnimSequence> CrouchIdleAnim;

	// One crouch tier only; CrouchRun (Shift) plays these same clips faster
	// -- see ECharacterLocomotionState::CrouchRun. Measured 160 cm/s, same
	// pace as Walk.
	UPROPERTY(EditAnywhere)
	FDirectionalAnimSet Crouch;

	UPROPERTY(EditAnywhere, meta = (ClampMin = "1.0"))
	float CrouchAnimSpeed = 160.27f;

	// Full launch-to-land arcs (2.2-3.1s), one per takeoff tier -- picked
	// at the moment the character leaves the ground by how fast it was
	// moving. Played once; if the character is still airborne when the arc
	// runs out (a long drop), FallShortAnim/FallLargeAnim loop instead.
	UPROPERTY(EditAnywhere)
	TObjectPtr<UAnimSequence> JumpIdleAnim;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UAnimSequence> JumpWalkingAnim;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UAnimSequence> JumpRunningAnim;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UAnimSequence> JumpSprintingAnim;

	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.1"))
	float JumpArcPlayRate = 1.0f;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UAnimSequence> FallShortAnim;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UAnimSequence> FallLargeAnim;

	// Airborne longer than this switches the fall loop to FallLargeAnim.
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float LongFallAirSeconds = 1.2f;

	// Landing recovery picked by how long the character was airborne.
	UPROPERTY(EditAnywhere)
	TObjectPtr<UAnimSequence> LandSoftAnim;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UAnimSequence> LandMediumAnim;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UAnimSequence> LandHardAnim;

	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float MediumLandingAirSeconds = 0.9f;

	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float HardLandingAirSeconds = 1.6f;

	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.1"))
	float LandPlayRate = 1.5f;

	// Once a landing has played at least this long, movement input is
	// allowed to interrupt the rest of the recovery -- the Land_Idle* clips
	// settle all the way to a standstill, which feels sticky if the player
	// is already holding a direction.
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float LandBreakoutSeconds = 0.25f;

	// ---- Slopes: the pack's Up25/Down25 forward clips, swapped in for the
	// flat forward clip while the floor under the character rises/falls
	// along its travel by more than SlopeEnterDegrees (see
	// FLocomotionInputs::SlopeDeg). Same native speeds as the flat clips.
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> WalkUpAnim;
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> WalkDownAnim;
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> JogUpAnim;
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> JogDownAnim;
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> SprintUpAnim;
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> SprintDownAnim;
	UPROPERTY(EditAnywhere) float SlopeEnterDegrees = 12.0f;
	UPROPERTY(EditAnywhere) float SlopeExitDegrees = 7.0f;

	// ---- Start transitions (idle -> walk/run), the ROOT-MOTION twins: the
	// in-place versions of the pack's 90/180 starts have the turn stripped
	// out entirely (measured), so the yaw lives only in the root track of
	// the RootMotion clips. The proxy plays these with the root bone reset
	// in the pose and hands the extracted yaw to the character each tick
	// (see FCharacterAnimInstanceProxy::RootYawDeltaPending), which turns
	// the capsule by it while orient-to-movement is suspended.
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> StartWalkF;
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> StartWalk90L;
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> StartWalk90R;
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> StartWalk180L;
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> StartWalk180R;
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> StartRunF;
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> StartRun90L;
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> StartRun90R;
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> StartRun180L;
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> StartRun180R;

	// ---- Stop transitions (walk/run -> idle), in-place clips: played the
	// moment movement input is released, while the capsule brakes; picked by
	// which foot is planted (the gait layer's stance detection).
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> StopWalkLFoot;
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> StopWalkRFoot;
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> StopRunLFoot;
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> StopRunRFoot;

	// ---- Crouch transitions. SprintToCrouchAnim is the ROOT-MOTION twin (a
	// long slide -- ~11.6m authored, scaled by SprintSlideScale) and drives
	// the capsule's translation the way the roll does.
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> StandToCrouchAnim;
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> CrouchToStandAnim;
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> SprintToCrouchAnim;
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0")) float SprintSlideScale = 0.6f;

	// ---- Turn in place (root-motion twins, yaw only; no translation).
	// Crouching has 90s only -- a crouched 180 plays the 90 twice.
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> TurnStanding90L;
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> TurnStanding90R;
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> TurnStanding180L;
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> TurnStanding180R;
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> TurnCrouching90L;
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> TurnCrouching90R;
};

// Everything the owning Character reports each tick, snapshotted into the
// proxy in one go (see FCharacterAnimInstanceProxy::UpdateLocomotionInputs)
// -- a struct rather than an ever-growing argument list. Speeds are the
// character's configured tier speeds so the state machine can pick the
// nearest tier without any hardcoded thresholds of its own.
struct FLocomotionInputs
{
	float Speed = 0.0f;              // horizontal speed, cm/s
	bool bIsInAir = false;
	float VelocityZ = 0.0f;
	bool bIsCrouched = false;
	float MaxSpeed = 0.0f;           // whichever cap currently applies (Lyra play rate)
	float WalkSpeed = 0.0f;
	float JogSpeed = 0.0f;
	float RunSpeed = 0.0f;
	float CrouchSlowSpeed = 0.0f;
	float CrouchFastSpeed = 0.0f;
	float MoveAngleDeg = 0.0f;       // travel direction relative to actor forward, -180..180
	float AirTime = 0.0f;            // seconds continuously airborne, 0 on the ground
	// Mesh scale along the direction of travel (1 = unscaled). A stride on
	// a mesh scaled 2x covers twice the ground, so every speed-synced play
	// rate divides by this -- the character's actual speed is unchanged by
	// scaling, only how far each step carries it. See FCharacterConfig::Scale.
	float StrideScale = 1.0f;
	// Movement INPUT this frame (acceleration), separate from velocity: what
	// the start/stop transitions key off -- a start turn needs the intended
	// direction before the body has rotated toward it, and a stop should
	// begin the moment the stick is released, not once the capsule has
	// already braked to a halt.
	bool bHasMoveInput = false;
	float InputAngleDeg = 0.0f;       // input direction relative to actor forward, -180..180
	// Floor grade along the direction of travel, degrees: + uphill, - downhill.
	float SlopeDeg = 0.0f;
};

USTRUCT()
struct FCharacterAnimInstanceProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

public:

	FCharacterAnimInstanceProxy() = default;
	explicit FCharacterAnimInstanceProxy(UAnimInstance* InAnimInstance)
		: FAnimInstanceProxy(InAnimInstance)
	{
	}

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

	// Called every tick by the owning Character with fresh movement state;
	// the actual state-machine transition + blend decision happens in
	// PreUpdate (using the values cached here), not synchronously in this
	// call, mirroring how the rest of this proxy already only mutates blend
	// state from PreUpdate. See FLocomotionInputs for what each field is
	// for; the one subtlety is Lyra's crouch play rate, which is computed
	// against the FIXED CrouchFastSpeed rather than the current cap
	// (MaxSpeed) -- against the current cap both crouch tiers settled to
	// ~1.0x once each reached its own cap, erasing the visible slow/fast
	// cadence difference the second tier exists to show.
	void UpdateLocomotionInputs(const FLocomotionInputs& In) { PendingInputs = In; }

	// Only ever writes the PENDING copies, never bHasLookAtTarget/
	// LookAtTargetWorld directly -- those are what Evaluate() (which can run
	// on an animation worker thread, overlapping with a LATER frame's game-
	// thread Tick -- the entire reason FAnimInstanceProxy splits PreUpdate
	// from Evaluate) reads through ApplyLookAt. The owning Character calls
	// this from its own Tick() with no synchronization at all relative to a
	// still-in-flight Evaluate() for a previous frame, so writing the "live"
	// fields directly here was a genuine data race -- confirmed as the real
	// cause of an otherwise unexplained intermittent "spiral" bug (sane,
	// finite numbers in every diagnostic log line, yet still visibly wrong
	// on screen some frames and not others -- the signature of a race, not a
	// math error, and why three different rotation-axis fixes in a row never
	// helped). PreUpdate (game thread only, always fully finished before the
	// engine kicks off the parallel Evaluate task for that same frame) is
	// the one place that copies these into the stable fields -- mirrors the
	// PendingSpeed/etc. pattern UpdateLocomotionInputs already uses for
	// exactly this reason.
	void SetLookAtTarget(const FVector& WorldPoint) { bPendingHasLookAtTarget = true; PendingLookAtTargetWorld = WorldPoint; }
	void ClearLookAtTarget() { bPendingHasLookAtTarget = false; }

	// Same Pending-then-snapshot-in-PreUpdate pattern as SetLookAtTarget
	// above, for the same reason -- ABaseCharacter::StartRoll calls this from
	// its own Tick() with no coordination with a possibly still-running
	// Evaluate() for a previous frame. UpdateLocomotionState (PreUpdate, game
	// thread) is what actually consumes bPendingRollTriggered.
	void TriggerRoll() { bPendingRollTriggered = true; }

	// Turn on the spot by roughly DeltaYaw degrees (the nearest 90/180 L/R
	// clip; a crouched 180 chains two 90s). Same pending pattern as the roll.
	void TriggerTurnInPlace(float DeltaYawDegrees) { bPendingTurnTriggered = true; PendingTurnYaw = DeltaYawDegrees; }
	// The sprint -> crouch slide (root-motion driven, see SprintToCrouchAnim).
	void TriggerSprintSlide() { bPendingSlideTriggered = true; }
	bool bPendingSlideTriggered = false;
	// Cuts a playing attack/try-out clip short: the Attack state falls
	// through to ordinary locomotion on the next update.
	void AbortAttack() { bPendingAttackAbort = true; }
	bool bPendingAttackAbort = false;

	// Root motion the current transition clip wants applied to the CAPSULE
	// (see FLocomotionAnimSet's start-transition comment): accumulated in
	// PreUpdate from the clip's root track, handed over once per tick by the
	// character (ConsumeRootMotion) which turns/moves the actor and, while
	// IsClipDrivingRotation, suspends orient-to-movement. Yaw in degrees;
	// translation in the MESH component's local space (the roll does the
	// same conversion).
	void ConsumeRootMotion(float& OutYawDeg, FVector& OutMeshTranslation)
	{
		OutYawDeg = RootYawDeltaPending; OutMeshTranslation = RootTranslationPending;
		RootYawDeltaPending = 0.0f; RootTranslationPending = FVector::ZeroVector;
	}
	bool IsClipDrivingRotation() const { return bClipDrivesYaw; }
	bool IsClipDrivingTranslation() const { return bClipDrivesTranslation; }

	// Same pattern as TriggerRoll, plus which of the 4 Dash clips to play --
	// ABaseCharacter::StartDash has already picked the direction from
	// current WASD input by the time this is called.
	void TriggerDash(ECharacterDashDirection Direction) { bPendingDashTriggered = true; PendingDashDirection = Direction; }

	// Same pattern again -- steps to the next entry in Owner->PoseAnims and
	// forces a transition into it, even if already showing a pose (so
	// repeated presses cycle visibly instead of the 2nd+ press doing
	// nothing because Desired == CurrentState already).
	void TriggerPoseCycle() { bPendingPoseCycleTriggered = true; }

	// Plays Anim once, then falls back to normal locomotion -- same forced-
	// transition-then-hold-until-finished pattern as Roll/Dash, but with no
	// root motion extraction of its own: these are plain in-place clips
	// (this project's usual convention), so CharacterMovementComponent
	// keeps driving actual movement while ABaseCharacter just blocks new
	// movement input for the clip's duration. Takes the clip directly
	// (not an index into Owner->AttackAnims) so this same path also serves
	// ABaseCharacter's full-combat-list preview/play tool ('[' ']' cycle
	// through Owner->AllCombatAnims, left mouse button plays whichever is
	// selected) without needing a second, near-duplicate trigger mechanism.
	// bLoop = a HELD clip: loops in the Attack state, ignoring locomotion
	// input, until ReleaseAttackHold() (or a new trigger) -- used for the
	// combat pack's Loop phases (Block/Stun/KnockDown), the Menacing taunt,
	// and the Death _Pose hold.
	void TriggerAttack(UAnimSequence* Anim, bool bLoop = false) { bPendingAttackTriggered = true; PendingAttackAnim = Anim; bPendingAttackLoop = bLoop; }
	void ReleaseAttackHold() { bCurrentAttackLoops = false; }

	// Advances which entry of Owner->IdleVariants the Idle state shows.
	// Takes effect immediately if currently idle-appropriate (grounded, not
	// crouched, not moving) by forcing a transition; otherwise just updates
	// which variant the NEXT natural entry into Idle will use, since
	// interrupting a run/jump/crouch just to preview an idle variant would
	// be a bigger visual glitch than a one-tick-delayed switch.
	void TriggerIdleVariantCycle() { bPendingIdleVariantCycleTriggered = true; }

	// Swaps which whole moveset (Lyra vs Synty) the core locomotion states
	// pull from -- see ECharacterLocomotionSet's header comment. The switch
	// itself is immediate (no Pending-then-snapshot needed, same reasoning
	// as CurrentIdleVariantIndex: only ever read from UpdateLocomotionState
	// on the game thread, never from Evaluate()); bPendingLocomotionSetToggled
	// is a separate one-shot flag purely so UpdateLocomotionState can force
	// an immediate PlayWithBlend the exact frame this was called, even if
	// Desired otherwise wouldn't change (e.g. standing still in Idle).
	void ToggleLocomotionSet()
	{
		CurrentLocomotionSet = (CurrentLocomotionSet == ECharacterLocomotionSet::Lyra) ? ECharacterLocomotionSet::Synty : ECharacterLocomotionSet::Lyra;
		bPendingLocomotionSetToggled = true;
	}

	// Read-only, thread-safe to call from anywhere (plain enum read) --
	// lets ABaseCharacter apply the Synty-set mesh-height compensation (see
	// SyntyMeshZOffset's header comment) without needing its own separate
	// copy of which set is active.
	ECharacterLocomotionSet GetCurrentLocomotionSet() const { return CurrentLocomotionSet; }

	// Read-only, thread-safe to call from anywhere (plain pointer read, same
	// reasoning as GetCurrentLocomotionSet above) -- lets the HUD show which
	// clip is actually playing right now, since the locomotion STATE name
	// alone doesn't distinguish e.g. Dash's 4 direction-specific clips or
	// which of Lyra's/Synty's Walk anims PickAnim chose.
	UAnimSequence* GetCurrentAnim() const { return ToAsset; }
	// Gait timing readouts for tests/HUD (see UpdateGaitTiming).
	float GetGaitRateMultiplier() const { return GaitRateMultiplier; }
	float GetGaitStanceBlend() const { return StanceBlend; }

	// Read-only diagnostic -- lets the HUD show the actual play rate being
	// used right now, so a skating/pacing fix can be confirmed as actually
	// engaging (or not) by watching the number, instead of only judging by
	// how the movement looks.
	float GetCurrentPlayRate() const { return ToPlayRate; }

private:

	// SeedFromAsset lets a call seed the blend even when this proxy has
	// never played anything before -- without it there is no FromAsset to
	// blend out of on that very first call, so it would have to snap
	// straight to NewAsset instead of fading in. Unlike the old split
	// AnimBP/crossfade design, this is now only relevant for the single
	// very-first call this instance ever makes (there's no more "just
	// switched systems" case to worry about, since this instance is
	// assigned once at BeginPlay and never swapped).
	void PlayWithBlend(UAnimSequence* NewAsset, bool bLoop, float PlayRate, float InBlendDuration);

	// Runs the locomotion state machine off PendingSpeed/bPendingIsInAir/
	// PendingVelocityZ/bPendingIsCrouched (see UpdateLocomotionInputs) and
	// calls PlayWithBlend on any real transition. Reads animation assets/
	// thresholds/blend durations from Owner.
	void UpdateLocomotionState(float DeltaSeconds);

	// Ramps LookAtWeight toward 0 or 1 depending on whether LookAtTargetWorld
	// is currently within Owner's LookAtMaxYawDegrees/LookAtMaxPitchDegrees
	// range of the owning actor's forward vector -- moved here (rather than
	// living on the Character) so the whole look-at concept, including the
	// "give up if it's behind you" logic, belongs to the animation system,
	// not the gameplay character.
	void UpdateLookAt(float DeltaSeconds);

	// Rotates LookAtBoneName (if set) within Output toward LookAtWorldTarget,
	// blended by LookAtWeight -- applied in COMPONENT space via FCSPose,
	// since "face this exact world point" needs to reason about the mesh's
	// actual world transform.
	void ApplyLookAt(FPoseContext& Output);

	// ---- Gait adjustments (see GaitAdjustments.h) --------------------------
	// Timing half, game thread (PreUpdate): the play-rate multiplier from
	// CadenceScale / Stumble jitter / Limp, plus the noise clock.
	void UpdateGaitTiming(float DeltaSeconds);
	// Posture half, on the final target pose: stance width, toe-out, hunch,
	// lean, arm swing, bounce, sway, and the pose side of stumble/limp.
	void ApplyGaitAdjustments(FPoseContext& Output);
	FGaitAdjustments CachedGait;             // Owner->Gait snapshot (PreUpdate)
	float CachedDeltaSeconds = 0.0f;
	float GaitNoiseTime = 0.0f;               // per-instance random offset + elapsed
	float GaitRateMultiplier = 1.0f;          // folded into the locomotion play rate
	float StumbleRateTarget = 1.0f;           // current jitter target
	float StumbleRetargetIn = 0.0f;           // seconds until the next jitter target
	float StumbleHitchLeft = 0.0f;            // seconds left in a hitch (rate dip)
	float LimpRateBlend = 0.0f;               // -1..1, +1 while the weak leg bears weight
	bool bStanceLeft = false;                 // which foot is lower (written in Evaluate)
	bool bStanceKnown = false;
	float StanceBlend = 0.0f;                 // -1 (right planted) .. +1 (left planted), eased
	float PelvisZAverage = 0.0f;              // running mean of pelvis height, for BounceScale
	bool bPelvisZAverageValid = false;
	FQuat ArmSwingAverage[2] = { FQuat::Identity, FQuat::Identity };   // running mean upperarm local rot (L, R), for ArmSwingScale
	bool bArmSwingAverageValid[2] = { false, false };
	bool bGaitNoiseSeeded = false;
	bool bGaitMoving = false;                 // locomotion state this frame (timing terms only then)
	float GaitMovingBlend = 0.0f;             // bGaitMoving eased 0..1, for the moving-only posture terms

	// Applies JawHingeDeg/EyesHingeDeg/EyesBlinkScale/BrowHeightCm/
	// BrowAngleDeg directly to their bones' LOCAL transforms -- simpler than
	// ApplyLookAt, no component-space conversion needed, since these are
	// plain bone-local rotation/translation/scale tweaks (the same thing a
	// "Transform (Modify) Bone" node set to Local Space would do), not
	// "face this world point."
	void ApplyFacialBoneEdits(FPoseContext& Output);

	// Same bone-local-hinge technique as ApplyFacialBoneEdits, applied to
	// the right hand's finger bones instead -- see WeaponGripWeight's
	// header comment.
	void ApplyWeaponGripCorrection(FPoseContext& Output);

	// Per-bone layered blend of CachedArmOverridePose onto the arm chains --
	// see ArmOverridePose's header comment on UCharacterAnimInstance. Runs
	// before ApplyLookAt/ApplyWeaponGripCorrection so the grip curl still
	// lands on top of the overridden arm.
	void ApplyArmOverride(FPoseContext& Output);

	// Two-bone IK, straight on the compact pose. Runs LAST, after everything else has had its
	// say about the arm, because whatever it is told to reach it must actually reach.
	void ApplyHandIK(FPoseContext& Output);
	// Leans the spine into the aim pitch. Runs before the look-at so the head compensates.
	void ApplySpineLean(FPoseContext& Output);
	// One arm. Target is in component space. The pole comes from the arm's own animated elbow,
	// so the pose keeps its character and the IK only corrects the hand -- inventing a pole
	// vector is where two-bone IK usually starts flipping elbows.
	void SolveTwoBone(FCSPose<FCompactPose>& CS, FName UpperName, FName LowerName, FName EndName,
	                  const FTransform& TargetCS, float Weight, float ElbowDownBias = 0.0f);

	// ---- Orientation retarget --------------------------------------------
	//
	// Every clip this project owns is on UE4_Mannequin_Skeleton. The Modular
	// Fantasy Hero rig has the SAME joint positions but its own bone-axis
	// conventions (root carries a 90 degree roll, every bone's local frame
	// is oriented differently), so copying the clip's local rotations onto
	// it -- which is all a compatible-skeleton link does -- lays the body
	// flat (measured: pelvis at Y=87 instead of Z=87, "swimming").
	//
	// Fix, done here at evaluation time rather than by re-authoring ~200
	// clips: the pose is kept in the CLIP's convention for the whole
	// evaluation (extraction, blends, arm override, look-at, facial edits,
	// grip -- everything that reasons about bone axes keeps working), then
	// ToTargetConvention converts it as the very last step. Per bone, the
	// world-space rotation the clip applied to its bone is applied to the
	// mesh's bone instead: TargetCS = SourceCS * Delta, with Delta =
	// SourceRefCS^-1 * TargetRefCS from the two reference skeletons.
	// Translations come from the mesh's own reference pose (bone lengths),
	// except the pelvis, whose animated offset from its reference position
	// is carried across so the hips still bob. Bones the clip's skeleton
	// doesn't have (the hero's extra fingers) are left untouched.
	//
	// When active, the clip is evaluated into a pose on the CLIP's own
	// skeleton (SourceBoneContainer) so no engine compatible-skeleton remap
	// runs -- the engine's remap was found to apply its correction twice in
	// the editor's raw-data path. Inactive (zero cost) when the mesh already
	// uses the clip's skeleton, which is every character this project had
	// before the modular pack.
	void EnsureRetargetTables(const FBoneContainer& BoneContainer, const USkeleton* AnimSkeleton);
	// The whole per-frame evaluation (extract/blend + every pose edit) into
	// whichever context is being used.
	void EvaluatePoseInto(FPoseContext& Ctx);
	void ConvertSourceToTarget(const FPoseContext& Source, FPoseContext& Output);

	// Pins the lowest sole bone to the rig's reference sole height while
	// grounded -- see UCharacterAnimInstance::bGroundFeet.
	void GroundFeet(FPoseContext& Output);
	float ComputeFootShift(const FPoseContext& Output) const;
	float GroundShift = 0.0f;

	bool bRetargetActive = false;
	const USkeleton* RetargetSourceSkeleton = nullptr;
	const USkeleton* RetargetTargetSkeleton = nullptr;
	int32 RetargetNumBones = 0;
	FBoneContainer SourceBoneContainer;
	TArray<FQuat> SourceRefCS;                  // per source compact bone
	TArray<FVector> SourceRefTranslation;       // per source compact bone
	TArray<int32> TargetToSourceCompact;        // per target compact bone
	TArray<FQuat> RetargetDelta;                // per target compact bone
	TArray<FVector> RetargetCurlAxis;           // per target compact bone (grip hinge on this rig)
	TArray<FVector> RetargetTargetRefTranslation;
	int32 RetargetPelvisCompactIndex = INDEX_NONE;


	UAnimSequence* FromAsset = nullptr;
	UAnimSequence* ToAsset = nullptr;
	float FromTime = 0.0f;
	float ToTime = 0.0f;
	float FromPlayRate = 1.0f;
	float ToPlayRate = 1.0f;
	bool bFromLooping = true;
	bool bToLooping = true;

	// 0 = fully showing FromAsset, 1 = fully showing ToAsset. Starts at 1
	// (nothing to blend from) so the very first PlayWithBlend() call snaps
	// immediately rather than fading in from a ref pose.
	float BlendAlpha = 1.0f;
	float BlendDuration = 0.2f;

	ECharacterLocomotionState CurrentState = ECharacterLocomotionState::Idle;
	float TimeInState = 0.0f;
	bool bLocomotionInitialized = false;

	FLocomotionInputs PendingInputs;

	// Air time as of the last airborne frame -- the landing branch needs it
	// to pick soft/medium/hard, but by the time it runs the character is
	// already grounded and PendingInputs.AirTime has reset to 0.
	float LastAirTime = 0.0f;

	// Synty jump arcs / landings are picked at the moment the state is
	// entered and then held until they finish -- these remember WHICH clip
	// (and at what rate) so the hold-check can measure against the right
	// length, same pattern as CurrentAttackAnim/CurrentDashDirection.
	UAnimSequence* CurrentJumpAnim = nullptr;
	UAnimSequence* CurrentLandAnim = nullptr;
	float CurrentLandPlayRate = 1.0f;

	// Transition clips (StartMove/StopMove/TurnInPlace/CrouchDown/CrouchUp/
	// SprintSlide): the clip being held, and whether its root track drives
	// the capsule (see ConsumeRootMotion). PrevToTime is ToTime before this
	// frame's AdvanceTime, the range the root motion is extracted over.
	UAnimSequence* CurrentTransitionAnim = nullptr;
	bool bClipDrivesYaw = false;
	bool bClipDrivesTranslation = false;
	float RootMotionScale = 1.0f;
	float PrevToTime = 0.0f;
	float RootYawDeltaPending = 0.0f;
	FVector RootTranslationPending = FVector::ZeroVector;
	bool bPendingTurnTriggered = false;
	float PendingTurnYaw = 0.0f;
	int32 TurnStepsLeft = 0;            // a crouched 180 = two 90s
	float TurnStepYaw = 0.0f;
	ECharacterLocomotionState StateBeforeTransition = ECharacterLocomotionState::Idle;
	// Slope clip hysteresis: -1 down, 0 flat, +1 up.
	int32 SlopeState = 0;

	// Arm-override layer state (see UCharacterAnimInstance::ArmOverridePose).
	// Alpha eases toward the target weight in PreUpdate; the pose clip's
	// own time advances there too. The Cached* copies are what Evaluate
	// reads -- same rule as CachedJawHingeDeg: Evaluate never touches
	// Owner-> itself, since the panel can rewrite those properties at any
	// time on the game thread.
	float ArmOverrideAlpha = 0.0f;
	float ArmOverrideTime = 0.0f;
	// A clip swap is a CROSSFADE, not a cut. When the owner sets a new pose the old one is kept
	// here, still advancing, and blended out over ArmOverrideSwapSeconds -- so a fired shot
	// hands back to the idle, and the idle hands over to the sights pose, without the pop of
	// one frame being clip A and the next being clip B at frame zero.
	UAnimSequence* PrevArmOverridePose = nullptr;
	float PrevArmOverrideTime = 0.0f;
	bool bPrevArmOverrideLoops = true;
	bool bCachedArmOverrideLoops = true;
	float ArmSwapAlpha = 1.0f;
	// The idle's clock keeps running whether or not the idle is what is playing. A looping pose
	// that comes back after a one-shot resumes at this clock rather than restarting at zero, so
	// the breathing does not hitch every time the trigger is pulled.
	float ArmIdleClock = 0.0f;
	// The serial this proxy has already acted on; a mismatch means the owner
	// swapped the clip and the layer restarts at zero (see SetArmOverride).
	int32 ArmOverrideSerialSeen = -1;
	UAnimSequence* CachedArmOverridePose = nullptr;
	TArray<FName> CachedArmOverrideRootBones;

	// Hand IK, copied on the game thread in PreUpdate: Evaluate never reads Owner-> itself.
	FTransform CachedIKTargetR = FTransform::Identity;
	FTransform CachedIKTargetL = FTransform::Identity;
	float CachedIKWeightR = 0.0f;
	float CachedElbowBiasR = 0.0f, CachedElbowBiasL = 0.0f;
	bool bCachedFullBody = false;
	float CachedIKWeightL = 0.0f;
	float CachedIKMaxReach = 0.985f;
	FName CachedIKBones[6];
	// Torso lean, copied in PreUpdate like the IK.
	float CachedAimPitch = 0.0f;
	float CachedSpineLeanWeight = 0.0f;
	float CachedSpineLeanFraction = 0.0f;
	FVector CachedAimForwardCS = FVector::ForwardVector;
	TArray<FName> CachedSpineLeanBones;

	// See TriggerRoll's header comment for why this is a Pending field rather
	// than snapping CurrentState directly.
	bool bPendingRollTriggered = false;

	// Same reasoning as bPendingRollTriggered, for Dash.
	bool bPendingDashTriggered = false;
	ECharacterDashDirection PendingDashDirection = ECharacterDashDirection::Forward;

	// Which direction the CURRENTLY playing (or just-finished) dash used --
	// needed by the "still playing, hold" branch in UpdateLocomotionState,
	// which has to know which of the 4 clips' GetPlayLength() to check
	// against, not just that Desired == CurrentState == Dash.
	ECharacterDashDirection CurrentDashDirection = ECharacterDashDirection::Forward;

	bool bPendingPoseCycleTriggered = false;
	bool bPendingIdleVariantCycleTriggered = false;

	bool bPendingAttackTriggered = false;
	UAnimSequence* PendingAttackAnim = nullptr;
	bool bPendingAttackLoop = false;
	bool bCurrentAttackLoops = false;

	// Which clip the CURRENTLY playing (or just-finished) attack is -- needed
	// by the "still playing, hold" branch in UpdateLocomotionState, which has
	// to know this specific clip's GetPlayLength() to check against.
	UAnimSequence* CurrentAttackAnim = nullptr;

	// Mutable proxy-owned state, same reasoning as CurrentState/TimeInState
	// above -- both just index into Owner->PoseAnims/Owner->IdleVariants,
	// which are themselves plain read-only config the proxy never mutates.
	int32 CurrentPoseIndex = 0;
	// Starts on the Synty idle (IdleVariants[3]) to match the default
	// locomotion set; 'I' still cycles through all of them.
	int32 CurrentIdleVariantIndex = 3;

	// Synty by default: the Walk/Jog/Run tiers and their speeds are built
	// around the Synty pack's clips (see FLocomotionAnimSet), and every
	// relaunch defaulting to Lyra meant an 'O' press before each test. 'O'
	// still toggles to Lyra for comparison.
	ECharacterLocomotionSet CurrentLocomotionSet = ECharacterLocomotionSet::Synty;
	bool bPendingLocomotionSetToggled = false;

	// Returns Owner->IdleVariants[CurrentIdleVariantIndex], falling back to
	// Owner->IdleAnim if the variants array is empty or the index is
	// somehow out of range -- keeps every Idle-anim call site from needing
	// its own bounds-check.
	UAnimSequence* GetCurrentIdleAnim() const;

	// Picks between a Lyra-set anim and its Synty-set counterpart based on
	// CurrentLocomotionSet, falling back to LyraAnim whenever the Synty
	// slot is unset (e.g. before SyntyLocomotionSet is fully populated) --
	// every core-locomotion call site in UpdateLocomotionState goes through
	// this instead of reading Owner->WalkAnim etc. directly, so toggling
	// ECharacterLocomotionSet is the ONLY thing that needs to change to
	// swap movesets, without restructuring the Lyra properties at all.
	UAnimSequence* PickAnim(UAnimSequence* LyraAnim, UAnimSequence* SyntyAnim) const;

	// Written anytime by SetLookAtTarget/ClearLookAtTarget (game thread, but
	// with no ordering guarantee relative to a still-running Evaluate() for
	// a previous frame); snapshotted into bHasLookAtTarget/LookAtTargetWorld
	// once per frame at the top of UpdateLookAt (PreUpdate, game thread) so
	// Evaluate() only ever sees a value frozen for its own frame.
	bool bPendingHasLookAtTarget = false;
	FVector PendingLookAtTargetWorld = FVector::ZeroVector;

	bool bHasLookAtTarget = false;
	// The eased point the head actually tracks (see UpdateLookAt).
	bool bLookAtSmoothedValid = false;
	FRotator SmoothedLookAtRotation = FRotator::ZeroRotator;
	FVector LookAtTargetWorld = FVector::ZeroVector;
	float LookAtWeight = 0.0f;
	FName LookAtBoneName = NAME_None;
	// Actor forward in mesh component space, cached in PreUpdate (see there).
	FVector CachedFacingComponent = FVector(0.0f, 1.0f, 0.0f);

	// The owning component's world transform, cached during PreUpdate (game
	// thread, safe) for use during Evaluate() (may run off the game thread)
	// to convert LookAtWorldTarget into the component space Evaluate()
	// actually works in.
	FTransform CachedComponentToWorld = FTransform::Identity;

	// Snapshots of Owner's JawHingeDeg/EyesHingeDeg/EyesBlinkScale/
	// BrowHeightCm/BrowAngleDeg, taken once per frame in PreUpdate.
	// AFaceController writes those UPROPERTYs directly, at arbitrary times,
	// with no coordination with anim evaluation -- reading Owner->JawHingeDeg
	// etc. straight from ApplyFacialBoneEdits (which, like the rest of
	// Evaluate(), can run on an animation worker thread overlapping a later
	// frame's game-thread write) is the same class of race that caused the
	// look-at spiral bug. Cheap to close off here too rather than leave a
	// second copy of the same landmine.
	float CachedJawHingeDeg = 0.0f;
	float CachedEyesHingeDeg = 0.0f;
	float CachedEyesBlinkScale = 1.0f;
	float CachedBrowHeightCm = 0.0f;
	float CachedBrowAngleDeg = 0.0f;

	// Cached each PreUpdate so Evaluate() (which may run off the game
	// thread) can read animation assets/bone names/tuning values without
	// touching the owning UAnimInstance directly.
	const UCharacterAnimInstance* Owner = nullptr;
};

UCLASS()
class REPLICAN_API UCharacterAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:

	UCharacterAnimInstance();

	// Populates AllCombatAnims via the Asset Registry -- deliberately NOT
	// done in the constructor alongside AttackAnims/PoseAnims's
	// ConstructorHelpers::FObjectFinder loops: Epic's own guidance is that
	// arbitrary Asset Registry queries/loads during CDO construction are
	// less safe than ConstructorHelpers's specific reentrant-safe path
	// (construction order during module/editor startup isn't guaranteed
	// the same way). NativeInitializeAnimation is UAnimInstance's standard
	// "fully initialized, safe to do real work" hook, called once per
	// instance well after that startup window has passed.
	virtual void NativeInitializeAnimation() override;

	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

	// Called every tick by the owning Character -- see FLocomotionInputs.
	void UpdateLocomotion(const FLocomotionInputs& In)
	{
		GetProxyOnGameThread<FCharacterAnimInstanceProxy>().UpdateLocomotionInputs(In);
	}

	// Makes the head bone continuously turn to face WorldPoint. If it ever
	// falls outside the LookAtMaxYawDegrees/LookAtMaxPitchDegrees range of the
	// owning actor's forward direction, the head eases back to its normal
	// animated position.
	void SetLookAtTarget(const FVector& WorldPoint) { GetProxyOnGameThread<FCharacterAnimInstanceProxy>().SetLookAtTarget(WorldPoint); }
	void ClearLookAtTarget() { GetProxyOnGameThread<FCharacterAnimInstanceProxy>().ClearLookAtTarget(); }

	// Plays RollAnim once, then returns to whatever locomotion state fits the
	// movement input by the time it finishes. Unlike every other clip this
	// project uses, RollAnim has REAL baked root motion (confirmed via a
	// properly root-motion-authored source clip -- see the header comment on
	// RollAnim) that ABaseCharacter::UpdateRollMovement extracts every frame
	// and applies directly to the actor, instead of this project's usual
	// "CharacterMovementComponent is the only thing that ever moves the
	// character" convention. That extraction is done by ABaseCharacter
	// calling RollAnim->ExtractRootMotionFromRange(...) directly -- a pure,
	// stateless query against the asset itself, deliberately NOT routed
	// through this proxy's own ToTime bookkeeping (an earlier version did
	// that and it was a real bug: ToTime is advanced by the mesh
	// COMPONENT's own tick, which has no guaranteed order relative to the
	// owning Character's Tick(), so reading it from ABaseCharacter::Tick()
	// could see a stale value from a frame ago). ABaseCharacter tracks the
	// roll's own elapsed time itself instead, sidestepping that entirely.
	void TriggerRoll() { GetProxyOnGameThread<FCharacterAnimInstanceProxy>().TriggerRoll(); }

	// See the proxy's TriggerTurnInPlace / ConsumeRootMotion.
	UFUNCTION(BlueprintCallable, Category = "Locomotion")
	void TriggerTurnInPlace(float DeltaYawDegrees) { GetProxyOnGameThread<FCharacterAnimInstanceProxy>().TriggerTurnInPlace(DeltaYawDegrees); }
	void TriggerSprintSlide() { GetProxyOnGameThread<FCharacterAnimInstanceProxy>().TriggerSprintSlide(); }
	void AbortAttack() { GetProxyOnGameThread<FCharacterAnimInstanceProxy>().AbortAttack(); }
	void ConsumeRootMotion(float& OutYawDeg, FVector& OutMeshTranslation) { GetProxyOnGameThread<FCharacterAnimInstanceProxy>().ConsumeRootMotion(OutYawDeg, OutMeshTranslation); }
	UFUNCTION(BlueprintPure, Category = "Locomotion")
	bool IsClipDrivingRotation() const { return GetProxyOnGameThread<FCharacterAnimInstanceProxy>().IsClipDrivingRotation(); }
	UFUNCTION(BlueprintPure, Category = "Locomotion")
	bool IsClipDrivingTranslation() const { return GetProxyOnGameThread<FCharacterAnimInstanceProxy>().IsClipDrivingTranslation(); }

	// Same real-root-motion approach as RollAnim, for whichever of the 4
	// DashXAnim clips ABaseCharacter::StartDash picks based on current WASD
	// input -- ABaseCharacter::UpdateDashMovement extracts and applies it
	// directly, same as UpdateRollMovement does for RollAnim.
	void TriggerDash(ECharacterDashDirection Direction) { GetProxyOnGameThread<FCharacterAnimInstanceProxy>().TriggerDash(Direction); }

	// Steps to the next pose in PoseAnims and blends into it, holding there
	// (looping) until real movement/action input arrives -- see
	// FCharacterAnimInstanceProxy::UpdateLocomotionState's Pose-hold branch
	// for the exact "what counts as real input" condition.
	void TriggerPoseCycle() { GetProxyOnGameThread<FCharacterAnimInstanceProxy>().TriggerPoseCycle(); }

	// Steps to the next entry in IdleVariants -- see TriggerIdleVariantCycle's
	// header comment on FCharacterAnimInstanceProxy for the immediate-vs-
	// next-time-idle distinction.
	void CycleIdleVariant() { GetProxyOnGameThread<FCharacterAnimInstanceProxy>().TriggerIdleVariantCycle(); }

	// Swaps the whole Idle/Walk/Run/Crouch/Jump moveset between Lyra and
	// Synty -- see ECharacterLocomotionSet's header comment. UFUNCTION so
	// this can be driven directly from Python remote-exec during PIE for
	// diagnosis, same reasoning as GetCurrentLocomotionSet.
	UFUNCTION(BlueprintCallable, Category = "Debug")
	void ToggleLocomotionSet() { GetProxyOnGameThread<FCharacterAnimInstanceProxy>().ToggleLocomotionSet(); }

	// Returns whichever of DashForwardAnim/DashBackwardAnim/DashLeftAnim/
	// DashRightAnim matches Direction -- the single place that mapping
	// lives, so ABaseCharacter::UpdateDashMovement doesn't need its own
	// switch on the enum.
	UAnimSequence* GetDashAnim(ECharacterDashDirection Direction) const;

	// Plays Anim once, then falls back to normal locomotion -- see
	// TriggerAttack's header comment on FCharacterAnimInstanceProxy.
	void TriggerAttack(UAnimSequence* Anim, bool bLoop = false) { GetProxyOnGameThread<FCharacterAnimInstanceProxy>().TriggerAttack(Anim, bLoop); }
	// Ends a held (looping) attack clip -- see the proxy's TriggerAttack.
	void ReleaseAttackHold() { GetProxyOnGameThread<FCharacterAnimInstanceProxy>().ReleaseAttackHold(); }

	// AttackAnims[Index], clamped -- lets ABaseCharacter::StartAttack read
	// the clip's own length to know how long to block movement input for,
	// without duplicating index-clamping logic on both sides.
	UAnimSequence* GetAttackAnim(int32 Index) const { return AttackAnims.IsValidIndex(Index) ? AttackAnims[Index] : nullptr; }

	// AllCombatAnims.Num()/[Index], clamped -- the full Synty Sword Combat
	// pack (every clip under /Game/Characters/Animations/SyntySwordCombat,
	// ~118 total, unfiltered), enumerated once at construction via the
	// Asset Registry rather than hardcoded like AttackAnims's curated 9 --
	// see the constructor for why a registry query fits better than 118
	// ConstructorHelpers::FObjectFinder lines. Backs ABaseCharacter's
	// '['/']' full-list browse-and-preview tool (AttackAnims/number-keys
	// 1-9 are unaffected, still their own separate, curated, always-usable
	// set).
	int32 GetNumAllCombatAnims() const { return AllCombatAnims.Num(); }
	UAnimSequence* GetAllCombatAnim(int32 Index) const { return AllCombatAnims.IsValidIndex(Index) ? AllCombatAnims[Index] : nullptr; }

	// See GetCurrentLocomotionSet's header comment on FCharacterAnimInstanceProxy.
	// UFUNCTION (not just a plain method, unlike most of this class) so it's
	// reachable from Python remote-exec for live diagnosis during PIE --
	// self-verifying a play-rate/state fix beats waiting on a relayed report.
	UFUNCTION(BlueprintCallable, Category = "Debug")
	ECharacterLocomotionSet GetCurrentLocomotionSet() const { return GetProxyOnGameThread<FCharacterAnimInstanceProxy>().GetCurrentLocomotionSet(); }

	// See GetCurrentAnim's header comment on FCharacterAnimInstanceProxy.
	UFUNCTION(BlueprintCallable, Category = "Debug")
	UAnimSequence* GetCurrentAnim() const { return GetProxyOnGameThread<FCharacterAnimInstanceProxy>().GetCurrentAnim(); }
	UFUNCTION(BlueprintPure, Category = "Debug")
	float GetGaitRateMultiplier() const { return GetProxyOnGameThread<FCharacterAnimInstanceProxy>().GetGaitRateMultiplier(); }
	UFUNCTION(BlueprintPure, Category = "Debug")
	float GetGaitStanceBlend() const { return GetProxyOnGameThread<FCharacterAnimInstanceProxy>().GetGaitStanceBlend(); }

	// See GetCurrentPlayRate's header comment on FCharacterAnimInstanceProxy.
	UFUNCTION(BlueprintCallable, Category = "Debug")
	float GetCurrentPlayRate() const { return GetProxyOnGameThread<FCharacterAnimInstanceProxy>().GetCurrentPlayRate(); }

	// AttackAnims.Num() -- lets ABaseCharacter's P-cycle preview wrap its
	// index without needing its own copy of the array.
	int32 GetNumAttackAnims() const { return AttackAnims.Num(); }

	// Plain public fields, written directly by AFaceController each time a
	// facial control changes -- no setter functions needed since these are
	// just data the proxy reads every PreUpdate/Evaluate, same shape as the
	// old UFaceAnimInstance's properties (so migrating FaceController to
	// this class was just a pointer-type change, not a rewrite).
	UPROPERTY(BlueprintReadWrite, Category = "Face")
	float JawHingeDeg = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Face")
	float EyesHingeDeg = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Face")
	float EyesBlinkScale = 1.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Face")
	float BrowHeightCm = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Face")
	float BrowAngleDeg = 0.0f;

	// Bone this project's Mannequin-derived skeletons use for each facial
	// control -- editable per-instance in case a future NPC rig names them
	// differently.
	UPROPERTY(EditAnywhere, Category = "Face")
	FName JawBoneName = TEXT("jaw");

	UPROPERTY(EditAnywhere, Category = "Face")
	FName EyesBoneName = TEXT("eyes");

	UPROPERTY(EditAnywhere, Category = "Face")
	FName EyebrowsBoneName = TEXT("eyebrows");

	// Additive curl applied on top of whatever the base animation already
	// has for the right hand's finger bones -- ABaseCharacter::WeaponMesh
	// is attached rigidly to hand_r, but every existing Idle/Walk/Run clip
	// this project uses is the "Unarmed" variant (open hand), so without
	// this the weapon would visibly float in an open palm instead of
	// looking gripped. 0 disables the correction entirely (open hand,
	// weapon unequipped look); 1 is full curl. Kept as ONE shared weight
	// (not per-finger) since there's no reason yet to blend them
	// independently -- either the weapon is in-hand or it isn't.
	UPROPERTY(EditAnywhere, Category = "Weapon Grip", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WeaponGripWeight = 1.0f;

	// Degrees each of index_01/02/03_r and middle_01/02/03_r curl inward
	// (same amount at every joint -- a real hand curls more at the base
	// knuckle than the tip, but a uniform curl is a reasonable first pass
	// for a fixed grip pose, not a fully articulated hand). This project
	// has no ring/pinky bones on the shared Mannequin skeleton (see
	// KEEP_BONES in clean_to_ue4_mannequin.py), so index+middle doing the
	// gripping is what's actually available. The rotation AXIS (local Z)
	// is an unverified first guess, same as every other bone-hinge
	// correction in this class (JawHingeDeg etc.). The SIGN was flipped
	// from an original +60 after live feedback: +60 curled the fingers
	// backwards (hyperextending away from the palm) while
	// ThumbCurlDegrees's already-negative sign on the same axis looked
	// reasonable, so this now matches that same sign convention.
	UPROPERTY(EditAnywhere, Category = "Weapon Grip")
	float FingerCurlDegrees = -60.0f;

	// Thumb curls the OPPOSING direction from the fingers (wraps around the
	// other side of a gripped cylinder, toward the palm) -- same uniform-
	// per-joint simplification and same "tune by eye" caveat as
	// FingerCurlDegrees.
	UPROPERTY(EditAnywhere, Category = "Weapon Grip")
	float ThumbCurlDegrees = 40.0f;

	// Which bones the curl applies to. Defaults are the trimmed Mannequin
	// rig this project's own characters use (index + middle, no ring/pinky);
	// the Modular Fantasy Hero rig names its fingers indexFinger_0N /
	// finger_0N instead, so ABaseCharacter swaps these when it assembles
	// one (see ABaseCharacter::EnterModularMode).
	UPROPERTY(EditAnywhere, Category = "Weapon Grip")
	TArray<FName> GripFingerBones = { TEXT("index_01_r"), TEXT("index_02_r"), TEXT("index_03_r"), TEXT("middle_01_r"), TEXT("middle_02_r"), TEXT("middle_03_r") };

	UPROPERTY(EditAnywhere, Category = "Weapon Grip")
	TArray<FName> GripThumbBones = { TEXT("thumb_01_r"), TEXT("thumb_02_r"), TEXT("thumb_03_r") };

	// Foot grounding: after everything else, shift the whole body so the
	// lowest toe/ball bone sits at the mesh's reference-pose sole height
	// whenever the character is on the ground. Every clip carries a
	// slightly different floor height (measured 0-2cm apart between the
	// Synty idle, crouch and jog, more after a cross-rig retarget), which
	// is what "floats standing, flush crouched" was. Off in the air.
	UPROPERTY(EditAnywhere, Category = "Feet")
	bool bGroundFeet = true;

	UPROPERTY(EditAnywhere, Category = "Feet")
	float GroundFeetMaxShift = 20.0f;

	// "Carry" pose for the weapon arm. UE has no built-in weapon-vs-body
	// avoidance; the standard fix is a per-bone layered blend -- the
	// locomotion clip drives the body, and a held pose drives the arm chain
	// that's carrying something, so a jog's arm pumping (which was driving
	// the sword through the face) never reaches that arm. This is that
	// layer, done directly on the compact pose in ApplyArmOverride: every
	// bone at or below each ArmOverrideRootBones entry is blended toward
	// this clip's LOCAL-space bone transforms by ArmOverrideWeight. Local
	// space, so the overridden arm still rides the running torso -- only
	// the arm's own swing is replaced. Applied during locomotion states
	// only; attacks/poses/rolls/dashes already author their own arms and
	// the alpha eases out over ArmOverrideBlendSeconds when they start.
	// Default: the Sword Combat set's base idle, i.e. "standing holding a
	// sword", on clavicle_r downward. Null disables the layer.
	UPROPERTY(EditAnywhere, Category = "Weapon Grip")
	TObjectPtr<UAnimSequence> ArmOverridePose;

	UPROPERTY(EditAnywhere, Category = "Weapon Grip", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ArmOverrideWeight = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Weapon Grip")
	TArray<FName> ArmOverrideRootBones = { TEXT("clavicle_r") };

	UPROPERTY(EditAnywhere, Category = "Weapon Grip")
	float ArmOverrideBlendSeconds = 0.25f;

	// The pose clip keeps playing (looping) rather than freezing on frame
	// 0, so the held arm still has the idle's slight breathing motion
	// instead of being rigidly locked.
	UPROPERTY(EditAnywhere, Category = "Weapon Grip")
	bool bArmOverrideAnimates = true;

	// A held IDLE loops; a one-shot -- a shot fired, a magazine changed, a
	// weapon drawn -- must not, or the character reloads forever. The layer
	// holds the clip's last frame when this is false, and whoever started
	// the action is responsible for putting the idle back.
	UPROPERTY(EditAnywhere, Category = "Weapon Grip")
	bool bArmOverrideLoops = true;

	// Bumped every time SetArmOverride changes what is playing, so the proxy
	// knows to restart the clip from zero rather than continuing at whatever
	// time the previous clip had reached. Reading the pose pointer alone is
	// not enough: firing twice in a row is the same clip and still needs to
	// start again.
	UPROPERTY(Transient)
	int32 ArmOverrideSerial = 0;

	// How long the outgoing arm clip takes to fade into the incoming one on a swap.
	UPROPERTY(EditAnywhere, Category = "Weapon Grip", meta = (ClampMin = "0.0"))
	float ArmOverrideSwapSeconds = 0.18f;

	// ---- Torso lean -------------------------------------------------------------------------
	// Written by the character every tick. The spine bones share SpineLeanFraction of the aim
	// pitch between them, scaled by SpineLeanWeight (0 unarmed, 1 armed, eased by the owner).
	UPROPERTY(Transient) float AimPitchDegrees = 0.0f;
	// True while the body is shouldered or on the sights: the head goes STRAIGHT to the aim,
	// no easing -- when the weapon is up, the head is the aim.
	UPROPERTY(Transient) bool bLookAtLocked = false;
	UPROPERTY(Transient) FVector AimForwardWorld = FVector::ForwardVector;
	UPROPERTY(Transient) float SpineLeanWeight = 0.0f;
	UPROPERTY(Transient) float SpineLeanFraction = 0.35f;
	UPROPERTY(EditAnywhere, Category = "Weapon Grip")
	TArray<FName> SpineLeanBones = { TEXT("spine_02"), TEXT("spine_03") };

	// The one way to drive the layer. Roots come from the weapon's stance
	// (WeaponCatalog::StanceRoots), so a two-handed rifle turns the torso
	// from spine_01 while a pistol only takes the right arm -- declared in
	// UI/Weapons.json, never branched on here.
	UFUNCTION(BlueprintCallable, Category = "Weapon Grip")
	void SetArmOverride(UAnimSequence* Pose, const TArray<FName>& Roots, float Weight, bool bLoops);

	// ---- Hand IK -------------------------------------------------------------
	// The hands follow the WEAPON, rather than the weapon following the hands. That inversion is
	// the only way one animation can serve a hundred guns of different lengths: the animation
	// says how the body stands, and two-bone IK puts each hand on the grip that gun actually
	// has. See Docs/HeldAssetStandard.md and the fore_grip field in UI/Weapons.json.
	//
	// Targets are WORLD transforms because that is what the owning character can compute from
	// the weapon's own component; the proxy converts them once per evaluation.
	UPROPERTY(Transient) FTransform HandIKTargetR = FTransform::Identity;
	UPROPERTY(Transient) FTransform HandIKTargetL = FTransform::Identity;
	UPROPERTY(Transient) float HandIKWeightR = 0.0f;
	// How far the elbow is pulled toward "down and a little out" from the pose's own bend plane (0 = the pose's elbow): a pistol's arms out in front want it.
	// A FULL-BODY ACTION (a sword swing from the root): the layers that shape the pose for play --
	// gait, spine lean, look-at, the grip correction, the hand IK -- stand aside so the clip is
	// seen as authored. The character sets it with the clip and clears it when the clip ends.
	UPROPERTY(Transient) bool bFullBodyAction = false;
	UPROPERTY(Transient) float ElbowDownBiasR = 0.0f;
	UPROPERTY(Transient) float ElbowDownBiasL = 0.0f;
	UPROPERTY(Transient) float HandIKWeightL = 0.0f;

	// How far past straight the arm is allowed to reach. Never 1: an arm locked dead straight
	// reads as broken, and floating-point noise at full extension makes the elbow flicker.
	UPROPERTY(EditAnywhere, Category = "Hand IK", meta = (ClampMin = "0.5", ClampMax = "1.0"))
	float HandIKMaxReach = 0.985f;

	UPROPERTY(EditAnywhere, Category = "Hand IK")
	FName IKUpperArmR = TEXT("upperarm_r");
	UPROPERTY(EditAnywhere, Category = "Hand IK")
	FName IKLowerArmR = TEXT("lowerarm_r");
	UPROPERTY(EditAnywhere, Category = "Hand IK")
	FName IKHandR = TEXT("hand_r");
	UPROPERTY(EditAnywhere, Category = "Hand IK")
	FName IKUpperArmL = TEXT("upperarm_l");
	UPROPERTY(EditAnywhere, Category = "Hand IK")
	FName IKLowerArmL = TEXT("lowerarm_l");
	UPROPERTY(EditAnywhere, Category = "Hand IK")
	FName IKHandL = TEXT("hand_l");

	// Bone driven by SetLookAtTarget.
	UPROPERTY(EditAnywhere, Category = "Look At")
	FName HeadBoneName = TEXT("head");

	// The yaw/pitch range check is against the TORSO's actual current pose
	// (see ShoulderLeftBoneName/ShoulderRightBoneName below), not the
	// capsule's forward vector -- bOrientRotationToMovement only turns the
	// CAPSULE to face movement direction, and the currently-playing pose's
	// spine/shoulders don't always agree with that exactly (mid-blend
	// between two states, Roll/Dash's frozen mesh rotation, or just the
	// natural lag of RotationRate-limited turning while the legs are
	// already moving a new direction) -- confirmed as a real, reported
	// mismatch: the head visibly turning further than it should whenever
	// the torso was rotated out of alignment with the capsule.
	UPROPERTY(EditAnywhere, Category = "Look At")
	FName ShoulderLeftBoneName = TEXT("clavicle_l");

	UPROPERTY(EditAnywhere, Category = "Look At")
	FName ShoulderRightBoneName = TEXT("clavicle_r");

	// Independent yaw/pitch gaze limits, each a HALF-angle either side of the
	// torso's own forward direction (so 80 here means a 160-degree total pan
	// range) -- a single circular cone can't represent a neck's natural
	// range, which pans further than it tilts, so yaw and pitch are checked
	// separately rather than as one angle-from-forward test. Beyond either
	// limit the target is treated as unreachable and the head eases back to
	// its normal animated position.
	// These are HARD limits on how far the head may turn away from its
	// neutral position on the neck (ApplyLookAt clamps the look direction to
	// them, and never adds roll), as well as the range gate that decides
	// whether a target is worth turning toward at all.
	UPROPERTY(EditAnywhere, Category = "Look At", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float LookAtMaxYawDegrees = 70.0f;

	UPROPERTY(EditAnywhere, Category = "Look At", meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float LookAtMaxPitchDegrees = 45.0f;

	// How far past the limits a target may sit and still be "tried" (the
	// head turns as far as it can toward it) before it is ignored instead.
	UPROPERTY(EditAnywhere, Category = "Look At", meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float LookAtGateSlackDegrees = 15.0f;

	// How fast the look-at blend weight ramps toward its target value each
	// second -- smooths engaging/releasing/losing-the-cone instead of a hard
	// snap.
	UPROPERTY(EditAnywhere, Category = "Look At", meta = (ClampMin = "0.01"))
	float LookAtBlendSpeed = 3.0f;
	// How fast the look-at point itself moves between targets (exponential
	// ease, per second). Lower = a slower, heavier turn of the head.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look At")
	float LookAtTurnSpeed = 4.0f;

	UPROPERTY(EditAnywhere, Category = "Animation")
	TObjectPtr<UAnimSequence> IdleAnim;

	// Idle variety, cycled through with CycleIdleVariant() (bound to 'I') --
	// both are stationary "break the idle loop" moments (a breathing-room
	// pose shift and a fidget respectively) rather than alternate LOOPING
	// idles per se, but looped the same way IdleAnim already is here since
	// there's no separate "play once then return to IdleAnim" plumbing yet
	// and the user's ask was simply "toggle between idles."
	UPROPERTY(EditAnywhere, Category = "Animation")
	TObjectPtr<UAnimSequence> IdleBreakAnim;

	UPROPERTY(EditAnywhere, Category = "Animation")
	TObjectPtr<UAnimSequence> IdleBreakFidgetAnim;

	UPROPERTY(EditAnywhere, Category = "Animation")
	TObjectPtr<UAnimSequence> WalkAnim;

	UPROPERTY(EditAnywhere, Category = "Animation")
	TObjectPtr<UAnimSequence> RunAnim;

	UPROPERTY(EditAnywhere, Category = "Animation")
	TObjectPtr<UAnimSequence> JumpRiseAnim;

	UPROPERTY(EditAnywhere, Category = "Animation")
	TObjectPtr<UAnimSequence> JumpFallAnim;

	UPROPERTY(EditAnywhere, Category = "Animation")
	TObjectPtr<UAnimSequence> JumpEndAnim;

	UPROPERTY(EditAnywhere, Category = "Animation", meta = (ClampMin = "0.1"))
	float JumpEndPlayRate = 1.5f;

	UPROPERTY(EditAnywhere, Category = "Animation")
	TObjectPtr<UAnimSequence> CrouchIdleAnim;

	UPROPERTY(EditAnywhere, Category = "Animation")
	TObjectPtr<UAnimSequence> CrouchWalkAnim;

	// The Synty Base Locomotion pack, retargeted onto this project's shared
	// Mannequin skeleton this session (see project notes on the
	// PolygonSource -> Mannequin IK Retargeter) -- toggled in as a whole
	// via ToggleLocomotionSet ('O'), alongside (not replacing) the Lyra
	// properties above. Populated via ConstructorHelpers in the .cpp, same
	// pattern as everything else in this class.
	UPROPERTY(EditAnywhere, Category = "Animation|Synty Locomotion")
	FLocomotionAnimSet SyntyLocomotionSet;

	// The pack's Feminine set (A_*_Femn) and the Goblin Locomotion pack
	// (A_POLY_GBL_*_Neut), both retargeted through the same
	// RTG_PolygonSource_To_Mannequin staging as the Masculine one. Any slot
	// a set lacks is filled from the Masculine set at construction, so
	// switching sets never drops a state. Selected per character by
	// LocomotionChoice (FCharacterConfig::Locomotion, else its Gender).
	UPROPERTY(EditAnywhere, Category = "Animation|Synty Locomotion")
	FLocomotionAnimSet SyntyFeminineLocomotionSet;

	UPROPERTY(EditAnywhere, Category = "Animation|Synty Locomotion")
	FLocomotionAnimSet SyntyGoblinLocomotionSet;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation|Synty Locomotion")
	ESyntyLocomotionChoice LocomotionChoice = ESyntyLocomotionChoice::Male;

	// Runtime posture/timing layer over the locomotion clips (see
	// GaitAdjustments.h); mirrored from FCharacterConfig::Gait by the
	// character and snapshotted by the proxy each PreUpdate.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation|Gait")
	FGaitAdjustments Gait;

	// The Synty set the proxy should read this frame.
	const FLocomotionAnimSet& GetActiveSyntySet() const
	{
		switch (LocomotionChoice)
		{
		case ESyntyLocomotionChoice::Female: return SyntyFeminineLocomotionSet.IdleAnim ? SyntyFeminineLocomotionSet : SyntyLocomotionSet;
		case ESyntyLocomotionChoice::Goblin: return SyntyGoblinLocomotionSet.IdleAnim ? SyntyGoblinLocomotionSet : SyntyLocomotionSet;
		default: return SyntyLocomotionSet;
		}
	}

	UPROPERTY(EditAnywhere, Category = "Animation")
	float WalkSpeedThreshold = 10.0f;

	// No Run/Jog/CrouchRun thresholds here on purpose -- tiers are chosen
	// by nearest configured speed from the values ABaseCharacter passes in
	// each tick (see FLocomotionInputs), so they can't drift out of step
	// with the movement tuning.

	// Default blend duration for transitions that aren't the specific
	// standing<->crouch cases below.
	UPROPERTY(EditAnywhere, Category = "Animation", meta = (ClampMin = "0.01"))
	float AnimBlendDuration = 0.35f;

	UPROPERTY(EditAnywhere, Category = "Animation", meta = (ClampMin = "0.01"))
	float CrouchDownBlendDuration = 0.25f;

	UPROPERTY(EditAnywhere, Category = "Animation", meta = (ClampMin = "0.01"))
	float StandUpBlendDuration = 0.3f;

	// Forward roll/dodge, imported from FreeSampleAnimationSet's
	// DashDodgeRollSet teaser (A_Roll_IdleFwd) -- unlike the free
	// "DynamicFalling" pack tried first (its only referencer in the source
	// project was a Sequencer showcase cinematic, never any gameplay
	// ability, and its head/body swing dramatically further than its root
	// bone moves, confirmed via live bone-position logging), this clip is
	// properly authored for real gameplay use: its root bone carries the
	// clip's entire ~4.6m of travel while the head stays essentially fixed
	// relative to it throughout (also confirmed live) -- exactly the
	// "author's intended mode of use" for this class of animation. Bone-
	// stripped onto this project's shared UE4_Mannequin_Skeleton the same
	// way every other imported clip here was, but WITHOUT zeroing root
	// translation (see clean_to_ue4_mannequin.py's "keep-root-motion" flag)
	// and with enable_root_motion left on, since that translation is exactly
	// what ABaseCharacter::UpdateRollMovement extracts and applies.
	// Only a forward roll for now -- the full commercial pack this teases
	// presumably has proper Dash/Dodge variants and other directions, but
	// the free sample only bundles this one clip.
	UPROPERTY(EditAnywhere, Category = "Animation|Roll")
	TObjectPtr<UAnimSequence> RollAnim;

	// Blend duration for both edges of a roll -- quick, since a dodge should
	// read as immediate rather than eased like ordinary locomotion
	// transitions. No longer has to hide a physics/animation distance
	// mismatch the way it did before switching to real root motion (root
	// motion makes the capsule move exactly as the clip dictates, by
	// construction), so this is purely a feel choice now.
	UPROPERTY(EditAnywhere, Category = "Animation|Roll", meta = (ClampMin = "0.01"))
	float RollBlendDuration = 0.15f;

	// The 4 real Lyra Dash clips (Forward/Backward/Left/Right -- the other 6
	// files in Lyra's Dash folder are single-frame loading-screen stills,
	// not usable animations, see project notes). Each carries its own real
	// baked root motion the same way RollAnim does (bone-stripped with
	// "keep-root-motion", no root-bone translation zeroed) -- ABaseCharacter
	// ::UpdateDashMovement extracts whichever one ABaseCharacter::StartDash
	// picked, using the identical ExtractRootMotionFromRange approach
	// already proven out on RollAnim.
	UPROPERTY(EditAnywhere, Category = "Animation|Dash")
	TObjectPtr<UAnimSequence> DashForwardAnim;

	UPROPERTY(EditAnywhere, Category = "Animation|Dash")
	TObjectPtr<UAnimSequence> DashBackwardAnim;

	UPROPERTY(EditAnywhere, Category = "Animation|Dash")
	TObjectPtr<UAnimSequence> DashLeftAnim;

	UPROPERTY(EditAnywhere, Category = "Animation|Dash")
	TObjectPtr<UAnimSequence> DashRightAnim;

	UPROPERTY(EditAnywhere, Category = "Animation|Dash", meta = (ClampMin = "0.01"))
	float DashBlendDuration = 0.15f;

	// On-demand poses (bound to 'P', see UCharacterAnimInstance::
	// TriggerPoseCycle) -- the real "Poses" folder Lyra ships turned out to
	// contain zero usable character animations (all single-bone rig-
	// calibration clips or static cinematic block-out holds, confirmed by
	// direct inspection), so these are Lyra's SplashPose_* set instead: real
	// full-body held poses meant for marketing splash screens, which is
	// exactly the "look good held indefinitely" property an on-demand pose
	// needs. Populated via ConstructorHelpers in the .cpp (17 individually
	// verbose but consistent with every other animation reference in this
	// class) rather than set on the CDO via Python post-import -- a native
	// C++ class's CDO has no backing asset for Python property edits to
	// persist into, unlike a placed level actor's own properties (which
	// save into the level) or a Blueprint's generated class defaults, so a
	// Python-only assignment would silently revert to empty on the next
	// editor restart.
	UPROPERTY(EditAnywhere, Category = "Animation|Pose")
	TArray<TObjectPtr<UAnimSequence>> PoseAnims;

	UPROPERTY(EditAnywhere, Category = "Animation|Pose", meta = (ClampMin = "0.01"))
	float PoseBlendDuration = 0.25f;

	// The 9 sword clips bound to number keys 1-9 (see ABaseCharacter::
	// StartAttack) -- the plain in-place "_Sword" variants from Synty's
	// Sword Combat pack (not the "_RootMotion"/"_ReturnToIdle" ones), so
	// these need no root-motion extraction of their own: CharacterMovementComponent
	// keeps driving actual movement while the clip plays, same convention as
	// every non-Roll/Dash animation in this project.
	UPROPERTY(EditAnywhere, Category = "Animation|Attack")
	TArray<TObjectPtr<UAnimSequence>> AttackAnims;

	UPROPERTY(EditAnywhere, Category = "Animation|Attack", meta = (ClampMin = "0.01"))
	float AttackBlendDuration = 0.1f;

	// Built in the constructor from IdleAnim/IdleBreakAnim/IdleBreakFidgetAnim/
	// SyntyLocomotionSet.IdleAnim -- FCharacterAnimInstanceProxy::
	// GetCurrentIdleAnim indexes into this with its own
	// CurrentIdleVariantIndex. Deliberately ONE shared list regardless of
	// which ECharacterLocomotionSet is active ('I' cycles through all known
	// idles; 'O' only affects Walk/Run/Crouch/Jump) -- not EditAnywhere,
	// since it's derived from those properties, not independent data.
	TArray<TObjectPtr<UAnimSequence>> IdleVariants;

	// Every UAnimSequence under /Game/Characters/Animations/SyntySwordCombat
	// (Attack/Block/Death/Dodge/Hit/Idle, ~118 total, unfiltered -- root-
	// motion and ReturnToIdle variants included, unlike AttackAnims's
	// curated 9), enumerated once via the Asset Registry in the constructor.
	// Not EditAnywhere: this is meant to always reflect the whole pack, not
	// something to hand-curate per instance -- see ABaseCharacter's
	// '['/']' browse tool.
	TArray<TObjectPtr<UAnimSequence>> AllCombatAnims;
};
