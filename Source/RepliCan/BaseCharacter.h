// Playable third-person character with a camera that zooms continuously
// between third-person and first-person, and Enhanced Input bindings for
// WASD movement, mouse look, scroll-wheel zoom, and jump.
//
// No Blueprint subclass -- the Input Mapping Context / Input Action
// references below are set per-instance via Python on a level-placed
// actor (AutoPossessPlayer = Player0), the same pattern already used for
// every other per-instance asset reference in this project (HighlightMesh,
// NoseMesh, etc.), since Blueprint graph editing isn't reliably scriptable
// through this project's remote-exec workflow.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CharacterAnimInstance.h" // for ECharacterDashDirection, stored by value below
#include "CharacterConfig.h"
#include "CombatAnimLibrary.h"
#include "BaseCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class UStaticMesh;
class UStaticMeshComponent;
class UWidgetComponent;
class UCharacterBuilderWidget;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class USkeletalMesh;
class AFaceController;
class ACharacterGhostActor;
struct FInputActionValue;
enum class ESyntyLocomotionChoice : uint8;

// Four ways to hold a gun. They are all the SAME SOLVE with a different offset from the eye,
// which is the whole reason this is an enum and not four code paths:
//
//   ADS         rear sight on the eye line, at eye relief. Looking through the sights.
//   Shouldered  still pointed at the target, but held down and to the right of the eye -- a
//               right-handed shooter with his cheek off the stock. Fast, less precise.
//   HipFire     lower and further right again, barely raised.
//   LowReady    muzzle depressed. Not aimed at anything, and safe to walk with.
//
// The first three all keep the POINT OF AIM on the target: the weapon is displaced from the
// eye but rotated so its barrel still converges on what the reticle is over. Only low ready
// breaks that, because breaking it is what low ready is.
UENUM(BlueprintType)
enum class EWeaponCarry : uint8
{
	LowReady, HipFire, Shouldered, ADS
};

// A blend with a duration. FInterpTo is a speed: it closes a fraction of the gap each frame,
// never arrives, and lands somewhere different at 30 fps than at 120. The carry system already
// moves the weapon over a configured number of seconds with an ease; this gives the sight
// alignment, the hand IK and the torso lean the same contract.
struct FTimedBlend
{
	float Value = 0.0f, From = 0.0f, To = 0.0f, Total = 0.0f, Left = 0.0f;
	void Set(float Target, float Seconds)
	{
		if (FMath::IsNearlyEqual(Target, To)) { return; }
		From = Value; To = Target; Total = FMath::Max(Seconds, 0.0001f); Left = Total;
	}
	void Snap(float Target) { Value = From = To = Target; Left = 0.0f; }
	void Tick(float Dt)
	{
		if (Left <= 0.0f) { Value = To; return; }
		Left = FMath::Max(0.0f, Left - Dt);
		Value = FMath::Lerp(From, To, FMath::InterpEaseInOut(0.0f, 1.0f, 1.0f - Left / Total, 2.0f));
	}
};

// A tick that runs in TG_PostUpdateWork -- AFTER the engine has finalised every camera for the
// frame (UWorld::Tick: TG_PostPhysics, then UpdateCameraManager, then TG_PostUpdateWork). The
// weapon is placed from the camera; placing it from Tick reads the PREVIOUS frame's camera, and
// the one-frame difference is the gun juddering against a reticle that never moves.
struct FWeaponPostCameraTick : public FTickFunction
{
	class ABaseCharacter* Target = nullptr;
	virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override;
	virtual FString DiagnosticMessage() override { return TEXT("ABaseCharacter::PostCameraTick"); }
	virtual FName DiagnosticContext(bool bDetailed) override { return FName(TEXT("WeaponPostCamera")); }
};

UCLASS()
class REPLICAN_API ABaseCharacter : public ACharacter
{
	GENERATED_BODY()
	friend struct FWeaponPostCameraTick;   // runs PostCameraTick after the camera
	friend class ABasePlayerController;    // the lag test and its report

public:
	// One step of a combat sequence (public so file-local helpers can build them).
	struct FCombatStep
	{
		UAnimSequence* Clip = nullptr;
		bool bHold = false;    // loop here until the matching End/Release call
	};

	ABaseCharacter();

	// Forwards straight to the mesh's UCharacterAnimInstance -- the look-at
	// cone/blend logic itself lives there now, not on this Character (see
	// CharacterAnimInstance.h), since it's an animation-system concern, not
	// a gameplay one.
	UFUNCTION(BlueprintCallable, Category = "Look At")
	void SetLookAtTarget(const FVector& WorldPoint);

	UFUNCTION(BlueprintCallable, Category = "Look At")
	void ClearLookAtTarget();

protected:

	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void RegisterActorTickFunctions(bool bRegister) override;
	virtual void Landed(const FHitResult& Hit) override;
	// Hiding the character hides its face pieces too (nose, brows, hair live on the FaceController actor).
	virtual void SetActorHiddenInGame(bool bNewHidden) override;

	// The engine does NOT automatically reposition the mesh component
	// during crouch -- only the capsule/root moves (to keep the capsule's
	// base planted, via bCrouchMaintainsBaseLocation). Since GetMesh() has
	// its own fixed relative offset (set in the constructor to align the
	// Knight's feet with the capsule bottom while standing), that offset
	// is now wrong by exactly the capsule's height change once the root
	// has moved, and the mesh visibly sinks/floats relative to the actual
	// floor unless compensated here. This is the engine's own intended hook
	// for exactly that compensation (HalfHeightAdjust is the authoritative
	// amount the engine just changed things by -- reacting to that value is
	// correct regardless of how the capsule repositioning itself works
	// internally, rather than re-deriving the offset by hand).
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

public:

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	// Optional weapon, added as a separate mesh rigidly attached to hand_r
	// (not welded into the character's own mesh) -- same "attach, don't
	// weld" approach as FaceController's NoseMesh, and left unset here in
	// C++ for the same reason: swapping which weapon (or none at all) an
	// instance carries is then just an asset-reference change, set
	// per-instance via Python like every other per-instance asset
	// reference in this project. See UCharacterAnimInstance::
	// WeaponGripWeight for the additive finger-curl correction that makes
	// the (otherwise "Unarmed" open-hand) existing animations look like
	// they're actually holding it.
	UPROPERTY(EditAnywhere, Category = "Weapon")
	TObjectPtr<UStaticMesh> WeaponMesh;

	// Placement relative to hand_r. hand_r's own bone ORIGIN sits at the
	// wrist (measured live: index_01_r/middle_01_r sit at local X of about
	// -10 relative to hand_r, lowerarm_r -- the parent, back toward the
	// elbow -- sits at local X of about +26, confirming local -X is
	// "toward the fingers"), which is why an identity offset put the sword
	// at the wrist instead of the palm -- shifted here along that same
	// axis. See NoseRelativeLocation's header comment on FaceController for
	// why a per-mesh offset like this generally needs tuning; this one is
	// grounded in an actual bone-position measurement rather than a blind
	// guess, but still first-pass and expected to need a nudge.
	UPROPERTY(EditAnywhere, Category = "Weapon")
	FVector WeaponRelativeLocation = FVector::ZeroVector;

	// Two corrections composed here: an initial Pitch=90 (a guess at fixing
	// a reported "rotated 90 degrees off" from the original identity
	// orientation -- the blade's long axis is the mesh's own local Z,
	// confirmed via its bounding box: ~94 units on Z vs ~5/18 on X/Y), then
	// a further +90 twist AROUND the blade's own length axis specifically
	// (per live feedback: "if looking at sword from the tip/top, rotate it
	// counterclockwise 90 degrees along its length") -- composed via
	// AddLocalTransform in a live PIE test (the twist pre-composes with the
	// existing placement, i.e. it's defined in the mesh's own original
	// frame, not the already-pitched one) and read back as this single
	// combined FRotator. The CCW-vs-CW direction of that twist was applied
	// as a best guess (no way to visually confirm which way is actually
	// counterclockwise from this session) -- if backwards, the fix is
	// negating just that second twist, not re-deriving this whole value.
	// Derived from hand_r's measured local frame rather than guessed: local
	// X runs wrist->fingers (fingers at X~-10, forearm at X~+26), local Z is
	// the knuckle line (index_01 at Z~+2.7, middle_01 at Z~-3.2, thumb at
	// Z~+4.1), local Y is the palm normal (thumb sits at Y~+2). A hilt lies
	// ACROSS the palm along the knuckle line with the tip on the thumb side,
	// so mesh Z (blade length) -> hand +Z, mesh Y (edge/guard) -> hand X,
	// mesh X (flat) -> hand -Y: that mapping is exactly Yaw -90 about hand
	// Z. If the tip ends up on the pommel side, add Roll 180 -- keep the
	// yaw; that part is the measured bit.
	UPROPERTY(EditAnywhere, Category = "Weapon")
	FRotator WeaponRelativeRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, Category = "Weapon")
	FVector WeaponRelativeScale = FVector::OneVector;

	// The socket every character skeleton carries, authored so that a weapon
	// baked into grip space hangs correctly off it with no further transform.
	// Named rather than hard-coded at the call site so a rig that spells it
	// differently can be pointed at its own socket from the details panel.
	// See Docs/HeldAssetStandard.md part 2.
	UPROPERTY(EditAnywhere, Category = "Weapon")
	FName WeaponGripSocket = TEXT("WeaponGrip_R");

	UPROPERTY(VisibleAnywhere, Category = "Weapon")
	TObjectPtr<UStaticMeshComponent> WeaponMeshComponent;

	// A fitted optic rides the weapon, not the hand: it is bolted to the rail, so it inherits
	// every recoil kick, sway and carry-position move for free.
	UPROPERTY(VisibleAnywhere, Category = "Weapon")
	TObjectPtr<UStaticMeshComponent> OpticMeshComponent;

	// Null mesh takes the optic off. The weapon's own sight point is set separately, because
	// with an optic fitted the catalogue already reports the lens centre as the sight.
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void SetWeaponOptic(UStaticMesh* OpticMesh, const FVector& MountLocal);

	// True when the weapon is carrying its own sighting device. The HUD hides its reticle while
	// aiming through one: two reticles on screen at once is worse than either alone, and the
	// one drawn on the glass is the one that is actually telling the truth about where the
	// barrel points.
	UFUNCTION(BlueprintPure, Category = "Weapon") bool HasOpticSight() const { return bWeaponHasOptic; }


	// The flash, parented to the weapon so it travels with it. Off except for the few frames
	// after a shot: a light left on would read as a torch taped to the barrel.
	UPROPERTY(VisibleAnywhere, Category = "Weapon")
	TObjectPtr<class UPointLightComponent> MuzzleFlash;
	float MuzzleFlashLeft = 0.0f;

	// Screen-space name tag above the head (UCharacterNameWidget); shown by
	// the controller while edit mode is up.
	UPROPERTY(VisibleAnywhere, Category = "Edit Mode")
	TObjectPtr<UWidgetComponent> NameLabel;

	// Name tag size follows the camera distance: MaxFontSize up close,
	// falling off quickly past NameLabelNearDistance and settling at
	// MinFontSize from NameLabelFarDistance (~5m) outward.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Edit Mode|Name Label")
	int32 NameLabelMinFontSize = 9;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Edit Mode|Name Label")
	int32 NameLabelMaxFontSize = 20;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Edit Mode|Name Label")
	float NameLabelNearDistance = 120.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Edit Mode|Name Label")
	float NameLabelFarDistance = 500.0f;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> MoveForwardAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> MoveRightAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> LookYawAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> LookPitchAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> ZoomAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> CrouchAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> SprintAction;

	// Bound to ALT -- same physical key the old (now removed) direct-press
	// Roll used, repurposed for Dash now that Roll triggers from crouching
	// while running instead (see StartCrouch). Still the same InputAction
	// ASSET as before (re-pointed via Python, not renamed on disk), so no
	// Input Mapping Context changes were needed for this.
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> DashAction;

	// Discrete alternative to scrolling the mouse wheel -- steps one zoom
	// level per press, wrapping from the last entry back to the first.
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> CycleZoomAction;

	// Steps through UCharacterAnimInstance::IdleVariants.
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> ToggleIdleAction;

	// Repurposed from the old Pose-cycling feature (removed earlier) --
	// same physical key ('P'), same InputAction asset, new meaning: an
	// alias for NextCombatAnim (see ']' / NextCombatAnimAction below),
	// kept working rather than removed since it doesn't hurt to have.
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> CyclePoseAction;

	// ']' -- advances SelectedCombatAnimIndex through
	// UCharacterAnimInstance::AllCombatAnims (the full ~118-clip Synty
	// Sword Combat pack, not just the 9 curated AttackAnims). Bound to the
	// same handler as CyclePoseAction/'P'.
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> NextCombatAnimAction;

	// '[' -- steps SelectedCombatAnimIndex backward instead.
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> PrevCombatAnimAction;

	// Left mouse button -- plays AllCombatAnims[SelectedCombatAnimIndex] via
	// the same StartAttack path number keys 1-9 use, letting every clip in
	// the full pack be previewed and played without needing it wired to its
	// own key.
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> PlaySelectedAttackAction;

	// TAB -- shows/hides the CharacterBuilder panel (see
	// ToggleCharacterBuilder). The panel itself covers the same
	// ManualXMultiplier values as ','/'.'/'/ ' via mouse-draggable sliders
	// instead, plus palette/accessories/nose controls planned for a later
	// pass.
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> ToggleCharacterBuilderAction;


	// 'X' -- toggles between the Walk and Jog standing tiers (Jog is the
	// default; Shift always sprints regardless). See UpdateStandingSpeed.
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> ToggleWalkAction;

	// Swaps the whole Idle/Walk/Run/Crouch/Jump moveset between Lyra and
	// the retargeted Synty set (see ECharacterLocomotionSet).
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> ToggleLocomotionSetAction;

	// Number keys 1-9 -- AttackActions[i] plays UCharacterAnimInstance::
	// AttackAnims[i] via StartAttack(Value, i) (see the BindAction overload
	// that takes an extra bound argument, used in SetupPlayerInputComponent
	// so this doesn't need 9 separate handler functions).
	UPROPERTY(EditAnywhere, Category = "Input")
	TArray<TObjectPtr<UInputAction>> AttackActions;

	// Three standing tiers, structured around what the Synty pack actually
	// ships and set to each clip's MEASURED native travel speed so the clip
	// plays at ~1.0x with no stride/distance mismatch: Walk 160 (X toggles
	// walk), Jog 285 (the default), Sprint ~800 (Shift) -- see
	// FLocomotionAnimSet. RunSpeed is deliberately below the sprint clip's
	// native 800 (which is ~29 km/h, a lot for gameplay); the clip plays
	// at 600/800 = 0.75x, long strides slightly slowed. Raise toward 800
	// for a 1.0x sprint. See UpdateStandingSpeed for how these apply.
	UPROPERTY(EditAnywhere, Category = "Movement")
	float WalkSpeed = 160.0f;

	UPROPERTY(EditAnywhere, Category = "Movement")
	float JogSpeed = 285.0f;

	UPROPERTY(EditAnywhere, Category = "Movement")
	float RunSpeed = 600.0f;

	// CharacterMovementComponent::MaxAcceleration -- how quickly ground
	// speed ramps up toward MaxWalkSpeed instead of reaching it instantly.
	// UE's own engine default is 2048, which (relative to this character's
	// fairly low WalkSpeed/RunSpeed) reaches full speed in a couple of
	// frames -- reads as an instant velocity snap rather than a body
	// actually picking up speed. Lowered for a bit of "inertia" -- the
	// character should visibly, if briefly, accelerate rather than teleport
	// to its target speed the instant a direction is pressed.
	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "1.0"))
	float MovementAcceleration = 1500.0f;

	// CharacterMovementComponent::RotationRate.Yaw -- the fastest the
	// character can turn to face its movement direction (see
	// bOrientRotationToMovement in the constructor), in degrees/second. UE's
	// own engine default here effectively turns instantly for this
	// character's movement speeds, which reads as "teleporting" to face a
	// new direction rather than turning through it. Capped lower so a sharp
	// direction change is visibly a turn, not a snap -- part of the same
	// "give the character some inertia" pass as MovementAcceleration.
	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "1.0"))
	float TurnRateDegPerSec = 360.0f;

	// Passed straight through to CharacterMovementComponent::JumpZVelocity.
	// Apex height is V^2/(2*Gravity) -- quadratic in velocity, not linear --
	// so doubling the apex height from the previous 300 tuning takes
	// multiplying by sqrt(2), not by 2: 300*sqrt(2) = ~424.26.
	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0.0"))
	float JumpVelocity = 424.26f;

	// CharacterMovementComponent's GravityScale while falling and moving
	// downward, as a multiple of the (untouched, 1.0) rise-phase gravity --
	// a value above 1 makes the fall faster than the rise instead of a
	// symmetric parabola. Purely a hand-feel knob: with the stock
	// JumpZVelocity (420) and default gravity, the rise to apex only takes
	// ~0.43s either way, but a truly symmetric fall reads as floaty/
	// indecisive at the top (reported as "reaches the highest point almost
	// instantly, then drifts down") -- this is the standard platformer fix
	// (Mario, Celeste, etc. all do some version of this), not a bug fix.
	// Applied continuously in Tick() rather than once on launch so it stays
	// correct through the whole fall even as velocity changes sign there.
	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "1.0"))
	float FallGravityScale = 1.6f;

	// How much steering AddMovementInput has while airborne, as a fraction
	// of ground control (0-1, passed straight through to
	// CharacterMovementComponent::AirControl). UE's own engine default here
	// is a very restrictive 0.05 -- barely any air steering at all, which
	// reads as the character being "locked in" to whatever direction they
	// jumped in. Most third-person action games give noticeably more
	// control than that; this is a standard feel adjustment; applied once
	// in the constructor since there's no reason for it to vary at runtime.
	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AirControlAmount = 0.4f;

	// Jump still counts as valid for this long after the character walks
	// off a ledge with no jump input yet, i.e. "coyote time" -- the
	// near-universal expectation in this genre that walking off an edge and
	// pressing jump a beat too late still works, rather than requiring
	// frame-perfect timing on the very last grounded tick. ACharacter's own
	// CanJump()/Jump() have no concept of this (they require
	// IsMovingOnGround() at the moment of the call), so StartJump() below
	// bypasses them for this case and launches directly.
	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0.0"))
	float CoyoteTimeSeconds = 0.15f;

	// A jump press this soon before actually landing still triggers a jump
	// immediately on landing, instead of being silently dropped because the
	// character technically wasn't grounded yet at the moment of the
	// button press -- the other half of the same "don't require
	// frame-perfect timing" expectation CoyoteTimeSeconds covers for the
	// other edge of a landing.
	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0.0"))
	float JumpBufferSeconds = 0.15f;

	// How long a Crouch key press can last and still count as a TAP rather
	// than a HOLD. A tap toggles crouch on persistently (stays crouched
	// after release, until another tap toggles it back off); anything
	// longer is treated as ordinary hold-to-crouch (crouches only while the
	// key is down, stands the instant it's released, same as this project's
	// original crouch behavior). The key is pressed immediately on Started
	// either way -- this only decides what happens on release, since there's
	// no way to know in advance which kind of press is happening.
	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0.05"))
	float CrouchTapThresholdSeconds = 0.3f;

	// Crouched movement has two speeds, picked continuously by whether
	// Shift is currently held (see UpdateCrouchWalkSpeed). Both independent,
	// explicitly-tuned values now -- originally "fast" was defined as just
	// WalkSpeed itself (so the two stayed in sync automatically), but that
	// coupling broke once WalkSpeed/RunSpeed and the crouch speeds needed to
	// be tuned independently to match each animation set's own pace.
	// Synty's one crouch tier measures 160 cm/s natively (same as Walk), so
	// slow crouch plays at 1.0x; fast (Shift) reuses the same clips at
	// 280/160 = 1.75x -- there's no crouched-sprint mocap in the pack.
	UPROPERTY(EditAnywhere, Category = "Movement")
	float CrouchWalkSlowSpeed = 160.0f;

	UPROPERTY(EditAnywhere, Category = "Movement")
	float CrouchWalkFastSpeed = 280.0f;

	// Compensates a measured ~2.09cm difference between Lyra's and the
	// retargeted Synty set's pelvis height at rest (both share the exact
	// same target skeleton and bone lengths -- the discrepancy is isolated
	// to the IK Retargeter's pose calibration, not a real anatomical
	// difference), which otherwise reads as the character floating slightly
	// above the ground whenever the Synty locomotion set is active. Applied
	// as a one-time delta to the mesh's relative Z the instant
	// ECharacterLocomotionSet actually changes (see Tick()), not baked into
	// a fixed constant offset, so it stacks correctly with the existing
	// crouch mesh-offset compensation (OnStartCrouch/OnEndCrouch) regardless
	// of which locomotion set happens to be active when a crouch starts.
	// Now 0: per-clip floor drift is corrected by the anim instance's foot
	// grounding (UCharacterAnimInstance::bGroundFeet) instead of a fixed
	// per-set nudge. Kept as a property in case a set ever needs a manual
	// bias on top.
	UPROPERTY(EditAnywhere, Category = "Movement")
	float SyntyMeshZOffset = 0.0f;

	// Minimum time between rolls -- prevents chaining rolls into a
	// speed-boosting or i-frame-stacking exploit.
	UPROPERTY(EditAnywhere, Category = "Movement|Roll", meta = (ClampMin = "0.0"))
	float RollCooldownSeconds = 0.6f;

	// How long a roll keeps this Character committed (ignoring new
	// Jump/Crouch/Roll input) before returning control -- tuned against
	// RollAnim's own ~1.03s length on UCharacterAnimInstance.
	UPROPERTY(EditAnywhere, Category = "Movement|Roll", meta = (ClampMin = "0.1"))
	float RollDurationSeconds = 1.0f;

	// Multiplies the root motion UpdateRollMovement extracts from RollAnim
	// before applying it to the actor. RollAnim's OWN authored root motion
	// covers ~4.6m over its ~1s length (confirmed live, see RollAnim's
	// header comment) -- a real distance from the source clip, not a bug,
	// but far more than this game's scale wants a single dodge to cover.
	// Rather than re-baking the FBX (which would also throw away the
	// asset's real timing/deceleration curve), this scales the extracted
	// translation uniformly every frame, so the roll keeps the clip's own
	// feel (fast start, easing out) just over a shorter distance. 1.0 =
	// the clip's real, unscaled distance. Raised to 1.2 (from 0.4) so the
	// roll covers roughly 3x its previous in-game distance, per direct
	// request, once it stopped being scaled down purely to hide the
	// double-motion bug that's since been fixed at its actual source.
	UPROPERTY(EditAnywhere, Category = "Movement|Roll", meta = (ClampMin = "0.01"))
	float RollRootMotionScale = 1.2f;

	// A Crouch press while moving at least this fast (world Velocity2D
	// size) triggers a Roll instead of an ordinary crouch -- see
	// StartCrouch. Sits just above JogSpeed (285) so only a Shift sprint
	// rolls; a jog or walk still crouches. The animation state machine has
	// no matching threshold of its own -- it picks tiers from the same
	// Walk/Jog/RunSpeed values above.
	UPROPERTY(EditAnywhere, Category = "Movement|Roll", meta = (ClampMin = "0.0"))
	float MinSpeedToTriggerRollFromCrouch = 320.0f;

	// Minimum time between dashes.
	UPROPERTY(EditAnywhere, Category = "Movement|Dash", meta = (ClampMin = "0.0"))
	float DashCooldownSeconds = 0.6f;

	// How long a dash keeps this Character committed before returning
	// control, mirroring RollDurationSeconds -- tuned against the Dash
	// clips' own ~1s length on UCharacterAnimInstance.
	UPROPERTY(EditAnywhere, Category = "Movement|Dash", meta = (ClampMin = "0.1"))
	float DashDurationSeconds = 1.0f;

	// Same purpose as RollRootMotionScale, applied to whichever of the 4
	// Dash clips is currently playing -- their raw authored distances
	// aren't in real-world units this project's Blender/FBX pipeline could
	// pin down cleanly (see project notes on the Dash root-motion bake), so
	// this is a feel-tuned multiplier rather than a physically-derived one,
	// same as Roll's.
	UPROPERTY(EditAnywhere, Category = "Movement|Dash", meta = (ClampMin = "0.01"))
	float DashRootMotionScale = 15.0f;

	// How fast the camera boom eases toward its crouch-compensated Z each
	// tick -- without this, the boom's Z was being set directly to its
	// target value every frame, so even though ThirdPersonCrouchCameraDropFraction
	// made that target closer to the standing height than before, reaching
	// it in a single frame (the same frame the capsule's own crouch resize
	// happens, which is itself instant) still reads as a camera "pop," just
	// a smaller one. Same FInterpTo smoothing pattern as ZoomInterpSpeed.
	UPROPERTY(EditAnywhere, Category = "Camera|Zoom")
	float CrouchCameraInterpSpeed = 12.0f;

	// How far (world units) the reticle's camera-center raycast reaches
	// before giving up and just aiming the head at a point this far along
	// the ray -- keeps the head tracking the reticle's direction even when
	// pointed at open space with nothing to hit.
	UPROPERTY(EditAnywhere, Category = "Look At", meta = (ClampMin = "0.0"))
	float ReticleLookAtTraceDistance = 8000.0f;

	// Diagnostic kill-switch for the reticle-driven head look-at system --
	// flip off to bisect whether a given pose problem is coming from
	// UCharacterAnimInstance's look-at bone rotation (ApplyLookAt) or from
	// somewhere else entirely (locomotion blending, retargeting, etc.).
	// Leave true for normal play.
	UPROPERTY(EditAnywhere, Category = "Look At")
	bool bEnableReticleLookAt = true;

	// Ambient look-at for characters nobody is controlling: every few seconds
	// the head turns to a small prop or another character in front of them
	// (within AmbientLookAtRadius and the forward cone), holds, and either
	// picks something else or rests. The player's own character keeps using
	// the reticle look-at instead.
	UPROPERTY(EditAnywhere, Category = "Look At")
	bool bAmbientLookAt = true;

	UPROPERTY(EditAnywhere, Category = "Look At", meta = (ClampMin = "0.0"))
	float AmbientLookAtRadius = 450.0f;

	// Half-angle of the forward cone a target has to sit in, degrees.
	UPROPERTY(EditAnywhere, Category = "Look At", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float AmbientLookAtConeDegrees = 70.0f;

	// Props bigger than this (largest bounds extent, cm) are architecture,
	// not something to glance at.
	UPROPERTY(EditAnywhere, Category = "Look At", meta = (ClampMin = "0.0"))
	float AmbientLookAtMaxPropExtent = 130.0f;

	UPROPERTY(EditAnywhere, Category = "Look At", meta = (ClampMin = "0.1"))
	float AmbientLookAtHoldMin = 2.0f;

	UPROPERTY(EditAnywhere, Category = "Look At", meta = (ClampMin = "0.1"))
	float AmbientLookAtHoldMax = 5.0f;

	// Chance (0-1) that, after a hold, the head rests instead of moving
	// straight on to the next target.
	UPROPERTY(EditAnywhere, Category = "Look At", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AmbientLookAtRestChance = 0.35f;

	// When the player's currently-controlled character is in front and within
	// this range, the NPC looks straight at them most of the time (bias), the
	// rest of the time glancing at nearby props.
	UPROPERTY(EditAnywhere, Category = "Look At", meta = (ClampMin = "0.0"))
	float AmbientLookAtPlayerRadius = 1000.0f;

	UPROPERTY(EditAnywhere, Category = "Look At", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AmbientLookAtPlayerBias = 0.75f;

	UPROPERTY(EditAnywhere, Category = "Input")
	float MouseLookSensitivity = 1.0f;

	// Discrete zoom levels, closest/first-person first -- one mouse wheel
	// click (Alt+Wheel) steps one entry up or down this array rather than
	// continuously scrubbing a distance. Index 0 is always treated as
	// first-person (mesh hidden from the owning camera -- see Tick), so it
	// should be 0 or close to it; every other entry is a third-person
	// TargetArmLength. Exposed as a plain array (not fixed count/spacing)
	// specifically so a different instance/level can be given more
	// zoomed-out steps outdoors and fewer indoors without any code change
	// -- e.g. set a shorter array with a lower max on a character placed
	// in a tight room, a longer one outdoors.
	UPROPERTY(EditAnywhere, Category = "Camera|Zoom")
	TArray<float> ZoomArmLengths = { 0.0f, 150.0f, 250.0f, 400.0f, 600.0f };

	// Index into ZoomArmLengths the camera starts at.
	UPROPERTY(EditAnywhere, Category = "Camera|Zoom")
	int32 DefaultZoomLevelIndex = 0;

	// How fast the boom eases toward each new step's distance -- higher is
	// snappier/more immediate, lower is a slower glide.
	UPROPERTY(EditAnywhere, Category = "Camera|Zoom")
	float ZoomInterpSpeed = 20.0f;

	// CameraBoom is attached directly to the capsule (root), with this fixed
	// relative Z offset approximating eye height while standing -- not
	// literally attached to the head bone. Crouching shrinks and lowers the
	// capsule itself (see the crouch-related comments below), and since the
	// boom is just along for that ride, its WORLD height drops by the same
	// amount as the capsule unless something compensates. In first person
	// that drop is exactly what should happen (the "eyes" should follow the
	// capsule down when crouching). In third person the same full drop reads
	// as a jarring camera dip, so Tick() partially cancels it there -- see
	// ThirdPersonCrouchCameraDropFraction.
	UPROPERTY(EditAnywhere, Category = "Camera")
	float EyeHeightOffset = 75.0f;

	// Third-person-only lateral shift of the camera off the character's own
	// centerline (applied via CameraBoom's SocketOffset, which shifts the
	// spring arm's collision-sweep target too, not just the final visual
	// position -- so it still hugs walls/corners correctly, not just clips
	// through them). The reticle/look-at raycast fires from the ACTIVE
	// camera (see UpdateReticleLookAtTarget), so this is also what keeps the
	// aim ray from running straight down the character's own centerline
	// through his own head -- a dead-center camera means the crosshair
	// always lands on (or just past) the back of his own head. Eased to 0 in
	// first person (see Tick()) so the "eyes" stay centered there instead of
	// reading as one eye peeking from the side of the face.
	// Live-tunable in play with the console variable RepliCan.ShoulderOffset
	// (a value >= 0 overrides this; -1 hands control back).
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "0.0"))
	float ShoulderOffsetY = 95.0f;
	// Third-person-only vertical shift of the boom's socket (positive looks down over the shoulder).
	UPROPERTY(EditAnywhere, Category = "Camera") float ShoulderLiftZ = 35.0f;
	// Multiplies every ZoomArmLengths entry (a preset's way of sitting closer or further).
	UPROPERTY(EditAnywhere, Category = "Camera") float CameraArmScale = 1.0f;
	bool bShoulderLeft = false;


	// Where the first-person eye sits relative to the HEAD BONE, in the actor's own frame.
	// The head bone sits at the base of the skull, so the eye belongs a little above it and
	// well in front of it. Measured on this project's player: standing, the head bone is
	// 166 cm above the capsule's base and 3 cm forward of its axis; crouched, the pose hunches
	// it to 108 cm and 43 cm FORWARD. That forward lean is the whole crouch problem -- a
	// camera pinned to the capsule axis ends up behind the neck looking through the chest.
	UPROPERTY(EditAnywhere, Category = "Camera")
	float FirstPersonEyeForwardOfHeadCm = 12.0f;
	UPROPERTY(EditAnywhere, Category = "Camera")
	float FirstPersonEyeAboveHeadCm = 6.0f;

	// The crouch lean carries the eye well outside the collision capsule, so the eye is swept
	// out from the body rather than simply clamped to the capsule: it travels as far as the head
	// says, unless real geometry is in the way, in which case it stops short of it. Clamping to
	// the capsule instead put the camera back inside the neck every time the character crouched.
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "1.0"))
	float FirstPersonEyeProbeRadiusCm = 10.0f;
	// A ceiling on how far the eye may lead the body, whatever the pose asks for.
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "0.0"))
	float FirstPersonEyeMaxLeadCm = 62.0f;

	// How fast the eye settles onto its target offset. Smoothed in the ACTOR's frame, not in
	// world space, so walking never leaves the camera trailing behind the body -- only the
	// pose-relative motion (crouching, the walk cycle's head bob) is damped.
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "1.0"))
	float FirstPersonEyeInterpSpeed = 12.0f;

	// How far the look may pitch in first person. The engine default is very nearly straight
	// up and down, which is what lets the player put the camera inside his own chest by looking
	// at his feet; shooters clamp well short of vertical for exactly this reason. Third person
	// keeps the engine's own limits, restored whenever the camera pulls back out.
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "-89.0", ClampMax = "0.0"))
	float FirstPersonPitchMin = -70.0f;
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "0.0", ClampMax = "89.0"))
	float FirstPersonPitchMax = 80.0f;

	// The bone the first-person eye rides. Every rig in this project names it "head".
	UPROPERTY(EditAnywhere, Category = "Camera")
	FName FirstPersonHeadBone = TEXT("head");

	// While crouched in third person, the camera boom only follows this
	// fraction of the capsule's own crouch-induced drop -- 0 anchors the
	// boom to the capsule's BOTTOM (the one point on the character that's
	// actually fixed during crouch, by design -- see
	// bCrouchMaintainsBaseLocation), giving a completely constant world Z
	// regardless of crouch state; 1 matches first-person's full drop
	// (tracks the capsule's own center/head-height exactly). First person
	// always uses 1.0 regardless of this value. Computed via direct
	// world-space math in Tick() (capsule bottom + a fixed height), not a
	// relative-offset compensation formula, specifically so there's no room
	// for a subtle "compensating for a moving parent" arithmetic error --
	// verified live (boom AND the actual FollowCamera, not just the boom's
	// pivot, both checked): world Z is bit-for-bit identical standing vs.
	// crouched at 0.0, no rotation or arm-length drift either. An earlier
	// attempt at 0.0 read as "camera overcompensates, gets higher" -- but
	// that test predated a since-fixed bug where the AnimBP kept showing a
	// standing pose for a beat after the capsule/camera had already reacted
	// to crouch, which alone would produce exactly that symptom without the
	// camera math being wrong at all.
	UPROPERTY(EditAnywhere, Category = "Camera|Zoom", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ThirdPersonCrouchCameraDropFraction = 0.0f;

private:

	// Index into ZoomArmLengths; Zoom() just steps this and looks up the
	// new target distance, Tick() does the actual smooth easing toward it
	// so each step glides rather than snaps.
	int32 CurrentZoomLevelIndex = 3;
	bool bInFirstPerson = false;

	// The camera boom's own tracked world Z, updated only by this
	// Character's Tick() -- deliberately NOT re-read from
	// CameraBoom->GetComponentLocation() each tick as the FInterpTo source.
	// Measured live (UE_LOG): the instant the capsule resizes/moves for
	// crouch (in CharacterMovementComponent's own tick, earlier in the same
	// frame), Unreal's attachment system immediately recomputes the boom's
	// world position from its currently-stored relative offset -- which is
	// still calibrated for the OLD capsule position, so the boom
	// momentarily SNAPS to the wrong height before this Character's own
	// Tick() runs later that same frame. FInterpTo-ing from that
	// momentarily-corrupted read is exactly what produced the "drops then
	// resets back up" bounce. Tracking the value ourselves sidesteps the
	// parent's own transient snap entirely.
	float CurrentCameraBoomWorldZ = 0.0f;
	bool bCameraBoomWorldZInitialized = false;

	// One foot's vertical arc, watched for the moment it bottoms out. Lo and Hi track the range
	// the foot has been sweeping lately, so the test is "near the bottom of ITS OWN arc" rather
	// than a fixed height that would only ever suit one gait.
	struct FFootTrack
	{
		FVector World = FVector::ZeroVector;
		float Height = 0.0f;
		bool bDown = false;             // was this foot planted last frame
		float LastStepTime = -100.0f;
		bool bValid = false;
	};
	FFootTrack FootTracks[2];
	// Which sample was used last, so the same one is never played twice running -- a repeated
	// sample is what makes footsteps sound synthetic.
	int32 LastFootstepIndex = -1;
	// The speed the character was falling at, sampled while still in the air: by the time
	// Landed runs the movement component has already dealt with the vertical velocity.
	float LastFallSpeedZ = 0.0f;
	// Last frame's yaw, for working out how fast the character is turning on the spot.
	float LastYawForSteps = 0.0f;
	bool bLastYawValid = false;
	void TickFootsteps(float DeltaSeconds);
	void PlayFootstep(bool bLanding);

	// The eye's offset from the actor, in the actor's yaw frame, eased toward its target.
	FVector CurrentEyeLocal = FVector::ZeroVector;
	bool bCurrentEyeLocalValid = false;

	// The engine's own pitch limits, captured before first person narrows them.
	float DefaultViewPitchMin = -89.9f;
	float DefaultViewPitchMax = 89.9f;
	bool bDefaultViewPitchCaptured = false;

	// Coyote time / jump buffering bookkeeping -- see CoyoteTimeSeconds/
	// JumpBufferSeconds above for what these implement and why.
	float TimeLastGrounded = -1000.0f;
	bool bCoyoteJumpUsed = false;
	bool bJumpInputBuffered = false;
	float TimeJumpBuffered = -1000.0f;

	// Crouch tap-vs-hold bookkeeping -- see CrouchTapThresholdSeconds above.
	// bIsCrouchToggled tracks ONLY the persistent (tap-toggled) case; an
	// ordinary hold never sets it, so a plain hold-crouch's release always
	// takes the "was this held past the threshold" branch in StopCrouch
	// correctly regardless of this flag's current value.
	bool bIsCrouchToggled = false;
	float TimeCrouchPressed = -1000.0f;

	// Whether Shift is currently held -- checked continuously (not just at
	// the moment Crouch is pressed) since crouch-walk speed needs to react
	// to Shift being pressed/released WHILE already crouched, not just at
	// the instant crouch begins. See UpdateCrouchWalkSpeed.
	bool bSprintHeld = false;

	// 'X' walk toggle -- see ToggleWalkAction / UpdateStandingSpeed.
	bool bWalkToggled = false;

	// The groggy walk after the waking-up intro: first person, a slow
	// unsteady shuffle, forward/back only, no sprint, the camera swaying.
	bool bGroggy = false;
	float GroggySpeed = 95.0f;
	float GroggyTime = 0.0f;

	// A Crouch press that turned into a Roll (see StartCrouch) must not
	// have its RELEASE run the ordinary tap/hold logic in StopCrouch --
	// holding the key past the roll's end otherwise hit the "hold" branch
	// and stood the character straight back up, undoing the roll's own
	// "land crouched" (found in review; the roll's crouch only survived if
	// the key happened to be released before the roll finished).
	bool bCrouchReleaseConsumedByRoll = false;

	// Seconds continuously airborne -- reported to the AnimInstance (see
	// FLocomotionInputs::AirTime) so landings can pick soft/medium/hard.
	float AirTimeSeconds = 0.0f;

	// Roll bookkeeping -- see RollCooldownSeconds/RollDurationSeconds above.
	bool bIsRolling = false;
	float TimeLastRoll = -1000.0f;

	// Dash bookkeeping -- see DashCooldownSeconds/DashDurationSeconds above.
	// Mirrors the Roll fields exactly, just for the 4-directional case.
	bool bIsDashing = false;
	float TimeLastDash = -1000.0f;
	float DashElapsedAtLastConsume = 0.0f;
	ECharacterDashDirection CurrentDashDirection = ECharacterDashDirection::Forward;

	// Attack bookkeeping. Unlike Roll/Dash there's no root motion to extract
	// (AttackAnims are plain in-place clips, see TriggerAttack's header
	// comment on FCharacterAnimInstanceProxy) -- CharacterMovementComponent
	// keeps working normally, this just blocks new movement/action input and
	// times how long to hold that block for, using the specific clip's own
	// GetPlayLength() (captured at trigger time, since AttackAnims[Index]
	// can differ in length from one index to the next) rather than a single
	// fixed duration for all 9.
	bool bIsAttacking = false;
	float TimeLastAttack = -1000.0f;
	float AttackDurationSeconds = 0.0f;

	// ---- Combat sequence player (see the Combat capabilities above).
	FCombatAnimLibrary CombatLibrary;
	void EnsureCombatLibrary();
	TArray<FCombatStep> CombatQueue;
	int32 CombatStepIndex = -1;
	bool bCombatHolding = false;
	bool bBlocking = false;
	TFunction<void()> CombatOnComplete;
	// Starts the sequence (interrupting anything if bInterrupt). False if a
	// clip is missing or an action is already playing and bInterrupt is off.
	bool PlayCombatSequence(const TArray<FCombatStep>& Steps, bool bInterrupt, TFunction<void()> OnComplete = nullptr);
	void StartCombatStep(int32 Index);
	void AdvanceCombat();            // called when the current step's clip finishes
	void ReleaseCombatHold();        // leaves a held step and continues the sequence
	// Resolves a library key, logging a warning when the pack lacks it.
	UAnimSequence* CombatClip(const FString& Key) const;
	FString GenderToken() const;     // "Masc" / "Femn" per CurrentConfig.Gender

	// Whether SyntyMeshZOffset is CURRENTLY baked into the mesh's relative
	// Z -- toggled exactly once per actual ECharacterLocomotionSet change
	// (see Tick()), not recomputed from scratch every frame, so it composes
	// as a plain add/subtract delta with whatever OnStartCrouch/OnEndCrouch
	// independently do to the same relative Z.
	bool bSyntyMeshOffsetApplied = false;

	// The most recent raw axis values MoveForward/MoveRight were called
	// with -- captured unconditionally (even while bIsRolling/bIsDashing
	// blocks the movement itself) so StartDash always has a fresh read of
	// "which way is the player currently holding" to pick a direction from,
	// regardless of whether that input was actually allowed to move the
	// capsule this frame.
	float LastForwardAxis = 0.0f;
	float LastRightAxis = 0.0f;

	// Snapshotted once when a roll/dash starts (never both -- they're
	// mutually exclusive) and reused for every frame of that same move,
	// instead of re-reading GetMesh()->GetComponentTransform() live each
	// frame -- CharacterMovementComponent's bOrientRotationToMovement keeps
	// turning the actor to face whatever residual velocity is left over
	// from the input that was happening right before the move started, and
	// re-deriving the root motion's world direction from a rotation that's
	// still drifting mid-move is exactly what turned a straight dash into a
	// curved "oval orbit" (confirmed live) -- each frame's local-space
	// delta got rotated by a slightly different yaw than the frame before
	// it. Locking the rotation once at the start, and separately freezing
	// bOrientRotationToMovement itself for the move's duration (see
	// StartRoll/StartDash), removes both the cause and this symptom of it.
	FQuat RootMotionMoveMeshRotation = FQuat::Identity;

	// How far into RollAnim's own timeline UpdateRollMovement has already
	// extracted root motion up to -- each call computes the delta from here
	// to (Now - TimeLastRoll), then advances this to match, so repeated
	// calls hand out contiguous, non-overlapping slices of the clip's root
	// motion. Reset to 0 whenever a fresh roll starts. Deliberately tracked
	// HERE (this Character's own reliable, always-current-by-the-time-Tick-
	// runs clock) rather than reading the mesh's AnimInstance's own internal
	// playback time -- that time is advanced by the SkeletalMeshComponent's
	// OWN tick, which has no guaranteed order relative to this Character's
	// Tick(), so a first attempt at this read a value that could still be
	// stale from a frame ago depending on tick scheduling, producing
	// exactly the "travels too far, snaps back" symptom this whole roll
	// feature kept running into regardless of which animation drove it.
	float RollElapsedAtLastConsume = 0.0f;

	// Pushes Speed/bIsInAir/VelocityZ/bIsCrouched into the mesh's
	// UCharacterAnimInstance every tick -- all the actual locomotion state
	// machine, blending, and crouch handling now lives in that class, not
	// here (see CharacterAnimInstance.h). This Character's own job is just
	// reporting movement state, not deciding which pose to show.
	void UpdateAnimationInstance();
	void UpdateReticleLookAtTarget();
	void UpdateAmbientLookAt(float DeltaSeconds);
	// Hides only the head region (face parts + head bone + FaceController
	// cosmetics) from the owning camera in first person, leaving the body,
	// arms, hands and weapon visible to the player.
	void ApplyFirstPersonHeadHiding(bool bFirstPerson);
	AActor* PickAmbientLookAtTarget() const;
	// The player's controlled character, if it is one of ours and sits in
	// front within AmbientLookAtPlayerRadius; null otherwise.
	AActor* GetPlayerLookAtCandidate() const;
	FVector GetAmbientLookAtPoint(const AActor* Target) const;
	// Copies CurrentConfig.Tags onto the actor's own Tags array.
	void SyncActorTagsFromConfig();
	// One-shot: every character that isn't the player gets the "npc" tag.
	bool bTagsInitialized = false;
	// Name label fade state (see SetInspectedNameplate / Tick).
	bool bNameLabelEditVisible = false;
	bool bNameLabelInspected = false;
	float NameLabelInspectLostTime = -1000.0f;
	float NameLabelAlpha = 0.0f;
	TWeakObjectPtr<AActor> AmbientLookAtActor;
	float AmbientLookAtTimer = 0.0f;
	bool bAmbientLookAtResting = false;

	// Extracts this frame's root motion delta directly from the mesh's
	// UCharacterAnimInstance::RollAnim asset (see RollElapsedAtLastConsume's
	// header comment for why this queries the asset directly rather than
	// the AnimInstance's own internal playback state) and applies it to the
	// actor -- called from Tick() while bIsRolling. RollAnim is the one clip
	// in this
	// whole project with real baked root motion (confirmed live against a
	// properly root-motion-authored source, see RollAnim's header comment),
	// so unlike every other animation here, the capsule's movement during a
	// roll comes from the clip itself rather than from CharacterMovementComponent's
	// own speed/acceleration model.
	void UpdateRollMovement();

	// Same approach as UpdateRollMovement, for whichever of the 4 Dash clips
	// CurrentDashDirection currently points at.
	void UpdateDashMovement();

	// Sets CharacterMovementComponent's MaxWalkSpeedCrouched from
	// bSprintHeld/CrouchWalkSlowSpeed/CrouchWalkFastSpeed -- called whenever
	// either of the first two could have changed (Crouch beginning, Shift
	// pressed/released) rather than every Tick, since it only actually
	// needs to change on those events.
	void UpdateCrouchWalkSpeed();

	// Sets CharacterMovementComponent's MaxWalkSpeed from bSprintHeld/
	// bWalkToggled: Shift -> RunSpeed, else X-toggled -> WalkSpeed, else
	// JogSpeed. Called on the events that change either flag, not per Tick.
	void UpdateStandingSpeed();
public:
	void SetGroggy(bool bOn, float Speed);
	bool IsGroggy() const { return bGroggy; }
	// Ctrl+C: the camera swaps to the other shoulder.
	UFUNCTION(BlueprintCallable, Category = "Camera") void ToggleShoulderSide() { bShoulderLeft = !bShoulderLeft; }
	UFUNCTION(BlueprintPure, Category = "Camera") bool IsShoulderLeft() const { return bShoulderLeft; }
	// C steps the zoom table (first person -> furthest, then wraps); the wheel no longer zooms.
	UFUNCTION(BlueprintCallable, Category = "Camera") void StepZoomLevel();
	UFUNCTION(BlueprintPure, Category = "Camera") int32 GetZoomLevelIndex() const { return CurrentZoomLevelIndex; }
	// Jump straight to a zoom step; 0 is first person. Exposed as a console command so the
	// camera can be put in a known state for testing without hunting the mouse wheel.
	UFUNCTION(Exec, BlueprintCallable, Category = "Camera") void SetZoomLevel(int32 Index);
	UFUNCTION(BlueprintPure, Category = "Camera") int32 GetNumZoomLevels() const { return ZoomArmLengths.Num(); }
	// Debugging: the guards on the move handlers and whether they are being reached.
	UFUNCTION(BlueprintPure, Category = "Debug") FString DescribeMoveGuards() const;
	int32 MoveForwardCalls = 0;
protected:

	// Bound to 'X' (ToggleWalkAction).
	void ToggleWalk(const FInputActionValue& Value);

	void MoveForward(const FInputActionValue& Value);
	void MoveRight(const FInputActionValue& Value);
	void LookYaw(const FInputActionValue& Value);
	void LookPitch(const FInputActionValue& Value);
	void Zoom(const FInputActionValue& Value);
	void StartJump(const FInputActionValue& Value);
	void StopJump(const FInputActionValue& Value);

	void StartCrouch(const FInputActionValue& Value);
	void StopCrouch(const FInputActionValue& Value);
	void StartSprint(const FInputActionValue& Value);
	void StopSprint(const FInputActionValue& Value);
	void CycleZoom(const FInputActionValue& Value);

	// No longer bound directly to any input -- called from StartCrouch when
	// a Crouch press arrives while running fast enough (see
	// MinSpeedToTriggerRollFromCrouch), not from its own key anymore.
	void StartRoll(const FInputActionValue& Value);

	// Bound to ALT (DashAction) -- picks a direction from current WASD
	// input (see LastForwardAxis/LastRightAxis) and plays the matching
	// Lyra Dash clip.
	void StartDash(const FInputActionValue& Value);

	// Bound to number keys 1-9 via AttackActions[Index] -- see that
	// property's header comment for the BindAction extra-argument overload
	// that routes all 9 keys through this one handler. Resolves
	// AttackAnims[Index] then defers to PlayAttackAnim.
	void StartAttack(const FInputActionValue& Value, int32 Index);

	void ToggleIdleVariant(const FInputActionValue& Value);

	// Bound to 'P' (CyclePoseAction) and ']' (NextCombatAnimAction) --
	// advances SelectedCombatAnimIndex through UCharacterAnimInstance::
	// AllCombatAnims, wrapping via GetNumAllCombatAnims(). Purely a
	// selection change, no animation plays from this call itself (see
	// PlaySelectedAttack).
	void CyclePose(const FInputActionValue& Value);

	// Bound to '[' (PrevCombatAnimAction) -- steps SelectedCombatAnimIndex
	// backward instead.
	void PrevCombatAnim(const FInputActionValue& Value);

	// Bound to left mouse button -- plays AllCombatAnims[SelectedCombatAnimIndex]
	// (the full pack, not just the 9 curated AttackAnims) via PlayAttackAnim.
	void PlaySelectedAttack(const FInputActionValue& Value);

	// Shared by StartAttack/PlaySelectedAttack -- both resolve to a specific
	// UAnimSequence* first (from different arrays/indices), then hand it
	// here to actually trigger it and start the bIsAttacking hold window.
	void PlayAttackAnim(UAnimSequence* Anim);

	void ToggleLocomotionSet(const FInputActionValue& Value);

	// Bound to TAB -- creates CharacterBuilderWidgetInstance on first press
	// (lazily, not in BeginPlay, since most instances will never open it),
	// then just shows/hides it on each subsequent press. While open the
	// pointer is available and viewport mouse capture is off (see the
	// implementation for the capture-mode reasoning); WASD stays live.
	void ToggleCharacterBuilder(const FInputActionValue& Value);

public:

	// Which entry of UCharacterAnimInstance::AllCombatAnims '['/']'/'P' has
	// currently selected -- read by ABaseHUD to show the name top-center,
	// alongside PlaySelectedAttackAction's own binding on the character side.
	int32 GetSelectedCombatAnimIndex() const { return SelectedCombatAnimIndex; }

	// Live re-equip/re-place, used by the CharacterBuilder panel -- each
	// writes the matching UPROPERTY above AND pushes it to
	// WeaponMeshComponent immediately (BeginPlay only applies them once).
	void SetWeaponMesh(UStaticMesh* NewMesh);
	void SetWeaponRelativeLocation(const FVector& NewLocation);
	void SetWeaponRelativeRotation(const FRotator& NewRotation);

	// Palette swap: material slot 0 on the character mesh, plus the attached
	// FaceController's nose/hair/beard/head-gear so they keep matching (see
	// AFaceController::ApplyMaterialToAttachments).
	void SetCharacterMaterial(UMaterialInterface* Material);
	UMaterialInterface* GetCharacterMaterial() const;

	// The FaceController attached to this character's mesh, if any -- the
	// nose/hair/head-gear cosmetics live on it, not here.
	AFaceController* GetAttachedFaceController() const;

	UCharacterAnimInstance* GetCharacterAnimInstance() const;

	// ---- Character configuration (see CharacterConfig.h) --------------
	//
	// Name of the <Project>/Characters/<Name>.json to load at BeginPlay.
	// If the file doesn't exist yet, a default Modular Fantasy Hero is
	// assembled under that name and saved. Empty = keep the level's own
	// mesh (the original knight) and stay non-modular.
	UPROPERTY(EditAnywhere, Category = "Character Config")
	FString DefaultCharacterConfigName;

	// Placed by the Character Manager at runtime (not a level actor): the
	// save game respawns these rather than looking them up by name.
	UPROPERTY() bool bRuntimeSpawned = false;

	// Ambient idle behaviour: FCharacterConfig::IdleAnimations played one
	// after another at random with short gaps, glancing at the player on
	// alternate clips (NPCs only).
	void StartIdleBehaviour();
	void PlayNextIdle();
	UPROPERTY() TArray<TObjectPtr<UAnimSequence>> IdleClips;
	UPROPERTY() TObjectPtr<class USpotLightComponent> Headlamp;
	TWeakObjectPtr<AActor> SeatActor;
	TWeakObjectPtr<AActor> ApproachSeat;
	float ApproachHeight = 66.0f, ApproachYaw = 0.0f, ApproachLean = 0.0f, ApproachHunch = 0.0f, ApproachTime = 0.0f;
	FRotator SeatMeshRotation = FRotator::ZeroRotator;   // the mesh's rest rotation, put back on standing
	float SeatHunchBefore = 0.0f;                         // the gait hunch before sitting, put back on standing
	FVector StandLocation = FVector::ZeroVector;
	FTimerHandle IdleTimer;
	int32 LastIdleIndex = -1;
	bool IdleGlanceToggle = false;
	// Gaze schedule (FCharacterConfig::GazeTargets): a weighted pick every
	// hold, re-aimed a few times a second so moving targets are tracked.
	void PickGazeTarget();
	void UpdateGaze();
	FTimerHandle GazeTimer;
	FTimerHandle GazeAimTimer;
	FString CurrentGazeTarget;
	UFUNCTION(BlueprintPure, Category = "Character") FString GetGazeTarget() const { return CurrentGazeTarget; }
	// True for the config file name, the display name, or the display name
	// without spaces ("HannahMartinez" / "Hannah Martinez").
	bool MatchesConfigName(const FString& Name) const;
	// Pins the gaze to a world point (the remote-call camera) over any gaze
	// schedule or idle glance until released.
	void SetGazeLock(bool bLock, const FVector& Point = FVector::ZeroVector);
	// Applies CurrentConfig.SkinTint / StubbleFade through a dynamic material
	// (M_CharacterTint) over the base material's atlas; no-op when SkinTint.A == 0.
	void ApplySkinTint();
	void SetSkinTint(const FLinearColor& Tint, float StubbleFade, float HeadStubbleFade = -1.0f);   // HeadStubble < 0 keeps the current value
	void SetHairMeshPath(const FString& Path);
	void SetFacialHairMeshPath(const FString& Path);
	void SetRemoteCamera(const FRemoteCameraConfig& Camera);
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> SkinTintMID;
	bool bGazeLocked = false;
	bool IsGazeLocked() const { return bGazeLocked; }
	FVector GazeLockPoint = FVector::ZeroVector;

	// Test hook: a non-zero value feeds that much forward movement input every
	// tick (locally controlled characters only), so locomotion can be
	// exercised from Python without holding a key.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	float DebugAutoMoveForward = 0.0f;
	// Direction of that test input relative to the actor's facing, degrees
	// (180 = start by turning round), so the start-turn transitions can be
	// exercised from Python.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	float DebugAutoMoveYaw = 0.0f;
	FVector DebugAutoMoveWorldDir = FVector::ForwardVector;
	bool bDebugAutoMoveActive = false;

	// Turn on the spot by about DeltaYaw degrees using the locomotion set's
	// turn-in-place clips (nearest 90/180, left/right); the clip's root yaw
	// rotates the capsule. Ignored while moving, airborne or mid-action.
	UFUNCTION(BlueprintCallable, Category = "Locomotion")
	void TurnInPlace(float DeltaYawDegrees);
	// Sprint -> crouch slide (see FLocomotionAnimSet::SprintToCrouchAnim):
	// started by a Crouch press at sprint speed, ends crouched.
	void StartSprintSlide();
	bool bIsSliding = false;
	float TimeSlideStarted = 0.0f;
	float SlideDurationSeconds = 0.0f;
	// True while a transition clip's root yaw is steering the capsule (orient-
	// to-movement is suspended for its duration).
	bool bAnimRotationHold = false;

	const FCharacterConfig& GetCharacterConfig() const { return CurrentConfig; }
	// Same, after pulling the live weapon/arm/scale/color state into it
	// (what a save should capture).
	const FCharacterConfig& GetLiveCharacterConfig() { SyncConfigFromLive(); return CurrentConfig; }
	bool IsModularCharacter() const { return bModularMode; }

	// Replaces the whole configuration and rebuilds the character from it.
	void ApplyCharacterConfig(const FCharacterConfig& Config);
	bool SaveCharacterConfig();
	bool LoadCharacterConfig(const FString& Name);
	void NewCharacterConfig(const FString& Name);

	// Unsaved-changes tracking for the Character Manager: the config as last
	// saved/loaded (JSON text), compared against the live config. A freshly
	// placed character has no saved baseline and so counts as dirty.
	UFUNCTION(BlueprintPure, Category = "Character Config")
	bool IsConfigDirty();
	void MarkConfigSaved();
	// Puts the character back to its saved baseline (the Discard choice).
	UFUNCTION(BlueprintCallable, Category = "Character Config")
	void RevertToSavedConfig();

	// What "reset to default" means for every field, for this character:
	// the type's stock config, with the level's original mesh/palette/
	// weapon and the anim instance's stock arm pose filled in.
	FCharacterConfig GetDefaultCharacterConfig() const;
	FLinearColor GetDefaultPartColor(const FString& Parameter) const;

	// Granular setters used by the CharacterBuilder panel -- each updates
	// CurrentConfig AND applies just that change, so a slider drag doesn't
	// rebuild 27 mesh components per tick.
	void SetCharacterName(const FString& NewName);

	// Edit-mode presentation. Hover = blue translucent shell, selected = gold
	// (selected wins); drawn via each mesh's overlay material so it works on
	// every rig. The name tag is a screen-space widget above the head.
	UFUNCTION(BlueprintCallable, Category = "Edit Mode")
	void SetHoverHighlight(bool bEnabled);
	UFUNCTION(BlueprintCallable, Category = "Edit Mode")
	void SetSelectedHighlight(bool bEnabled);
	void SetNameLabelVisible(bool bVisible);
	// Nameplate while inspected in normal play (reticle on the character
	// within reach): fades in quickly, lingers for NameLabelInspectHoldSeconds
	// after the reticle leaves (so a brief slip doesn't blink it), then
	// fades out softly. Edit mode's visibility (SetNameLabelVisible) is
	// OR'd with this; both drive one fade.
	void SetInspectedNameplate(bool bInspected);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Edit Mode|Name Label")
	float NameLabelInspectHoldSeconds = 0.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Edit Mode|Name Label")
	float NameLabelFadeInPerSecond = 8.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Edit Mode|Name Label")
	float NameLabelFadeOutPerSecond = 2.0f;
	void UpdateNameLabel();
	// Per tick while the tag is shown: font size from the camera distance.
	void UpdateNameLabelScale();
	void SetCharacterType(const FString& NewType);
	void SetBaseMesh(const FString& AssetPath);
	void SetPaletteMaterial(const FString& AssetPath);
	UFUNCTION(BlueprintCallable, Category = "Character Config")
	void SetGender(const FString& NewGender);
	UFUNCTION(BlueprintPure, Category = "Character Config")
	FString GetGender() const { return CurrentConfig.Gender; }

	// Locomotion set: "Male", "Female" or "Goblin" (FCharacterConfig::
	// Locomotion; empty follows Gender). Independent of Gender, which still
	// drives the modular parts.
	UFUNCTION(BlueprintCallable, Category = "Character Config")
	void SetLocomotionSet(const FString& SetName);
	UFUNCTION(BlueprintPure, Category = "Character Config")
	FString GetLocomotionSetName() const;
	ESyntyLocomotionChoice ResolveLocomotionChoice() const;

	// Try-out playback of any clip on this character (the Animation Browser
	// page): a one-shot returns to locomotion when it ends; a loop holds
	// until StopAnimationClip. Goes through the combat sequence player, so
	// it interrupts and is interrupted like an attack.
	UFUNCTION(BlueprintCallable, Category = "Animation")
	void PlayAnimationClip(UAnimSequence* Clip, bool bLoop);
	UFUNCTION(BlueprintCallable, Category = "Animation")
	void StopAnimationClip();
	// Sitting: parked on a seat (a prop tagged seat:<top height>,<facing yaw>) with the bench
	// loop held; movement input or the Stand action gets up again.
	// A headlamp on the brow: L toggles it.
	// The six attributes this character was built with (see FAttributes).
	const FAttributes& GetAttributes() const { return CurrentConfig.Attributes; }
	UFUNCTION(BlueprintPure, Category = "Attributes") int32 GetAttribute(FName Which) const { return CurrentConfig.Attributes.Get(Which); }
	UFUNCTION(BlueprintCallable, Category = "Attributes") void SetAttribute(FName Which, int32 Value) { CurrentConfig.Attributes.Set(Which, Value); }
	// ---- Footsteps -------------------------------------------------------
	// Driven by ground covered, not by animation notifies: a stride accumulator fires a step
	// every so many centimetres, so the cadence follows the real speed and no clip has to be
	// edited to add or retime a sound. Sounds are the loose WAVs in RawAudio written by
	// Tools/make_footstep_sounds.py, played 2D -- these are the player's own boots, and only
	// a locally controlled player character runs this at all.
	UPROPERTY(EditAnywhere, Category = "Audio|Footsteps") bool bFootstepsEnabled = true;
	// The two feet the steps are read from. Every rig in this project names them this way.
	UPROPERTY(EditAnywhere, Category = "Audio|Footsteps") FName FootBoneLeft = TEXT("foot_l");
	UPROPERTY(EditAnywhere, Category = "Audio|Footsteps") FName FootBoneRight = TEXT("foot_r");
	// A foot is planted when it stops moving THROUGH THE WORLD. While it swings it travels at
	// roughly twice the character's speed; the moment it takes the load it is stationary, give
	// or take whatever the animation slides. Measuring against the character's own speed makes
	// that test work at a walk, at a run and while sliding, with no per-clip tuning. This is the
	// fraction of the character's speed below which the foot counts as down.
	//
	// It also sets the TIMING. A foot decelerates into its plant, so the lower this is the later
	// in that deceleration the sound lands -- 0.35 read as slightly late. Higher catches the foot
	// on the way down and fires closer to the moment of contact; too high and it triggers
	// mid-swing. Tunable live with FootstepTune, no rebuild needed.
	UPROPERTY(EditAnywhere, Category = "Audio|Footsteps", meta = (ClampMin = "0.05", ClampMax = "0.9")) float FootPlantSpeedFraction = 0.55f;
	// Live tuning without a rebuild: FootstepTune <plantedFraction> <minInterval>.
	UFUNCTION(Exec) void FootstepTune(float PlantedFraction, float MinInterval);
	// The same foot cannot step twice inside this, which throws away jitter at the bottom of the arc.
	UPROPERTY(EditAnywhere, Category = "Audio|Footsteps", meta = (ClampMin = "0.02")) float FootstepMinInterval = 0.13f;
	// Below this the character is shuffling against a wall, not walking.
	UPROPERTY(EditAnywhere, Category = "Audio|Footsteps", meta = (ClampMin = "0.0")) float FootstepMinSpeed = 45.0f;
	// Turning on the spot moves the feet without moving the body, so a speed gate alone silences
	// the whole shuffle. Above this rate of turn the character counts as moving even at a
	// standstill, in degrees per second.
	UPROPERTY(EditAnywhere, Category = "Audio|Footsteps", meta = (ClampMin = "0.0")) float FootstepMinYawRate = 40.0f;
	// How far out the feet sit from the axis a turn pivots about. Turns the rate of turn into a
	// foot speed, so the same planted test works whether the character is walking or spinning.
	UPROPERTY(EditAnywhere, Category = "Audio|Footsteps", meta = (ClampMin = "1.0")) float FootstepStanceRadiusCm = 34.0f;
	// Deliberately low: boots in a corridor should sit under the room, not on top of it.
	// The settings dial multiplies this; see URepliCanUserSettings::FootstepVolumeStep.
	UPROPERTY(EditAnywhere, Category = "Audio|Footsteps", meta = (ClampMin = "0.0")) float FootstepVolume = 0.30f;
	UPROPERTY(EditAnywhere, Category = "Audio|Footsteps", meta = (ClampMin = "0.0")) float FootstepCrouchVolumeScale = 0.45f;
	// The camera is a microphone. In first person the feet are a metre below the ear and the
	// player owns them, so they should still be audible; from an over-the-shoulder camera they
	// are somebody else's boots several metres away and were far too loud for that.
	UPROPERTY(EditAnywhere, Category = "Audio|Footsteps", meta = (ClampMin = "0.0")) float FootstepFirstPersonVolumeScale = 0.70f;
	UPROPERTY(EditAnywhere, Category = "Audio|Footsteps", meta = (ClampMin = "0.0")) float FootstepThirdPersonVolumeScale = 0.30f;
	UPROPERTY(EditAnywhere, Category = "Audio|Footsteps", meta = (ClampMin = "0.0")) float FootstepLandVolume = 0.50f;
	// A drop slower than this is a kerb, not a landing, and gets no thump of its own.
	UPROPERTY(EditAnywhere, Category = "Audio|Footsteps", meta = (ClampMin = "0.0")) float FootstepLandMinFallSpeed = 260.0f;
	// How many steps have been played this session. VisibleAnywhere so a test script can read
	// it back: a plain UPROPERTY is invisible to the editor's Python bindings.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio|Footsteps") int32 FootstepsPlayed = 0;

	// ---- Firing ----------------------------------------------------------
	// Called when the held weapon goes off. MuzzleLocal is the point in the weapon MESH's own
	// space that the shot leaves from, which UI/Weapons.json carries per weapon.
	void OnWeaponFired(const FVector& MuzzleLocal);
	// How far the view is thrown up per shot, in degrees.
	// ---- Stance -------------------------------------------------------------
	// What the body does with what it is holding. A stance is an animation
	// FOLDER name that the weapon carries in UI/Weapons.json -- Rifle, Pistol,
	// Shotgun, Blade -- and every clip in it is reached by composing the name,
	// never by a lookup table here. Empty means unarmed. See
	// Docs/HeldAssetStandard.md part 3.
	UFUNCTION(BlueprintCallable, Category = "Weapon") void SetWeaponStance(const FString& Stance);
	UFUNCTION(BlueprintPure, Category = "Weapon") FString GetWeaponStance() const { return WeaponStance; }

	// Right mouse: narrows the camera, tightens the spread, and swaps the held
	// idle for the stance's aim-down-sights pose.
	UFUNCTION(BlueprintCallable, Category = "Weapon") void SetAiming(bool bNewAiming);
	UFUNCTION(BlueprintPure, Category = "Weapon") bool IsAiming() const { return bAiming; }

	// One-shot upper-body action -- "Fire", "Reload", "Equip", "Melee",
	// "DryFire". False when no stance in the fallback chain ships that clip,
	// which is a normal answer rather than an error (Shotgun has no Equip).
	UFUNCTION(BlueprintCallable, Category = "Weapon") bool PlayWeaponAction(const FString& Clip);
	UFUNCTION(BlueprintPure, Category = "Weapon") bool IsWeaponBusy() const { return WeaponActionLeft > 0.0f; }

	// Half-angle of the cone the next shot can land in. Steady and crouched and
	// aiming is tight; running and firing repeatedly opens it up. The reticle
	// draws this number, so what the player sees is literally the accuracy.
	UFUNCTION(BlueprintPure, Category = "Weapon") float GetWeaponSpreadDegrees() const;

	UPROPERTY(EditAnywhere, Category = "Weapon|Accuracy", meta = (ClampMin = "0.0")) float SpreadHipDegrees = 0.8f;
	UPROPERTY(EditAnywhere, Category = "Weapon|Accuracy", meta = (ClampMin = "0.0")) float SpreadAimDegrees = 0.15f;
	// Added at full running speed, scaled down linearly toward standing still.
	UPROPERTY(EditAnywhere, Category = "Weapon|Accuracy", meta = (ClampMin = "0.0")) float SpreadMoveDegrees = 0.9f;
	UPROPERTY(EditAnywhere, Category = "Weapon|Accuracy", meta = (ClampMin = "0.0")) float SpreadCrouchScale = 0.55f;
	UPROPERTY(EditAnywhere, Category = "Weapon|Accuracy", meta = (ClampMin = "0.0")) float SpreadAirborneDegrees = 1.0f;
	// Bloom: each shot opens the cone, and it closes again while the trigger is
	// off. This is what makes a burst tighter than a spray.
	UPROPERTY(EditAnywhere, Category = "Weapon|Accuracy", meta = (ClampMin = "0.0")) float SpreadPerShotDegrees = 0.18f;
	UPROPERTY(EditAnywhere, Category = "Weapon|Accuracy", meta = (ClampMin = "0.0")) float SpreadBloomMaxDegrees = 1.0f;
	UPROPERTY(EditAnywhere, Category = "Weapon|Accuracy", meta = (ClampMin = "0.0")) float SpreadRecoverDegreesPerSecond = 1.0f;

	// How much closer and narrower the camera gets while aiming.
	UPROPERTY(EditAnywhere, Category = "Weapon|Accuracy", meta = (ClampMin = "0.0")) float AimArmScale = 0.45f;
	UPROPERTY(EditAnywhere, Category = "Weapon|Accuracy", meta = (ClampMin = "10.0")) float AimFieldOfView = 62.0f;
	UPROPERTY(EditAnywhere, Category = "Weapon|Accuracy", meta = (ClampMin = "10.0")) float HipFieldOfView = 90.0f;
	UPROPERTY(EditAnywhere, Category = "Weapon|Accuracy", meta = (ClampMin = "0.1")) float AimInterpSpeed = 9.0f;

	// ---- Carry positions ----------------------------------------------------
	// Four ways to hold a gun, and they are all the SAME SOLVE with a different offset from the
	// eye. That is the whole model:
	//
	//   ADS         rear sight on the eye line, at eye relief. Looking through the sights.
	//   Shouldered  still pointed at the target, but held down and to the right of the eye --
	//               a right-handed shooter's cheek is off the stock. Fast, less precise.
	//   HipFire     lower and further right again, barely raised. Fastest, least precise.
	//   LowReady    muzzle depressed. Not aimed at anything, and safe to walk with.
	//
	// The first three all keep the POINT OF AIM on the target: the weapon is displaced from the
	// eye but rotated so its barrel still converges on what the reticle is over. Only low ready
	// breaks that, because breaking it is what low ready is.
	//
	// The hands then follow the weapon's grips rather than the weapon following the hands. That
	// inversion is why this works for 43 guns of different lengths with no per-weapon animation.
	// What the weapon is ACTUALLY doing. Not the same as what the player has asked for: moving
	// between postures costs time, and during that time the weapon is between the two.
	UFUNCTION(BlueprintPure, Category = "Weapon") EWeaponCarry GetCarry() const { return CurrentCarry; }
	// What the player's inputs are asking for right now.
	UFUNCTION(BlueprintPure, Category = "Weapon") EWeaponCarry GetDesiredCarry() const;
	UFUNCTION(BlueprintPure, Category = "Weapon") bool IsCarryTransitioning() const { return CarryBlendLeft > 0.0f; }
	// Seconds until the weapon is settled in a posture it can be fired from, 0 if it already is.
	// A trigger pull during that window waits rather than being thrown away.
	UFUNCTION(BlueprintPure, Category = "Weapon") float SecondsUntilReadyToFire() const;

	// Offsets from the eye, in camera space: forward is eye relief, then right and up. A
	// right-handed shooter goes DOWN and RIGHT as the position gets less precise.
	UPROPERTY(EditAnywhere, Category = "Weapon|Carry") FVector CarryADS = FVector(16.0f, 0.0f, 0.0f);
	UPROPERTY(EditAnywhere, Category = "Weapon|Carry") FVector CarryShouldered = FVector(22.0f, 11.0f, -9.0f);
	UPROPERTY(EditAnywhere, Category = "Weapon|Carry") FVector CarryHipFire = FVector(26.0f, 17.0f, -26.0f);
	UPROPERTY(EditAnywhere, Category = "Weapon|Carry") FVector CarryLowReady = FVector(24.0f, 12.0f, -28.0f);   // a little higher and closer to the body than the first pass (28, 14, -34)
	// FIRST PERSON low ready. The numbers above hang the weapon 34 cm under the eye, which is
	// right for a body seen from outside and wrong for a camera: a 90 degree lens on a 16:9
	// frame reaches only 29 degrees below its axis, so at (28, 14, -34) the whole gun sat under
	// the bottom edge of the screen and read as missing. From the camera the piece must stay in
	// frame: higher, and drooped less. Same solve, different numbers.
	UPROPERTY(EditAnywhere, Category = "Weapon|Carry") FVector CarryLowReadyFirstPerson = FVector(27.0f, 10.0f, -10.0f);   // likewise, from (32, 12, -14)
	UPROPERTY(EditAnywhere, Category = "Weapon|Carry") float LowReadyDropCmFirstPerson = 175.0f;
	// Low ready is aimed too -- just at the deck a few metres ahead rather than at the target.
	// Expressing it that way instead of as a fixed muzzle depression means it is the SAME
	// calculation as every other carry position with a nearer, lower convergence point, so
	// there is no special case in the solve and it self-corrects on a slope or a stair.
	// SWAY. A carried weapon moves with the stride: a lateral figure of eight, a dip at each
	// footfall and a touch of roll, all scaled by ground speed and almost gone in the sights.
	// Procedural, on top of whatever the clip does, so every weapon and every gait get it.
	UPROPERTY(EditAnywhere, Category = "Weapon|Sway") float SwayLateralCm = 1.6f;      // side to side, at full run
	UPROPERTY(EditAnywhere, Category = "Weapon|Sway") float SwayVerticalCm = 1.1f;     // the dip at each step, at full run
	UPROPERTY(EditAnywhere, Category = "Weapon|Sway") float SwayRollDegrees = 1.4f;    // the roll that goes with the lateral swing
	UPROPERTY(EditAnywhere, Category = "Weapon|Sway") float SwayStepsPerSecond = 2.3f; // stride rate at full run; slower gaits scale it down
	UPROPERTY(EditAnywhere, Category = "Weapon|Sway") float SwayAdsScale = 0.15f;      // what survives in the sights
	float SwayPhase = 0.0f;      // radians, one stride cycle per 2 pi
	float SwayAmount = 0.0f;     // smoothed 0..1 fraction of run speed
	FVector2D SwayOffset = FVector2D::ZeroVector;   // eye space: lateral, vertical (cm)
	float SwayRoll = 0.0f;
	void TickWeaponSway(float DeltaSeconds);
	UPROPERTY(EditAnywhere, Category = "Weapon|Carry") float LowReadyConvergeCm = 500.0f;
	UPROPERTY(EditAnywhere, Category = "Weapon|Carry") float LowReadyDropCm = 150.0f;
	// ---- Posture transitions ------------------------------------------------
	// Changing how a weapon is held takes TIME, and that time is most of what makes a weapon
	// feel like a weapon rather than a cursor. Pulling the trigger at low ready does not fire
	// into the deck and does not snap the rifle to the shoulder either: it starts the raise,
	// and the shot goes when the weapon arrives.
	//
	// All four are separate because they are not symmetrical in life. Coming down out of the
	// sights is quicker than going up into them, and getting a slung weapon up off the thigh
	// is the slowest thing here.
	UPROPERTY(EditAnywhere, Category = "Weapon|Carry") float RaiseFromLowReadySeconds = 0.34f;
	// LOW READY IS THE RESTING STATE. A shot raises the weapon to the shoulder and it stays there
	// for this long after the last shot or the last time the sights were used, then drops back.
	UPROPERTY(EditAnywhere, Category = "Weapon|Carry", meta = (ClampMin = "0.0")) float ShoulderedHoldSeconds = 5.0f;
	UPROPERTY(EditAnywhere, Category = "Weapon|Carry") float DropToLowReadySeconds = 0.26f;
	UPROPERTY(EditAnywhere, Category = "Weapon|Carry") float IntoSightsSeconds = 0.22f;
	UPROPERTY(EditAnywhere, Category = "Weapon|Carry") float OutOfSightsSeconds = 0.15f;
	// Anything else -- shouldered to hip and back.
	UPROPERTY(EditAnywhere, Category = "Weapon|Carry") float CarryChangeSeconds = 0.18f;

	// Starts the move to a posture that can be fired from. Returns the seconds to wait, or 0.
	UFUNCTION(BlueprintCallable, Category = "Weapon") float RaiseToShoulder();
	// How far out the point of aim is taken to be when nothing is hit. Convergence at infinity
	// and convergence at two metres look very different when the weapon is held off to one side.
	UPROPERTY(EditAnywhere, Category = "Weapon|Carry") float AimConvergeCm = 2500.0f;
	// Sprinting drops the weapon: nobody runs with a rifle in their face.
	UPROPERTY(EditAnywhere, Category = "Weapon|Carry") bool bLowReadyWhileSprinting = true;

	// ---- Sights (first person) ----------------------------------------------
	// Aiming down a gun's sights means one thing exactly: the eye, the rear sight and the
	// muzzle are on one line, and that line is where the camera is looking. The ADS animation
	// brings the weapon most of the way there; this closes the last few centimetres, which is
	// the difference between "holding a rifle near his face" and "looking through it".
	//
	// It is a CORRECTION, not a placement -- the weapon still rides the hand and the animation
	// still does the work. Turning AimSightAlignment down to 0 gives the raw animation back.
	UPROPERTY(EditAnywhere, Category = "Weapon|Sights", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AimSightAlignment = 1.0f;
	// How far in front of the eye the rear sight sits. Real eye relief on a rifle is about 8 cm;
	// a little more here keeps the receiver from filling the screen at a game field of view.
	UPROPERTY(EditAnywhere, Category = "Weapon|Sights") float AimEyeReliefCm = 16.0f;
	// Nudge, in camera space (right, up), for weapons whose sight line sits off centre.
	// ZERO. The shot is traced down the eye line, so the sight point (the optic's eye, the
	// holosight dot) has to sit ON that line: the 1.5 cm this used to drop it, at 16 cm of eye
	// relief, put the dot five degrees under where the shot went -- a metre off at ten metres.
	UPROPERTY(EditAnywhere, Category = "Weapon|Sights") FVector2D AimSightNudgeCm = FVector2D(0.0f, 0.0f);
	// How long the weapon takes to move between hand placement and sight placement. A duration,
	// not a rate -- see FTimedBlend.
	UPROPERTY(EditAnywhere, Category = "Weapon|Sights", meta = (ClampMin = "0.02")) float AimSightBlendSeconds = 0.20f;
	// Where the eye should line up, in the weapon mesh's own space. Set by the controller from
	// the catalogue when a weapon is drawn; zero means the weapon has no derived sight and the
	// alignment falls back to the bore line.
	void SetWeaponSight(const FVector& SightLocal, bool bHasSight, float SightPitch);
	bool IsFirstPerson() const { return bInFirstPerson; }
	// UNSTUCK. Every couple of seconds on solid ground the spot is remembered; asked to get
	// unstuck, the character goes back to the newest remembered spot that is a stride away and
	// has room for the capsule, and failing every one of those, to the player start. Returns
	// false only when nothing at all could be found.
	bool TryUnstuck();
	TArray<FVector> GoodSpots;
	float GoodSpotClock = 0.0f;
	void TickGoodSpots(float DeltaSeconds);
	// The trigger hand's weapon-space correction (see TriggerHandRotation); the console's HandRot goes through here.
	void SetTriggerHandRotation(const FRotator& R);
	FRotator GetTriggerHandRotation() const { return TriggerHandRotation; }
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void SetWeaponMuzzle(const FVector& MuzzleLocal);


	// ---- Hand IK ------------------------------------------------------------
	// Where the support hand wraps this weapon, in the weapon's own space.
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void SetWeaponForeGrip(const FVector& ForeGripLocal, bool bHasForeGrip, float ForeGripPitch = 0.0f);
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void SetWeaponHipFire(bool bHipFire);

	// How strongly each hand is pulled onto the weapon. The support hand is the one that
	// matters: it is the hand that visibly misses the handguard when the animation was made
	// for a different length of gun. Zero either to see the raw animation.
	UPROPERTY(EditAnywhere, Category = "Weapon|Hand IK", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SupportHandIKWeight = 1.0f;
	UPROPERTY(EditAnywhere, Category = "Weapon|Hand IK", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TriggerHandIKWeight = 1.0f;
	UPROPERTY(EditAnywhere, Category = "Weapon|Hand IK", meta = (ClampMin = "0.1"))
	float HandIKBlendSeconds = 0.15f;
	// The support hand's wrist, relative to the weapon. A hand placed correctly but rotated
	// wrongly grips the handguard sideways, so this is as much a part of the answer as the
	// position is. Tuned by eye against the rifle, shared by every two-handed weapon.
	UPROPERTY(EditAnywhere, Category = "Weapon|Hand IK")
	FRotator SupportHandRotation = FRotator(0.0f, 0.0f, 90.0f);
	// THE TRIGGER HAND'S CORRECTION, in weapon space, on top of the WeaponGrip_R socket. Probed
	// live (2026-09-16) with the socket at identity: the wrist sat 2.8 cm to the LEFT of the
	// grip and the palm faced down -- the right hand taking the grip from the wrong side. In the
	// weapon's frame the measured hand basis was fingers (0.89, 0.34, 0.32), thumb side
	// (0.12, -0.84, 0.53), palm (0.45, -0.43, -0.79); roll +58 then yaw -20 turns that into
	// fingers forward, thumb up, palm to the left, which is a right hand on a pistol grip. It is
	// applied to the socket hold and to the IK target alike, so the two agree; the console
	// command HandRot (pitch yaw roll) tunes it live, and the number that looks right belongs
	// in the socket itself.
	UPROPERTY(EditAnywhere, Category = "Weapon|Hands") FRotator TriggerHandRotation = FRotator(0.0f, -20.0f, 58.0f);
	// The weapon's transform under the grip socket that realises TriggerHandRotation.
	// The mesh under the grip socket: turned by the trigger-hand correction, and shifted so the
	// weapon's own GRIP point (mesh space, usually the origin) sits at the socket.
	FTransform WeaponOnSocket() const { const FQuat R = TriggerHandRotation.Quaternion().Inverse(); return FTransform(R, -R.RotateVector(WeaponGripLocal)); }
	FVector WeaponGripLocal = FVector::ZeroVector;
	void SetWeaponGrip(const FVector& GripLocal);

	// Whether the body should face the way the camera is looking rather than the way it is
	// walking. True in first person, and true ANY TIME A WEAPON IS OUT: a man carrying a rifle
	// turns to face what he is pointing it at, and lets his feet cross over to do it. This is
	// also what brings the stance's 8-way strafe clips into play -- with orient-to-movement on,
	// the character is always walking forwards and they never get used.
	// ---- Free look ----------------------------------------------------------
	// Held, the camera swings on its own and the character does not follow it: he goes on
	// facing, and aiming, wherever he already was. Letting go brings the camera back to him.
	//
	// The consequence worth being explicit about: while this is held the camera is NO LONGER
	// the aim. Everything that used to read the camera's forward vector -- the shot, the
	// reticle, the body's facing -- has to ask GetAimRotation instead, or the player looks
	// left and shoots left while the rifle is still pointing down the corridor.
	UFUNCTION(BlueprintCallable, Category = "Camera") void SetFreelook(bool bOn);
	UFUNCTION(BlueprintPure, Category = "Camera") bool IsFreelook() const { return bFreelook; }
	// Where the character is actually aiming: frozen while free looking, the camera otherwise.
	UFUNCTION(BlueprintPure, Category = "Camera") FRotator GetAimRotation() const;
	// Where that aim starts from. The camera normally, but while free looking the camera has
	// wandered off, so it is the character's own eye line instead.
	UFUNCTION(BlueprintPure, Category = "Camera") FVector GetAimOrigin() const;

	UFUNCTION(BlueprintPure, Category = "Weapon") bool ShouldFaceAim() const;
	// Pushes ShouldFaceAim() onto the movement component. Called when the camera changes AND
	// when a weapon is drawn or put away, since both change the answer.
	void ApplyFacingMode();

	UPROPERTY(EditAnywhere, Category = "Weapon", meta = (ClampMin = "0.0")) float RecoilPitchDegrees = 0.9f;
	// The kick comes back. Without recovery every shot walks the view up permanently and the
	// player is forever dragging it down; with it the view settles over this long, which is
	// what makes a burst feel like a burst rather than a climb.
	UPROPERTY(EditAnywhere, Category = "Weapon|Recoil", meta = (ClampMin = "0.0")) float RecoilRecoverSeconds = 0.15f;
	// How much of each kick is given back. 1 returns exactly to the pre-shot aim; a little less
	// leaves a trace of climb to fight, which is what sustained fire should cost.
	UPROPERTY(EditAnywhere, Category = "Weapon|Recoil", meta = (ClampMin = "0.0", ClampMax = "1.0")) float RecoilRecoverFraction = 0.85f;
	// The four postures kick differently. This, with the spread cone, is what makes them FEEL
	// different rather than merely look different.
	UPROPERTY(EditAnywhere, Category = "Weapon|Recoil") float RecoilScaleADS = 0.6f;
	UPROPERTY(EditAnywhere, Category = "Weapon|Recoil") float RecoilScaleShouldered = 1.0f;
	UPROPERTY(EditAnywhere, Category = "Weapon|Recoil") float RecoilScaleHipFire = 1.6f;
	UPROPERTY(EditAnywhere, Category = "Weapon|Recoil") float RecoilScaleLowReady = 1.0f;
	// The torso follows the aim pitch, so a steep aim is a body leaning into it rather than two
	// arms stretched at a gun. Fraction of the aim pitch shared across the spine bones; negative
	// if it turns out to lean the wrong way on this rig.
	UPROPERTY(EditAnywhere, Category = "Weapon|Posture", meta = (ClampMin = "-1.0", ClampMax = "1.0")) float SpineLeanFraction = 0.35f;
	UPROPERTY(EditAnywhere, Category = "Weapon|Posture", meta = (ClampMin = "0.02")) float SpineLeanBlendSeconds = 0.30f;
	UPROPERTY(EditAnywhere, Category = "Weapon", meta = (ClampMin = "0.0")) float MuzzleFlashSeconds = 0.045f;
	// The smoke off the muzzle: which system, how big, and how long it is allowed to emit.
	UPROPERTY(EditAnywhere, Category = "Weapon") FString MuzzleSmokeSystem = TEXT("/Game/PolygonSciFiWorlds/FX/Niagara/NS_Smoke_Small_01");
	UPROPERTY(EditAnywhere, Category = "Weapon", meta = (ClampMin = "0.0")) float MuzzleSmokeScale = 0.05f;
	UPROPERTY(EditAnywhere, Category = "Weapon", meta = (ClampMin = "0.0")) float MuzzleSmokeSeconds = 0.04f;

	UFUNCTION(BlueprintCallable, Category = "Light") void ToggleHeadlamp();
	// The lamp is mounted on the head BONE, so left alone it points wherever the animation is
	// looking -- which is never quite where the player is aiming, and in first person is not
	// even close. Every frame it is re-aimed at a point down the aim ray instead, so the hot
	// spot sits on the reticle. Converging on a point rather than simply copying the aim
	// direction matters because the lamp is on the brow, a few centimetres off the eye.
	UPROPERTY(EditAnywhere, Category = "Light", meta = (ClampMin = "100.0"))
	float HeadlampConvergeCm = 1500.0f;
	UFUNCTION(BlueprintPure, Category = "Light") bool IsHeadlampOn() const;
	// Walks to the seat under its own locomotion, then SitOn blends into the seated loop.
	UFUNCTION(BlueprintCallable, Category = "Animation") void BeginSit(AActor* Seat, float SeatHeight, float FacingYaw, float LeanDegrees = 0.0f, float HunchDegrees = 0.0f);
	UFUNCTION(BlueprintCallable, Category = "Animation") void SitOn(AActor* Seat, float SeatHeight, float FacingYaw, float LeanDegrees = 0.0f, float HunchDegrees = 0.0f);
	UFUNCTION(BlueprintPure, Category = "Animation") bool IsApproachingSeat() const { return ApproachSeat.IsValid(); }
	UFUNCTION(BlueprintCallable, Category = "Animation") void StandUp();
	UFUNCTION(BlueprintPure, Category = "Animation") bool IsSitting() const { return SeatActor.IsValid(); }
	AActor* GetSeat() const { return SeatActor.Get(); }
	// True while a looped try-out clip is being held (so a new pick in the
	// browser can swap the loop in place).
	UFUNCTION(BlueprintPure, Category = "Animation")
	bool IsAnimationClipLooping() const { return bIsAttacking && bCombatHolding; }

	// The face's skin color when the character can state it outright (the
	// modular hero's Color_Skin material parameter). False for single-mesh
	// characters, whose skin is painted in the atlas -- AFaceController then
	// samples it (see MatchNoseToSkin).
	bool ResolveSkinColor(FLinearColor& OutColor) const;
	void MatchNoseToSkin();

	// Gait adjustments (see GaitAdjustments.h): whole struct, or one field by
	// its property name ("StanceWidthDegrees", "LimpAmount", ...). LimpSide
	// is the one string field ("Left"/"Right").
	UFUNCTION(BlueprintCallable, Category = "Character Config")
	void SetGaitAdjustments(const FGaitAdjustments& NewGait);
	UFUNCTION(BlueprintCallable, Category = "Character Config")
	void SetGaitAdjustment(const FString& FieldName, float Value);
	UFUNCTION(BlueprintCallable, Category = "Character Config")
	void SetLimpSide(const FString& Side);
	UFUNCTION(BlueprintPure, Category = "Character Config")
	float GetGaitAdjustment(const FString& FieldName) const;
	UFUNCTION(BlueprintPure, Category = "Character Config")
	FGaitAdjustments GetGaitAdjustments() const { return CurrentConfig.Gait; }

	// Actor click events (bEnableClickEvents on the controller) -- a second,
	// input-stack-independent way for an edit-mode click on this character
	// (or on its attached face actor) to reach the controller's selection.
	UFUNCTION()
	void HandleActorClicked(AActor* TouchedActor, FKey ButtonPressed);
	void SetPart(const FString& Slot, const FString& AssetPath);
	void SetPartColor(const FString& Parameter, const FLinearColor& Color);
	FLinearColor GetPartColor(const FString& Parameter) const;
	void SetCharacterScale(const FVector& NewScale);
	void SetSpeedMultiplier(float NewMultiplier);

	// Tags: freeform string labels stored in the config, saved to JSON, and
	// mirrored onto the actor's own Tags so AActor::ActorHasTag works. The
	// manager edits them as a comma-separated list.
	UFUNCTION(BlueprintCallable, Category = "Character Config")
	void SetCharacterTags(const FString& CommaSeparatedTags);
	// Inspect menu text (see FCharacterConfig::Description / Comment).
	UFUNCTION(BlueprintCallable, Category = "Character Config")
	void SetDescription(const FString& Text) { CurrentConfig.Description = Text; }
	UFUNCTION(BlueprintCallable, Category = "Character Config")
	void SetComment(const FString& Text) { CurrentConfig.Comment = Text; }
	UFUNCTION(BlueprintPure, Category = "Character Config")
	FString GetDescription() const { return CurrentConfig.Description; }
	UFUNCTION(BlueprintPure, Category = "Character Config")
	FString GetComment() const { return CurrentConfig.Comment; }
	UFUNCTION(BlueprintCallable, Category = "Character Config")
	FString GetCharacterTagsString() const;

	// ---- Combat capabilities -----------------------------------------------
	// Invokable actions assembled from the Synty Sword Combat pack by the
	// clips' own naming (see CombatAnimLibrary.h for the conventions). Each
	// plays a SEQUENCE of in-place clips: an attack then its own ReturnToIdle
	// recovery; Begin -> Loop (held until the matching End call) -> End for
	// block/knockdown/stun/taunt; a death then its held _Pose. Movement input
	// is blocked for the duration (IsInCombatAction). Attacks/blocks/dodges
	// refuse while another action is playing (return false); hits, knockdown,
	// stun and death interrupt anything.
	//
	//  AttackName: "LightCombo01A", "HeavyStab01", "LightFencing01", ...
	//  ComboName:  "LightCombo01" / "HeavyCombo01"  (steps A->B->C then recovery)
	//  Dir:        "F" | "B" | "L" | "R"   ParryDir: "F" | "L" | "R"
	//  CounterKind:"CounterShove" | "PommelStrike"
	UFUNCTION(BlueprintCallable, Category = "Combat") bool CombatAttack(const FString& AttackName);
	UFUNCTION(BlueprintCallable, Category = "Combat") bool CombatCombo(const FString& ComboName, int32 NumSteps = 3);
	UFUNCTION(BlueprintCallable, Category = "Combat") bool CombatBlockBegin();
	UFUNCTION(BlueprintCallable, Category = "Combat") void CombatBlockEnd();
	UFUNCTION(BlueprintCallable, Category = "Combat") bool CombatParry(const FString& ParryDir);
	UFUNCTION(BlueprintCallable, Category = "Combat") bool CombatParryCounter(const FString& CounterKind);
	UFUNCTION(BlueprintCallable, Category = "Combat") bool CombatParryBreak();
	UFUNCTION(BlueprintCallable, Category = "Combat") bool CombatDodge(const FString& Dir, bool bRoll);
	UFUNCTION(BlueprintCallable, Category = "Combat") bool CombatHit(const FString& Dir, bool bStagger);
	UFUNCTION(BlueprintCallable, Category = "Combat") bool CombatKnockDown();
	UFUNCTION(BlueprintCallable, Category = "Combat") void CombatGetUp();
	UFUNCTION(BlueprintCallable, Category = "Combat") bool CombatStun();
	UFUNCTION(BlueprintCallable, Category = "Combat") void CombatRecover();
	UFUNCTION(BlueprintCallable, Category = "Combat") bool CombatDie(const FString& Dir);
	UFUNCTION(BlueprintCallable, Category = "Combat") bool CombatTauntBegin();
	UFUNCTION(BlueprintCallable, Category = "Combat") void CombatTauntEnd();
	UFUNCTION(BlueprintCallable, Category = "Combat") bool CombatFidget(int32 Index);
	UFUNCTION(BlueprintCallable, Category = "Combat") bool CombatDrawSword();
	UFUNCTION(BlueprintCallable, Category = "Combat") bool CombatSheatheSword();
	UFUNCTION(BlueprintCallable, Category = "Combat") bool CombatReadyStance(bool bReady);
	UFUNCTION(BlueprintPure, Category = "Combat") bool IsInCombatAction() const { return bIsAttacking; }
	UFUNCTION(BlueprintPure, Category = "Combat") bool IsBlocking() const { return bBlocking; }
	// The clip currently playing in the sequence, "" if none.
	UFUNCTION(BlueprintPure, Category = "Combat") FString GetCurrentCombatClip() const;
	// The invokable capability names, and the pack's grouping as read from
	// the clip names.
	UFUNCTION(BlueprintPure, Category = "Combat") TArray<FString> GetCombatCapabilities() const;
	UFUNCTION(BlueprintPure, Category = "Combat") FString DescribeCombatAnims();
	UFUNCTION(BlueprintPure, Category = "Combat") TArray<FString> GetCombatClipKeys();
	void SetArmPose(UAnimSequence* Pose);
	void SetArmPoseWeight(float Weight);
	void SetGripCurl(float FingerDegrees, float ThumbDegrees);
	void SetFaceConfig(const FCharacterFaceConfig& NewFace);
	// Placement only (slider drags): updates the offsets on the live
	// controller without re-attaching anything.
	void SetFaceOffsets(const FVector& NoseLocation, const FVector& MouthLocation);
	// The chooser's nose: mesh path (empty for none) and scale; enables the face.
	void SetNose(const FString& MeshPath, const FVector& Scale);
	void SetBrows(const FString& MeshPath, const FVector& Location, float Tilt, const FVector& Scale);
	void SetHairColor(const FLinearColor& Color);   // alpha 0 = as painted
	// Debug: re-applies the skin tint and reports what it found per part.
	UFUNCTION(BlueprintCallable, Category = "Debug") FString DebugSkinTint();
	void SetMouthLook(float Scale, const FLinearColor& Tint);

	// The FaceController this character is using (placed in the level and
	// attached, or spawned by ApplyFace), null until a config with the face
	// enabled has been applied.
	AFaceController* GetFaceController() const { return FaceController; }

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// While the Character Manager panel covers the left of the screen, the
	// camera slides sideways (CameraBoom SocketOffset, eased in Tick) so
	// the character is framed in the uncovered part. Negative Y = camera
	// moves left = character appears further right.
	UPROPERTY(EditAnywhere, Category = "Camera")
	float ManagerCameraSideOffset = -130.0f;

	void SetManagerCameraOffset(bool bEnabled) { bManagerCameraOffset = bEnabled; }

private:

	bool bManagerCameraOffset = false;

	// Pushes WeaponMesh/WeaponRelative* to WeaponMeshComponent. BeginPlay and
	// the SetWeapon* setters both go through here.
	void ApplyWeapon();

	// Body: Type dispatch. Modular switches GetMesh() to the Modular
	// Fantasy Hero base rig (hidden; it only drives the animation) with the
	// parts as leader-pose children; Single shows one ordinary mesh.
	void ApplyBody();
	void ApplyModularBody();
	void ApplySingleBody();
	void ApplyPart(const FString& Slot);
	void ApplyColors();
	void ApplyScale();

	// Face systems: finds the attached AFaceController or spawns one, and
	// wires nose / mouth decal / expression / blink from CurrentConfig.Face.
	void ApplyFace();

	// Called whenever GetMesh()'s skeletal mesh changes: registers the mesh's
	// skeleton as compatible with the animation skeleton (so the clips play
	// on it at all), re-points the grip correction at the rig's finger
	// naming, and refreshes the anim instance pointer the face uses.
	void OnBaseMeshChanged();

	// Rotation that takes an offset authored in a Mannequin bone's space
	// (head for the face, hand_r for the weapon) into this mesh's own
	// bone space -- identity for Mannequin rigs. Same per-bone delta the
	// anim proxy's orientation retarget uses.
	FQuat ComputeBoneFrameConversion(FName BoneName) const;

	// Keeps CurrentConfig's weapon/arm-pose fields in step with the live
	// UPROPERTYs / anim instance, so a save records what's actually shown.
	void SyncConfigFromLive();

	FCharacterConfig CurrentConfig;
	FString SavedConfigJson;   // see IsConfigDirty
	bool bMarkSavedOnFirstTick = false;   // baseline taken once the anim instance exists
	bool bModularMode = false;

	// See SetHoverHighlight / SetSelectedHighlight.
	bool bHoverHighlight = false;
	bool bSelectedHighlight = false;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> HighlightMID;
	void ApplyHighlightOverlay();

	UPROPERTY()
	TMap<FString, TObjectPtr<USkeletalMeshComponent>> PartComponents;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> ModularMID;

	UPROPERTY()
	TObjectPtr<AFaceController> FaceController;

	// What the level gave this instance before any config was applied --
	// the "default" a Single config resets to.
	UPROPERTY() TObjectPtr<USkeletalMesh> OriginalBaseMesh;
	UPROPERTY() TObjectPtr<UMaterialInterface> OriginalMaterial;
	// Stance state. WeaponActionLeft counts a one-shot down so the held idle
	// can be put back when it finishes -- the arm layer holds a non-looping
	// clip's last frame forever otherwise.
	FString WeaponStance;
	bool bAiming = false;
	bool bFreelook = false;
	FRotator FrozenAim = FRotator::ZeroRotator;
	// Letting go of the key no longer snaps the camera back: the look is LEFT where it is, aim
	// still frozen, until the player moves or raises the sights. Looking over your shoulder and
	// then standing still should not yank the view around the moment a finger lifts.
	bool bFreelookLingering = false;
	void EndFreelookNow();
	float WeaponActionLeft = 0.0f;
	float SpreadBloomDegrees = 0.0f;
	// Re-applies Idle_ADS or Idle_Hipfire, whichever the current aim state
	// wants, onto the arm-override layer.
	void RefreshWeaponStancePose();
	void TickWeaponStance(float DeltaSeconds);
	// Eases the weapon onto the view axis while aiming in first person, and releases it again.
	void TickSightAlignment(float DeltaSeconds);
	// ---- The eye, and the weapon placed from it -------------------------------------------
	// Tick works out where the camera WILL be at the end of this frame (PredictEye) and places
	// the weapon and the hands' IK targets from that; PostCameraTick then places the weapon once
	// more from where the camera actually IS, after the camera manager has run. In first person
	// the prediction is exact (the boom is placed by this Tick, the arm is zero length, the
	// rotation is the control rotation, and nothing modifies the camera after that), so the two
	// agree and the hands and the weapon are locked to the view together.
	void PredictEye();
	bool GetEye(bool bFinal, FVector& OutLoc, FRotator& OutRot) const;
	FTransform SolveWeaponPose(const FVector& EyeLoc, const FRotator& EyeRot) const;
	void PlaceWeapon(const FVector& EyeLoc, const FRotator& EyeRot);
	void PostCameraTick(float DeltaSeconds);
	FWeaponPostCameraTick PostCameraTickFunction;
	FVector PredictedEyeLoc = FVector::ZeroVector;
	FRotator PredictedEyeRot = FRotator::ZeroRotator;
	bool bPredictedEyeValid = false;
	// ---- The lag test: numbers instead of eyes ------------------------------------------------
	// WeaponLagTest N in the console (or pc.weapon_lag_test(N) from Python) turns the view and
	// walks forward for N seconds while measuring, every frame, (a) how far the predicted eye
	// was from the final camera and (b) how far the weapon moved IN CAMERA SPACE between frames
	// outside any carry transition -- which is exactly the judder. The report is logged and kept.
	void BeginWeaponLagTest(float Seconds);
	void FinishWeaponLagTest();
	FString LastLagReport;
	float LagTestLeft = 0.0f, LagTestClock = 0.0f;
	int32 LagFrames = 0, LagJumpFrames = 0;
	double LagCamErrSum = 0.0, LagCamErrMax = 0.0, LagRotErrMax = 0.0, LagJumpSum = 0.0, LagJumpMax = 0.0, LagRotJumpMax = 0.0, LagTurned = 0.0;
	FTransform LagPrevWeaponInCamera;
	bool bLagPrevValid = false;
	float LagPrevYaw = 0.0f;
	void TickHeadlamp();
	// Works out where each hand has to be for this weapon, and hands it to the anim instance.
	void TickHandIK(float DeltaSeconds);
	FVector WeaponForeGripLocal = FVector::ZeroVector;
	bool bWeaponHasForeGrip = false;
	float SupportIKAlpha = 0.0f;
	float TriggerIKAlpha = 0.0f;
	FTimedBlend SupportIKBlend, TriggerIKBlend, SightBlend, LeanBlend;
	float WeaponForeGripPitch = 0.0f;
	bool bWeaponHipFire = false;
	// Which carry the arm clip currently represents. The clip is swapped at the midpoint of a
	// carry move rather than the instant the button is pressed, so the arms and the weapon agree.
	bool bPosedForADS = false;
	float RecoilToRecover = 0.0f;
	float RecoilRecoverLeft = 0.0f;
	void TickRecoil(float DeltaSeconds);
	FVector WeaponMuzzleLocal = FVector::ZeroVector;
	// 0 hip-ish, 1 sights, eased through a carry move: what the spread cone and the clip follow.
	float AdsAlpha() const;
	// The carry the arms should be posed for right now, given where a move has got to.
	EWeaponCarry PosedCarry() const;
	FVector WeaponSightLocal = FVector::ZeroVector;
	bool bWeaponHasSight = false;
	// Degrees to pitch the weapon up so its sight line is the line of the shot.
	float WeaponSightPitch = 0.0f;
	bool bWeaponHasOptic = false;
	// Whether the weapon is currently being driven by the geometric solve rather than hanging
	// off its socket. Needed so the hand-off back to the socket happens exactly once.
	bool bSightAlignActive = false;
	// Seconds left of a commanded raise out of low ready, which outranks everything but the
	// sights while it lasts.
	float ForceShoulderLeft = 0.0f;
	// The posture the weapon is in, the one it came from, and how long is left of the move.
	EWeaponCarry CurrentCarry = EWeaponCarry::LowReady;
	EWeaponCarry CarryFrom = EWeaponCarry::LowReady;
	float CarryBlendLeft = 0.0f;
	float CarryBlendTotal = 0.0f;
	void TickCarry(float DeltaSeconds);
	// The eye offset for one posture, and how long moving between two of them takes.
	FVector CarryOffset(EWeaponCarry Carry) const;
	float CarryTransitionSeconds(EWeaponCarry From, EWeaponCarry To) const;
	float SightAlignAlpha = 0.0f;

	UPROPERTY() TObjectPtr<UStaticMesh> OriginalWeaponMesh;
	FVector OriginalWeaponLocation = FVector::ZeroVector;
	FRotator OriginalWeaponRotation = FRotator::ZeroRotator;
	UPROPERTY() TObjectPtr<UAnimSequence> DefaultArmPose;

	int32 SelectedCombatAnimIndex = 0;
};
