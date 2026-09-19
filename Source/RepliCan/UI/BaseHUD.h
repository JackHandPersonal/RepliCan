// Minimal HUD: a small white dot fixed at the center of the viewport, used
// as a reticle for pointing at things in the world (see ABaseCharacter's
// look-at system, which raycasts from the camera through this same point).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "BaseHUD.generated.h"

// The HUD face, shared by both of ABaseHUD's translation units (see BaseHUDSight.cpp):
// declared here and defined once in BaseHUD.cpp, never a file-scope static in each.
class UFont* HudMonoFont();

UCLASS()
class REPLICAN_API ABaseHUD : public AHUD
{
	GENERATED_BODY()

public:

	// Width/height in pixels of the reticle dot.
	UPROPERTY(EditAnywhere, Category = "Reticle")
	float ReticleSize = 4.0f;

	// The weapon reticle: a centre dot inside a ring broken into arcs, where the ring's radius
	// is the actual projection of the weapon's spread cone onto the screen. It is not a mood
	// indicator -- a shot can land anywhere inside the ring and nowhere outside it, so the
	// player reading it is reading the truth about the next trigger pull.
	UPROPERTY(EditAnywhere, Category = "Reticle") int32 ReticleArcs = 4;
	UPROPERTY(EditAnywhere, Category = "Reticle") float ReticleArcSweepDegrees = 54.0f;
	UPROPERTY(EditAnywhere, Category = "Reticle") float ReticleThickness = 2.0f;
	// Floors and ceilings on the ring in pixels, so a pinpoint weapon still shows a ring and a
	// wild one does not draw off the edge of the screen.
	UPROPERTY(EditAnywhere, Category = "Reticle") float ReticleMinRadius = 4.0f;
	UPROPERTY(EditAnywhere, Category = "Reticle") float ReticleMaxRadius = 220.0f;
	UPROPERTY(EditAnywhere, Category = "Reticle") float ReticleInterpSpeed = 16.0f;

	// Aiming down the sights swaps the ring for a crosshair: four ticks pointing in at a centre
	// dot. The ring says "somewhere in here"; the crosshair says "here". That is the honest
	// distinction, since ADS is where the spread is small enough to mean a point rather than an
	// area -- and the change of shape is also the clearest possible read that the sights are up.
	UPROPERTY(EditAnywhere, Category = "Reticle") float CrosshairGap = 5.0f;    // clear space around the dot
	UPROPERTY(EditAnywhere, Category = "Reticle") float CrosshairArm = 9.0f;    // length of each tick
	// The arms still ride the spread cone, so a crosshair opening up under fire reads the same
	// way the ring did. The gap is the cone; the arms hang off it.
	UPROPERTY(EditAnywhere, Category = "Reticle") bool bCrosshairTracksSpread = true;
	UPROPERTY(EditAnywhere, Category = "Reticle") FLinearColor ReticleColor = FLinearColor(0.25f, 0.91f, 0.41f, 0.9f);

	virtual void DrawHUD() override;

private:

	// Upper-left: FPS + frame time, smoothed so the number doesn't jitter
	// every single frame. Upper-right: which locomotion set is active
	// ('O' toggle) and current ground speed -- exactly the numbers needed
	// to see live whether a speed/skating fix is actually taking effect,
	// without guessing from how it looks.
	void DrawPerformanceOverlay();
	void DrawLocomotionOverlay();

	// Top-center: name of whichever AttackAnims entry 'P' has currently
	// selected (see ABaseCharacter::SelectedAttackIndex/CyclePose) -- left
	// mouse button plays it.
	void DrawAttackPreviewOverlay();
	// A SMART SIGHT'S FIGURES: range to whatever is under the reticle and what is left in the
	// magazine, beside the aim mark and only while such a sight is actually up. Drawn here rather
	// than as a widget because it belongs to the reticle -- it should sit where the eye already is,
	// and disappear the moment the weapon comes down.
	void DrawSmartOptic(float CenterX, float CenterY);
	/** The fitted sight's mark, in screen space, on the point of impact. Returns true when it drew
	 *  one -- the ordinary spread reticle then stands down, because two marks on one aim point is
	 *  worse than either alone. */
	bool DrawOpticReticle(float CenterX, float CenterY);
	/** The scope picture: black over the whole view but a circle in the middle. Returns true when
	 *  the sight is blacked out entirely (jammed against something), so nothing else is drawn. */
	bool DrawScopeOverlay(float CenterX, float CenterY);
	UPROPERTY(Transient) TObjectPtr<class UMaterialInstanceDynamic> ScopeMaskMID;
	bool bScopeBlackedOut = false;

	// The armed reticle. Returns false when nothing is in hand, so the caller falls back to
	// the plain dot the rest of the game points with.
	bool DrawWeaponReticle(float CenterX, float CenterY);

	float SmoothedFPS = 0.0f;
	// Eased, so the ring opens and closes as a movement rather than jumping a frame after
	// every shot. Negative until the first armed frame gives it something to start from.
	float ReticleRadius = -1.0f;
};
