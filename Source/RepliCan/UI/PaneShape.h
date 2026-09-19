#pragma once
#include "CoreMinimal.h"

// THE SHAPE OF EVERY RENDER-TARGET PANE IN THE UI, in one place.
//
// A render target has an aspect and the box it is shown in has an aspect, and if the two disagree
// the picture is stretched on one axis -- a figure a third wider than it really is, which is useless
// for judging a hold or a likeness, and which nothing in the code complains about. Worse, an
// ORTHOGRAPHIC capture takes its vertical extent from the target's aspect, so a mismatched target is
// wrong twice: it renders the wrong shape and then that shape is stretched again to fit.
//
// It happened because each pane carried the number twice -- once where the target was made, once
// where the box was sized -- as two unrelated literals that had to agree by hand. The hand tuning
// pane was made 900x900 and shown in a 620x450 frame. These constants exist so the pair can only
// ever be written once.
namespace PaneShape
{
	// width / height.
	constexpr float HandTune = 620.0f / 450.0f;   // the hold, beside its table of numbers
	constexpr float WeaponPreview = 1.618f;       // the Reference page's turntable
	constexpr float Mirror = 0.625f;              // the character sheet's standing figure: tall

	/** The height a box of this shape needs for a given width. */
	constexpr float HeightFor(float Aspect, float Width) { return Width / Aspect; }

	/** The render-target height to pair with a given target width, so the two cannot disagree. */
	inline int32 TargetHeight(float Aspect, int32 Width)
	{
		return FMath::Max(16, FMath::RoundToInt(Width / FMath::Max(0.01f, Aspect)));
	}
}
