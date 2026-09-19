#include "UI/BaseHUD.h"
#include "Engine/Canvas.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Components/SkeletalMeshComponent.h"
#include "Characters/BaseCharacter.h"
#include "Core/BasePlayerController.h"
#include "Characters/CharacterAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "UI/CrtStyle.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"

// The HUD draws in the same fixed-pitch face as the rest of the UI (the engine's DroidSansMono
// font asset), falling back to the small engine font if it is ever missing.
UFont* HudMonoFont()
{
	UFont* F = Crt::Font();
	return F ? F : (GEngine ? GEngine->GetSmallFont() : nullptr);
}

void ABaseHUD::DrawHUD()
{
	bScopeBlackedOut = false;
	Super::DrawHUD();

	if (!Canvas) { return; }

	float CenterX = Canvas->SizeX * 0.5f;
	float CenterY = Canvas->SizeY * 0.5f;
	// While free looking the camera has swung away from the aim, so screen centre is no longer
	// where the weapon points. The reticle follows the AIM: it slides across the screen and
	// stays on whatever the character is still pointing at, which is the whole point of being
	// able to look around without turning. The same when the aim is swaying: the reticle wanders
	// with the point of aim, so what it covers is what the shot goes toward. Centre otherwise.
	if (const ABaseCharacter* Me = Cast<ABaseCharacter>(GetOwningPawn()))
	{
		if (Me->IsFreelook() || !Me->GetAimSway().IsNearlyZero())
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
	// No aim mark under a page: the Reference, the sheet, a transfer or the pause menu covers
	// the world, and a circle in the middle of it read as a ghost of something.
	const ABasePlayerController* PC = Cast<ABasePlayerController>(GetOwner());
	const bool bScreen = PC && PC->IsPageOpen();
	// THE SCOPE PICTURE FIRST, then the mark inside it.
	// A FITTED SIGHT OWNS THE AIM MARK. Down the sights you are looking through glass with its own
	// reticle on it, so the spread ring has nothing to add and two marks on one point read as a
	// fault. Off the sights, or with no optic, the ordinary ring comes back.
	if (!bScreen)
	{
		bScopeBlackedOut = DrawScopeOverlay(CenterX, CenterY);
		if (!bScopeBlackedOut && !DrawOpticReticle(CenterX, CenterY) && !DrawWeaponReticle(CenterX, CenterY))
		{
			DrawRect(FLinearColor::White, CenterX - ReticleSize * 0.5f, CenterY - ReticleSize * 0.5f, ReticleSize, ReticleSize);
		}
	}

	// The readout lives inside the sight picture. Drawn after the mask so it is never under it, and
	// not at all when the sight is blacked out.
	if (!bScreen && !bScopeBlackedOut) { DrawSmartOptic(CenterX, CenterY); }

	// The development overlays are intentionally not drawn: the frame counter (the metrics
	// panel on F11 carries it, with far more), the upper-right locomotion readout, and the
	// centred combat-clip selector. Their draw functions are kept for when one is wanted back.
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
