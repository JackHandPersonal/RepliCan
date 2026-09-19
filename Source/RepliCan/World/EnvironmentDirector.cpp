#include "World/EnvironmentDirector.h"
#include "World/PestActor.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Core/RepliCanUserSettings.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/PointLight.h"
#include "Engine/SkyLight.h"
#include "Components/SkyLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "EngineUtils.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

// How big a box the motes live in, around the camera. Anything outside it is invisible anyway,
// so the same few hundred instances are recycled rather than filling the facility.
static const float DustBoxSize = 900.0f;
static const int32 DustPerStep = 90;     // instances per density step

AEnvironmentDirector::AEnvironmentDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

AEnvironmentDirector* AEnvironmentDirector::Ensure(UWorld* World)
{
	if (!World) { return nullptr; }
	for (TActorIterator<AEnvironmentDirector> It(World); It; ++It) { return *It; }
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	return World->SpawnActor<AEnvironmentDirector>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
}

void AEnvironmentDirector::BeginPlay()
{
	Super::BeginPlay();
	ApplyEnvironment();
}

void AEnvironmentDirector::EndPlay(const EEndPlayReason::Type Reason)
{
	// Hand the lamps back exactly as they were found, or a director that ran once would leave
	// every lamp sitting at whatever point of its flicker it happened to stop on.
	for (const FLamp& L : Lamps)
	{
		if (APointLight* Light = L.Light.Get())
		{
			if (Light->PointLightComponent) { Light->PointLightComponent->SetIntensity(L.BaseIntensity); }
		}
	}
	Super::EndPlay(Reason);
}

void AEnvironmentDirector::ApplyEnvironment()
{
	ApplyPostProcess();
	ApplyFog();
	ApplyDust();
	GatherLamps();
}

// ---- Post process ----------------------------------------------------------
// The level ships with no post process volume at all, so everything the renderer does was
// running at engine defaults: no exposure control, no ambient occlusion worth the name, and a
// vignette and motion blur nobody chose. One unbound volume is the single biggest change
// available to the look of the facility.
void AEnvironmentDirector::ApplyPostProcess()
{
	UWorld* W = GetWorld();
	if (!W) { return; }
	const URepliCanUserSettings* S = URepliCanUserSettings::Get();
	if (!Volume)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		Volume = W->SpawnActor<APostProcessVolume>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	}
	if (!Volume) { return; }
	Volume->bUnbound = true;
	Volume->Priority = 1.0f;
	Volume->BlendWeight = 1.0f;

	FPostProcessSettings& PP = Volume->Settings;
	const bool bGrade = !S || S->bEnvGrade;
	const bool bAO = !S || S->bEnvAmbientOcclusion;

	// Ambient occlusion. Synty kits are large flat planes meeting at hard corners, and darkening
	// those corners does most of the shading work the geometry cannot.
	PP.bOverride_AmbientOcclusionIntensity = true;
	// AO at CORRIDOR radius is the other half of "the air has weight": it darkens where surfaces
	// approach each other, so a doorway reads as a depth rather than a hole in a wall. It costs
	// nothing at these radii and, unlike a fog card, it is never a thing you can see the edge of.
	// Steps 0..7 are the ladder the settings dial offers: 7 is already as much AO as the shot
	// can carry without the corners reading as dirt. Steps 8 and 9 exist to SHOW what the
	// effect is -- intensity is pinned at Unreal's maximum of 1 and the two dials that actually
	// have headroom are opened up instead. Radius decides how far from a corner the darkening
	// reaches, so a big radius stops being contact shadow and becomes "every recess in the room
	// is a cave"; power is the curve, so a high power drives the middle of that gradient to
	// black. They are deliberately too much and are not meant to be left on.
	static const float AOAmount[10] = { 0.0f, 0.22f, 0.36f, 0.50f, 0.62f, 0.76f, 0.88f, 1.00f, 1.00f, 1.00f };
	static const float AORadius[10] = { 90.0f, 90.0f, 90.0f, 90.0f, 90.0f, 90.0f, 90.0f, 90.0f, 250.0f, 700.0f };
	static const float AOPower [10] = { 2.2f, 2.2f, 2.2f, 2.2f, 2.2f, 2.2f, 2.2f, 2.2f, 3.6f, 6.5f };
	const int32 AOStep = S ? FMath::Clamp(S->EnvAOStrength, 0, 9) : 4;
	// AO ONLY DARKENS AMBIENT LIGHT, and this level barely has any: the SkyLight is at 0.06 and
	// the project runs with no dynamic GI, so almost every photon comes straight from a point
	// lamp. Multiplying a near-zero ambient term by an occlusion factor is still near zero,
	// which is why turning the intensity up to its maximum changed nothing you could see. The
	// demo steps therefore raise the ambient as well -- otherwise they are a dial connected to
	// nothing.
	ApplySkyLightForAO(bAO ? AOStep : 0);
	PP.AmbientOcclusionIntensity = bAO ? AOAmount[AOStep] : 0.0f;
	PP.bOverride_AmbientOcclusionRadius = true;
	PP.AmbientOcclusionRadius = AORadius[AOStep];    // corridor scale, not object scale
	PP.bOverride_AmbientOcclusionPower = true;
	PP.AmbientOcclusionPower = AOPower[AOStep];
	// Radius is expressed in world units, not screen space, so a wide radius does not fall apart
	// when the player backs away from the wall. That costs more, which is the other reason the
	// demo steps are not the default.
	PP.bOverride_AmbientOcclusionRadiusInWS = true;
	PP.AmbientOcclusionRadiusInWS = true;
	// Fade it out over distance rather than letting it band on the far wall. The demo steps push
	// that out too, or the effect stops at arm's length and there is nothing to look at.
	PP.bOverride_AmbientOcclusionFadeDistance = true;
	PP.AmbientOcclusionFadeDistance = AOStep >= 8 ? 20000.0f : 6000.0f;

	// EXPOSURE IS LEFT ALONE BY DEFAULT, and this is deliberate. With no post process volume in
	// the level the renderer was running its own auto exposure, adapted to a dark facility.
	// Pinning manual exposure in its place lifted the whole map several stops: the first version
	// of this director did exactly that and the map simply looked brighter. Not overriding it at
	// all is the only way to guarantee the light level is unchanged, because the thing that set
	// the level before is still the thing setting it now.
	//
	// Fixed exposure is still worth having -- a corridor game breathes every time the player
	// turns around -- so it is offered as its own switch, off unless asked for, with a bias the
	// player can step until it matches what they want.
	const bool bFixed = S && S->bEnvFixedExposure;
	PP.bOverride_AutoExposureMethod = bFixed;
	PP.bOverride_AutoExposureBias = bFixed;
	if (bFixed)
	{
		PP.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
		// Steps either side of a middle setting, so the row reads as a dial rather than a number.
		static const float Bias[5] = { 9.5f, 10.25f, 11.0f, 11.75f, 12.5f };
		PP.AutoExposureBias = Bias[FMath::Clamp(S ? S->EnvExposure : 2, 0, 4)];
	}

	// A grade that keeps the facility cold and slightly green without tinting the lamps.
	PP.bOverride_ColorSaturation = true;
	PP.ColorSaturation = bGrade ? FVector4(0.94f, 1.00f, 0.97f, 1.0f) : FVector4(1, 1, 1, 1);
	PP.bOverride_ColorContrast = true;
	PP.ColorContrast = bGrade ? FVector4(1.06f, 1.05f, 1.08f, 1.0f) : FVector4(1, 1, 1, 1);
	PP.bOverride_ColorGamma = true;
	// Gamma is left at unity: lifting it is the other quiet way to brighten a whole map.
	PP.ColorGamma = FVector4(1, 1, 1, 1);

	PP.bOverride_BloomIntensity = true;
	PP.BloomIntensity = bGrade ? 0.22f : 0.0f;       // a suggestion around the lamps, not a glow
	PP.bOverride_BloomThreshold = true;
	PP.BloomThreshold = 1.1f;

	// Chosen rather than inherited: the engine's defaults put a 0.4 vignette and half a unit of
	// motion blur on everything, which is what made the previews look dirty and smeared.
	PP.bOverride_VignetteIntensity = true;
	PP.VignetteIntensity = bGrade ? 0.28f : 0.0f;
	PP.bOverride_MotionBlurAmount = true;
	PP.MotionBlurAmount = 0.12f;
	PP.bOverride_SceneFringeIntensity = true;
	PP.SceneFringeIntensity = bGrade ? 0.4f : 0.0f;
	PP.bOverride_FilmGrainIntensity = true;
	PP.FilmGrainIntensity = bGrade ? 0.08f : 0.0f;
}

// ---- Fog -------------------------------------------------------------------
// The one that changes the look most. Volumetric fog is what turns the ceiling panels from
// bright rectangles into lamps that throw a visible cone down the corridor, and it is what puts
// air between the player and the far end of a room.
void AEnvironmentDirector::ApplyFog()
{
	UWorld* W = GetWorld();
	if (!W) { return; }
	const URepliCanUserSettings* S = URepliCanUserSettings::Get();
	const bool bOn = !S || S->bEnvFog;
	const bool bVolumetric = (!S || S->bEnvVolumetricFog) && bOn;
	const int32 Step = S ? FMath::Clamp(S->EnvFogDensity, 0, 7) : 3;

	if (!Fog)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		Fog = W->SpawnActor<AExponentialHeightFog>(FVector(0.0f, 0.0f, -200.0f), FRotator::ZeroRotator, Params);
	}
	UExponentialHeightFogComponent* C = Fog ? Fog->GetComponent() : nullptr;
	if (!C) { return; }

	// Indoors the fog has to be thin: enough to read the light shafts, not enough to fog the far
	// wall of a room the player is standing in.
	// Halved. These were set by eye against an empty grey room; with the facility dressed, lit
	// and carrying its own ground haze on top, the same numbers read as weather indoors. The
	// dial still spans the same useful range, it just starts far lower -- step 2 is now what
	// step 1 used to be, and the old step 2 is available at step 4 for anyone who wants it.
	// EIGHT steps, and the whole range sits below where the old five even started. Fog indoors
	// is a small number: the difference between "the far wall is slightly softer" and "it is
	// raining inside" is about 0.01, so a table that jumps 0.01 at a time has no useful middle.
	// This is the thing the sprite fog could never do and a density can.
	static const float Density[8] = { 0.0f, 0.0015f, 0.003f, 0.0055f, 0.009f, 0.014f, 0.022f, 0.034f };
	C->SetFogDensity(bOn ? Density[Step] : 0.0f);
	C->SetFogHeightFalloff(0.08f);                   // nearly flat: the facility is one storey
	// Inscattering is light the fog itself adds. Kept dark, or thin fog quietly raises the
	// black level of every corridor it touches.
	C->SetFogInscatteringColor(FLinearColor(0.16f, 0.21f, 0.23f, 1.0f));
	// Nothing within a room's width of the player gets fogged at all. Indoors this matters far
	// more than outdoors: every wall is close, so a short start distance fogs the room you are
	// standing in rather than the distance.
	C->SetStartDistance(300.0f);
	C->SetVolumetricFog(bVolumetric);
	// Volumetric fog is what makes the light shafts; extinction is how much it also swallows.
	// Near 1 it does both, and the swallowing is what reads as "heavy". Low keeps the shafts
	// and stops the fog dimming everything behind it.
	C->SetVolumetricFogExtinctionScale(0.35f);
	// Scattering is what makes air look like it is THERE -- light picking out the volume rather
	// than the volume swallowing light. Tied to the same dial so one control moves the whole
	// impression instead of needing two to be balanced against each other.
	C->SetVolumetricFogScatteringDistribution(0.2f + 0.05f * Step);
	C->SetVolumetricFogDistance(6000.0f);
	C->SetVolumetricFogScatteringDistribution(0.35f);
	C->SetVolumetricFogAlbedo(FColor(140, 158, 163));
	C->SetVisibility(bOn, true);
}

// ---- Dust ------------------------------------------------------------------
void AEnvironmentDirector::ApplyDust()
{
	const URepliCanUserSettings* S = URepliCanUserSettings::Get();
	const bool bOn = !S || S->bEnvDust;
	const int32 Step = S ? FMath::Clamp(S->EnvDustDensity, 0, 4) : 2;
	const int32 Want = bOn ? Step * DustPerStep : 0;

	if (!Dust)
	{
		Dust = NewObject<UInstancedStaticMeshComponent>(this, TEXT("DustMotes"));
		Dust->SetupAttachment(RootComponent);
		Dust->RegisterComponent();
		Dust->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Dust->SetCastShadow(false);
		Dust->SetMobility(EComponentMobility::Movable);
		// Motes must not be lit, must not receive the fog's own lighting pass, and must never
		// occlude anything: they are a hint of air, not geometry.
		Dust->bAffectDistanceFieldLighting = false;
		Dust->SetReceivesDecals(false);
		if (UStaticMesh* Quad = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")))
		{
			Dust->SetStaticMesh(Quad);
		}
		if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/RepliCan/Materials/M_DustMote.M_DustMote")))
		{
			Dust->SetMaterial(0, Mat);
		}
		else
		{
			// Without its material a mote would draw as a solid white card, which is far worse
			// than no dust at all.
			UE_LOG(LogTemp, Warning, TEXT("EnvironmentDirector: M_DustMote is missing; run Tools/make_env_materials.py. Dust is off."));
			Dust->SetVisibility(false);
			return;
		}
		Dust->SetVisibility(true);
	}

	if (Dust->GetInstanceCount() != Want)
	{
		Dust->ClearInstances();
		DustPosition.Reset(); DustVelocity.Reset();
		const FVector Centre = DustBoxCentre;
		for (int32 i = 0; i < Want; ++i)
		{
			const FVector P = Centre + FVector(
				FMath::FRandRange(-DustBoxSize, DustBoxSize),
				FMath::FRandRange(-DustBoxSize, DustBoxSize),
				FMath::FRandRange(-DustBoxSize * 0.45f, DustBoxSize * 0.45f));
			DustPosition.Add(P);
			// Barely moving, and mostly sideways: still air with a little circulation in it.
			DustVelocity.Add(FVector(FMath::FRandRange(-6.0f, 6.0f), FMath::FRandRange(-6.0f, 6.0f), FMath::FRandRange(-2.5f, 3.5f)));
			FTransform T(FRotator::ZeroRotator, P, FVector(FMath::FRandRange(0.008f, 0.020f)));
			Dust->AddInstance(T, true);
		}
	}
	Dust->SetVisibility(Want > 0);
}

void AEnvironmentDirector::TickDust(float DeltaSeconds)
{
	if (!Dust || Dust->GetInstanceCount() == 0) { return; }
	const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!Cam) { return; }
	const FVector Eye = Cam->GetCameraLocation();
	// The box follows the camera. A mote that falls out of one side is put back in at the other,
	// so the same instances serve the whole facility and none are ever wasted off screen.
	DustBoxCentre = Eye;
	const FRotator Face = Cam->GetCameraRotation() + FRotator(90.0f, 0.0f, 0.0f);   // the quad faces the eye
	const int32 Count = FMath::Min(Dust->GetInstanceCount(), DustPosition.Num());
	for (int32 i = 0; i < Count; ++i)
	{
		FVector P = DustPosition[i] + DustVelocity[i] * DeltaSeconds;
		// A slow wander so they do not travel in straight lines.
		DustVelocity[i] += FVector(
			FMath::Sin(Clock * 0.7f + i * 0.37f), FMath::Cos(Clock * 0.5f + i * 0.61f), FMath::Sin(Clock * 0.9f + i * 0.23f)) * DeltaSeconds * 1.2f;
		DustVelocity[i] = DustVelocity[i].GetClampedToMaxSize(9.0f);
		const FVector Rel = P - Eye;
		if (FMath::Abs(Rel.X) > DustBoxSize) { P.X -= FMath::Sign(Rel.X) * 2.0f * DustBoxSize; }
		if (FMath::Abs(Rel.Y) > DustBoxSize) { P.Y -= FMath::Sign(Rel.Y) * 2.0f * DustBoxSize; }
		if (FMath::Abs(Rel.Z) > DustBoxSize * 0.45f) { P.Z -= FMath::Sign(Rel.Z) * 0.9f * DustBoxSize; }
		DustPosition[i] = P;
		FTransform T;
		Dust->GetInstanceTransform(i, T, true);
		T.SetLocation(P);
		T.SetRotation(Face.Quaternion());
		Dust->UpdateInstanceTransform(i, T, true, i == Count - 1, true);
	}
}

// ---- Lamps -----------------------------------------------------------------
// Every ceiling lamp the layout tool placed carries a lightid tag. The red emergency lamps are
// a different class with their own animation, so they are left alone: this is the small,
// constant unsteadiness of a working fixture, not a fault.
void AEnvironmentDirector::GatherLamps()
{
	if (bLampsGathered) { return; }
	UWorld* W = GetWorld();
	if (!W) { return; }
	for (TActorIterator<APointLight> It(W); It; ++It)
	{
		APointLight* Light = *It;
		if (!Light || !Light->PointLightComponent) { continue; }
		bool bTagged = false;
		for (const FName& Tag : Light->Tags) { if (Tag.ToString().StartsWith(TEXT("lightid:"))) { bTagged = true; break; } }
		if (!bTagged) { continue; }
		FLamp L;
		L.Light = Light;
		L.BaseIntensity = Light->PointLightComponent->Intensity;
		L.Phase = FMath::FRandRange(0.0f, 100.0f);
		L.Rate = FMath::FRandRange(5.0f, 13.0f);
		L.Depth = FMath::FRandRange(0.015f, 0.05f);
		Lamps.Add(L);
	}
	bLampsGathered = true;
}

void AEnvironmentDirector::TickFlicker(float DeltaSeconds)
{
	const URepliCanUserSettings* S = URepliCanUserSettings::Get();
	const bool bOn = !S || S->bEnvLampFlicker;
	for (const FLamp& L : Lamps)
	{
		APointLight* Light = L.Light.Get();
		if (!Light || !Light->PointLightComponent) { continue; }
		if (!bOn) { Light->PointLightComponent->SetIntensity(L.BaseIntensity); continue; }
		// Two beats of different speed, so the wobble never settles into a pulse the eye can
		// follow, plus a rare deeper dip as though the supply sagged.
		const float T = Clock * L.Rate + L.Phase;
		const float Wobble = 0.6f * FMath::Sin(T) + 0.4f * FMath::Sin(T * 1.7f + 1.1f);
		const float Sag = FMath::Max(0.0f, FMath::Sin(Clock * 0.23f + L.Phase) - 0.985f) * 40.0f;
		Light->PointLightComponent->SetIntensity(L.BaseIntensity * (1.0f + Wobble * L.Depth - Sag));
	}
}

void AEnvironmentDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Clock += DeltaSeconds;
	TickDust(DeltaSeconds);
	TickFlicker(DeltaSeconds);
	TickPests(DeltaSeconds);
}

// ---- Switches --------------------------------------------------------------
static void SaveSettings() { if (URepliCanUserSettings* S = URepliCanUserSettings::Get()) { S->SaveSettings(); } }

void AEnvironmentDirector::EnvFog(int32 OnOff)        { if (URepliCanUserSettings* S = URepliCanUserSettings::Get()) { S->bEnvFog = OnOff != 0; } SaveSettings(); ApplyFog(); }
void AEnvironmentDirector::EnvVolumetric(int32 OnOff) { if (URepliCanUserSettings* S = URepliCanUserSettings::Get()) { S->bEnvVolumetricFog = OnOff != 0; } SaveSettings(); ApplyFog(); }
void AEnvironmentDirector::EnvAO(int32 OnOff)         { if (URepliCanUserSettings* S = URepliCanUserSettings::Get()) { S->bEnvAmbientOcclusion = OnOff != 0; } SaveSettings(); ApplyPostProcess(); }
void AEnvironmentDirector::EnvFixedExposure(int32 OnOff) { if (URepliCanUserSettings* S = URepliCanUserSettings::Get()) { S->bEnvFixedExposure = OnOff != 0; } SaveSettings(); ApplyPostProcess(); }
void AEnvironmentDirector::EnvExposure(int32 Step)      { if (URepliCanUserSettings* S = URepliCanUserSettings::Get()) { S->EnvExposure = FMath::Clamp(Step, 0, 4); } SaveSettings(); ApplyPostProcess(); }
void AEnvironmentDirector::EnvGrade(int32 OnOff)      { if (URepliCanUserSettings* S = URepliCanUserSettings::Get()) { S->bEnvGrade = OnOff != 0; } SaveSettings(); ApplyPostProcess(); }
void AEnvironmentDirector::EnvAOStrength(int32 Step)
{
	if (URepliCanUserSettings* S = URepliCanUserSettings::Get())
	{
		S->EnvAOStrength = FMath::Clamp(Step, 0, 9);
	}
	SaveSettings();
	ApplyEnvironment();
}

void AEnvironmentDirector::EnvDust(int32 OnOff)       { if (URepliCanUserSettings* S = URepliCanUserSettings::Get()) { S->bEnvDust = OnOff != 0; } SaveSettings(); ApplyDust(); }
void AEnvironmentDirector::EnvFlicker(int32 OnOff)    { if (URepliCanUserSettings* S = URepliCanUserSettings::Get()) { S->bEnvLampFlicker = OnOff != 0; } SaveSettings(); }
void AEnvironmentDirector::EnvFogDensity(int32 Step)  { if (URepliCanUserSettings* S = URepliCanUserSettings::Get()) { S->EnvFogDensity = FMath::Clamp(Step, 0, 4); } SaveSettings(); ApplyFog(); }
void AEnvironmentDirector::EnvDustDensity(int32 Step) { if (URepliCanUserSettings* S = URepliCanUserSettings::Get()) { S->EnvDustDensity = FMath::Clamp(Step, 0, 4); } SaveSettings(); ApplyDust(); }

void AEnvironmentDirector::ApplySkyLightForAO(int32 Step)
{
	UWorld* W = GetWorld();
	if (!W) { return; }
	ASkyLight* Sky = nullptr;
	for (TActorIterator<ASkyLight> It(W); It; ++It) { Sky = *It; break; }
	if (!Sky || !Sky->GetLightComponent()) { return; }
	USkyLightComponent* C = Sky->GetLightComponent();
	// Whatever the level shipped with is the baseline, captured once. Never hard-coded: the
	// level is the user's to light and this must put back exactly what it found.
	if (SkyLightBaseIntensity < 0.0f) { SkyLightBaseIntensity = C->Intensity; }

	// Only the two demo steps touch it. Steps 0..7 are the real ladder and leave the level's
	// own lighting alone.
	const float Want = (Step == 9) ? 1.00f : (Step == 8) ? 0.45f : SkyLightBaseIntensity;
	if (!FMath::IsNearlyEqual(C->Intensity, Want, 0.001f))
	{
		C->SetIntensity(Want);
		C->MarkRenderStateDirty();
	}
}

// ---- Pests -----------------------------------------------------------------

void AEnvironmentDirector::LoadPestMeshes()
{
	if (PestMeshes.Num() > 0) { return; }
	static const TCHAR* Paths[] = {
		TEXT("/Game/RepliCan/Pests/SM_Pest_Tick_01.SM_Pest_Tick_01"),
		TEXT("/Game/RepliCan/Pests/SM_Pest_Crawler_01.SM_Pest_Crawler_01"),
		TEXT("/Game/RepliCan/Pests/SM_Pest_Hopper_01.SM_Pest_Hopper_01"),
	};
	for (const TCHAR* P : Paths)
	{
		if (UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, P, nullptr, LOAD_NoWarn | LOAD_Quiet)) { PestMeshes.Add(M); }
	}
}


bool AEnvironmentDirector::CanPlayerSee(const FVector& Point) const
{
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC || !PC->PlayerCameraManager) { return false; }
	const FVector Eye = PC->PlayerCameraManager->GetCameraLocation();
	const FVector Forward = PC->PlayerCameraManager->GetActorForwardVector();
	const FVector ToPoint = (Point - Eye);
	const FVector Dir = ToPoint.GetSafeNormal();
	// Behind, or far enough out to the side, counts as unseen. 0.35 is roughly a 70 degree
	// half-angle: wider than the actual frustum on purpose, so a pest does not appear at the
	// very edge of the screen where the eye is most likely to catch the pop.
	if (FVector::DotProduct(Forward, Dir) < 0.35f) { return false; }
	// In front, but is there anything between? A crate, a bulkhead, a table leg will do.
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PestVisibility), false, PC->GetPawn());
	const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Hit, Eye, Point + FVector(0, 0, 4.0f), ECC_Visibility, Params);
	return !bBlocked;
}

bool AEnvironmentDirector::FindHiddenFloorPoint(const FVector& Around, float MinRadius, float MaxRadius, FVector& Out) const
{
	UWorld* W = GetWorld();
	if (!W) { return false; }
	for (int32 Try = 0; Try < 24; ++Try)
	{
		const float Angle = FMath::FRandRange(0.0f, 360.0f);
		const float Radius = FMath::FRandRange(MinRadius, MaxRadius);
		const FVector Guess = Around + FRotator(0.0f, Angle, 0.0f).Vector() * Radius;
		// Find the deck under the guess. No floor means the guess landed outside the room.
		FHitResult Down;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(PestFloor), false);
		if (!W->LineTraceSingleByChannel(Down, Guess + FVector(0, 0, 200.0f), Guess - FVector(0, 0, 400.0f), ECC_Visibility, Params))
		{
			continue;
		}
		const FVector Floor = Down.ImpactPoint + FVector(0, 0, 1.0f);
		if (CanPlayerSee(Floor)) { continue; }
		Out = Floor;
		return true;
	}
	return false;
}

void AEnvironmentDirector::TickPests(float DeltaSeconds)
{
	const URepliCanUserSettings* Settings = URepliCanUserSettings::Get();
	const bool bEnabled = !Settings || Settings->bEnvPests;

	// Sweep the ones that have finished. They destroy themselves; this just drops the handles.
	Pests.RemoveAll([](const TObjectPtr<APestActor>& P) { return !IsValid(P); });

	if (!bEnabled)
	{
		for (const TObjectPtr<APestActor>& P : Pests) { if (IsValid(P)) { P->Destroy(); } }
		Pests.Reset();
		return;
	}
	if (Pests.Num() >= MaxPests) { return; }

	PestCountdown -= DeltaSeconds;
	if (PestCountdown > 0.0f) { return; }

	// The rate dial stretches or squeezes the gap rather than changing the behaviour.
	static const float RateScale[5] = { 2.6f, 1.6f, 1.0f, 0.6f, 0.32f };
	const float Scale = Settings ? RateScale[FMath::Clamp(Settings->EnvPestRateStep, 0, 4)] : 1.0f;
	PestCountdown = FMath::FRandRange(PestGapSeconds.X, PestGapSeconds.Y) * Scale;

	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const APawn* Player = PC ? PC->GetPawn() : nullptr;
	if (!Player) { return; }

	LoadPestMeshes();
	if (PestMeshes.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Pests: no meshes in /Game/RepliCan/Pests -- run Tools/make_pests.py"));
		return;
	}

	const FVector Here = Player->GetActorLocation();
	FVector Start, Goal;
	if (!FindHiddenFloorPoint(Here, PestNearCm, PestFarCm, Start)) { return; }
	// The goal is hidden too, and deliberately on the far side: a run that starts and ends
	// behind the player never enters view at all, which is a pest nobody ever sees.
	if (!FindHiddenFloorPoint(Start, PestNearCm * 1.2f, PestFarCm, Goal)) { return; }

	SpawnPestAt(Start, Goal);
}

bool AEnvironmentDirector::SpawnPestAt(const FVector& Where, const FVector& Goal)
{
	if (!GetWorld() || PestMeshes.Num() == 0) { return false; }
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APestActor* Pest = GetWorld()->SpawnActor<APestActor>(APestActor::StaticClass(), FTransform(Where), Params);
	if (!Pest) { return false; }
	Pest->Launch(PestMeshes[FMath::RandRange(0, PestMeshes.Num() - 1)], Where, Goal, PestScale);
	Pests.Add(Pest);
	return true;
}

void AEnvironmentDirector::EnvPestNow()
{
	// Everything the timer does, except the one rule that makes them hard to find: this puts it
	// on the floor IN FRONT of the player, running across their view.
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	APawn* Player = PC ? PC->GetPawn() : nullptr;
	if (!Player) { return; }
	LoadPestMeshes();
	if (PestMeshes.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Pests: no meshes in /Game/RepliCan/Pests -- run Tools/make_pests.py"));
		return;
	}

	const FVector Here = Player->GetActorLocation();
	const FVector Fwd = Player->GetActorForwardVector();
	const FVector Right = Player->GetActorRightVector();
	// Four metres ahead, a metre and a half off to one side, dropped onto whatever floor is
	// under that point so it does not start inside the geometry or in mid-air.
	auto Drop = [this](const FVector& P, FVector& Out) -> bool
	{
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(PestDebugFloor), false);
		if (GetWorld()->LineTraceSingleByChannel(Hit, P + FVector(0, 0, 200.0f), P - FVector(0, 0, 400.0f), ECC_Visibility, Q))
		{
			Out = Hit.ImpactPoint + FVector(0, 0, 1.0f);
			return true;
		}
		return false;
	};
	FVector Start, Goal;
	if (!Drop(Here + Fwd * 400.0f - Right * 160.0f, Start)) { return; }
	if (!Drop(Here + Fwd * 400.0f + Right * 220.0f, Goal)) { Goal = Start + Right * 300.0f; }
	if (SpawnPestAt(Start, Goal))
	{
		UE_LOG(LogTemp, Log, TEXT("Pests: one spawned 4 m ahead of you, running left to right"));
	}
}

void AEnvironmentDirector::EnvPests(int32 OnOff)
{
	if (URepliCanUserSettings* S = URepliCanUserSettings::Get())
	{
		S->bEnvPests = OnOff != 0;
		S->SaveConfig();
	}
	PestCountdown = 0.5f;
}

void AEnvironmentDirector::EnvPestRate(int32 Step)
{
	if (URepliCanUserSettings* S = URepliCanUserSettings::Get())
	{
		S->EnvPestRateStep = FMath::Clamp(Step, 0, 4);
		S->SaveConfig();
	}
	PestCountdown = 0.5f;
}
