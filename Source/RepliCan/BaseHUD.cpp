#include "BaseHUD.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Components/SkeletalMeshComponent.h"
#include "BaseCharacter.h"
#include "CharacterAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "CrtStyle.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"

// The HUD draws in the same fixed-pitch face as the rest of the UI (the engine's DroidSansMono
// font asset), falling back to the small engine font if it is ever missing.
static UFont* HudMonoFont()
{
	UFont* F = Crt::Font();
	return F ? F : (GEngine ? GEngine->GetSmallFont() : nullptr);
}

void ABaseHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas) { return; }

	float CenterX = Canvas->SizeX * 0.5f;
	float CenterY = Canvas->SizeY * 0.5f;
	// While free looking the camera has swung away from the aim, so screen centre is no longer
	// where the weapon points. The reticle follows the AIM: it slides across the screen and
	// stays on whatever the character is still pointing at, which is the whole point of being
	// able to look around without turning. Drawn at centre the rest of the time, as before.
	if (const ABaseCharacter* Me = Cast<ABaseCharacter>(GetOwningPawn()))
	{
		if (Me->IsFreelook())
		{
			const FVector Along = Me->GetAimOrigin() + Me->GetAimRotation().Vector() * 6000.0f;
			FVector2D Screen;
			if (UGameplayStatics::ProjectWorldToScreen(Cast<APlayerController>(GetOwner()), Along, Screen, false))
			{
				CenterX = Screen.X;
				CenterY = Screen.Y;
			}
		}
	}
	if (!DrawWeaponReticle(CenterX, CenterY))
	{
		DrawRect(FLinearColor::White, CenterX - ReticleSize * 0.5f, CenterY - ReticleSize * 0.5f, ReticleSize, ReticleSize);
	}

	// The development overlays are intentionally not drawn: the frame counter (the metrics
	// panel on F11 carries it, with far more), the upper-right locomotion readout, and the
	// centred combat-clip selector. Their draw functions are kept for when one is wanted back.
}

bool ABaseHUD::DrawWeaponReticle(float CenterX, float CenterY)
{
	const ABaseCharacter* Me = Cast<ABaseCharacter>(GetOwningPawn());
	if (!Me || Me->GetWeaponStance().IsEmpty()) { ReticleRadius = -1.0f; return false; }

	// Looking through an optic: the glass has its own reticle, and it is collimated, so it is
	// the honest one. Drawing ours over it would put a second aiming mark on screen a few
	// pixels away from the first, which is worse than having neither.
	if (Me->IsAiming() && Me->HasOpticSight())
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

void ABaseHUD::DrawPerformanceOverlay()
{
	const float DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;
	if (DeltaSeconds > 0.0f)
	{
		const float InstantFPS = 1.0f / DeltaSeconds;
		// Exponential smoothing (not a plain running average) so the number
		// settles quickly after a real change instead of dragging out a long
		// history -- SmoothedFPS <= 0 only on the very first frame, where
		// there's nothing to interpolate from yet.
		SmoothedFPS = (SmoothedFPS <= 0.0f) ? InstantFPS : FMath::FInterpTo(SmoothedFPS, InstantFPS, DeltaSeconds, 5.0f);
	}

	UFont* Font = HudMonoFont();
	if (!Font) { return; }

	const float Margin = 10.0f;
	float Y = Margin;
	Canvas->SetDrawColor(FColor::White);
	Canvas->DrawText(Font, FString::Printf(TEXT("FPS: %.0f"), SmoothedFPS), Margin, Y);
	Y += 16.0f;
	Canvas->DrawText(Font, FString::Printf(TEXT("Frame: %.2f ms"), DeltaSeconds * 1000.0f), Margin, Y);
}

void ABaseHUD::DrawLocomotionOverlay()
{
	UFont* Font = HudMonoFont();
	if (!Font) { return; }

	const ABaseCharacter* Character = Cast<ABaseCharacter>(GetOwningPawn());
	const UCharacterAnimInstance* AnimInst = Character ? Cast<UCharacterAnimInstance>(Character->GetMesh()->GetAnimInstance()) : nullptr;
	const ECharacterLocomotionSet Set = AnimInst ? AnimInst->GetCurrentLocomotionSet() : ECharacterLocomotionSet::Lyra;
	const TCHAR* SetName = (Set == ECharacterLocomotionSet::Synty) ? TEXT("Synty") : TEXT("Lyra");
	const float Speed = Character ? Character->GetVelocity().Size2D() : 0.0f;
	const UAnimSequence* CurrentAnim = AnimInst ? AnimInst->GetCurrentAnim() : nullptr;
	const FString AnimName = CurrentAnim ? CurrentAnim->GetName() : TEXT("None");

	const float Margin = 10.0f;
	float Y = Margin;
	Canvas->SetDrawColor(FColor::White);

	// Right-aligned: measured per-line since "Locomotion: Synty" and
	// "Speed: 133" aren't the same width, and a fixed X would either clip
	// the wider line or leave the narrower one looking oddly indented.
	const auto DrawRightAligned = [this, Font, Margin, &Y](const FString& Text)
	{
		float XL = 0.0f, YL = 0.0f;
		Canvas->StrLen(Font, Text, XL, YL);
		Canvas->DrawText(Font, Text, Canvas->SizeX - Margin - XL, Y);
		Y += YL + 2.0f;
	};

	DrawRightAligned(FString::Printf(TEXT("Locomotion: %s"), SetName));
	DrawRightAligned(FString::Printf(TEXT("Speed: %.0f"), Speed));
	DrawRightAligned(FString::Printf(TEXT("Anim: %s"), *AnimName));

	// Diagnostic for the Synty skating fix -- lets the actual play rate
	// being used be confirmed against the calibrated formula's expected
	// value (see WalkAnimSpeed's header comment on FLocomotionAnimSet)
	// instead of only judging by how the movement looks.
	const float PlayRate = AnimInst ? AnimInst->GetCurrentPlayRate() : 1.0f;
	DrawRightAligned(FString::Printf(TEXT("PlayRate: %.2f"), PlayRate));
}

void ABaseHUD::DrawAttackPreviewOverlay()
{
	UFont* Font = HudMonoFont();
	if (!Font) { return; }

	const ABaseCharacter* Character = Cast<ABaseCharacter>(GetOwningPawn());
	const UCharacterAnimInstance* AnimInst = Character ? Cast<UCharacterAnimInstance>(Character->GetMesh()->GetAnimInstance()) : nullptr;
	if (!Character || !AnimInst) { return; }

	const int32 Index = Character->GetSelectedCombatAnimIndex();
	const int32 NumClips = AnimInst->GetNumAllCombatAnims();
	const UAnimSequence* SelectedAnim = AnimInst->GetAllCombatAnim(Index);
	const FString CombatText = FString::Printf(TEXT("Combat [%d/%d]: %s"), Index + 1, NumClips, SelectedAnim ? *SelectedAnim->GetName() : TEXT("None"));

	Canvas->SetDrawColor(FColor::White);
	float Y = 10.0f;

	float XL = 0.0f, YL = 0.0f;
	Canvas->StrLen(Font, CombatText, XL, YL);
	Canvas->DrawText(Font, CombatText, (Canvas->SizeX - XL) * 0.5f, Y);
}
