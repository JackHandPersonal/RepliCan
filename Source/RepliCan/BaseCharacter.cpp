#include "BaseCharacter.h"
#include "Vitality.h"
#include "ShotReactions.h"
#include "DeathThrash.h"
#include "GameFramework/PlayerStart.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "ImpactEffects.h"
#include "Components/PointLightComponent.h"
#include "AmbientPlayer.h"
#include "RepliCanUserSettings.h"
#include "Components/SpotLightComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/SpectatorPawn.h"
#include "JsonObjectConverter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "CharacterAnimInstance.h"
#include "FaceController.h"
#include "Engine/World.h"
#include "Animation/AnimSequence.h"
#include "CharacterBuilderWidget.h"
#include "Blueprint/UserWidget.h"
#include "CharacterConfig.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Animation/Skeleton.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "BasePlayerController.h"
#include "WeaponCatalog.h"
#include "DrawDebugHelpers.h"
#include "Engine/SkeletalMeshSocket.h"
#include "EngineUtils.h"
#include "Engine/StaticMeshActor.h"
#include "Kismet/GameplayStatics.h"
#include "Components/WidgetComponent.h"
#include "CharacterNameWidget.h"
#include "InputMappingContext.h"
#include "InputAction.h"

// Over-the-shoulder framing, tunable in play: RepliCan.ShoulderOffset 85 (cm right of the
// centreline in third person); -1 uses the character's ShoulderOffsetY.
static TAutoConsoleVariable<float> CVarShoulderOffset(TEXT("RepliCan.ShoulderOffset"), -1.0f, TEXT("Third-person camera shoulder offset in cm; -1 = the character default"), ECVF_Default);

ABaseCharacter::ABaseCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	// The post-camera placement of the weapon (see FWeaponPostCameraTick).
	PostCameraTickFunction.TickGroup = TG_PostUpdateWork;
	PostCameraTickFunction.bCanEverTick = true;
	PostCameraTickFunction.bStartWithTickEnabled = true;

	// Classic third-person/action-adventure feel: the character turns to
	// face its movement direction, not the camera, which is always free to
	// orbit independently (CameraBoom->bUsePawnControlRotation below) --
	// aiming/looking is handled separately by the head look-at system
	// (SetLookAtTarget, implemented on UCharacterAnimInstance), not by
	// turning the whole body to match the camera.
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, TurnRateDegPerSec, 0.0f);
	GetCharacterMovement()->MaxAcceleration = MovementAcceleration;

	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;
	// Without this, the capsule shrinks around its fixed center when
	// crouching instead of keeping the feet planted, so the character's
	// feet visibly lift off the ground ("floats") on crouch. (Confirmed via
	// the Python property name this compiles to -- bCrouchMovesCharacterDown
	// is the deprecated pre-5.x name for the same flag.)
	GetCharacterMovement()->bCrouchMaintainsBaseLocation = true;

	GetCharacterMovement()->MaxWalkSpeed = JogSpeed;
	GetCharacterMovement()->AirControl = AirControlAmount;
	GetCharacterMovement()->JumpZVelocity = JumpVelocity;

	// The single native AnimInstance handling locomotion/crouch/look-at/face
	// for this (and any other) character -- see CharacterAnimInstance.h.
	// Assigned once, here, and never swapped: there's no more "AnimBP vs.
	// crossfade" mode to switch between, since this one class does
	// everything a graph-based AnimBP would have, in code.
	GetMesh()->SetAnimInstanceClass(UCharacterAnimInstance::StaticClass());

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	// Z offset raises the boom's pivot from the capsule's center (default)
	// up to roughly eye height, so that the closest zoom step (index 0,
	// ~0 length) actually sits where the character's eyes would be
	// instead of at chest height. Kept as its own EyeHeightOffset property
	// (rather than a bare literal here) so Tick()'s crouch compensation has
	// a single source of truth for "what Z counts as not-crouched" instead
	// of a second hardcoded 75 that could drift out of sync with this one.
	CameraBoom->SetRelativeLocation(FVector(0.0f, 0.0f, EyeHeightOffset));
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bDoCollisionTest = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->SetFieldOfView(90.0f);
	FollowCamera->bUsePawnControlRotation = false;

	// SetupAttachment with a socket name works here (unlike FaceController's
	// nose, which needs a runtime AttachToComponent dance) because this
	// component is part of THIS actor's own hierarchy from construction --
	// no separate-actor SnapToTarget step needed. WeaponMesh itself (and
	// the relative transform) is applied in BeginPlay instead of here,
	// since WeaponMesh is meant to be set per-instance via Python after
	// construction, same as FaceController's NoseMesh.
	WeaponMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMeshComponent"));
	WeaponMeshComponent->SetupAttachment(GetMesh(), TEXT("hand_r"));
	WeaponMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Parented to the weapon rather than the hand: the flash belongs at the muzzle, and the
	// muzzle is a point on the weapon mesh that differs for every weapon in the catalogue.
	MuzzleFlash = CreateDefaultSubobject<UPointLightComponent>(TEXT("MuzzleFlash"));
	MuzzleFlash->SetupAttachment(WeaponMeshComponent);
	MuzzleFlash->SetIntensityUnits(ELightUnits::Candelas);
	MuzzleFlash->SetIntensity(900.0f);
	MuzzleFlash->SetAttenuationRadius(650.0f);
	MuzzleFlash->SetLightColor(FLinearColor(1.0f, 0.86f, 0.42f));   // a cartridge's flash: yellow-white, not orange
	MuzzleFlash->SetCastShadows(false);
	MuzzleFlash->SetVisibility(false);

	// Name tag: a screen-space widget parented to the mesh so it rides the
	// character (and scales up with a scaled one). The mesh origin is at the
	// soles, so +200 sits just above a standing head. Hidden until edit mode.
	NameLabel = CreateDefaultSubobject<UWidgetComponent>(TEXT("NameLabel"));

	// A headlamp on the brow, off until L. Head-bone frame: x up, y forward, z lateral, so the
	// lamp sits a little up and forward of the bone and the beam (a spot's +X) turns to the bone's +Y.
	Headlamp = CreateDefaultSubobject<USpotLightComponent>(TEXT("Headlamp"));
	Headlamp->SetupAttachment(GetMesh(), TEXT("head"));
	Headlamp->SetRelativeLocation(FVector(12.0f, 17.0f, 0.0f));   // on the brow surface, clear of the skull so the head cannot shadow the beam
	Headlamp->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
	Headlamp->SetIntensityUnits(ELightUnits::Candelas);
	Headlamp->SetIntensity(400.0f);
	Headlamp->SetAttenuationRadius(2000.0f);
	Headlamp->SetInnerConeAngle(9.0f);
	Headlamp->SetOuterConeAngle(21.0f);   // a tight circle with a softer rim
	Headlamp->SetLightColor(FLinearColor(1.0f, 0.95f, 0.85f));
	Headlamp->SetCastShadows(true);
	Headlamp->SetVisibility(false);
	NameLabel->SetupAttachment(GetMesh());
	NameLabel->SetRelativeLocation(FVector(0.0f, 0.0f, 200.0f));
	NameLabel->SetWidgetSpace(EWidgetSpace::Screen);
	NameLabel->SetDrawAtDesiredSize(true);
	NameLabel->SetPivot(FVector2D(0.5f, 1.0f));
	NameLabel->SetWidgetClass(UCharacterNameWidget::StaticClass());
	NameLabel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NameLabel->SetVisibility(false);

	// Mesh placement under the capsule, facing +X. -88 puts the mesh origin
	// (where every Synty rig keeps its soles in the reference pose) at the
	// capsule's bottom; the extra -2 is the ~2.15cm CharacterMovement
	// keeps a walking capsule above the floor (MIN/MAX_FLOOR_DIST). Per-
	// clip floor drift is handled by the anim instance's foot grounding,
	// not by tuning this -- see UCharacterAnimInstance::bGroundFeet.
	GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f));
	GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));

}

void ABaseCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Applied here rather than the constructor since WeaponMesh is meant to
	// be assigned per-instance (via Python or the CharacterBuilder panel,
	// same as FaceController's NoseMesh) after construction.
	ApplyWeapon();
	// (There used to be a tick prerequisite on the camera manager here, on the belief that the
	// camera is updated in its tick. It is not: UWorld::Tick updates every camera after ALL
	// actors have ticked, so nothing an actor's Tick reads from the camera manager is this
	// frame's. The weapon is now placed from a predicted eye in Tick and from the final camera
	// in PostCameraTick -- see FWeaponPostCameraTickFunction.)

	// The MESH ticks after this actor -- every character, not only the player's. The weapon is
	// placed and the hand targets computed in Tick; the anim proxy copies them in its own
	// update, and component-vs-actor order inside a tick group is not promised. Without this
	// the hands chase the weapon by a frame on whichever frames the mesh happened to go first.
	if (GetMesh()) { GetMesh()->AddTickPrerequisiteActor(this); }

	OnClicked.AddUniqueDynamic(this, &ABaseCharacter::HandleActorClicked);

	// Remember what the level gave us -- the Single-type defaults.
	OriginalBaseMesh = GetMesh()->GetSkeletalMeshAsset();
	OriginalMaterial = GetMesh()->GetMaterial(0);
	// A level-placed NPC carries its mesh from the editor, so the config
	// applied below sees "same mesh" and never runs the skeleton-compat /
	// grip-bone hookup that SetBaseMesh would; run it once here for
	// whatever rig this instance starts with.
	OnBaseMeshChanged();
	OriginalWeaponMesh = WeaponMesh;
	OriginalWeaponLocation = WeaponRelativeLocation;
	OriginalWeaponRotation = WeaponRelativeRotation;
	if (const UCharacterAnimInstance* AnimInst = GetCharacterAnimInstance()) { DefaultArmPose = AnimInst->ArmOverridePose; }

	// Until a config is applied, CurrentConfig describes the level's own
	// character as a Single-type config.
	CurrentConfig = SyntyCharacters::MakeDefaultConfig(TEXT("Level"),
		OriginalBaseMesh ? OriginalBaseMesh->GetPathName() : FString(),
		OriginalMaterial ? OriginalMaterial->GetPathName() : FString());
	SyncConfigFromLive();

	if (!DefaultCharacterConfigName.IsEmpty())
	{
		FCharacterConfig Config;
		if (!CharacterConfigFile::Load(DefaultCharacterConfigName, Config))
		{
			Config = ModularHero::MakeDefaultConfig(DefaultCharacterConfigName);
			// Start the default hero with whatever this instance was already
			// carrying in the level, rather than empty-handed.
			Config.WeaponMesh = CurrentConfig.WeaponMesh;
			Config.WeaponLocation = CurrentConfig.WeaponLocation;
			Config.WeaponRotation = CurrentConfig.WeaponRotation;
			CharacterConfigFile::Save(Config);
		}
		ApplyCharacterConfig(Config);
	}

	// Whatever the level/JSON gave us is the "saved" baseline (see
	// IsConfigDirty) -- taken on the first Tick, once the anim instance
	// exists, since SyncConfigFromLive reads the arm pose off it.
	bMarkSavedOnFirstTick = !DefaultCharacterConfigName.IsEmpty();

	// The name tag's widget only exists once the component has initialised;
	// give it the name now (ApplyCharacterConfig above may have run before).
	UpdateNameLabel();

	// Clamped here (not just at the constructor default) since
	// ZoomArmLengths/DefaultZoomLevelIndex are editable per-instance and
	// could be reconfigured (e.g. a shorter indoor array) to something
	// that no longer contains the constructor's default index.
	if (ZoomArmLengths.Num() > 0)
	{
		CurrentZoomLevelIndex = FMath::Clamp(DefaultZoomLevelIndex, 0, ZoomArmLengths.Num() - 1);
		CameraBoom->TargetArmLength = ZoomArmLengths[CurrentZoomLevelIndex];
	}

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				if (DefaultMappingContext)
				{
					Subsystem->AddMappingContext(DefaultMappingContext, 0);
				}
			}
		}
	}

}

void ABaseCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// The action assets are only assigned on Blueprint subclasses; the game mode spawns
	// this class directly, so resolve anything unset from /Game/Input by convention
	// (an unbound MoveForward is a player who cannot walk).
	auto Ensure = [](TObjectPtr<UInputAction>& Action, const TCHAR* Asset)
	{
		if (!Action) { Action = LoadObject<UInputAction>(nullptr, *FString::Printf(TEXT("/Game/Input/%s.%s"), Asset, Asset)); }
	};
	Ensure(MoveForwardAction, TEXT("IA_MoveForward")); Ensure(MoveRightAction, TEXT("IA_MoveRight"));
	Ensure(LookYawAction, TEXT("IA_LookYaw")); Ensure(LookPitchAction, TEXT("IA_LookPitch"));
	Ensure(ZoomAction, TEXT("IA_Zoom")); Ensure(JumpAction, TEXT("IA_Jump")); Ensure(CrouchAction, TEXT("IA_Crouch")); Ensure(SprintAction, TEXT("IA_Sprint"));
	Ensure(DashAction, TEXT("IA_Roll")); Ensure(CycleZoomAction, TEXT("IA_CycleZoom")); Ensure(ToggleIdleAction, TEXT("IA_ToggleIdle")); Ensure(CyclePoseAction, TEXT("IA_CyclePose"));
	Ensure(NextCombatAnimAction, TEXT("IA_NextCombatAnim")); Ensure(PrevCombatAnimAction, TEXT("IA_PrevCombatAnim")); Ensure(PlaySelectedAttackAction, TEXT("IA_PlaySelectedAttack"));
	Ensure(ToggleCharacterBuilderAction, TEXT("IA_ToggleCharacterBuilder")); Ensure(ToggleWalkAction, TEXT("IA_ToggleWalk")); Ensure(ToggleLocomotionSetAction, TEXT("IA_ToggleLocomotionSet"));
	if (AttackActions.Num() == 0)
	{
		for (int32 i = 1; i <= 9; ++i) { if (UInputAction* A = LoadObject<UInputAction>(nullptr, *FString::Printf(TEXT("/Game/Input/IA_Attack%d.IA_Attack%d"), i, i))) { AttackActions.Add(A); } }
	}
	if (!DefaultMappingContext) { DefaultMappingContext = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Input/IMC_Dungeon.IMC_Dungeon")); }

	// Runs on every possession, not just the one at BeginPlay -- a
	// character the Character Manager spawned and later took over needs
	// the mapping context too (AddMappingContext is idempotent).
	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (const ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				if (DefaultMappingContext) { Subsystem->AddMappingContext(DefaultMappingContext, 0); }
			}
		}
	}

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (MoveForwardAction) { EIC->BindAction(MoveForwardAction, ETriggerEvent::Triggered, this, &ABaseCharacter::MoveForward); }
		if (MoveRightAction) { EIC->BindAction(MoveRightAction, ETriggerEvent::Triggered, this, &ABaseCharacter::MoveRight); }
		if (LookYawAction) { EIC->BindAction(LookYawAction, ETriggerEvent::Triggered, this, &ABaseCharacter::LookYaw); }
		if (LookPitchAction) { EIC->BindAction(LookPitchAction, ETriggerEvent::Triggered, this, &ABaseCharacter::LookPitch); }
		if (ZoomAction) { EIC->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &ABaseCharacter::Zoom); }
		if (JumpAction)
		{
			EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &ABaseCharacter::StartJump);
			EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &ABaseCharacter::StopJump);
		}
		if (CrouchAction)
		{
			EIC->BindAction(CrouchAction, ETriggerEvent::Started, this, &ABaseCharacter::StartCrouch);
			EIC->BindAction(CrouchAction, ETriggerEvent::Completed, this, &ABaseCharacter::StopCrouch);
		}
		if (SprintAction)
		{
			EIC->BindAction(SprintAction, ETriggerEvent::Started, this, &ABaseCharacter::StartSprint);
			EIC->BindAction(SprintAction, ETriggerEvent::Completed, this, &ABaseCharacter::StopSprint);
		}
		if (CycleZoomAction) { EIC->BindAction(CycleZoomAction, ETriggerEvent::Started, this, &ABaseCharacter::CycleZoom); }
		if (ToggleIdleAction) { EIC->BindAction(ToggleIdleAction, ETriggerEvent::Started, this, &ABaseCharacter::ToggleIdleVariant); }
		if (ToggleLocomotionSetAction) { EIC->BindAction(ToggleLocomotionSetAction, ETriggerEvent::Started, this, &ABaseCharacter::ToggleLocomotionSet); }
		if (ToggleWalkAction) { EIC->BindAction(ToggleWalkAction, ETriggerEvent::Started, this, &ABaseCharacter::ToggleWalk); }
		if (CyclePoseAction) { EIC->BindAction(CyclePoseAction, ETriggerEvent::Started, this, &ABaseCharacter::CyclePose); }
		if (NextCombatAnimAction) { EIC->BindAction(NextCombatAnimAction, ETriggerEvent::Started, this, &ABaseCharacter::CyclePose); }
		if (PrevCombatAnimAction) { EIC->BindAction(PrevCombatAnimAction, ETriggerEvent::Started, this, &ABaseCharacter::PrevCombatAnim); }
		if (PlaySelectedAttackAction) { EIC->BindAction(PlaySelectedAttackAction, ETriggerEvent::Started, this, &ABaseCharacter::PlaySelectedAttack); }
		// TAB (ToggleCharacterBuilderAction) is deliberately NOT bound here:
		// ABasePlayerController binds the key itself so it works in fly mode
		// too, and binding both would toggle twice per press.

		// BindAction's templated overload forwards Index as an extra bound
		// argument to StartAttack -- avoids needing 9 near-identical
		// StartAttack1()..StartAttack9() wrapper functions just to know which
		// key was pressed.
		for (int32 Index = 0; Index < AttackActions.Num(); ++Index)
		{
			if (UInputAction* Action = AttackActions[Index])
			{
				EIC->BindAction(Action, ETriggerEvent::Started, this, &ABaseCharacter::StartAttack, Index);
			}
		}
	}
}

void ABaseCharacter::MoveForward(const FInputActionValue& Value)
{
	if (IsApproachingSeat()) { return; }   // the walk to the seat drives itself
	if (IsSitting()) { if (!FMath::IsNearlyZero(Value.Get<float>())) { StandUp(); } return; }
	// Captured BEFORE the roll/dash block below (and unconditionally, even
	// if it turns out to be 0) so StartDash always has an up-to-date read of
	// "what is the player currently holding" to pick a direction from --
	// see LastForwardAxis's header comment.
	LastForwardAxis = Value.Get<float>();
	++MoveForwardCalls;
	if (bFreelookLingering && !FMath::IsNearlyZero(LastForwardAxis)) { EndFreelookNow(); }

	// A roll/dash is meant to be a discrete, committed move, not something
	// WASD keeps blending with -- without this, holding a movement key
	// straight through one has CharacterMovementComponent accelerating the
	// capsule back toward normal walk/run velocity every single tick,
	// fighting the roll/dash's own directly-set Velocity (see StartRoll/
	// StartDash) the whole time. That tug-of-war is what read as the
	// capsule not quite following where the animation visually goes.
	if (bIsRolling || bIsDashing || bIsAttacking) { return; }

	if (Controller && LastForwardAxis != 0.0f)
	{
		const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
		AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), LastForwardAxis);
	}
}

void ABaseCharacter::MoveRight(const FInputActionValue& Value)
{
	if (IsApproachingSeat()) { return; }
	if (IsSitting()) { if (!FMath::IsNearlyZero(Value.Get<float>())) { StandUp(); } return; }
	LastRightAxis = Value.Get<float>();
	if (bFreelookLingering && !FMath::IsNearlyZero(LastRightAxis)) { EndFreelookNow(); }
	if (bGroggy) { LastRightAxis = 0.0f; return; }   // no strafing while groggy

	if (bIsRolling || bIsDashing || bIsAttacking) { return; }

	if (Controller && LastRightAxis != 0.0f)
	{
		const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
		AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), LastRightAxis);
	}
}

// The look scale of the moment: slower down the sights (AimSensitivityScale), full otherwise.
float ABaseCharacter::LookScale() const
{
	return MouseLookSensitivity * (CurrentCarry == EWeaponCarry::ADS ? AimSensitivityScale : 1.0f);
}

void ABaseCharacter::LookYaw(const FInputActionValue& Value)
{
	AddControllerYawInput(Value.Get<float>() * LookScale());
}

void ABaseCharacter::LookPitch(const FInputActionValue& Value)
{
	AddControllerPitchInput(Value.Get<float>() * LookScale());
}

void ABaseCharacter::Zoom(const FInputActionValue& Value)
{
	// The wheel no longer zooms (C does, see StepZoomLevel); the action stays bound so the
	// inspect menu can keep using the wheel for its selection.
}

void ABaseCharacter::StepZoomLevel()
{
	if (ZoomArmLengths.Num() == 0) { return; }
	CurrentZoomLevelIndex = (CurrentZoomLevelIndex + 1) % ZoomArmLengths.Num();
}

void ABaseCharacter::CycleZoom(const FInputActionValue& Value)
{
	if (ZoomArmLengths.Num() == 0) { return; }

	// Always steps toward third person then wraps back to index 0 (closest/
	// first-person) rather than clamping like the wheel does -- a single
	// dedicated key is naturally a "cycle through the list" gesture, not a
	// "push further in one direction" one the way continuous wheel deltas are.
	CurrentZoomLevelIndex = (CurrentZoomLevelIndex + 1) % ZoomArmLengths.Num();
}

void ABaseCharacter::StartJump(const FInputActionValue& Value)
{
	if (IsSitting()) { StandUp(); return; }   // Space leaves the seat
	if (IsApproachingSeat()) { ApproachSeat.Reset(); return; }
	if (bIsRolling || bIsDashing || bIsAttacking) { return; }

	if (GetCharacterMovement()->IsMovingOnGround())
	{
		// A normal jump also has to consume the coyote allowance, not just
		// the coyote branch below -- TimeLastGrounded was just set (this
		// same recent frame, before takeoff), so without this a mashed
		// second press arriving a moment later would ALSO pass the coyote
		// window check below and re-launch on top of the jump already in
		// progress, stacking height with each repeated press. This is what
		// was actually happening when "mashing jump" made him keep getting
		// higher -- not physics/animation double-counting.
		bCoyoteJumpUsed = true;
		Jump();
		return;
	}

	// Coyote time: still grounded-equivalent for jump purposes if we left
	// the ground within the last CoyoteTimeSeconds and haven't already
	// spent this fall's coyote jump (otherwise every subsequent frame while
	// still falling and within the window would also pass this check,
	// letting the player jump repeatedly instead of just the once).
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (!bCoyoteJumpUsed && (Now - TimeLastGrounded) <= CoyoteTimeSeconds)
	{
		bCoyoteJumpUsed = true;
		LaunchCharacter(FVector(0.0f, 0.0f, GetCharacterMovement()->JumpZVelocity), false, true);
		return;
	}

	// Neither grounded nor within coyote time -- buffer the press so
	// landing within JumpBufferSeconds still triggers a jump instead of
	// silently eating an early press (see Tick()).
	bJumpInputBuffered = true;
	TimeJumpBuffered = Now;
}

void ABaseCharacter::StopJump(const FInputActionValue& Value)
{
	StopJumping();
}

void ABaseCharacter::StartCrouch(const FInputActionValue& Value)
{
	if (bIsRolling || bIsDashing || bIsAttacking) { return; }

	// Crouch is how Roll is triggered now (ALT is Dash instead) -- a Crouch
	// press while moving fast enough reads as "roll" rather than "crouch
	// down", matching the request that pressing Crouch while running should
	// roll and land crouched (see StartRoll/Tick's roll-expiry, which is
	// what actually leaves the character crouched once the roll finishes).
	// Below this speed a Crouch press still means an ordinary crouch.
	// At sprint speed a Crouch press is the pack's sprint->crouch slide
	// (when the active set has one); at run speed it stays the roll.
	{
		const float SprintBoundary = 0.5f * (JogSpeed + RunSpeed) * CurrentConfig.SpeedMultiplier;
		const UCharacterAnimInstance* AnimInst = GetCharacterAnimInstance();
		if (!bIsSliding && AnimInst && AnimInst->GetActiveSyntySet().SprintToCrouchAnim && GetVelocity().Size2D() >= SprintBoundary
			&& AnimInst->GetCurrentLocomotionSet() == ECharacterLocomotionSet::Synty && !GetCharacterMovement()->IsFalling())
		{
			bCrouchReleaseConsumedByRoll = true;
			StartSprintSlide();
			return;
		}
	}
	if (GetVelocity().Size2D() >= MinSpeedToTriggerRollFromCrouch)
	{
		// See bCrouchReleaseConsumedByRoll -- the matching release must not
		// go through StopCrouch's tap/hold logic.
		bCrouchReleaseConsumedByRoll = true;
		StartRoll(Value);
		return;
	}

	// Crouches immediately on key-down either way -- whether this turns out
	// to be a tap or a hold is only decided on release (see StopCrouch),
	// since there's no way to know in advance, and hold-to-crouch needs the
	// crouch to begin the instant the key goes down regardless.
	TimeCrouchPressed = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	Crouch();
}

void ABaseCharacter::StartSprintSlide()
{
	UCharacterAnimInstance* AnimInst = GetCharacterAnimInstance();
	const UAnimSequence* Clip = AnimInst ? AnimInst->GetActiveSyntySet().SprintToCrouchAnim.Get() : nullptr;
	if (!Clip) { return; }
	bIsSliding = true;
	TimeSlideStarted = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	SlideDurationSeconds = Clip->GetPlayLength();
	// The clip's root motion carries the character (see Tick); the movement
	// component just keeps the capsule on the floor meanwhile.
	GetCharacterMovement()->Velocity = FVector::ZeroVector;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	AnimInst->TriggerSprintSlide();
}

void ABaseCharacter::TurnInPlace(float DeltaYawDegrees)
{
	if (bIsRolling || bIsDashing || bIsAttacking || bIsSliding || GetCharacterMovement()->IsFalling()) { return; }
	if (GetVelocity().Size2D() > 5.0f || FMath::Abs(DeltaYawDegrees) < 30.0f) { return; }
	if (UCharacterAnimInstance* AnimInst = GetCharacterAnimInstance()) { AnimInst->TriggerTurnInPlace(DeltaYawDegrees); }
}

void ABaseCharacter::StopCrouch(const FInputActionValue& Value)
{
	if (bCrouchReleaseConsumedByRoll)
	{
		bCrouchReleaseConsumedByRoll = false;
		return;
	}

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if ((Now - TimeCrouchPressed) < CrouchTapThresholdSeconds)
	{
		// A tap: flip the persistent toggle. Turning it ON just means
		// staying crouched (already crouched since key-down above); turning
		// it OFF means this same tap is what ends the crouch.
		bIsCrouchToggled = !bIsCrouchToggled;
		if (!bIsCrouchToggled)
		{
			UnCrouch();
		}
	}
	else
	{
		// A hold always ends in standing on release, regardless of toggle
		// state -- e.g. holding through and past a previous toggle-crouch
		// still stands back up when let go, rather than leaving the
		// toggle's own persistence in charge of the outcome.
		bIsCrouchToggled = false;
		UnCrouch();
	}
}

void ABaseCharacter::StartSprint(const FInputActionValue& Value)
{
	if (bGroggy) { return; }   // Shift has no effect on the groggy walk
	// No longer force-uncrouches -- Shift while crouched now means "fast
	// crouch-walk" instead of "stand up and run" (see UpdateCrouchWalkSpeed).
	// Setting MaxWalkSpeed here is harmless even while crouched: crouched
	// movement is governed by MaxWalkSpeedCrouched instead, so this value
	// just sits ready for whenever the player next stands up.
	bSprintHeld = true;
	UpdateStandingSpeed();
	UpdateCrouchWalkSpeed();
}

void ABaseCharacter::StopSprint(const FInputActionValue& Value)
{
	bSprintHeld = false;
	UpdateStandingSpeed();
	UpdateCrouchWalkSpeed();
}

void ABaseCharacter::UpdateCrouchWalkSpeed()
{
	GetCharacterMovement()->MaxWalkSpeedCrouched = (bSprintHeld ? CrouchWalkFastSpeed : CrouchWalkSlowSpeed) * CurrentConfig.SpeedMultiplier;
}

void ABaseCharacter::UpdateStandingSpeed()
{
	if (bGroggy) { GetCharacterMovement()->MaxWalkSpeed = GroggySpeed; return; }   // Shift and X do nothing while groggy
	// An injured leg cannot run and gives up a quarter of whatever pace is left.
	GetCharacterMovement()->MaxWalkSpeed = ((bSprintHeld && !bLegInjured) ? RunSpeed : (bWalkToggled ? WalkSpeed : JogSpeed)) * CurrentConfig.SpeedMultiplier * (bLegInjured ? 0.75f : 1.0f);
}

FString ABaseCharacter::DescribeMoveGuards() const
{
	const UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent);
	return FString::Printf(TEXT("moveFwdCalls=%d lastFwd=%.2f rolling=%d dashing=%d attacking=%d groggy=%d bindings=%d pending=%s accel=%.0f maxWalk=%.0f mode=%d inputComp=%s controller=%s"),
		MoveForwardCalls, LastForwardAxis, bIsRolling ? 1 : 0, bIsDashing ? 1 : 0, bIsAttacking ? 1 : 0, bGroggy ? 1 : 0,
		EIC ? EIC->GetActionEventBindings().Num() : -1, *GetPendingMovementInputVector().ToString(),
		GetCharacterMovement()->GetMaxAcceleration(), GetCharacterMovement()->MaxWalkSpeed, (int32)GetCharacterMovement()->MovementMode,
		InputComponent ? *InputComponent->GetClass()->GetName() : TEXT("none"), GetController() ? *GetController()->GetName() : TEXT("none"));
}

void ABaseCharacter::SetGroggy(bool bOn, float Speed)
{
	bGroggy = bOn;
	if (bOn) { GroggySpeed = Speed > 0.0f ? Speed : 95.0f; GroggyTime = 0.0f; CurrentZoomLevelIndex = 0; }   // first person
	else if (FollowCamera)
	{
		FollowCamera->SetRelativeRotation(FRotator::ZeroRotator);
		FVector L = FollowCamera->GetRelativeLocation(); L.Z = 0.0f; FollowCamera->SetRelativeLocation(L);
	}
	UpdateStandingSpeed();
}

void ABaseCharacter::ToggleWalk(const FInputActionValue& Value)
{
	bWalkToggled = !bWalkToggled;
	UpdateStandingSpeed();
}

void ABaseCharacter::StartRoll(const FInputActionValue& Value)
{
	if (bIsRolling || bIsDashing || bIsAttacking) { return; }

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if ((Now - TimeLastRoll) < RollCooldownSeconds) { return; }

	UCharacterAnimInstance* AnimInst = Cast<UCharacterAnimInstance>(GetMesh()->GetAnimInstance());
	if (!AnimInst) { return; }

	// RollAnim only rolls forward relative to the character's OWN facing
	// (the free sample this project has only bundles that one direction --
	// see RollAnim's header comment on UCharacterAnimInstance), so there's
	// no input-relative direction to compute here at all; TriggerRoll just
	// plays it. Only ever called from StartCrouch now (a Crouch press while
	// running -- see MinSpeedToTriggerRollFromCrouch), not bound to its own
	// key anymore.
	UE_LOG(LogTemp, Warning, TEXT("[RollStart] StartLoc=%s ActorRot=%s"),
		*GetActorLocation().ToString(), *GetActorRotation().ToString());

	AnimInst->TriggerRoll();

	bIsRolling = true;
	TimeLastRoll = Now;
	RollElapsedAtLastConsume = 0.0f;

	// See RootMotionMoveMeshRotation's header comment -- locks the
	// direction root motion gets converted with, and stops
	// CharacterMovementComponent from continuing to turn the actor toward
	// whatever velocity is left over from just before the roll, both of
	// which fed a curved path instead of a straight one.
	RootMotionMoveMeshRotation = GetMesh()->GetComponentQuat();
	GetCharacterMovement()->Velocity = FVector::ZeroVector;
	GetCharacterMovement()->bOrientRotationToMovement = false;

	UpdateRollMovement();

	// A toggled crouch would otherwise still be trying to hold the capsule
	// crouched through the roll -- standing up first avoids the roll and a
	// crouch fighting over capsule height/speed at the same time.
	if (bIsCrouchToggled)
	{
		bIsCrouchToggled = false;
		UnCrouch();
	}
}

void ABaseCharacter::UpdateRollMovement()
{
	UCharacterAnimInstance* AnimInst = Cast<UCharacterAnimInstance>(GetMesh()->GetAnimInstance());
	if (!AnimInst || !AnimInst->RollAnim) { return; }

	// Extracted directly against the ASSET (a pure, stateless query -- it
	// doesn't touch the AnimInstance's own runtime playback state at all),
	// using THIS Character's own elapsed-time bookkeeping rather than the
	// mesh's AnimInstance's internal ToTime -- see RollElapsedAtLastConsume's
	// header comment for why reading that instead was a real bug (its
	// update order relative to this Tick() isn't guaranteed).
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const float CurrentElapsed = FMath::Min(Now - TimeLastRoll, AnimInst->RollAnim->GetPlayLength());
	const float PreviousElapsed = RollElapsedAtLastConsume;
	RollElapsedAtLastConsume = CurrentElapsed;

	if (FMath::IsNearlyEqual(PreviousElapsed, CurrentElapsed)) { return; }

	FDeltaTimeRecord DeltaTimeRecord;
	DeltaTimeRecord.Set(PreviousElapsed, CurrentElapsed - PreviousElapsed);
	const FAnimExtractContext ExtractContext(static_cast<double>(CurrentElapsed), /*bExtractRootMotion=*/true, DeltaTimeRecord, /*bLooping=*/false);
	const FTransform RootMotionDelta = AnimInst->RollAnim->ExtractRootMotionFromRange(PreviousElapsed, CurrentElapsed, ExtractContext);

	// RollAnim's root motion is authored in the MESH COMPONENT's own local
	// space, not the actor's -- this project's SkeletalMeshComponent carries
	// a fixed -90 degree yaw relative to the actor (the standard correction
	// every Blender-round-tripped rig here needs, see other components'
	// notes on the same offset). Uses the ROTATION CACHED AT ROLL START
	// (RootMotionMoveMeshRotation), not a fresh live read of the mesh's
	// current transform -- see that field's header comment for why reading
	// live here was a real bug.
	// See RollRootMotionScale's header comment -- RollAnim's real authored
	// distance (~4.6m) is faithfully extracted above, then scaled down here
	// to fit this game's sense of scale while keeping the clip's own
	// fast-in/ease-out timing intact.
	const FVector WorldDelta = RootMotionMoveMeshRotation.RotateVector(RootMotionDelta.GetTranslation()) * RollRootMotionScale;
	AddActorWorldOffset(WorldDelta, /*bSweep=*/true);
}

void ABaseCharacter::StartDash(const FInputActionValue& Value)
{
	if (bIsRolling || bIsDashing || bIsAttacking) { return; }

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if ((Now - TimeLastDash) < DashCooldownSeconds) { return; }

	UCharacterAnimInstance* AnimInst = Cast<UCharacterAnimInstance>(GetMesh()->GetAnimInstance());
	if (!AnimInst) { return; }

	// Picks a direction from whatever WASD is CURRENTLY held (LastForwardAxis/
	// LastRightAxis, updated every MoveForward/MoveRight call regardless of
	// whether that input was allowed through) -- forward/backward wins a
	// tie so a diagonal press still reads as one clean direction rather
	// than an arbitrary axis preference, and no input held at all defaults
	// to dashing forward (the character's own facing).
	ECharacterDashDirection Direction = ECharacterDashDirection::Forward;
	if (FMath::Abs(LastForwardAxis) >= FMath::Abs(LastRightAxis))
	{
		Direction = (LastForwardAxis < 0.0f) ? ECharacterDashDirection::Backward : ECharacterDashDirection::Forward;
	}
	else
	{
		Direction = (LastRightAxis < 0.0f) ? ECharacterDashDirection::Left : ECharacterDashDirection::Right;
	}

	UE_LOG(LogTemp, Warning, TEXT("[DashStart] Direction=%d StartLoc=%s ActorRot=%s"),
		static_cast<int32>(Direction), *GetActorLocation().ToString(), *GetActorRotation().ToString());

	AnimInst->TriggerDash(Direction);

	CurrentDashDirection = Direction;
	bIsDashing = true;
	TimeLastDash = Now;
	DashElapsedAtLastConsume = 0.0f;

	// See RootMotionMoveMeshRotation's header comment.
	RootMotionMoveMeshRotation = GetMesh()->GetComponentQuat();
	GetCharacterMovement()->Velocity = FVector::ZeroVector;
	GetCharacterMovement()->bOrientRotationToMovement = false;

	UpdateDashMovement();

	if (bIsCrouchToggled)
	{
		bIsCrouchToggled = false;
		UnCrouch();
	}
}

void ABaseCharacter::UpdateDashMovement()
{
	UCharacterAnimInstance* AnimInst = Cast<UCharacterAnimInstance>(GetMesh()->GetAnimInstance());
	if (!AnimInst) { return; }

	UAnimSequence* DashAnim = AnimInst->GetDashAnim(CurrentDashDirection);
	if (!DashAnim) { return; }

	// Identical approach to UpdateRollMovement -- see its header comment for
	// why elapsed time is tracked here (this Character's own Tick()-driven
	// clock) rather than reading the AnimInstance's own internal playback
	// state, and why root motion is extracted directly against the asset.
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const float CurrentElapsed = FMath::Min(Now - TimeLastDash, DashAnim->GetPlayLength());
	const float PreviousElapsed = DashElapsedAtLastConsume;
	DashElapsedAtLastConsume = CurrentElapsed;

	if (FMath::IsNearlyEqual(PreviousElapsed, CurrentElapsed)) { return; }

	FDeltaTimeRecord DeltaTimeRecord;
	DeltaTimeRecord.Set(PreviousElapsed, CurrentElapsed - PreviousElapsed);
	const FAnimExtractContext ExtractContext(static_cast<double>(CurrentElapsed), /*bExtractRootMotion=*/true, DeltaTimeRecord, /*bLooping=*/false);
	const FTransform RootMotionDelta = DashAnim->ExtractRootMotionFromRange(PreviousElapsed, CurrentElapsed, ExtractContext);

	// See RootMotionMoveMeshRotation's header comment -- uses the rotation
	// cached at dash start, not a live read, for the same reason
	// UpdateRollMovement does.
	const FVector WorldDelta = RootMotionMoveMeshRotation.RotateVector(RootMotionDelta.GetTranslation()) * DashRootMotionScale;
	AddActorWorldOffset(WorldDelta, /*bSweep=*/true);
}

void ABaseCharacter::ToggleIdleVariant(const FInputActionValue& Value)
{
	if (UCharacterAnimInstance* AnimInst = Cast<UCharacterAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		AnimInst->CycleIdleVariant();
	}
}

void ABaseCharacter::ToggleLocomotionSet(const FInputActionValue& Value)
{
	if (UCharacterAnimInstance* AnimInst = Cast<UCharacterAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		AnimInst->ToggleLocomotionSet();
	}
}

void ABaseCharacter::CyclePose(const FInputActionValue& Value)
{
	// Purely a selection change (see SelectedCombatAnimIndex) -- not gated
	// on bIsRolling/bIsDashing/bIsAttacking the way actually PLAYING an
	// attack is (see PlaySelectedAttack/StartAttack), since browsing which
	// one is selected shouldn't be blocked by whatever's currently playing.
	const UCharacterAnimInstance* AnimInst = Cast<UCharacterAnimInstance>(GetMesh()->GetAnimInstance());
	const int32 NumClips = AnimInst ? AnimInst->GetNumAllCombatAnims() : 0;
	if (NumClips <= 0) { return; }

	SelectedCombatAnimIndex = (SelectedCombatAnimIndex + 1) % NumClips;
}

void ABaseCharacter::PrevCombatAnim(const FInputActionValue& Value)
{
	const UCharacterAnimInstance* AnimInst = Cast<UCharacterAnimInstance>(GetMesh()->GetAnimInstance());
	const int32 NumClips = AnimInst ? AnimInst->GetNumAllCombatAnims() : 0;
	if (NumClips <= 0) { return; }

	// +NumClips before the modulo so this never goes negative -- C++'s %
	// on a negative left-hand side returns a negative result, not the
	// positive wrap a "previous" step needs.
	SelectedCombatAnimIndex = (SelectedCombatAnimIndex - 1 + NumClips) % NumClips;
}

void ABaseCharacter::PlaySelectedAttack(const FInputActionValue& Value)
{
	// In edit mode, LMB belongs to the controller (select / place). This
	// binding runs BEFORE the controller's own binding of the same action and
	// consumes it, so hand the click over explicitly.
	ABasePlayerController* PC = Cast<ABasePlayerController>(GetController());
	if (PC && PC->IsEditMode()) { PC->NotifyEditClick(); return; }
	if (bIsRolling || bIsDashing || bIsAttacking) { return; }

	UCharacterAnimInstance* AnimInst = Cast<UCharacterAnimInstance>(GetMesh()->GetAnimInstance());
	if (!AnimInst) { return; }

	PlayAttackAnim(AnimInst->GetAllCombatAnim(SelectedCombatAnimIndex));
}

void ABaseCharacter::HandleActorClicked(AActor* TouchedActor, FKey ButtonPressed)
{
	if (ButtonPressed != EKeys::LeftMouseButton) { return; }
	// Any controller in edit mode, not just the one possessing us -- NPCs are
	// clicked while the player controls someone else (or is flying).
	if (ABasePlayerController* PC = Cast<ABasePlayerController>(GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr))
	{
		UE_LOG(LogTemp, Log, TEXT("EditMode: actor click on %s (edit=%d)"), *GetName(), PC->IsEditMode());
		PC->NotifyEditClick();
	}
}

void ABaseCharacter::StartAttack(const FInputActionValue& Value, int32 Index)
{
	if (bIsRolling || bIsDashing || bIsAttacking) { return; }
	// 1-9 pick conversation replies while one is open.
	if (const ABasePlayerController* PC = Cast<ABasePlayerController>(GetController())) { if (PC->IsInConversation() || PC->IsInCinematic()) { return; } }

	UCharacterAnimInstance* AnimInst = Cast<UCharacterAnimInstance>(GetMesh()->GetAnimInstance());
	if (!AnimInst) { return; }

	PlayAttackAnim(AnimInst->GetAttackAnim(Index));
}

void ABaseCharacter::PlayAttackAnim(UAnimSequence* Anim)
{
	// The number-key / '[' ']' preview: one clip, played through the same
	// sequence player the combat capabilities use.
	if (!Anim) { return; }
	TArray<FCombatStep> Steps;
	FCombatStep Step; Step.Clip = Anim; Step.bHold = false;
	Steps.Add(Step);
	PlayCombatSequence(Steps, /*bInterrupt=*/false);
}

// ---- Combat capabilities ----------------------------------------------------

void ABaseCharacter::EnsureCombatLibrary()
{
	if (CombatLibrary.IsBuilt()) { return; }
	const UCharacterAnimInstance* AnimInst = GetCharacterAnimInstance();
	if (!AnimInst) { return; }
	TArray<UAnimSequence*> Clips;
	for (int32 i = 0; i < AnimInst->GetNumAllCombatAnims(); ++i) { Clips.Add(AnimInst->GetAllCombatAnim(i)); }
	CombatLibrary.Build(Clips);
}

UAnimSequence* ABaseCharacter::CombatClip(const FString& Key) const
{
	UAnimSequence* Clip = CombatLibrary.Find(Key);
	if (!Clip) { UE_LOG(LogTemp, Warning, TEXT("Combat: the pack has no clip '%s'"), *Key); }
	return Clip;
}

FString ABaseCharacter::GenderToken() const
{
	return CurrentConfig.Gender == TEXT("Female") ? TEXT("Femn") : TEXT("Masc");
}

bool ABaseCharacter::PlayCombatSequence(const TArray<FCombatStep>& Steps, bool bInterrupt, TFunction<void()> OnComplete)
{
	if (Steps.Num() == 0) { return false; }
	for (const FCombatStep& Step : Steps) { if (!Step.Clip) { return false; } }
	if (bIsRolling || bIsDashing) { return false; }
	if (bIsAttacking && !bInterrupt) { return false; }

	if (bCombatHolding)
	{
		if (UCharacterAnimInstance* AnimInst = GetCharacterAnimInstance()) { AnimInst->ReleaseAttackHold(); }
		bCombatHolding = false;
	}
	CombatQueue = Steps;
	CombatOnComplete = MoveTemp(OnComplete);
	bBlocking = false;
	StartCombatStep(0);
	return true;
}

void ABaseCharacter::StartCombatStep(int32 Index)
{
	UCharacterAnimInstance* AnimInst = GetCharacterAnimInstance();
	if (!AnimInst || !CombatQueue.IsValidIndex(Index)) { AdvanceCombat(); return; }
	const FCombatStep& Step = CombatQueue[Index];
	CombatStepIndex = Index;
	AnimInst->TriggerAttack(Step.Clip, Step.bHold);
	bIsAttacking = true;
	bCombatHolding = Step.bHold;
	TimeLastAttack = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	AttackDurationSeconds = Step.bHold ? TNumericLimits<float>::Max() : Step.Clip->GetPlayLength();
}

void ABaseCharacter::AdvanceCombat()
{
	const int32 Next = CombatStepIndex + 1;
	if (CombatQueue.IsValidIndex(Next))
	{
		StartCombatStep(Next);
		return;
	}
	CombatQueue.Reset();
	CombatStepIndex = -1;
	bCombatHolding = false;
	bIsAttacking = false;
	if (CombatOnComplete)
	{
		TFunction<void()> Done = MoveTemp(CombatOnComplete);
		CombatOnComplete = nullptr;
		Done();
	}
}

void ABaseCharacter::ReleaseCombatHold()
{
	if (!bCombatHolding) { return; }
	if (UCharacterAnimInstance* AnimInst = GetCharacterAnimInstance()) { AnimInst->ReleaseAttackHold(); }
	bCombatHolding = false;
	AdvanceCombat();
}

namespace
{
	ABaseCharacter::FCombatStep MakeStep(UAnimSequence* Clip, bool bHold)
	{
		ABaseCharacter::FCombatStep Step;
		Step.Clip = Clip;
		Step.bHold = bHold;
		return Step;
	}
}

bool ABaseCharacter::CombatAttack(const FString& AttackName)
{
	EnsureCombatLibrary();
	TArray<FCombatStep> Steps;
	Steps.Add(MakeStep(CombatClip(TEXT("Attack_") + AttackName), false));
	Steps.Add(MakeStep(CombatClip(TEXT("Attack_") + AttackName + TEXT("_ReturnToIdle")), false));
	return PlayCombatSequence(Steps, false);
}

bool ABaseCharacter::CombatCombo(const FString& ComboName, int32 NumSteps)
{
	EnsureCombatLibrary();
	static const TCHAR Letters[] = { TEXT('A'), TEXT('B'), TEXT('C'), TEXT('D') };
	TArray<FCombatStep> Steps;
	FString LastKey;
	for (int32 i = 0; i < FMath::Clamp(NumSteps, 1, 4); ++i)
	{
		const FString Key = TEXT("Attack_") + ComboName + FString::Chr(Letters[i]);
		UAnimSequence* Clip = CombatLibrary.Find(Key);
		if (!Clip) { break; }
		Steps.Add(MakeStep(Clip, false));
		LastKey = Key;
	}
	if (Steps.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Combat: no combo '%s' in the pack"), *ComboName);
		return false;
	}
	// The last step played gets its own recovery back to the ready idle.
	Steps.Add(MakeStep(CombatClip(LastKey + TEXT("_ReturnToIdle")), false));
	return PlayCombatSequence(Steps, false);
}

bool ABaseCharacter::CombatBlockBegin()
{
	EnsureCombatLibrary();
	if (bBlocking) { return true; }
	TArray<FCombatStep> Steps;
	Steps.Add(MakeStep(CombatClip(TEXT("Block_Begin")), false));
	Steps.Add(MakeStep(CombatClip(TEXT("Block_Loop")), true));
	if (!PlayCombatSequence(Steps, false)) { return false; }
	bBlocking = true;
	return true;
}

void ABaseCharacter::CombatBlockEnd()
{
	if (!bBlocking) { return; }
	bBlocking = false;
	if (!bCombatHolding) { return; }   // mid-parry etc.: the sequence will finish on its own
	CombatQueue.Reset();
	CombatQueue.Add(MakeStep(CombatClip(TEXT("Block_End")), false));
	CombatStepIndex = -1;
	ReleaseCombatHold();
}

bool ABaseCharacter::CombatParry(const FString& ParryDir)
{
	EnsureCombatLibrary();
	TArray<FCombatStep> Steps;
	Steps.Add(MakeStep(CombatClip(TEXT("Parry_") + ParryDir), false));
	if (ParryDir == TEXT("F")) { Steps.Add(MakeStep(CombatClip(TEXT("Parry_F_ReturnToBlock")), false)); }
	const bool bWasBlocking = bBlocking;
	if (bWasBlocking) { Steps.Add(MakeStep(CombatClip(TEXT("Block_Loop")), true)); }
	// A parry is allowed straight out of the block hold.
	if (!PlayCombatSequence(Steps, bWasBlocking)) { return false; }
	bBlocking = bWasBlocking;
	return true;
}

bool ABaseCharacter::CombatParryCounter(const FString& CounterKind)
{
	EnsureCombatLibrary();
	TArray<FCombatStep> Steps;
	Steps.Add(MakeStep(CombatClip(TEXT("Parry_F_") + CounterKind), false));
	const bool bWasBlocking = bBlocking;
	if (bWasBlocking) { Steps.Add(MakeStep(CombatClip(TEXT("Block_Loop")), true)); }
	if (!PlayCombatSequence(Steps, bWasBlocking)) { return false; }
	bBlocking = bWasBlocking;
	return true;
}

bool ABaseCharacter::CombatParryBreak()
{
	EnsureCombatLibrary();
	TArray<FCombatStep> Steps;
	Steps.Add(MakeStep(CombatClip(TEXT("Parry_Break")), false));
	return PlayCombatSequence(Steps, true);   // a broken guard interrupts the block
}

bool ABaseCharacter::CombatDodge(const FString& Dir, bool bRoll)
{
	EnsureCombatLibrary();
	TArray<FCombatStep> Steps;
	Steps.Add(MakeStep(CombatClip((bRoll ? TEXT("DodgeRoll_") : TEXT("Dodge_")) + Dir), false));
	return PlayCombatSequence(Steps, false);
}

bool ABaseCharacter::CombatHit(const FString& Dir, bool bStagger)
{
	EnsureCombatLibrary();
	TArray<FCombatStep> Steps;
	Steps.Add(MakeStep(CombatClip(TEXT("Hit_") + Dir + (bStagger ? TEXT("_Stagger") : TEXT("_React"))), false));
	return PlayCombatSequence(Steps, true);
}

bool ABaseCharacter::CombatKnockDown()
{
	EnsureCombatLibrary();
	TArray<FCombatStep> Steps;
	Steps.Add(MakeStep(CombatClip(TEXT("KnockDown_Begin")), false));
	Steps.Add(MakeStep(CombatClip(TEXT("KnockDown_Loop")), true));
	return PlayCombatSequence(Steps, true);
}

void ABaseCharacter::CombatGetUp()
{
	if (!bCombatHolding) { return; }
	CombatQueue.Reset();
	CombatQueue.Add(MakeStep(CombatClip(TEXT("KnockDown_End")), false));
	CombatStepIndex = -1;
	ReleaseCombatHold();
}

bool ABaseCharacter::CombatStun()
{
	EnsureCombatLibrary();
	TArray<FCombatStep> Steps;
	Steps.Add(MakeStep(CombatClip(TEXT("Stun_Begin")), false));
	Steps.Add(MakeStep(CombatClip(TEXT("Stun_Loop")), true));
	return PlayCombatSequence(Steps, true);
}

void ABaseCharacter::CombatRecover()
{
	if (!bCombatHolding) { return; }
	CombatQueue.Reset();
	CombatQueue.Add(MakeStep(CombatClip(TEXT("Stun_End")), false));
	CombatStepIndex = -1;
	ReleaseCombatHold();
}

bool ABaseCharacter::CombatDie(const FString& Dir)
{
	EnsureCombatLibrary();
	TArray<FCombatStep> Steps;
	Steps.Add(MakeStep(CombatClip(TEXT("Death_") + Dir + TEXT("_01")), false));
	Steps.Add(MakeStep(CombatClip(TEXT("Death_") + Dir + TEXT("_01_Pose")), true));   // held; nothing releases it
	return PlayCombatSequence(Steps, true);
}

bool ABaseCharacter::CombatTauntBegin()
{
	EnsureCombatLibrary();
	TArray<FCombatStep> Steps;
	Steps.Add(MakeStep(CombatClip(TEXT("Idle_Menacing01_Begin")), false));
	Steps.Add(MakeStep(CombatClip(TEXT("Idle_Menacing01")), true));
	return PlayCombatSequence(Steps, false);
}

void ABaseCharacter::CombatTauntEnd()
{
	if (!bCombatHolding) { return; }
	CombatQueue.Reset();
	CombatQueue.Add(MakeStep(CombatClip(TEXT("Idle_Menacing01_End")), false));
	CombatStepIndex = -1;
	ReleaseCombatHold();
}

bool ABaseCharacter::CombatFidget(int32 Index)
{
	EnsureCombatLibrary();
	static const TCHAR* Fidgets[] = { TEXT("Idle_EnergeticStance01"), TEXT("Idle_Flourish01") };
	TArray<FCombatStep> Steps;
	Steps.Add(MakeStep(CombatClip(Fidgets[FMath::Abs(Index) % 2]), false));
	return PlayCombatSequence(Steps, false);
}

bool ABaseCharacter::CombatDrawSword()
{
	EnsureCombatLibrary();
	TArray<FCombatStep> Steps;
	Steps.Add(MakeStep(CombatClip(TEXT("Draw_Sword_") + GenderToken()), false));
	UAnimSequence* ReadyIdle = CombatLibrary.Find(TEXT("Idle_Base"));
	TWeakObjectPtr<ABaseCharacter> Self(this);
	return PlayCombatSequence(Steps, false, [Self, ReadyIdle]()
	{
		if (!Self.IsValid()) { return; }
		if (Self->WeaponMeshComponent) { Self->WeaponMeshComponent->SetVisibility(Self->WeaponMesh != nullptr); }
		if (UCharacterAnimInstance* AnimInst = Self->GetCharacterAnimInstance()) { if (ReadyIdle) { AnimInst->ArmOverridePose = ReadyIdle; } }
	});
}

bool ABaseCharacter::CombatSheatheSword()
{
	EnsureCombatLibrary();
	TArray<FCombatStep> Steps;
	Steps.Add(MakeStep(CombatClip(TEXT("Sheathe_Sword_") + GenderToken()), false));
	UAnimSequence* SheathedIdle = CombatLibrary.Find(TEXT("Idle_Base_Sheathed"));
	TWeakObjectPtr<ABaseCharacter> Self(this);
	return PlayCombatSequence(Steps, false, [Self, SheathedIdle]()
	{
		if (!Self.IsValid()) { return; }
		if (Self->WeaponMeshComponent) { Self->WeaponMeshComponent->SetVisibility(false); }
		if (UCharacterAnimInstance* AnimInst = Self->GetCharacterAnimInstance()) { if (SheathedIdle) { AnimInst->ArmOverridePose = SheathedIdle; } }
	});
}

bool ABaseCharacter::CombatReadyStance(bool bReady)
{
	EnsureCombatLibrary();
	TArray<FCombatStep> Steps;
	Steps.Add(MakeStep(CombatClip(bReady ? (TEXT("Idle_") + GenderToken() + TEXT("_ToBase")) : (TEXT("Idle_Base_ToIdle_") + GenderToken())), false));
	return PlayCombatSequence(Steps, false);
}

FString ABaseCharacter::GetCurrentCombatClip() const
{
	return CombatQueue.IsValidIndex(CombatStepIndex) && CombatQueue[CombatStepIndex].Clip ? CombatQueue[CombatStepIndex].Clip->GetName() : FString();
}

TArray<FString> ABaseCharacter::GetCombatCapabilities() const
{
	return {
		TEXT("CombatAttack(AttackName)      e.g. LightCombo01A, HeavyStab01 -> attack then its ReturnToIdle"),
		TEXT("CombatCombo(ComboName, N)     LightCombo01 / HeavyCombo01: steps A..N then that step's ReturnToIdle"),
		TEXT("CombatBlockBegin / CombatBlockEnd   Block_Begin -> Block_Loop (held) -> Block_End"),
		TEXT("CombatParry(F|L|R)            parry (F also ReturnToBlock), back to the block hold if blocking"),
		TEXT("CombatParryCounter(CounterShove|PommelStrike)   a counter out of a front parry"),
		TEXT("CombatParryBreak              guard broken (interrupts the block)"),
		TEXT("CombatDodge(F|B|L|R, bRoll)   Dodge_ / DodgeRoll_"),
		TEXT("CombatHit(F|B|L|R, bStagger)  Hit_*_React (light) / Hit_*_Stagger (heavy); interrupts"),
		TEXT("CombatKnockDown / CombatGetUp   KnockDown_Begin -> Loop (held) -> End"),
		TEXT("CombatStun / CombatRecover      Stun_Begin -> Loop (held) -> End"),
		TEXT("CombatDie(F|B|L|R)            Death_*_01 then the held Death_*_01_Pose"),
		TEXT("CombatTauntBegin / CombatTauntEnd   Idle_Menacing01_Begin -> loop (held) -> End"),
		TEXT("CombatFidget(0|1)             Idle_EnergeticStance01 / Idle_Flourish01"),
		TEXT("CombatDrawSword / CombatSheatheSword   gendered Draw/Sheathe; weapon shown/hidden, ready/sheathed arm pose"),
		TEXT("CombatReadyStance(bReady)     relaxed <-> ready transitions (gendered)"),
	};
}

FString ABaseCharacter::DescribeCombatAnims()
{
	EnsureCombatLibrary();
	return CombatLibrary.Describe();
}

TArray<FString> ABaseCharacter::GetCombatClipKeys()
{
	EnsureCombatLibrary();
	return CombatLibrary.Keys();
}

void ABaseCharacter::ToggleCharacterBuilder(const FInputActionValue& Value)
{
	// The panel, selection and placement live on the controller now (they
	// have to survive switching pawns) -- see ABasePlayerController.
	if (ABasePlayerController* PC = Cast<ABasePlayerController>(GetController()))
	{
		PC->ToggleEditMode();
	}
}

void ABaseCharacter::ApplyWeapon()
{
	if (!WeaponMeshComponent) { return; }
	// A null WeaponMesh clears the component -- "no weapon" is a valid pick.
	if (!OpticMeshComponent)
	{
		OpticMeshComponent = NewObject<UStaticMeshComponent>(this, TEXT("OpticMesh"));
		OpticMeshComponent->SetupAttachment(WeaponMeshComponent);
		OpticMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		OpticMeshComponent->RegisterComponent();
	}
	WeaponMeshComponent->SetStaticMesh(WeaponMesh);
	WeaponMeshComponent->SetVisibility(WeaponMesh != nullptr);
	// Where a weapon sits in a hand is an ASSET question, not a code one: the
	// mesh is baked into grip space and the skeleton carries a WeaponGrip_R
	// socket that cancels out its own hand frame, so the correct transform
	// here is identity. See Docs/HeldAssetStandard.md.
	//
	// The old path -- WeaponRelative* authored in the Mannequin hand_r frame
	// and rotated per rig at runtime -- is kept only for a mesh that has not
	// been through Tools/normalise_weapons.py yet, and for hand placement of
	// props in the character builder. A skeleton with no socket says so in the
	// log rather than silently looking wrong.
	if (GetMesh() && GetMesh()->DoesSocketExist(WeaponGripSocket))
	{
		if (WeaponMeshComponent->GetAttachSocketName() != WeaponGripSocket)
		{
			WeaponMeshComponent->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale, WeaponGripSocket);
		}
		// IDENTITY. Not "identity plus the old offsets", which is what this used to do and why
		// the rifle hung a quarter of a metre from the fist even after the socket was measured
		// correctly: the socket put the weapon in the hand and then these three lines moved it
		// straight back out again. The whole point of baking the mesh and authoring the socket
		// is that there is nothing left to add here. See Docs/HeldAssetStandard.md 2.2.
		//
		// WeaponRelative* survive only for the fallback below and for hand-placed props in the
		// character builder, which are not HAC1 weapons and have no socket to hang off.
		// ... identity but for TriggerHandRotation, the measured correction to the socket's roll.
		FTransform OnSocket = WeaponOnSocket(); OnSocket.SetScale3D(WeaponRelativeScale);
		WeaponMeshComponent->SetRelativeTransform(OnSocket);
	}
	else
	{
		if (WeaponMesh) { UE_LOG(LogTemp, Warning, TEXT("ApplyWeapon: no %s socket on this skeleton, falling back to the runtime hand-frame conversion"), *WeaponGripSocket.ToString()); }
		const FQuat HandFrame = ComputeBoneFrameConversion(TEXT("hand_r"));
		WeaponMeshComponent->SetRelativeLocation(HandFrame.RotateVector(WeaponRelativeLocation));
		WeaponMeshComponent->SetRelativeRotation((HandFrame * WeaponRelativeRotation.Quaternion()).Rotator());
		WeaponMeshComponent->SetRelativeScale3D(WeaponRelativeScale);
	}

	// The finger-curl grip correction only makes sense with something in the
	// hand -- otherwise every unarmed character stands with its right hand
	// permanently clenched as if holding an invisible weapon. Gate it on the
	// weapon actually being present (the sword-idle arm layer is gated the
	// same way in ApplyCharacterConfig).
	if (UCharacterAnimInstance* AnimInst = GetCharacterAnimInstance())
	{
		AnimInst->WeaponGripWeight = (WeaponMesh != nullptr) ? 1.0f : 0.0f;
	}
}

// ---- Weapon stance ---------------------------------------------------------
// A stance names a folder; every clip in it is reached by composing the name.
// Nothing in here knows what a rifle is.

void ABaseCharacter::SetWeaponStance(const FString& Stance)
{
	if (WeaponStance == Stance) { return; }
	WeaponStance = Stance;
	WeaponActionLeft = 0.0f;
	SpreadBloomDegrees = 0.0f;
	// Armed and unarmed face differently, so this is one of the two things that changes it.
	ApplyFacingMode();
	if (WeaponStance.IsEmpty() || !PlayWeaponAction(TEXT("Equip")))
	{
		RefreshWeaponStancePose();
	}
}

void ABaseCharacter::SetAiming(bool bNewAiming)
{
	// Aiming something you are not holding is just a slower walk.
	const bool bWanted = bNewAiming && !WeaponStance.IsEmpty() && !bWeaponMelee;   // no sights on a blade
	if (bAiming == bWanted) { return; }
	// Raising the sights ends free look. The two ask for opposite things from the camera, and
	// between them the sights are the one the player pressed a button for.
	if (bWanted && bFreelook) { EndFreelookNow(); }
	// Leaving the sights does not drop the weapon: it comes down to the shoulder and stays there
	// for the hold, then to low ready.
	if (!bWanted && bAiming) { ForceShoulderLeft = FMath::Max(ForceShoulderLeft, ShoulderedHoldSeconds); }
	bAiming = bWanted;
	// The arm clip is NOT swapped here. Doing so put the arms in the sights pose the instant the
	// button went down while the weapon was still a fifth of a second from arriving; TickCarry
	// swaps it at the midpoint of the move instead, so the two agree.
}

bool ABaseCharacter::PlayWeaponAction(const FString& Clip)
{
	UCharacterAnimInstance* AnimInst = GetCharacterAnimInstance();
	if (!AnimInst || WeaponStance.IsEmpty()) { return false; }
	UAnimSequence* Seq = WeaponCatalog::StanceClip(WeaponStance, *Clip, ResolveLocomotionChoice() == ESyntyLocomotionChoice::Female);
	if (!Seq) { return false; }
	AnimInst->SetArmOverride(Seq, WeaponCatalog::StanceRoots(WeaponStance), 1.0f, false);
	WeaponActionLeft = Seq->GetPlayLength();
	if (Clip == TEXT("Reload")) { ReloadTotal = ReloadLeft = WeaponActionLeft; }   // the handling curve runs the clip's length
	return true;
}

bool ABaseCharacter::PlayMeleeClip(UAnimSequence* Clip)
{
	UCharacterAnimInstance* AnimInst = GetCharacterAnimInstance();
	if (!AnimInst || !Clip) { return false; }
	// The whole body, as authored, with the play-time layers out of the way for the swing's length.
	AnimInst->SetArmOverride(Clip, { FName(TEXT("root")) }, 1.0f, false);
	AnimInst->bFullBodyAction = true;
	WeaponActionLeft = Clip->GetPlayLength();
	return true;
}

bool ABaseCharacter::PlayDeathClip(UAnimSequence* Clip)
{
	UCharacterAnimInstance* AnimInst = GetCharacterAnimInstance();
	if (!AnimInst || !Clip) { return false; }
	AnimInst->SetArmOverride(Clip, { FName(TEXT("root")) }, 1.0f, false);
	WeaponActionLeft = Clip->GetPlayLength();
	return true;
}

bool ABaseCharacter::PlayHitReaction(UAnimSequence* Clip)
{
	UCharacterAnimInstance* AnimInst = GetCharacterAnimInstance();
	if (!AnimInst || !Clip) { return false; }
	AnimInst->SetArmOverride(Clip, { FName(TEXT("spine_01")) }, 1.0f, false);
	WeaponActionLeft = Clip->GetPlayLength();   // the same clock that hands the arms back to the stance
	return true;
}

void ABaseCharacter::RefreshWeaponStancePose()
{
	UCharacterAnimInstance* AnimInst = GetCharacterAnimInstance();
	if (!AnimInst) { return; }
	AnimInst->bFullBodyAction = false;   // whatever action was running is over
	if (WeaponStance.IsEmpty())
	{
		// Unarmed: drop the layer entirely rather than leaving the arms frozen
		// around a weapon that is no longer there.
		AnimInst->SetArmOverride(nullptr, TArray<FName>(), 0.0f, true);
		return;
	}
	const bool bFeminine = ResolveLocomotionChoice() == ESyntyLocomotionChoice::Female;
	const bool bADSPose = PosedCarry() == EWeaponCarry::ADS;
	UAnimSequence* Idle = WeaponCatalog::StanceClip(WeaponStance, bADSPose ? TEXT("Idle_ADS") : TEXT("Idle_Hipfire"), bFeminine);
	if (!Idle) { Idle = WeaponCatalog::StanceClip(WeaponStance, TEXT("Idle_Hipfire"), bFeminine); }
	if (Idle) { AnimInst->SetArmOverride(Idle, WeaponCatalog::StanceRoots(WeaponStance), 1.0f, true); }
	else { AnimInst->SetArmOverride(nullptr, TArray<FName>(), 0.0f, true); }   // a stance with no idle of its own hands the arms back to locomotion, rather than leaving a finished swing frozen
	bPosedForADS = bADSPose;
}

void ABaseCharacter::TickWeaponStance(float DeltaSeconds)
{
	// Bloom always closes, whether or not anything is held -- putting a weapon
	// away should not leave the next one inheriting the last one spray.
	SpreadBloomDegrees = FMath::Max(0.0f, SpreadBloomDegrees - SpreadRecoverDegreesPerSecond * DeltaSeconds);
	ForceShoulderLeft = FMath::Max(0.0f, ForceShoulderLeft - DeltaSeconds);
	TickCarry(DeltaSeconds);
	if (ReloadLeft > 0.0f) { ReloadLeft = FMath::Max(0.0f, ReloadLeft - DeltaSeconds); }
	if (WeaponActionLeft > 0.0f)
	{
		WeaponActionLeft -= DeltaSeconds;
		if (WeaponActionLeft <= 0.0f)
		{
			WeaponActionLeft = 0.0f;
			RefreshWeaponStancePose();
		}
	}
}

void ABaseCharacter::ApplyFacingMode()
{
	// Free look is the one case where facing the aim and following the controller come apart:
	// he still faces his aim, but his aim is no longer the controller's rotation.
	const bool bFace = ShouldFaceAim();
	GetCharacterMovement()->bOrientRotationToMovement = !bFace && !bFreelook;
	bUseControllerRotationYaw = bFace && !bFreelook;
}

void ABaseCharacter::SetFreelook(bool bOn)
{
	// Aiming down sights and looking somewhere else are not compatible, so the sights win.
	const bool bWanted = bOn && !bAiming;
	if (bWanted && bFreelook)
	{
		// Pressed again while the last look was still lingering: carry on from where the camera
		// is, without re-freezing the aim, and stop movement from ending it while held.
		bFreelookLingering = false;
		return;
	}
	if (bFreelook == bWanted) { return; }
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (bWanted)
	{
		// Remember where he was pointed. This is the aim for as long as the key is held.
		FrozenAim = PC ? PC->GetControlRotation() : GetActorRotation();
		bFreelook = true;
		bFreelookLingering = false;
		ApplyFacingMode();
	}
	else
	{
		// Key released: the look STAYS. The aim is still frozen and he still faces it; the camera
		// comes back the moment the player moves or aims -- see EndFreelookNow.
		bFreelookLingering = true;
	}
}

void ABaseCharacter::EndFreelookNow()
{
	if (!bFreelook) { return; }
	bFreelook = false;
	bFreelookLingering = false;
	// Snapping is correct here, not easing: the player moved BECAUSE they mean to go where they
	// were aimed, and a slow return just delays them.
	if (APlayerController* PC = Cast<APlayerController>(GetController())) { PC->SetControlRotation(FrozenAim); }
	ApplyFacingMode();
}

FRotator ABaseCharacter::GetAimRotation() const
{
	// The steady aim plus this frame's sway: the point of aim wanders, and the shot, the reticle
	// and the weapon in the hands all follow it.
	return AimRotationSteady() + AimSway;
}

FRotator ABaseCharacter::AimRotationSteady() const
{
	if (bFreelook) { return FrozenAim; }
	if (bInFirstPerson && bPredictedEyeValid) { return PredictedEyeRot; }   // this frame's, not the cache's
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC && PC->PlayerCameraManager) { return PC->PlayerCameraManager->GetCameraRotation(); }
	return GetActorRotation();
}

FVector ABaseCharacter::GetAimOrigin() const
{
	if (!bFreelook)
	{
		if (bInFirstPerson && bPredictedEyeValid) { return PredictedEyeLoc; }   // this frame's, not the cache's
		const APlayerController* PC = Cast<APlayerController>(GetController());
		if (PC && PC->PlayerCameraManager) { return PC->PlayerCameraManager->GetCameraLocation(); }
	}
	// Free looking: the camera is off somewhere else, so the shot comes from his own eye line.
	// CameraBoom sits at eye height on the capsule, which is exactly that point.
	return CameraBoom ? CameraBoom->GetComponentLocation() : GetActorLocation();
}

bool ABaseCharacter::ShouldFaceAim() const
{
	// First person: the camera direction IS the player's facing, always.
	// Armed in third person: the weapon has to point where the reticle is, and it cannot do
	// that if the body is busy facing whichever way the legs are going.
	return bInFirstPerson || !WeaponStance.IsEmpty();
}

void ABaseCharacter::SetWeaponGrip(const FVector& GripLocal)
{
	WeaponGripLocal = GripLocal;
	if (WeaponMeshComponent && WeaponMeshComponent->GetAttachParent() && GetMesh() && GetMesh()->DoesSocketExist(WeaponGripSocket))
	{
		FTransform OnSocket = WeaponOnSocket(); OnSocket.SetScale3D(WeaponRelativeScale);
		WeaponMeshComponent->SetRelativeTransform(OnSocket);
	}
}

void ABaseCharacter::SetTriggerHandRotation(const FRotator& R)
{
	TriggerHandRotation = R;
	if (WeaponMeshComponent && WeaponMeshComponent->GetAttachParent() && GetMesh() && GetMesh()->DoesSocketExist(WeaponGripSocket))
	{
		FTransform OnSocket = WeaponOnSocket(); OnSocket.SetScale3D(WeaponRelativeScale);
		WeaponMeshComponent->SetRelativeTransform(OnSocket);
	}
}

void ABaseCharacter::SetWeaponSight(const FVector& SightLocal, bool bHasSight, float SightPitch)
{
	WeaponSightLocal = SightLocal;
	bWeaponHasSight = bHasSight;
	WeaponSightPitch = SightPitch;
}

float ABaseCharacter::RaiseToShoulder()
{
	// Every shot renews the hold, so a burst never drops the weapon between rounds.
	ForceShoulderLeft = FMath::Max(ForceShoulderLeft, ShoulderedHoldSeconds);
	// Already up and settled: nothing to wait for.
	if (CurrentCarry != EWeaponCarry::LowReady && CarryBlendLeft <= 0.0f) { return 0.0f; }
	if (CurrentCarry != EWeaponCarry::LowReady) { return CarryBlendLeft; }
	// The stance's own equip clip reads as bringing the weapon up, which is exactly the motion
	// wanted here; if the stance has no such clip the geometric raise carries it alone.
	PlayWeaponAction(TEXT("Equip"));
	return RaiseFromLowReadySeconds;
}

EWeaponCarry ABaseCharacter::GetDesiredCarry() const
{
	if (bAiming) { return EWeaponCarry::ADS; }
	// A commanded raise beats everything short of the sights: he was told to shoot. A hip-fire
	// weapon raises to the hip; that IS its firing posture.
	if (ForceShoulderLeft > 0.0f) { return bWeaponHipFire ? EWeaponCarry::HipFire : EWeaponCarry::Shouldered; }
	// Nobody runs with a rifle in their face.
	if (bLowReadyWhileSprinting && bSprintHeld) { return EWeaponCarry::LowReady; }
	// At rest, the weapon is DOWN. Shouldered is a state you are put into by firing or by
	// leaving the sights, and held for ShoulderedHoldSeconds; hip fire is the raised state of
	// the special weapons that carry that way. Everything decays to low ready.
	return EWeaponCarry::LowReady;
}

FVector ABaseCharacter::CarryOffset(EWeaponCarry Carry) const
{
	// A stance with its own hold (a pistol indexes off the trigger hand, arms out, not a stock in
	// the shoulder) overrides the rifle-tuned defaults below; the sights are the sights either way.
	// ADS included: a pistol's rear sight sits at arm's length, a rifle's at the cheek, and only the stance knows which.
	if (!WeaponStance.IsEmpty())
	{
		const TCHAR* Which = Carry == EWeaponCarry::ADS ? TEXT("ads") : Carry == EWeaponCarry::HipFire ? TEXT("hip") : Carry == EWeaponCarry::LowReady ? (bInFirstPerson ? TEXT("low_ready_first_person") : TEXT("low_ready")) : TEXT("shouldered");
		FVector V;
		if (WeaponCatalog::StanceCarry(WeaponStance, Which, V)) { return V; }
	}
	switch (Carry)
	{
	case EWeaponCarry::ADS:      return CarryADS;
	case EWeaponCarry::HipFire:  return CarryHipFire;
	case EWeaponCarry::LowReady: return bInFirstPerson ? CarryLowReadyFirstPerson : CarryLowReady;
	default:                     return CarryShouldered;
	}
}

float ABaseCharacter::CarryTransitionSeconds(EWeaponCarry From, EWeaponCarry To) const
{
	if (From == To) { return 0.0f; }
	if (From == EWeaponCarry::LowReady) { return RaiseFromLowReadySeconds; }
	if (To == EWeaponCarry::LowReady)   { return DropToLowReadySeconds; }
	if (To == EWeaponCarry::ADS)        { return IntoSightsSeconds; }
	if (From == EWeaponCarry::ADS)      { return OutOfSightsSeconds; }
	return CarryChangeSeconds;
}

void ABaseCharacter::TickCarry(float DeltaSeconds)
{
	if (CarryBlendLeft > 0.0f)
	{
		CarryBlendLeft = FMath::Max(0.0f, CarryBlendLeft - DeltaSeconds);
	}
	const EWeaponCarry Want = GetDesiredCarry();
	// Swap the arm clip when the move is half done -- not when it starts, not when it ends.
	// Only when nothing else owns the arms: a one-shot in flight puts the idle back itself.
	if (!IsWeaponBusy() && bPosedForADS != (PosedCarry() == EWeaponCarry::ADS))
	{
		RefreshWeaponStancePose();
	}
	if (Want == CurrentCarry) { return; }
	// Start the move. Interrupting one already in flight is allowed and starts a fresh move
	// from wherever the weapon actually is -- a player who changes their mind halfway should
	// not have to wait out the first decision.
	CarryFrom = CurrentCarry;
	CurrentCarry = Want;
	CarryBlendTotal = CarryTransitionSeconds(CarryFrom, CurrentCarry);
	CarryBlendLeft = CarryBlendTotal;
}

float ABaseCharacter::SecondsUntilReadyToFire() const
{
	// Low ready is the one posture a shot cannot come from.
	if (CurrentCarry == EWeaponCarry::LowReady) { return RaiseFromLowReadySeconds; }
	return CarryBlendLeft;
}

void ABaseCharacter::SetWeaponForeGrip(const FVector& ForeGripLocal, bool bHasForeGrip, float ForeGripPitch)
{
	WeaponForeGripLocal = ForeGripLocal;
	bWeaponHasForeGrip = bHasForeGrip;
	WeaponForeGripPitch = ForeGripPitch;
}

void ABaseCharacter::SetWeaponMuzzle(const FVector& MuzzleLocal)
{
	WeaponMuzzleLocal = MuzzleLocal;
}

void ABaseCharacter::SetWeaponHipFire(bool bHipFire)
{
	bWeaponHipFire = bHipFire;
}

void ABaseCharacter::SetWeaponMelee(bool bMelee)
{
	bWeaponMelee = bMelee;
	if (bMelee) { SetAiming(false); }
}

void ABaseCharacter::TickHandIK(float DeltaSeconds)
{
	UCharacterAnimInstance* Anim = GetCharacterAnimInstance();
	if (!Anim) { return; }

	const bool bArmed = WeaponMesh && WeaponMeshComponent && !WeaponStance.IsEmpty();
	// The support hand only has somewhere to be on a two-handed weapon. A pistol's off hand is
	// free, and dragging it onto the barrel would look far worse than leaving it alone.
	const bool bWantSupport = (bArmed && bWeaponHasForeGrip && WeaponCatalog::StanceIsTwoHanded(WeaponStance)) || CarriedByHand.IsValid();   // or the off hand is holding something
	// The trigger hand only needs solving when the weapon is NOT hanging off it -- which is
	// exactly when the geometric solve has taken the weapon off the socket. The rest of the
	// time the socket already puts the hand and the weapon in the same place by construction,
	// and running IK would be asking the arm to reach somewhere it already is.
	const bool bWantTrigger = bArmed && bSightAlignActive;

	SupportIKBlend.Set(bWantSupport ? FMath::Clamp(SupportHandIKWeight, 0.0f, 1.0f) : 0.0f, HandIKBlendSeconds);
	TriggerIKBlend.Set(bWantTrigger ? FMath::Clamp(TriggerHandIKWeight, 0.0f, 1.0f) : 0.0f, HandIKBlendSeconds);
	SupportIKBlend.Tick(DeltaSeconds);
	TriggerIKBlend.Tick(DeltaSeconds);
	SupportIKAlpha = SupportIKBlend.Value;
	TriggerIKAlpha = TriggerIKBlend.Value;

	// --- #8: the torso leans into the aim pitch. Handed to the proxy here because it already
	// has the anim instance in hand; the lean itself is applied in ApplySpineLean.
	LeanBlend.Set(bArmed ? 1.0f : 0.0f, SpineLeanBlendSeconds);
	LeanBlend.Tick(DeltaSeconds);
	Anim->SpineLeanWeight = LeanBlend.Value;
	Anim->SpineLeanFraction = SpineLeanFraction;
	Anim->AimPitchDegrees = FRotator::NormalizeAxis(GetAimRotation().Pitch);
	Anim->bLookAtLocked = bArmed && PosedCarry() != EWeaponCarry::LowReady;   // weapon up: the head IS the aim
	Anim->AimForwardWorld = GetActorForwardVector();
	Anim->HandIKWeightR = TriggerIKAlpha;
	Anim->HandIKWeightL = SupportIKAlpha;
	Anim->ElbowDownBiasR = Anim->ElbowDownBiasL = WeaponCatalog::StanceElbowDown(WeaponStance);   // a pistol's elbows down and out
	if (SupportIKAlpha <= KINDA_SMALL_NUMBER && TriggerIKAlpha <= KINDA_SMALL_NUMBER) { return; }

	const FTransform WeaponWorld = WeaponMeshComponent->GetComponentTransform();

	// A socket's transform relative to its bone is the whole relationship between hand and
	// weapon. Going through it in reverse -- weapon world, undo the socket -- gives the place
	// the HAND has to be for the weapon to end up where it already is.
	auto HandFor = [this](FName SocketName, const FTransform& DesiredSocketWorld, FTransform& Out) -> bool
	{
		const USkeletalMesh* Asset = GetMesh() ? GetMesh()->GetSkeletalMeshAsset() : nullptr;
		const USkeletalMeshSocket* Sock = Asset ? Asset->FindSocket(SocketName) : nullptr;
		if (!Sock) { return false; }
		const FTransform SocketOnBone(Sock->RelativeRotation, Sock->RelativeLocation, Sock->RelativeScale);
		Out = SocketOnBone.Inverse() * DesiredSocketWorld;
		return true;
	};

	if (TriggerIKAlpha > KINDA_SMALL_NUMBER)
	{
		FTransform Target;
		// The socket the hand must present is the weapon's frame with the correction undone.
		if (HandFor(WeaponGripSocket, WeaponOnSocket().Inverse() * WeaponWorld, Target)) { Anim->HandIKTargetR = Target; }
		else { Anim->HandIKWeightR = 0.0f; }
	}
	if (SupportIKAlpha > KINDA_SMALL_NUMBER && CarriedByHand.IsValid())
	{
		// The carried thing: the palm on its near side, the hand turned toward it. A first guess
		// at the wrap; the reach is what matters.
		const FVector Obj = CarriedByHand->GetComponentLocation();
		const FVector From = GetMesh() && GetMesh()->DoesSocketExist(TEXT("clavicle_l")) ? GetMesh()->GetSocketLocation(TEXT("clavicle_l")) : GetActorLocation() + FVector(0, 0, 50.0f);
		const FVector To = (Obj - From).GetSafeNormal();
		const FVector Palm = Obj - To * (CarriedHandRadius * 0.9f);
		FTransform Target;
		if (HandFor(TEXT("WeaponGrip_L"), FTransform(FRotationMatrix::MakeFromXZ(To, FVector::UpVector).ToQuat(), Palm), Target)) { Anim->HandIKTargetL = Target; }
		else { Anim->HandIKWeightL = 0.0f; }
	}
	else if (SupportIKAlpha > KINDA_SMALL_NUMBER)
	{
		// The fore grip, in the world: the weapon's own point, turned so the palm wraps it.
		// The global wrap angle, pitched by this weapon's own handguard slope. Component-wise on
		// the rotator is right here: UE applies roll, then pitch, then yaw, so the pitch is taken
		// about the weapon's lateral axis after the palm has been rolled onto the guard.
		const FRotator Wrap(SupportHandRotation.Pitch + WeaponForeGripPitch, SupportHandRotation.Yaw, SupportHandRotation.Roll);
		const FTransform ForeGripWorld = FTransform(Wrap, WeaponForeGripLocal) * WeaponWorld;
		FTransform Target;
		if (HandFor(TEXT("WeaponGrip_L"), ForeGripWorld, Target)) { Anim->HandIKTargetL = Target; }
		else { Anim->HandIKWeightL = 0.0f; }
	}
}

void ABaseCharacter::TickGoodSpots(float DeltaSeconds)
{
	GoodSpotClock += DeltaSeconds;
	if (GoodSpotClock < 2.0f) { return; }
	GoodSpotClock = 0.0f;
	const UCharacterMovementComponent* Move = GetCharacterMovement();
	if (!Move || !Move->IsMovingOnGround()) { return; }
	const FVector Here = GetActorLocation();
	if (GoodSpots.Num() > 0 && FVector::Dist(GoodSpots.Last(), Here) < 60.0f) { return; }   // standing still: one entry is enough
	GoodSpots.Add(Here);
	if (GoodSpots.Num() > 24) { GoodSpots.RemoveAt(0); }
}

bool ABaseCharacter::TryUnstuck()
{
	UWorld* World = GetWorld();
	const UCapsuleComponent* Cap = GetCapsuleComponent();
	if (!World || !Cap) { return false; }
	const FVector Here = GetActorLocation();
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Cap->GetScaledCapsuleRadius() * 0.95f, Cap->GetScaledCapsuleHalfHeight() * 0.95f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(Unstuck), false, this);
	auto Free = [&](const FVector& At) { return !World->OverlapBlockingTestByChannel(At, FQuat::Identity, ECC_Pawn, Shape, Params); };
	auto GoTo = [&](const FVector& At)
	{
		if (UCharacterMovementComponent* Move = GetCharacterMovement()) { Move->StopMovementImmediately(); Move->Velocity = FVector::ZeroVector; }
		SetActorLocation(At + FVector(0, 0, 2.0f), false, nullptr, ETeleportType::TeleportPhysics);
	};
	// Newest first: the most recent spot a stride or more away that has room.
	for (int32 i = GoodSpots.Num() - 1; i >= 0; --i)
	{
		if (FVector::Dist(GoodSpots[i], Here) < 150.0f) { continue; }
		if (Free(GoodSpots[i])) { GoTo(GoodSpots[i]); return true; }
	}
	// Then the player start.
	if (AActor* Start = UGameplayStatics::GetActorOfClass(World, APlayerStart::StaticClass()))
	{
		const FVector At = Start->GetActorLocation();
		if (Free(At)) { GoTo(At); return true; }
	}
	// Last: straight up in steps, for a body that has sunk into the floor.
	for (float Up = 50.0f; Up <= 300.0f; Up += 50.0f)
	{
		if (Free(Here + FVector(0, 0, Up))) { GoTo(Here + FVector(0, 0, Up)); return true; }
	}
	return false;
}

void ABaseCharacter::NoteInjury(const FString& Region, bool bDestroyed)
{
	if (Region == TEXT("LegL") || Region == TEXT("LegR"))
	{
		bLegInjured = true; bLegInjuredLeft = Region == TEXT("LegL");
		bSprintHeld = false;
		UpdateStandingSpeed(); UpdateCrouchWalkSpeed();
	}
	else if (Region == TEXT("ArmR")) { AimInjuryScale = 1.25f; }   // the trigger arm; the support arm costs nothing yet
}

void ABaseCharacter::Die(const FVector& ShotDir)
{
	if (bDead) { return; }
	bDead = true;
	Tags.AddUnique(TEXT("dead"));
	if (UCharacterMovementComponent* Move = GetCharacterMovement()) { Move->StopMovementImmediately(); Move->DisableMovement(); }
	if (UCapsuleComponent* Cap = GetCapsuleComponent()) { Cap->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
	if (WeaponMeshComponent) { WeaponMeshComponent->SetVisibility(false, true); }
	// The body lets go: every bone simulated, the last shot's push on it; the modular parts and
	// the hair ride the leader's bones down with it.
	if (USkeletalMeshComponent* M = GetMesh())
	{
		DeadMeshRelative = M->GetRelativeTransform();
		UDeathPlayComponent::Begin(this, M, UDeathPlayComponent::Pick(this), ShotDir, CurrentConfig.Attributes.Brawn, NAME_None);
	}
}

void ABaseCharacter::Revive()
{
	if (USkeletalMeshComponent* M = GetMesh())
	{
		M->SetSimulatePhysics(false);
		M->SetAllBodiesSimulatePhysics(false);
		M->SetCollisionProfileName(TEXT("CharacterMesh"));
		M->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		if (bDead) { M->SetRelativeTransform(DeadMeshRelative); }
	}
	// Every folded bone back on every mesh, every piece shown again.
	TInlineComponentArray<USkeletalMeshComponent*> Skels(this);
	for (USkeletalMeshComponent* S : Skels)
	{
		if (!S) { continue; }
		for (int32 i = 0; i < S->GetNumBones(); ++i) { S->UnHideBoneByName(S->GetBoneName(i)); }
		S->SetVisibility(true, true);
		S->RecreatePhysicsState();   // the bodies a severing terminated come back
	}
	if (UCapsuleComponent* Cap = GetCapsuleComponent()) { Cap->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); }
	if (UCharacterMovementComponent* Move = GetCharacterMovement()) { Move->SetMovementMode(MOVE_Walking); }
	if (WeaponMeshComponent) { WeaponMeshComponent->SetVisibility(true, true); }
	for (int32 i = Tags.Num() - 1; i >= 0; --i) { const FString T = Tags[i].ToString(); if (T == TEXT("dead") || T == TEXT("headless") || T.StartsWith(TEXT("severed_"))) { Tags.RemoveAt(i); } }
	bDead = false; bLegInjured = false; AimInjuryScale = 1.0f; LimpApplied = 0.0f;
	UpdateStandingSpeed(); UpdateCrouchWalkSpeed();
	if (UVitalityComponent* V = UVitalityComponent::FindOrAdd(this)) { V->Reset(UVitalityComponent::PoolFor(CurrentConfig.Attributes.Endurance, CurrentConfig.Attributes.Brawn)); }
}

void ABaseCharacter::TickLimp(float DeltaSeconds)
{
	// The hitch: the body drops on the bad leg's step and comes back up on the other. The stride
	// clock is the weapon sway's, which already runs at the pace of the feet. Additive to the mesh
	// offset, and forgotten when a crouch rewrites that offset from scratch.
	if (!GetMesh()) { return; }
	if (bIsCrouched != bLimpWasCrouched) { bLimpWasCrouched = bIsCrouched; LimpApplied = 0.0f; }
	const bool bMoving = !bDead && GetVelocity().Size2D() > 30.0f;
	const float Want = (bLegInjured && bMoving) ? LimpDipCm * FMath::Max(0.0f, FMath::Sin(SwayPhase + (bLegInjuredLeft ? 0.0f : PI))) : 0.0f;
	if (FMath::IsNearlyEqual(Want, LimpApplied)) { return; }
	GetMesh()->GetRelativeLocation_DirectMutable().Z += (LimpApplied - Want);
	LimpApplied = Want;
}

float ABaseCharacter::AimSwayTargetDeg() const
{
	if (!WeaponMesh || WeaponStance.IsEmpty()) { return 0.0f; }
	float Base = 0.0f;
	switch (CurrentCarry)
	{
	case EWeaponCarry::ADS:        Base = AimSwayAdsDeg; break;
	case EWeaponCarry::HipFire:    Base = AimSwayHipDeg; break;
	case EWeaponCarry::Shouldered: Base = AimSwayShoulderedDeg; break;
	default:                       return 0.0f;   // low ready: nothing is being aimed
	}
	const FAttributes& A = CurrentConfig.Attributes;
	// Brawn carries part of the weight: at 100 the weapon feels a third lighter, at 0 a third heavier.
	const float FeltKg = WeaponMassKg * (1.0f - FMath::Clamp((A.Brawn - 50) / 50.0f, -1.0f, 1.0f) * 0.35f);
	const float Weight = FMath::Clamp(0.6f + FeltKg * 0.13f, 0.6f, 2.5f);               // 3 kg reads 1.0, an 8 kg gun 1.6, a pistol 0.75
	const float Agile = FMath::Clamp(1.3f - A.Agility / 100.0f * 0.6f, 0.5f, 1.5f);    // 50 reads 1.0; 100 reads 0.7, 0 reads 1.3
	return Base * Weight * Agile * AimSwayScale * AimInjuryScale;
}

void ABaseCharacter::TickAimSway(float DeltaSeconds)
{
	AimSwayAmplitude = FMath::FInterpTo(AimSwayAmplitude, AimSwayTargetDeg(), DeltaSeconds, 4.0f);
	AimSwayClock += DeltaSeconds;
	if (AimSwayAmplitude <= KINDA_SMALL_NUMBER) { AimSway = FRotator::ZeroRotator; return; }
	// Two slow sines per axis at rates that never line up, so the path does not visibly repeat,
	// and a breath on the pitch. Pitch smaller than yaw: arms drift sideways more readily than they rise.
	const float T = AimSwayClock * 2.0f * PI;
	const float Yaw = 0.62f * FMath::Sin(T * 0.42f) + 0.38f * FMath::Sin(T * 0.9f + 1.3f);
	const float Pitch = 0.7f * (0.62f * FMath::Sin(T * 0.37f + 0.7f) + 0.38f * FMath::Sin(T * 1.1f + 2.1f)) + 0.3f * FMath::Sin(T * 0.25f);
	AimSway = FRotator(Pitch * AimSwayAmplitude, Yaw * AimSwayAmplitude, 0.0f);
}

void ABaseCharacter::TickWeaponSway(float DeltaSeconds)
{
	const UCharacterMovementComponent* Move = GetCharacterMovement();
	// Against the RUN speed, not the gait's own cap: measured against MaxWalkSpeed a steady jog
	// counted as a full run and swung at the full-run stride rate, which read as far too fast.
	const float MaxSpeed = FMath::Max(1.0f, RunSpeed * CurrentConfig.SpeedMultiplier);
	const float Speed = (Move && Move->IsMovingOnGround()) ? Move->Velocity.Size2D() : 0.0f;
	const float Want = FMath::Clamp(Speed / MaxSpeed, 0.0f, 1.2f);
	// Settles in and out over a few tenths rather than snapping with the first frame of input.
	SwayAmount = FMath::FInterpTo(SwayAmount, Want, DeltaSeconds, 6.0f);
	if (SwayAmount > 0.02f)
	{
		// Stride rate follows speed: a walk swings slower than a run.
		SwayPhase = FMath::Fmod(SwayPhase + DeltaSeconds * SwayStepsPerSecond * (0.55f + 0.45f * FMath::Min(1.0f, SwayAmount)) * 2.0f * PI, 2.0f * PI);
	}
	else
	{
		// Standing: ease the phase back toward rest so the weapon does not stop mid-swing.
		SwayPhase = FMath::FInterpTo(SwayPhase, SwayPhase > PI ? 2.0f * PI : 0.0f, DeltaSeconds, 8.0f);
	}
	// In the sights most of it goes; the rest reads as breathing on the move.
	const float Scale = SwayAmount * FMath::Lerp(1.0f, SwayAdsScale, AdsAlpha());
	SwayOffset.X = FMath::Sin(SwayPhase) * SwayLateralCm * Scale;
	SwayOffset.Y = -(0.5f - 0.5f * FMath::Cos(2.0f * SwayPhase)) * SwayVerticalCm * Scale;   // dips twice a cycle: one per footfall
	SwayRoll = FMath::Sin(SwayPhase) * SwayRollDegrees * Scale;
}

void ABaseCharacter::TickSightAlignment(float DeltaSeconds)
{
	if (!WeaponMeshComponent || !WeaponMesh) { SightAlignAlpha = 0.0f; return; }

	// Every view. In first person the eye is the camera; in third person it is the CHARACTER'S
	// eye -- the head bone plus the same forward/above offsets the first-person camera uses --
	// aimed along the control rotation. Placing from the camera in third person would swing the
	// weapon out to the over-the-shoulder camera; placing from the head puts it at the sights
	// or the shoulder of the body you are looking at, and the hands follow it by IK.
	const bool bWant = !WeaponStance.IsEmpty();
	const float Target = bWant ? FMath::Clamp(AimSightAlignment, 0.0f, 1.0f) : 0.0f;
	SightBlend.Set(Target, AimSightBlendSeconds);
	SightBlend.Tick(DeltaSeconds);
	SightAlignAlpha = SightBlend.Value;
	// MELEE never aligns to the eye: the swing clips move the arm and the weapon must follow the hand,
	// not sit where a rear sight would be with the hand IK dragging the arm back to it (the jiggle).
	if (SightAlignAlpha <= KINDA_SMALL_NUMBER || bWeaponMelee)   // a blade or a hammer rides the hand; the sights never own it
	{
		// HAND THE WEAPON BACK TO THE HAND. This is where the gun was being left floating behind
		// the player on a camera change: the solve drives the component by WORLD transform,
		// which overwrites the relative transform it has as a child of the grip socket. Simply
		// stopping leaves that stale relative transform in place forever, and it is whatever
		// the last blended frame happened to be -- metres away, in world terms, once the camera
		// has moved somewhere else. So the last frame of the blend has to put it back.
		if (bSightAlignActive)
		{
			bSightAlignActive = false;
			SightAlignAlpha = 0.0f;
			ApplyWeapon();
		}
		return;
	}
	bSightAlignActive = true;

	FVector EyeLoc;
	FRotator EyeRot;
	if (!GetEye(false, EyeLoc, EyeRot)) { return; }

	PlaceWeapon(EyeLoc, EyeRot);
}

// Where the weapon's origin goes and how it is turned, for its sights to sit at the carry
// offset from this eye and the shot to converge on the point of aim. Pure: the same numbers in
// give the same pose out, so Tick and PostCameraTick can both run it.
FTransform ABaseCharacter::SolveWeaponPose(const FVector& EyeLoc, const FRotator& EyeRot) const
{
	// In HAC1 space the weapon points down its own +X, so aligning the weapon with the view is
	// simply giving it the camera's rotation. That is the whole return on normalising the
	// meshes: with ninety-six different conventions this line could not exist.
	// Where the rear sight goes, as an offset from the eye. This one vector is the ONLY thing
	// separating the four carry positions.
	// Between two postures for as long as the move takes, so the weapon travels rather than
	// teleports, and the travel is the configured duration rather than a spring constant that
	// nobody can put a number on.
	const float T = (CarryBlendTotal > KINDA_SMALL_NUMBER)
		? FMath::Clamp(1.0f - CarryBlendLeft / CarryBlendTotal, 0.0f, 1.0f) : 1.0f;
	FVector Offset = FMath::Lerp(CarryOffset(CarryFrom), CarryOffset(CurrentCarry), FMath::InterpEaseInOut(0.0f, 1.0f, T, 2.0f));
	Offset += FVector(0.0f, AimSightNudgeCm.X, AimSightNudgeCm.Y);
	Offset += FVector(0.0f, SwayOffset.X, SwayOffset.Y);   // the stride's sway, in the same eye frame
	// Reloading: worked, not aimed. In and back out over the clip, with a rock on top.
	float HandlingRoll = 0.0f, HandlingPitch = 0.0f;
	if (ReloadLeft > 0.0f && ReloadTotal > KINDA_SMALL_NUMBER)
	{
		const float U = 1.0f - ReloadLeft / ReloadTotal;
		const float Arc = FMath::Sin(U * PI);
		Offset += FVector(-ReloadPullCm * Arc, ReloadSideCm * Arc + FMath::Sin(U * PI * 3.0f) * 1.2f * Arc, -ReloadDropCm * Arc);
		HandlingRoll = -ReloadRollDegrees * Arc;
		HandlingPitch = ReloadPitchDegrees * Arc + FMath::Sin(U * PI * 4.0f) * 2.0f * Arc;
	}
	const FVector SightWorld = EyeLoc + EyeRot.RotateVector(Offset);

	// The point of aim. Everything but low ready keeps the barrel CONVERGING on it, which is
	// what makes "held off to one side" still mean "pointed at the target": the weapon is
	// displaced from the eye, not swivelled away from what you are looking at.
	// Low ready points at the deck a few metres ahead; everything else points at the target.
	// Same line of code either way -- only the point differs.
	auto ConvergePoint = [&](EWeaponCarry Carry)
	{
		return (Carry == EWeaponCarry::LowReady)
			? EyeLoc + EyeRot.Vector() * LowReadyConvergeCm - FVector(0.0f, 0.0f, bInFirstPerson ? LowReadyDropCmFirstPerson : LowReadyDropCm)
			: EyeLoc + EyeRot.Vector() * AimConvergeCm;
	};
	const FVector AimPoint = FMath::Lerp(ConvergePoint(CarryFrom), ConvergePoint(CurrentCarry), T);
	FRotator DesiredRotator = (AimPoint - SightWorld).Rotation();
	// A weapon whose sights sit off the bore line is pitched so that looking along the sight
	// line is looking along the shot. Zero on anything with no usable sights.
	DesiredRotator.Pitch += WeaponSightPitch + HandlingPitch;
	DesiredRotator.Roll += SwayRoll + HandlingRoll;
	const FQuat DesiredRot = DesiredRotator.Quaternion();

	// A weapon with no derived rear sight aims down the bore instead, which is close enough to
	// look deliberate and never looks broken.
	const FVector SightLocal = bWeaponHasSight ? WeaponSightLocal : FVector::ZeroVector;
	// Put the weapon's origin where it has to be for its sight point to land where it belongs.
	const FVector DesiredLoc = SightWorld - DesiredRot.RotateVector(SightLocal);
	const FVector Scale = WeaponMeshComponent ? WeaponMeshComponent->GetComponentScale() : FVector::OneVector;
	return FTransform(DesiredRot, DesiredLoc, Scale);
}

void ABaseCharacter::PlaceWeapon(const FVector& EyeLoc, const FRotator& EyeRot)
{
	if (!WeaponMeshComponent || !WeaponMesh) { return; }
	// OFF THE HAND while the sights own it. The solve writes the weapon's WORLD transform, but a
	// child of the grip socket stores that as a transform RELATIVE to the hand -- and the hand
	// then moves: the idle breathes, and the IK reaches for the weapon. So every frame the
	// weapon was placed, dragged off by the hand's motion, and placed again: a jitter at frame
	// rate, felt as the gun shaking against a perfectly smooth reticle. Detached, it goes where
	// it is put and stays there, and the hands come to IT. ApplyWeapon() puts it back on the
	// socket when the blend runs out.
	if (WeaponMeshComponent->GetAttachParent() != nullptr)
	{
		WeaponMeshComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	}
	const FTransform Desired = SolveWeaponPose(EyeLoc, EyeRot);
	// BLEND FROM THE HAND, NOT FROM WHEREVER IT ENDED UP LAST FRAME. The two ends of this blend
	// are both poses computed fresh this frame -- where the HAND holds the weapon, and where the
	// SIGHTS want it -- so it is a crossfade, not a feedback loop, stable at every alpha and
	// exact at alpha 1.
	const FTransform Current = WeaponMeshComponent->GetComponentTransform();
	const FTransform HandWorld = (GetMesh() && GetMesh()->DoesSocketExist(WeaponGripSocket))
		? GetMesh()->GetSocketTransform(WeaponGripSocket, ERelativeTransformSpace::RTS_World)
		: Current;
	const FTransform Held = WeaponOnSocket() * HandWorld;   // where the hand's hold puts the weapon, correction included
	const FTransform From(Held.GetRotation(), Held.GetLocation(), Current.GetScale3D());
	FTransform Blended;
	Blended.Blend(From, Desired, SightAlignAlpha);
	WeaponMeshComponent->SetWorldTransform(Blended, false, nullptr, ETeleportType::TeleportPhysics);
}

// Where the camera will be at the end of this frame (first person), or where the character's
// eye is (third person), from things that are final by this point of Tick.
void ABaseCharacter::PredictEye()
{
	bPredictedEyeValid = false;
	if (bInFirstPerson && CameraBoom && FollowCamera)
	{
		// What APlayerCameraManager will read: the camera component's world transform. That is
		// the boom (placed by this Tick), the arm's socket -- along the control rotation, its
		// length (zero in first person) and offsets -- and the camera's own relative transform
		// (the groggy sway). No lag, no collision push at zero length, no camera modifiers in
		// this game: the prediction is the camera.
		const FRotator ArmRot = CameraBoom->bUsePawnControlRotation ? GetViewRotation() : CameraBoom->GetComponentRotation();
		const FVector SocketLoc = CameraBoom->GetComponentLocation() + CameraBoom->TargetOffset
			+ ArmRot.RotateVector(CameraBoom->SocketOffset) - ArmRot.Vector() * CameraBoom->TargetArmLength;
		PredictedEyeLoc = SocketLoc + ArmRot.RotateVector(FollowCamera->GetRelativeLocation());
		PredictedEyeRot = (ArmRot.Quaternion() * FollowCamera->GetRelativeRotation().Quaternion()).Rotator();
		bPredictedEyeValid = true;
	}
	else if (GetMesh() && GetMesh()->DoesSocketExist(FirstPersonHeadBone))
	{
		// Third person: the CHARACTER'S eye -- the head bone plus the first-person offsets --
		// aimed along the control rotation. Placing from the camera would swing the weapon out
		// to the over-the-shoulder camera; placing from the head puts it at the sights or the
		// shoulder of the body you are looking at, and the hands follow it by IK.
		const FRotator Ctl = bFreelook ? FrozenAim : GetViewRotation();
		const FRotator YawOnly(0.0f, Ctl.Yaw, 0.0f);
		PredictedEyeLoc = GetMesh()->GetSocketLocation(FirstPersonHeadBone)
		                + YawOnly.RotateVector(FVector(FirstPersonEyeForwardOfHeadCm, 0.0f, FirstPersonEyeAboveHeadCm));
		PredictedEyeRot = Ctl;
		bPredictedEyeValid = true;
	}
}

bool ABaseCharacter::GetEye(bool bFinal, FVector& OutLoc, FRotator& OutRot) const
{
	if (bFinal)
	{
		// After the camera manager has run: its cache IS this frame's camera. In third person
		// the mesh has animated by now too, so the head socket is this frame's as well.
		const APlayerController* PC = Cast<APlayerController>(GetController());
		if (bInFirstPerson && PC && PC->PlayerCameraManager)
		{
			const FMinimalViewInfo& View = PC->PlayerCameraManager->GetCameraCacheView();
			OutLoc = View.Location; OutRot = View.Rotation;
			return true;
		}
		if (!bInFirstPerson && GetMesh() && GetMesh()->DoesSocketExist(FirstPersonHeadBone))
		{
			const FRotator Ctl = bFreelook ? FrozenAim : GetViewRotation();
			const FRotator YawOnly(0.0f, Ctl.Yaw, 0.0f);
			OutLoc = GetMesh()->GetSocketLocation(FirstPersonHeadBone) + YawOnly.RotateVector(FVector(FirstPersonEyeForwardOfHeadCm, 0.0f, FirstPersonEyeAboveHeadCm));
			OutRot = Ctl;
			return true;
		}
		return false;
	}
	if (!bPredictedEyeValid) { return false; }
	OutLoc = PredictedEyeLoc; OutRot = PredictedEyeRot;
	return true;
}

void FWeaponPostCameraTick::ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	if (Target && IsValid(Target) && TickType != LEVELTICK_ViewportsOnly) { Target->PostCameraTick(DeltaTime); }
}

void ABaseCharacter::RegisterActorTickFunctions(bool bRegister)
{
	Super::RegisterActorTickFunctions(bRegister);
	if (bRegister)
	{
		if (PostCameraTickFunction.bCanEverTick && !IsTemplate())
		{
			PostCameraTickFunction.Target = this;
			PostCameraTickFunction.SetTickFunctionEnable(PostCameraTickFunction.bStartWithTickEnabled);
			PostCameraTickFunction.RegisterTickFunction(GetLevel());
		}
	}
	else if (PostCameraTickFunction.IsTickFunctionRegistered())
	{
		PostCameraTickFunction.UnRegisterTickFunction();
	}
}

// After the camera. The weapon goes where the FINAL camera says; in first person that is the
// same place Tick predicted, and the difference -- measured by the lag test -- is the proof.
void ABaseCharacter::PostCameraTick(float DeltaSeconds)
{
	if (!bSightAlignActive || !WeaponMeshComponent || !WeaponMesh) { bLagPrevValid = false; return; }
	FVector EyeLoc; FRotator EyeRot;
	if (!GetEye(true, EyeLoc, EyeRot)) { return; }
	if (LagTestLeft > 0.0f && bPredictedEyeValid)
	{
		const double CamErr = FVector::Dist(EyeLoc, PredictedEyeLoc);
		const double RotErr = FMath::RadiansToDegrees(EyeRot.Quaternion().AngularDistance(PredictedEyeRot.Quaternion()));
		LagCamErrSum += CamErr; LagCamErrMax = FMath::Max(LagCamErrMax, CamErr); LagRotErrMax = FMath::Max(LagRotErrMax, RotErr);
	}
	EyeRot += AimSway;   // the point of aim wanders; the weapon is held along it, sights and all
	PlaceWeapon(EyeLoc, EyeRot);
	if (LagTestLeft > 0.0f)
	{
		++LagFrames;
		const FTransform Camera(EyeRot, EyeLoc);
		const FTransform InCamera = WeaponMeshComponent->GetComponentTransform().GetRelativeTransform(Camera);
		// Only frames where nothing is MEANT to move the weapon in the view: no carry
		// transition in flight, the sight blend complete.
		const bool bSteady = CarryBlendLeft <= 0.0f && SightAlignAlpha >= 1.0f - KINDA_SMALL_NUMBER;
		if (bLagPrevValid && bSteady)
		{
			const double Jump = FVector::Dist(InCamera.GetLocation(), LagPrevWeaponInCamera.GetLocation());
			const double RotJump = FMath::RadiansToDegrees(InCamera.GetRotation().AngularDistance(LagPrevWeaponInCamera.GetRotation()));
			++LagJumpFrames; LagJumpSum += Jump; LagJumpMax = FMath::Max(LagJumpMax, Jump); LagRotJumpMax = FMath::Max(LagRotJumpMax, RotJump);
		}
		LagTurned += FMath::Abs(FRotator::NormalizeAxis(EyeRot.Yaw - LagPrevYaw));
		LagPrevYaw = EyeRot.Yaw;
		LagPrevWeaponInCamera = InCamera; bLagPrevValid = true;
	}
}

void ABaseCharacter::BeginWeaponLagTest(float Seconds)
{
	LagTestLeft = FMath::Max(0.5f, Seconds); LagTestClock = 0.0f;
	LagFrames = 0; LagJumpFrames = 0;
	LagCamErrSum = LagCamErrMax = LagRotErrMax = LagJumpSum = LagJumpMax = LagRotJumpMax = LagTurned = 0.0;
	bLagPrevValid = false;
	FVector L; FRotator R;
	LagPrevYaw = GetEye(true, L, R) ? R.Yaw : 0.0f;
	LastLagReport = TEXT("running");
	UE_LOG(LogTemp, Log, TEXT("WeaponLag: test started (%.1fs, %s, %s)"), LagTestLeft, bInFirstPerson ? TEXT("first person") : TEXT("third person"), WeaponMesh ? TEXT("armed") : TEXT("UNARMED"));
}

void ABaseCharacter::FinishWeaponLagTest()
{
	LagTestLeft = 0.0f;
	if (!WeaponMesh || WeaponStance.IsEmpty()) { LastLagReport = TEXT("WeaponLag: not armed -- nothing measured"); }
	else if (LagFrames == 0) { LastLagReport = TEXT("WeaponLag: no frames measured (sight solve not active?)"); }
	else
	{
		LastLagReport = FString::Printf(TEXT("WeaponLag: %d frames, %s, turned %.0f deg | predicted vs final camera: mean %.3f cm, max %.3f cm, rot max %.3f deg | weapon jump in camera space over %d steady frames: mean %.3f cm, max %.3f cm, rot max %.3f deg"),
			LagFrames, bInFirstPerson ? TEXT("first person") : TEXT("third person"), LagTurned,
			LagCamErrSum / LagFrames, LagCamErrMax, LagRotErrMax,
			LagJumpFrames, LagJumpFrames > 0 ? LagJumpSum / LagJumpFrames : 0.0, LagJumpMax, LagRotJumpMax);
	}
	UE_LOG(LogTemp, Log, TEXT("%s"), *LastLagReport);
}

float ABaseCharacter::GetWeaponSpreadDegrees() const
{
	if (WeaponStance.IsEmpty()) { return 0.0f; }
	// Eased with the carry move, so the reticle tightens AS the gun comes up rather than snapping
	// to the sights cone the instant the button is pressed.
	float Spread = FMath::Lerp(SpreadHipDegrees, SpreadAimDegrees, AdsAlpha());
	Spread *= AimInjuryScale;   // an injured trigger arm: a quarter worse

	if (const UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		const float Top = FMath::Max(1.0f, Move->MaxWalkSpeed);
		Spread += SpreadMoveDegrees * FMath::Clamp(GetVelocity().Size2D() / Top, 0.0f, 1.0f);
		if (Move->IsCrouching()) { Spread *= SpreadCrouchScale; }
		// Feet off the ground beats every other consideration.
		if (Move->IsFalling()) { Spread += SpreadAirborneDegrees; }
	}
	return Spread + FMath::Min(SpreadBloomDegrees, SpreadBloomMaxDegrees);
}

void ABaseCharacter::SetWeaponOptic(UStaticMesh* OpticMesh, const FVector& MountLocal)
{
	if (!OpticMeshComponent)
	{
		if (!WeaponMeshComponent) { return; }
		OpticMeshComponent = NewObject<UStaticMeshComponent>(this, TEXT("OpticMesh"));
		OpticMeshComponent->SetupAttachment(WeaponMeshComponent);
		OpticMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		OpticMeshComponent->RegisterComponent();
	}
	OpticMeshComponent->SetStaticMesh(OpticMesh);
	OpticMeshComponent->SetVisibility(OpticMesh != nullptr);
	bWeaponHasOptic = OpticMesh != nullptr;
	// Bolted flat to the rail: the optic is authored in the same space as the weapon, so the
	// mount is a translation and nothing else.
	OpticMeshComponent->SetRelativeLocation(MountLocal);
	OpticMeshComponent->SetRelativeRotation(FRotator::ZeroRotator);
	// The head is hidden from its owner in first person; the optic must NOT be, or the player
	// aims down a sight they cannot see.
	OpticMeshComponent->SetOwnerNoSee(false);
}

void ABaseCharacter::SetWeaponMesh(UStaticMesh* NewMesh)
{
	WeaponMesh = NewMesh;
	CurrentConfig.WeaponMesh = NewMesh ? NewMesh->GetPathName() : FString();
	ApplyWeapon();
}

void ABaseCharacter::SetWeaponRelativeLocation(const FVector& NewLocation)
{
	WeaponRelativeLocation = NewLocation;
	CurrentConfig.WeaponLocation = NewLocation;
	ApplyWeapon();
}

void ABaseCharacter::SetWeaponRelativeRotation(const FRotator& NewRotation)
{
	WeaponRelativeRotation = NewRotation;
	CurrentConfig.WeaponRotation = NewRotation;
	ApplyWeapon();
}

UCharacterAnimInstance* ABaseCharacter::GetCharacterAnimInstance() const
{
	return GetMesh() ? Cast<UCharacterAnimInstance>(GetMesh()->GetAnimInstance()) : nullptr;
}

AFaceController* ABaseCharacter::GetAttachedFaceController() const
{
	TArray<AActor*> AttachedActors;
	GetAttachedActors(AttachedActors, true, true);
	for (AActor* Attached : AttachedActors)
	{
		if (AFaceController* Attached_Face = Cast<AFaceController>(Attached))
		{
			return Attached_Face;
		}
	}
	return nullptr;
}

void ABaseCharacter::SetCharacterMaterial(UMaterialInterface* Material)
{
	if (!Material) { return; }
	GetMesh()->SetMaterial(0, Material);
	if (FaceController) { FaceController->ApplyMaterialToAttachments(Material); }
}

UMaterialInterface* ABaseCharacter::GetCharacterMaterial() const
{
	return GetMesh() ? GetMesh()->GetMaterial(0) : nullptr;
}

// ---- Character configuration --------------------------------------------

void ABaseCharacter::SyncConfigFromLive()
{
	CurrentConfig.WeaponMesh = WeaponMesh ? WeaponMesh->GetPathName() : FString();
	CurrentConfig.WeaponLocation = WeaponRelativeLocation;
	CurrentConfig.WeaponRotation = WeaponRelativeRotation;
	if (const UCharacterAnimInstance* AnimInst = GetCharacterAnimInstance())
	{
		CurrentConfig.ArmPose = AnimInst->ArmOverridePose ? AnimInst->ArmOverridePose->GetPathName() : TEXT("None");
		CurrentConfig.ArmPoseWeight = AnimInst->ArmOverrideWeight;
	}
	CurrentConfig.Scale = GetMesh()->GetRelativeScale3D();
	if (ModularMID && CurrentConfig.Type == CharacterType::Modular)
	{
		for (const FString& Param : ModularHero::ColorParameters())
		{
			CurrentConfig.Colors.Add(Param, GetPartColor(Param));
		}
	}
}

FCharacterConfig ABaseCharacter::GetDefaultCharacterConfig() const
{
	FCharacterConfig Config = (CurrentConfig.Type == CharacterType::Modular)
		? ModularHero::MakeDefaultConfig(CurrentConfig.Name)
		: SyntyCharacters::MakeDefaultConfig(CurrentConfig.Name,
			OriginalBaseMesh ? OriginalBaseMesh->GetPathName() : FString(),
			OriginalMaterial ? OriginalMaterial->GetPathName() : FString());
	Config.WeaponMesh = OriginalWeaponMesh ? OriginalWeaponMesh->GetPathName() : FString();
	Config.WeaponLocation = OriginalWeaponLocation;
	Config.WeaponRotation = OriginalWeaponRotation;
	Config.ArmPose = DefaultArmPose ? DefaultArmPose->GetPathName() : TEXT("None");
	return Config;
}

FLinearColor ABaseCharacter::GetDefaultPartColor(const FString& Parameter) const
{
	FLinearColor Value = FLinearColor::Black;
	if (const UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, ModularHero::MaterialInstancePath()))
	{
		Material->GetVectorParameterValue(FMaterialParameterInfo(*Parameter), Value);
	}
	return Value;
}

void ABaseCharacter::ApplyCharacterConfig(const FCharacterConfig& Config)
{
	CurrentConfig = Config;

	ApplyBody();

	WeaponMesh = Config.WeaponMesh.IsEmpty() ? nullptr : LoadObject<UStaticMesh>(nullptr, *Config.WeaponMesh);
	WeaponRelativeLocation = Config.WeaponLocation;
	WeaponRelativeRotation = Config.WeaponRotation;
	WeaponRelativeScale = Config.WeaponScale;
	ApplyWeapon();

	if (UCharacterAnimInstance* AnimInst = GetCharacterAnimInstance())
	{
		// "" = leave the anim instance's default; "None" = explicitly none.
		if (Config.ArmPose == TEXT("None")) { AnimInst->ArmOverridePose = nullptr; }
		else if (!Config.ArmPose.IsEmpty()) { AnimInst->ArmOverridePose = LoadObject<UAnimSequence>(nullptr, *Config.ArmPose); }
		// The sword-idle arm layer keeps the weapon clear of the face while
		// running -- pointless (and visibly wrong: arm cocked as if holding a
		// blade) on an unarmed character, so force it off when there's no weapon.
		AnimInst->ArmOverrideWeight = (WeaponMesh != nullptr) ? Config.ArmPoseWeight : 0.0f;
		AnimInst->FingerCurlDegrees = Config.FingerCurlDegrees;
		AnimInst->ThumbCurlDegrees = Config.ThumbCurlDegrees;
		AnimInst->LocomotionChoice = ResolveLocomotionChoice();
		AnimInst->Gait = Config.Gait;
	}

	ApplyFace();
	ApplyScale();
	UpdateStandingSpeed();
	UpdateCrouchWalkSpeed();
	SyncActorTagsFromConfig();
	SyncConfigFromLive();
	UpdateNameLabel();

	// Ambient idle behaviour from the config's clip list (see PlayNextIdle).
	IdleClips.Reset();
	for (const FString& Path : Config.IdleAnimations)
	{
		if (UAnimSequence* Clip = Path.IsEmpty() ? nullptr : LoadObject<UAnimSequence>(nullptr, *Path)) { IdleClips.Add(Clip); }
		else if (!Path.IsEmpty()) { UE_LOG(LogTemp, Warning, TEXT("CharacterConfig: idle clip '%s' not found"), *Path); }
	}
	if (GetWorld()) { GetWorldTimerManager().SetTimerForNextTick(this, &ABaseCharacter::StartIdleBehaviour); }
}

// ---- Ambient idle behaviour ---------------------------------------------------

void ABaseCharacter::StartIdleBehaviour()
{
	GetWorldTimerManager().ClearTimer(IdleTimer);
	GetWorldTimerManager().ClearTimer(GazeTimer);
	GetWorldTimerManager().ClearTimer(GazeAimTimer);
	CurrentGazeTarget.Reset();
	if (IsPlayerControlled()) { return; }
	if (CurrentConfig.GazeTargets.Num() > 0)
	{
		if (CurrentConfig.GazeTurnSpeed > 0.0f)
		{
			if (UCharacterAnimInstance* AnimInst = GetCharacterAnimInstance()) { AnimInst->LookAtTurnSpeed = CurrentConfig.GazeTurnSpeed; }
		}
		PickGazeTarget();
		GetWorldTimerManager().SetTimer(GazeAimTimer, this, &ABaseCharacter::UpdateGaze, 0.2f, true);
	}
	if (IdleClips.Num() == 0) { return; }
	PlayNextIdle();
}

bool ABaseCharacter::MatchesConfigName(const FString& Name) const
{
	if (Name.IsEmpty()) { return false; }
	return Name == DefaultCharacterConfigName || Name == CurrentConfig.Name || Name == CurrentConfig.Name.Replace(TEXT(" "), TEXT(""));
}

void ABaseCharacter::PickGazeTarget()
{
	const TArray<FGazeTarget>& Targets = CurrentConfig.GazeTargets;
	if (Targets.Num() == 0 || !GetWorld()) { return; }
	float Total = 0.0f;
	for (const FGazeTarget& T : Targets) { Total += FMath::Max(0.0f, T.Weight); }
	FString Pick = Targets[0].Target;
	// Weighted draw; try a few times not to repeat the current target.
	for (int32 Attempt = 0; Attempt < 4; ++Attempt)
	{
		float R = FMath::FRand() * FMath::Max(Total, KINDA_SMALL_NUMBER);
		for (const FGazeTarget& T : Targets)
		{
			R -= FMath::Max(0.0f, T.Weight);
			if (R <= 0.0f) { Pick = T.Target; break; }
		}
		if (Pick != CurrentGazeTarget || Targets.Num() == 1) { break; }
	}
	CurrentGazeTarget = Pick;
	UpdateGaze();
	const float Hold = FMath::FRandRange(FMath::Min(CurrentConfig.GazeHoldMin, CurrentConfig.GazeHoldMax), FMath::Max(CurrentConfig.GazeHoldMin, CurrentConfig.GazeHoldMax));
	GetWorldTimerManager().SetTimer(GazeTimer, this, &ABaseCharacter::PickGazeTarget, FMath::Max(0.3f, Hold), false);
}

void ABaseCharacter::SetGazeLock(bool bLock, const FVector& Point)
{
	bGazeLocked = bLock;
	GazeLockPoint = Point;
	if (bLock) { SetLookAtTarget(Point); } else { ClearLookAtTarget(); }
}

void ABaseCharacter::UpdateGaze()
{
	if (bGazeLocked) { SetLookAtTarget(GazeLockPoint); return; }
	if (!GetWorld() || CurrentGazeTarget.IsEmpty() || CurrentGazeTarget == TEXT("none")) { ClearLookAtTarget(); return; }
	FVector Point = FVector::ZeroVector;
	bool bFound = false;
	if (CurrentGazeTarget == TEXT("player"))
	{
		// The camera: in a cinematic that is the player's eye, in play their view.
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
		{
			if (PC->PlayerCameraManager) { Point = PC->PlayerCameraManager->GetCameraLocation(); bFound = true; }
			else if (APawn* P = PC->GetPawn()) { Point = P->GetActorLocation() + FVector(0, 0, 60.0f); bFound = true; }
		}
	}
	else if (CurrentGazeTarget == TEXT("prop"))
	{
		if (WeaponMeshComponent && WeaponMeshComponent->GetStaticMesh()) { Point = WeaponMeshComponent->Bounds.Origin; bFound = true; }
	}
	else if (CurrentGazeTarget.StartsWith(TEXT("character:")))
	{
		const FString Who = CurrentGazeTarget.Mid(10);
		for (TActorIterator<ABaseCharacter> It(GetWorld()); It; ++It)
		{
			if (*It != this && It->MatchesConfigName(Who))
			{
				Point = It->GetMesh() && It->GetMesh()->DoesSocketExist(TEXT("head")) ? It->GetMesh()->GetSocketLocation(TEXT("head")) : It->GetActorLocation() + FVector(0, 0, 60.0f);
				bFound = true;
				break;
			}
		}
	}
	if (bFound) { SetLookAtTarget(Point); } else { ClearLookAtTarget(); }
}

void ABaseCharacter::PlayNextIdle()
{
	if (IdleClips.Num() == 0 || !GetWorld()) { return; }
	// A single clip is a held loop: no gaps, no drop back to the base idle.
	if (IdleClips.Num() == 1)
	{
		if (LastIdleIndex != 0) { LastIdleIndex = 0; PlayAnimationClip(IdleClips[0], true); }
		return;
	}
	// Pick a different clip than last time when there is a choice.
	int32 Index = FMath::RandRange(0, IdleClips.Num() - 1);
	if (IdleClips.Num() > 1 && Index == LastIdleIndex) { Index = (Index + 1) % IdleClips.Num(); }
	LastIdleIndex = Index;
	UAnimSequence* Clip = IdleClips[Index];
	PlayAnimationClip(Clip, false);
	// Without a gaze schedule: glance at the player every other clip.
	if (CurrentConfig.GazeTargets.Num() == 0 && !bGazeLocked)
	{
		IdleGlanceToggle = !IdleGlanceToggle;
		if (IdleGlanceToggle)
		{
			if (APawn* P = UGameplayStatics::GetPlayerPawn(GetWorld(), 0)) { SetLookAtTarget(P->GetActorLocation() + FVector(0, 0, 60.0f)); }
		}
		else { ClearLookAtTarget(); }
	}
	const float Next = (Clip ? Clip->GetPlayLength() : 2.0f) + FMath::FRandRange(0.4f, 1.6f);
	GetWorldTimerManager().SetTimer(IdleTimer, this, &ABaseCharacter::PlayNextIdle, Next, false);
}

// ---- Edit-mode presentation: highlight shell and name tag -------------------

void ABaseCharacter::SetHoverHighlight(bool bEnabled)
{
	if (bHoverHighlight == bEnabled) { return; }
	bHoverHighlight = bEnabled;
	ApplyHighlightOverlay();
}

void ABaseCharacter::SetSelectedHighlight(bool bEnabled)
{
	if (bSelectedHighlight == bEnabled) { return; }
	bSelectedHighlight = bEnabled;
	ApplyHighlightOverlay();
}

void ABaseCharacter::ApplyHighlightOverlay()
{
	// One rim-glow shell (M_CharacterHighlight: unlit, translucent, fresnel
	// opacity, flagged for skeletal AND static meshes) drawn over every
	// visible mesh of the character -- body, modular parts, weapon -- via the
	// mesh overlay-material slot, so it reads the same on every rig whether
	// or not it has a face system. Selected (gold) wins over hover (blue).
	// (The ghost preview material was tried first: it lacks the skeletal-
	// mesh usage flag, so only the static weapon mesh ever showed it.)
	UMaterialInterface* Overlay = nullptr;
	if (bSelectedHighlight || bHoverHighlight)
	{
		if (!HighlightMID)
		{
			if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Characters/M_CharacterHighlight.M_CharacterHighlight")))
			{
				HighlightMID = UMaterialInstanceDynamic::Create(Base, this);
			}
		}
		if (HighlightMID)
		{
			HighlightMID->SetVectorParameterValue(TEXT("HighlightColor"), bSelectedHighlight ? FLinearColor(0.75f, 1.0f, 0.8f) : FLinearColor(0.32f, 1.0f, 0.45f));
			HighlightMID->SetScalarParameterValue(TEXT("Intensity"), bSelectedHighlight ? 1.0f : 0.8f);
			Overlay = HighlightMID;
		}
	}

	GetMesh()->SetOverlayMaterial(Overlay);
	for (const TPair<FString, TObjectPtr<USkeletalMeshComponent>>& Pair : PartComponents)
	{
		if (Pair.Value) { Pair.Value->SetOverlayMaterial(Overlay); }
	}
	if (WeaponMeshComponent) { WeaponMeshComponent->SetOverlayMaterial(Overlay); }
}

void ABaseCharacter::SetNameLabelVisible(bool bVisible)
{
	// Edit mode's say; the fade in Tick reconciles it with inspection.
	bNameLabelEditVisible = bVisible;
	if (bVisible && NameLabel && !NameLabel->IsVisible()) { NameLabel->SetVisibility(true); }
}

void ABaseCharacter::SetInspectedNameplate(bool bInspected)
{
	if (bNameLabelInspected == bInspected) { return; }
	bNameLabelInspected = bInspected;
	if (!bInspected) { NameLabelInspectLostTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f; }
}

void ABaseCharacter::UpdateNameLabel()
{
	if (!NameLabel) { return; }
	if (UCharacterNameWidget* Tag = Cast<UCharacterNameWidget>(NameLabel->GetUserWidgetObject()))
	{
		Tag->SetName(CurrentConfig.Name);
	}
}

void ABaseCharacter::UpdateNameLabelScale()
{
	UCharacterNameWidget* Tag = NameLabel ? Cast<UCharacterNameWidget>(NameLabel->GetUserWidgetObject()) : nullptr;
	const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!Tag || !Camera) { return; }

	// Distance is measured from the viewer's own character when there is
	// one (a third-person camera hangs ~4m behind it, which would keep every
	// tag at the far size even when standing nose to nose); from the camera
	// when flying. 0 at NearDistance (and closer), 1 at FarDistance (and
	// beyond), then an ease-out so the size drops quickly as the viewer
	// starts to pull away and only creeps before settling at the minimum.
	FVector From = Camera->GetCameraLocation();
	if (const APlayerController* PC = Camera->GetOwningPlayerController())
	{
		if (const APawn* Viewer = PC->GetPawn()) { if (!Viewer->IsA<ASpectatorPawn>()) { From = Viewer->GetActorLocation() + FVector(0, 0, 60.0f); } }
	}
	const float Distance = FVector::Dist(From, NameLabel->GetComponentLocation());
	const float Span = FMath::Max(NameLabelFarDistance - NameLabelNearDistance, 1.0f);
	const float T = FMath::Clamp((Distance - NameLabelNearDistance) / Span, 0.0f, 1.0f);
	const float Eased = 1.0f - FMath::Square(1.0f - T);
	const float Size = FMath::Lerp(static_cast<float>(NameLabelMaxFontSize), static_cast<float>(NameLabelMinFontSize), Eased);
	Tag->SetFontSize(FMath::RoundToInt(Size));
}

void ABaseCharacter::SetCharacterName(const FString& NewName)
{
	CurrentConfig.Name = NewName;
	UpdateNameLabel();
}

bool ABaseCharacter::SaveCharacterConfig()
{
	SyncConfigFromLive();
	const bool bOk = CharacterConfigFile::Save(CurrentConfig);
	if (bOk) { MarkConfigSaved(); }
	return bOk;
}

bool ABaseCharacter::LoadCharacterConfig(const FString& Name)
{
	FCharacterConfig Config;
	if (!CharacterConfigFile::Load(Name, Config)) { return false; }
	ApplyCharacterConfig(Config);
	MarkConfigSaved();
	return true;
}

void ABaseCharacter::MarkConfigSaved()
{
	SyncConfigFromLive();
	FString Json;
	SavedConfigJson = FJsonObjectConverter::UStructToJsonObjectString(CurrentConfig, Json) ? Json : FString();
}

bool ABaseCharacter::IsConfigDirty()
{
	SyncConfigFromLive();
	FString Json;
	if (!FJsonObjectConverter::UStructToJsonObjectString(CurrentConfig, Json)) { return false; }
	return Json != SavedConfigJson;
}

void ABaseCharacter::RevertToSavedConfig()
{
	if (SavedConfigJson.IsEmpty()) { return; }
	FCharacterConfig Saved;
	if (FJsonObjectConverter::JsonObjectStringToUStruct(SavedConfigJson, &Saved)) { ApplyCharacterConfig(Saved); }
}

void ABaseCharacter::NewCharacterConfig(const FString& Name)
{
	FCharacterConfig Config = GetDefaultCharacterConfig();
	Config.Name = Name;
	ApplyCharacterConfig(Config);
}

// ---- Body ------------------------------------------------------------------

void ABaseCharacter::ApplyBody()
{
	if (CurrentConfig.Type == CharacterType::Modular)
	{
		ApplyModularBody();
	}
	else
	{
		ApplySingleBody();
	}
}

void ABaseCharacter::ApplyModularBody()
{
	if (!bModularMode)
	{
		const TCHAR* BasePath = CurrentConfig.IsSciFiKit() ? CutLibrary::BaseMeshPath() : ModularHero::BaseMeshPath();
		USkeletalMesh* BaseMesh = LoadObject<USkeletalMesh>(nullptr, BasePath);
		if (!BaseMesh)
		{
			UE_LOG(LogTemp, Warning, TEXT("ABaseCharacter: modular base mesh not found at %s"), BasePath);
			return;
		}
		bModularMode = true;

		// The base rig is the animated leader every part follows; it never
		// renders itself. AlwaysTickPoseAndRefreshBones so a hidden leader
		// still produces the bone transforms its visible followers read.
		GetMesh()->SetSkeletalMeshAsset(BaseMesh);
		GetMesh()->SetVisibility(false);
		GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		OnBaseMeshChanged();

		if (!ModularMID && !CurrentConfig.IsSciFiKit())
		{
			if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, ModularHero::MaterialInstancePath()))
			{
				ModularMID = UMaterialInstanceDynamic::Create(Material, this);
			}
		}
	}

	if (CurrentConfig.IsSciFiKit())
	{
		// Cut-library parts keep their own atlas materials; only the skin tint applies.
		for (const FString& Slot : CutLibrary::Slots()) { ApplyPart(Slot); }
		ApplySkinTint();
		return;
	}
	for (const ModularHero::FSlotDef& Slot : ModularHero::Slots())
	{
		ApplyPart(Slot.Slot);
	}
	ApplyColors();
}

void ABaseCharacter::ApplySingleBody()
{
	USkeletalMesh* BodyMesh = CurrentConfig.BaseMesh.IsEmpty() ? OriginalBaseMesh.Get() : LoadObject<USkeletalMesh>(nullptr, *CurrentConfig.BaseMesh);
	if (!BodyMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("ABaseCharacter: base mesh not found: %s"), *CurrentConfig.BaseMesh);
		return;
	}

	// Leaving modular mode: the leader rig stops being hidden and the part
	// followers go away.
	for (TPair<FString, TObjectPtr<USkeletalMeshComponent>>& Pair : PartComponents)
	{
		if (Pair.Value) { Pair.Value->SetSkeletalMeshAsset(nullptr); Pair.Value->SetVisibility(false); }
	}
	bModularMode = false;

	if (GetMesh()->GetSkeletalMeshAsset() != BodyMesh)
	{
		GetMesh()->SetSkeletalMeshAsset(BodyMesh);
		OnBaseMeshChanged();
	}
	GetMesh()->SetVisibility(true);

	UMaterialInterface* Material = CurrentConfig.Material.IsEmpty() ? nullptr : LoadObject<UMaterialInterface>(nullptr, *CurrentConfig.Material);
	if (!Material && BodyMesh->GetMaterials().Num() > 0) { Material = BodyMesh->GetMaterials()[0].MaterialInterface; }
	if (Material)
	{
		GetMesh()->SetMaterial(0, Material);
		if (FaceController) { FaceController->ApplyMaterialToAttachments(Material); }
	}
}

void ABaseCharacter::OnBaseMeshChanged()
{
	USkeletalMesh* CurrentMesh = GetMesh()->GetSkeletalMeshAsset();
	if (!CurrentMesh || !CurrentMesh->GetSkeleton()) { return; }

	// Every clip this project owns lives on one skeleton; a mesh on another
	// skeleton asset (Synty ships a Mannequin copy per pack, and the modular
	// hero has its own) needs the compatibility link or nothing plays. The
	// anim proxy handles bone-axis differences on top of this -- see
	// FCharacterAnimInstanceProxy::EnsureRetargetTables.
	const UCharacterAnimInstance* AnimDefaults = GetDefault<UCharacterAnimInstance>();
	USkeleton* AnimSkeleton = (AnimDefaults && AnimDefaults->IdleAnim) ? AnimDefaults->IdleAnim->GetSkeleton() : nullptr;
	if (AnimSkeleton && AnimSkeleton != CurrentMesh->GetSkeleton())
	{
		// One direction only (mesh skeleton accepts the animation skeleton).
		// Registering both directions made the remapping registry compose
		// two remaps and apply the reference-pose correction twice.
		CurrentMesh->GetSkeleton()->AddCompatibleSkeleton(AnimSkeleton);
	}

	// SetSkeletalMeshAsset re-created the anim instance; point its grip
	// correction at this rig's finger naming.
	if (UCharacterAnimInstance* AnimInst = GetCharacterAnimInstance())
	{
		const bool bModularFingers = GetMesh()->GetBoneIndex(TEXT("indexFinger_01_r")) != INDEX_NONE;
		AnimInst->GripFingerBones = bModularFingers
			? TArray<FName>{ TEXT("indexFinger_01_r"), TEXT("indexFinger_02_r"), TEXT("indexFinger_03_r"), TEXT("finger_01_r"), TEXT("finger_02_r"), TEXT("finger_03_r") }
			: TArray<FName>{ TEXT("index_01_r"), TEXT("index_02_r"), TEXT("index_03_r"), TEXT("middle_01_r"), TEXT("middle_02_r"), TEXT("middle_03_r") };
	}
}

// Snap a piece onto its socket but KEEP its own relative scale, so it rides whatever scale the
// character is wearing. The engine's SnapToTargetNotIncludingScale uses KeepWorld for scale,
// which bakes 1/CharacterScale into the child the instant it attaches: the body then grows or
// shrinks and the hair, beard, brows, nose and body parts stay their authored size.
static const FAttachmentTransformRules SnapKeepScale(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepRelative, false);

void ABaseCharacter::ApplyPart(const FString& Slot)
{
	const ModularHero::FSlotDef* Def = ModularHero::FindSlot(Slot);
	const bool bCutSlot = CutLibrary::Slots().Contains(Slot);
	if ((!Def && !bCutSlot) || !bModularMode) { return; }
	const FName AttachSocket = Def ? Def->AttachSocket : NAME_None;

	const FString* Path = CurrentConfig.Parts.Find(Slot);
	USkeletalMesh* PartMesh = (Path && !Path->IsEmpty()) ? LoadObject<USkeletalMesh>(nullptr, **Path) : nullptr;

	TObjectPtr<USkeletalMeshComponent>& Comp = PartComponents.FindOrAdd(Slot);
	if (!Comp)
	{
		if (!PartMesh) { return; }
		Comp = NewObject<USkeletalMeshComponent>(this, *FString::Printf(TEXT("Part_%s"), *Slot));
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->RegisterComponent();
		Comp->AttachToComponent(GetMesh(), SnapKeepScale, AttachSocket);
		Comp->SetRelativeScale3D(FVector::OneVector);   // the part is authored at the body's own size
	}

	Comp->SetSkeletalMeshAsset(PartMesh);
	Comp->SetVisibility(PartMesh != nullptr);
	if (!PartMesh) { return; }

	// Parts on the shared rig follow the leader pose; anything with its own
	// skeleton (the BackAttachments folder) just rides its socket.
	// Cut parts come from three packs with three Mannequin skeleton assets
	// that share every bone name, so they always follow the leader.
	const USkeletalMesh* LeaderMesh = GetMesh()->GetSkeletalMeshAsset();
	const bool bSharedRig = bCutSlot || (LeaderMesh && PartMesh->GetSkeleton() == LeaderMesh->GetSkeleton());
	Comp->SetLeaderPoseComponent((bSharedRig && AttachSocket.IsNone()) ? GetMesh() : nullptr);

	if (ModularMID && !bCutSlot)
	{
		for (int32 i = 0; i < Comp->GetNumMaterials(); ++i)
		{
			Comp->SetMaterial(i, ModularMID);
		}
	}
}

void ABaseCharacter::ApplyColors()
{
	if (!ModularMID) { return; }
	for (const TPair<FString, FLinearColor>& Pair : CurrentConfig.Colors)
	{
		ModularMID->SetVectorParameterValue(*Pair.Key, Pair.Value);
	}
}


// ---- Footsteps -------------------------------------------------------------

void ABaseCharacter::FootstepTune(float PlantedFraction, float MinInterval)
{
	FootPlantSpeedFraction = FMath::Clamp(PlantedFraction, 0.05f, 0.9f);
	FootstepMinInterval = FMath::Max(0.02f, MinInterval);
	UE_LOG(LogTemp, Log, TEXT("Footsteps: planted<%.2f of body speed, min interval %.2fs"), FootPlantSpeedFraction, FootstepMinInterval);
}

void ABaseCharacter::TickFootsteps(float DeltaSeconds)
{
	// Only the player's own boots: these are 2D sounds, so an NPC walking across the facility
	// would be heard at full volume in the player's ear.
	if (!bFootstepsEnabled || !IsLocallyControlled() || !Cast<APlayerController>(GetController())) { return; }
	const UCharacterMovementComponent* Move = GetCharacterMovement();
	USkeletalMeshComponent* Skel = GetMesh();
	if (!Move || !Skel || DeltaSeconds <= 0.0f) { return; }
	if (Move->IsFalling()) { LastFallSpeedZ = FMath::Abs(GetVelocity().Z); }

	// Steps come off the SKELETON, not off ground covered: a distance accumulator drifts against
	// whatever the animation is doing and can never line up with the feet.
	//
	// The signal is the one procedural systems normally use: a foot that is carrying weight is
	// STATIONARY IN THE WORLD, while a swinging foot is travelling at about twice the body's
	// speed. Watching the foot's height instead, as an earlier version did, finds the bottom of
	// the arc -- which is close to contact but not the same frame, and is noisy on a retargeted
	// clip. The moment the world-space motion stops is the moment the boot takes the load.
	//
	// The proper fix for this is an AnimNotify on each locomotion clip at its own contact frame,
	// authored once against the animation. This is the runtime stand-in for that, and it needs
	// nothing added to the assets.
	const bool bGrounded = Move->IsMovingOnGround();
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	// Turning on the spot is still walking as far as the feet are concerned: the body barely
	// translates while the boots pick up and set down all the way round. Measuring translation
	// alone silenced the entire shuffle. The rate of turn is converted into the speed the feet
	// are actually travelling at, so one test covers walking, turning, and both at once.
	const float Yaw = GetActorRotation().Yaw;
	float YawRate = 0.0f;
	if (bLastYawValid) { YawRate = FMath::Abs(FRotator::NormalizeAxis(Yaw - LastYawForSteps)) / DeltaSeconds; }
	LastYawForSteps = Yaw; bLastYawValid = true;
	const float TurnFootSpeed = FMath::DegreesToRadians(YawRate) * FootstepStanceRadiusCm;
	const float BodySpeed = FMath::Max(GetVelocity().Size2D(), TurnFootSpeed);
	const bool bUnderway = GetVelocity().Size2D() >= FootstepMinSpeed || YawRate >= FootstepMinYawRate;
	const float BaseZ = GetCapsuleComponent()->GetComponentLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FName Bones[2] = { FootBoneLeft, FootBoneRight };

	// Sample both feet first: the test below wants to know which of the two is lower.
	FVector Positions[2]; float Heights[2] = { 0.0f, 0.0f }; bool bHave[2] = { false, false };
	for (int32 i = 0; i < 2; ++i)
	{
		if (!Skel->DoesSocketExist(Bones[i])) { continue; }
		Positions[i] = Skel->GetSocketLocation(Bones[i]);
		Heights[i] = Positions[i].Z - BaseZ;
		bHave[i] = true;
	}

	for (int32 i = 0; i < 2; ++i)
	{
		if (!bHave[i]) { continue; }
		FFootTrack& Foot = FootTracks[i];
		if (!Foot.bValid)
		{
			Foot.World = Positions[i]; Foot.Height = Heights[i]; Foot.bDown = true; Foot.bValid = true;
			continue;
		}
		const float FootSpeed = (Positions[i] - Foot.World).Size() / DeltaSeconds;
		// Scaled to the body so it holds at any gait, with a floor so that standing still does
		// not read as one continuous plant.
		const float Threshold = FMath::Max(12.0f, BodySpeed * FootPlantSpeedFraction);
		const bool bDownNow = FootSpeed < Threshold;
		// The other foot being higher is what separates a real plant from both feet briefly
		// slowing at the top of a stride.
		const int32 Other = 1 - i;
		const bool bLower = !bHave[Other] || Heights[i] <= Heights[Other] + 2.0f;

		if (bGrounded && bUnderway && bDownNow && !Foot.bDown && bLower
			&& (Now - Foot.LastStepTime) > FootstepMinInterval)
		{
			Foot.LastStepTime = Now;
			PlayFootstep(false);
		}
		Foot.bDown = bDownNow;
		Foot.World = Positions[i];
		Foot.Height = Heights[i];
	}
}

void ABaseCharacter::PlayFootstep(bool bLanding)
{
	// Crouched, the boot is set down deliberately and the deck barely answers.
	const bool bQuiet = bIsCrouched;
	FString File;
	if (bLanding)
	{
		File = bQuiet ? TEXT("step_land_soft.wav") : TEXT("step_land.wav");
	}
	else
	{
		const int32 Count = bQuiet ? 4 : 6;
		int32 Pick = FMath::RandRange(1, Count);
		if (Pick == LastFootstepIndex) { Pick = Pick % Count + 1; }   // never the same sample twice running
		LastFootstepIndex = Pick;
		File = FString::Printf(TEXT("step_%s_%d.wav"), bQuiet ? TEXT("soft") : TEXT("metal"), Pick);
	}

	float Volume = FootstepLandVolume;
	if (!bLanding)
	{
		const UCharacterMovementComponent* Move = GetCharacterMovement();
		const float TopSpeed = Move ? FMath::Max(FootstepMinSpeed + 1.0f, Move->MaxWalkSpeed) : 600.0f;
		// Creeping is quieter than running even on the same sample.
		Volume = FootstepVolume * FMath::GetMappedRangeValueClamped(
			FVector2D(FootstepMinSpeed, TopSpeed), FVector2D(0.55f, 1.0f), GetVelocity().Size2D());
	}
	if (bQuiet) { Volume *= FootstepCrouchVolumeScale; }
	// Where the listener is standing. Zoom level 0 is the first-person eye; anything else is a
	// camera out behind the character, which needs the feet a lot further back in the mix.
	Volume *= (CurrentZoomLevelIndex == 0) ? FootstepFirstPersonVolumeScale : FootstepThirdPersonVolumeScale;
	// The player's own dial, applied last so it scales everything above it. Step 0 is silence.
	if (const URepliCanUserSettings* Settings = URepliCanUserSettings::Get())
	{
		static const float Steps[5] = { 0.0f, 0.5f, 1.0f, 1.6f, 2.4f };
		Volume *= Steps[FMath::Clamp(Settings->FootstepVolumeStep, 0, 4)];
	}
	if (Volume <= KINDA_SMALL_NUMBER) { return; }
	// A little pitch either way, so six samples do not wear out over a long corridor.
	UAmbientPlayer::PlayOneShot(this, GetWorld(), File, Volume, FMath::FRandRange(0.92f, 1.09f));
	++FootstepsPlayed;
}

void ABaseCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	if (bFootstepsEnabled && IsLocallyControlled() && Cast<APlayerController>(GetController())
		&& LastFallSpeedZ > FootstepLandMinFallSpeed)
	{
		PlayFootstep(true);
	}
	LastFallSpeedZ = 0.0f;
	// The landing IS the step for both feet; hold them off so the touchdown does not also
	// register as two plants on the same frame.
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	FootTracks[0].LastStepTime = Now;
	FootTracks[1].LastStepTime = Now;
}

void ABaseCharacter::SetZoomLevel(int32 Index)
{
	CurrentZoomLevelIndex = FMath::Clamp(Index, 0, FMath::Max(0, ZoomArmLengths.Num() - 1));
}

void ABaseCharacter::OnWeaponFired(const FVector& MuzzleLocal)
{
	SpreadBloomDegrees = FMath::Min(SpreadBloomDegrees + SpreadPerShotDegrees, SpreadBloomMaxDegrees);
	PlayWeaponAction(TEXT("Fire"));
	// The flash sits at the weapon's own muzzle point, so a launcher lights up a metre further
	// out than a pistol does.
	if (MuzzleFlash)
	{
		MuzzleFlash->SetRelativeLocation(MuzzleLocal);
		MuzzleFlash->SetVisibility(true);
		MuzzleFlashLeft = MuzzleFlashSeconds;
	}
	// A wisp of propellant smoke off the muzzle: a few hundredths of the pack's small smoke
	// system, tiny, pointed down the bore so it drifts away from the shooter.
	if (!MuzzleSmokeSystem.IsEmpty() && WeaponMeshComponent && MuzzleSmokeScale > 0.0f)
	{
		const FTransform& WT = WeaponMeshComponent->GetComponentTransform();
		ImpactEffects::SpawnBurst(GetWorld(), { MuzzleSmokeSystem, MuzzleSmokeScale, MuzzleSmokeSeconds }, WT.TransformPosition(MuzzleLocal), WT.GetRotation().Rotator());
	}
	// Recoil throws the VIEW up rather than the arms: the shot has already been traced down the
	// camera's line, so kicking the camera afterwards moves the next shot, not this one.
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		float Scale = RecoilScaleShouldered;
		switch (CurrentCarry)
		{
		case EWeaponCarry::ADS:      Scale = RecoilScaleADS; break;
		case EWeaponCarry::HipFire:  Scale = RecoilScaleHipFire; break;
		case EWeaponCarry::LowReady: Scale = RecoilScaleLowReady; break;
		default: break;
		}
		// The weapon's own number when it has one (zero is a real answer: no kick), the character's
		// default otherwise. Applied to the control rotation directly rather than as pitch INPUT:
		// input goes through InputPitchScale, whose sign is a project setting, and a kick that
		// arrived through it went DOWN.
		const float Kick = (WeaponRecoilOverride >= 0.0f ? WeaponRecoilOverride : RecoilPitchDegrees) * Scale;
		if (Kick > KINDA_SMALL_NUMBER)
		{
			FRotator R = PC->GetControlRotation(); const float Before = R.Pitch; R.Pitch = FRotator::NormalizeAxis(R.Pitch + Kick); PC->SetControlRotation(R);
			UE_LOG(LogTemp, Log, TEXT("Recoil: kick %.2f (weapon %.2f, scale %.2f) pitch %.2f -> %.2f (read back %.2f)"), Kick, WeaponRecoilOverride, Scale, Before, R.Pitch, PC->GetControlRotation().Pitch);
		}
		else { UE_LOG(LogTemp, Log, TEXT("Recoil: no kick (weapon %.2f, scale %.2f)"), WeaponRecoilOverride, Scale); }
		// Queue the return. A burst stacks kicks; each recovers over the same window from now.
		RecoilToRecover += Kick * FMath::Clamp(RecoilRecoverFraction, 0.0f, 1.0f);
		RecoilRecoverLeft = RecoilRecoverSeconds;
	}
}

void ABaseCharacter::TickRecoil(float DeltaSeconds)
{
	if (RecoilRecoverLeft <= 0.0f || RecoilToRecover <= KINDA_SMALL_NUMBER) { RecoilToRecover = 0.0f; return; }
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) { RecoilToRecover = 0.0f; RecoilRecoverLeft = 0.0f; return; }
	const float Step = RecoilToRecover * FMath::Clamp(DeltaSeconds / FMath::Max(RecoilRecoverLeft, KINDA_SMALL_NUMBER), 0.0f, 1.0f);
	{ FRotator R = PC->GetControlRotation(); R.Pitch = FRotator::NormalizeAxis(R.Pitch - Step); PC->SetControlRotation(R); }   // back down, the same way it went up
	RecoilToRecover -= Step;
	RecoilRecoverLeft -= DeltaSeconds;
}

float ABaseCharacter::AdsAlpha() const
{
	const float T = (CarryBlendTotal > KINDA_SMALL_NUMBER)
		? FMath::InterpEaseInOut(0.0f, 1.0f, FMath::Clamp(1.0f - CarryBlendLeft / CarryBlendTotal, 0.0f, 1.0f), 2.0f)
		: 1.0f;
	if (CurrentCarry == EWeaponCarry::ADS) { return (CarryFrom == EWeaponCarry::ADS || CarryBlendLeft <= 0.0f) ? 1.0f : T; }
	if (CarryFrom == EWeaponCarry::ADS && CarryBlendLeft > 0.0f) { return 1.0f - T; }
	return 0.0f;
}

EWeaponCarry ABaseCharacter::PosedCarry() const
{
	// Past the midpoint of a move the arms belong to the destination; before it, the origin.
	if (CarryBlendLeft > 0.0f && CarryBlendTotal > KINDA_SMALL_NUMBER && CarryBlendLeft > CarryBlendTotal * 0.5f)
	{
		return CarryFrom;
	}
	return CurrentCarry;
}

void ABaseCharacter::ApplyScale()
{
	// Mesh only, about its origin (the feet), so the capsule and camera are
	// untouched and the character keeps standing on the ground. Everything hanging off the mesh
	// -- body parts, and through the face controller the hair, beard, brows and nose -- inherits
	// this because each is attached keeping its own relative scale (see SnapKeepScale).
	GetMesh()->SetRelativeScale3D(CurrentConfig.Scale);
	for (const TPair<FString, TObjectPtr<USkeletalMeshComponent>>& Pair : PartComponents)
	{
		if (Pair.Value) { Pair.Value->SetRelativeScale3D(FVector::OneVector); }
	}
}

// ---- Face ------------------------------------------------------------------

FQuat ABaseCharacter::ComputeBoneFrameConversion(FName BoneName) const
{
	const USkeletalMesh* CurrentMesh = GetMesh()->GetSkeletalMeshAsset();
	const UCharacterAnimInstance* AnimDefaults = GetDefault<UCharacterAnimInstance>();
	const USkeleton* AnimSkeleton = (AnimDefaults && AnimDefaults->IdleAnim) ? AnimDefaults->IdleAnim->GetSkeleton() : nullptr;
	if (!CurrentMesh || !AnimSkeleton || CurrentMesh->GetSkeleton() == AnimSkeleton) { return FQuat::Identity; }

	// Component-space reference rotation of the bone on both rigs.
	auto BoneRefCS = [BoneName](const FReferenceSkeleton& Ref) -> FQuat
	{
		int32 Index = Ref.FindBoneIndex(BoneName);
		FQuat CS = FQuat::Identity;
		while (Index != INDEX_NONE)
		{
			CS = Ref.GetRefBonePose()[Index].GetRotation() * CS;
			Index = Ref.GetParentIndex(Index);
		}
		return CS;
	};
	const FQuat SourceCS = BoneRefCS(AnimSkeleton->GetReferenceSkeleton());
	const FQuat TargetCS = BoneRefCS(CurrentMesh->GetRefSkeleton());
	return TargetCS.Inverse() * SourceCS;
}

void ABaseCharacter::ApplyFace()
{
	const FCharacterFaceConfig& Face = CurrentConfig.Face;

	if (!FaceController)
	{
		FaceController = GetAttachedFaceController();
	}
	if (!FaceController && Face.bEnabled && GetWorld())
	{
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		FaceController = GetWorld()->SpawnActor<AFaceController>(AFaceController::StaticClass(), GetActorTransform(), Params);
	}
	if (!FaceController) { return; }
	FaceController->OnClicked.AddUniqueDynamic(this, &ABaseCharacter::HandleActorClicked);

	FaceController->SetActorHiddenInGame(!Face.bEnabled || IsHidden());
	if (!Face.bEnabled) { return; }

	FaceController->SetHeadFrameConversion(ComputeBoneFrameConversion(TEXT("head")));
	FaceController->NoseMesh = Face.NoseMesh.IsEmpty() ? nullptr : LoadObject<UStaticMesh>(nullptr, *Face.NoseMesh);
	FaceController->NoseRelativeLocation = Face.NoseLocation;
	FaceController->NoseRelativeRotation = Face.NoseRotation;
	FaceController->NoseRelativeScale = Face.NoseScale;
	FaceController->BrowMesh = Face.BrowMesh.IsEmpty() ? nullptr : LoadObject<UStaticMesh>(nullptr, *Face.BrowMesh);
	FaceController->BrowLocation = Face.BrowLocation;
	FaceController->BrowTilt = Face.BrowTilt;
	FaceController->BrowScale = Face.BrowScale;
	FaceController->HairColor = Face.HairColor;
	FaceController->MouthRelativeLocation = Face.MouthLocation;
	FaceController->MouthRelativeRotation = Face.MouthRotation;
	FaceController->MouthScale = Face.MouthScale;
	FaceController->SetMouthTint(Face.MouthTint);

	// The Goblin War Camp hair/beard/hat attachments only fit that pack's
	// heads; the modular hero has its own hair slots.
	if (CurrentConfig.Type == CharacterType::Modular && !CurrentConfig.IsSciFiKit())
	{
		FaceController->SetHairIndex(-1);
		FaceController->SetFacialHairIndex(-1);
		FaceController->SetHeadGearIndex(-1);
	}
	else
	{
		FaceController->SetFacialHairMesh(Face.FacialHairMesh.IsEmpty() ? nullptr : LoadObject<UStaticMesh>(nullptr, *Face.FacialHairMesh));
		FaceController->SetHairMesh(Face.HairMesh.IsEmpty() ? nullptr : LoadObject<UStaticMesh>(nullptr, *Face.HairMesh));
		// Offset and scale first: SetHeadGearMesh applies them as it attaches.
		FaceController->HeadGearOffset = Face.HeadGearLocation;
		FaceController->HeadGearScale = Face.HeadGearScale > 0.0f ? Face.HeadGearScale : 1.0f;
		FaceController->SetHeadGearMesh(Face.HeadGearMesh.IsEmpty() ? nullptr : LoadObject<UStaticMesh>(nullptr, *Face.HeadGearMesh));
		FaceController->SetHeadGearBadge(Face.HeadGearBadgeMaterial.IsEmpty() ? nullptr : LoadObject<UMaterialInterface>(nullptr, *Face.HeadGearBadgeMaterial), Face.HeadGearBadgeLocation, Face.HeadGearBadgeRotation, Face.HeadGearBadgeSize);
	}

	FaceController->AttachToCharacter(GetMesh(), TEXT("head"));
	ApplySkinTint();
	MatchNoseToSkin();
	FaceController->SetMouthDecalEnabled(Face.bMouthDecal);
	if (!Face.Expression.IsEmpty()) { FaceController->SetExpression(*Face.Expression); }
	FaceController->SetAutoBlink(Face.bAutoBlink);
}

void ABaseCharacter::SetFaceConfig(const FCharacterFaceConfig& NewFace)
{
	CurrentConfig.Face = NewFace;
	ApplyFace();
}

void ABaseCharacter::SetNose(const FString& MeshPath, const FVector& Scale)
{
	CurrentConfig.Face.bEnabled = true;
	CurrentConfig.Face.bMouthDecal = false;   // Synty heads paint their own mouths
	CurrentConfig.Face.NoseMesh = MeshPath;
	CurrentConfig.Face.NoseScale = Scale;
	ApplyFace();
}

void ABaseCharacter::SetBrows(const FString& MeshPath, const FVector& Location, float Tilt, const FVector& Scale)
{
	CurrentConfig.Face.bEnabled = true;
	CurrentConfig.Face.BrowMesh = MeshPath;
	CurrentConfig.Face.BrowLocation = Location;
	CurrentConfig.Face.BrowTilt = Tilt;
	CurrentConfig.Face.BrowScale = Scale;
	ApplyFace();
}

void ABaseCharacter::SetActorHiddenInGame(bool bNewHidden)
{
	Super::SetActorHiddenInGame(bNewHidden);
	if (FaceController) { FaceController->SetActorHiddenInGame(bNewHidden || !CurrentConfig.Face.bEnabled); }
}

void ABaseCharacter::SetHairColor(const FLinearColor& Color)
{
	CurrentConfig.Face.HairColor = Color;
	if (FaceController) { FaceController->HairColor = Color; FaceController->ApplyHairColor(); }
}

void ABaseCharacter::SetFaceOffsets(const FVector& NoseLocation, const FVector& MouthLocation)
{
	CurrentConfig.Face.NoseLocation = NoseLocation;
	CurrentConfig.Face.MouthLocation = MouthLocation;
	if (FaceController)
	{
		FaceController->SetNoseRelativeLocation(NoseLocation);
		FaceController->SetMouthRelativeLocation(MouthLocation);
	}
}

void ABaseCharacter::SetMouthLook(float Scale, const FLinearColor& Tint)
{
	CurrentConfig.Face.MouthScale = Scale;
	CurrentConfig.Face.MouthTint = Tint;
	if (FaceController)
	{
		FaceController->SetMouthScale(Scale);
		FaceController->SetMouthTint(Tint);
	}
}

// ---- Granular setters --------------------------------------------------------

void ABaseCharacter::SetCharacterType(const FString& NewType)
{
	if (NewType == CurrentConfig.Type) { return; }
	// Switching kinds: start from that kind's stock config but keep the
	// name, weapon and everything else that isn't body-specific.
	FCharacterConfig Config = CurrentConfig;
	Config.Type = NewType;
	const FCharacterConfig Stock = (NewType == CharacterType::Modular)
		? ModularHero::MakeDefaultConfig(Config.Name)
		: SyntyCharacters::MakeDefaultConfig(Config.Name,
			OriginalBaseMesh ? OriginalBaseMesh->GetPathName() : FString(),
			OriginalMaterial ? OriginalMaterial->GetPathName() : FString());
	Config.Parts = Stock.Parts;
	Config.Gender = Stock.Gender;
	Config.BaseMesh = Stock.BaseMesh;
	Config.Material = Stock.Material;
	Config.Face.bMouthDecal = Stock.Face.bMouthDecal;
	ApplyCharacterConfig(Config);
}

void ABaseCharacter::SetBaseMesh(const FString& AssetPath)
{
	CurrentConfig.Type = CharacterType::Single;
	CurrentConfig.BaseMesh = AssetPath;
	CurrentConfig.Material.Empty();   // a new mesh brings its own palette family
	ApplySingleBody();
	ApplyFace();
}

void ABaseCharacter::SetPaletteMaterial(const FString& AssetPath)
{
	CurrentConfig.Material = AssetPath;
	ApplySingleBody();
	MatchNoseToSkin();   // the skin cell may be a different color on this palette
}

void ABaseCharacter::SetGender(const FString& NewGender)
{
	if (NewGender == CurrentConfig.Gender) { return; }
	const FString OldGender = CurrentConfig.Gender;
	CurrentConfig.Gender = NewGender;

	// With no explicit Locomotion choice the movement set follows the sex
	// (the anim proxy swaps on its next update).
	if (UCharacterAnimInstance* AnimInst = GetCharacterAnimInstance()) { AnimInst->LocomotionChoice = ResolveLocomotionChoice(); }

	// Carry each gendered part across by number (Torso_Male_07 ->
	// Torso_Female_07) when the counterpart exists, else fall back to the
	// first option so the body never ends up with holes.
	for (const ModularHero::FSlotDef& Slot : ModularHero::Slots())
	{
		if (!Slot.bGendered) { continue; }
		FString* Path = CurrentConfig.Parts.Find(Slot.Slot);
		if (!Path || Path->IsEmpty()) { continue; }

		const FString WantedName = FPackageName::ObjectPathToObjectName(*Path)
			.Replace(*FString::Printf(TEXT("_%s_"), *OldGender), *FString::Printf(TEXT("_%s_"), *NewGender));
		const TArray<FAssetData> Options = ModularHero::PartOptions(Slot, NewGender);
		const FAssetData* Match = Options.FindByPredicate([&WantedName](const FAssetData& D) { return D.AssetName.ToString() == WantedName; });
		if (!Match && Options.Num() > 0) { Match = &Options[0]; }
		*Path = Match ? Match->GetObjectPathString() : FString();
		ApplyPart(Slot.Slot);
	}
}

ESyntyLocomotionChoice ABaseCharacter::ResolveLocomotionChoice() const
{
	const FString& Set = CurrentConfig.Locomotion;
	if (Set == TEXT("Goblin")) { return ESyntyLocomotionChoice::Goblin; }
	if (Set == TEXT("Female")) { return ESyntyLocomotionChoice::Female; }
	if (Set == TEXT("Male")) { return ESyntyLocomotionChoice::Male; }
	return CurrentConfig.Gender == TEXT("Female") ? ESyntyLocomotionChoice::Female : ESyntyLocomotionChoice::Male;
}

FString ABaseCharacter::GetLocomotionSetName() const
{
	switch (ResolveLocomotionChoice())
	{
	case ESyntyLocomotionChoice::Goblin: return TEXT("Goblin");
	case ESyntyLocomotionChoice::Female: return TEXT("Female");
	default: return TEXT("Male");
	}
}

void ABaseCharacter::SetLocomotionSet(const FString& SetName)
{
	CurrentConfig.Locomotion = (SetName == TEXT("Goblin") || SetName == TEXT("Female") || SetName == TEXT("Male")) ? SetName : FString();
	if (UCharacterAnimInstance* AnimInst = GetCharacterAnimInstance()) { AnimInst->LocomotionChoice = ResolveLocomotionChoice(); }
}

void ABaseCharacter::PlayAnimationClip(UAnimSequence* Clip, bool bLoop)
{
	if (!Clip) { return; }
	TArray<FCombatStep> Steps;
	Steps.Add(MakeStep(Clip, bLoop));
	PlayCombatSequence(Steps, /*bInterrupt=*/true);
}

void ABaseCharacter::StopAnimationClip()
{
	// Cut whatever try-out clip is playing (held loop or mid one-shot) and
	// drop back to ordinary locomotion at once.
	if (!bIsAttacking) { return; }
	CombatQueue.Reset();
	CombatStepIndex = -1;
	bCombatHolding = false;
	bIsAttacking = false;
	CombatOnComplete = nullptr;
	if (UCharacterAnimInstance* AnimInst = GetCharacterAnimInstance()) { AnimInst->AbortAttack(); }
}

void ABaseCharacter::TickHeadlamp()
{
	if (!Headlamp || !Headlamp->IsVisible()) { return; }
	// Down the aim ray, not down the head bone. GetAimRotation is what the reticle represents;
	// the head bone is wherever the look-at and the locomotion have put it.
	const FVector Target = GetAimOrigin() + GetAimRotation().Vector() * FMath::Max(100.0f, HeadlampConvergeCm);
	const FVector From = Headlamp->GetComponentLocation();
	Headlamp->SetWorldRotation((Target - From).Rotation());
}

void ABaseCharacter::ToggleHeadlamp()
{
	if (!Headlamp) { return; }
	Headlamp->SetVisibility(!Headlamp->IsVisible());
	UAmbientPlayer::PlayOneShot(this, GetWorld(), TEXT("switch_click.wav"), 0.5f, 1.0f);
}

bool ABaseCharacter::IsHeadlampOn() const { return Headlamp && Headlamp->IsVisible(); }

void ABaseCharacter::BeginSit(AActor* Seat, float SeatHeight, float FacingYaw, float LeanDegrees, float HunchDegrees)
{
	if (!Seat || IsSitting()) { return; }
	ApproachSeat = Seat; ApproachHeight = SeatHeight; ApproachYaw = FacingYaw; ApproachLean = LeanDegrees; ApproachHunch = HunchDegrees; ApproachTime = 0.0f;
}

void ABaseCharacter::SitOn(AActor* Seat, float SeatHeight, float FacingYaw, float LeanDegrees, float HunchDegrees)
{
	if (!Seat || IsSitting()) { return; }
	UAnimSequence* Loop = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Animations/Lyra/Bench/int_sit_bench_sit_idle_loop.int_sit_bench_sit_idle_loop"));
	if (!Loop) { UE_LOG(LogTemp, Warning, TEXT("SitOn: no sit loop")); return; }
	SeatActor = Seat;
	StandLocation = GetActorLocation();
	// The bench loop holds the pelvis 53 cm over the root and the seat bones sit about 8 cm
	// under it, so the root goes 45 cm below the seat top. Facing is absolute (the tag's yaw).
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->SetMovementMode(MOVE_None);
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector SeatBase = Seat->GetActorLocation();
	SetActorLocationAndRotation(FVector(SeatBase.X, SeatBase.Y, SeatBase.Z + SeatHeight - 45.0f + HalfHeight), FRotator(0.0f, FacingYaw, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
	// The bench loop reclines against a backrest a stool does not have: tip the whole mesh forward.
	SeatMeshRotation = GetMesh()->GetRelativeRotation();
	if (!FMath::IsNearlyZero(LeanDegrees)) { GetMesh()->AddWorldRotation(FQuat(GetActorRightVector(), FMath::DegreesToRadians(LeanDegrees))); }
	// The bench loop reclines; the gait hunch bends the spine forward from the hips so the torso sits
	// upright over the stool with the pelvis where it is.
	SeatHunchBefore = CurrentConfig.Gait.HunchDegrees;
	if (!FMath::IsNearlyZero(HunchDegrees)) { SetGaitAdjustment(TEXT("HunchDegrees"), SeatHunchBefore + HunchDegrees); }
	PlayAnimationClip(Loop, true);
}

void ABaseCharacter::StandUp()
{
	if (!IsSitting()) { return; }
	SeatActor.Reset();
	GetMesh()->SetRelativeRotation(SeatMeshRotation);
	SetGaitAdjustment(TEXT("HunchDegrees"), SeatHunchBefore);
	StopAnimationClip();
	SetActorLocation(StandLocation, false, nullptr, ETeleportType::TeleportPhysics);
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}

bool ABaseCharacter::ResolveSkinColor(FLinearColor& OutColor) const
{
	if (CurrentConfig.Type != CharacterType::Modular) { return false; }
	if (ModularMID && ModularMID->GetVectorParameterValue(FMaterialParameterInfo(TEXT("Color_Skin")), OutColor)) { return true; }
	if (const FLinearColor* Stored = CurrentConfig.Colors.Find(TEXT("Color_Skin"))) { OutColor = *Stored; return true; }
	return false;
}

void ABaseCharacter::MatchNoseToSkin()
{
	if (!FaceController) { return; }
	FLinearColor Skin = FLinearColor::White;
	bool bKnown = ResolveSkinColor(Skin);
	// A skin tint replaces the painted tone, so the nose takes the tint itself.
	if (CurrentConfig.SkinTint.A > 0.0f) { Skin = CurrentConfig.SkinTint; Skin.A = 1.0f; bKnown = true; }
	FaceController->MatchNoseToSkin(GetMesh(), bKnown, Skin);
}

void ABaseCharacter::SetGaitAdjustments(const FGaitAdjustments& NewGait)
{
	CurrentConfig.Gait = NewGait;
	if (UCharacterAnimInstance* AnimInst = GetCharacterAnimInstance()) { AnimInst->Gait = NewGait; }
}

void ABaseCharacter::SetGaitAdjustment(const FString& FieldName, float Value)
{
	// By reflection so the panel, JSON and Python all address fields by the
	// same names as the struct declares them.
	if (const FFloatProperty* Prop = FindFProperty<FFloatProperty>(FGaitAdjustments::StaticStruct(), *FieldName))
	{
		FGaitAdjustments Gait = CurrentConfig.Gait;
		Prop->SetPropertyValue_InContainer(&Gait, Value);
		SetGaitAdjustments(Gait);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SetGaitAdjustment: no float field '%s' on FGaitAdjustments"), *FieldName);
	}
}

void ABaseCharacter::SetLimpSide(const FString& Side)
{
	FGaitAdjustments Gait = CurrentConfig.Gait;
	Gait.LimpSide = (Side == TEXT("Right")) ? TEXT("Right") : TEXT("Left");
	SetGaitAdjustments(Gait);
}

float ABaseCharacter::GetGaitAdjustment(const FString& FieldName) const
{
	if (const FFloatProperty* Prop = FindFProperty<FFloatProperty>(FGaitAdjustments::StaticStruct(), *FieldName))
	{
		return Prop->GetPropertyValue_InContainer(&CurrentConfig.Gait);
	}
	return 0.0f;
}

void ABaseCharacter::SetPart(const FString& Slot, const FString& AssetPath)
{
	CurrentConfig.Parts.Add(Slot, AssetPath);
	const bool bWantSciFi = CutLibrary::Slots().Contains(Slot);
	if (bModularMode && bWantSciFi != CurrentConfig.IsSciFiKit())
	{
		// Changing kit: rebuild on the other base rig.
		CurrentConfig.Kit = bWantSciFi ? TEXT("SciFi") : TEXT("");
		for (const TPair<FString, TObjectPtr<USkeletalMeshComponent>>& Pair : PartComponents) { if (Pair.Value) { Pair.Value->DestroyComponent(); } }
		PartComponents.Reset();
		bModularMode = false;
	}
	if (!bModularMode)
	{
		CurrentConfig.Type = CharacterType::Modular;
		if (bWantSciFi) { CurrentConfig.Kit = TEXT("SciFi"); }
		ApplyModularBody(); ApplyFace(); return;
	}
	ApplyPart(Slot);
	if (CurrentConfig.IsSciFiKit()) { ApplySkinTint(); }
}

void ABaseCharacter::SetPartColor(const FString& Parameter, const FLinearColor& Color)
{
	CurrentConfig.Colors.Add(Parameter, Color);
	if (ModularMID) { ModularMID->SetVectorParameterValue(*Parameter, Color); }
	if (Parameter == TEXT("Color_Skin")) { MatchNoseToSkin(); }
}

FLinearColor ABaseCharacter::GetPartColor(const FString& Parameter) const
{
	if (const FLinearColor* Stored = CurrentConfig.Colors.Find(Parameter)) { return *Stored; }
	if (ModularMID)
	{
		FLinearColor Value = FLinearColor::Black;
		ModularMID->GetVectorParameterValue(FMaterialParameterInfo(*Parameter), Value);
		return Value;
	}
	return GetDefaultPartColor(Parameter);
}

void ABaseCharacter::SetCharacterScale(const FVector& NewScale)
{
	CurrentConfig.Scale = NewScale;
	ApplyScale();
}

void ABaseCharacter::SetSpeedMultiplier(float NewMultiplier)
{
	CurrentConfig.SpeedMultiplier = FMath::Max(NewMultiplier, 0.05f);
	UpdateStandingSpeed();
	UpdateCrouchWalkSpeed();
}

void ABaseCharacter::SyncActorTagsFromConfig()
{
	Tags.Reset();
	for (const FString& Tag : CurrentConfig.Tags)
	{
		const FString Trimmed = Tag.TrimStartAndEnd();
		if (!Trimmed.IsEmpty()) { Tags.AddUnique(FName(*Trimmed)); }
	}
}

void ABaseCharacter::SetCharacterTags(const FString& CommaSeparatedTags)
{
	TArray<FString> Parsed;
	CommaSeparatedTags.ParseIntoArray(Parsed, TEXT(","), /*CullEmpty=*/true);

	CurrentConfig.Tags.Reset();
	for (FString& Tag : Parsed)
	{
		Tag.TrimStartAndEndInline();
		if (!Tag.IsEmpty()) { CurrentConfig.Tags.AddUnique(Tag); }
	}
	SyncActorTagsFromConfig();
}

FString ABaseCharacter::GetCharacterTagsString() const
{
	return FString::Join(CurrentConfig.Tags, TEXT(", "));
}

void ABaseCharacter::SetArmPose(UAnimSequence* Pose)
{
	if (UCharacterAnimInstance* AnimInst = GetCharacterAnimInstance()) { AnimInst->ArmOverridePose = Pose; }
	CurrentConfig.ArmPose = Pose ? Pose->GetPathName() : TEXT("None");
}

void ABaseCharacter::SetArmPoseWeight(float Weight)
{
	if (UCharacterAnimInstance* AnimInst = GetCharacterAnimInstance()) { AnimInst->ArmOverrideWeight = Weight; }
	CurrentConfig.ArmPoseWeight = Weight;
}

void ABaseCharacter::SetGripCurl(float FingerDegrees, float ThumbDegrees)
{
	CurrentConfig.FingerCurlDegrees = FingerDegrees;
	CurrentConfig.ThumbCurlDegrees = ThumbDegrees;
	if (UCharacterAnimInstance* AnimInst = GetCharacterAnimInstance())
	{
		AnimInst->FingerCurlDegrees = FingerDegrees;
		AnimInst->ThumbCurlDegrees = ThumbDegrees;
	}
}

void ABaseCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Cosmetics this character spawned for itself go with it.
	if (FaceController && FaceController->GetOwner() == this) { FaceController->Destroy(); }
	Super::EndPlay(EndPlayReason);
}

void ABaseCharacter::Tick(float DeltaSeconds)
{
	// Walking up to a seat: steer at its centre under normal locomotion; close enough (or long enough), sit.
	if (AActor* Seat = ApproachSeat.Get())
	{
		ApproachTime += DeltaSeconds;
		FVector To = Seat->GetActorLocation() - GetActorLocation(); To.Z = 0.0f;
		if (To.Size() < 28.0f || ApproachTime > 4.0f)
		{
			ApproachSeat.Reset();
			SitOn(Seat, ApproachHeight, ApproachYaw, ApproachLean, ApproachHunch);
		}
		else { AddMovementInput(To.GetSafeNormal(), 0.55f); }
	}
	Super::Tick(DeltaSeconds);

	if (DebugAutoMoveForward != 0.0f && IsLocallyControlled())
	{
		// World direction fixed when the test input starts (the character
		// turns toward it, so a facing-relative angle would chase itself).
		if (!bDebugAutoMoveActive)
		{
			bDebugAutoMoveActive = true;
			DebugAutoMoveWorldDir = GetActorForwardVector().RotateAngleAxis(DebugAutoMoveYaw, FVector::UpVector);
		}
		AddMovementInput(DebugAutoMoveWorldDir, DebugAutoMoveForward);
	}
	else { bDebugAutoMoveActive = false; }

	// Name label: one fade driven by edit mode OR inspection (with its
	// hold-over after the reticle leaves), sized by distance while shown.
	if (NameLabel)
	{
		const float NowSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		const bool bInspectHeld = bNameLabelInspected || (NowSeconds - NameLabelInspectLostTime) < NameLabelInspectHoldSeconds;
		const float TargetAlpha = (bNameLabelEditVisible || bInspectHeld) ? 1.0f : 0.0f;
		const float Rate = TargetAlpha > NameLabelAlpha ? NameLabelFadeInPerSecond : NameLabelFadeOutPerSecond;
		NameLabelAlpha = FMath::FInterpConstantTo(NameLabelAlpha, TargetAlpha, DeltaSeconds, Rate);
		const bool bShow = NameLabelAlpha > 0.005f;
		if (NameLabel->IsVisible() != bShow) { NameLabel->SetVisibility(bShow); }
		if (bShow)
		{
			if (UUserWidget* Tag = NameLabel->GetUserWidgetObject()) { Tag->SetRenderOpacity(NameLabelAlpha); }
			UpdateNameLabelScale();
		}
	}

	if (bMarkSavedOnFirstTick && GetCharacterAnimInstance())
	{
		bMarkSavedOnFirstTick = false;
		MarkConfigSaved();
	}

	if (ZoomArmLengths.IsValidIndex(CurrentZoomLevelIndex))
	{
		// Aiming pulls the camera in over the shoulder and narrows the lens. Both
		// ride the same easing as zoom, so raising the sights is a move rather
		// than a cut.
		const float TargetArmLength = ZoomArmLengths[CurrentZoomLevelIndex] * CameraArmScale * (bAiming ? AimArmScale : 1.0f);
		CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, TargetArmLength, DeltaSeconds, ZoomInterpSpeed);
	}
	if (FollowCamera)
	{
		const float TargetFov = bAiming ? AimFieldOfView : HipFieldOfView;
		FollowCamera->SetFieldOfView(FMath::FInterpTo(FollowCamera->FieldOfView, TargetFov, DeltaSeconds, AimInterpSpeed));
	}
	if (bFreelook && ShouldFaceAim())
	{
		// bUseControllerRotationYaw is off, so nothing else is turning him; pin him to the aim
		// he had when the key went down.
		FRotator Held = GetActorRotation();
		Held.Yaw = FrozenAim.Yaw;
		SetActorRotation(Held);
	}
	TickWeaponStance(DeltaSeconds);
	// A weapon given at BeginPlay can arrive before the modular mesh is assembled, so the socket
	// did not exist and ApplyWeapon took the fallback path. Once it does exist, re-apply.
	if (WeaponMesh && WeaponMeshComponent && GetMesh() && GetMesh()->DoesSocketExist(WeaponGripSocket)
		&& WeaponMeshComponent->GetAttachSocketName() != WeaponGripSocket)
	{
		ApplyWeapon();
	}
	TickRecoil(DeltaSeconds);
	// The weapon, the hands and the headlamp are placed AFTER the camera boom below has been
	// put where this frame's camera will be -- see PredictEye.

	// Manager-open framing (see ManagerCameraSideOffset), eased the same way
	// as zoom so opening/closing the panel doesn't snap the view.
	{
		FVector SocketOffset = CameraBoom->SocketOffset;
		SocketOffset.Y = FMath::FInterpTo(SocketOffset.Y, bManagerCameraOffset ? ManagerCameraSideOffset : 0.0f, DeltaSeconds, ZoomInterpSpeed);
		CameraBoom->SocketOffset = SocketOffset;
	}

	// Index 0 is always first-person. Rather than hide the whole body from
	// the owning camera, keep it visible -- the player sees their own arms,
	// hands, weapon, torso and legs -- and hide only the HEAD region, which
	// is what would otherwise clip through the camera (the face, eyebrows,
	// hair, and the nose/cosmetics on the attached FaceController). Uses
	// SetOwnerNoSee throughout so the character still renders in full for
	// any other viewer.
	const bool bShouldBeFirstPerson = (CurrentZoomLevelIndex == 0);
	if (bShouldBeFirstPerson != bInFirstPerson)
	{
		bInFirstPerson = bShouldBeFirstPerson;
		ApplyFirstPersonHeadHiding(bInFirstPerson);

		// Third person: body turns to face movement direction over time
		// (TurnRateDegPerSec), independent of where the camera/mouse is
		// pointing -- the "aim" is handled separately by the head look-at
		// system instead (see UpdateReticleLookAtTarget). First person is a
		// different expectation entirely: the camera direction IS the
		// player's own facing, so the body needs to snap to match mouselook
		// immediately, not ease toward it the way movement-direction turning
		// does. bUseControllerRotationYaw applies the controller's yaw to the
		// actor directly every tick with no smoothing of its own (unlike
		// CharacterMovementComponent's RotationRate-limited orient-to-
		// movement), which is exactly the instant response first person
		// needs.
		ApplyFacingMode();
	}

	// Crouching shrinks the capsule AND lowers its center (to keep the base
	// planted -- see bCrouchMaintainsBaseLocation above), and CameraBoom is
	// just attached to that same capsule, so it drops by the same amount
	// unless compensated. Computed directly in WORLD space (capsule bottom
	// + a fixed height above it) rather than as a relative-offset
	// compensation formula, specifically to remove any room for a subtle
	// "compensating for a moving parent" arithmetic mistake -- an earlier
	// relative-offset version of this had exactly such a bug. Recomputed
	// continuously (not reacting once in OnStartCrouch/OnEndCrouch the way
	// the mesh offset fix does) so this stays correct even if the player
	// zooms between first and third person mid-crouch.
	{
		const ACharacter* DefaultChar = GetClass()->GetDefaultObject<ACharacter>();
		const float DefaultHalfHeight = DefaultChar->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const float CapsuleHalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const float CapsuleCenterWorldZ = GetCapsuleComponent()->GetComponentLocation().Z;
		const float CapsuleBottomWorldZ = CapsuleCenterWorldZ - CapsuleHalfHeight;
		const float FollowFraction = bInFirstPerson ? 1.0f : ThirdPersonCrouchCameraDropFraction;

		// Two candidate world heights, blended by FollowFraction:
		//  - Track the capsule's own center 1:1 (FollowFraction=1): what a
		//    plain fixed relative offset already does on its own, correct
		//    for first person where the "eyes" should follow the capsule
		//    down when crouching.
		//  - Anchor to the capsule's BOTTOM plus a FIXED height built from
		//    the DEFAULT (standing) half-height, not the current one
		//    (FollowFraction=0): the capsule's bottom is the one point
		//    bCrouchMaintainsBaseLocation guarantees stays put during
		//    crouch, so this gives a world Z that's genuinely constant
		//    across crouch state while still following ordinary vertical
		//    movement normally (jumping, falling, stairs all move the
		//    capsule's bottom too -- just not crouching).
		const float TrackCapsuleCenterZ = CapsuleCenterWorldZ + EyeHeightOffset;
		const float AnchorToCapsuleBaseZ = CapsuleBottomWorldZ + EyeHeightOffset + DefaultHalfHeight;
		const float TargetWorldZ = FMath::Lerp(AnchorToCapsuleBaseZ, TrackCapsuleCenterZ, FollowFraction);

		// Interpolate from OUR OWN tracked value, not a fresh read of
		// CameraBoom->GetComponentLocation() -- see CurrentCameraBoomWorldZ's
		// header comment for why that read can be transiently wrong the
		// instant the capsule itself moves.
		if (!bCameraBoomWorldZInitialized)
		{
			CurrentCameraBoomWorldZ = TargetWorldZ;
			bCameraBoomWorldZInitialized = true;
		}
		CurrentCameraBoomWorldZ = FMath::FInterpTo(CurrentCameraBoomWorldZ, TargetWorldZ, DeltaSeconds, CrouchCameraInterpSpeed);

		FVector BoomWorldLoc = CameraBoom->GetComponentLocation();
		BoomWorldLoc.Z = CurrentCameraBoomWorldZ;

		// First person puts the eye on the HEAD, not on the capsule axis. Measured on this
		// project's player: standing, the head bone is 166 cm above the capsule's base and 3 cm
		// forward of its axis, while the camera sat at 163 cm and on the axis -- inside the
		// throat, which is why looking down showed the inside of the neck and chest. Crouched it
		// is worse: the pose hunches the head to 108 cm and 43 cm FORWARD, so a camera on the
		// axis ends up a full 35 cm behind the body it is meant to be inside the head of, looking
		// out through the back of the chest. Riding the bone fixes both, because the eye now goes
		// wherever the animation actually puts the head.
		if (bInFirstPerson && GetMesh() && GetMesh()->DoesSocketExist(FirstPersonHeadBone))
		{
			const FRotator YawOnly(0.0f, GetActorRotation().Yaw, 0.0f);
			const FVector HeadWorld = GetMesh()->GetSocketLocation(FirstPersonHeadBone);
			FVector TargetLocal = YawOnly.UnrotateVector(HeadWorld - GetActorLocation());
			TargetLocal.X += FirstPersonEyeForwardOfHeadCm;
			TargetLocal.Z += FirstPersonEyeAboveHeadCm;
			TargetLocal.X = FMath::Clamp(TargetLocal.X, -FirstPersonEyeMaxLeadCm, FirstPersonEyeMaxLeadCm);
			TargetLocal.Y = FMath::Clamp(TargetLocal.Y, -FirstPersonEyeMaxLeadCm, FirstPersonEyeMaxLeadCm);

			// The eye is swept out from the body's own axis to where the head says it belongs.
			// Crouched, the pose leans the head 43 cm forward and the eye has to follow it or the
			// camera is left behind the chest looking through it; a flat clamp to the capsule
			// radius does exactly that. Sweeping means the eye leads the body freely in the open
			// and stops short of a wall when the character crouches up against one.
			if (UWorld* W = GetWorld())
			{
				const FVector AnchorWorld = GetActorLocation() + YawOnly.RotateVector(FVector(0.0f, 0.0f, TargetLocal.Z));
				const FVector WantWorld = GetActorLocation() + YawOnly.RotateVector(TargetLocal);
				FHitResult Hit;
				FCollisionQueryParams Q(SCENE_QUERY_STAT(FirstPersonEye), false, this);
				if (W->SweepSingleByChannel(Hit, AnchorWorld, WantWorld, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(FirstPersonEyeProbeRadiusCm), Q))
				{
					TargetLocal = YawOnly.UnrotateVector(Hit.Location - GetActorLocation());
				}
			}

			// Eased in the ACTOR's frame, so only pose-relative motion is damped; the body's own
			// translation is applied whole and the camera never trails behind a running character.
			if (!bCurrentEyeLocalValid) { CurrentEyeLocal = TargetLocal; bCurrentEyeLocalValid = true; }
			CurrentEyeLocal = FMath::VInterpTo(CurrentEyeLocal, TargetLocal, DeltaSeconds, FirstPersonEyeInterpSpeed);
			BoomWorldLoc = GetActorLocation() + YawOnly.RotateVector(CurrentEyeLocal);
			CurrentCameraBoomWorldZ = BoomWorldLoc.Z;   // keep the third-person tracker in step for the way back out
		}
		else
		{
			bCurrentEyeLocalValid = false;
		}
		CameraBoom->SetWorldLocation(BoomWorldLoc);
	}

	// Look limits. The engine lets the player pitch to within a tenth of a degree of straight
	// down, which in first person means looking through the top of his own chest however well
	// the eye is placed. Shooters clamp short of vertical instead; that clamp applies only in
	// first person, and the engine's own limits come back when the camera pulls out. Groggy has
	// its own much tighter clamp and keeps priority while it runs.
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (APlayerCameraManager* Cam = PC->PlayerCameraManager)
		{
			if (!bDefaultViewPitchCaptured)
			{
				DefaultViewPitchMin = Cam->ViewPitchMin; DefaultViewPitchMax = Cam->ViewPitchMax;
				bDefaultViewPitchCaptured = true;
			}
			if (!bGroggy)
			{
				Cam->ViewPitchMin = bInFirstPerson ? FirstPersonPitchMin : DefaultViewPitchMin;
				Cam->ViewPitchMax = bInFirstPerson ? FirstPersonPitchMax : DefaultViewPitchMax;
			}
		}
	}

	// Shoulder framing (third person) / forward eye push (first person) --
	// see ShoulderOffsetY and FirstPersonEyeForwardOffsetCm's header comments.
	// Both eased with FInterpTo (reusing ZoomInterpSpeed) so crossing the
	// first/third-person boundary while zooming is a smooth slide, not a pop.
	{
		const float ShoulderNow = CVarShoulderOffset.GetValueOnGameThread() >= 0.0f ? CVarShoulderOffset.GetValueOnGameThread() : ShoulderOffsetY;
		const float TargetSocketOffsetY = bInFirstPerson ? 0.0f : (bShoulderLeft ? -ShoulderNow : ShoulderNow);
		FVector BoomSocketOffset = CameraBoom->SocketOffset;
		BoomSocketOffset.Y = FMath::FInterpTo(BoomSocketOffset.Y, TargetSocketOffsetY, DeltaSeconds, ZoomInterpSpeed);
		BoomSocketOffset.Z = FMath::FInterpTo(BoomSocketOffset.Z, bInFirstPerson ? 0.0f : ShoulderLiftZ, DeltaSeconds, ZoomInterpSpeed);
		CameraBoom->SocketOffset = BoomSocketOffset;

		// The camera sits on the boom pivot in both modes now: in first person the boom itself
		// is already placed at the eye (see above), so a further push off it would only shove
		// the view out of the head again.
		FVector CameraRelativeLoc = FollowCamera->GetRelativeLocation();
		CameraRelativeLoc = FMath::VInterpTo(CameraRelativeLoc, FVector::ZeroVector, DeltaSeconds, ZoomInterpSpeed);
		if (bGroggy)
		{
			// The unsteady head: a slow drift standing still, a heavier lurch and bob when moving.
			GroggyTime += DeltaSeconds;
			const float Walk = FMath::Clamp(GetVelocity().Size2D() / FMath::Max(1.0f, GroggySpeed), 0.0f, 1.0f);
			const float Roll = 2.2f * FMath::Sin(GroggyTime * 1.3f) + 1.8f * Walk * FMath::Sin(GroggyTime * 3.1f);
			const float Pitch = 0.8f * FMath::Sin(GroggyTime * 0.9f + 1.0f) + 1.0f * Walk * FMath::Sin(GroggyTime * 6.2f);
			FollowCamera->SetRelativeRotation(FRotator(Pitch, 0.0f, Roll));
			CameraRelativeLoc.Z = 2.5f * Walk * FMath::Sin(GroggyTime * 6.2f) + 1.0f * FMath::Sin(GroggyTime * 1.7f);
		}
		FollowCamera->SetRelativeLocation(CameraRelativeLoc);
	}

	// THE EYE FOR THIS FRAME. The boom is where the camera will be read from at the end of the
	// frame, the control rotation is final (the controller has ticked), so the camera can be
	// predicted here to the centimetre and the weapon and hands placed against it. Reading the
	// camera manager here instead gives LAST frame's camera -- the judder.
	TickWeaponSway(DeltaSeconds);
	TickAimSway(DeltaSeconds);
	TickLimp(DeltaSeconds);
	TickGoodSpots(DeltaSeconds);
	PredictEye();
	TickSightAlignment(DeltaSeconds);
	TickHandIK(DeltaSeconds);
	TickHeadlamp();
	if (LagTestLeft > 0.0f)
	{
		// A turn that reverses, a nod, and a walk: the motions that showed the judder.
		LagTestLeft -= DeltaSeconds; LagTestClock += DeltaSeconds;
		AddControllerYawInput(0.6f * FMath::Sin(LagTestClock * 2.5f));
		AddControllerPitchInput(0.25f * FMath::Sin(LagTestClock * 1.7f));
		AddMovementInput(GetActorForwardVector(), 1.0f);
		if (LagTestLeft <= 0.0f) { FinishWeaponLagTest(); }
	}

	if (MuzzleFlashLeft > 0.0f)
	{
		MuzzleFlashLeft -= DeltaSeconds;
		if (MuzzleFlashLeft <= 0.0f && MuzzleFlash) { MuzzleFlash->SetVisibility(false); }
	}
	TickFootsteps(DeltaSeconds);

	// See FallGravityScale's header comment -- makes the fall snappier than
	// a symmetric parabola instead of reading as floaty/indecisive at the
	// apex. Reset to 1.0 whenever not actively falling downward so rises
	// (and ground movement, where GravityScale is largely irrelevant
	// anyway) are completely unaffected.
	GetCharacterMovement()->GravityScale = (GetCharacterMovement()->IsFalling() && GetVelocity().Z < 0.0f)
		? FallGravityScale
		: 1.0f;

	// Coyote time / jump buffering bookkeeping -- see the header comments on
	// CoyoteTimeSeconds/JumpBufferSeconds for what these implement.
	{
		const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		const bool bGroundedNow = GetCharacterMovement()->IsMovingOnGround();
		if (bGroundedNow)
		{
			TimeLastGrounded = Now;
			bCoyoteJumpUsed = false;

			// Landed (or was already grounded) with a recent-enough jump
			// press still pending -- honor it now rather than dropping it.
			if (bJumpInputBuffered && (Now - TimeJumpBuffered) <= JumpBufferSeconds)
			{
				bJumpInputBuffered = false;
				Jump();
			}
		}

		if (bIsRolling)
		{
			UpdateRollMovement();
		}

		// Transition clips steering the capsule (start turns, turn in place,
		// the sprint slide): apply what the anim proxy banked last frame, and
		// keep orient-to-movement out of the way while a clip owns the yaw.
		if (UCharacterAnimInstance* AnimInst = GetCharacterAnimInstance())
		{
			float YawDelta = 0.0f;
			FVector MeshTranslation = FVector::ZeroVector;
			AnimInst->ConsumeRootMotion(YawDelta, MeshTranslation);
			if (YawDelta != 0.0f) { AddActorWorldRotation(FRotator(0.0f, YawDelta, 0.0f)); }
			if (!MeshTranslation.IsNearlyZero()) { AddActorWorldOffset(GetMesh()->GetComponentQuat().RotateVector(MeshTranslation), /*bSweep=*/true); }

			const bool bHold = AnimInst->IsClipDrivingRotation();
			if (bHold != bAnimRotationHold)
			{
				bAnimRotationHold = bHold;
				if (!bIsRolling && !bIsDashing && !bIsSliding) { GetCharacterMovement()->bOrientRotationToMovement = bHold ? false : !ShouldFaceAim(); }
			}
		}

		if (bIsSliding && (Now - TimeSlideStarted) >= SlideDurationSeconds)
		{
			bIsSliding = false;
			GetCharacterMovement()->bOrientRotationToMovement = !ShouldFaceAim();
			bIsCrouchToggled = true;
			Crouch();
		}

		if (bIsRolling && (Now - TimeLastRoll) >= RollDurationSeconds)
		{
			bIsRolling = false;

			// Restores whatever StartRoll turned off -- see
			// RootMotionMoveMeshRotation's header comment. Respects
			// bInFirstPerson rather than hardcoding true, matching the same
			// first/third-person distinction Tick() already applies this
			// every time the zoom boundary is crossed.
			GetCharacterMovement()->bOrientRotationToMovement = !ShouldFaceAim();

			// Roll is now only ever triggered by crouching while running
			// (see StartCrouch), and the request was specifically "roll,
			// end up crouched" -- so land the crouch here rather than
			// falling back through to standing locomotion.
			bIsCrouchToggled = true;
			Crouch();
		}

		if (bIsDashing)
		{
			UpdateDashMovement();
		}

		if (bIsDashing && (Now - TimeLastDash) >= DashDurationSeconds)
		{
			bIsDashing = false;
			GetCharacterMovement()->bOrientRotationToMovement = !ShouldFaceAim();
		}

		// A one-shot combat step ends when its clip has run its length; a held
		// step (block loop, stun, death pose...) waits for its release call.
		// AdvanceCombat plays the sequence's next step or clears the flag.
		if (bIsAttacking && !bCombatHolding && (Now - TimeLastAttack) >= AttackDurationSeconds)
		{
			AdvanceCombat();
		}

		// Kept in place deliberately alongside [RollStart] above -- see that
		// log's comment. This is the trace that actually settled the "does
		// the roll really teleport" question: position/rotation/velocity
		// through the roll and for ~1.5s after it ends, which showed a
		// perfectly smooth deceleration to a stable resting point with zero
		// drift afterward -- i.e. the capsule itself never moves wrong, so
		// any future "wrong position" report is a pose/camera/blend question,
		// not a movement one, and this log is what proves that quickly. Also
		// tracks the head bone's world location relative to the camera
		// specifically -- the mesh-vs-capsule check already ruled out the
		// mesh ORIGIN diverging from the capsule, but the head bone can still
		// move relative to both (it's animated, not rigid), and this is the
		// most direct way to confirm whether the camera itself is failing to
		// track rather than the pose/animation being the whole story.
		if (Now - TimeLastRoll < 2.5f)
		{
			const FVector HeadLoc = GetMesh()->GetBoneLocation(TEXT("head"), EBoneSpaces::WorldSpace);
			const FVector CamLoc = FollowCamera->GetComponentLocation();
			UE_LOG(LogTemp, Warning, TEXT("[RollTick] t=%.3f bIsRolling=%d Loc=%s Rot=%s Vel=%s HeadLoc=%s CamLoc=%s HeadRelToCam=%s"),
				Now, bIsRolling ? 1 : 0, *GetActorLocation().ToString(), *GetActorRotation().ToString(), *GetVelocity().ToString(),
				*HeadLoc.ToString(), *CamLoc.ToString(), *(HeadLoc - CamLoc).ToString());
		}

		// Same purpose as [RollTick] above -- kept for the same reason (see
		// that log's comment), added specifically to diagnose the "oval
		// orbit" dash path live if the RootMotionMoveMeshRotation fix above
		// doesn't fully resolve it.
		if (Now - TimeLastDash < 2.5f)
		{
			UE_LOG(LogTemp, Warning, TEXT("[DashTick] t=%.3f bIsDashing=%d Direction=%d Loc=%s Rot=%s Vel=%s"),
				Now, bIsDashing ? 1 : 0, static_cast<int32>(CurrentDashDirection),
				*GetActorLocation().ToString(), *GetActorRotation().ToString(), *GetVelocity().ToString());
		}
	}

	// See SyntyMeshZOffset's header comment -- applied as a one-time delta
	// exactly when ECharacterLocomotionSet actually changes, not every
	// frame, so it composes correctly with OnStartCrouch/OnEndCrouch's own
	// independent writes to the same relative Z instead of fighting them.
	if (UCharacterAnimInstance* AnimInstForOffset = Cast<UCharacterAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		const bool bShouldApplySyntyOffset = (AnimInstForOffset->GetCurrentLocomotionSet() == ECharacterLocomotionSet::Synty);
		if (bShouldApplySyntyOffset != bSyntyMeshOffsetApplied)
		{
			const float Delta = bShouldApplySyntyOffset ? SyntyMeshZOffset : -SyntyMeshZOffset;
			GetMesh()->GetRelativeLocation_DirectMutable().Z += Delta;
			bSyntyMeshOffsetApplied = bShouldApplySyntyOffset;
		}
	}

	UpdateAnimationInstance();
	UpdateReticleLookAtTarget();
	UpdateAmbientLookAt(DeltaSeconds);

	// Done once, on the first tick (after auto-possession has resolved, which
	// it hasn't yet in BeginPlay): every character that isn't the player is
	// tagged "npc". Baked into CurrentConfig so it saves with the character.
	if (!bTagsInitialized)
	{
		bTagsInitialized = true;
		if (!IsPlayerControlled() && !CurrentConfig.Tags.Contains(TEXT("npc")))
		{
			CurrentConfig.Tags.Add(TEXT("npc"));
			SyncActorTagsFromConfig();
		}
	}
}

void ABaseCharacter::ApplyFirstPersonHeadHiding(bool bFirstPerson)
{
	// Modular characters carry the head/face/hair as separate part
	// components; these are the slots that make up the head region.
	static const TSet<FString> HeadParts = {
		TEXT("Head"), TEXT("Eyebrows"), TEXT("FacialHair"), TEXT("Hair"), TEXT("Ears"),
		TEXT("HeadCoverBaseHair"), TEXT("HeadCoverNoHair"), TEXT("HeadCoverNoFacialHair"), TEXT("Helmet"),
		TEXT("CutHead") };   // the sci-fi player's own head part (the cut library's slot name)

	// The body itself stays visible to the owner (arms, hands, torso, legs).
	GetMesh()->SetOwnerNoSee(false);
	for (const TPair<FString, TObjectPtr<USkeletalMeshComponent>>& Pair : PartComponents)
	{
		if (Pair.Value)
		{
			Pair.Value->SetOwnerNoSee(bFirstPerson && HeadParts.Contains(Pair.Key));
		}
	}

	// A single-mesh character has the head baked into the one body mesh, so
	// there's no per-owner way to hide just it -- hide the head bone (for all
	// viewers) while in first person and restore it on the way out. Every rig
	// in this project names it "head".
	if (!bModularMode)
	{
		if (bFirstPerson) { GetMesh()->HideBoneByName(TEXT("head"), EPhysBodyOp::PBO_None); }
		else { GetMesh()->UnHideBoneByName(TEXT("head")); }
	}

	// FaceController's nose/hair/facial-hair/head-gear are a SEPARATE actor
	// attached to this mesh (the nose especially sits right at eye level), so
	// it needs hiding independently of the body mesh.
	TArray<AActor*> AttachedActors;
	GetAttachedActors(AttachedActors);
	for (AActor* AttachedActor : AttachedActors)
	{
		if (AFaceController* AttachedFace = Cast<AFaceController>(AttachedActor))
		{
			AttachedFace->SetHiddenFromOwner(bFirstPerson);
		}
	}
}

void ABaseCharacter::UpdateAmbientLookAt(float DeltaSeconds)
{
	// The possessed character is driven by the reticle (above); everyone
	// else glances around the camp on their own.
	if (!bAmbientLookAt || IsPlayerControlled() || CurrentConfig.GazeTargets.Num() > 0)
	{
		AmbientLookAtActor = nullptr;
		AmbientLookAtTimer = 0.0f;
		return;
	}

	AmbientLookAtTimer -= DeltaSeconds;
	if (AmbientLookAtTimer <= 0.0f)
	{
		AActor* Player = GetPlayerLookAtCandidate();
		const float HoldMax = FMath::Max(AmbientLookAtHoldMin, AmbientLookAtHoldMax);

		if (!bAmbientLookAtResting)
		{
			// End every look with a brief rest so the head eases back to the
			// idle animation before turning to the next target -- it never
			// snaps straight from one to another. Rests are short while the
			// player is nearby so the player still gets most of the attention,
			// and longer (about half the time) otherwise.
			bAmbientLookAtResting = true;
			AmbientLookAtActor = nullptr;
			AmbientLookAtTimer = Player ? FMath::FRandRange(0.4f, 0.9f)
			                            : FMath::FRandRange(AmbientLookAtHoldMin, HoldMax) * 0.7f;
		}
		else
		{
			bAmbientLookAtResting = false;
			if (Player && FMath::FRand() < AmbientLookAtPlayerBias)
			{
				// Player in front and close: look right at them, and hold longer.
				AmbientLookAtActor = Player;
				AmbientLookAtTimer = FMath::FRandRange(HoldMax * 0.8f, HoldMax * 1.4f);
			}
			else if (AActor* Prop = PickAmbientLookAtTarget())
			{
				AmbientLookAtActor = Prop;
				AmbientLookAtTimer = FMath::FRandRange(AmbientLookAtHoldMin, HoldMax);
			}
			else if (Player)
			{
				AmbientLookAtActor = Player;
				AmbientLookAtTimer = FMath::FRandRange(HoldMax * 0.8f, HoldMax * 1.4f);
			}
			else
			{
				// Nothing worth looking at -- keep resting.
				bAmbientLookAtResting = true;
				AmbientLookAtActor = nullptr;
				AmbientLookAtTimer = FMath::FRandRange(1.0f, 2.0f);
			}
		}
	}

	// Re-aimed every tick so a target that moves (another character) is
	// tracked, and the proxy's range check can release it if it wanders out
	// of the neck's reach; the weight ramp (LookAtBlendSpeed) does the easing.
	if (const AActor* Target = AmbientLookAtActor.Get())
	{
		SetLookAtTarget(GetAmbientLookAtPoint(Target));
	}
	else
	{
		ClearLookAtTarget();
	}
}

AActor* ABaseCharacter::PickAmbientLookAtTarget() const
{
	UWorld* World = GetWorld();
	if (!World) { return nullptr; }

	const FVector Eye = GetActorLocation();
	const FVector Forward = GetActorForwardVector();
	const float FeetZ = Eye.Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float CosCone = FMath::Cos(FMath::DegreesToRadians(AmbientLookAtConeDegrees));
	const float RadiusSq = FMath::Square(AmbientLookAtRadius);

	TArray<AActor*> Candidates;
	TArray<AActor*> Attached;
	GetAttachedActors(Attached, true, true);
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor == this || Actor == AmbientLookAtActor.Get() || Attached.Contains(Actor)) { continue; }

		FVector Point;
		if (const ABaseCharacter* Other = Cast<ABaseCharacter>(Actor))
		{
			Point = Other->GetActorLocation();
		}
		else if (Actor->IsA<AStaticMeshActor>())
		{
			FVector Origin, Extent;
			Actor->GetActorBounds(false, Origin, Extent);
			if (Extent.GetMax() > AmbientLookAtMaxPropExtent) { continue; }   // walls, floors, carts
			if (Origin.Z < FeetZ - 30.0f || Origin.Z > FeetZ + 230.0f) { continue; }   // overhead lines/banners
			Point = Origin;
		}
		else
		{
			continue;
		}

		const FVector To = Point - Eye;
		if (To.SizeSquared() > RadiusSq || To.SizeSquared() < 1.0f) { continue; }
		if (FVector::DotProduct(To.GetSafeNormal2D(), Forward) < CosCone) { continue; }
		Candidates.Add(Actor);
	}
	return Candidates.Num() > 0 ? Candidates[FMath::RandRange(0, Candidates.Num() - 1)] : nullptr;
}

AActor* ABaseCharacter::GetPlayerLookAtCandidate() const
{
	// Only a real controlled character counts -- not the free-fly spectator
	// pawn used when nothing is possessed.
	ABaseCharacter* Player = Cast<ABaseCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Player || Player == this) { return nullptr; }

	const FVector To = Player->GetActorLocation() - GetActorLocation();
	if (To.SizeSquared() > FMath::Square(AmbientLookAtPlayerRadius)) { return nullptr; }
	const float CosCone = FMath::Cos(FMath::DegreesToRadians(AmbientLookAtConeDegrees));
	if (FVector::DotProduct(To.GetSafeNormal2D(), GetActorForwardVector()) < CosCone) { return nullptr; }
	return Player;
}

FVector ABaseCharacter::GetAmbientLookAtPoint(const AActor* Target) const
{
	if (const ABaseCharacter* Other = Cast<ABaseCharacter>(Target))
	{
		return Other->GetMesh()->GetSocketLocation(TEXT("head"));
	}
	FVector Origin, Extent;
	Target->GetActorBounds(false, Origin, Extent);
	return Origin;
}

void ABaseCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	// ACharacter::OnStartCrouch (the Super call below) ALSO repositions the
	// mesh -- but it does so relative to this class's DEFAULT OBJECT's mesh
	// offset, not this specific instance's actual offset. This project sets
	// the Knight mesh's relative location per-instance via Python (the same
	// pattern used for every other per-instance asset reference here, per
	// the class comment at the top of the header), never in the C++
	// constructor, so the CDO's mesh Z is 0 -- meaning Super's own "fix"
	// silently discards the real standing offset and replaces it with a
	// small near-zero value, and this function's own compensation used to
	// stack an extra shift on top of that wrong result. Net effect: the
	// mesh ended up roughly (2 * ScaledHalfHeightAdjust) too high, i.e. the
	// character visibly floats above the ground while crouched. Fix:
	// capture the real pre-crouch Z first, let Super do its (wrong) thing
	// for its other side effects (RecalculateBaseEyeHeight, K2 events), then
	// explicitly recompute Z from the captured baseline instead of trusting
	// what Super left behind.
	const float StandingMeshZ = GetMesh()->GetRelativeLocation().Z;
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

	// Deliberately mirrors how ACharacter::OnStartCrouch itself (the Super
	// call above) writes this value, via GetRelativeLocation_DirectMutable()
	// rather than SetRelativeLocation() -- confirmed by logging that the
	// public setter's write was NOT reflected by an immediate readback in
	// this call path (something about crouch happening mid-movement-update
	// makes SetRelativeLocation's usual UpdateComponentToWorld path not
	// stick here), while the direct-mutable member write Super uses does.
	GetMesh()->GetRelativeLocation_DirectMutable().Z = StandingMeshZ + ScaledHalfHeightAdjust;

	// Sets the crouched-movement speed from whatever bSprintHeld already is
	// -- covers starting a crouch with Shift already held, not just
	// pressing/releasing Shift while already crouched (StartSprint/
	// StopSprint call this too, for that case).
	UpdateCrouchWalkSpeed();

	// No more synchronous AnimInstance mode-switch needed here -- there's
	// only ever one AnimInstance now (UCharacterAnimInstance), and
	// UpdateAnimationInstance() already runs every Tick() reporting
	// bIsCrouched, which its own internal state machine reacts to on the
	// very next tick regardless. The old version of this comment described
	// a real race (AnimBP vs. crossfade instance, no guaranteed tick order
	// relative to CharacterMovementComponent's own update) that simply
	// doesn't exist anymore with a single persistent instance.
}

void ABaseCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	// Mirror of OnStartCrouch above -- same Super-clobbers-the-real-offset
	// issue applies on the way back up, and same DirectMutable requirement.
	const float CrouchedMeshZ = GetMesh()->GetRelativeLocation().Z;
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

	GetMesh()->GetRelativeLocation_DirectMutable().Z = CrouchedMeshZ - ScaledHalfHeightAdjust;
}

void ABaseCharacter::UpdateAnimationInstance()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	UCharacterAnimInstance* AnimInst = Cast<UCharacterAnimInstance>(GetMesh()->GetAnimInstance());
	if (!Movement || !AnimInst) { return; }

	// Everything the locomotion state machine needs this frame, snapshotted
	// into one struct (see FLocomotionInputs) -- tier speeds included, so
	// the anim side picks Walk/Jog/Run by the same numbers that drive
	// movement instead of keeping its own thresholds.
	const float DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;
	AirTimeSeconds = Movement->IsFalling() ? AirTimeSeconds + DeltaSeconds : 0.0f;

	// Direction of travel in the ACTOR's frame (not the mesh's, which
	// carries this project's fixed -90 yaw) -- what FDirectionalAnimSet::
	// Pick selects strafes from. Zero when not moving so idle doesn't
	// inherit a stale angle.
	const FVector Velocity = GetVelocity();
	const FVector LocalVelocity = GetActorTransform().InverseTransformVectorNoScale(Velocity);
	const float MoveAngleDeg = (Velocity.SizeSquared2D() > 1.0f)
		? FMath::RadiansToDegrees(FMath::Atan2(LocalVelocity.Y, LocalVelocity.X))
		: 0.0f;

	FLocomotionInputs In;
	In.Speed = Velocity.Size2D();
	In.bIsInAir = Movement->IsFalling();
	In.VelocityZ = Velocity.Z;
	In.bIsCrouched = bIsCrouched;
	In.MaxSpeed = bIsCrouched ? Movement->MaxWalkSpeedCrouched : Movement->MaxWalkSpeed;
	// Tier speeds carry this character's multiplier so tier selection stays
	// aligned with the caps UpdateStandingSpeed actually set.
	const float Mult = CurrentConfig.SpeedMultiplier;
	In.WalkSpeed = WalkSpeed * Mult;
	In.JogSpeed = JogSpeed * Mult;
	In.RunSpeed = RunSpeed * Mult;
	In.CrouchSlowSpeed = CrouchWalkSlowSpeed * Mult;
	In.CrouchFastSpeed = CrouchWalkFastSpeed * Mult;
	In.MoveAngleDeg = MoveAngleDeg;
	In.AirTime = AirTimeSeconds;

	// Intended direction (acceleration = the movement input this frame),
	// for the start/stop transitions -- see FLocomotionInputs.
	const FVector Accel = Movement->GetCurrentAcceleration();
	In.bHasMoveInput = Accel.SizeSquared2D() > 1.0f;
	if (In.bHasMoveInput)
	{
		const FVector LocalAccel = GetActorTransform().InverseTransformVectorNoScale(Accel);
		In.InputAngleDeg = FMath::RadiansToDegrees(FMath::Atan2(LocalAccel.Y, LocalAccel.X));
	}

	// Floor grade along the travel direction (+ uphill): from the floor
	// normal the movement component already found under the capsule.
	if (Movement->CurrentFloor.IsWalkableFloor() && Velocity.SizeSquared2D() > 1.0f)
	{
		const FVector Normal = Movement->CurrentFloor.HitResult.ImpactNormal;
		const FVector Dir = Velocity.GetSafeNormal2D();
		// A normal leaning back toward the mover means the surface rises ahead.
		In.SlopeDeg = FMath::RadiansToDegrees(FMath::Atan2(-FVector::DotProduct(Normal, Dir), FMath::Max(Normal.Z, 0.01f)));
	}

	// Mesh scale along the travel direction. The mesh sits at yaw -90 under
	// the actor, so its local Y is the actor's forward and local X its
	// side; blend between them by how far off forward the travel is.
	const FVector MeshScale = GetMesh()->GetRelativeScale3D();
	const float SideFraction = FMath::Abs(FMath::Sin(FMath::DegreesToRadians(MoveAngleDeg)));
	In.StrideScale = FMath::Lerp(static_cast<float>(MeshScale.Y), static_cast<float>(MeshScale.X), SideFraction);
	AnimInst->UpdateLocomotion(In);
}

void ABaseCharacter::SetLookAtTarget(const FVector& WorldPoint)
{
	if (UCharacterAnimInstance* AnimInst = Cast<UCharacterAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		AnimInst->SetLookAtTarget(WorldPoint);
	}
}

void ABaseCharacter::ClearLookAtTarget()
{
	if (UCharacterAnimInstance* AnimInst = Cast<UCharacterAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		AnimInst->ClearLookAtTarget();
	}
}

void ABaseCharacter::UpdateReticleLookAtTarget()
{
	// Aims the head at whatever the reticle (screen-center dot, drawn by
	// ABaseHUD) is currently pointing at -- the first real user of
	// SetLookAtTarget. Traces from the ACTIVE camera (PlayerCameraManager,
	// not just FollowCamera -- correct in both third- and first-person zoom
	// levels) straight down its own forward vector, which is exactly what
	// the centered reticle represents on screen.
	// Unpossessed characters are aimed by UpdateAmbientLookAt instead.
	if (!IsPlayerControlled()) { return; }

	if (!bEnableReticleLookAt)
	{
		ClearLookAtTarget();
		return;
	}

	// NEVER in first person. The first-person eye rides the head bone (see FirstPersonHeadBone),
	// and this aims the head bone at whatever the camera is pointing at -- so the camera moves
	// the head, the head moves the camera, and the two chase each other every frame. That is
	// the stutter: it is not the mouse, it is a feedback loop, and it gets worse the closer the
	// trace lands because the target distance then changes with every small movement.
	//
	// Nothing is lost by switching it off here. The head is hidden from its owner in first
	// person anyway, and the body already turns to face the aim whenever a weapon is out
	// (see ShouldFaceAim), so the character is pointed the right way without the head's help.
	if (bInFirstPerson)
	{
		ClearLookAtTarget();
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->PlayerCameraManager)
	{
		return;
	}

	const FVector CamLoc = PC->PlayerCameraManager->GetCameraLocation();
	const FVector CamForward = PC->PlayerCameraManager->GetCameraRotation().Vector();
	const FVector TraceEnd = CamLoc + CamForward * ReticleLookAtTraceDistance;

	FHitResult Hit;
	FCollisionQueryParams Params(TEXT("ReticleLookAt"), false, this);

	// FCollisionQueryParams(..., this) above already ignores THIS actor, but
	// FaceController (the nose/hair/highlight cosmetics) is a SEPARATE actor
	// attached to this one's mesh, not covered by that -- without this, the
	// centered camera's forward ray could self-hit FaceController's own
	// geometry a few cm away, feeding a near-degenerate look-at target back
	// into the head bone. Recursive so it also covers anything FaceController
	// itself might have attached.
	TArray<AActor*> ActorsToIgnore;
	GetAttachedActors(ActorsToIgnore, /*bResetArray=*/true, /*bRecursivelyIncludeAttachedActors=*/true);
	Params.AddIgnoredActors(ActorsToIgnore);

	const FVector TargetPoint = GetWorld()->LineTraceSingleByChannel(Hit, CamLoc, TraceEnd, ECC_Visibility, Params)
		? Hit.Location
		: TraceEnd; // nothing hit -- keep the head aimed along the ray anyway rather than snapping to idle.

	SetLookAtTarget(TargetPoint);
}

void ABaseCharacter::ApplySkinTint()
{
	if (!GetMesh()) { return; }
	// Every mesh that shows skin: the single body, or each modular part.
	TArray<USkeletalMeshComponent*> Comps;
	if (bModularMode) { for (const TPair<FString, TObjectPtr<USkeletalMeshComponent>>& Pair : PartComponents) { if (Pair.Value && Pair.Value->GetSkeletalMeshAsset()) { Comps.Add(Pair.Value); } } }
	else { Comps.Add(GetMesh()); }
	if (CurrentConfig.SkinTint.A <= 0.0f)
	{
		// Tint off: put the assets' own materials back.
		for (USkeletalMeshComponent* C : Comps)
		{
			if (const USkeletalMesh* Asset = C->GetSkeletalMeshAsset())
			{
				const TArray<FSkeletalMaterial>& Mats = Asset->GetMaterials();
				for (int32 i = 0; i < Mats.Num(); ++i) { C->SetMaterial(i, Mats[i].MaterialInterface); }
			}
		}
		return;
	}
	UMaterialInterface* Tint = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/RepliCan/Materials/M_CharacterTint.M_CharacterTint"));
	if (!Tint) { UE_LOG(LogTemp, Warning, TEXT("SkinTint: M_CharacterTint missing")); return; }
	for (USkeletalMeshComponent* C : Comps)
	{
		const USkeletalMesh* Asset = C->GetSkeletalMeshAsset();
		if (!Asset || Asset->GetMaterials().Num() == 0) { continue; }
		// A hero-pack head (the Fantasy Hero base skull) carries its own parametric material:
		// the skin tint goes straight into Color_Skin, and the stubble slider fades the pack's
		// scalp/jaw shadow (Color_Stubble, painted as a darker skin) toward the skin.
		if (UMaterialInterface* Base0 = Asset->GetMaterials()[0].MaterialInterface)
		{
			FLinearColor Probe;
			if (Base0->GetVectorParameterValue(FMaterialParameterInfo(TEXT("Color_Stubble")), Probe))
			{
				// Section 0 is the face (jaw stubble), section 1 the scalp (head stubble): one instance each.
				FLinearColor Skin = CurrentConfig.SkinTint; Skin.A = 1.0f;
				const FLinearColor Stubble(Skin.R * 0.61f, Skin.G * 0.75f, Skin.B * 0.84f, 1.0f);   // the pack's own stubble-to-skin ratio
				for (int32 i = 0; i < C->GetNumMaterials(); ++i)
				{
					UMaterialInstanceDynamic* HeroMID = Cast<UMaterialInstanceDynamic>(C->GetMaterial(i));
					if (!HeroMID || HeroMID->Parent != Base0) { HeroMID = UMaterialInstanceDynamic::Create(Base0, this); }
					const float Amount = FMath::Clamp(i == 1 ? CurrentConfig.HeadStubbleFade : CurrentConfig.StubbleFade, 0.0f, 1.0f);   // the amount shown
					HeroMID->SetVectorParameterValue(TEXT("Color_Skin"), Skin);
					HeroMID->SetVectorParameterValue(TEXT("Color_Stubble"), FMath::Lerp(Skin, Stubble, Amount));
					C->SetMaterial(i, HeroMID);
				}
				continue;
			}
		}
		// The atlas the part was painted with: its asset material's first texture.
		UTexture* Atlas = nullptr;
		if (UMaterialInterface* Base = Asset->GetMaterials()[0].MaterialInterface)
		{
			TArray<UTexture*> Textures;
			Base->GetUsedTextures(Textures);
			for (UTexture* T : Textures) { if (T && !T->GetName().Contains(TEXT("Emissive")) && !T->GetName().Contains(TEXT("Normal"))) { Atlas = T; break; } }
		}
		if (!Atlas) { continue; }
		// The mask that matches that atlas' layout (Worlds and CyberCity share one).
		const TCHAR* MaskPath = Atlas->GetName().Contains(TEXT("SciFiSpace")) ? TEXT("/Game/RepliCan/Textures/T_SkinMask_Space.T_SkinMask_Space") : TEXT("/Game/RepliCan/Textures/T_SkinMask_WorldsCyber.T_SkinMask_WorldsCyber");
		UTexture* Mask = LoadObject<UTexture>(nullptr, MaskPath);
		UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(C->GetMaterial(0));
		if (!MID || MID->Parent != Tint) { MID = UMaterialInstanceDynamic::Create(Tint, this); }
		MID->SetTextureParameterValue(TEXT("Atlas"), Atlas);
		if (Mask) { MID->SetTextureParameterValue(TEXT("Mask"), Mask); }
		MID->SetVectorParameterValue(TEXT("SkinTint"), CurrentConfig.SkinTint);
		MID->SetScalarParameterValue(TEXT("StubbleFade"), FMath::Clamp(CurrentConfig.StubbleFade, 0.0f, 1.0f));
		// The cryo-suit knobs (dark metal chest plugs, grimy underwear) are for the player's own body only.
		const bool bPlayerBody = CurrentConfig.Tags.Contains(TEXT("player")) || CurrentConfig.Name.StartsWith(TEXT("Player"));
		MID->SetScalarParameterValue(TEXT("PlugMetal"), bPlayerBody ? 1.0f : 0.0f);
		MID->SetScalarParameterValue(TEXT("Grime"), bPlayerBody ? 1.0f : 0.0f);
		for (int32 i = 0; i < C->GetNumMaterials(); ++i) { C->SetMaterial(i, MID); }
	}
	SkinTintMID = nullptr;
}

FString ABaseCharacter::DebugSkinTint()
{
	ApplySkinTint();
	FString Out = FString::Printf(TEXT("modular=%d parts=%d tint=%s"), bModularMode ? 1 : 0, PartComponents.Num(), *CurrentConfig.SkinTint.ToString());
	for (const TPair<FString, TObjectPtr<USkeletalMeshComponent>>& Pair : PartComponents)
	{
		USkeletalMeshComponent* C = Pair.Value; if (!C) { continue; }
		const USkeletalMesh* Asset = C->GetSkeletalMeshAsset();
		UMaterialInterface* Base = (Asset && Asset->GetMaterials().Num()) ? Asset->GetMaterials()[0].MaterialInterface : nullptr;
		TArray<UTexture*> Textures; if (Base) { Base->GetUsedTextures(Textures); }
		UMaterialInterface* Now = C->GetMaterial(0);
		Out += FString::Printf(TEXT(" | %s: asset=%s base=%s textures=%d now=%s(%s)"), *Pair.Key, Asset ? *Asset->GetName() : TEXT("-"), Base ? *Base->GetName() : TEXT("-"), Textures.Num(), Now ? *Now->GetName() : TEXT("-"), Now ? *Now->GetClass()->GetName() : TEXT("-"));
	}
	return Out;
}

void ABaseCharacter::SetSkinTint(const FLinearColor& Tint, float StubbleFade, float HeadStubbleFade)
{
	CurrentConfig.SkinTint = Tint;
	CurrentConfig.StubbleFade = StubbleFade;
	if (HeadStubbleFade >= 0.0f) { CurrentConfig.HeadStubbleFade = HeadStubbleFade; }
	ApplySkinTint();
	MatchNoseToSkin();   // the nose is a flat color: re-sample it from the new tone
}

void ABaseCharacter::SetHairMeshPath(const FString& Path)
{
	CurrentConfig.Face.HairMesh = Path;
	if (FaceController) { FaceController->SetHairMesh(Path.IsEmpty() ? nullptr : LoadObject<UStaticMesh>(nullptr, *Path)); }
}

void ABaseCharacter::SetFacialHairMeshPath(const FString& Path)
{
	CurrentConfig.Face.FacialHairMesh = Path;
	if (FaceController) { FaceController->SetFacialHairMesh(Path.IsEmpty() ? nullptr : LoadObject<UStaticMesh>(nullptr, *Path)); }
}

void ABaseCharacter::SetRemoteCamera(const FRemoteCameraConfig& Camera)
{
	CurrentConfig.RemoteCamera = Camera;
}
