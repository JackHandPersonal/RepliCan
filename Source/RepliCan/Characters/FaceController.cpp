#include "Characters/FaceController.h"
#include "Components/DecalComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "Engine/Texture2D.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "ShaderCompiler.h"
#include "Characters/CharacterAnimInstance.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// All of these were previously set ONLY as per-instance Python overrides
	// on the one placed FaceController actor in Lvl_ForestGlade -- meaning
	// deleting that level would have silently discarded this entire
	// configuration even though the C++ classes themselves have no level
	// dependency. Baked in here instead, matching the same
	// FObjectFinder-in-constructor pattern ABaseCharacter already uses for
	// its own animation defaults, so a freshly-spawned FaceController is
	// fully configured out of the box on any character/level. Every path
	// below already lived in a project-owned folder (not a vendor pack), so
	// no asset relocation was needed -- only wiring the references into code.
	constexpr const TCHAR* BaseMouthMaterialPath = TEXT("/Game/Characters/KnightDemo/M_Mouth.M_Mouth");
	constexpr const TCHAR* NoseMeshPath = TEXT("/Game/Characters/KnightDemo/SM_Nose.SM_Nose");
	constexpr const TCHAR* HighlightMeshPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	constexpr const TCHAR* HighlightMaterialPath = TEXT("/Game/Characters/KnightDemo/M_HighlightBeam.M_HighlightBeam");

	constexpr const TCHAR* HairOptionPaths[] = {
		TEXT("/Game/PolygonSciFiWorlds/Models/CharactersUE4Mannequin/Attachments/SM_Chr_Attach_Hair_Male_01.SM_Chr_Attach_Hair_Male_01"),
		TEXT("/Game/PolygonSciFiWorlds/Models/CharactersUE4Mannequin/Attachments/SM_Chr_Attach_Hair_Male_02.SM_Chr_Attach_Hair_Male_02"),
		TEXT("/Game/PolygonSciFiWorlds/Models/CharactersUE4Mannequin/Attachments/SM_Chr_Attach_Hair_Male_03.SM_Chr_Attach_Hair_Male_03"),
		TEXT("/Game/PolygonSciFiWorlds/Models/CharactersUE4Mannequin/Attachments/SM_Chr_Attach_Hair_Male_04.SM_Chr_Attach_Hair_Male_04"),
		TEXT("/Game/PolygonSciFiWorlds/Models/CharactersUE4Mannequin/Attachments/SM_Chr_Attach_Hair_Female_01.SM_Chr_Attach_Hair_Female_01"),
		TEXT("/Game/PolygonSciFiWorlds/Models/CharactersUE4Mannequin/Attachments/SM_Chr_Attach_Hair_Female_02.SM_Chr_Attach_Hair_Female_02"),
		TEXT("/Game/PolygonSciFiWorlds/Models/CharactersUE4Mannequin/Attachments/SM_Chr_Attach_Hair_Female_03.SM_Chr_Attach_Hair_Female_03"),
		TEXT("/Game/PolygonSciFiWorlds/Models/CharactersUE4Mannequin/Attachments/SM_Chr_Attach_Hair_Female_04.SM_Chr_Attach_Hair_Female_04"),
	};

	constexpr const TCHAR* FacialHairOptionPaths[] = {
		TEXT("/Game/PolygonSciFiWorlds/Models/CharactersUE4Mannequin/Attachments/SM_Chr_Attach_Beard_01.SM_Chr_Attach_Beard_01"),
		TEXT("/Game/PolygonSciFiWorlds/Models/CharactersUE4Mannequin/Attachments/SM_Chr_Attach_Beard_02.SM_Chr_Attach_Beard_02"),
		TEXT("/Game/PolygonSciFiWorlds/Models/CharactersUE4Mannequin/Attachments/SM_Chr_Attach_Beard_03.SM_Chr_Attach_Beard_03"),
		TEXT("/Game/PolygonSciFiWorlds/Models/CharactersUE4Mannequin/Attachments/SM_Chr_Attach_Beard_04.SM_Chr_Attach_Beard_04"),
	};

	constexpr const TCHAR* HeadGearOptionPaths[] = {
		TEXT("/Game/PolygonSciFiWorlds/Models/CharactersUE4Mannequin/Attachments/SM_Chr_Attach_Helmet_01.SM_Chr_Attach_Helmet_01"),
		TEXT("/Game/PolygonSciFiWorlds/Models/CharactersUE4Mannequin/Attachments/SM_Chr_Attach_Helmet_02.SM_Chr_Attach_Helmet_02"),
		TEXT("/Game/PolygonSciFiWorlds/Models/CharactersUE4Mannequin/Attachments/SM_Chr_Attach_Helmet_03.SM_Chr_Attach_Helmet_03"),
		TEXT("/Game/PolygonSciFiWorlds/Models/CharactersUE4Mannequin/Attachments/SM_Chr_Attach_Helmet_04.SM_Chr_Attach_Helmet_04"),
		TEXT("/Game/PolygonSciFiWorlds/Models/CharactersUE4Mannequin/Attachments/SM_Chr_Attach_Helmet_05.SM_Chr_Attach_Helmet_05"),
		TEXT("/Game/PolygonSciFiWorlds/Models/CharactersUE4Mannequin/Attachments/SM_Chr_Attach_Hat_01.SM_Chr_Attach_Hat_01"),
		TEXT("/Game/PolygonSciFiWorlds/Models/CharactersUE4Mannequin/Attachments/SM_Chr_Attach_Hat_02.SM_Chr_Attach_Hat_02"),
		TEXT("/Game/PolygonSciFiWorlds/Models/CharactersUE4Mannequin/Attachments/SM_Chr_Attach_Hat_03.SM_Chr_Attach_Hat_03"),
		TEXT("/Game/PolygonSciFiWorlds/Models/CharactersUE4Mannequin/Attachments/SM_Chr_Attach_Hat_04.SM_Chr_Attach_Hat_04"),
	};

	// Palette alternates are per pack; the Character Manager's Palette combo
	// scans them, so the old cycle list is empty here.
	constexpr const TCHAR* MaterialOptionPaths[] = { nullptr };

	struct FMouthTextureEntry { const TCHAR* StateName; const TCHAR* Path; };
	constexpr FMouthTextureEntry MouthTextureEntries[] = {
		{ TEXT("Neutral"), TEXT("/Game/Characters/KnightDemo/T_Mouth_Neutral.T_Mouth_Neutral") },
		{ TEXT("Sad"), TEXT("/Game/Characters/KnightDemo/T_Mouth_Sad.T_Mouth_Sad") },
		{ TEXT("Happy"), TEXT("/Game/Characters/KnightDemo/T_Mouth_Happy.T_Mouth_Happy") },
		{ TEXT("Surprised"), TEXT("/Game/Characters/KnightDemo/T_Mouth_Surprised.T_Mouth_Surprised") },
		{ TEXT("Smirk"), TEXT("/Game/Characters/KnightDemo/T_Mouth_Smirk.T_Mouth_Smirk") },
		{ TEXT("Frown"), TEXT("/Game/Characters/KnightDemo/T_Mouth_Frown.T_Mouth_Frown") },
		{ TEXT("Angry"), TEXT("/Game/Characters/KnightDemo/T_Mouth_Angry.T_Mouth_Angry") },
		{ TEXT("Open1"), TEXT("/Game/Characters/KnightDemo/T_Mouth_Open1.T_Mouth_Open1") },
		{ TEXT("Open2"), TEXT("/Game/Characters/KnightDemo/T_Mouth_Open2.T_Mouth_Open2") },
		{ TEXT("Open3"), TEXT("/Game/Characters/KnightDemo/T_Mouth_Open3.T_Mouth_Open3") },
		{ TEXT("Talking1"), TEXT("/Game/Characters/KnightDemo/T_Mouth_Talking1.T_Mouth_Talking1") },
		{ TEXT("Talking2"), TEXT("/Game/Characters/KnightDemo/T_Mouth_Talking2.T_Mouth_Talking2") },
		{ TEXT("Talking3"), TEXT("/Game/Characters/KnightDemo/T_Mouth_Talking3.T_Mouth_Talking3") },
	};
}

AFaceController::AFaceController()
{
	PrimaryActorTick.bCanEverTick = false;

	Decal = CreateDefaultSubobject<UDecalComponent>(TEXT("Decal"));
	RootComponent = Decal;
	Decal->DecalSize = FVector(20.f, 7.f, 3.4f);

	// Purely cosmetic attachments -- a plain UStaticMeshComponent defaults to
	// full (BlockAll) collision, which put real, camera-blocking geometry
	// right at head height the moment NoseMesh/HighlightMesh got real
	// defaults (see the constructor's asset-finder block below). Never
	// noticed on the static demo Knight (no SpringArmComponent/camera
	// anywhere nearby to react to it), but attaching this to an actual
	// player character with CameraBoom->bDoCollisionTest=true pulled the
	// camera in to avoid clipping through its own face/highlight column --
	// reported as "camera is stuck inside the character's head." None of
	// these are meant to be physically solid.
	NoseComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NoseComponent"));
	NoseComponent->SetupAttachment(Decal);
	NoseComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	BrowLComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BrowLComponent"));
	BrowLComponent->SetupAttachment(Decal);
	BrowLComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BrowRComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BrowRComponent"));
	BrowRComponent->SetupAttachment(Decal);
	BrowRComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	HairComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HairComponent"));
	HairComponent->SetupAttachment(Decal);
	HairComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	FacialHairComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FacialHairComponent"));
	FacialHairComponent->SetupAttachment(Decal);
	FacialHairComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	HeadGearComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadGearComponent"));
	HeadGearComponent->SetupAttachment(Decal);
	HeadGearComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeadGearBadgeComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadGearBadgeComponent"));
	HeadGearBadgeComponent->SetupAttachment(HeadGearComponent);
	HeadGearBadgeComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeadGearBadgeComponent->SetCastShadow(false);
	HeadGearBadgeComponent->SetVisibility(false);
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(TEXT("/Engine/BasicShapes/Plane.Plane"));
		if (PlaneFinder.Succeeded()) { HeadGearBadgeComponent->SetStaticMesh(PlaneFinder.Object); }
	}

	HighlightComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HighlightComponent"));
	HighlightComponent->SetupAttachment(Decal);
	HighlightComponent->SetCastShadow(false);
	HighlightComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BaseMouthMaterialFinder(BaseMouthMaterialPath);
	BaseMouthMaterial = BaseMouthMaterialFinder.Object;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> NoseMeshFinder(NoseMeshPath);
	NoseMesh = NoseMeshFinder.Object;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> HighlightMeshFinder(HighlightMeshPath);
	HighlightMesh = HighlightMeshFinder.Object;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> HighlightMaterialFinder(HighlightMaterialPath);
	HighlightMaterial = HighlightMaterialFinder.Object;

	// Optional cosmetic option lists: whatever of these exists in the
	// project's content is offered; a missing asset just isn't listed.
	for (const TCHAR* Path : HairOptionPaths)       { ConstructorHelpers::FObjectFinder<UStaticMesh> F(Path); if (F.Object) { HairOptions.Add(F.Object); } }
	for (const TCHAR* Path : FacialHairOptionPaths) { ConstructorHelpers::FObjectFinder<UStaticMesh> F(Path); if (F.Object) { FacialHairOptions.Add(F.Object); } }
	for (const TCHAR* Path : HeadGearOptionPaths)   { ConstructorHelpers::FObjectFinder<UStaticMesh> F(Path); if (F.Object) { HeadGearOptions.Add(F.Object); } }
	for (const TCHAR* Path : MaterialOptionPaths)   { if (Path) { ConstructorHelpers::FObjectFinder<UMaterialInterface> F(Path); if (F.Object) { MaterialOptions.Add(F.Object); } } }

	{
		static ConstructorHelpers::FObjectFinder<UTexture2D> Neutral(MouthTextureEntries[0].Path);
		static ConstructorHelpers::FObjectFinder<UTexture2D> Sad(MouthTextureEntries[1].Path);
		static ConstructorHelpers::FObjectFinder<UTexture2D> Happy(MouthTextureEntries[2].Path);
		static ConstructorHelpers::FObjectFinder<UTexture2D> Surprised(MouthTextureEntries[3].Path);
		static ConstructorHelpers::FObjectFinder<UTexture2D> Smirk(MouthTextureEntries[4].Path);
		static ConstructorHelpers::FObjectFinder<UTexture2D> Frown(MouthTextureEntries[5].Path);
		static ConstructorHelpers::FObjectFinder<UTexture2D> Angry(MouthTextureEntries[6].Path);
		static ConstructorHelpers::FObjectFinder<UTexture2D> Open1(MouthTextureEntries[7].Path);
		static ConstructorHelpers::FObjectFinder<UTexture2D> Open2(MouthTextureEntries[8].Path);
		static ConstructorHelpers::FObjectFinder<UTexture2D> Open3(MouthTextureEntries[9].Path);
		static ConstructorHelpers::FObjectFinder<UTexture2D> Talking1(MouthTextureEntries[10].Path);
		static ConstructorHelpers::FObjectFinder<UTexture2D> Talking2(MouthTextureEntries[11].Path);
		static ConstructorHelpers::FObjectFinder<UTexture2D> Talking3(MouthTextureEntries[12].Path);

		StateTextures.Add(MouthTextureEntries[0].StateName, Neutral.Object);
		StateTextures.Add(MouthTextureEntries[1].StateName, Sad.Object);
		StateTextures.Add(MouthTextureEntries[2].StateName, Happy.Object);
		StateTextures.Add(MouthTextureEntries[3].StateName, Surprised.Object);
		StateTextures.Add(MouthTextureEntries[4].StateName, Smirk.Object);
		StateTextures.Add(MouthTextureEntries[5].StateName, Frown.Object);
		StateTextures.Add(MouthTextureEntries[6].StateName, Angry.Object);
		StateTextures.Add(MouthTextureEntries[7].StateName, Open1.Object);
		StateTextures.Add(MouthTextureEntries[8].StateName, Open2.Object);
		StateTextures.Add(MouthTextureEntries[9].StateName, Open3.Object);
		StateTextures.Add(MouthTextureEntries[10].StateName, Talking1.Object);
		StateTextures.Add(MouthTextureEntries[11].StateName, Talking2.Object);
		StateTextures.Add(MouthTextureEntries[12].StateName, Talking3.Object);
	}

	// Eyes-only pose per named expression (brow posing is independent, set
	// separately via SetBrowHeight/SetBrowAngle -- see FFacialExpressionPose's
	// header comment).
	ExpressionPoses.Add(TEXT("Neutral"), FFacialExpressionPose{ 0.0f });
	ExpressionPoses.Add(TEXT("Sad"), FFacialExpressionPose{ -6.0f });
	ExpressionPoses.Add(TEXT("Happy"), FFacialExpressionPose{ 4.0f });
	ExpressionPoses.Add(TEXT("Surprised"), FFacialExpressionPose{ 10.0f });
	ExpressionPoses.Add(TEXT("Smirk"), FFacialExpressionPose{ 0.0f });
}

void AFaceController::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Runs in the plain editor viewport too (unlike BeginPlay), so the
	// decal shows the correct texture there instead of whatever leftover/
	// unset material it would otherwise have.
	EnsureMouthMID();
	if (!DefaultState.IsNone())
	{
		SetExpression(DefaultState);
	}
}

void AFaceController::EnsureMouthMID()
{
	if (MouthMID || !BaseMouthMaterial)
	{
		return;
	}

	// One dynamic instance of the shared mouth material, reused for every
	// expression -- swapping expressions just changes its texture
	// parameter, which is a cheap parameter update, not a material/shader
	// swap.
	MouthMID = UMaterialInstanceDynamic::Create(BaseMouthMaterial, this);
	MouthMID->SetVectorParameterValue(TEXT("Tint"), MouthTint);
	Decal->SetDecalMaterial(MouthMID);
}

void AFaceController::BeginPlay()
{
	Super::BeginPlay();

	// Re-resolve fresh for this Play session -- the pointer cached in
	// AttachToCharacter may point at an editor-context AnimInstance that no
	// longer exists once real gameplay creates its own.
	if (TargetSkeletalMesh)
	{
		FaceAnimInstance = Cast<UCharacterAnimInstance>(TargetSkeletalMesh->GetAnimInstance());

		if (AActor* CharacterActor = TargetSkeletalMesh->GetOwner())
		{
			SetOwner(CharacterActor);
		}
	}

	EnsureMouthMID();
	// Its shader only needs to compile once (here), so there's no
	// per-expression first-use flicker to guard against anymore.
	if (GShaderCompilingManager)
	{
		GShaderCompilingManager->FinishAllCompilation();
	}

	if (!DefaultState.IsNone())
	{
		SetExpression(DefaultState);
	}

	// Establish the resting eye openness before any blinking starts, and the
	// default brow height/angle.
	if (FaceAnimInstance)
	{
		FaceAnimInstance->EyesBlinkScale = GetRestEyeScale();
		FaceAnimInstance->BrowHeightCm = GetBrowHeightCm();
		FaceAnimInstance->BrowAngleDeg = GetBrowAngleDeg();
	}

	if (bAutoBlink)
	{
		ScheduleNextBlink();
	}
}

void AFaceController::ScheduleNextBlink()
{
	const float Delay = FMath::FRandRange(BlinkIntervalMin, BlinkIntervalMax);
	GetWorldTimerManager().SetTimer(BlinkScheduleTimer, this, &AFaceController::StartBlink, Delay, false);
}

void AFaceController::StartBlink()
{
	if (FaceAnimInstance)
	{
		FaceAnimInstance->EyesBlinkScale = 0.05f;
	}
	GetWorldTimerManager().SetTimer(BlinkReopenTimer, this, &AFaceController::EndBlink, BlinkCloseDuration, false);
}

void AFaceController::EndBlink()
{
	if (FaceAnimInstance)
	{
		FaceAnimInstance->EyesBlinkScale = GetRestEyeScale();
	}
	if (bAutoBlink)
	{
		ScheduleNextBlink();
	}
}

float AFaceController::GetRestEyeScale() const
{
	switch (CurrentEyeOpenness)
	{
		case EEyeOpenness::Wide: return WideEyeScale;
		case EEyeOpenness::Narrow: return NarrowEyeScale;
		case EEyeOpenness::Squint: return SquintEyeScale;
		case EEyeOpenness::Normal:
		default: return NormalEyeScale;
	}
}

void AFaceController::SetEyeOpenness(EEyeOpenness NewOpenness)
{
	CurrentEyeOpenness = NewOpenness;

	// Apply immediately rather than waiting for the next blink cycle to
	// land on the new resting value.
	if (FaceAnimInstance)
	{
		FaceAnimInstance->EyesBlinkScale = GetRestEyeScale();
	}
}

float AFaceController::GetBrowHeightCm() const
{
	switch (CurrentBrowHeight)
	{
		case EBrowHeight::Low: return LowBrowHeightCm;
		case EBrowHeight::High: return HighBrowHeightCm;
		case EBrowHeight::Highest: return HighestBrowHeightCm;
		case EBrowHeight::Normal:
		default: return NormalBrowHeightCm;
	}
}

void AFaceController::SetBrowHeight(EBrowHeight NewHeight)
{
	CurrentBrowHeight = NewHeight;
	if (FaceAnimInstance)
	{
		FaceAnimInstance->BrowHeightCm = GetBrowHeightCm();
	}
}

float AFaceController::GetBrowAngleDeg() const
{
	switch (CurrentBrowAngle)
	{
		case EBrowAngle::HighLeft: return HighLeftBrowAngleDeg;
		case EBrowAngle::Left: return LeftBrowAngleDeg;
		case EBrowAngle::Right: return RightBrowAngleDeg;
		case EBrowAngle::HighRight: return HighRightBrowAngleDeg;
		case EBrowAngle::Normal:
		default: return 0.0f;
	}
}

void AFaceController::SetBrowAngle(EBrowAngle NewAngle)
{
	CurrentBrowAngle = NewAngle;
	if (FaceAnimInstance)
	{
		FaceAnimInstance->BrowAngleDeg = GetBrowAngleDeg();
	}
}

// Snap a piece onto its socket but KEEP its own relative scale, so it rides whatever scale the
// character is wearing. The engine's SnapToTargetNotIncludingScale uses KeepWorld for scale,
// which bakes 1/CharacterScale into the child the instant it attaches: the body then grows or
// shrinks and the hair, beard, brows, nose and body parts stay their authored size.
static const FAttachmentTransformRules SnapKeepScale(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepRelative, false);

void AFaceController::AttachToCharacter(USkeletalMeshComponent* SkeletalMesh, FName BoneName)
{
	if (!SkeletalMesh)
	{
		return;
	}

	// Snaps to the bone and applies MouthRelativeLocation/Rotation -- was
	// KeepWorldTransform (expecting an external script to have positioned
	// the decal in world space first), which left the Lvl_DungeonBase copy
	// floating at the world origin. The offsets are now data, so a freshly
	// spawned controller lands on the mouth by itself.
	Decal->AttachToComponent(SkeletalMesh, SnapKeepScale, BoneName);
	ApplyMouthPlacement();
	TargetSkeletalMesh = SkeletalMesh;

	// SetOwnerNoSee (see SetHiddenFromOwner) only hides a component from the
	// possessing player if that player's Pawn is actually this ACTOR's Owner
	// -- since FaceController is a separate actor from the character, not a
	// component of it, that relationship doesn't exist for free the way it
	// does for the character's own mesh.
	if (AActor* CharacterActor = SkeletalMesh->GetOwner())
	{
		SetOwner(CharacterActor);
	}

	// Skipped entirely when NoseMesh is unset -- not every character/pack
	// needs one. Snaps to the bone origin, then applies the configured
	// bone-relative offset explicitly -- self-contained, unlike Decal's
	// KeepWorldTransform approach which depends on external code having
	// pre-positioned it in world space before this call.
	AttachedBoneName = BoneName;
	AttachedMesh = SkeletalMesh;
	NoseComponent->SetStaticMesh(NoseMesh);
	NoseComponent->SetVisibility(NoseMesh != nullptr);
	if (NoseMesh)
	{
		NoseComponent->AttachToComponent(SkeletalMesh, SnapKeepScale, BoneName);
		ApplyNosePlacement();
	}
	for (UStaticMeshComponent* B : { BrowLComponent.Get(), BrowRComponent.Get() })
	{
		B->SetStaticMesh(BrowMesh);
		B->SetVisibility(BrowMesh != nullptr);
		if (BrowMesh) { B->AttachToComponent(SkeletalMesh, SnapKeepScale, BoneName); }
	}
	ApplyBrowPlacement();

	// Hair/FacialHair/HeadGear: snap directly onto the dedicated attachment
	// socket with identity transform -- unlike the nose, these Synty meshes
	// are pre-authored for exactly this attach point, so no per-instance
	// offset is needed. Attached even with no mesh assigned yet so cycling
	// works immediately.
	AttachHeadPiece(HairComponent, HairComponent->GetStaticMesh());
	AttachHeadPiece(FacialHairComponent, FacialHairComponent->GetStaticMesh());
	HeadGearComponent->AttachToComponent(SkeletalMesh, SnapKeepScale, TEXT("SOC_head"));
	HeadGearComponent->SetRelativeLocation(HeadGearOffset);
	HeadGearComponent->SetRelativeScale3D(FVector(HeadGearScale));
	UpdateAttachmentDecalReceiving();

	// Match whatever material this specific character is actually using --
	// GetMaterial() resolves the component's own override if set, falling
	// back to the skeletal mesh asset's default otherwise. The nose/hair/
	// beard/headgear meshes were each imported with a hardcoded material
	// reference (whichever Alt palette variant happened to be used at the
	// time), which silently drifts out of sync with characters using a
	// different override -- pulling it fresh here keeps them matched
	// regardless of which Alt variant this instance is on.
	ApplyMaterialToAttachments(SkeletalMesh->GetMaterial(0));

	// Skipped when HighlightMesh is unset. Starts hidden regardless --
	// SetHighlightVisible(true) turns it on (e.g. a UI toggle button).
	if (HighlightMesh)
	{
		HighlightComponent->SetStaticMesh(HighlightMesh);
		if (HighlightMaterial)
		{
			HighlightMID = UMaterialInstanceDynamic::Create(HighlightMaterial, this);
			HighlightMID->SetVectorParameterValue(TEXT("BeamColor"), HighlightColor);
			HighlightComponent->SetMaterial(0, HighlightMID);
		}
		// Attached to the mesh component's own root (NAME_None), not a bone --
		// see the HighlightHeightMultiplier comment in the header for why.
		HighlightComponent->AttachToComponent(SkeletalMesh, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, false), NAME_None);

		// Auto-size from the character's own CURRENT-POSE local bounds --
		// CalcBounds(Identity) rather than SkeletalMesh->Bounds so this is
		// unaffected by the actor's current facing (world-space bounds would
		// skew if the actor is yawed), and rather than the skeletal mesh
		// asset's own reference-pose bounds, which for a T-pose/A-pose bind
		// pose measure the arm span, not the standing silhouette.
		const FBoxSphereBounds PosedLocalBounds = SkeletalMesh->CalcBounds(FTransform::Identity);
		const float CharacterHalfHeight = PosedLocalBounds.BoxExtent.Z;
		const float CharacterRadius = FMath::Max(PosedLocalBounds.BoxExtent.X, PosedLocalBounds.BoxExtent.Y);

		// /Engine/BasicShapes/Cylinder's own local half-height and radius are
		// both 50 -- see the HighlightMesh comment in the header.
		constexpr float MeshLocalHalfExtent = 50.0f;
		const float ScaleZ = (CharacterHalfHeight * HighlightHeightMultiplier) / MeshLocalHalfExtent;
		const float ScaleXY = (CharacterRadius * HighlightRadiusMultiplier) / MeshLocalHalfExtent;

		HighlightComponent->SetRelativeLocation(FVector(0.0f, 0.0f, ScaleZ * MeshLocalHalfExtent));
		HighlightComponent->SetRelativeRotation(FRotator::ZeroRotator);
		HighlightComponent->SetRelativeScale3D(FVector(ScaleXY, ScaleXY, ScaleZ));
		HighlightComponent->SetVisibility(false);
	}

	// No class assignment here at all -- whatever character owns
	// SkeletalMesh has already set up its own UCharacterAnimInstance as the
	// mesh's main AnimInstance (see ABaseCharacter's constructor); eyes/brow
	// control just needs to find and write onto that same instance, not
	// install a second one. If the mesh isn't using a UCharacterAnimInstance
	// at all, this stays null and eyes/brow control is silently a no-op --
	// the mouth decal (this function's other half) works regardless.
	FaceAnimInstance = Cast<UCharacterAnimInstance>(SkeletalMesh->GetAnimInstance());
}

void AFaceController::SetHiddenFromOwner(bool bHideFromOwner)
{
	// HighlightComponent deliberately excluded -- it's a third-person "this
	// one is selected" indicator, not a head cosmetic that would clip into a
	// first-person view, and hiding it from its own owner would defeat its
	// purpose if a player is ever meant to see their own selection state.
	if (NoseComponent) { NoseComponent->SetOwnerNoSee(bHideFromOwner); }
	if (BrowLComponent) { BrowLComponent->SetOwnerNoSee(bHideFromOwner); }
	if (BrowRComponent) { BrowRComponent->SetOwnerNoSee(bHideFromOwner); }
	if (HairComponent) { HairComponent->SetOwnerNoSee(bHideFromOwner); }
	if (FacialHairComponent) { FacialHairComponent->SetOwnerNoSee(bHideFromOwner); }
	if (HeadGearComponent) { HeadGearComponent->SetOwnerNoSee(bHideFromOwner); }
	if (HeadGearBadgeComponent) { HeadGearBadgeComponent->SetOwnerNoSee(bHideFromOwner); }
}

void AFaceController::SetExpression(FName StateName)
{
	bool bFoundAny = false;

	if (StateTextures.Contains(StateName))
	{
		SetMouthTexture(StateName);
		bFoundAny = true;
	}

	if (const FFacialExpressionPose* FoundPose = ExpressionPoses.Find(StateName))
	{
		if (FaceAnimInstance)
		{
			FaceAnimInstance->EyesHingeDeg = FoundPose->EyesHingeDeg;
		}
		bFoundAny = true;
	}

	if (!bFoundAny)
	{
		UE_LOG(LogTemp, Warning, TEXT("FaceController: unknown expression '%s'"), *StateName.ToString());
	}
}

void AFaceController::SetMouthTexture(FName StateName)
{
	// Any explicit mouth pick (a UI button) cancels the auto-talk cycle --
	// TickTalking drives the mouth via ApplyMouthTextureInternal directly so
	// its own per-tick updates don't trip this and cancel themselves.
	StopTalking();
	ApplyMouthTextureInternal(StateName);
}

void AFaceController::ApplyMouthTextureInternal(FName StateName)
{
	if (StateName == FName(TEXT("None")))
	{
		SetMouthNone();
		return;
	}

	if (const TObjectPtr<UTexture2D>* FoundTex = StateTextures.Find(StateName))
	{
		// Re-creates MouthMID if a previous "None" selection released it.
		EnsureMouthMID();
		// A mouth-decal-disabled character (heads with painted mouths) keeps
		// the decal hidden through expression changes.
		Decal->SetVisibility(bMouthDecalEnabled);
		if (MouthMID)
		{
			MouthMID->SetTextureParameterValue(TEXT("MouthTexture"), *FoundTex);
		}
		CurrentState = StateName;
		UpdateAttachmentDecalReceiving();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("FaceController: unknown mouth state '%s'"), *StateName.ToString());
	}
}

void AFaceController::SetMouthNone()
{
	// Frees what a real mouth state costs: hide the decal (skips its draw
	// entirely) and drop the dynamic material instance so it can be GC'd --
	// EnsureMouthMID() recreates it lazily if a real mouth state is picked
	// again later.
	Decal->SetVisibility(false);
	Decal->SetDecalMaterial(nullptr);
	MouthMID = nullptr;
	CurrentState = FName(TEXT("None"));
	UpdateAttachmentDecalReceiving();
}

void AFaceController::SetTalkingMode(bool bEnable)
{
	if (bEnable)
	{
		bAutoTalking = true;
		TickTalking();
	}
	else
	{
		StopTalking();
	}
}

void AFaceController::StopTalking()
{
	if (bAutoTalking)
	{
		bAutoTalking = false;
		GetWorldTimerManager().ClearTimer(TalkingTimer);
	}
}

void AFaceController::TickTalking()
{
	// Mostly hop between the three talking frames on quick, flap-like
	// timing; occasionally settle on Neutral for a longer pause, reading as
	// a lull in a silent conversation rather than constant chatter.
	FName NextState;
	float Delay;
	if (FMath::FRand() < 0.2f)
	{
		NextState = TEXT("Neutral");
		Delay = FMath::FRandRange(0.5f, 2.0f);
	}
	else
	{
		static const FName TalkFrames[] = { TEXT("Talking1"), TEXT("Talking2"), TEXT("Talking3") };
		NextState = TalkFrames[FMath::RandRange(0, 2)];
		Delay = FMath::FRandRange(0.08f, 0.2f);
	}

	ApplyMouthTextureInternal(NextState);
	GetWorldTimerManager().SetTimer(TalkingTimer, this, &AFaceController::TickTalking, Delay, false);
}

void AFaceController::UpdateAttachmentDecalReceiving()
{
	// Only worth letting head attachments receive decals when there's no
	// mouth decal active to accidentally paint onto them.
	const bool bAllowDecals = (CurrentState == FName(TEXT("None")));
	HairComponent->SetReceivesDecals(bAllowDecals);
	FacialHairComponent->SetReceivesDecals(bAllowDecals);
	HeadGearComponent->SetReceivesDecals(bAllowDecals);
}

void AFaceController::ApplyHeadAttachment(UStaticMeshComponent* Comp, const TArray<TObjectPtr<UStaticMesh>>& Options, int32& Index, int32 NewIndex)
{
	Index = Options.IsValidIndex(NewIndex) ? NewIndex : -1;

	UStaticMesh* Mesh = Options.IsValidIndex(Index) ? Options[Index].Get() : nullptr;
	Comp->SetStaticMesh(Mesh);
	Comp->SetVisibility(Mesh != nullptr);
}

// Cycling wraps to -1 ("None") past the last option; ApplyHeadAttachment's
// out-of-range-becomes-None rule does the wrap.
void AFaceController::CycleHair()
{
	ApplyHeadAttachment(HairComponent, HairOptions, CurrentHairIndex, CurrentHairIndex + 1);
}

void AFaceController::CycleFacialHair()
{
	ApplyHeadAttachment(FacialHairComponent, FacialHairOptions, CurrentFacialHairIndex, CurrentFacialHairIndex + 1);
}

void AFaceController::CycleHeadGear()
{
	ApplyHeadAttachment(HeadGearComponent, HeadGearOptions, CurrentHeadGearIndex, CurrentHeadGearIndex + 1);
}

void AFaceController::SetHairIndex(int32 Index)
{
	ApplyHeadAttachment(HairComponent, HairOptions, CurrentHairIndex, Index);
}

void AFaceController::SetHairMesh(UStaticMesh* Mesh)
{
	CurrentHairIndex = Mesh ? HairOptions.IndexOfByKey(Mesh) : -1;
	HairComponent->SetStaticMesh(Mesh);
	HairComponent->SetVisibility(Mesh != nullptr);
	AttachHeadPiece(HairComponent, Mesh);
	ApplyHairColor();
}

void AFaceController::AttachHeadPiece(UStaticMeshComponent* Piece, UStaticMesh* Mesh)
{
	if (!Piece || !AttachedMesh.IsValid()) { return; }
	const bool bHero = Mesh && Mesh->GetPathName().Contains(TEXT("/Hero/"));
	Piece->AttachToComponent(AttachedMesh.Get(), SnapKeepScale, bHero ? AttachedBoneName : FName(TEXT("SOC_head")));
	Piece->SetRelativeLocationAndRotation(FVector::ZeroVector, FQuat::Identity);
	// Hero pieces are baked at true size; Synty attachments keep whatever scale the slot had.
	if (!SyntyPieceScale.Contains(Piece)) { SyntyPieceScale.Add(Piece, Piece->GetRelativeScale3D()); }
	Piece->SetRelativeScale3D(bHero ? FVector::OneVector : SyntyPieceScale[Piece]);
	// Override materials survive a mesh swap, so a body material set while the slot
	// was empty would stay on a hero piece: hero pieces take their own materials,
	// Synty pieces take the body's.
	if (Mesh)
	{
		if (bHero) { for (int32 i = 0; i < Mesh->GetStaticMaterials().Num(); ++i) { Piece->SetMaterial(i, Mesh->GetMaterial(i)); } }
		else if (AttachedMesh.IsValid() && AttachedMesh->GetMaterial(0)) { Piece->SetMaterial(0, AttachedMesh->GetMaterial(0)); }
	}
}

void AFaceController::ApplyHairColor()
{
	for (UStaticMeshComponent* C : { HairComponent.Get(), FacialHairComponent.Get(), BrowLComponent.Get(), BrowRComponent.Get() })
	{
		if (!C || !C->GetStaticMesh()) { continue; }
		for (int32 i = 0; i < C->GetNumMaterials(); ++i)
		{
			UMaterialInterface* M = C->GetMaterial(i);
			if (!M) { continue; }
			FLinearColor Probe;
			if (!M->GetVectorParameterValue(FMaterialParameterInfo(TEXT("Color_Hair")), Probe)) { continue; }
			if (HairColor.A <= 0.0f)
			{
				// Back to the asset's own material if we had swapped in an instance.
				if (Cast<UMaterialInstanceDynamic>(M) && C->GetStaticMesh()->GetStaticMaterials().IsValidIndex(i)) { C->SetMaterial(i, C->GetStaticMesh()->GetStaticMaterials()[i].MaterialInterface); }
				continue;
			}
			UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(M);
			if (!MID) { MID = UMaterialInstanceDynamic::Create(M, this); C->SetMaterial(i, MID); }
			MID->SetVectorParameterValue(TEXT("Color_Hair"), HairColor);
		}
	}
}

void AFaceController::SetFacialHairMesh(UStaticMesh* Mesh)
{
	CurrentFacialHairIndex = Mesh ? FacialHairOptions.IndexOfByKey(Mesh) : -1;
	FacialHairComponent->SetStaticMesh(Mesh);
	FacialHairComponent->SetVisibility(Mesh != nullptr);
	AttachHeadPiece(FacialHairComponent, Mesh);
	ApplyHairColor();
}

UStaticMesh* AFaceController::GetFacialHairMesh() const
{
	return FacialHairComponent ? FacialHairComponent->GetStaticMesh() : nullptr;
}

void AFaceController::SetHeadGearMesh(UStaticMesh* Mesh)
{
	CurrentHeadGearIndex = Mesh ? HeadGearOptions.IndexOfByKey(Mesh) : -1;
	HeadGearComponent->SetStaticMesh(Mesh);
	HeadGearComponent->SetVisibility(Mesh != nullptr);
	HeadGearComponent->SetRelativeLocation(HeadGearOffset);
	HeadGearComponent->SetRelativeScale3D(FVector(HeadGearScale));
}

UStaticMesh* AFaceController::GetHeadGearMesh() const
{
	return HeadGearComponent ? HeadGearComponent->GetStaticMesh() : nullptr;
}

void AFaceController::SetHeadGearBadge(UMaterialInterface* Material, const FVector& Location, const FRotator& Rotation, const FVector2D& Size)
{
	if (!HeadGearBadgeComponent) { return; }
	const bool bShow = Material != nullptr && HeadGearComponent && HeadGearComponent->GetStaticMesh() != nullptr;
	HeadGearBadgeComponent->SetVisibility(bShow);
	if (!bShow) { return; }
	HeadGearBadgeComponent->SetMaterial(0, Material);
	HeadGearBadgeComponent->SetRelativeLocationAndRotation(Location, Rotation);
	HeadGearBadgeComponent->SetRelativeScale3D(FVector(Size.X / 100.0f, Size.Y / 100.0f, 1.0f));   // the engine plane is 100 x 100
}

UStaticMesh* AFaceController::GetHairMesh() const
{
	return HairComponent ? HairComponent->GetStaticMesh() : nullptr;
}

void AFaceController::SetFacialHairIndex(int32 Index)
{
	ApplyHeadAttachment(FacialHairComponent, FacialHairOptions, CurrentFacialHairIndex, Index);
}

void AFaceController::SetHeadGearIndex(int32 Index)
{
	ApplyHeadAttachment(HeadGearComponent, HeadGearOptions, CurrentHeadGearIndex, Index);
}

void AFaceController::ApplyNosePlacement()
{
	if (!NoseComponent) { return; }
	NoseComponent->SetRelativeLocation(HeadFrameConversion.RotateVector(NoseRelativeLocation));
	NoseComponent->SetRelativeRotation((HeadFrameConversion * NoseRelativeRotation.Quaternion()).Rotator());
	NoseComponent->SetRelativeScale3D(NoseRelativeScale);
}

void AFaceController::ApplyBrowPlacement()
{
	// Right brow at BrowLocation, left at its lateral mirror; the tilt turns each
	// about the head's forward axis in opposite directions so inner ends
	// drop (angry) or rise (worried) together.
	const bool bHeroPair = BrowMesh && BrowMesh->GetPathName().Contains(TEXT("/Hero/"));
	for (int32 Side = 0; Side < 2; ++Side)
	{
		UStaticMeshComponent* B = Side == 0 ? BrowRComponent.Get() : BrowLComponent.Get();
		if (!B) { continue; }
		if (bHeroPair)
		{
			B->SetVisibility(Side == 0 && BrowMesh != nullptr);
			B->SetRelativeLocationAndRotation(FVector::ZeroVector, FQuat::Identity);
			B->SetRelativeScale3D(BrowScale);
			continue;
		}
		const float Mirror = Side == 0 ? 1.0f : -1.0f;
		const FVector Loc(BrowLocation.X, BrowLocation.Y, BrowLocation.Z * Mirror);
		const FQuat Tilt(FVector(0.0f, 1.0f, 0.0f), FMath::DegreesToRadians(BrowTilt * Mirror));
		B->SetRelativeLocation(HeadFrameConversion.RotateVector(Loc));
		B->SetRelativeRotation((HeadFrameConversion * Tilt).Rotator());
		B->SetRelativeScale3D(BrowScale);
	}
	ApplyHairColor();
}

void AFaceController::ApplyMouthPlacement()
{
	if (!Decal) { return; }
	Decal->SetRelativeLocation(HeadFrameConversion.RotateVector(MouthRelativeLocation));
	Decal->SetRelativeRotation((HeadFrameConversion * MouthRelativeRotation.Quaternion()).Rotator());
	Decal->SetRelativeScale3D(FVector(1.0f, MouthScale, MouthScale));
}

void AFaceController::SetMouthScale(float NewScale)
{
	MouthScale = FMath::Max(NewScale, 0.05f);
	ApplyMouthPlacement();
}

void AFaceController::SetMouthTint(const FLinearColor& NewTint)
{
	MouthTint = NewTint;
	if (MouthMID) { MouthMID->SetVectorParameterValue(TEXT("Tint"), MouthTint); }
}

void AFaceController::SetNoseRelativeLocation(const FVector& NewLocation)
{
	NoseRelativeLocation = NewLocation;
	ApplyNosePlacement();
}

void AFaceController::SetNoseRelativeRotation(const FRotator& NewRotation)
{
	NoseRelativeRotation = NewRotation;
	ApplyNosePlacement();
}

void AFaceController::SetMouthRelativeLocation(const FVector& NewLocation)
{
	MouthRelativeLocation = NewLocation;
	ApplyMouthPlacement();
}

void AFaceController::SetMouthRelativeRotation(const FRotator& NewRotation)
{
	MouthRelativeRotation = NewRotation;
	ApplyMouthPlacement();
}

void AFaceController::SetMouthDecalEnabled(bool bEnabled)
{
	bMouthDecalEnabled = bEnabled;
	if (Decal) { Decal->SetVisibility(bEnabled && CurrentState != FName(TEXT("None"))); }
}

TArray<FName> AFaceController::GetMouthStateNames() const
{
	TArray<FName> Names;
	StateTextures.GetKeys(Names);
	return Names;
}

void AFaceController::SetAutoBlink(bool bEnabled)
{
	if (bAutoBlink == bEnabled) { return; }
	bAutoBlink = bEnabled;
	if (bAutoBlink)
	{
		ScheduleNextBlink();
	}
	else
	{
		GetWorldTimerManager().ClearTimer(BlinkScheduleTimer);
		GetWorldTimerManager().ClearTimer(BlinkReopenTimer);
		if (FaceAnimInstance) { FaceAnimInstance->EyesBlinkScale = GetRestEyeScale(); }
	}
}

void AFaceController::ApplyMaterialToAttachments(UMaterialInterface* Material)
{
	if (!Material)
	{
		return;
	}
	// The nose keeps its skin-matched flat color (re-sampled if that color
	// came from the material that just changed); the rest follow the body.
	if (NoseColorMID && bNoseColorSampled) { MatchNoseToSkin(AttachedMesh.Get(), false, FLinearColor::White); }
	else if (!NoseColorMID) { NoseComponent->SetMaterial(0, Material); }
	// Hero pieces (baked from the Modular Fantasy Hero) keep their own atlas material, and their hair color.
	auto IsHero = [](UStaticMeshComponent* C) { return C && C->GetStaticMesh() && C->GetStaticMesh()->GetPathName().Contains(TEXT("/Hero/")); };
	if (!IsHero(HairComponent)) { HairComponent->SetMaterial(0, Material); }
	if (!IsHero(FacialHairComponent)) { FacialHairComponent->SetMaterial(0, Material); }
	HeadGearComponent->SetMaterial(0, Material);
	ApplyHairColor();
}

void AFaceController::MatchNoseToSkin(USkeletalMeshComponent* SkeletalMesh, bool bHasKnownColor, FLinearColor KnownColor)
{
	if (!NoseComponent || !NoseMesh) { return; }

	FLinearColor Color = KnownColor;
	if (!bHasKnownColor && !SampleSkinColorAroundNose(SkeletalMesh, Color))
	{
		// Nothing to go on: back to the old behaviour (the body material).
		NoseColorMID = nullptr;
		bNoseColorSampled = false;
		if (SkeletalMesh) { NoseComponent->SetMaterial(0, SkeletalMesh->GetMaterial(0)); }
		return;
	}
	if (!NoseColorMID)
	{
		UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Characters/M_FlatColor.M_FlatColor"));
		if (!Base) { return; }
		NoseColorMID = UMaterialInstanceDynamic::Create(Base, this);
	}
	NoseColor = Color;
	bNoseColorSampled = !bHasKnownColor;
	NoseColorMID->SetVectorParameterValue(TEXT("Color"), Color);
	NoseComponent->SetMaterial(0, NoseColorMID);
}

bool AFaceController::SampleSkinColorAroundNose(USkeletalMeshComponent* SkeletalMesh, FLinearColor& OutColor) const
{
#if WITH_EDITOR
	USkeletalMesh* Mesh = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
	const FSkeletalMeshRenderData* RenderData = Mesh ? Mesh->GetResourceForRendering() : nullptr;
	if (!RenderData || RenderData->LODRenderData.Num() == 0) { UE_LOG(LogTemp, Log, TEXT("NoseSkin: no render data")); return false; }
	const FSkeletalMeshLODRenderData& LOD = RenderData->LODRenderData[0];
	const FPositionVertexBuffer& Positions = LOD.StaticVertexBuffers.PositionVertexBuffer;
	const FStaticMeshVertexBuffer& UVs = LOD.StaticVertexBuffers.StaticMeshVertexBuffer;
	if (Positions.GetNumVertices() == 0 || UVs.GetNumVertices() != Positions.GetNumVertices()
		|| !Positions.GetAllowCPUAccess() || !UVs.GetAllowCPUAccess() || UVs.GetNumTexCoords() == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("NoseSkin: %s vertex buffers not readable (verts %u/%u, cpu %d/%d, uvs %u)"), *Mesh->GetName(),
			Positions.GetNumVertices(), UVs.GetNumVertices(), Positions.GetAllowCPUAccess(), UVs.GetAllowCPUAccess(), UVs.GetNumTexCoords());
		return false;
	}

	// The base-color atlas: the material's largest sRGB 2D texture whose
	// name doesn't mark it as a normal/mask/emissive map.
	UMaterialInterface* Material = SkeletalMesh->GetMaterial(0);
	if (!Material) { return false; }
	// Texture parameter overrides first (the Synty instances set the atlas
	// through a "BaseTexture" parameter), then anything sampled directly in
	// the parent material's graph. (GetUsedTextures came back empty for
	// these instances in PIE.)
	TArray<UTexture*> Textures;
	{
		TArray<FMaterialParameterInfo> Infos;
		TArray<FGuid> Ids;
		Material->GetAllTextureParameterInfo(Infos, Ids);
		for (const FMaterialParameterInfo& Info : Infos)
		{
			UTexture* Tex = nullptr;
			if (Material->GetTextureParameterValue(Info, Tex) && Tex) { Textures.AddUnique(Tex); }
		}
		if (UMaterial* Parent = Material->GetMaterial())
		{
			for (const TObjectPtr<UObject>& Referenced : Parent->GetReferencedTextures())
			{
				if (UTexture* Tex = Cast<UTexture>(Referenced.Get())) { Textures.AddUnique(Tex); }
			}
		}
	}
	UTexture2D* Atlas = nullptr;
	for (UTexture* Texture : Textures)
	{
		UTexture2D* Tex2D = Cast<UTexture2D>(Texture);
		if (!Tex2D || !Tex2D->SRGB || !Tex2D->Source.IsValid()) { continue; }
		const FString Name = Tex2D->GetName();
		if (Name.Contains(TEXT("Normal")) || Name.EndsWith(TEXT("_N")) || Name.Contains(TEXT("Emis")) || Name.Contains(TEXT("Mask")) || Name.Contains(TEXT("Metal"))) { continue; }
		if (!Atlas || Tex2D->Source.GetSizeX() * Tex2D->Source.GetSizeY() > Atlas->Source.GetSizeX() * Atlas->Source.GetSizeY()) { Atlas = Tex2D; }
	}
	if (!Atlas)
	{
		FString Names; for (UTexture* T : Textures) { Names += T ? T->GetName() + TEXT(" ") : TEXT("null "); }
		UE_LOG(LogTemp, Log, TEXT("NoseSkin: no usable atlas among [%s] for %s"), *Names, *Material->GetName());
		return false;
	}
	const ETextureSourceFormat Format = Atlas->Source.GetFormat();
	if (Format != TSF_BGRA8) { UE_LOG(LogTemp, Log, TEXT("NoseSkin: %s source format %d not BGRA8"), *Atlas->GetName(), static_cast<int32>(Format)); return false; }
	TArray64<uint8> Pixels;
	if (!Atlas->Source.GetMipData(Pixels, 0)) { UE_LOG(LogTemp, Log, TEXT("NoseSkin: %s mip data unavailable"), *Atlas->GetName()); return false; }
	const int32 W = Atlas->Source.GetSizeX();
	const int32 H = Atlas->Source.GetSizeY();
	if (Pixels.Num() < static_cast<int64>(W) * H * 4) { UE_LOG(LogTemp, Log, TEXT("NoseSkin: %s mip data short (%lld bytes)"), *Atlas->GetName(), Pixels.Num()); return false; }

	// Where the nose sits on the bind-pose mesh: head bone reference pose,
	// then the nose offset in that bone's frame.
	const FReferenceSkeleton& RefSkel = Mesh->GetRefSkeleton();
	int32 Bone = RefSkel.FindBoneIndex(AttachedBoneName);
	if (Bone == INDEX_NONE) { UE_LOG(LogTemp, Log, TEXT("NoseSkin: bone %s not on %s"), *AttachedBoneName.ToString(), *Mesh->GetName()); return false; }
	FTransform HeadCS = FTransform::Identity;
	for (int32 B = Bone; B != INDEX_NONE; B = RefSkel.GetParentIndex(B)) { HeadCS = HeadCS * RefSkel.GetRefBonePose()[B]; }
	const FVector NosePoint = HeadCS.TransformPosition(HeadFrameConversion.RotateVector(NoseRelativeLocation));

	// Vote over the texels under the face vertices around the nose (the
	// atlas is flat-painted cells, so a coarse color bucket isolates the
	// skin cell from eyes/brows/hair that may also be nearby).
	const auto Sample = [&](const FVector2f& UV) -> FColor
	{
		const int32 X = FMath::Clamp(FMath::FloorToInt(FMath::Frac(UV.X) * W), 0, W - 1);
		const int32 Y = FMath::Clamp(FMath::FloorToInt(FMath::Frac(UV.Y) * H), 0, H - 1);
		const uint8* P = Pixels.GetData() + (static_cast<int64>(Y) * W + X) * 4;
		return FColor(P[2], P[1], P[0], 255);
	};
	// Votes are weighted by triangle AREA, not vertex count: the eyes right
	// next to the nose are dense little meshes (measured: black pupils and
	// white eyeballs out-voted the skin on every rig by vertex count) while
	// the skin around the nose is a few large flat faces. Near-black and
	// near-white texels are skipped outright -- no Synty skin is either.
	struct FBucket { FLinearColor Sum = FLinearColor::Black; float Weight = 0.0f; int32 Count = 0; };
	TMap<uint32, FBucket> Buckets;
	const auto Vote = [&](uint32 VertexIndex, float Weight)
	{
		const FColor C = Sample(UVs.GetVertexUV(VertexIndex, 0));
		const uint8 MaxC = FMath::Max3(C.R, C.G, C.B);
		const uint8 MinC = FMath::Min3(C.R, C.G, C.B);
		if (MaxC < 40 || MinC > 235) { return; }
		const uint32 Key = ((C.R >> 4) << 8) | ((C.G >> 4) << 4) | (C.B >> 4);
		FBucket& Bucket = Buckets.FindOrAdd(Key);
		Bucket.Sum += FLinearColor(C) * Weight;
		Bucket.Weight += Weight;
		Bucket.Count += 1;
	};
	// Sample the face BELOW the nose point (cheeks, upper lip): eyes and
	// eyebrows sit level with or above the nose, hair/hoods above that --
	// measured: a plain radius around the nose picked the eyebrows (large
	// dark quads) on the Dungeon Realms men.
	const auto NearFace = [&](const FVector& P, float Radius) -> bool
	{
		const FVector D = P - NosePoint;
		return D.Z <= 1.0f && D.Z >= -1.5f * Radius && (D.X * D.X + D.Y * D.Y) <= Radius * Radius;
	};
	const FRawStaticIndexBuffer16or32Interface* Indices = LOD.MultiSizeIndexContainer.IsIndexBufferValid() ? LOD.MultiSizeIndexContainer.GetIndexBuffer() : nullptr;
	const bool bTriangles = Indices && Indices->GetNeedsCPUAccess() && Indices->Num() >= 3;
	for (float Radius : { 8.0f, 14.0f, 25.0f })
	{
		Buckets.Reset();
		if (bTriangles)
		{
			for (int32 T = 0; T + 2 < Indices->Num(); T += 3)
			{
				const uint32 I0 = Indices->Get(T), I1 = Indices->Get(T + 1), I2 = Indices->Get(T + 2);
				if (I0 >= Positions.GetNumVertices() || I1 >= Positions.GetNumVertices() || I2 >= Positions.GetNumVertices()) { continue; }
				const FVector P0(Positions.VertexPosition(I0)), P1(Positions.VertexPosition(I1)), P2(Positions.VertexPosition(I2));
				if (!NearFace((P0 + P1 + P2) / 3.0f, Radius)) { continue; }
				Vote(I0, 0.5f * FVector::CrossProduct(P1 - P0, P2 - P0).Size());
			}
		}
		else
		{
			for (uint32 V = 0; V < Positions.GetNumVertices(); ++V)
			{
				if (NearFace(FVector(Positions.VertexPosition(V)), Radius)) { Vote(V, 1.0f); }
			}
		}
		const FBucket* BestBucket = nullptr;
		for (const TPair<uint32, FBucket>& Pair : Buckets)
		{
			if (!BestBucket || Pair.Value.Weight > BestBucket->Weight) { BestBucket = &Pair.Value; }
		}
		if (BestBucket && BestBucket->Count >= 3 && BestBucket->Weight > 0.0f)
		{
			OutColor = BestBucket->Sum / BestBucket->Weight;
			OutColor.A = 1.0f;
			UE_LOG(LogTemp, Log, TEXT("NoseSkin: %s r=%.0f %s weight=%.1f n=%d buckets=%d -> (%.2f %.2f %.2f) from %s"), *Mesh->GetName(), Radius,
				bTriangles ? TEXT("tris") : TEXT("verts"), BestBucket->Weight, BestBucket->Count, Buckets.Num(), OutColor.R, OutColor.G, OutColor.B, *Atlas->GetName());
			return true;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("NoseSkin: %s too few vertices near nose point (%.1f %.1f %.1f)"), *Mesh->GetName(), NosePoint.X, NosePoint.Y, NosePoint.Z);
	return false;
#else
	return false;
#endif
}

void AFaceController::CycleMaterial()
{
	if (MaterialOptions.Num() == 0)
	{
		return;
	}
	SetMaterialIndex((CurrentMaterialIndex + 1) % MaterialOptions.Num());
}

void AFaceController::SetMaterialIndex(int32 Index)
{
	if (!MaterialOptions.IsValidIndex(Index) || !TargetSkeletalMesh)
	{
		return;
	}

	CurrentMaterialIndex = Index;
	UMaterialInterface* Material = MaterialOptions[CurrentMaterialIndex];

	TargetSkeletalMesh->SetMaterial(0, Material);
	ApplyMaterialToAttachments(Material);
}

void AFaceController::SetHighlightVisible(bool bVisible)
{
	HighlightComponent->SetVisibility(bVisible);
}

bool AFaceController::IsHighlightVisible() const
{
	return HighlightComponent->IsVisible();
}

void AFaceController::SetHighlightColor(FLinearColor NewColor)
{
	HighlightColor = NewColor;
	if (HighlightMID)
	{
		HighlightMID->SetVectorParameterValue(TEXT("BeamColor"), HighlightColor);
	}
}
