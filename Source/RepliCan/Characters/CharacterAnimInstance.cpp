#include "CharacterAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Animation/SkeletonRemapping.h"
#include "Animation/SkeletonRemappingRegistry.h"
#include "AnimationRuntime.h"
#include "Components/SkeletalMeshComponent.h"
#include "BonePose.h"
#include "UObject/ConstructorHelpers.h"
#include "Misc/Paths.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"

namespace
{
	// Sourced from the same free Lyra Mannequin animation pack (see
	// C:\Dev\Assets\Lyra) via the Blender bone-strip pipeline this project's
	// other locomotion clips already use, targeting the shared
	// UE4_Mannequin_Skeleton.
	constexpr const TCHAR* IdleAnimPath = TEXT("/Game/PolygonGoblinWarCamp/EpicContent/Mannequin/Animations/MM_Unarmed_Idle_Ready.MM_Unarmed_Idle_Ready");
	constexpr const TCHAR* WalkAnimPath = TEXT("/Game/PolygonGoblinWarCamp/EpicContent/Mannequin/Animations/MM_Unarmed_Walk_Fwd.MM_Unarmed_Walk_Fwd");
	constexpr const TCHAR* RunAnimPath = TEXT("/Game/PolygonGoblinWarCamp/EpicContent/Mannequin/Animations/MM_Unarmed_Jog_Fwd.MM_Unarmed_Jog_Fwd");
	constexpr const TCHAR* JumpRiseAnimPath = TEXT("/Game/PolygonGoblinWarCamp/EpicContent/Mannequin/Animations/MM_Unarmed_Jump_Start_Loop.MM_Unarmed_Jump_Start_Loop");
	constexpr const TCHAR* JumpFallAnimPath = TEXT("/Game/PolygonGoblinWarCamp/EpicContent/Mannequin/Animations/MM_Unarmed_Jump_Fall_Loop.MM_Unarmed_Jump_Fall_Loop");
	constexpr const TCHAR* JumpEndAnimPath = TEXT("/Game/PolygonGoblinWarCamp/EpicContent/Mannequin/Animations/MM_Unarmed_Jump_Fall_Land.MM_Unarmed_Jump_Fall_Land");
	constexpr const TCHAR* CrouchIdleAnimPath = TEXT("/Game/PolygonGoblinWarCamp/EpicContent/Mannequin/Animations/MM_Unarmed_Crouch_Idle.MM_Unarmed_Crouch_Idle");
	constexpr const TCHAR* CrouchWalkAnimPath = TEXT("/Game/PolygonGoblinWarCamp/EpicContent/Mannequin/Animations/MM_Unarmed_Crouch_Walk_Fwd.MM_Unarmed_Crouch_Walk_Fwd");

	// See RollAnim's header comment for why this specific clip (not the
	// DynamicFalling pack tried first) and why it keeps its root motion.
	constexpr const TCHAR* RollAnimPath = TEXT("/Game/Characters/Animations/DashDodgeRoll/A_Roll_IdleFwd.A_Roll_IdleFwd");

	constexpr const TCHAR* IdleBreakAnimPath = TEXT("/Game/Characters/Animations/IdleVariety/MF_Unarmed_Idle_Break_clean.MF_Unarmed_Idle_Break_clean");
	constexpr const TCHAR* IdleBreakFidgetAnimPath = TEXT("/Game/Characters/Animations/IdleVariety/MM_Unarmed_IdleBreak_Fidget_clean.MM_Unarmed_IdleBreak_Fidget_clean");

	// See DashForwardAnim's header comment -- all 4 have real baked root
	// motion the same way RollAnim does.
	constexpr const TCHAR* DashForwardAnimPath = TEXT("/Game/Characters/Animations/Dash/MM_Dash_Forward_clean.MM_Dash_Forward_clean");
	constexpr const TCHAR* DashBackwardAnimPath = TEXT("/Game/Characters/Animations/Dash/MM_Dash_Backward_clean.MM_Dash_Backward_clean");
	constexpr const TCHAR* DashLeftAnimPath = TEXT("/Game/Characters/Animations/Dash/MM_Dash_Left_clean.MM_Dash_Left_clean");
	constexpr const TCHAR* DashRightAnimPath = TEXT("/Game/Characters/Animations/Dash/MM_Dash_Right_clean.MM_Dash_Right_clean");

	// The Synty Base Locomotion pack, retargeted this session from Synty's
	// proprietary skeleton onto this project's shared UE4_Mannequin_Skeleton
	// (see project notes on the PolygonSource -> Mannequin IK Retargeter).
	// Individual clip names are assembled in the constructor from this root
	// -- see LoadSyntyLocomotionSet there for the naming convention. Only
	// the non-RootMotion variants are used (the character is capsule-
	// driven); the _RootMotion siblings are what the *AnimSpeed native
	// speeds in FLocomotionAnimSet were measured from.
	constexpr const TCHAR* SyntyMascRoot = TEXT("/Game/Characters/Animations/SyntyBaseLocomotion/Masculine/");
	constexpr const TCHAR* SyntyFemnRoot = TEXT("/Game/Characters/Animations/SyntyBaseLocomotion/Feminine/");
	// The Goblin Locomotion pack (A_POLY_GBL_*_Neut), same staging retarget.
	constexpr const TCHAR* SyntyGoblinRoot = TEXT("/Game/Characters/Animations/SyntyBaseLocomotion/Goblin/");
	// Its native speeds (cm/s), measured off the _RM twins' root travel.
	constexpr float GoblinWalkSpeed = 185.7f;
	constexpr float GoblinJogSpeed = 399.8f;
	constexpr float GoblinSprintSpeed = 799.7f;
	constexpr float GoblinCrouchSpeed = 158.1f;

	// See PoseAnims's header comment -- Lyra's SplashPose_1 through _16 plus
	// SmearPoses, the real usable content in what Lyra ships as its "Poses"
	// set (the actual /Poses folder is all rig-calibration/block-out junk).
	constexpr const TCHAR* PoseAnimPaths[] = {
		TEXT("/Game/Characters/Animations/Poses/SplashPose_1_clean.SplashPose_1_clean"),
		TEXT("/Game/Characters/Animations/Poses/SplashPose_2_clean.SplashPose_2_clean"),
		TEXT("/Game/Characters/Animations/Poses/SplashPose_3_clean.SplashPose_3_clean"),
		TEXT("/Game/Characters/Animations/Poses/SplashPose_4_clean.SplashPose_4_clean"),
		TEXT("/Game/Characters/Animations/Poses/SplashPose_5_clean.SplashPose_5_clean"),
		TEXT("/Game/Characters/Animations/Poses/SplashPose_6_clean.SplashPose_6_clean"),
		TEXT("/Game/Characters/Animations/Poses/SplashPose_7_clean.SplashPose_7_clean"),
		TEXT("/Game/Characters/Animations/Poses/SplashPose_8_clean.SplashPose_8_clean"),
		TEXT("/Game/Characters/Animations/Poses/SplashPose_9_clean.SplashPose_9_clean"),
		TEXT("/Game/Characters/Animations/Poses/SplashPose_10_clean.SplashPose_10_clean"),
		TEXT("/Game/Characters/Animations/Poses/SplashPose_11_clean.SplashPose_11_clean"),
		TEXT("/Game/Characters/Animations/Poses/SplashPose_12_clean.SplashPose_12_clean"),
		TEXT("/Game/Characters/Animations/Poses/SplashPose_13_clean.SplashPose_13_clean"),
		TEXT("/Game/Characters/Animations/Poses/SplashPose_14_clean.SplashPose_14_clean"),
		TEXT("/Game/Characters/Animations/Poses/SplashPose_15_clean.SplashPose_15_clean"),
		TEXT("/Game/Characters/Animations/Poses/SplashPose_16_clean.SplashPose_16_clean"),
		TEXT("/Game/Characters/Animations/Poses/SplashPose_SmearPoses_clean.SplashPose_SmearPoses_clean"),
	};

	// The 9 sword clips bound to number keys 1-9 -- see AttackAnims's header
	// comment for why these are the plain in-place "_Sword" variants (not
	// "_RootMotion"/"_ReturnToIdle") from the retargeted Synty Sword Combat
	// pack. Each lives in its own per-move subfolder under Attack/, unlike
	// Base Locomotion's flatter layout.
	constexpr const TCHAR* AttackAnimPaths[] = {
		TEXT("/Game/Characters/Animations/SyntySwordCombat/Attack/LightCombo01/A_Attack_LightCombo01A_Sword.A_Attack_LightCombo01A_Sword"),
		TEXT("/Game/Characters/Animations/SyntySwordCombat/Attack/LightCombo01/A_Attack_LightCombo01B_Sword.A_Attack_LightCombo01B_Sword"),
		TEXT("/Game/Characters/Animations/SyntySwordCombat/Attack/LightCombo01/A_Attack_LightCombo01C_Sword.A_Attack_LightCombo01C_Sword"),
		TEXT("/Game/Characters/Animations/SyntySwordCombat/Attack/HeavyCombo01/A_Attack_HeavyCombo01A_Sword.A_Attack_HeavyCombo01A_Sword"),
		TEXT("/Game/Characters/Animations/SyntySwordCombat/Attack/HeavyCombo01/A_Attack_HeavyCombo01B_Sword.A_Attack_HeavyCombo01B_Sword"),
		TEXT("/Game/Characters/Animations/SyntySwordCombat/Attack/HeavyCombo01/A_Attack_HeavyCombo01C_Sword.A_Attack_HeavyCombo01C_Sword"),
		TEXT("/Game/Characters/Animations/SyntySwordCombat/Attack/HeavyStab01/A_Attack_HeavyStab01_Sword.A_Attack_HeavyStab01_Sword"),
		TEXT("/Game/Characters/Animations/SyntySwordCombat/Attack/HeavyFlourish01/A_Attack_HeavyFlourish01_Sword.A_Attack_HeavyFlourish01_Sword"),
		TEXT("/Game/Characters/Animations/SyntySwordCombat/Attack/LightFencing01/A_Attack_LightFencing01_Sword.A_Attack_LightFencing01_Sword"),
	};

	void AdvanceTime(float& Time, float DeltaSeconds, float PlayRate, bool bLooping, const UAnimSequence* Asset)
	{
		if (!Asset) { return; }
		Time += DeltaSeconds * PlayRate;
		const float Length = Asset->GetPlayLength();
		if (Length <= 0.0f) { return; }
		if (bLooping)
		{
			Time = FMath::Fmod(Time, Length);
			if (Time < 0.0f) { Time += Length; }
		}
		else
		{
			Time = FMath::Clamp(Time, 0.0f, Length);
		}
	}
}

UCharacterAnimInstance::UCharacterAnimInstance()
{
	// Hardcoded defaults rather than per-instance Python wiring -- these are
	// UPROPERTY(EditAnywhere) so still overridable per-instance (e.g. a
	// future NPC with different clips), but a sane default here means any
	// character using this as its AnimClass works with zero post-spawn
	// configuration.
	static ConstructorHelpers::FObjectFinder<UAnimSequence> IdleFinder(IdleAnimPath);
	static ConstructorHelpers::FObjectFinder<UAnimSequence> WalkFinder(WalkAnimPath);
	static ConstructorHelpers::FObjectFinder<UAnimSequence> RunFinder(RunAnimPath);
	static ConstructorHelpers::FObjectFinder<UAnimSequence> JumpRiseFinder(JumpRiseAnimPath);
	static ConstructorHelpers::FObjectFinder<UAnimSequence> JumpFallFinder(JumpFallAnimPath);
	static ConstructorHelpers::FObjectFinder<UAnimSequence> JumpEndFinder(JumpEndAnimPath);
	static ConstructorHelpers::FObjectFinder<UAnimSequence> CrouchIdleFinder(CrouchIdleAnimPath);
	static ConstructorHelpers::FObjectFinder<UAnimSequence> CrouchWalkFinder(CrouchWalkAnimPath);
	static ConstructorHelpers::FObjectFinder<UAnimSequence> RollFinder(RollAnimPath);
	static ConstructorHelpers::FObjectFinder<UAnimSequence> IdleBreakFinder(IdleBreakAnimPath);
	static ConstructorHelpers::FObjectFinder<UAnimSequence> IdleBreakFidgetFinder(IdleBreakFidgetAnimPath);
	static ConstructorHelpers::FObjectFinder<UAnimSequence> DashForwardFinder(DashForwardAnimPath);
	static ConstructorHelpers::FObjectFinder<UAnimSequence> DashBackwardFinder(DashBackwardAnimPath);
	static ConstructorHelpers::FObjectFinder<UAnimSequence> DashLeftFinder(DashLeftAnimPath);
	static ConstructorHelpers::FObjectFinder<UAnimSequence> DashRightFinder(DashRightAnimPath);
	IdleAnim = IdleFinder.Object;
	WalkAnim = WalkFinder.Object;
	RunAnim = RunFinder.Object;
	JumpRiseAnim = JumpRiseFinder.Object;
	JumpFallAnim = JumpFallFinder.Object;
	JumpEndAnim = JumpEndFinder.Object;
	CrouchIdleAnim = CrouchIdleFinder.Object;
	CrouchWalkAnim = CrouchWalkFinder.Object;
	RollAnim = RollFinder.Object;
	IdleBreakAnim = IdleBreakFinder.Object;
	IdleBreakFidgetAnim = IdleBreakFidgetFinder.Object;
	DashForwardAnim = DashForwardFinder.Object;
	DashBackwardAnim = DashBackwardFinder.Object;
	DashLeftAnim = DashLeftFinder.Object;
	DashRightAnim = DashRightFinder.Object;
	// Synty set. Clip naming: "<Sub>/A_<Gait>_<Dir>_Masc", where the
	// forward-facing strafes are FwdStrafe{F,FL,FR,L,R} and backpedals are
	// BckStrafe{B,BL,BR} (the pack's other Bck* / FwdStrafeBR / Up25 /
	// Down25 clips are for a back-facing rig or slopes -- not used). Walk
	// and Run have a dedicated straight A_<Gait>_F clip; Crouch doesn't, so
	// its F is FwdStrafeF. Non-static finders on purpose -- see the
	// PoseAnims loop comment below on why per-construction lookup is fine.
	// One loader for all three sets; the naming differences between the
	// Base Locomotion pack (A_<Gait>_<Dir>_<Masc|Femn>, "FwdStrafeFL",
	// "FallShort", crouch-forward = "FwdStrafeF") and the Goblin pack
	// (A_POLY_GBL_<Gait>_<Dir>_Neut, "FwdStrafe_FL", "Fall_Short", a real
	// "Crouch_F") are the only knobs.
	struct FSetNaming { const TCHAR* Root; const TCHAR* Prefix; const TCHAR* Token; const TCHAR* DirSep; const TCHAR* FallShort; const TCHAR* FallLarge; const TCHAR* CrouchForward; };
	const auto LoadSyntySet = [](FLocomotionAnimSet& Set, const FSetNaming& N)
	{
		const auto Load = [&N](const TCHAR* Sub, const TCHAR* Gait, const FString& Dir) -> UAnimSequence*
		{
			const FString Name = FString::Printf(TEXT("%s%s_%s_%s"), N.Prefix, Gait, *Dir, N.Token);
			const FString Path = FString::Printf(TEXT("%s%s/%s.%s"), N.Root, Sub, *Name, *Name);
			ConstructorHelpers::FObjectFinder<UAnimSequence> Finder(*Path);
			return Finder.Object;
		};
		const auto LoadDirectional = [&](FDirectionalAnimSet& Dirs, const TCHAR* Sub, const TCHAR* Gait, const TCHAR* ForwardDir)
		{
			const FString Fwd = FString(TEXT("FwdStrafe")) + N.DirSep;
			const FString Bck = FString(TEXT("BckStrafe")) + N.DirSep;
			Dirs.F = Load(Sub, Gait, ForwardDir);
			Dirs.FL = Load(Sub, Gait, Fwd + TEXT("FL"));
			Dirs.FR = Load(Sub, Gait, Fwd + TEXT("FR"));
			Dirs.L = Load(Sub, Gait, Fwd + TEXT("L"));
			Dirs.R = Load(Sub, Gait, Fwd + TEXT("R"));
			Dirs.B = Load(Sub, Gait, Bck + TEXT("B"));
			Dirs.BL = Load(Sub, Gait, Bck + TEXT("BL"));
			Dirs.BR = Load(Sub, Gait, Bck + TEXT("BR"));
		};
		Set.IdleAnim = Load(TEXT("Idle"), TEXT("Idle"), TEXT("Standing"));
		Set.CrouchIdleAnim = Load(TEXT("Idle"), TEXT("Idle"), TEXT("Crouching"));
		LoadDirectional(Set.Walk, TEXT("Locomotion/Walk"), TEXT("Walk"), TEXT("F"));
		LoadDirectional(Set.Jog, TEXT("Locomotion/Run"), TEXT("Run"), TEXT("F"));
		LoadDirectional(Set.Crouch, TEXT("Locomotion/Crouch"), TEXT("Crouch"), N.CrouchForward);
		Set.SprintAnim = Load(TEXT("Locomotion/Sprint"), TEXT("Sprint"), TEXT("F"));
		Set.JumpIdleAnim = Load(TEXT("InAir"), TEXT("Jump"), TEXT("Idle"));
		Set.JumpWalkingAnim = Load(TEXT("InAir"), TEXT("Jump"), TEXT("Walking"));
		Set.JumpRunningAnim = Load(TEXT("InAir"), TEXT("Jump"), TEXT("Running"));
		Set.JumpSprintingAnim = Load(TEXT("InAir"), TEXT("Jump"), TEXT("Sprinting"));
		Set.FallShortAnim = Load(TEXT("InAir"), TEXT("InAir"), N.FallShort);
		Set.FallLargeAnim = Load(TEXT("InAir"), TEXT("InAir"), N.FallLarge);
		Set.LandSoftAnim = Load(TEXT("InAir"), TEXT("Land"), TEXT("IdleSoft"));
		Set.LandMediumAnim = Load(TEXT("InAir"), TEXT("Land"), TEXT("IdleMedium"));
		Set.LandHardAnim = Load(TEXT("InAir"), TEXT("Land"), TEXT("IdleHard"));
	};
	LoadSyntySet(SyntyLocomotionSet,         { SyntyMascRoot,   TEXT("A_"),          TEXT("Masc"), TEXT(""),  TEXT("FallShort"),  TEXT("FallLarge"), TEXT("FwdStrafeF") });
	LoadSyntySet(SyntyFeminineLocomotionSet, { SyntyFemnRoot,   TEXT("A_"),          TEXT("Femn"), TEXT(""),  TEXT("FallShort"),  TEXT("FallLarge"), TEXT("FwdStrafeF") });
	LoadSyntySet(SyntyGoblinLocomotionSet,   { SyntyGoblinRoot, TEXT("A_POLY_GBL_"), TEXT("Neut"), TEXT("_"), TEXT("Fall_Short"), TEXT("Fall_Long"), TEXT("F") });

	// The goblin pack's native speeds, measured off its _RM twins' root
	// travel the same way the Masculine numbers were (see FLocomotionAnimSet).
	SyntyGoblinLocomotionSet.WalkAnimSpeed = GoblinWalkSpeed;
	SyntyGoblinLocomotionSet.JogAnimSpeed = GoblinJogSpeed;
	SyntyGoblinLocomotionSet.SprintAnimSpeed = GoblinSprintSpeed;
	SyntyGoblinLocomotionSet.CrouchAnimSpeed = GoblinCrouchSpeed;

	// Feminine and Goblin fall back to Masculine slot-by-slot, so a partial
	// pack never leaves a state without a clip.
	{
		const auto Fill = [](TObjectPtr<UAnimSequence>& Slot, const TObjectPtr<UAnimSequence>& From) { if (!Slot) { Slot = From; } };
		const auto FillDirectional = [&Fill](FDirectionalAnimSet& To, const FDirectionalAnimSet& From)
		{
			Fill(To.F, From.F); Fill(To.FL, From.FL); Fill(To.FR, From.FR); Fill(To.L, From.L);
			Fill(To.R, From.R); Fill(To.B, From.B); Fill(To.BL, From.BL); Fill(To.BR, From.BR);
		};
		const auto FillSet = [&](FLocomotionAnimSet& To, const FLocomotionAnimSet& From)
		{
			Fill(To.IdleAnim, From.IdleAnim); Fill(To.CrouchIdleAnim, From.CrouchIdleAnim);
			FillDirectional(To.Walk, From.Walk); FillDirectional(To.Jog, From.Jog); FillDirectional(To.Crouch, From.Crouch);
			Fill(To.SprintAnim, From.SprintAnim);
			Fill(To.JumpIdleAnim, From.JumpIdleAnim); Fill(To.JumpWalkingAnim, From.JumpWalkingAnim);
			Fill(To.JumpRunningAnim, From.JumpRunningAnim); Fill(To.JumpSprintingAnim, From.JumpSprintingAnim);
			Fill(To.FallShortAnim, From.FallShortAnim); Fill(To.FallLargeAnim, From.FallLargeAnim);
			Fill(To.LandSoftAnim, From.LandSoftAnim); Fill(To.LandMediumAnim, From.LandMediumAnim); Fill(To.LandHardAnim, From.LandHardAnim);
		};
		FillSet(SyntyFeminineLocomotionSet, SyntyLocomotionSet);
		FillSet(SyntyGoblinLocomotionSet, SyntyLocomotionSet);
	}

	// Transition, turn and slope clips. These are irregular enough across
	// the packs (RootMotion twins, "LFoot" vs "FL", "180L" vs "180_L") that
	// an explicit table per set beats a naming rule.
	{
		const auto LoadPath = [](const FString& Path) -> UAnimSequence*
		{
			const FString Name = FPaths::GetBaseFilename(Path);
			ConstructorHelpers::FObjectFinder<UAnimSequence> Finder(*FString::Printf(TEXT("%s.%s"), *Path, *Name));
			return Finder.Object;
		};
		const auto LoadHuman = [&LoadPath](FLocomotionAnimSet& Set, const FString& Root, const TCHAR* Tok)
		{
			const auto P = [&](const TCHAR* Sub, const TCHAR* Stem) { return LoadPath(FString::Printf(TEXT("%s%s/A_%s%s"), *Root, Sub, Stem, Tok)); };
			Set.WalkUpAnim = P(TEXT("Locomotion/Walk"), TEXT("Walk_Up25F_"));       Set.WalkDownAnim = P(TEXT("Locomotion/Walk"), TEXT("Walk_Down25F_"));
			Set.JogUpAnim = P(TEXT("Locomotion/Run"), TEXT("Run_Up25F_"));           Set.JogDownAnim = P(TEXT("Locomotion/Run"), TEXT("Run_Down25F_"));
			Set.SprintUpAnim = P(TEXT("Locomotion/Sprint"), TEXT("Sprint_Up25F_"));  Set.SprintDownAnim = P(TEXT("Locomotion/Sprint"), TEXT("Sprint_Down25F_"));
			Set.StartWalkF = P(TEXT("Transitions/Idle_ToWalk"), TEXT("Idle_ToWalkFRootMotion_"));
			Set.StartWalk90L = P(TEXT("Transitions/Idle_ToWalk"), TEXT("Idle_ToWalk90LRootMotion_"));   Set.StartWalk90R = P(TEXT("Transitions/Idle_ToWalk"), TEXT("Idle_ToWalk90RRootMotion_"));
			Set.StartWalk180L = P(TEXT("Transitions/Idle_ToWalk"), TEXT("Idle_ToWalk180LRootMotion_")); Set.StartWalk180R = P(TEXT("Transitions/Idle_ToWalk"), TEXT("Idle_ToWalk180RRootMotion_"));
			Set.StartRunF = P(TEXT("Transitions/Idle_ToRun"), TEXT("Idle_ToRunFRootMotion_"));
			Set.StartRun90L = P(TEXT("Transitions/Idle_ToRun"), TEXT("Idle_ToRun90LRootMotion_"));      Set.StartRun90R = P(TEXT("Transitions/Idle_ToRun"), TEXT("Idle_ToRun90RRootMotion_"));
			Set.StartRun180L = P(TEXT("Transitions/Idle_ToRun"), TEXT("Idle_ToRun180LRootMotion_"));    Set.StartRun180R = P(TEXT("Transitions/Idle_ToRun"), TEXT("Idle_ToRun180RRootMotion_"));
			Set.StopWalkLFoot = P(TEXT("Transitions/Walk_ToIdle"), TEXT("Walk_ToIdleF_LFoot_"));        Set.StopWalkRFoot = P(TEXT("Transitions/Walk_ToIdle"), TEXT("Walk_ToIdleF_RFoot_"));
			Set.StopRunLFoot = P(TEXT("Transitions/Run_ToIdle"), TEXT("Run_ToIdleF_LFoot_"));           Set.StopRunRFoot = P(TEXT("Transitions/Run_ToIdle"), TEXT("Run_ToIdleF_RFoot_"));
			Set.StandToCrouchAnim = P(TEXT("Transitions/Stand_ToCrouch"), TEXT("Stand_ToCrouch_"));
			Set.CrouchToStandAnim = P(TEXT("Transitions/Crouch_ToStand"), TEXT("Crouch_ToStand_"));
			Set.SprintToCrouchAnim = P(TEXT("Transitions/Sprint_ToCrouch"), TEXT("Sprint_ToCrouchRootMotion_"));
			Set.TurnStanding90L = P(TEXT("Locomotion/Turn"), TEXT("Turn_Standing_90LRootMotion_"));     Set.TurnStanding90R = P(TEXT("Locomotion/Turn"), TEXT("Turn_Standing_90RRootMotion_"));
			Set.TurnStanding180L = P(TEXT("Locomotion/Turn"), TEXT("Turn_Standing_180LRootMotion_"));   Set.TurnStanding180R = P(TEXT("Locomotion/Turn"), TEXT("Turn_Standing_180RRootMotion_"));
			Set.TurnCrouching90L = P(TEXT("Locomotion/Turn"), TEXT("Turn_Crouching_90LRootMotion_"));   Set.TurnCrouching90R = P(TEXT("Locomotion/Turn"), TEXT("Turn_Crouching_90RRootMotion_"));
		};
		LoadHuman(SyntyLocomotionSet, SyntyMascRoot, TEXT("Masc"));
		LoadHuman(SyntyFeminineLocomotionSet, SyntyFemnRoot, TEXT("Femn"));
		{
			FLocomotionAnimSet& Set = SyntyGoblinLocomotionSet;
			const auto G = [&](const TCHAR* Sub, const TCHAR* Stem) { return LoadPath(FString::Printf(TEXT("%s%s/A_POLY_GBL_%s_Neut"), SyntyGoblinRoot, Sub, Stem)); };
			Set.WalkUpAnim = G(TEXT("Locomotion/Walk"), TEXT("Walk_Up25_F"));       Set.WalkDownAnim = G(TEXT("Locomotion/Walk"), TEXT("Walk_Down25_F"));
			Set.JogUpAnim = G(TEXT("Locomotion/Run"), TEXT("Run_Up25_F"));           Set.JogDownAnim = G(TEXT("Locomotion/Run"), TEXT("Run_Down25_F"));
			Set.SprintUpAnim = G(TEXT("Locomotion/Sprint"), TEXT("Sprint_Up25_F"));  Set.SprintDownAnim = G(TEXT("Locomotion/Sprint"), TEXT("Sprint_Down25_F"));
			Set.StartWalkF = G(TEXT("Transitions/Idle_ToWalk"), TEXT("Idle_ToWalk_F_RM"));
			Set.StartWalk90L = G(TEXT("Transitions/Idle_ToWalk"), TEXT("Idle_ToWalk_90_L_RM"));   Set.StartWalk90R = G(TEXT("Transitions/Idle_ToWalk"), TEXT("Idle_ToWalk_90_R_RM"));
			Set.StartWalk180L = G(TEXT("Transitions/Idle_ToWalk"), TEXT("Idle_ToWalk_180_L_RM")); Set.StartWalk180R = G(TEXT("Transitions/Idle_ToWalk"), TEXT("Idle_ToWalk_180_R_RM"));
			Set.StartRunF = G(TEXT("Transitions/Idle_ToRun"), TEXT("Idle_ToRun_F_RM"));
			Set.StartRun90L = G(TEXT("Transitions/Idle_ToRun"), TEXT("Idle_ToRun_90_L_RM"));      Set.StartRun90R = G(TEXT("Transitions/Idle_ToRun"), TEXT("Idle_ToRun_90_R_RM"));
			Set.StartRun180L = G(TEXT("Transitions/Idle_ToRun"), TEXT("Idle_ToRun_180_L_RM"));    Set.StartRun180R = G(TEXT("Transitions/Idle_ToRun"), TEXT("Idle_ToRun_180_R_RM"));
			Set.StopWalkLFoot = G(TEXT("Transitions/Walk_ToIdle"), TEXT("Walk_ToIdle_FL"));        Set.StopWalkRFoot = G(TEXT("Transitions/Walk_ToIdle"), TEXT("Walk_ToIdle_FR"));
			Set.StopRunLFoot = G(TEXT("Transitions/Run_ToIdle"), TEXT("Run_ToIdle_LFoot"));        Set.StopRunRFoot = G(TEXT("Transitions/Run_ToIdle"), TEXT("Run_ToIdle_RFoot"));
			Set.StandToCrouchAnim = G(TEXT("Transitions/Crouch"), TEXT("Stand_ToCrouch"));
			Set.CrouchToStandAnim = G(TEXT("Transitions/Crouch"), TEXT("Crouch_ToStand"));
			Set.SprintToCrouchAnim = G(TEXT("Transitions/Crouch"), TEXT("Sprint_ToCrouch_RM"));
			Set.TurnStanding90L = G(TEXT("Locomotion/Turn"), TEXT("Turn_Standing_90L_RM"));      Set.TurnStanding90R = G(TEXT("Locomotion/Turn"), TEXT("Turn_Standing_90R_RM"));
			Set.TurnStanding180L = G(TEXT("Locomotion/Turn"), TEXT("Turn_Standing_180L_RM"));    Set.TurnStanding180R = G(TEXT("Locomotion/Turn"), TEXT("Turn_Standing_180R_RM"));
			Set.TurnCrouching90L = G(TEXT("Locomotion/Turn"), TEXT("Turn_Crouching_90_L_RM"));   Set.TurnCrouching90R = G(TEXT("Locomotion/Turn"), TEXT("Turn_Crouching_90_R_RM"));
		}
	}

	// See IdleVariants's header comment on the class -- built here, not
	// EditAnywhere of its own, since it's purely derived from the 4
	// properties above/below. Includes SyntyLocomotionSet.IdleAnim (already
	// assigned above) so 'I' cycles through every known idle regardless of
	// which locomotion set 'O' currently has active.
	IdleVariants = { IdleAnim, IdleBreakAnim, IdleBreakFidgetAnim, SyntyLocomotionSet.IdleAnim };

	// A loop rather than 17 individually-named static FObjectFinder
	// declarations -- ConstructorHelpers::FObjectFinder only requires being
	// constructed from within a constructor (enforced internally via an
	// RF_ClassDefaultObject/RF_NeedLoad check), not that each one be its own
	// function-local static; the "static" convention elsewhere in this
	// constructor exists purely to avoid repeating the asset lookup on
	// every construction, which for 17 flat entries in a TArray isn't worth
	// the boilerplate it'd take to preserve.
	PoseAnims.Reserve(UE_ARRAY_COUNT(PoseAnimPaths));
	for (const TCHAR* Path : PoseAnimPaths)
	{
		ConstructorHelpers::FObjectFinder<UAnimSequence> PoseFinder(Path);
		if (PoseFinder.Object)
		{
			PoseAnims.Add(PoseFinder.Object);
		}
	}

	// Same loop pattern as PoseAnims above, for the 9 Sword Combat attack
	// clips -- order here is exactly the 1-9 key order (see ABaseCharacter::
	// AttackActions).
	AttackAnims.Reserve(UE_ARRAY_COUNT(AttackAnimPaths));
	for (const TCHAR* Path : AttackAnimPaths)
	{
		ConstructorHelpers::FObjectFinder<UAnimSequence> AttackFinder(Path);
		if (AttackFinder.Object)
		{
			AttackAnims.Add(AttackFinder.Object);
		}
	}

	// See ArmOverridePose's header comment.
	static ConstructorHelpers::FObjectFinder<UAnimSequence> ArmOverrideFinder(TEXT("/Game/Characters/Animations/SyntySwordCombat/Idle/Base/A_Idle_Base_Sword.A_Idle_Base_Sword"));
	ArmOverridePose = ArmOverrideFinder.Object;
}

void UCharacterAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	// Only populate once -- NativeInitializeAnimation can run more than
	// once for the same instance (e.g. if the mesh's skeleton changes), and
	// re-querying the registry every time would be wasted work for data
	// that never changes at runtime.
	if (AllCombatAnims.Num() > 0)
	{
		return;
	}

	const IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FAssetData> CombatAssetData;
	AssetRegistry.GetAssetsByPath(FName(TEXT("/Game/Characters/Animations/SyntySwordCombat")), CombatAssetData, /*bRecursive=*/true);

	// Sorted by name for a stable, predictable '['/']' browsing order --
	// the registry doesn't guarantee any particular enumeration order.
	CombatAssetData.Sort([](const FAssetData& A, const FAssetData& B)
	{
		return A.AssetName.LexicalLess(B.AssetName);
	});

	AllCombatAnims.Reserve(CombatAssetData.Num());
	for (const FAssetData& AssetData : CombatAssetData)
	{
		if (UAnimSequence* Anim = Cast<UAnimSequence>(AssetData.GetAsset()))
		{
			AllCombatAnims.Add(Anim);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("UCharacterAnimInstance: loaded %d clips into AllCombatAnims"), AllCombatAnims.Num());
}

UAnimSequence* UCharacterAnimInstance::GetDashAnim(ECharacterDashDirection Direction) const
{
	switch (Direction)
	{
	case ECharacterDashDirection::Forward: return DashForwardAnim;
	case ECharacterDashDirection::Backward: return DashBackwardAnim;
	case ECharacterDashDirection::Left: return DashLeftAnim;
	case ECharacterDashDirection::Right: return DashRightAnim;
	default: return DashForwardAnim;
	}
}

FAnimInstanceProxy* UCharacterAnimInstance::CreateAnimInstanceProxy()
{
	return new FCharacterAnimInstanceProxy(this);
}

void UCharacterAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete InProxy;
}

void FCharacterAnimInstanceProxy::PlayWithBlend(UAnimSequence* NewAsset, bool bLoop, float PlayRate, float InBlendDuration)
{
	if (NewAsset == ToAsset)
	{
		// Already playing/transitioning to this asset -- just update the
		// loop/rate in case they changed, don't restart the blend.
		bToLooping = bLoop;
		ToPlayRate = PlayRate;
		return;
	}

	const bool bHadPreviousAsset = (ToAsset != nullptr);
	if (bHadPreviousAsset)
	{
		FromAsset = ToAsset;
		FromTime = ToTime;
		FromPlayRate = ToPlayRate;
		bFromLooping = bToLooping;
	}
	// else: this is the very first call this instance has ever made (right
	// after BeginPlay) -- nothing to fade from, snaps immediately below.
	// Unlike the old split AnimBP/crossfade design, that's the ONLY case
	// this can happen in now, since this instance is assigned once and
	// never swapped.

	ToAsset = NewAsset;
	ToTime = 0.0f;
	ToPlayRate = PlayRate;
	bToLooping = bLoop;

	BlendDuration = FMath::Max(InBlendDuration, 0.01f);
	BlendAlpha = bHadPreviousAsset ? 0.0f : 1.0f;
}

void FCharacterAnimInstanceProxy::UpdateLocomotionState(float DeltaSeconds)
{
	if (!Owner) { return; }

	TimeInState += DeltaSeconds;

	const FLocomotionInputs& In = PendingInputs;
	const bool bSynty = (CurrentLocomotionSet == ECharacterLocomotionSet::Synty);
	const FLocomotionAnimSet& S = Owner->GetActiveSyntySet();

	// See LastAirTime's header comment.
	if (In.bIsInAir) { LastAirTime = In.AirTime; }

	const auto IsCrouchState = [](ECharacterLocomotionState State)
	{
		return State == ECharacterLocomotionState::CrouchIdle || State == ECharacterLocomotionState::CrouchWalk || State == ECharacterLocomotionState::CrouchRun;
	};
	const bool bWasCrouched = IsCrouchState(CurrentState);

	// The transition states and what breaks each one off early: a start
	// turn dies with the stick, a stop revives with it, a turn/crouch
	// change yields to any movement; all of them to going airborne. The
	// slide is only ever ended by the character (its duration).
	const auto IsTransitionState = [](ECharacterLocomotionState State)
	{
		return State == ECharacterLocomotionState::StartMove || State == ECharacterLocomotionState::StopMove
			|| State == ECharacterLocomotionState::TurnInPlace || State == ECharacterLocomotionState::CrouchDown
			|| State == ECharacterLocomotionState::CrouchUp || State == ECharacterLocomotionState::SprintSlide;
	};
	const auto TransitionInterrupted = [this](const FLocomotionInputs& Inputs)
	{
		if (Inputs.bIsInAir) { return CurrentState != ECharacterLocomotionState::SprintSlide; }
		switch (CurrentState)
		{
		case ECharacterLocomotionState::StartMove:   return !Inputs.bHasMoveInput || Inputs.bIsCrouched;
		case ECharacterLocomotionState::StopMove:    return Inputs.bHasMoveInput || Inputs.bIsCrouched;
		case ECharacterLocomotionState::TurnInPlace: return Inputs.bHasMoveInput || Inputs.Speed > Owner->WalkSpeedThreshold;
		case ECharacterLocomotionState::CrouchDown:  return !Inputs.bIsCrouched || Inputs.bHasMoveInput;
		case ECharacterLocomotionState::CrouchUp:    return Inputs.bIsCrouched || Inputs.bHasMoveInput;
		default: return false;
		}
	};

	// Idle-variant cycle ('I'): the index advances immediately; whether the
	// new clip is visible THIS frame depends on whether Idle is what the
	// chain below decides on -- the "clip changed" rule near the end forces
	// the refresh if so, without ever interrupting a jump/roll/crouch just
	// to preview an idle. Same for 'O': ToggleLocomotionSet already flipped
	// the set, and that same rule surfaces the new set's clip.
	if (bPendingIdleVariantCycleTriggered && Owner->IdleVariants.Num() > 0)
	{
		CurrentIdleVariantIndex = (CurrentIdleVariantIndex + 1) % Owner->IdleVariants.Num();
	}
	bPendingIdleVariantCycleTriggered = false;
	bPendingLocomotionSetToggled = false;
	if (bPendingAttackAbort)
	{
		// Drop the held clip so the Attack hold below no longer applies.
		bPendingAttackAbort = false;
		if (CurrentState == ECharacterLocomotionState::Attack) { CurrentAttackAnim = nullptr; bCurrentAttackLoops = false; }
	}

	// Standing tier by NEAREST configured speed (boundaries at the
	// midpoints), driven by the character's own tier speeds so there are no
	// thresholds duplicated here to drift out of step with them.
	const float WalkJogBoundary = 0.5f * (In.WalkSpeed + In.JogSpeed);
	const float JogRunBoundary = 0.5f * (In.JogSpeed + In.RunSpeed);
	const ECharacterLocomotionState StandingTier =
		(In.Speed < WalkJogBoundary) ? ECharacterLocomotionState::Walk
		: (In.Speed < JogRunBoundary) ? ECharacterLocomotionState::Jog
		: ECharacterLocomotionState::Run;

	// Lyra play rates: ratio to the CURRENT cap (1.0 = "moving as fast as
	// the clip was tuned for"), narrowly clamped so acceleration transients
	// don't blur or freeze the legs -- Lyra's clips have no measured native
	// speed (root motion zeroed), so the cap is the best reference there
	// is. CrouchWalk uses the fixed fast-crouch speed instead of the
	// current cap so the two crouch tiers don't both settle to ~1.0x.
	// Every rate divides by StrideScale -- see its comment on FLocomotionInputs.
	const float Stride = FMath::Max(In.StrideScale, 0.05f);
	const float LyraSpeedPlayRate = FMath::Clamp(In.Speed / FMath::Max(In.MaxSpeed * Stride, 1.0f), 0.5f, 1.5f);
	const float LyraCrouchPlayRate = FMath::Clamp(In.Speed / FMath::Max(In.CrouchFastSpeed * Stride, 1.0f), 0.5f, 1.5f);

	// Synty play rates: ratio to the clip's own MEASURED native speed (see
	// FLocomotionAnimSet's header comment). Wider clamp than Lyra's on
	// purpose -- a tier speed that legitimately differs from the clip's
	// native pace (sprint clip at ~800 driven at a 600 cap) is a steady
	// state here, not a transient.
	const auto SyntyRate = [&In, Stride](float NativeSpeed)
	{
		return FMath::Clamp(In.Speed / FMath::Max(NativeSpeed * Stride, 1.0f), 0.25f, 3.0f);
	};

	ECharacterLocomotionState Desired = CurrentState;
	UAnimSequence* DesiredAnim = nullptr;
	bool bDesiredLoop = true;
	float DesiredPlayRate = 1.0f;

	// A fresh roll/dash/pose trigger must always force a real PlayWithBlend
	// call, even on the (currently unreachable for Roll/Dash, since
	// ABaseCharacter gates re-triggering with its own cooldown/bIsRolling/
	// bIsDashing, but not something this state machine should silently rely
	// on -- and genuinely reachable for Pose, which should visibly cycle
	// even while already showing a pose) chance CurrentState already
	// matches -- the plain "Desired != CurrentState" check below can't tell
	// that apart from "nothing changed".
	bool bForceTransition = false;

	if (bPendingRollTriggered)
	{
		// Highest priority, always honored -- by the time ABaseCharacter::
		// StartRoll calls TriggerRoll, it has already decided a roll is
		// allowed (cooldown, not already rolling, etc.), so this state
		// machine's job is just to play it, not to gate whether it happens.
		Desired = ECharacterLocomotionState::Roll;
		DesiredAnim = Owner->RollAnim;
		bDesiredLoop = false;
		bPendingRollTriggered = false;
		bForceTransition = true;
	}
	else if (bPendingDashTriggered)
	{
		// Same reasoning as the roll trigger above -- ABaseCharacter::
		// StartDash has already decided a dash is allowed and picked the
		// direction from current WASD input.
		CurrentDashDirection = PendingDashDirection;
		Desired = ECharacterLocomotionState::Dash;
		DesiredAnim = Owner->GetDashAnim(CurrentDashDirection);
		bDesiredLoop = false;
		bPendingDashTriggered = false;
		bForceTransition = true;
	}
	else if (bPendingPoseCycleTriggered && Owner->PoseAnims.Num() > 0)
	{
		// Unlike Roll/Dash this CAN retrigger while already CurrentState ==
		// Pose -- each press should visibly cycle to the next pose, not be
		// silently absorbed because Desired == CurrentState already.
		CurrentPoseIndex = (CurrentPoseIndex + 1) % Owner->PoseAnims.Num();
		Desired = ECharacterLocomotionState::Pose;
		DesiredAnim = Owner->PoseAnims[CurrentPoseIndex];
		bDesiredLoop = true;
		bPendingPoseCycleTriggered = false;
		bForceTransition = true;
	}
	else if (bPendingAttackTriggered)
	{
		// Same "already decided this is allowed" reasoning as Roll/Dash --
		// ABaseCharacter::StartAttack/PlaySelectedAttack have already
		// checked bIsRolling/bIsDashing/bIsAttacking before calling
		// TriggerAttack.
		CurrentAttackAnim = PendingAttackAnim;
		bCurrentAttackLoops = bPendingAttackLoop;
		Desired = ECharacterLocomotionState::Attack;
		DesiredAnim = CurrentAttackAnim;
		bDesiredLoop = bCurrentAttackLoops;
		bPendingAttackTriggered = false;
		bForceTransition = true;
	}
	else if (bPendingSlideTriggered)
	{
		// Sprint -> crouch slide: root-motion driven (translation), see
		// ABaseCharacter::StartSprintSlide, which has already vetted it.
		bPendingSlideTriggered = false;
		if (bSynty && S.SprintToCrouchAnim)
		{
			Desired = ECharacterLocomotionState::SprintSlide;
			DesiredAnim = S.SprintToCrouchAnim;
			bDesiredLoop = false;
			bForceTransition = true;
			CurrentTransitionAnim = DesiredAnim;
			bClipDrivesYaw = false;
			bClipDrivesTranslation = true;
			RootMotionScale = S.SprintSlideScale;
		}
	}
	else if (bPendingTurnTriggered)
	{
		// Turn in place: nearest 90/180 clip on the requested side; its root
		// yaw steers the capsule. A crouched 180 chains two 90s.
		bPendingTurnTriggered = false;
		const bool bLeft = PendingTurnYaw < 0.0f;
		const bool bBig = FMath::Abs(PendingTurnYaw) >= 135.0f;
		UAnimSequence* Clip = nullptr;
		TurnStepsLeft = 0;
		if (In.bIsCrouched)
		{
			Clip = bLeft ? S.TurnCrouching90L.Get() : S.TurnCrouching90R.Get();
			if (bBig && Clip) { TurnStepsLeft = 1; }
		}
		else
		{
			Clip = bBig ? (bLeft ? S.TurnStanding180L.Get() : S.TurnStanding180R.Get()) : (bLeft ? S.TurnStanding90L.Get() : S.TurnStanding90R.Get());
		}
		if (bSynty && Clip && !In.bIsInAir && In.Speed <= Owner->WalkSpeedThreshold)
		{
			Desired = ECharacterLocomotionState::TurnInPlace;
			DesiredAnim = Clip;
			bDesiredLoop = false;
			bForceTransition = true;
			CurrentTransitionAnim = Clip;
			bClipDrivesYaw = true;
			bClipDrivesTranslation = false;
		}
	}
	else if (IsTransitionState(CurrentState) && CurrentTransitionAnim && TimeInState < CurrentTransitionAnim->GetPlayLength()
		&& !TransitionInterrupted(In))
	{
		// A transition clip still playing and nothing has overridden it.
		return;
	}
	else if (CurrentState == ECharacterLocomotionState::TurnInPlace && TurnStepsLeft > 0 && CurrentTransitionAnim && !TransitionInterrupted(In))
	{
		// Second half of a crouched 180.
		--TurnStepsLeft;
		Desired = ECharacterLocomotionState::TurnInPlace;
		DesiredAnim = CurrentTransitionAnim;
		bDesiredLoop = false;
		bForceTransition = true;
	}
	else if (CurrentState == ECharacterLocomotionState::Attack && CurrentAttackAnim
		&& (bCurrentAttackLoops || TimeInState < CurrentAttackAnim->GetPlayLength()))
	{
		// A held clip (bCurrentAttackLoops) stays here regardless of input
		// until ReleaseAttackHold(); a one-shot holds only for its length.
		// Still playing the attack -- same hold pattern as Roll/Dash above.
		// Unlike those, there's no root motion of its own to extract
		// (AttackAnims are plain in-place clips), but the state machine
		// still needs to hold here so normal locomotion doesn't immediately
		// override the attack pose mid-swing. Deliberately folded into the
		// else-if's own condition (not a nested if inside an unconditional
		// "CurrentState == Attack" branch) -- a nested-if version matched
		// this branch of the chain regardless of whether the clip had
		// already finished, and once finished left DesiredAnim unset with
		// no further branch in the chain able to run, which is what caused
		// the very real "doesn't return to a rest state when done" bug:
		// the state machine hit the final "if (!DesiredAnim) return;" and
		// froze on the attack's last held frame forever. Matching Roll/
		// Dash's exact shape means a finished attack correctly falls
		// through to whichever later branch (jump/crouch/run/walk/idle)
		// actually applies.
		return;
	}
	else if (CurrentState == ECharacterLocomotionState::Roll && Owner->RollAnim
		&& TimeInState < Owner->RollAnim->GetPlayLength())
	{
		// Still playing the roll -- let it finish before falling through to
		// normal locomotion, same hold pattern as JumpEnd below.
		return;
	}
	else if (CurrentState == ECharacterLocomotionState::Dash)
	{
		const UAnimSequence* CurrentDashAnim = Owner->GetDashAnim(CurrentDashDirection);
		if (CurrentDashAnim && TimeInState < CurrentDashAnim->GetPlayLength())
		{
			// Still playing the dash -- same hold pattern as Roll above.
			return;
		}
	}
	else if (CurrentState == ECharacterLocomotionState::Pose
		&& !In.bIsInAir && !In.bIsCrouched && In.Speed <= Owner->WalkSpeedThreshold)
	{
		// Holds the pose until the player actually does something -- became
		// airborne, crouched, or started moving. A fresh Roll/Dash/Pose
		// trigger already took priority above this branch.
		return;
	}
	else if (In.bIsInAir)
	{
		if (!bSynty)
		{
			// Lyra: near-static rise/fall hold poses, switched on actual
			// vertical velocity since neither is a continuous arc.
			const bool bRising = In.VelocityZ > 0.0f;
			Desired = bRising ? ECharacterLocomotionState::JumpRise : ECharacterLocomotionState::JumpFall;
			DesiredAnim = bRising ? Owner->JumpRiseAnim.Get() : Owner->JumpFallAnim.Get();
		}
		else if (CurrentState != ECharacterLocomotionState::JumpRise && CurrentState != ECharacterLocomotionState::JumpFall)
		{
			// Just left the ground: pick the full launch-to-land arc by the
			// tier the character took off at, and play it once.
			UAnimSequence* Arc = S.JumpIdleAnim;
			if (In.Speed > Owner->WalkSpeedThreshold)
			{
				Arc = (StandingTier == ECharacterLocomotionState::Walk) ? S.JumpWalkingAnim.Get()
					: (StandingTier == ECharacterLocomotionState::Jog) ? S.JumpRunningAnim.Get()
					: S.JumpSprintingAnim.Get();
			}
			CurrentJumpAnim = Arc ? Arc : S.FallShortAnim.Get();
			Desired = ECharacterLocomotionState::JumpRise;
			DesiredAnim = CurrentJumpAnim;
			bDesiredLoop = false;
			DesiredPlayRate = S.JumpArcPlayRate;
		}
		else if (CurrentState == ECharacterLocomotionState::JumpRise && CurrentJumpAnim
			&& TimeInState < CurrentJumpAnim->GetPlayLength() / FMath::Max(S.JumpArcPlayRate, 0.01f))
		{
			// Arc still playing.
			return;
		}
		else
		{
			// Arc ran out while still airborne (a long drop): hold a fall
			// loop, the large one once it's been a while.
			Desired = ECharacterLocomotionState::JumpFall;
			DesiredAnim = (In.AirTime > S.LongFallAirSeconds && S.FallLargeAnim) ? S.FallLargeAnim.Get() : S.FallShortAnim.Get();
		}
	}
	else if (CurrentState == ECharacterLocomotionState::JumpRise || CurrentState == ECharacterLocomotionState::JumpFall)
	{
		// Just landed.
		Desired = ECharacterLocomotionState::JumpEnd;
		bDesiredLoop = false;
		if (bSynty)
		{
			DesiredAnim = (LastAirTime > S.HardLandingAirSeconds && S.LandHardAnim) ? S.LandHardAnim.Get()
				: (LastAirTime > S.MediumLandingAirSeconds && S.LandMediumAnim) ? S.LandMediumAnim.Get()
				: S.LandSoftAnim.Get();
			DesiredPlayRate = S.LandPlayRate;
		}
		else
		{
			DesiredAnim = Owner->JumpEndAnim;
			DesiredPlayRate = Owner->JumpEndPlayRate;
		}
		CurrentLandAnim = DesiredAnim;
		CurrentLandPlayRate = DesiredPlayRate;
	}
	else if (CurrentState == ECharacterLocomotionState::JumpEnd && CurrentLandAnim
		&& TimeInState < CurrentLandAnim->GetPlayLength() / FMath::Max(CurrentLandPlayRate, 0.01f)
		&& !(TimeInState > S.LandBreakoutSeconds && In.Speed > Owner->WalkSpeedThreshold))
	{
		// Still playing the landing recovery -- held, unless the player is
		// already moving again past the impact (see LandBreakoutSeconds).
		return;
	}
	else if (In.bIsCrouched)
	{
		const bool bWasStanding = !IsCrouchState(CurrentState) && CurrentState != ECharacterLocomotionState::CrouchDown && CurrentState != ECharacterLocomotionState::SprintSlide;
		if (In.Speed <= Owner->WalkSpeedThreshold && bWasStanding && bSynty && S.StandToCrouchAnim)
		{
			// Going down from standing still: the pack's stand->crouch clip
			// first, then the crouch idle.
			Desired = ECharacterLocomotionState::CrouchDown;
			DesiredAnim = S.StandToCrouchAnim;
			bDesiredLoop = false;
			CurrentTransitionAnim = DesiredAnim;
			bClipDrivesYaw = false;
			bClipDrivesTranslation = false;
		}
		else if (In.Speed <= Owner->WalkSpeedThreshold)
		{
			Desired = ECharacterLocomotionState::CrouchIdle;
			DesiredAnim = PickAnim(Owner->CrouchIdleAnim, S.CrouchIdleAnim);
		}
		else
		{
			// One crouch clip set, two speed tiers (Shift) -- separate states
			// so each gets its own tuning slot, since they need different
			// correction factors against the one shared clip pace.
			const bool bCrouchRunning = In.Speed > 0.5f * (In.CrouchSlowSpeed + In.CrouchFastSpeed);
			Desired = bCrouchRunning ? ECharacterLocomotionState::CrouchRun : ECharacterLocomotionState::CrouchWalk;
			DesiredAnim = PickAnim(Owner->CrouchWalkAnim, S.Crouch.Pick(In.MoveAngleDeg));
			DesiredPlayRate = bSynty
				? SyntyRate(S.CrouchAnimSpeed)
				: LyraCrouchPlayRate;
		}
	}
	else if (In.Speed <= Owner->WalkSpeedThreshold && !In.bHasMoveInput)
	{
		const bool bWasCrouchedState = IsCrouchState(CurrentState) || CurrentState == ECharacterLocomotionState::CrouchDown;
		if (bWasCrouchedState && bSynty && S.CrouchToStandAnim)
		{
			// Coming up from a crouch: the crouch->stand clip, then idle.
			Desired = ECharacterLocomotionState::CrouchUp;
			DesiredAnim = S.CrouchToStandAnim;
			bDesiredLoop = false;
			CurrentTransitionAnim = DesiredAnim;
			bClipDrivesYaw = false;
			bClipDrivesTranslation = false;
		}
		else
		{
			// Deliberately NOT routed through PickAnim -- 'I' cycling governs
			// Idle unconditionally (IdleVariants already includes the Synty
			// idle), regardless of which set 'O' has active.
			Desired = ECharacterLocomotionState::Idle;
			DesiredAnim = GetCurrentIdleAnim();
		}
	}
	else if (!In.bHasMoveInput && bSynty
		&& (CurrentState == ECharacterLocomotionState::Walk || CurrentState == ECharacterLocomotionState::Jog
			|| CurrentState == ECharacterLocomotionState::Run || CurrentState == ECharacterLocomotionState::StartMove)
		&& (S.StopWalkLFoot || S.StopRunLFoot))
	{
		// Input released while moving: settle into idle with the stop clip
		// for the planted foot while the capsule brakes underneath it.
		const bool bWalking = (CurrentState == ECharacterLocomotionState::Walk)
			|| (CurrentState == ECharacterLocomotionState::StartMove && In.MaxSpeed <= WalkJogBoundary);
		UAnimSequence* Clip = bWalking ? (bStanceLeft ? S.StopWalkLFoot.Get() : S.StopWalkRFoot.Get())
			: (bStanceLeft ? S.StopRunLFoot.Get() : S.StopRunRFoot.Get());
		if (!Clip) { Clip = bWalking ? S.StopWalkRFoot.Get() : S.StopRunRFoot.Get(); }
		Desired = ECharacterLocomotionState::StopMove;
		DesiredAnim = Clip;
		bDesiredLoop = false;
		CurrentTransitionAnim = Clip;
		bClipDrivesYaw = false;
		bClipDrivesTranslation = false;
	}
	else if (In.Speed <= Owner->WalkSpeedThreshold && !In.bHasMoveInput)
	{
		Desired = ECharacterLocomotionState::Idle;
		DesiredAnim = GetCurrentIdleAnim();
	}
	else if (In.bHasMoveInput && bSynty && S.StartRunF
		&& (CurrentState == ECharacterLocomotionState::Idle || CurrentState == ECharacterLocomotionState::StopMove
			|| CurrentState == ECharacterLocomotionState::TurnInPlace || CurrentState == ECharacterLocomotionState::CrouchUp
			|| CurrentState == ECharacterLocomotionState::Pose))
	{
		// Setting off from a standstill: the start clip for the intended
		// direction. The 90/180 clips carry their turn in the root track,
		// which steers the capsule (orient-to-movement waits for the clip).
		const float A = In.InputAngleDeg;
		const bool bLeft = A < 0.0f;
		const float Abs = FMath::Abs(A);
		const bool bWalk = In.MaxSpeed <= WalkJogBoundary;
		UAnimSequence* Clip;
		if (Abs <= 60.0f) { Clip = bWalk ? S.StartWalkF.Get() : S.StartRunF.Get(); }
		else if (Abs <= 135.0f) { Clip = bWalk ? (bLeft ? S.StartWalk90L.Get() : S.StartWalk90R.Get()) : (bLeft ? S.StartRun90L.Get() : S.StartRun90R.Get()); }
		else { Clip = bWalk ? (bLeft ? S.StartWalk180L.Get() : S.StartWalk180R.Get()) : (bLeft ? S.StartRun180L.Get() : S.StartRun180R.Get()); }
		if (!Clip) { Clip = bWalk ? S.StartWalkF.Get() : S.StartRunF.Get(); }
		Desired = ECharacterLocomotionState::StartMove;
		DesiredAnim = Clip;
		bDesiredLoop = false;
		CurrentTransitionAnim = Clip;
		bClipDrivesYaw = Abs > 60.0f;
		bClipDrivesTranslation = false;
	}
	else
	{
		Desired = StandingTier;

		// Slope clips (forward travel only), with hysteresis so a bumpy
		// floor doesn't flicker between the flat and Up/Down cycles.
		if (bSynty && FMath::Abs(In.MoveAngleDeg) < 45.0f)
		{
			if (SlopeState == 0) { SlopeState = (In.SlopeDeg > S.SlopeEnterDegrees) ? 1 : (In.SlopeDeg < -S.SlopeEnterDegrees) ? -1 : 0; }
			else if (SlopeState > 0 && In.SlopeDeg < S.SlopeExitDegrees) { SlopeState = 0; }
			else if (SlopeState < 0 && In.SlopeDeg > -S.SlopeExitDegrees) { SlopeState = 0; }
		}
		else { SlopeState = 0; }
		const auto Sloped = [this](UAnimSequence* Flat, UAnimSequence* Up, UAnimSequence* Down) -> UAnimSequence*
		{
			return (SlopeState > 0 && Up) ? Up : (SlopeState < 0 && Down) ? Down : Flat;
		};

		switch (StandingTier)
		{
		case ECharacterLocomotionState::Walk:
			DesiredAnim = PickAnim(Owner->WalkAnim, Sloped(S.Walk.Pick(In.MoveAngleDeg), S.WalkUpAnim, S.WalkDownAnim));
			DesiredPlayRate = bSynty ? SyntyRate(S.WalkAnimSpeed) : LyraSpeedPlayRate;
			break;
		case ECharacterLocomotionState::Jog:
			DesiredAnim = PickAnim(Owner->RunAnim, Sloped(S.Jog.Pick(In.MoveAngleDeg), S.JogUpAnim, S.JogDownAnim));
			DesiredPlayRate = bSynty ? SyntyRate(S.JogAnimSpeed) : LyraSpeedPlayRate;
			break;
		default:
			// Lyra has no sprint clip -- its jog stands in for the Run tier.
			DesiredAnim = PickAnim(Owner->RunAnim, Sloped(S.SprintAnim, S.SprintUpAnim, S.SprintDownAnim));
			DesiredPlayRate = bSynty ? SyntyRate(S.SprintAnimSpeed) : LyraSpeedPlayRate;
			break;
		}
	}

	if (!DesiredAnim) { return; }

	// Leaving (or never entering) a transition state: nothing steers the
	// capsule from the root track any more.
	if (!IsTransitionState(Desired))
	{
		if (IsTransitionState(CurrentState))
		{
			UE_LOG(LogTemp, Verbose, TEXT("Transition %d ended -> %d: tis=%.3f len=%.3f interrupted=%d input=%d speed=%.1f air=%d crouch=%d"), static_cast<int32>(CurrentState), static_cast<int32>(Desired),
				TimeInState, CurrentTransitionAnim ? CurrentTransitionAnim->GetPlayLength() : -1.0f, TransitionInterrupted(In), In.bHasMoveInput, In.Speed, In.bIsInAir, In.bIsCrouched);
		}
		bClipDrivesYaw = false;
		bClipDrivesTranslation = false;
		CurrentTransitionAnim = nullptr;
	}

	// Gait timing layer (cadence / stumble jitter / limp) warps the ground
	// locomotion cycles only -- never jumps, rolls, attacks or idles.
	bGaitMoving = Desired == ECharacterLocomotionState::Walk || Desired == ECharacterLocomotionState::Jog
		|| Desired == ECharacterLocomotionState::Run || Desired == ECharacterLocomotionState::CrouchWalk
		|| Desired == ECharacterLocomotionState::CrouchRun;
	if (bGaitMoving) { DesiredPlayRate *= GaitRateMultiplier; }

	// Any change of the actual CLIP while the state name stays the same --
	// a strafe direction change within Walk, an 'I' idle-variant cycle, an
	// 'O' set swap -- has to start a blend too, not just a state change.
	if (DesiredAnim != ToAsset) { bForceTransition = true; }

	// !bLocomotionInitialized forces a real PlayWithBlend call on the very
	// first tick even though Desired starts out equal to CurrentState's
	// default (Idle) -- without this, a character that spawns stationary
	// would never call PlayWithBlend at all until its first real
	// transition, leaving the mesh showing its raw bind pose (T-pose) until
	// then, the same bug already fixed once for the old design.
	if (Desired != CurrentState || !bLocomotionInitialized || bForceTransition)
	{
		float BlendDurationToUse = Owner->AnimBlendDuration;
		const bool bWillBeCrouched = IsCrouchState(Desired);
		if (!bWasCrouched && bWillBeCrouched) { BlendDurationToUse = Owner->CrouchDownBlendDuration; }
		else if (bWasCrouched && !bWillBeCrouched) { BlendDurationToUse = Owner->StandUpBlendDuration; }
		else if (Desired == ECharacterLocomotionState::Roll) { BlendDurationToUse = Owner->RollBlendDuration; }
		else if (Desired == ECharacterLocomotionState::Dash) { BlendDurationToUse = Owner->DashBlendDuration; }
		else if (Desired == ECharacterLocomotionState::Pose) { BlendDurationToUse = Owner->PoseBlendDuration; }
		else if (Desired == ECharacterLocomotionState::Attack) { BlendDurationToUse = Owner->AttackBlendDuration; }

		CurrentState = Desired;
		TimeInState = 0.0f;
		bLocomotionInitialized = true;
		PlayWithBlend(DesiredAnim, bDesiredLoop, DesiredPlayRate, BlendDurationToUse);
	}
	else
	{
		// Same clip, same state: still push the CURRENT play rate through.
		// This used to happen only at transitions, which froze the speed-
		// synced rate at whatever speed the character had the instant it
		// entered the state -- e.g. Run entered at ~325 cm/s locked in
		// 325/285 = 1.14x for the whole run even as speed climbed to 500.
		// That frozen rate was the skating that survived every calibration
		// pass (confirmed live: the HUD showed exactly 1.14 while running).
		ToPlayRate = DesiredPlayRate;
		bToLooping = bDesiredLoop;
	}
}

UAnimSequence* FDirectionalAnimSet::Pick(float MoveAngleDeg) const
{
	const float A = FMath::Abs(MoveAngleDeg);
	const bool bRight = MoveAngleDeg > 0.0f;
	UAnimSequence* Chosen = nullptr;
	if (A <= 22.5f) { Chosen = F; }
	else if (A <= 67.5f) { Chosen = bRight ? FR : FL; }
	else if (A <= 112.5f) { Chosen = bRight ? R : L; }
	else if (A <= 157.5f) { Chosen = bRight ? BR : BL; }
	else { Chosen = B; }
	return Chosen ? Chosen : F.Get();
}

UAnimSequence* FCharacterAnimInstanceProxy::GetCurrentIdleAnim() const
{
	if (Owner && Owner->IdleVariants.IsValidIndex(CurrentIdleVariantIndex) && Owner->IdleVariants[CurrentIdleVariantIndex])
	{
		UAnimSequence* Anim = Owner->IdleVariants[CurrentIdleVariantIndex];
		// The Synty entry in IdleVariants is the Masculine idle; a Female
		// character gets her own set's idle in its place.
		if (Anim == Owner->SyntyLocomotionSet.IdleAnim && Owner->GetActiveSyntySet().IdleAnim) { Anim = Owner->GetActiveSyntySet().IdleAnim; }
		return Anim;
	}
	return Owner ? Owner->IdleAnim : nullptr;
}

UAnimSequence* FCharacterAnimInstanceProxy::PickAnim(UAnimSequence* LyraAnim, UAnimSequence* SyntyAnim) const
{
	if (CurrentLocomotionSet == ECharacterLocomotionSet::Synty && SyntyAnim)
	{
		return SyntyAnim;
	}
	return LyraAnim;
}

void FCharacterAnimInstanceProxy::UpdateLookAt(float DeltaSeconds)
{
	if (!Owner) { return; }

	// Freeze this frame's target here (game thread, PreUpdate) -- see the
	// header comment on bPendingHasLookAtTarget for why reading the Pending
	// copies directly from Evaluate() (worker thread) instead was a real
	// data race, and the actual cause of an intermittent "spiral" that no
	// amount of fixing the rotation math itself ever resolved.
	bHasLookAtTarget = bPendingHasLookAtTarget;
	LookAtTargetWorld = PendingLookAtTargetWorld;
	// Ease the look DIRECTION from the head toward the requested target so a
	// change of target (pad -> face -> colleague) reads as one turn of the
	// head at one pace, whether the new target is near or far. (Easing the
	// point in space made near targets crawl in angle.)
	if (bHasLookAtTarget)
	{
		FVector HeadForSmoothing = LookAtTargetWorld;
		if (const USkeletalMeshComponent* MeshComp = GetSkelMeshComponent())
		{
			HeadForSmoothing = MeshComp->DoesSocketExist(Owner->HeadBoneName) ? MeshComp->GetSocketLocation(Owner->HeadBoneName)
				: (MeshComp->GetOwner() ? MeshComp->GetOwner()->GetActorLocation() : LookAtTargetWorld);
		}
		const FVector ToWanted = LookAtTargetWorld - HeadForSmoothing;
		const float Dist = FMath::Max(10.0f, static_cast<float>(ToWanted.Size()));
		const FRotator Wanted = ToWanted.GetSafeNormal().Rotation();
		if (!bLookAtSmoothedValid) { SmoothedLookAtRotation = Wanted; bLookAtSmoothedValid = true; }
		// Shouldered or on the sights the head is LOCKED to the aim: the easing that makes a glance
		// read as one unhurried turn of the head is exactly wrong for a man tracking a target down
		// a barrel, where the head and the muzzle move as one. Everything else eases as before.
		else if (Owner->bLookAtLocked) { SmoothedLookAtRotation = Wanted; }
		else { SmoothedLookAtRotation = FMath::RInterpTo(SmoothedLookAtRotation, Wanted, DeltaSeconds, FMath::Max(0.1f, Owner->LookAtTurnSpeed)); }
		LookAtTargetWorld = HeadForSmoothing + SmoothedLookAtRotation.Vector() * Dist;
	}
	else { bLookAtSmoothedValid = false; }

	bool bWithinRange = false;
	if (bHasLookAtTarget)
	{
		if (const USkeletalMeshComponent* MeshComp = GetSkelMeshComponent())
		{
			if (const AActor* OwningActor = MeshComp->GetOwner())
			{
				// Measured from the head itself (falling back to the actor) so a
				// prop on the floor a metre away reads at the pitch the head
				// would actually have to tilt to, not the capsule centre's.
				const FVector HeadLoc = MeshComp->DoesSocketExist(Owner->HeadBoneName)
					? MeshComp->GetSocketLocation(Owner->HeadBoneName)
					: OwningActor->GetActorLocation();
				const FVector ToTarget = LookAtTargetWorld - HeadLoc;

				// Torso basis derived from the CURRENT POSE's actual shoulder
				// positions, not the capsule's forward vector -- see
				// ShoulderLeftBoneName's header comment on why the two can
				// disagree. Built from bone POSITIONS (not a bone's own local
				// rotation axes, which can be arbitrarily twisted depending on
				// how a given rig was authored) so it works regardless of any
				// particular skeleton's bone-local axis convention -- the same
				// reasoning that makes this robust across both the Lyra and
				// Synty rigs this proxy already switches between.
				FVector Forward = OwningActor->GetActorForwardVector();
				FVector Right = OwningActor->GetActorRightVector();
				const FVector Up = OwningActor->GetActorUpVector();

				const FVector ShoulderL = MeshComp->GetSocketLocation(Owner->ShoulderLeftBoneName);
				const FVector ShoulderR = MeshComp->GetSocketLocation(Owner->ShoulderRightBoneName);
				const FVector ShoulderRight = (ShoulderR - ShoulderL).GetSafeNormal();
				if (!ShoulderRight.IsNearlyZero())
				{
					// Right x Up = Forward (cyclic with UE's Forward x Right =
					// Up convention) -- re-derived from the shoulder line
					// rather than trusting Up to also come from the pose,
					// since a slight shoulder tilt (leaning, uneven footing)
					// shouldn't be read as the neck's whole reference frame
					// pitching too, only its yaw.
					Right = ShoulderRight;
					Forward = FVector::CrossProduct(Right, Up).GetSafeNormal();
					// A rig whose clavicles are named/positioned mirror-wise (or
					// a frame where the pose isn't refreshed yet) hands back a
					// basis pointing BACKWARDS, which read every target in front
					// as behind and vice versa -- the head then chased targets it
					// should have ignored. Never let the shoulder basis disagree
					// with the capsule about which way is forward.
					if (FVector::DotProduct(Forward, OwningActor->GetActorForwardVector()) < 0.0f)
					{
						Right = -Right;
						Forward = -Forward;
					}
				}
				// else: bone lookup failed (name typo, or a mesh missing these
				// bones entirely) -- falls back to the capsule's own vectors,
				// the previous behavior, rather than dividing by a near-zero
				// vector into garbage angles.

				// Yaw/pitch checked independently against this basis rather
				// than a single angle-from-forward cone -- a cone can't
				// express "pans further than it tilts", which is the actual
				// shape of a neck's range.
				const float ForwardDot = FVector::DotProduct(ToTarget, Forward);
				const float RightDot = FVector::DotProduct(ToTarget, Right);
				const float UpDot = FVector::DotProduct(ToTarget, Up);

				const float YawDeg = FMath::RadiansToDegrees(FMath::Atan2(RightDot, ForwardDot));
				const float HorizontalDist = FMath::Sqrt(ForwardDot * ForwardDot + RightDot * RightDot);
				const float PitchDeg = FMath::RadiansToDegrees(FMath::Atan2(UpDot, HorizontalDist));

				// Slightly wider than the hard clamp in ApplyLookAt: a target a
				// little past the limit is still "tried" (the head turns as far
				// as it naturally can toward it) rather than ignored outright;
				// only targets well outside the range make the head ease back.
				bWithinRange = FMath::Abs(YawDeg) <= Owner->LookAtMaxYawDegrees + Owner->LookAtGateSlackDegrees
					&& FMath::Abs(PitchDeg) <= Owner->LookAtMaxPitchDegrees + Owner->LookAtGateSlackDegrees;
			}
		}
	}

	const float TargetWeight = (bHasLookAtTarget && bWithinRange) ? 1.0f : 0.0f;
	LookAtWeight = FMath::FInterpTo(LookAtWeight, TargetWeight, DeltaSeconds, Owner->LookAtBlendSpeed);
	LookAtBoneName = Owner->HeadBoneName;
}

void FCharacterAnimInstanceProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);

	Owner = CastChecked<UCharacterAnimInstance>(InAnimInstance);

	// Cached here (game thread, safe) rather than read directly in
	// Evaluate() (which may run off the game thread).
	if (USkeletalMeshComponent* MeshComp = InAnimInstance->GetSkelMeshComponent())
	{
		CachedComponentToWorld = MeshComp->GetComponentTransform();
		// The actor's facing re-expressed in the mesh's component space (+Y
		// for the standard -90-yaw mesh offset every rig here uses) -- what
		// ApplyLookAt treats as "the way the face looks at rest".
		if (const AActor* OwnerActor = MeshComp->GetOwner())
		{
			const FVector Facing = CachedComponentToWorld.InverseTransformVectorNoScale(OwnerActor->GetActorForwardVector()).GetSafeNormal();
			if (!Facing.IsNearlyZero()) { CachedFacingComponent = Facing; }
		}
	}

	// See the header comments on these Cached* fields -- AFaceController
	// writes the live properties directly, at arbitrary times, so Evaluate()
	// must never read Owner->JawHingeDeg etc. itself.
	CachedJawHingeDeg = Owner->JawHingeDeg;
	CachedEyesHingeDeg = Owner->EyesHingeDeg;
	CachedEyesBlinkScale = Owner->EyesBlinkScale;
	CachedBrowHeightCm = Owner->BrowHeightCm;
	CachedBrowAngleDeg = Owner->BrowAngleDeg;

	CachedGait = Owner->Gait;
	CachedDeltaSeconds = DeltaSeconds;
	UpdateGaitTiming(DeltaSeconds);      // before locomotion: it feeds the play rate
	UpdateLocomotionState(DeltaSeconds);
	UpdateLookAt(DeltaSeconds);

	// Arm-override layer: full weight during ordinary locomotion, eased out
	// for anything that authors its own arms (see ArmOverridePose).
	{
		const bool bArmsAuthoredByClip =
			CurrentState == ECharacterLocomotionState::Attack || CurrentState == ECharacterLocomotionState::Pose
			|| CurrentState == ECharacterLocomotionState::Roll || CurrentState == ECharacterLocomotionState::Dash;
		// What was playing before this update, so a swap can keep it as the outgoing side.
		UAnimSequence* const PoseBeforeUpdate = CachedArmOverridePose;
		CachedArmOverridePose = Owner->ArmOverridePose;
		CachedArmOverrideRootBones = Owner->ArmOverrideRootBones;
		const float TargetAlpha = (!bArmsAuthoredByClip && CachedArmOverridePose) ? FMath::Clamp(Owner->ArmOverrideWeight, 0.0f, 1.0f) : 0.0f;
		const float AlphaPerSecond = 1.0f / FMath::Max(Owner->ArmOverrideBlendSeconds, KINDA_SMALL_NUMBER);
		ArmOverrideAlpha = FMath::FInterpConstantTo(ArmOverrideAlpha, TargetAlpha, DeltaSeconds, AlphaPerSecond);
		ArmIdleClock += DeltaSeconds;
		if (ArmOverrideSerialSeen != Owner->ArmOverrideSerial)
		{
			// The clip that was playing becomes the outgoing side of a crossfade. Its own time
			// and loop flag go with it, so a held last frame stays held while it fades.
			if (ArmOverrideSerialSeen >= 0 && PoseBeforeUpdate)
			{
				PrevArmOverridePose = PoseBeforeUpdate;
				PrevArmOverrideTime = ArmOverrideTime;
				bPrevArmOverrideLoops = bCachedArmOverrideLoops;
				ArmSwapAlpha = 0.0f;
			}
			ArmOverrideSerialSeen = Owner->ArmOverrideSerial;
			bCachedArmOverrideLoops = Owner->bArmOverrideLoops;
			// A looping pose is an idle: it resumes on the running clock. A one-shot starts
			// from the top, which is what a shot fired or a magazine changed has to do.
			const float Len = CachedArmOverridePose ? CachedArmOverridePose->GetPlayLength() : 0.0f;
			ArmOverrideTime = (bCachedArmOverrideLoops && Len > KINDA_SMALL_NUMBER) ? FMath::Fmod(ArmIdleClock, Len) : 0.0f;
		}
		if (CachedArmOverridePose && Owner->bArmOverrideAnimates)
		{
			AdvanceTime(ArmOverrideTime, DeltaSeconds, 1.0f, Owner->bArmOverrideLoops, CachedArmOverridePose);
		}
		if (PrevArmOverridePose)
		{
			if (Owner->bArmOverrideAnimates)
			{
				AdvanceTime(PrevArmOverrideTime, DeltaSeconds, 1.0f, bPrevArmOverrideLoops, PrevArmOverridePose);
			}
			ArmSwapAlpha = FMath::Min(1.0f, ArmSwapAlpha + DeltaSeconds / FMath::Max(Owner->ArmOverrideSwapSeconds, KINDA_SMALL_NUMBER));
			if (ArmSwapAlpha >= 1.0f) { PrevArmOverridePose = nullptr; }
		}
	}

	// Hand IK, copied here for the same reason as everything else: Evaluate can run off the
	// game thread and the owner rewrites these every frame.
	CachedIKTargetR = Owner->HandIKTargetR;
	CachedIKTargetL = Owner->HandIKTargetL;
	CachedIKWeightR = FMath::Clamp(Owner->HandIKWeightR, 0.0f, 1.0f);
	CachedIKWeightL = FMath::Clamp(Owner->HandIKWeightL, 0.0f, 1.0f);
	CachedIKMaxReach = Owner->HandIKMaxReach;
	CachedElbowBiasR = Owner->ElbowDownBiasR; CachedElbowBiasL = Owner->ElbowDownBiasL;
	bCachedFullBody = Owner->bFullBodyAction;
	CachedIKBones[0] = Owner->IKUpperArmR; CachedIKBones[1] = Owner->IKLowerArmR; CachedIKBones[2] = Owner->IKHandR;
	CachedIKBones[3] = Owner->IKUpperArmL; CachedIKBones[4] = Owner->IKLowerArmL; CachedIKBones[5] = Owner->IKHandL;
	CachedAimPitch = Owner->AimPitchDegrees;
	CachedSpineLeanWeight = FMath::Clamp(Owner->SpineLeanWeight, 0.0f, 1.0f);
	CachedSpineLeanFraction = Owner->SpineLeanFraction;
	CachedSpineLeanBones = Owner->SpineLeanBones;
	// Forward in COMPONENT space, so the lean axis is right whatever the actor's yaw is.
	CachedAimForwardCS = GetComponentTransform().InverseTransformVectorNoScale(Owner->AimForwardWorld).GetSafeNormal();

	PrevToTime = ToTime;
	AdvanceTime(ToTime, DeltaSeconds, ToPlayRate, bToLooping, ToAsset);
	// Transition clips whose root track drives the capsule: extract the root
	// motion this frame advanced over and bank it for the character (game
	// thread, consumed in its next Tick). Yaw always; translation only for
	// the sprint slide.
	if ((bClipDrivesYaw || bClipDrivesTranslation) && ToAsset && ToAsset == CurrentTransitionAnim && ToTime > PrevToTime)
	{
		FDeltaTimeRecord Record;
		Record.Set(PrevToTime, ToTime - PrevToTime);
		const FAnimExtractContext Ctx(static_cast<double>(ToTime), true, Record, false);
		const FTransform Delta = ToAsset->ExtractRootMotionFromRange(PrevToTime, ToTime, Ctx);
		UE_LOG(LogTemp, Verbose, TEXT("RootMotion: %s %.3f->%.3f yaw=%.2f t=(%.1f %.1f %.1f) state=%d tis=%.3f"), *ToAsset->GetName(), PrevToTime, ToTime, Delta.GetRotation().Rotator().Yaw, Delta.GetTranslation().X, Delta.GetTranslation().Y, Delta.GetTranslation().Z, static_cast<int32>(CurrentState), TimeInState);
		if (bClipDrivesYaw) { RootYawDeltaPending += Delta.GetRotation().Rotator().Yaw; }
		if (bClipDrivesTranslation) { RootTranslationPending += Delta.GetTranslation() * RootMotionScale; }
	}
	if (BlendAlpha < 1.0f)
	{
		AdvanceTime(FromTime, DeltaSeconds, FromPlayRate, bFromLooping, FromAsset);

		BlendAlpha = FMath::Min(BlendAlpha + DeltaSeconds / BlendDuration, 1.0f);
		if (BlendAlpha >= 1.0f)
		{
			// Blend finished -- drop the reference so Evaluate() takes the
			// cheap single-pose path from here on.
			FromAsset = nullptr;
		}
	}
}

namespace
{
	// Zeroes the root bone's own LOCAL translation in Output, leaving its
	// rotation/scale alone. bExtractRootMotion=true on an FAnimExtractContext
	// turns out NOT to strip translation out of the pose GetAnimationPose
	// hands back -- confirmed live: with that flag set, the rendered head
	// bone still drifted ~4.7m from the actual capsule position by the end
	// of a roll (logged via [RollTick]'s HeadLoc vs Loc), the exact same
	// "travels too far, then snaps back when the blend-out finishes" symptom
	// as before. That flag apparently only signals INTENT to callers that
	// build on top of GetAnimationPose (like the standard AnimGraph Sequence
	// Player node, which does its own explicit root-bone zeroing after
	// calling it) -- it isn't a self-contained "strip root motion" switch.
	// So do that same explicit zeroing ourselves, the same technique
	// ApplyFacialBoneEdits already uses for one bone at a time: this is a
	// no-op for every clip except RollAnim (every other asset already has
	// zero root translation baked in at the FBX level), and for RollAnim it
	// makes the RENDERED pose match what ABaseCharacter::UpdateRollMovement
	// is doing to the actual capsule via ExtractRootMotionFromRange, instead
	// of the mesh separately carrying its own copy of the same motion.
	void ZeroRootBoneTranslation(FPoseContext& Output)
	{
		const FBoneContainer& BoneContainer = Output.Pose.GetBoneContainer();
		const USkeleton* SkeletonAsset = BoneContainer.GetSkeletonAsset();
		const int32 SkeletonBoneIndex = SkeletonAsset ? SkeletonAsset->GetReferenceSkeleton().FindBoneIndex(TEXT("root")) : INDEX_NONE;
		if (SkeletonBoneIndex == INDEX_NONE)
		{
			return;
		}
		const FCompactPoseBoneIndex CompactIndex = BoneContainer.GetCompactPoseIndexFromSkeletonIndex(SkeletonBoneIndex);
		if (!CompactIndex.IsValid())
		{
			return;
		}
		FTransform Bone = Output.Pose[CompactIndex];
		Bone.SetTranslation(FVector::ZeroVector);
		// Rotation too: the RootMotion twins of the turn/start clips carry
		// their yaw in the root track (measured: -90/+90/±180 over the clip);
		// that yaw is applied to the CAPSULE by the character (see
		// ConsumeRootMotion), so the rendered pose must not turn as well.
		// Every other clip's root rotation is the reference anyway.
		Bone.SetRotation(BoneContainer.GetRefPoseTransform(CompactIndex).GetRotation());
		Output.Pose[CompactIndex] = Bone;
	}
}

bool FCharacterAnimInstanceProxy::Evaluate(FPoseContext& Output)
{
	if (!ToAsset)
	{
		Output.ResetToRefPose();
	}
	else
	{
		EnsureRetargetTables(Output.Pose.GetBoneContainer(), ToAsset->GetSkeleton());
		if (!bRetargetActive)
		{
			EvaluatePoseInto(Output);
		}
		else
		{
			// Different rig: evaluate on the CLIP's own skeleton (no engine
			// remap -- see EnsureRetargetTables), then convert to the mesh.
			FPoseContext Source(SourceBoneContainer);
			EvaluatePoseInto(Source);
			Output.ResetToRefPose();
			ConvertSourceToTarget(Source, Output);
		}
	}

	// Post steps, always on the final target pose in the mesh's own bone
	// space: head look-at, facial edits (the eyes-scale blink in particular,
	// which the retarget's rotation/translation-only conversion drops), and
	// the weapon-hand grip curl, which targets the mesh's own finger bones.
	if (!bCachedFullBody)   // a full-body action is seen as authored: no gait, lean or look-at over it
	{
		ApplyGaitAdjustments(Output);   // before look-at so the head compensates a hunch/lean
		ApplySpineLean(Output);         // likewise: the head is aimed AFTER the torso has leaned
		ApplyLookAt(Output);
	}
	ApplyFacialBoneEdits(Output);
	if (!bCachedFullBody) { ApplyWeaponGripCorrection(Output); }
	// IK goes LAST of the arm work. Anything that ran after it would move the hand off the grip
	// it was just placed on, which defeats the entire point of solving for it.
	if (!bCachedFullBody) { ApplyHandIK(Output); }
	if (ToAsset) { GroundFeet(Output); }
	return true;
}

void FCharacterAnimInstanceProxy::GroundFeet(FPoseContext& Output)
{
	// Every clip carries its own idea of where the floor is (measured: the
	// Synty idle keeps the soles ~1.2cm above the mesh origin, the crouch
	// ~0, a jog swings between), and a retarget across proportions adds
	// its own error -- so instead of tuning a mesh offset per clip, pin the
	// lowest toe/ball bone to the rig's own reference sole height while
	// the character is standing on something.
	if (!Owner || !Owner->bGroundFeet) { return; }
	const bool bAirborne = PendingInputs.bIsInAir
		|| CurrentState == ECharacterLocomotionState::JumpRise || CurrentState == ECharacterLocomotionState::JumpFall
		|| CurrentState == ECharacterLocomotionState::Roll || CurrentState == ECharacterLocomotionState::Dash;
	const float TargetShift = bAirborne ? 0.0f : ComputeFootShift(Output);
	// Eased so a blend between clips with different floor heights slides
	// rather than pops.
	GroundShift = FMath::Lerp(GroundShift, TargetShift, 0.25f);
	if (FMath::Abs(GroundShift) < 0.01f) { return; }

	const FBoneContainer& BoneContainer = Output.Pose.GetBoneContainer();
	const USkeleton* SkeletonAsset = BoneContainer.GetSkeletonAsset();
	const int32 PelvisSkeletonIndex = SkeletonAsset ? SkeletonAsset->GetReferenceSkeleton().FindBoneIndex(TEXT("pelvis")) : INDEX_NONE;
	const FCompactPoseBoneIndex Pelvis = PelvisSkeletonIndex != INDEX_NONE ? BoneContainer.GetCompactPoseIndexFromSkeletonIndex(PelvisSkeletonIndex) : FCompactPoseBoneIndex(INDEX_NONE);
	if (!Pelvis.IsValid()) { return; }

	// Move the pelvis (and so the whole body) by -shift in component
	// space, expressed in its parent's (the root's) frame.
	const FCompactPoseBoneIndex Root = Output.Pose.GetParentBoneIndex(Pelvis);
	const FQuat RootRot = Root.IsValid() ? Output.Pose[Root].GetRotation() : FQuat::Identity;
	FTransform Bone = Output.Pose[Pelvis];
	Bone.AddToTranslation(RootRot.Inverse().RotateVector(FVector(0.0f, 0.0f, -GroundShift)));
	Output.Pose[Pelvis] = Bone;
}

// ---- Gait adjustments ---------------------------------------------------------

void FCharacterAnimInstanceProxy::UpdateGaitTiming(float DeltaSeconds)
{
	const FGaitAdjustments& G = CachedGait;
	if (!bGaitNoiseSeeded)
	{
		// A random offset into the noise so a crowd sharing one config
		// doesn't stumble in lockstep.
		GaitNoiseTime = FMath::FRandRange(0.0f, 1000.0f);
		bGaitNoiseSeeded = true;
	}
	GaitNoiseTime += DeltaSeconds;

	// Stumble: a jittery play rate that re-targets every few tenths of a
	// second, with the occasional short hitch (the foot "catching").
	float Jitter = 1.0f;
	if (G.Stumble > 0.0f && bGaitMoving)
	{
		StumbleRetargetIn -= DeltaSeconds;
		if (StumbleRetargetIn <= 0.0f)
		{
			StumbleRetargetIn = FMath::FRandRange(0.2f, 0.6f);
			StumbleRateTarget = 1.0f + FMath::FRandRange(-0.35f, 0.35f) * G.Stumble;
			if (FMath::FRand() < 0.35f * G.Stumble) { StumbleHitchLeft = FMath::FRandRange(0.08f, 0.18f); }
		}
		StumbleHitchLeft = FMath::Max(0.0f, StumbleHitchLeft - DeltaSeconds);
		Jitter = StumbleRateTarget * (StumbleHitchLeft > 0.0f ? 0.3f : 1.0f);
	}
	else
	{
		StumbleRateTarget = 1.0f;
		StumbleHitchLeft = 0.0f;
	}

	// Limp: hurry through the weak leg's stance, linger on the good one.
	// Which foot is planted comes from the pose itself (bStanceLeft, written
	// in Evaluate), so this works on any clip without phase markers.
	float Limp = 1.0f;
	if (G.LimpAmount > 0.0f && bGaitMoving && bStanceKnown)
	{
		const bool bWeakLeft = G.LimpSide != TEXT("Right");
		const float Target = (bStanceLeft == bWeakLeft) ? 1.0f : -1.0f;
		LimpRateBlend = FMath::FInterpTo(LimpRateBlend, Target, DeltaSeconds, 14.0f);
		Limp = 1.0f + 0.55f * FMath::Clamp(G.LimpAmount, 0.0f, 1.0f) * LimpRateBlend;
	}
	else
	{
		LimpRateBlend = FMath::FInterpTo(LimpRateBlend, 0.0f, DeltaSeconds, 8.0f);
	}

	const float Target = FMath::Clamp(G.CadenceScale, 0.25f, 3.0f) * Jitter * Limp;
	GaitRateMultiplier = FMath::FInterpTo(GaitRateMultiplier, Target, DeltaSeconds, 20.0f);
}

void FCharacterAnimInstanceProxy::ApplyGaitAdjustments(FPoseContext& Output)
{
	const FGaitAdjustments& G = CachedGait;
	// Authored poses (the splash poses) are left exactly as authored; the
	// running averages below keep tracking regardless so nothing pops when
	// the state comes back.
	const bool bIdentity = G.IsIdentity();
	if (CurrentState == ECharacterLocomotionState::Pose) { return; }

	FCompactPose& Pose = Output.Pose;
	const FBoneContainer& BC = Pose.GetBoneContainer();
	const USkeleton* Skel = BC.GetSkeletonAsset();
	if (!Skel) { return; }
	const FReferenceSkeleton& RefSkel = Skel->GetReferenceSkeleton();

	const auto Find = [&](const TCHAR* Name) -> FCompactPoseBoneIndex
	{
		const int32 SkeletonIndex = RefSkel.FindBoneIndex(Name);
		return SkeletonIndex == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : BC.GetCompactPoseIndexFromSkeletonIndex(SkeletonIndex);
	};
	// Component-space transform by walking up the hierarchy -- only ever a
	// handful of bones, each a few levels deep.
	const auto CS = [&](FCompactPoseBoneIndex Index) -> FTransform
	{
		FTransform T = FTransform::Identity;
		while (Index.IsValid()) { T = T * Pose[Index]; Index = Pose.GetParentBoneIndex(Index); }
		return T;
	};
	const auto ParentCSRot = [&](FCompactPoseBoneIndex Index) -> FQuat
	{
		const FCompactPoseBoneIndex Parent = Pose.GetParentBoneIndex(Index);
		return Parent.IsValid() ? CS(Parent).GetRotation() : FQuat::Identity;
	};
	// Rotate a bone's component-space orientation by DeltaCS; children follow.
	const auto RotateCS = [&](FCompactPoseBoneIndex Index, const FQuat& DeltaCS)
	{
		if (!Index.IsValid()) { return; }
		const FQuat P = ParentCSRot(Index);
		FTransform Local = Pose[Index];
		Local.SetRotation(P.Inverse() * DeltaCS * P * Local.GetRotation());
		Local.NormalizeRotation();
		Pose[Index] = Local;
	};
	// Set a bone's local rotation so its component-space orientation becomes TargetCS.
	const auto SetCSRotation = [&](FCompactPoseBoneIndex Index, const FQuat& TargetCS)
	{
		if (!Index.IsValid()) { return; }
		FTransform Local = Pose[Index];
		Local.SetRotation(ParentCSRot(Index).Inverse() * TargetCS);
		Local.NormalizeRotation();
		Pose[Index] = Local;
	};
	const auto TranslateCS = [&](FCompactPoseBoneIndex Index, const FVector& DeltaCS)
	{
		if (!Index.IsValid()) { return; }
		FTransform Local = Pose[Index];
		Local.AddToTranslation(ParentCSRot(Index).UnrotateVector(DeltaCS));
		Pose[Index] = Local;
	};
	// Sign of the rotation about Axis (applied at Pivot) that moves Point
	// along Toward -- so nothing here depends on the rig's axis conventions.
	const auto SignToward = [](const FVector& Axis, const FVector& Pivot, const FVector& Point, const FVector& Toward) -> float
	{
		const FVector Arm = Point - Pivot;
		const FVector Moved = FQuat(Axis, FMath::DegreesToRadians(10.0f)).RotateVector(Arm);
		return FVector::DotProduct(Moved - Arm, Toward) >= 0.0f ? 1.0f : -1.0f;
	};

	const FCompactPoseBoneIndex Pelvis = Find(TEXT("pelvis"));
	const FCompactPoseBoneIndex ThighL = Find(TEXT("thigh_l")), ThighR = Find(TEXT("thigh_r"));
	const FCompactPoseBoneIndex CalfL = Find(TEXT("calf_l")), CalfR = Find(TEXT("calf_r"));
	const FCompactPoseBoneIndex FootL = Find(TEXT("foot_l")), FootR = Find(TEXT("foot_r"));
	const FCompactPoseBoneIndex BallL = Find(TEXT("ball_l")), BallR = Find(TEXT("ball_r"));
	const FCompactPoseBoneIndex Spine[3] = { Find(TEXT("spine_01")), Find(TEXT("spine_02")), Find(TEXT("spine_03")) };
	const FCompactPoseBoneIndex Neck = Find(TEXT("neck_01"));
	const FCompactPoseBoneIndex Head = Find(TEXT("head"));
	const FCompactPoseBoneIndex Arms[2] = { Find(TEXT("upperarm_l")), Find(TEXT("upperarm_r")) };
	if (!Pelvis.IsValid() || !ThighL.IsValid() || !ThighR.IsValid()) { return; }

	// Frame from the rig itself: facing (cached from the actor), up, and
	// "left" as the hip line.
	const FVector Up = FVector::UpVector;
	FVector Facing = CachedFacingComponent; Facing.Z = 0.0f;
	if (!Facing.Normalize()) { Facing = FVector(0.0f, 1.0f, 0.0f); }
	FVector LeftDir = CS(ThighL).GetTranslation() - CS(ThighR).GetTranslation(); LeftDir.Z = 0.0f;
	if (!LeftDir.Normalize()) { LeftDir = FVector::CrossProduct(Up, Facing); }
	const FVector RightDir = -LeftDir;
	const float Dt = FMath::Clamp(CachedDeltaSeconds, 0.0f, 0.1f);

	// Which foot bears weight (lower foot), eased into -1..1 for the pose
	// terms and handed to the game thread for the limp timing.
	if (FootL.IsValid() && FootR.IsValid())
	{
		const float ZL = CS(FootL).GetTranslation().Z;
		const float ZR = CS(FootR).GetTranslation().Z;
		if (FMath::Abs(ZL - ZR) > 0.75f) { bStanceLeft = ZL < ZR; bStanceKnown = true; }
	}
	const float StanceTarget = (bGaitMoving && bStanceKnown) ? (bStanceLeft ? 1.0f : -1.0f) : 0.0f;
	StanceBlend = FMath::FInterpTo(StanceBlend, StanceTarget, Dt, 10.0f);
	GaitMovingBlend = FMath::FInterpTo(GaitMovingBlend, bGaitMoving ? 1.0f : 0.0f, Dt, 6.0f);

	// Running baselines (tracked even when nothing is adjusted).
	const FVector PelvisPos = CS(Pelvis).GetTranslation();
	if (!bPelvisZAverageValid) { PelvisZAverage = PelvisPos.Z; bPelvisZAverageValid = true; }
	else { PelvisZAverage = FMath::Lerp(PelvisZAverage, static_cast<float>(PelvisPos.Z), FMath::Clamp(Dt / 0.8f, 0.0f, 1.0f)); }
	for (int32 Side = 0; Side < 2; ++Side)
	{
		if (!Arms[Side].IsValid()) { continue; }
		const FQuat Anim = Pose[Arms[Side]].GetRotation();
		if (!bArmSwingAverageValid[Side]) { ArmSwingAverage[Side] = Anim; bArmSwingAverageValid[Side] = true; }
		else
		{
			const float Alpha = FMath::Clamp(Dt / 0.8f, 0.0f, 1.0f);
			ArmSwingAverage[Side] = FQuat::FastLerp(ArmSwingAverage[Side], Anim, Alpha).GetNormalized();
		}
	}
	if (bIdentity) { return; }

	const bool bWeakLeft = G.LimpSide != TEXT("Right");
	const float OnWeakLeg = FMath::Max(0.0f, StanceBlend * (bWeakLeft ? 1.0f : -1.0f));   // 0..1 while the weak leg is planted
	const float LimpAmount = FMath::Clamp(G.LimpAmount, 0.0f, 1.0f);
	const float Stumble = (bGaitMoving ? FMath::Clamp(G.Stumble, 0.0f, 1.0f) : 0.0f);

	// ---- Stance width: swing each leg outward at the hip, then put the
	// foot back to the orientation it had so the sole stays level.
	const float StanceWidth = G.StanceWidthDegrees + G.MovingStanceWidthDegrees * GaitMovingBlend;
	if (StanceWidth != 0.0f && CalfL.IsValid() && CalfR.IsValid())
	{
		const auto Abduct = [&](FCompactPoseBoneIndex Thigh, FCompactPoseBoneIndex Calf, FCompactPoseBoneIndex Foot, const FVector& Outward)
		{
			const FVector ThighPos = CS(Thigh).GetTranslation();
			const FQuat FootCS = Foot.IsValid() ? CS(Foot).GetRotation() : FQuat::Identity;
			const float Sign = SignToward(Facing, ThighPos, CS(Calf).GetTranslation(), Outward);
			RotateCS(Thigh, FQuat(Facing, Sign * FMath::DegreesToRadians(StanceWidth)));
			if (Foot.IsValid()) { SetCSRotation(Foot, FootCS); }
		};
		Abduct(ThighL, CalfL, FootL, LeftDir);
		Abduct(ThighR, CalfR, FootR, RightDir);
	}

	// ---- Toe out: yaw each foot about vertical, toes outward.
	if (G.ToeOutDegrees != 0.0f && FootL.IsValid() && FootR.IsValid())
	{
		const auto TurnOut = [&](FCompactPoseBoneIndex Foot, FCompactPoseBoneIndex Ball, const FVector& Outward)
		{
			const FVector FootPos = CS(Foot).GetTranslation();
			const FVector ToePos = Ball.IsValid() ? CS(Ball).GetTranslation() : FootPos + Facing * 10.0f;
			const float Sign = SignToward(Up, FootPos, ToePos, Outward);
			RotateCS(Foot, FQuat(Up, Sign * FMath::DegreesToRadians(G.ToeOutDegrees)));
		};
		TurnOut(FootL, BallL, LeftDir);
		TurnOut(FootR, BallR, RightDir);
	}

	// ---- Spine: hunch (forward), lean (sideways), stumble wobble, limp lean.
	{
		float Pitch = G.HunchDegrees;
		float Lean = G.LeanDegrees;   // + toward the character's right
		if (Stumble > 0.0f)
		{
			Lean += 7.0f * Stumble * FMath::PerlinNoise1D(GaitNoiseTime * 1.7f);
			Pitch += 5.0f * Stumble * FMath::PerlinNoise1D(GaitNoiseTime * 1.3f + 37.0f);
		}
		if (LimpAmount > 0.0f) { Lean += (bWeakLeft ? -1.0f : 1.0f) * 6.0f * LimpAmount * OnWeakLeg; }

		if (Spine[0].IsValid() && (Pitch != 0.0f || Lean != 0.0f))
		{
			const FVector SpinePos = CS(Spine[0]).GetTranslation();
			const FVector HeadPos = Head.IsValid() ? CS(Head).GetTranslation() : SpinePos + Up * 50.0f;
			const float PitchSign = SignToward(LeftDir, SpinePos, HeadPos, Facing);
			const float LeanSign = SignToward(Facing, SpinePos, HeadPos, RightDir);
			int32 NumSpine = 0;
			for (const FCompactPoseBoneIndex& S : Spine) { if (S.IsValid()) { ++NumSpine; } }
			for (const FCompactPoseBoneIndex& S : Spine)
			{
				if (!S.IsValid()) { continue; }
				RotateCS(S, FQuat(LeftDir, PitchSign * FMath::DegreesToRadians(Pitch / NumSpine)) * FQuat(Facing, LeanSign * FMath::DegreesToRadians(Lean / NumSpine)));
			}
			// The neck takes back half the hunch so the character still
			// looks where it is going.
			if (Neck.IsValid() && G.HunchDegrees != 0.0f)
			{
				RotateCS(Neck, FQuat(LeftDir, -PitchSign * FMath::DegreesToRadians(G.HunchDegrees * 0.5f)));
			}
		}
	}

	// ---- Arm swing: scale each upper arm's motion about its own running
	// mean (not the reference pose -- on the modular hero that is a wide
	// rest pose, and "half the swing" must not mean "half way to it").
	if (G.ArmSwingScale != 1.0f)
	{
		const float Scale = FMath::Clamp(G.ArmSwingScale, 0.0f, 3.0f);
		for (int32 Side = 0; Side < 2; ++Side)
		{
			if (!Arms[Side].IsValid() || !bArmSwingAverageValid[Side]) { continue; }
			FTransform Local = Pose[Arms[Side]];
			FQuat Delta = ArmSwingAverage[Side].Inverse() * Local.GetRotation();
			Delta.Normalize();
			FVector Axis; float Angle;
			Delta.ToAxisAndAngle(Axis, Angle);
			if (Angle > PI) { Angle -= 2.0f * PI; }
			Local.SetRotation((ArmSwingAverage[Side] * FQuat(Axis, Angle * Scale)).GetNormalized());
			Pose[Arms[Side]] = Local;
		}
	}

	// ---- Torso bob (bounce, limp dip) goes on spine_01 rather than the
	// pelvis so the feet stay planted (GroundFeet re-pins the soles after
	// this, and would otherwise cancel a pelvis-level bounce).
	{
		FVector TorsoShift = FVector::ZeroVector;
		if (G.BounceScale != 1.0f)
		{
			TorsoShift += Up * ((static_cast<float>(PelvisPos.Z) - PelvisZAverage) * (FMath::Clamp(G.BounceScale, 0.0f, 4.0f) - 1.0f));
		}
		if (LimpAmount > 0.0f) { TorsoShift -= Up * (4.0f * LimpAmount * OnWeakLeg); }
		if (!TorsoShift.IsNearlyZero()) { TranslateCS(Spine[0].IsValid() ? Spine[0] : Pelvis, TorsoShift); }
	}

	// ---- Hips: sway toward the planted foot (shift + roll), stumble drift.
	{
		FVector HipShift = FVector::ZeroVector;
		const float Sway = FMath::Clamp(G.Sway, 0.0f, 1.0f);
		if (Sway > 0.0f) { HipShift += LeftDir * (2.5f * Sway * StanceBlend); }
		if (Stumble > 0.0f) { HipShift += LeftDir * (3.0f * Stumble * FMath::PerlinNoise1D(GaitNoiseTime * 2.3f + 11.0f)); }
		if (!HipShift.IsNearlyZero()) { TranslateCS(Pelvis, HipShift); }

		if (Sway > 0.0f && FMath::Abs(StanceBlend) > 0.01f)
		{
			// The swing-side hip drops: rotate the pelvis about the facing
			// axis so the unweighted thigh moves down.
			const FCompactPoseBoneIndex SwingThigh = StanceBlend > 0.0f ? ThighR : ThighL;
			const float Sign = SignToward(Facing, CS(Pelvis).GetTranslation(), CS(SwingThigh).GetTranslation(), -Up);
			RotateCS(Pelvis, FQuat(Facing, Sign * FMath::DegreesToRadians(4.0f * Sway * FMath::Abs(StanceBlend))));
		}
	}
}

float FCharacterAnimInstanceProxy::ComputeFootShift(const FPoseContext& Output) const
{
	const FBoneContainer& BoneContainer = Output.Pose.GetBoneContainer();
	const USkeleton* SkeletonAsset = BoneContainer.GetSkeletonAsset();
	if (!SkeletonAsset) { return 0.0f; }

	// Component-space heights of the sole bones, animated and in the
	// reference pose (walking up the hierarchy for just these few bones).
	static const FName SoleBoneNames[] = { TEXT("toes_l"), TEXT("toes_r"), TEXT("ball_l"), TEXT("ball_r") };
	float MinAnimZ = TNumericLimits<float>::Max();
	float MinRefZ = TNumericLimits<float>::Max();
	for (const FName& Name : SoleBoneNames)
	{
		const int32 SkeletonIndex = SkeletonAsset->GetReferenceSkeleton().FindBoneIndex(Name);
		if (SkeletonIndex == INDEX_NONE) { continue; }
		FCompactPoseBoneIndex Index = BoneContainer.GetCompactPoseIndexFromSkeletonIndex(SkeletonIndex);
		if (!Index.IsValid()) { continue; }
		FTransform AnimCS = FTransform::Identity;
		FTransform RefCS = FTransform::Identity;
		while (Index.IsValid())
		{
			AnimCS = AnimCS * Output.Pose[Index];
			RefCS = RefCS * BoneContainer.GetRefPoseTransform(Index);
			Index = Output.Pose.GetParentBoneIndex(Index);
		}
		MinAnimZ = FMath::Min(MinAnimZ, static_cast<float>(AnimCS.GetTranslation().Z));
		MinRefZ = FMath::Min(MinRefZ, static_cast<float>(RefCS.GetTranslation().Z));
	}
	if (MinAnimZ == TNumericLimits<float>::Max()) { return 0.0f; }
	return FMath::Clamp(MinAnimZ - MinRefZ, -Owner->GroundFeetMaxShift, Owner->GroundFeetMaxShift);
}

void FCharacterAnimInstanceProxy::EvaluatePoseInto(FPoseContext& Ctx)
{
	if (BlendAlpha >= 1.0f || !FromAsset)
	{
		FAnimationPoseData OutputPoseData(Ctx);
		FDeltaTimeRecord ToDeltaTimeRecord;
		FAnimExtractContext ExtractContext(static_cast<double>(ToTime), true, ToDeltaTimeRecord, bToLooping);
		ToAsset->GetAnimationPose(OutputPoseData, ExtractContext);
	}
	else
	{
		FCompactPose FromPose;
		FCompactPose ToPose;
		FBlendedCurve FromCurve;
		FBlendedCurve ToCurve;
		UE::Anim::FStackAttributeContainer FromAttributes;
		UE::Anim::FStackAttributeContainer ToAttributes;
		FromPose.SetBoneContainer(&Ctx.Pose.GetBoneContainer());
		ToPose.SetBoneContainer(&Ctx.Pose.GetBoneContainer());
		FromCurve.InitFrom(Ctx.Curve);
		ToCurve.InitFrom(Ctx.Curve);

		FAnimationPoseData FromPoseData(FromPose, FromCurve, FromAttributes);
		FAnimationPoseData ToPoseData(ToPose, ToCurve, ToAttributes);

		FDeltaTimeRecord FromDeltaTimeRecord;
		FAnimExtractContext FromExtractContext(static_cast<double>(FromTime), true, FromDeltaTimeRecord, bFromLooping);
		FromAsset->GetAnimationPose(FromPoseData, FromExtractContext);

		FDeltaTimeRecord ToDeltaTimeRecord;
		FAnimExtractContext ToExtractContext(static_cast<double>(ToTime), true, ToDeltaTimeRecord, bToLooping);
		ToAsset->GetAnimationPose(ToPoseData, ToExtractContext);

		// WeightOfPoseOne is the weight given to the FIRST pose argument (From)
		// -- so as BlendAlpha rises from 0 (fully From) to 1 (fully To), From's
		// weight is (1 - BlendAlpha).
		FAnimationPoseData OutputPoseData(Ctx);
		FAnimationRuntime::BlendTwoPosesTogether(FromPoseData, ToPoseData, 1.0f - BlendAlpha, OutputPoseData);
	}

	ZeroRootBoneTranslation(Ctx);
	// The full-body arm-override layer belongs here (in the clip's own bone
	// space on the retarget path, so it converts along with everything else).
	// Look-at, facial edits and grip are NOT applied here -- they run on the
	// final target pose in Evaluate(), so the eyes-scale blink survives the
	// rotation/translation-only retarget conversion and every face/hand edit
	// is expressed in the mesh's own bone space.
	ApplyArmOverride(Ctx);
}

// ---- Orientation retarget ---------------------------------------------------

void FCharacterAnimInstanceProxy::EnsureRetargetTables(const FBoneContainer& BoneContainer, const USkeleton* AnimSkeleton)
{
	const USkeleton* MeshSkeleton = BoneContainer.GetSkeletonAsset();
	const int32 NumBones = BoneContainer.GetCompactPoseNumBones();
	if (MeshSkeleton == RetargetTargetSkeleton && AnimSkeleton == RetargetSourceSkeleton && NumBones == RetargetNumBones)
	{
		return;
	}
	RetargetTargetSkeleton = MeshSkeleton;
	RetargetSourceSkeleton = AnimSkeleton;
	RetargetNumBones = NumBones;
	bRetargetActive = AnimSkeleton && MeshSkeleton && AnimSkeleton != MeshSkeleton;
	if (!bRetargetActive)
	{
		return;
	}

	// A full bone container on the clip's own skeleton. Evaluating against
	// it means no compatible-skeleton remapping runs at all -- the engine's
	// remap corrected the reference-pose difference once in decompression
	// and once more in the editor's raw-data path, laying the body flat
	// (root came out at 2 x 90 degrees). Doing the conversion here, once,
	// keeps PIE and packaged builds identical.
	const FReferenceSkeleton& SourceRef = AnimSkeleton->GetReferenceSkeleton();
	TArray<FBoneIndexType> AllSourceBones;
	AllSourceBones.SetNumUninitialized(SourceRef.GetNum());
	for (int32 i = 0; i < SourceRef.GetNum(); ++i) { AllSourceBones[i] = static_cast<FBoneIndexType>(i); }
	SourceBoneContainer.InitializeTo(AllSourceBones, UE::Anim::FCurveFilterSettings(), *AnimSkeleton);

	const int32 NumSource = SourceBoneContainer.GetCompactPoseNumBones();
	SourceRefCS.SetNum(NumSource);
	SourceRefTranslation.SetNum(NumSource);

	// The source reference pose comes straight from the clip skeleton's own
	// reference skeleton, NOT from SourceBoneContainer.GetRefPoseTransform:
	// on a bone container built via InitializeTo against a bare skeleton (not
	// a mesh), that accessor returns identity transforms, which silently
	// collapsed every RetargetDelta to just TargetRefCS -- double-applying the
	// target's A-pose and splaying every retargeted character into a
	// scarecrow "T-pose". Accumulate the real ref pose in skeleton-index order
	// (guaranteed parent-before-child) and scatter it into compact slots.
	const TArray<FTransform>& SourceRefLocal = SourceRef.GetRefBonePose();
	TArray<FQuat> SourceSkelCS;
	SourceSkelCS.SetNum(SourceRef.GetNum());
	for (int32 b = 0; b < SourceRef.GetNum(); ++b)
	{
		const int32 ParentSkel = SourceRef.GetParentIndex(b);
		SourceSkelCS[b] = (ParentSkel != INDEX_NONE ? SourceSkelCS[ParentSkel] : FQuat::Identity) * SourceRefLocal[b].GetRotation();
		const FCompactPoseBoneIndex Compact = SourceBoneContainer.GetCompactPoseIndexFromSkeletonIndex(b);
		if (Compact.IsValid())
		{
			SourceRefCS[Compact.GetInt()] = SourceSkelCS[b];
			SourceRefTranslation[Compact.GetInt()] = SourceRefLocal[b].GetTranslation();
		}
	}

	// Target reference pose, and the per-bone delta between the two rigs' bone
	// axes: TargetRefCS = SourceRefCS * Delta. Like the source above, this
	// comes from the target SKELETON's reference pose, NOT the mesh bind pose
	// (BoneContainer.GetRefPoseTransform): a Synty mesh's bind pose can differ
	// from its skeleton's reference pose by tens of degrees at the shoulder,
	// and taking the source ref from the skeleton but the target ref from the
	// mesh bind made every delta a spurious ~47deg rotation -- even between two
	// copies of the identical Mannequin skeleton, which should retarget 1:1 --
	// splaying the arms. Translations still come from the mesh (bone lengths).
	const FReferenceSkeleton& MeshSkelRef = MeshSkeleton->GetReferenceSkeleton();
	const TArray<FTransform>& MeshRefLocal = MeshSkelRef.GetRefBonePose();
	TArray<FQuat> MeshSkelCS;
	MeshSkelCS.SetNum(MeshSkelRef.GetNum());
	for (int32 b = 0; b < MeshSkelRef.GetNum(); ++b)
	{
		const int32 ParentSkel = MeshSkelRef.GetParentIndex(b);
		MeshSkelCS[b] = (ParentSkel != INDEX_NONE ? MeshSkelCS[ParentSkel] : FQuat::Identity) * MeshRefLocal[b].GetRotation();
	}

	// Per source bone, the direction to its FIRST child in the bone's own
	// local frame -- the bone's "length" axis, used as the twist axis below.
	// Bones with no child (leaves) are left at the default and get an identity
	// delta (their own orientation copied straight across).
	const TArray<FTransform>& SourceRefBonePose = SourceRef.GetRefBonePose();
	TArray<FVector> SourceChildDir;
	SourceChildDir.Init(FVector::ForwardVector, SourceRef.GetNum());
	TArray<bool> SourceHasChild;
	SourceHasChild.Init(false, SourceRef.GetNum());
	for (int32 b = 0; b < SourceRef.GetNum(); ++b)
	{
		const int32 ParentIdx = SourceRef.GetParentIndex(b);
		if (ParentIdx != INDEX_NONE && !SourceHasChild[ParentIdx])
		{
			SourceHasChild[ParentIdx] = true;
			const FVector Dir = SourceRefBonePose[b].GetTranslation();
			if (!Dir.IsNearlyZero()) { SourceChildDir[ParentIdx] = Dir.GetSafeNormal(); }
		}
	}
	// Twist (roll) component of Q about Axis, discarding the swing.
	auto ExtractTwist = [](const FQuat& Q, const FVector& Axis) -> FQuat
	{
		const FVector R(Q.X, Q.Y, Q.Z);
		const FVector Proj = FVector::DotProduct(R, Axis) * Axis;
		FQuat Twist(Proj.X, Proj.Y, Proj.Z, Q.W);
		if (Twist.SizeSquared() < UE_SMALL_NUMBER) { return FQuat::Identity; }
		Twist.Normalize();
		return Twist;
	};

	const FReferenceSkeleton& TargetRef = BoneContainer.GetReferenceSkeleton();
	TArray<FQuat> TargetRefCS;
	TargetRefCS.SetNum(NumBones);
	RetargetDelta.SetNum(NumBones);
	RetargetCurlAxis.SetNum(NumBones);
	TargetToSourceCompact.SetNum(NumBones);
	RetargetTargetRefTranslation.SetNum(NumBones);
	RetargetPelvisCompactIndex = INDEX_NONE;
	int32 NumMapped = 0;
	for (int32 i = 0; i < NumBones; ++i)
	{
		const FCompactPoseBoneIndex Compact(i);
		const FTransform& TargetLocal = BoneContainer.GetRefPoseTransform(Compact);
		const FCompactPoseBoneIndex Parent = BoneContainer.GetParentBoneIndex(Compact);
		RetargetTargetRefTranslation[i] = TargetLocal.GetTranslation();

		const FName BoneName = TargetRef.GetBoneName(BoneContainer.MakeMeshPoseIndex(Compact).GetInt());
		// Component-space ref rotation from the skeleton (fall back to the mesh
		// bind accumulation only if the name isn't in the skeleton).
		const int32 MeshSkelIndex = MeshSkelRef.FindBoneIndex(BoneName);
		TargetRefCS[i] = MeshSkelIndex != INDEX_NONE
			? MeshSkelCS[MeshSkelIndex]
			: (Parent.IsValid() ? TargetRefCS[Parent.GetInt()] : FQuat::Identity) * TargetLocal.GetRotation();

		const int32 SourceSkeletonIndex = SourceRef.FindBoneIndex(BoneName);   // FName compare: case-insensitive
		const FCompactPoseBoneIndex SourceCompact = SourceSkeletonIndex != INDEX_NONE
			? SourceBoneContainer.GetCompactPoseIndexFromSkeletonIndex(SourceSkeletonIndex)
			: FCompactPoseBoneIndex(INDEX_NONE);
		TargetToSourceCompact[i] = SourceCompact.IsValid() ? SourceCompact.GetInt() : INDEX_NONE;

		// The full reference-pose difference between the two rigs at this bone.
		// We keep only its TWIST about the bone's length axis (the rigs' bone-
		// roll/axis convention) and drop the SWING (the difference in rest-pose
		// direction, e.g. the modular hero's wide-resting arms) so the animation
		// supplies the actual limb direction. Leaf bones (no child, so no length
		// axis) copy the source orientation directly (identity delta).
		const FQuat FullDelta = SourceCompact.IsValid() ? SourceRefCS[SourceCompact.GetInt()].Inverse() * TargetRefCS[i] : FQuat::Identity;
		const bool bHasChild = (SourceSkeletonIndex != INDEX_NONE) && SourceHasChild[SourceSkeletonIndex];
		// Default is the FULL reference-pose delta -- a plain orientation
		// retarget that transfers each rig's bone-axis convention and is correct
		// wherever the two rigs' rest poses point the same way (spine, legs,
		// feet, head, hands, and the entire same-skeleton Mannequin family,
		// where the delta is identity).
		//
		// The ONE place the rest poses genuinely disagree is the modular hero's
		// ARMS: they rest far wider (an open A-pose) than the animations assume,
		// so the full delta bakes that width in as a permanent T-pose. For the
		// arm chain only, keep just the TWIST of the delta (the bone-roll/axis
		// part) and drop its SWING (the rest-direction width), letting the
		// animation supply the actual arm direction. Restricting this to the arm
		// bones avoids mis-rolling bones (feet, head) whose child axis isn't
		// their real length axis.
		const FString BoneNameStr = BoneName.ToString();
		const bool bArmChain =
			BoneNameStr.Contains(TEXT("upperarm"), ESearchCase::IgnoreCase) ||
			BoneNameStr.Contains(TEXT("lowerarm"), ESearchCase::IgnoreCase) ||
			BoneNameStr.Contains(TEXT("clavicle"), ESearchCase::IgnoreCase);
		RetargetDelta[i] = (SourceCompact.IsValid() && Parent.IsValid() && bHasChild && bArmChain)
			? ExtractTwist(FullDelta, SourceChildDir[SourceSkeletonIndex])
			: FullDelta;
		if (SourceCompact.IsValid()) { ++NumMapped; }
		if (BoneName.IsEqual(TEXT("pelvis"), ENameCase::IgnoreCase)) { RetargetPelvisCompactIndex = i; }

		// Grip curl axis: the Mannequin finger hinge (local Z) carried onto
		// this bone's axes through the FULL delta (the curl needs the real axis
		// mapping, not the twist-only retarget delta). Fingers the clip skeleton
		// names differently are aliased for this purpose only (the modular
		// hero's indexFinger_N / finger_N are its index / middle).
		FQuat CurlDelta = FullDelta;
		if (!SourceCompact.IsValid())
		{
			FString Alias = BoneName.ToString();
			if (Alias.StartsWith(TEXT("indexFinger"), ESearchCase::IgnoreCase)) { Alias = TEXT("index") + Alias.RightChop(11); }
			else if (Alias.StartsWith(TEXT("finger_"), ESearchCase::IgnoreCase)) { Alias = TEXT("middle_") + Alias.RightChop(7); }
			const int32 AliasSkeletonIndex = SourceRef.FindBoneIndex(*Alias);
			const FCompactPoseBoneIndex AliasCompact = AliasSkeletonIndex != INDEX_NONE
				? SourceBoneContainer.GetCompactPoseIndexFromSkeletonIndex(AliasSkeletonIndex)
				: FCompactPoseBoneIndex(INDEX_NONE);
			CurlDelta = AliasCompact.IsValid() ? SourceRefCS[AliasCompact.GetInt()].Inverse() * TargetRefCS[i] : FQuat::Identity;
		}
		RetargetCurlAxis[i] = CurlDelta.Inverse().RotateVector(FVector::UpVector);
	}
	UE_LOG(LogTemp, Log, TEXT("CharacterAnimInstance: orientation retarget %s -> %s (%d of %d bones mapped, pelvis=%d)"),
		*AnimSkeleton->GetName(), *MeshSkeleton->GetName(), NumMapped, NumBones, RetargetPelvisCompactIndex);
}

void FCharacterAnimInstanceProxy::ConvertSourceToTarget(const FPoseContext& Source, FPoseContext& Output)
{
	const int32 NumSource = Source.Pose.GetNumBones();
	const int32 NumTarget = Output.Pose.GetNumBones();
	if (NumTarget != RetargetNumBones || NumSource != SourceRefCS.Num()) { return; }

	// Component-space rotations of the evaluated source pose.
	TArray<FQuat> SourceCS;
	SourceCS.SetNum(NumSource);
	for (FCompactPoseBoneIndex Index : Source.Pose.ForEachBoneIndex())
	{
		const FCompactPoseBoneIndex Parent = Source.Pose.GetParentBoneIndex(Index);
		SourceCS[Index.GetInt()] = (Parent.IsValid() ? SourceCS[Parent.GetInt()] : FQuat::Identity) * Source.Pose[Index].GetRotation();
	}

	// Each mapped target bone takes the source bone's world-space orientation,
	// composed with the per-bone RetargetDelta -- which is the TWIST-ONLY part
	// of the two rigs' reference-pose difference (see EnsureRetargetTables).
	// Keeping only the twist (roll about the bone) transfers the rigs' differing
	// bone-axis conventions while DROPPING the swing (direction) part, i.e. the
	// difference in rest-pose shape: that is what lets the modular hero -- whose
	// arms rest much wider than the animation's -- take the animation's actual
	// arm direction instead of splaying into its own wide rest pose, while the
	// legs/spine still get their real axis correction.
	TArray<FQuat> TargetCS;
	TargetCS.SetNum(NumTarget);
	for (FCompactPoseBoneIndex Index : Output.Pose.ForEachBoneIndex())
	{
		const int32 i = Index.GetInt();
		const FCompactPoseBoneIndex Parent = Output.Pose.GetParentBoneIndex(Index);
		const FQuat ParentTargetCS = Parent.IsValid() ? TargetCS[Parent.GetInt()] : FQuat::Identity;
		FTransform Bone = Output.Pose[Index];

		const int32 s = TargetToSourceCompact[i];
		if (s == INDEX_NONE)
		{
			// A bone the clip skeleton lacks (the hero's extra fingers): keep
			// its reference-pose local rotation under its converted parent.
			TargetCS[i] = ParentTargetCS * Bone.GetRotation();
			continue;
		}

		TargetCS[i] = SourceCS[s] * RetargetDelta[i];
		Bone.SetRotation(ParentTargetCS.Inverse() * TargetCS[i]);

		if (i == 0)
		{
			Bone.SetTranslation(FVector::ZeroVector);
		}
		else if (i == RetargetPelvisCompactIndex)
		{
			// Hip motion: the source pelvis's offset from ITS reference
			// position, carried across in component space and scaled by the
			// rigs' pelvis heights so a crouch that drops the Mannequin's
			// 96.8cm pelvis ~42cm only drops this rig's 87.6cm one ~38cm
			// instead of sinking the feet.
			const FCompactPoseBoneIndex SourceCompact(s);
			const FCompactPoseBoneIndex SourceParent = Source.Pose.GetParentBoneIndex(SourceCompact);
			const FQuat SourceParentCS = SourceParent.IsValid() ? SourceCS[SourceParent.GetInt()] : FQuat::Identity;
			const float SourceLen = SourceRefTranslation[s].Size();
			const float Scale = SourceLen > UE_KINDA_SMALL_NUMBER ? RetargetTargetRefTranslation[i].Size() / SourceLen : 1.0f;
			const FVector CSDelta = SourceParentCS.RotateVector(Source.Pose[SourceCompact].GetTranslation() - SourceRefTranslation[s]) * Scale;
			Bone.SetTranslation(RetargetTargetRefTranslation[i] + ParentTargetCS.Inverse().RotateVector(CSDelta));
		}
		else
		{
			Bone.SetTranslation(RetargetTargetRefTranslation[i]);
		}
		Output.Pose[Index] = Bone;
	}
}

void FCharacterAnimInstanceProxy::ApplyArmOverride(FPoseContext& Output)
{
	if (ArmOverrideAlpha <= 0.0f || !CachedArmOverridePose || CachedArmOverrideRootBones.Num() == 0)
	{
		return;
	}

	const FBoneContainer& BoneContainer = Output.Pose.GetBoneContainer();
	const USkeleton* SkeletonAsset = BoneContainer.GetSkeletonAsset();
	if (!SkeletonAsset)
	{
		return;
	}

	// Mark each chain root, then one forward pass propagates membership to
	// every descendant -- compact poses are ordered parent-before-child,
	// so a bone's parent has always been decided by the time it's visited.
	TArray<bool> bInChain;
	bInChain.Init(false, Output.Pose.GetNumBones());
	for (const FName& RootName : CachedArmOverrideRootBones)
	{
		const int32 SkeletonBoneIndex = SkeletonAsset->GetReferenceSkeleton().FindBoneIndex(RootName);
		if (SkeletonBoneIndex == INDEX_NONE) { continue; }
		const FCompactPoseBoneIndex RootIndex = BoneContainer.GetCompactPoseIndexFromSkeletonIndex(SkeletonBoneIndex);
		if (RootIndex.IsValid()) { bInChain[RootIndex.GetInt()] = true; }
	}
	bool bAnyInChain = false;
	for (FCompactPoseBoneIndex Index : Output.Pose.ForEachBoneIndex())
	{
		if (!bInChain[Index.GetInt()])
		{
			const FCompactPoseBoneIndex Parent = Output.Pose.GetParentBoneIndex(Index);
			if (Parent.IsValid() && bInChain[Parent.GetInt()]) { bInChain[Index.GetInt()] = true; }
		}
		bAnyInChain |= bInChain[Index.GetInt()];
	}
	if (!bAnyInChain)
	{
		return;
	}

	FCompactPose OverridePose;
	FBlendedCurve OverrideCurve;
	UE::Anim::FStackAttributeContainer OverrideAttributes;
	OverridePose.SetBoneContainer(&BoneContainer);
	OverrideCurve.InitFrom(Output.Curve);
	FAnimationPoseData OverridePoseData(OverridePose, OverrideCurve, OverrideAttributes);
	FDeltaTimeRecord DeltaTimeRecord;
	FAnimExtractContext ExtractContext(static_cast<double>(ArmOverrideTime), false, DeltaTimeRecord, true);
	CachedArmOverridePose->GetAnimationPose(OverridePoseData, ExtractContext);

	// The outgoing clip, if a swap is still fading. Evaluated at ITS time, then eased toward the
	// incoming pose before the layer as a whole is blended onto the body.
	const bool bSwapping = PrevArmOverridePose != nullptr && ArmSwapAlpha < 1.0f;
	FCompactPose PrevPose;
	FBlendedCurve PrevCurve;
	UE::Anim::FStackAttributeContainer PrevAttributes;
	if (bSwapping)
	{
		PrevPose.SetBoneContainer(&BoneContainer);
		PrevCurve.InitFrom(Output.Curve);
		FAnimationPoseData PrevPoseData(PrevPose, PrevCurve, PrevAttributes);
		FDeltaTimeRecord PrevRecord;
		FAnimExtractContext PrevContext(static_cast<double>(PrevArmOverrideTime), false, PrevRecord, true);
		PrevArmOverridePose->GetAnimationPose(PrevPoseData, PrevContext);
	}
	const float Swap = FMath::InterpEaseInOut(0.0f, 1.0f, ArmSwapAlpha, 2.0f);

	for (FCompactPoseBoneIndex Index : Output.Pose.ForEachBoneIndex())
	{
		if (!bInChain[Index.GetInt()]) { continue; }
		FTransform Target = OverridePose[Index];
		if (bSwapping)
		{
			FTransform From = PrevPose[Index];
			From.BlendWith(Target, Swap);
			Target = From;
		}
		FTransform Blended = Output.Pose[Index];
		Blended.BlendWith(Target, ArmOverrideAlpha);
		Output.Pose[Index] = Blended;
	}
}

void FCharacterAnimInstanceProxy::SolveTwoBone(FCSPose<FCompactPose>& CS, FName UpperName, FName LowerName,
                                              FName EndName, const FTransform& TargetCS, float Weight, float ElbowDownBias)
{
	if (Weight <= KINDA_SMALL_NUMBER) { return; }
	const FBoneContainer& BC = CS.GetPose().GetBoneContainer();
	const USkeleton* Skel = BC.GetSkeletonAsset();
	if (!Skel) { return; }

	auto Index = [&](FName Bone) -> FCompactPoseBoneIndex
	{
		const int32 SkelIdx = Skel->GetReferenceSkeleton().FindBoneIndex(Bone);
		return (SkelIdx == INDEX_NONE) ? FCompactPoseBoneIndex(INDEX_NONE)
		                               : BC.GetCompactPoseIndexFromSkeletonIndex(SkelIdx);
	};
	const FCompactPoseBoneIndex UpperIdx = Index(UpperName);
	const FCompactPoseBoneIndex LowerIdx = Index(LowerName);
	const FCompactPoseBoneIndex EndIdx = Index(EndName);
	if (!UpperIdx.IsValid() || !LowerIdx.IsValid() || !EndIdx.IsValid()) { return; }

	const FTransform Upper = CS.GetComponentSpaceTransform(UpperIdx);
	const FTransform Lower = CS.GetComponentSpaceTransform(LowerIdx);
	const FTransform End = CS.GetComponentSpaceTransform(EndIdx);

	const FVector Root = Upper.GetLocation();
	const FVector Joint = Lower.GetLocation();
	const FVector Hand = End.GetLocation();
	const float L1 = FVector::Dist(Root, Joint);
	const float L2 = FVector::Dist(Joint, Hand);
	if (L1 < 1.0f || L2 < 1.0f) { return; }

	// How far the hand has to go, clamped short of a locked-straight arm. Never exactly
	// L1 + L2: a dead straight arm reads as broken, and at full extension the elbow's plane
	// becomes undefined and flickers on floating point noise alone.
	const FVector ToTarget = TargetCS.GetLocation() - Root;
	const float MaxReach = (L1 + L2) * FMath::Clamp(CachedIKMaxReach, 0.5f, 1.0f);
	const float Reach = FMath::Clamp(ToTarget.Size(), KINDA_SMALL_NUMBER, MaxReach);
	const FVector Dir = ToTarget.GetSafeNormal();
	if (Dir.IsNearlyZero()) { return; }

	// THE POLE COMES FROM THE ANIMATION. The elbow the clip already chose, projected off the
	// shoulder-to-target line, is the plane to bend in. Inventing a pole vector -- "elbows point
	// down and out" -- is where two-bone IK starts flipping the elbow through the torso the
	// moment the body turns; taking it from the pose means the arm keeps whatever character the
	// animator gave it and the IK only corrects where the hand ends up.
	FVector Pole = (Joint - Root) - Dir * FVector::DotProduct(Joint - Root, Dir);
	if (Pole.SizeSquared() < 1.0f)
	{
		// Arm already dead straight in the source pose: any perpendicular will do.
		Pole = FVector::CrossProduct(Dir, FVector::UpVector);
		if (Pole.SizeSquared() < 1.0f) { Pole = FVector::CrossProduct(Dir, FVector::ForwardVector); }
	}
	Pole = Pole.GetSafeNormal();
	// A STANCE'S ELBOWS. Arms held out in front (a pistol) want the elbows down and a little out,
	// which the clip's bent-arm pose does not give once the hands are pulled out to the grip:
	// its elbows ride up. The wanted direction is blended in over the pose's own by the stance's
	// bias, in the bend plane, so the arm keeps some of its character and cannot flip.
	if (ElbowDownBias > KINDA_SMALL_NUMBER)
	{
		const FVector Out = FVector(Root.X, Root.Y, 0.0f).GetSafeNormal();   // away from the body's centre line
		FVector Want = (FVector(0.0f, 0.0f, -1.0f) + Out * 0.45f).GetSafeNormal();
		Want = (Want - Dir * FVector::DotProduct(Want, Dir)).GetSafeNormal();
		if (!Want.IsNearlyZero()) { Pole = FMath::Lerp(Pole, Want, FMath::Clamp(ElbowDownBias, 0.0f, 1.0f)).GetSafeNormal(); }
	}

	// Law of cosines: the angle at the shoulder between the reach line and the upper arm.
	const float CosShoulder = FMath::Clamp((L1 * L1 + Reach * Reach - L2 * L2) / (2.0f * L1 * Reach), -1.0f, 1.0f);
	const float Shoulder = FMath::Acos(CosShoulder);
	const FVector NewJoint = Root + Dir * (L1 * FMath::Cos(Shoulder)) + Pole * (L1 * FMath::Sin(Shoulder));
	const FVector NewHand = Root + Dir * Reach;

	// Rotations follow the bones' new directions, so any twist the animation had is kept.
	FTransform OutUpper = Upper;
	OutUpper.SetRotation((FQuat::FindBetweenVectors(Joint - Root, NewJoint - Root) * Upper.GetRotation()).GetNormalized());
	FTransform OutLower = Lower;
	OutLower.SetRotation((FQuat::FindBetweenVectors(Hand - Joint, NewHand - NewJoint) * Lower.GetRotation()).GetNormalized());
	OutLower.SetLocation(NewJoint);
	FTransform OutEnd = End;
	OutEnd.SetLocation(NewHand);
	// The wrist takes the target's rotation outright. Position alone would put the hand in the
	// right place holding the weapon sideways, which is the single most common way this looks
	// wrong.
	OutEnd.SetRotation(TargetCS.GetRotation());

	// Blend by weight so the IK can be eased in and out rather than snapping on.
	auto Blend = [Weight](const FTransform& From, const FTransform& To)
	{
		FTransform R;
		R.SetLocation(FMath::Lerp(From.GetLocation(), To.GetLocation(), Weight));
		R.SetRotation(FQuat::Slerp(From.GetRotation(), To.GetRotation(), Weight).GetNormalized());
		R.SetScale3D(From.GetScale3D());
		return R;
	};
	CS.SetComponentSpaceTransform(UpperIdx, Blend(Upper, OutUpper));
	CS.SetComponentSpaceTransform(LowerIdx, Blend(Lower, OutLower));
	CS.SetComponentSpaceTransform(EndIdx, Blend(End, OutEnd));
}

void FCharacterAnimInstanceProxy::ApplySpineLean(FPoseContext& Output)
{
	const float Total = CachedAimPitch * CachedSpineLeanFraction * CachedSpineLeanWeight;
	if (FMath::Abs(Total) < 0.05f || CachedSpineLeanBones.Num() == 0) { return; }
	const FBoneContainer& BC = Output.Pose.GetBoneContainer();
	const USkeleton* Skel = BC.GetSkeletonAsset();
	if (!Skel) { return; }

	// The pitch axis in component space: across the body, perpendicular to where it faces.
	const FVector Right = FVector::CrossProduct(FVector::UpVector, CachedAimForwardCS).GetSafeNormal();
	if (Right.IsNearlyZero()) { return; }
	const float Each = FMath::DegreesToRadians(-Total / CachedSpineLeanBones.Num());

	// Applied in LOCAL space, one bone at a time, so every child comes along for free -- but
	// the delta is authored in component space, so it is conjugated into each bone's parent
	// frame first. The component-space pose is rebuilt between bones because the second bone's
	// parent is the first bone, which has just moved.
	for (const FName& BoneName : CachedSpineLeanBones)
	{
		const int32 SkelIdx = Skel->GetReferenceSkeleton().FindBoneIndex(BoneName);
		if (SkelIdx == INDEX_NONE) { continue; }
		const FCompactPoseBoneIndex Idx = BC.GetCompactPoseIndexFromSkeletonIndex(SkelIdx);
		if (!Idx.IsValid()) { continue; }
		const FCompactPoseBoneIndex Parent = Output.Pose.GetParentBoneIndex(Idx);
		FCSPose<FCompactPose> CS;
		CS.InitPose(Output.Pose);
		const FQuat ParentCS = Parent.IsValid() ? CS.GetComponentSpaceTransform(Parent).GetRotation() : FQuat::Identity;
		const FQuat DeltaCS(Right, Each);
		const FQuat DeltaLocal = ParentCS.Inverse() * DeltaCS * ParentCS;
		Output.Pose[Idx].SetRotation((DeltaLocal * Output.Pose[Idx].GetRotation()).GetNormalized());
	}
}

void FCharacterAnimInstanceProxy::ApplyHandIK(FPoseContext& Output)
{
	if (CachedIKWeightR <= KINDA_SMALL_NUMBER && CachedIKWeightL <= KINDA_SMALL_NUMBER) { return; }

	// The targets arrive in world space, because that is what the character can compute from the
	// weapon's own component. One conversion here, not two solves' worth.
	const FTransform CompToWorld = GetComponentTransform();
	const FTransform WorldToComp = CompToWorld.Inverse();

	FCSPose<FCompactPose> CS;
	CS.InitPose(Output.Pose);

	if (CachedIKWeightR > KINDA_SMALL_NUMBER)
	{
		SolveTwoBone(CS, CachedIKBones[0], CachedIKBones[1], CachedIKBones[2],
		             CachedIKTargetR * WorldToComp, CachedIKWeightR, CachedElbowBiasR);
	}
	if (CachedIKWeightL > KINDA_SMALL_NUMBER)
	{
		SolveTwoBone(CS, CachedIKBones[3], CachedIKBones[4], CachedIKBones[5],
		             CachedIKTargetL * WorldToComp, CachedIKWeightL, CachedElbowBiasL);
	}

	FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(CS), Output.Pose);
}

void FCharacterAnimInstanceProxy::ApplyLookAt(FPoseContext& Output)
{
	if (LookAtWeight <= KINDA_SMALL_NUMBER || LookAtBoneName == NAME_None)
	{
		return;
	}

	const FBoneContainer& BoneContainer = Output.Pose.GetBoneContainer();
	const USkeleton* SkeletonAsset = BoneContainer.GetSkeletonAsset();
	const int32 SkeletonBoneIndex = SkeletonAsset ? SkeletonAsset->GetReferenceSkeleton().FindBoneIndex(LookAtBoneName) : INDEX_NONE;
	if (SkeletonBoneIndex == INDEX_NONE)
	{
		return;
	}

	const FCompactPoseBoneIndex CompactIndex = BoneContainer.GetCompactPoseIndexFromSkeletonIndex(SkeletonBoneIndex);
	if (!CompactIndex.IsValid() || !Owner)
	{
		return;
	}
	const FCompactPoseBoneIndex ParentIndex = Output.Pose.GetParentBoneIndex(CompactIndex);

	// ---- Rig-agnostic head axes ---------------------------------------------
	// Which of the head bone's LOCAL axes is "forward (the way the face
	// looks)" and which is "up the skull" differs per rig (the knight and the
	// modular hero disagree by ~90 degrees), so nothing here assumes a
	// convention. Instead both are derived from the SKELETON's reference pose:
	// at rest every character faces the component-space direction the actor's
	// forward maps to (CachedFacingComponent, +Y for the standard -90-yaw mesh
	// offset) and component +Z is up; express those two directions in the head
	// bone's own frame at rest and they are constants for this rig.
	const FReferenceSkeleton& RefSkel = SkeletonAsset->GetReferenceSkeleton();
	const TArray<FTransform>& RefLocal = RefSkel.GetRefBonePose();
	FQuat HeadRefCS = FQuat::Identity;
	for (int32 b = SkeletonBoneIndex; b != INDEX_NONE; b = RefSkel.GetParentIndex(b))
	{
		HeadRefCS = RefLocal[b].GetRotation() * HeadRefCS;
	}
	const FVector HeadForwardLocal = HeadRefCS.UnrotateVector(CachedFacingComponent).GetSafeNormal();
	const FVector HeadUpLocal = HeadRefCS.UnrotateVector(FVector::UpVector).GetSafeNormal();
	if (HeadForwardLocal.IsNearlyZero() || HeadUpLocal.IsNearlyZero())
	{
		return;
	}

	FCSPose<FCompactPose> CSPose;
	CSPose.InitPose(Output.Pose);
	const FTransform HeadCS = CSPose.GetComponentSpaceTransform(CompactIndex);
	const FQuat ParentCSRot = ParentIndex.IsValid() ? CSPose.GetComponentSpaceTransform(ParentIndex).GetRotation() : FQuat::Identity;

	// ---- Where the target is, in component space -----------------------------
	const FVector HeadWorldLoc = CachedComponentToWorld.TransformPosition(HeadCS.GetLocation());
	const FVector ToTargetWorld = (LookAtTargetWorld - HeadWorldLoc).GetSafeNormal();
	if (ToTargetWorld.IsNearlyZero())
	{
		return;
	}
	const FVector DesiredFwd = CachedComponentToWorld.InverseTransformVectorNoScale(ToTargetWorld).GetSafeNormal();

	// ---- The NEUTRAL head on the current neck, and its frame ----------------
	// "Neutral" = the head's reference-pose local rotation sitting on this
	// frame's ANIMATED parent (neck). Every limit below is measured against
	// this frame, so the limits are always relative to the neck the head is
	// actually attached to -- the head can never be turned past them no
	// matter what the torso, the animation, or the target are doing.
	const FQuat NeutralHeadCS = ParentCSRot * RefLocal[SkeletonBoneIndex].GetRotation();
	const FVector NFwd = NeutralHeadCS.RotateVector(HeadForwardLocal).GetSafeNormal();
	const FVector NUp = NeutralHeadCS.RotateVector(HeadUpLocal).GetSafeNormal();
	const FVector NRight = FVector::CrossProduct(NUp, NFwd).GetSafeNormal();   // Forward x Right = Up  =>  Right = Up x Forward
	if (NRight.IsNearlyZero())
	{
		return;
	}

	// ---- Clamp the requested direction to the neck's natural range ----------
	const float F = FVector::DotProduct(DesiredFwd, NFwd);
	const float R = FVector::DotProduct(DesiredFwd, NRight);
	const float U = FVector::DotProduct(DesiredFwd, NUp);
	const float YawDeg = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(R, F)), -Owner->LookAtMaxYawDegrees, Owner->LookAtMaxYawDegrees);
	const float PitchDeg = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(U, FMath::Sqrt(F * F + R * R))), -Owner->LookAtMaxPitchDegrees, Owner->LookAtMaxPitchDegrees);
	const float CY = FMath::Cos(FMath::DegreesToRadians(YawDeg)), SY = FMath::Sin(FMath::DegreesToRadians(YawDeg));
	const float CP = FMath::Cos(FMath::DegreesToRadians(PitchDeg)), SP = FMath::Sin(FMath::DegreesToRadians(PitchDeg));
	const FVector ClampedFwd = (NFwd * (CP * CY) + NRight * (CP * SY) + NUp * SP).GetSafeNormal();

	// ---- Target head orientation: clamped yaw + pitch, and NO roll ----------
	// The target frame keeps its up vector in the plane of the neutral up and
	// the look direction, i.e. the head only pans and tilts, it never rolls
	// about the direction it is looking. Built as "the rotation that carries
	// the neutral basis onto the target basis", applied to the neutral head,
	// so the head's own axis convention never enters into it.
	FVector TargetUp = (NUp - ClampedFwd * FVector::DotProduct(NUp, ClampedFwd)).GetSafeNormal();
	if (TargetUp.IsNearlyZero()) { TargetUp = NUp; }
	const FVector TargetRight = FVector::CrossProduct(TargetUp, ClampedFwd).GetSafeNormal();
	const FQuat NeutralBasis = FMatrix(NFwd, NRight, NUp, FVector::ZeroVector).ToQuat();
	const FQuat TargetBasis = FMatrix(ClampedFwd, TargetRight, TargetUp, FVector::ZeroVector).ToQuat();
	const FQuat TargetHeadCS = (TargetBasis * NeutralBasis.Inverse()) * NeutralHeadCS;

	// Blend from the ANIMATED head (so idle head motion shows through at low
	// weight) toward the clamped target, and write ONLY the head bone's local
	// rotation relative to its parent -- the same single-bone write
	// ApplyFacialBoneEdits uses (a whole-pose CSPose write-back was the cause
	// of an earlier "spiral" artifact).
	const FQuat BlendedCS = FQuat::Slerp(HeadCS.GetRotation(), TargetHeadCS, LookAtWeight).GetNormalized();
	FTransform Bone = Output.Pose[CompactIndex];
	Bone.SetRotation((ParentCSRot.Inverse() * BlendedCS).GetNormalized());
	Output.Pose[CompactIndex] = Bone;
}

void FCharacterAnimInstanceProxy::ApplyFacialBoneEdits(FPoseContext& Output)
{
	if (!Owner)
	{
		return;
	}

	const FBoneContainer& BoneContainer = Output.Pose.GetBoneContainer();
	const USkeleton* SkeletonAsset = BoneContainer.GetSkeletonAsset();
	if (!SkeletonAsset)
	{
		return;
	}

	auto FindCompactIndex = [&](FName BoneName) -> FCompactPoseBoneIndex
	{
		const int32 SkeletonBoneIndex = SkeletonAsset->GetReferenceSkeleton().FindBoneIndex(BoneName);
		if (SkeletonBoneIndex == INDEX_NONE)
		{
			return FCompactPoseBoneIndex(INDEX_NONE);
		}
		return BoneContainer.GetCompactPoseIndexFromSkeletonIndex(SkeletonBoneIndex);
	};

	// Jaw: hinge rotation around the bone's own local Z (verified axis --
	// see the property's header comment on UCharacterAnimInstance).
	if (CachedJawHingeDeg != 0.0f)
	{
		const FCompactPoseBoneIndex Index = FindCompactIndex(Owner->JawBoneName);
		if (Index.IsValid())
		{
			FTransform Bone = Output.Pose[Index];
			Bone.SetRotation(Bone.GetRotation() * FQuat(FVector::UpVector, FMath::DegreesToRadians(CachedJawHingeDeg)));
			Output.Pose[Index] = Bone;
		}
	}

	// Eyes: hinge rotation around local Z, plus a uniform scale for
	// blinking (1.0 = open, near 0 = closed).
	if (CachedEyesHingeDeg != 0.0f || CachedEyesBlinkScale != 1.0f)
	{
		const FCompactPoseBoneIndex Index = FindCompactIndex(Owner->EyesBoneName);
		if (Index.IsValid())
		{
			FTransform Bone = Output.Pose[Index];
			if (CachedEyesHingeDeg != 0.0f)
			{
				Bone.SetRotation(Bone.GetRotation() * FQuat(FVector::UpVector, FMath::DegreesToRadians(CachedEyesHingeDeg)));
			}
			Bone.SetScale3D(Bone.GetScale3D() * CachedEyesBlinkScale);
			Output.Pose[Index] = Bone;
		}
	}

	// Eyebrows: vertical (local Z) translation for height, plus rotation
	// about the bone's front-to-back (local X) axis for the left/right tilt.
	if (CachedBrowHeightCm != 0.0f || CachedBrowAngleDeg != 0.0f)
	{
		const FCompactPoseBoneIndex Index = FindCompactIndex(Owner->EyebrowsBoneName);
		if (Index.IsValid())
		{
			FTransform Bone = Output.Pose[Index];
			Bone.AddToTranslation(FVector(0.0f, 0.0f, CachedBrowHeightCm));
			if (CachedBrowAngleDeg != 0.0f)
			{
				Bone.SetRotation(Bone.GetRotation() * FQuat(FVector::ForwardVector, FMath::DegreesToRadians(CachedBrowAngleDeg)));
			}
			Output.Pose[Index] = Bone;
		}
	}
}

void FCharacterAnimInstanceProxy::ApplyWeaponGripCorrection(FPoseContext& Output)
{
	if (!Owner || Owner->WeaponGripWeight <= 0.0f)
	{
		return;
	}

	const FBoneContainer& BoneContainer = Output.Pose.GetBoneContainer();
	const USkeleton* SkeletonAsset = BoneContainer.GetSkeletonAsset();
	if (!SkeletonAsset)
	{
		return;
	}

	auto FindCompactIndex = [&](FName BoneName) -> FCompactPoseBoneIndex
	{
		const int32 SkeletonBoneIndex = SkeletonAsset->GetReferenceSkeleton().FindBoneIndex(BoneName);
		if (SkeletonBoneIndex == INDEX_NONE)
		{
			return FCompactPoseBoneIndex(INDEX_NONE);
		}
		return BoneContainer.GetCompactPoseIndexFromSkeletonIndex(SkeletonBoneIndex);
	};

	// Bone-local hinge. The axis is local Z on the Mannequin rig (where
	// the curl amounts were tuned); on another rig it's that same hinge
	// expressed on the rig's own bone axes -- see RetargetCurlAxis.
	auto CurlBone = [&](FName BoneName, float Degrees)
	{
		const FCompactPoseBoneIndex Index = FindCompactIndex(BoneName);
		if (!Index.IsValid())
		{
			return;
		}
		const FVector Axis = (bRetargetActive && RetargetCurlAxis.IsValidIndex(Index.GetInt())) ? RetargetCurlAxis[Index.GetInt()] : FVector::UpVector;
		FTransform Bone = Output.Pose[Index];
		Bone.SetRotation(Bone.GetRotation() * FQuat(Axis, FMath::DegreesToRadians(Degrees * Owner->WeaponGripWeight)));
		Output.Pose[Index] = Bone;
	};

	// Bone lists are read from Owner here (not cached in PreUpdate like the
	// facial values) -- they only change when the character is assembled,
	// never per-frame, and the TArray is never resized while playing.
	for (const FName& Bone : Owner->GripFingerBones) { CurlBone(Bone, Owner->FingerCurlDegrees); }
	for (const FName& Bone : Owner->GripThumbBones) { CurlBone(Bone, -Owner->ThumbCurlDegrees); }
}

void UCharacterAnimInstance::SetArmOverride(UAnimSequence* Pose, const TArray<FName>& Roots, float Weight, bool bLoops)
{
	// Restarting matters even when nothing else changed -- two shots in a row are the same
	// clip and the second one still has to play from the top.
	++ArmOverrideSerial;
	ArmOverridePose = Pose;
	if (Roots.Num() > 0) { ArmOverrideRootBones = Roots; }
	ArmOverrideWeight = FMath::Clamp(Weight, 0.0f, 1.0f);
	bArmOverrideLoops = bLoops;
	bArmOverrideAnimates = true;
}
