// Standalone actor that manages a character's full facial expression --
// mouth (a DecalComponent, entirely independent of the animation system)
// plus eyes/brow (writing directly onto whatever UCharacterAnimInstance the
// target character's mesh is already using as its MAIN AnimInstance --
// independent of whatever body animation that same instance is showing
// underneath, since it applies facial bone edits as a separate pass; see
// CharacterAnimInstance.h). One named expression drives both regions
// together via SetExpression(), with either half optional per expression.
//
// Previously drove a SEPARATE post-process AnimBP (ABP_KnightFace) for the
// eyes/brow half -- retired because a post-process AnimBP needs a working
// "Input Pose" node to receive the main pose or it silently discards it and
// shows a broken/static body (confirmed live), and because the user prefers
// native C++ over touching Unreal's Blueprint/AnimGraph editor wherever
// there's a viable alternative. Writing straight onto the character's own
// single main AnimInstance sidesteps that whole problem category.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Characters/FacialExpressionInterface.h"
#include "Characters/FacialExpressionPose.h"
#include "FaceController.generated.h"

class UDecalComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTexture2D;
class USkeletalMeshComponent;
class UCharacterAnimInstance;
class UStaticMesh;
class UStaticMeshComponent;

// The eyes' resting (non-blinking) openness. Blinking still happens on top
// of whichever of these is current -- it dips to fully closed and returns
// to this rest level, not necessarily to fully open.
UENUM(BlueprintType)
enum class EEyeOpenness : uint8
{
	Wide,
	Normal,
	Narrow,
	Squint,
};

// Vertical position of the (single, combined) Eyebrows bone.
UENUM(BlueprintType)
enum class EBrowHeight : uint8
{
	Low,
	Normal,
	High,
	Highest,
};

// Tilt of the same bone about its front-to-back axis. The rig has no
// separate left/right eyebrow bones, so an asymmetric "one eyebrow raised"
// look comes from rotating this one bar rather than moving two bones --
// Left/HighLeft raise the left end (and dip the right), Right/HighRight the
// reverse.
UENUM(BlueprintType)
enum class EBrowAngle : uint8
{
	HighLeft,
	Left,
	Normal,
	Right,
	HighRight,
};

UCLASS()
class REPLICAN_API AFaceController : public AActor, public IFacialExpressionInterface
{
	GENERATED_BODY()

public:

	AFaceController();

	// Attaches this controller's decal to the given skeletal mesh component
	// at the given bone/socket, preserving the decal's current world
	// transform (set its relative/world transform beforehand to position it
	// where you want it to land). Also resolves the character's own
	// UCharacterAnimInstance from SkeletalMesh->GetAnimInstance() -- eyes/
	// brow control writes directly onto that same instance's facial fields,
	// which it applies as a bone-edit pass independent of whatever body
	// pose it's showing. Does nothing to eyes/brow if the mesh isn't
	// already using a UCharacterAnimInstance as its main AnimInstance.
	UFUNCTION(BlueprintCallable, Category = "Face")
	void AttachToCharacter(USkeletalMeshComponent* SkeletalMesh, FName BoneName);

	// Hides (or reveals) the nose/hair/facial-hair/head-gear attachments from
	// the possessing player's OWN camera specifically (SetOwnerNoSee, same
	// technique the character's own mesh uses for first person -- see
	// ABaseCharacter::Tick) without affecting how this character looks to
	// anyone else. Meant to be called whenever the owning character crosses
	// the first/third-person boundary: at close range these attachments
	// (the nose especially) sit close enough to a first-person camera to
	// visibly poke into view.
	UFUNCTION(BlueprintCallable, Category = "Face")
	void SetHiddenFromOwner(bool bHideFromOwner);

	// Swaps the active mouth decal texture AND the eyes/brow pose by name.
	// Either half is optional -- an expression only in StateTextures just
	// swaps the mouth, one only in ExpressionPoses just moves eyes/brow.
	// Logs a warning if StateName isn't registered in either map.
	UFUNCTION(BlueprintCallable, Category = "Face")
	void SetExpression(FName StateName);

	// Swaps only the mouth decal texture, leaving eyes/brow pose untouched --
	// use this when mouth selection should stay on its own independent axis
	// (e.g. UI buttons), since SetExpression's combined mouth+pose lookup by
	// the same name is what you want for keyboard-driven full expressions
	// but not for a control panel with separate eye-state buttons.
	UFUNCTION(BlueprintCallable, Category = "Face")
	void SetMouthTexture(FName StateName);

	// Starts (true) or stops (false) an automatic "silent conversation" mouth
	// cycle: hops between Neutral and Talking1/2/3 on randomized timing, with
	// occasional longer pauses on Neutral so it reads as pauses between
	// bursts of talking rather than constant motion. Any explicit mouth
	// selection via SetMouthTexture (i.e. clicking a different mouth button)
	// stops it automatically.
	UFUNCTION(BlueprintCallable, Category = "Face")
	void SetTalkingMode(bool bEnable);

	UFUNCTION(BlueprintPure, Category = "Face")
	FName GetCurrentExpression() const { return CurrentState; }

	// IFacialExpressionInterface
	virtual void SetMouthExpression_Implementation(FName ExpressionName) override { SetExpression(ExpressionName); }

	// Shared mouth material (must expose a texture parameter named
	// "MouthTexture", e.g. M_Mouth). One dynamic instance of this is
	// created at BeginPlay and reused for every expression -- swapping the
	// mouth just changes which texture that one instance samples, instead
	// of swapping between several near-duplicate Material assets that only
	// differed by which texture they hardcoded.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face")
	TObjectPtr<UMaterialInterface> BaseMouthMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face")
	TMap<FName, TObjectPtr<UTexture2D>> StateTextures;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face")
	TMap<FName, FFacialExpressionPose> ExpressionPoses;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face")
	FName DefaultState = TEXT("Neutral");

	UPROPERTY(VisibleAnywhere, Category = "Face")
	TObjectPtr<UDecalComponent> Decal;

	// Automatic blinking, entirely independent of expression selection --
	// runs on its own timer regardless of which mouth/eyes/brow state is
	// currently active.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face|Blink")
	bool bAutoBlink = true;

	// Random range between the start of one blink and the next.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face|Blink")
	float BlinkIntervalMin = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face|Blink")
	float BlinkIntervalMax = 5.5f;

	// How long the eyes stay closed before reopening.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face|Blink")
	float BlinkCloseDuration = 0.12f;

	// Sets the eyes' resting openness (applied immediately, and used as the
	// level blinks return to). Independent of expression selection, same as
	// blinking itself.
	UFUNCTION(BlueprintCallable, Category = "Face|Blink")
	void SetEyeOpenness(EEyeOpenness NewOpenness);

	UFUNCTION(BlueprintPure, Category = "Face|Blink")
	EEyeOpenness GetEyeOpenness() const { return CurrentEyeOpenness; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face|Blink")
	float WideEyeScale = 1.0f;

	// This is a uniform 3-axis scale on the eyes bone (see
	// UCharacterAnimInstance::ApplyFacialBoneEdits), which shrinks the flat
	// Synty "visor" eye mesh toward its OWN bone pivot rather than closing it
	// like a real eyelid would -- fine for a genuine blink (brief, and the
	// pivot-shift artifact reads as a natural squint mid-motion) or a
	// deliberate Narrow/Squint expression, but NOT for the default resting
	// state: anything below 1.0 here permanently shows that pivot-shift
	// artifact (the bottom of the eye rectangle visibly cut off/receded into
	// the face) on every character all the time, which is what "Normal"
	// (the default EEyeOpenness) was doing at 0.85 before this was caught.
	// "Normal" should mean the eyes' actual authored size.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face|Blink")
	float NormalEyeScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face|Blink")
	float NarrowEyeScale = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face|Blink")
	float SquintEyeScale = 0.3f;

	// Sets the eyebrows' vertical position (applied immediately).
	UFUNCTION(BlueprintCallable, Category = "Face|Brow")
	void SetBrowHeight(EBrowHeight NewHeight);

	UFUNCTION(BlueprintPure, Category = "Face|Brow")
	EBrowHeight GetBrowHeight() const { return CurrentBrowHeight; }

	// Signs flipped relative to what you'd guess from the names -- positive Z
	// in this bone's local space moves the eyebrows down, not up, on this rig.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face|Brow")
	float LowBrowHeightCm = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face|Brow")
	float NormalBrowHeightCm = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face|Brow")
	float HighBrowHeightCm = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face|Brow")
	float HighestBrowHeightCm = -2.0f;

	// Sets the eyebrows' left/right tilt (applied immediately).
	UFUNCTION(BlueprintCallable, Category = "Face|Brow")
	void SetBrowAngle(EBrowAngle NewAngle);

	UFUNCTION(BlueprintPure, Category = "Face|Brow")
	EBrowAngle GetBrowAngle() const { return CurrentBrowAngle; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face|Brow")
	float LeftBrowAngleDeg = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face|Brow")
	float HighLeftBrowAngleDeg = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face|Brow")
	float RightBrowAngleDeg = -4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face|Brow")
	float HighRightBrowAngleDeg = -8.0f;

	// Optional nose, added as a separate mesh rigidly attached to the same
	// bone as the mouth decal, rather than welded into the character's own
	// mesh -- swapping which nose an instance uses is then just an asset
	// reference change, and leaving this unset skips the nose entirely for
	// characters/packs that don't need one.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face|Nose")
	TObjectPtr<UStaticMesh> NoseMesh;

	// Placement relative to the attach bone (e.g. "head"). Defaults are the
	// values dialed in for SM_NoseSharp on the Knight rig's head bone --
	// forward/up offset from the bone origin, a -90 pitch (the mesh's
	// authored "point away from face" axis mapped onto the bone's own
	// local-forward axis), and unit scale (size is baked into the mesh
	// itself, not scaled here). A different nose mesh or a different
	// character's head bone will likely need these re-tuned per instance.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face|Nose")
	FVector NoseRelativeLocation = FVector(6.89f, 13.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face|Nose")
	FRotator NoseRelativeRotation = FRotator(-90.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face|Nose")
	FVector NoseRelativeScale = FVector(1.0f, 1.0f, 1.0f);
	// Brows: one mesh on both sides, mirrored across the head's lateral axis.
	UPROPERTY() TObjectPtr<UStaticMesh> BrowMesh;
	FVector BrowLocation = FVector(12.0f, 12.3f, 4.3f);   // right brow, head-bone space
	float BrowTilt = 0.0f;                                  // degrees about the forward axis, mirrored
	FVector BrowScale = FVector(1.0f, 1.0f, 1.0f);
	void ApplyBrowPlacement();
	// Hair color on any piece whose material has Color_Hair (the hero pieces); alpha 0 = leave painted.
	FLinearColor HairColor = FLinearColor(0, 0, 0, 0);
	void ApplyHairColor();
	// Synty attachments snap to SOC_head; pieces baked into head-bone space (path contains /Hero/) go on the head bone.
	void AttachHeadPiece(UStaticMeshComponent* Piece, UStaticMesh* Mesh);
	// The scale the Synty attachment convention gave each slot, restored when a hero piece is swapped back out.
	TMap<TObjectPtr<UStaticMeshComponent>, FVector> SyntyPieceScale;

	UPROPERTY(VisibleAnywhere, Category = "Face|Nose")
	TObjectPtr<UStaticMeshComponent> NoseComponent;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> BrowLComponent;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> BrowRComponent;

	// Mouth decal placement relative to the attach bone, in the SAME head
	// frame the nose offsets use (the Mannequin-rig head axes). Defaults
	// measured from the working knight in Lvl_ForestGlade.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face|Mouth")
	FVector MouthRelativeLocation = FVector(-1.06f, 13.70f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face|Mouth")
	FRotator MouthRelativeRotation = FRotator(0.0f, -90.0f, -90.0f);

	UFUNCTION(BlueprintCallable, Category = "Face|Mouth")
	void SetMouthRelativeLocation(const FVector& NewLocation);

	UFUNCTION(BlueprintCallable, Category = "Face|Mouth")
	void SetMouthRelativeRotation(const FRotator& NewRotation);

	UFUNCTION(BlueprintCallable, Category = "Face|Mouth")
	void SetMouthDecalEnabled(bool bEnabled);

	// Decal footprint multiplier (its Y/Z; X is projection depth) and a
	// color multiplied into the mouth texture (the material's "Tint").
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face|Mouth")
	float MouthScale = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face|Mouth")
	FLinearColor MouthTint = FLinearColor::White;

	UFUNCTION(BlueprintCallable, Category = "Face|Mouth")
	void SetMouthScale(float NewScale);

	UFUNCTION(BlueprintCallable, Category = "Face|Mouth")
	void SetMouthTint(const FLinearColor& NewTint);

	// Every mouth state name registered in StateTextures, for a picker.
	TArray<FName> GetMouthStateNames() const;

	// Turns automatic blinking on/off at runtime (BeginPlay only reads
	// bAutoBlink once).
	UFUNCTION(BlueprintCallable, Category = "Face|Blink")
	void SetAutoBlink(bool bEnabled);

	// Rigs whose head bone axes differ from the Mannequin's (the Modular
	// Fantasy Hero) get the nose/mouth offsets converted through this
	// rotation before they're applied -- offsets stay authored once, in
	// Mannequin head space. Identity for every Mannequin-convention rig.
	// See ABaseCharacter::ComputeHeadFrameConversion.
	void SetHeadFrameConversion(const FQuat& InConversion) { HeadFrameConversion = InConversion; }

	// Interchangeable head attachments (hair, facial hair, head gear), each
	// cycled through one at a time via a single button rather than one
	// button per option -- there are too many variants (7 hair, 2 beard, 9
	// hat/helmet) to give each its own button. "None" (no mesh shown) is
	// always reachable by cycling past the last option. Unlike the nose,
	// these Synty meshes are pre-authored to snap directly onto the
	// dedicated "SOC_head" socket with an identity relative transform --
	// no per-instance tuning needed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face|Hair")
	TArray<TObjectPtr<UStaticMesh>> HairOptions;

	UPROPERTY(VisibleAnywhere, Category = "Face|Hair")
	TObjectPtr<UStaticMeshComponent> HairComponent;

	UFUNCTION(BlueprintCallable, Category = "Face|Hair")
	void CycleHair();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face|FacialHair")
	TArray<TObjectPtr<UStaticMesh>> FacialHairOptions;

	UPROPERTY(VisibleAnywhere, Category = "Face|FacialHair")
	TObjectPtr<UStaticMeshComponent> FacialHairComponent;

	UFUNCTION(BlueprintCallable, Category = "Face|FacialHair")
	void CycleFacialHair();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face|HeadGear")
	TArray<TObjectPtr<UStaticMesh>> HeadGearOptions;

	UPROPERTY(VisibleAnywhere, Category = "Face|HeadGear")
	TObjectPtr<UStaticMeshComponent> HeadGearComponent;

	UFUNCTION(BlueprintCallable, Category = "Face|HeadGear")
	void CycleHeadGear();

	// Skin/costume palette variants (e.g. the shared trim-sheet material's
	// "_A"/"_B"/"_C" recolors). Cycling reassigns material slot 0 on the
	// character mesh itself AND on the nose/hair/facial-hair/head-gear
	// attachments, since all of them are meant to always match the
	// character (see AttachToCharacter's material-matching comment) --
	// leaving the attachments on a stale variant after cycling would
	// reintroduce the same mismatch that was just fixed there.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face|Material")
	TArray<TObjectPtr<UMaterialInterface>> MaterialOptions;

	UFUNCTION(BlueprintCallable, Category = "Face|Material")
	void CycleMaterial();

	// Direct-select counterparts of the Cycle*() functions, for a dropdown
	// (the CharacterBuilder panel) rather than a cycle button. -1 = None
	// for the attachments; out-of-range is treated as None / ignored.
	UFUNCTION(BlueprintCallable, Category = "Face|Hair")
	void SetHairIndex(int32 Index);
	// Any SOC_head hair mesh, not just the cycle list (null = none).
	UFUNCTION(BlueprintCallable, Category = "Face|Hair")
	void SetHairMesh(UStaticMesh* Mesh);
	void SetFacialHairMesh(UStaticMesh* Mesh);
	UStaticMesh* GetFacialHairMesh() const;
	UFUNCTION(BlueprintPure, Category = "Face|Hair")
	UStaticMesh* GetHairMesh() const;
	UFUNCTION(BlueprintCallable, Category = "Face|HeadGear")
	void SetHeadGearMesh(UStaticMesh* Mesh);
	// Offset from the SOC_head socket (applied on attach and by SetHeadGearMesh).
	UPROPERTY(EditAnywhere, Category = "Face|HeadGear") FVector HeadGearOffset = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, Category = "Face|HeadGear") float HeadGearScale = 1.0f;
	UFUNCTION(BlueprintPure, Category = "Face|HeadGear")
	UStaticMesh* GetHeadGearMesh() const;
	// A plate on the headgear (a cap badge): a plane in the headgear mesh's space, its +Z the face; null material = none.
	UPROPERTY(VisibleAnywhere, Category = "Face|HeadGear")
	TObjectPtr<UStaticMeshComponent> HeadGearBadgeComponent;
	void SetHeadGearBadge(UMaterialInterface* Material, const FVector& Location, const FRotator& Rotation, const FVector2D& Size);

	UFUNCTION(BlueprintCallable, Category = "Face|FacialHair")
	void SetFacialHairIndex(int32 Index);

	UFUNCTION(BlueprintCallable, Category = "Face|HeadGear")
	void SetHeadGearIndex(int32 Index);

	UFUNCTION(BlueprintCallable, Category = "Face|Material")
	void SetMaterialIndex(int32 Index);

	// Nose skin match. With the character's own material the nose mesh's
	// UVs always land on one fixed skin cell of the atlas -- every nose came
	// out the same light skin whatever the face was. Instead the nose gets a
	// flat-color material (M_FlatColor) set to the face's skin: the color
	// the caller already knows (modular hero: its Color_Skin parameter), or
	// else sampled from the character's base-color texture at the mesh
	// vertices around the nose attachment point (editor builds).
	UFUNCTION(BlueprintCallable, Category = "Face|Nose")
	void MatchNoseToSkin(USkeletalMeshComponent* SkeletalMesh, bool bHasKnownColor, FLinearColor KnownColor);
	UFUNCTION(BlueprintPure, Category = "Face|Nose")
	FLinearColor GetNoseColor() const { return NoseColor; }

	int32 GetHairIndex() const { return CurrentHairIndex; }
	int32 GetFacialHairIndex() const { return CurrentFacialHairIndex; }
	int32 GetHeadGearIndex() const { return CurrentHeadGearIndex; }
	int32 GetMaterialIndex() const { return CurrentMaterialIndex; }

	// Live nose placement -- writes the UPROPERTY and pushes it to
	// NoseComponent immediately (AttachToCharacter only applies it once).
	UFUNCTION(BlueprintCallable, Category = "Face|Nose")
	void SetNoseRelativeLocation(const FVector& NewLocation);

	UFUNCTION(BlueprintCallable, Category = "Face|Nose")
	void SetNoseRelativeRotation(const FRotator& NewRotation);

	// Re-applies Material to the nose/hair/facial-hair/head-gear meshes so
	// they stay matched to the character's -- called from AttachToCharacter
	// (initial sync), SetMaterialIndex, and ABaseCharacter::
	// SetCharacterMaterial (when the palette is changed from the character
	// side rather than through this controller's own MaterialOptions).
	void ApplyMaterialToAttachments(UMaterialInterface* Material);

	// A wide, tall, fully unlit/additive glowing column enclosing the whole
	// character -- meant to read as "this one is focused/being inspected"
	// from far away, where a thin selection outline would just disappear.
	// Off by default; toggled via SetHighlightVisible (e.g. a UI button).
	// The material's per-pixel behavior (fade-to-nothing near the base,
	// the downward wave/scroll, masking out the cylinder's end caps so
	// only the wall renders) is baked into M_HighlightBeam and applies as
	// -- HighlightMesh is expected to be /Engine/BasicShapes/Cylinder (or
	// another mesh sharing its local bounds: radius 50, half-height 50)
	// since HighlightHeightMultiplier/HighlightRadiusMultiplier below and
	// the material's own FadeDistance parameter are tuned against those
	// specific local dimensions.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face|Highlight")
	TObjectPtr<UStaticMesh> HighlightMesh;

	// Must expose a "BeamColor" vector parameter (e.g. M_HighlightBeam) --
	// a dynamic material instance is created from this at AttachToCharacter
	// time so HighlightColor below can tint it per-instance without needing
	// a separate Material asset per color.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face|Highlight")
	TObjectPtr<UMaterialInterface> HighlightMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face|Highlight")
	FLinearColor HighlightColor = FLinearColor(0.32f, 1.0f, 0.45f, 1.0f);   // CRT green

	UFUNCTION(BlueprintCallable, Category = "Face|Highlight")
	void SetHighlightColor(FLinearColor NewColor);

	// Auto-sizing, rather than a fixed per-instance transform, so the same
	// HighlightMesh/HighlightMaterial pairing works on any character
	// AttachToCharacter is called on. Size is derived at attach time from
	// SkeletalMesh's own CURRENT-POSE local bounds (not the skeletal mesh
	// asset's reference/bind-pose bounds, which for a T-pose rig would
	// measure the arm span, not the standing silhouette) -- see
	// AttachToCharacter. These multipliers scale that measured
	// half-height/radius; 2.25/1.0 reproduce the hand-tuned look this was
	// built and tested against on the Knight rig.
	//
	// Assumes the attach bone/component root sits at the character's feet
	// (true for this rig -- the column's bottom is placed at the root) and
	// that the mesh's local Z axis is up; a very differently-conventioned
	// skeleton would need those assumptions revisited in AttachToCharacter.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face|Highlight")
	float HighlightHeightMultiplier = 2.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face|Highlight")
	float HighlightRadiusMultiplier = 1.0f;

	UPROPERTY(VisibleAnywhere, Category = "Face|Highlight")
	TObjectPtr<UStaticMeshComponent> HighlightComponent;

	UFUNCTION(BlueprintCallable, Category = "Face|Highlight")
	void SetHighlightVisible(bool bVisible);

	UFUNCTION(BlueprintPure, Category = "Face|Highlight")
	bool IsHighlightVisible() const;

protected:

	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:

	FName CurrentState = NAME_None;

	// Cached at AttachToCharacter time; re-resolved fresh in BeginPlay since
	// Play mode creates its own AnimInstance separate from whatever existed
	// when AttachToCharacter was called from an editor-context script --
	// holding onto the editor-time instance would silently go stale the
	// moment real gameplay starts.
	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> TargetSkeletalMesh;

	UPROPERTY()
	TObjectPtr<UCharacterAnimInstance> FaceAnimInstance;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> MouthMID;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> HighlightMID;

	FTimerHandle BlinkScheduleTimer;
	FTimerHandle BlinkReopenTimer;

	EEyeOpenness CurrentEyeOpenness = EEyeOpenness::Normal;
	EBrowHeight CurrentBrowHeight = EBrowHeight::Normal;
	EBrowAngle CurrentBrowAngle = EBrowAngle::Normal;

	int32 CurrentHairIndex = -1;
	int32 CurrentFacialHairIndex = -1;
	int32 CurrentMaterialIndex = 0;
	int32 CurrentHeadGearIndex = -1;

	FQuat HeadFrameConversion = FQuat::Identity;

	// See MatchNoseToSkin.
	bool SampleSkinColorAroundNose(USkeletalMeshComponent* SkeletalMesh, FLinearColor& OutColor) const;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> NoseColorMID;
	FLinearColor NoseColor = FLinearColor::White;
	bool bNoseColorSampled = false;               // re-sample when the character's material changes
	FName AttachedBoneName = NAME_None;           // from AttachToCharacter
	TWeakObjectPtr<USkeletalMeshComponent> AttachedMesh;
	bool bMouthDecalEnabled = true;
	void ApplyNosePlacement();
	void ApplyMouthPlacement();

	float GetRestEyeScale() const;
	float GetBrowHeightCm() const;
	float GetBrowAngleDeg() const;

	// Creates MouthMID from BaseMouthMaterial if it doesn't exist yet and
	// assigns it to the decal. Called from both BeginPlay (so it exists
	// during actual gameplay) and OnConstruction (so the decal also shows
	// correctly in the plain editor viewport, not just in Play), and again
	// from SetMouthTexture when returning from the "None" mouth state,
	// since that state releases MouthMID entirely.
	void EnsureMouthMID();

	// Hides the mouth decal and releases its dynamic material instance --
	// used for the "None" mouth state, which exists specifically to free up
	// what a real mouth state costs (decal draw + MID) when no mouth is
	// wanted at all. Also re-enables decal reception on the head
	// attachments, since there's no mouth decal left for them to conflict
	// with.
	void SetMouthNone();

	// The actual texture-swap/None logic, factored out of SetMouthTexture so
	// the auto-talk cycle (TickTalking) can drive the mouth every tick
	// without each tick re-triggering SetMouthTexture's StopTalking() call
	// and cancelling itself.
	void ApplyMouthTextureInternal(FName StateName);

	bool bAutoTalking = false;
	FTimerHandle TalkingTimer;
	void TickTalking();
	void StopTalking();

	// Decal reception on the head attachments needs to track whether the
	// mouth decal is currently active: allowed when the mouth is "None"
	// (nothing to conflict with), blocked otherwise (so the mouth decal
	// doesn't paint onto a beard/hat sitting in its projection box).
	void UpdateAttachmentDecalReceiving();

	// Shared by the Cycle*()/Set*Index() functions: stores NewIndex into
	// Index (anything out of range becomes -1 = "None") and applies the
	// resulting mesh (or no mesh) to Comp.
	void ApplyHeadAttachment(UStaticMeshComponent* Comp, const TArray<TObjectPtr<UStaticMesh>>& Options, int32& Index, int32 NewIndex);

	// Keeps the nose/hair/facial-hair/head-gear attachments' material

	void ScheduleNextBlink();
	void StartBlink();
	void EndBlink();
};
