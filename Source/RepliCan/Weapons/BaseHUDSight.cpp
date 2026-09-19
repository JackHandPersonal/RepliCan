// THE SIGHT PICTURE. Everything drawn because of what the character is holding: the spread
// reticle, the scope mask, a fitted sight's own reticle, and the smart optic's readout.
//
// Split out of BaseHUD.cpp because it is not user interface. The dialogs -- the pause menu, the
// sheet, the Reference, a transfer -- are UMG widgets and are what "UI" means in this project.
// This is weapon rendering that happens to reach the screen through AHUD's canvas, and it moves
// with the weapons and optics work, not with the panels. Splitting the file is what lets the two
// be worked on at the same time without touching one another.
//
// These are still ABaseHUD members: one HUD object, two translation units. HudMonoFont() is
// declared in BaseHUD.h and defined once in BaseHUD.cpp rather than being a file-scope static in
// both -- with adaptive unity builds merging .cpp files, two statics of the same name in one
// merged unit is a redefinition, and it appears only once the build decides to merge them.

#include "UI/BaseHUD.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Characters/BaseCharacter.h"
#include "Core/BasePlayerController.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"

bool ABaseHUD::DrawWeaponReticle(float CenterX, float CenterY)
{
	const ABaseCharacter* Me = Cast<ABaseCharacter>(GetOwningPawn());
	if (!Me || Me->GetWeaponStance().IsEmpty()) { ReticleRadius = -1.0f; return false; }

	// Looking through an optic: the glass has its own reticle, and it is collimated, so it is
	// the honest one. Drawing ours over it would put a second aiming mark on screen a few
	// pixels away from the first, which is worse than having neither.
	// Third person aims over the shoulder, where the glass cannot be read: the dot and cross stay.
	if (Me->IsAiming() && Me->HasOpticSight() && Me->IsFirstPerson())
	{
		ReticleRadius = -1.0f;
		return true;   // handled: draw nothing at all, not even the plain dot
	}

	// Degrees of cone -> pixels on this screen, at this field of view. The ring is the cone,
	// projected: half the screen width spans tan(FOV/2), so the spread spans the same
	// fraction of it that tan(spread) is of that.
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	const float Fov = (PC && PC->PlayerCameraManager) ? PC->PlayerCameraManager->GetFOVAngle() : 90.0f;
	const float HalfFovTan = FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(Fov, 10.0f, 170.0f) * 0.5f));
	const float SpreadTan = FMath::Tan(FMath::DegreesToRadians(FMath::Max(Me->GetWeaponSpreadDegrees(), 0.0f)));
	const float Target = FMath::Clamp((Canvas->SizeX * 0.5f) * SpreadTan / FMath::Max(HalfFovTan, KINDA_SMALL_NUMBER),
		ReticleMinRadius, ReticleMaxRadius);

	const float DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;
	ReticleRadius = (ReticleRadius < 0.0f) ? Target : FMath::FInterpTo(ReticleRadius, Target, DeltaSeconds, ReticleInterpSpeed);

	// The dot. Deliberately the same size as the unarmed one: it is still the point the world
	// is queried through, and it should not move or change when a weapon comes out.
	DrawRect(ReticleColor, CenterX - ReticleSize * 0.5f, CenterY - ReticleSize * 0.5f, ReticleSize, ReticleSize);

	if (Me->IsAiming())
	{
		// Four ticks pointing in at the dot, from the spread cone outward. Nothing is drawn
		// inside CrosshairGap, so the dot always sits in clear space however tight the cone is.
		const float Inner = bCrosshairTracksSpread ? FMath::Max(ReticleRadius, CrosshairGap) : CrosshairGap;
		const float Outer = Inner + CrosshairArm;
		DrawLine(CenterX, CenterY - Inner, CenterX, CenterY - Outer, ReticleColor, ReticleThickness);
		DrawLine(CenterX, CenterY + Inner, CenterX, CenterY + Outer, ReticleColor, ReticleThickness);
		DrawLine(CenterX - Inner, CenterY, CenterX - Outer, CenterY, ReticleColor, ReticleThickness);
		DrawLine(CenterX + Inner, CenterY, CenterX + Outer, CenterY, ReticleColor, ReticleThickness);
		return true;
	}

	// The arcs. Each is drawn as a run of short chords -- enough of them that the eye reads a
	// curve, few enough that a wide ring is not hundreds of draw calls.
	const int32 Arcs = FMath::Max(1, ReticleArcs);
	const float Sweep = FMath::Clamp(ReticleArcSweepDegrees, 5.0f, 360.0f / Arcs);
	const int32 Steps = FMath::Clamp(FMath::CeilToInt(ReticleRadius * 0.25f), 4, 24);
	for (int32 Arc = 0; Arc < Arcs; ++Arc)
	{
		// Gaps centred on the diagonals, so the four arcs sit above, below, left and right and
		// the dot is never crowded.
		const float Mid = 360.0f * Arc / Arcs;
		const float Start = Mid - Sweep * 0.5f;
		for (int32 Step = 0; Step < Steps; ++Step)
		{
			const float A0 = FMath::DegreesToRadians(Start + Sweep * Step / Steps);
			const float A1 = FMath::DegreesToRadians(Start + Sweep * (Step + 1) / Steps);
			DrawLine(CenterX + FMath::Cos(A0) * ReticleRadius, CenterY + FMath::Sin(A0) * ReticleRadius,
			         CenterX + FMath::Cos(A1) * ReticleRadius, CenterY + FMath::Sin(A1) * ReticleRadius,
			         ReticleColor, ReticleThickness);
		}
	}
	return true;
}

bool ABaseHUD::DrawScopeOverlay(float CenterX, float CenterY)
{
	ABasePlayerController* PC = Cast<ABasePlayerController>(GetOwner());
	if (!PC || !Canvas) { return false; }
	float Radius = 0.34f, Alpha = 0.0f; bool bBlocked = false;
	if (!PC->OpticOverlayInfo(Radius, Alpha, bBlocked)) { return false; }

	if (!ScopeMaskMID)
	{
		UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/RepliCan/Materials/M_ScopeMask.M_ScopeMask"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (Base) { ScopeMaskMID = UMaterialInstanceDynamic::Create(Base, this); }
	}
	if (!ScopeMaskMID) { return false; }

	// Jammed into a wall: the whole sight goes dark. Closing the opening to nothing is the same
	// drawing call, so there is no second path to keep working.
	const float Shown = bBlocked ? 0.0f : Radius;
	ScopeMaskMID->SetScalarParameterValue(TEXT("Radius"), Shown * Alpha + 0.75f * (1.0f - Alpha));
	ScopeMaskMID->SetScalarParameterValue(TEXT("Soft"), FMath::Max(0.004f, Shown * 0.08f));
	// The opening has to be round on screen, not an oval, so the material is told the shape of it.
	ScopeMaskMID->SetScalarParameterValue(TEXT("Aspect"), (Canvas->SizeY > 0) ? ((float)Canvas->SizeX / (float)Canvas->SizeY) : 1.7777f);
	// Drawn over the entire view, so its UVs span the screen and nothing is left uncovered.
	DrawMaterialSimple(ScopeMaskMID, 0.0f, 0.0f, Canvas->SizeX, Canvas->SizeY, 1.0f, false);
	return bBlocked;
}

bool ABaseHUD::DrawOpticReticle(float CenterX, float CenterY)
{
	ABasePlayerController* PC = Cast<ABasePlayerController>(GetOwner());
	if (!PC) { return false; }
	FString Kind; FLinearColor Colour; float Zoom = 1.0f, Alpha = 0.0f;
	if (!PC->OpticReticleInfo(Kind, Colour, Zoom, Alpha)) { return false; }

	// THE GLASS OWNS THE MARK WHENEVER YOU CAN SEE THE GLASS. Down a looked-through sight in first
	// person there is already a real reticle on the lens, drawn by the optic's own material,
	// collimated and parallax-correct. Drawing this screen-space one as well put TWO marks on one
	// aim point -- and they disagreed, because the screen mark rides the aim's own wander while the
	// dot on the glass rides the weapon. Two answers to "where am I pointing" is worse than either
	// alone. A SCOPE is the opposite case and keeps this mark: at magnification its tube is hidden
	// from its owner and covered by the overlay, so there is no glass left to carry one.
	if (const ABaseCharacter* Me = Cast<ABaseCharacter>(GetOwningPawn()))
	{
		if (Me->IsFirstPerson() && !Me->OpticUsesOverlay() && Me->CarryAdsAlpha() > 0.5f) { return false; }
	}
	Colour.A *= Alpha;

	// SIZED IN SCREEN PIXELS, NOT IN THE WORLD. A reticle is a mark on glass a few centimetres from
	// the eye: it does not grow when the view narrows. Holding it to a fixed pixel size is what
	// makes a 6x scope feel like a 6x scope -- the world gets bigger behind a mark that does not.
	const float S = FMath::Max(1.0f, Canvas ? Canvas->SizeY / 1080.0f : 1.0f);   // one size at every resolution
	const FLinearColor Ink = Colour;

	auto Dot = [&](float R) { DrawRect(Ink, CenterX - R, CenterY - R, R * 2.0f, R * 2.0f); };
	auto Ring = [&](float R, float Thick)
	{
		const int32 Steps = 48;
		for (int32 i = 0; i < Steps; ++i)
		{
			const float A0 = 2.0f * PI * i / Steps;
			const float A1 = 2.0f * PI * (i + 1) / Steps;
			DrawLine(CenterX + FMath::Cos(A0) * R, CenterY + FMath::Sin(A0) * R,
			         CenterX + FMath::Cos(A1) * R, CenterY + FMath::Sin(A1) * R, Ink, Thick);
		}
	};
	auto Arm = [&](float Dx, float Dy, float Gap, float Len, float Thick)
	{
		DrawLine(CenterX + Dx * Gap, CenterY + Dy * Gap,
		         CenterX + Dx * (Gap + Len), CenterY + Dy * (Gap + Len), Ink, Thick);
	};

	if (Kind == TEXT("dot"))
	{
		Dot(1.6f * S);                       // a red dot is a dot: nothing else belongs on the glass
	}
	else if (Kind == TEXT("circle_dot"))
	{
		Dot(1.6f * S);
		Ring(15.0f * S, 1.6f * S);           // the holographic ring, well clear of the dot
	}
	else if (Kind == TEXT("chevron"))
	{
		const float W = 9.0f * S, H = 11.0f * S, T = 1.8f * S;
		DrawLine(CenterX - W, CenterY - H, CenterX, CenterY, Ink, T);
		DrawLine(CenterX + W, CenterY - H, CenterX, CenterY, Ink, T);
		DrawLine(CenterX, CenterY + 4.0f * S, CenterX, CenterY + 16.0f * S, Ink, T);
	}
	else   // "crosshair", and anything unrecognised: a scope reticle is the safe default
	{
		const float Gap = 7.0f * S, Len = 34.0f * S, T = 1.5f * S;
		Arm(-1.0f, 0.0f, Gap, Len, T);
		Arm(1.0f, 0.0f, Gap, Len, T);
		Arm(0.0f, -1.0f, Gap, Len, T);
		Arm(0.0f, 1.0f, Gap, Len, T);
		Dot(1.2f * S);
		// HOLDOVER MARKS below the centre, the way a ranging reticle carries them. Spaced by the
		// magnification: a stronger scope sees further, so its marks stand for longer distances and
		// sit closer together on the glass.
		const float Step = FMath::Max(7.0f, 17.0f / FMath::Max(1.0f, Zoom)) * S;
		for (int32 i = 1; i <= 3; ++i)
		{
			const float Y = CenterY + Gap + Len * 0.35f + Step * i;
			const float HalfW = (i == 2 ? 7.0f : 4.5f) * S;
			DrawLine(CenterX - HalfW, Y, CenterX + HalfW, Y, Ink, 1.2f * S);
		}
	}
	return true;
}

void ABaseHUD::DrawSmartOptic(float CenterX, float CenterY)
{
	ABasePlayerController* PC = Cast<ABasePlayerController>(GetOwner());
	UFont* Font = HudMonoFont();
	if (!PC || !Font || !Canvas) { return; }
	float RangeM = 0.0f; int32 Rounds = 0, Mag = 0;
	if (!PC->SmartOpticInfo(RangeM, Rounds, Mag)) { return; }

	// Nothing in front of the muzzle is NOT a range. Showing the far end of the trace would be a
	// number that looks measured and is not, which is worse than three dashes.
	const FString RangeText = (RangeM > 0.05f) ? FString::Printf(TEXT("%.0fm"), RangeM) : TEXT("---m");
	const FString AmmoText = (Mag > 0) ? FString::Printf(TEXT("%d/%d"), Rounds, Mag) : FString();

	// Down and right of the aim mark, clear of the reticle's own arms at every spread.
	const float X = CenterX + 34.0f;
	float Y = CenterY + 16.0f;
	Canvas->SetDrawColor(FColor(150, 255, 170, 230));
	Canvas->DrawText(Font, RangeText, X, Y);
	if (!AmmoText.IsEmpty())
	{
		float XL = 0.0f, YL = 0.0f;
		Canvas->StrLen(Font, RangeText, XL, YL);
		Y += FMath::Max(12.0f, YL);
		// The last few rounds go amber: a count you have to read is no use in a firefight, a colour
		// change is seen without reading.
		const bool bLow = Mag > 0 && Rounds <= FMath::Max(1, Mag / 4);
		Canvas->SetDrawColor(bLow ? FColor(255, 190, 80, 235) : FColor(150, 255, 170, 230));
		Canvas->DrawText(Font, AmmoText, X, Y);
	}
}

