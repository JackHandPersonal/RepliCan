// A dot-matrix LED sign: a strip of Columns x 7 round LEDs on a dark housing,
// showing up to Columns/6 characters of Text in a 5x7 bitmap font. The text
// is drawn into a small render target (one texel per LED) and the material
// turns each lit texel into a glowing dot. Local +X is the face normal.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SignActor.generated.h"

class UStaticMeshComponent;
class UCanvasRenderTarget2D;
class UMaterialInstanceDynamic;
class UCanvas;

UCLASS()
class REPLICAN_API ASignActor : public AActor
{
	GENERATED_BODY()

public:
	ASignActor();

	// A vertical bar starts a new row of text: "CAFETERIA|MESS OPEN 24H" is a two line sign.
	// The grid grows to fit however many rows are asked for, and the columns follow from that,
	// so nothing else has to be told about it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sign") FString Text = TEXT("PROMPT CRITICAL SERVICES");
	// Blank lamp rows between one line of text and the next.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sign", meta = (ClampMin = "0", ClampMax = "6")) int32 LineGap = 2;
	// Blank lamp columns kept clear at each end, so the text never runs into the bezel.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sign", meta = (ClampMin = "0", ClampMax = "12")) int32 SideMargin = 3;
	// LED columns (6 per character: 5 dots and a gap) and the strip's size in cm.
	// 0 means work it out from the face: see Columns(). A fixed count is an override for a sign
	// that wants smaller lettering than its shape would choose.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sign") int32 Characters = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sign") float Width = 370.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sign") float Height = 24.0f;
	// How far the housing reaches back from the face, cm. The face sits at the actor origin, so a
	// sign stood off its wall by ten is made ten deeper to close up to it again.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sign") float Depth = 6.0f;

	// The resolution knob. The grid has 7 rows because the font is 7 tall, so on a strip this
	// shallow squareness alone decides how many columns fit, and that caps a 366 x 22 lintel at
	// about 19 characters. This is how much narrower than tall a lamp is allowed to be: 1.0 is
	// perfectly square, and every step above it buys columns.
	//   1.00 -> 19 characters, lamps exactly square
	//   1.25 -> 24 characters, barely distinguishable from square
	//   2.10 -> 41 characters, the original, where lamps read as vertical dashes and the
	//           letters lost their horizontal definition
	// Past about 1.4 the strokes of the font start closing up again at a distance.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sign", meta = (ClampMin = "0.5", ClampMax = "2.5"))
	float LampAspect = 1.25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sign") FLinearColor Color = FLinearColor(1.0f, 0.55f, 0.12f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sign") float Intensity = 5.0f;
	// Text alignment within the strip: 0 left, 1 centre.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sign") bool bCentred = true;

	UFUNCTION(BlueprintCallable, Category = "Sign") void SetText(const FString& InText);

	UPROPERTY(VisibleAnywhere, Category = "Sign") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "Sign") TObjectPtr<UStaticMeshComponent> Housing;
	UPROPERTY(VisibleAnywhere, Category = "Sign") TObjectPtr<UStaticMeshComponent> Face;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	void Rebuild();
	UFUNCTION() void DrawText(UCanvas* Canvas, int32 W, int32 H);

private:
	UPROPERTY() TObjectPtr<UCanvasRenderTarget2D> Target;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> FaceMID;
	UPROPERTY() TObjectPtr<UMaterialInterface> FaceMaterial;
	UPROPERTY() TObjectPtr<UMaterialInterface> HousingMaterial;
	UPROPERTY() TObjectPtr<UTexture2D> WhiteTexture;
	// How many LED columns fit across the face. Sized so a lamp is SQUARE on the wall, which is
	// the thing that was wrong: the strip is 366 x 22, and 41 characters put 246 columns across
	// it, making each lamp 1.5 cm wide by 3.1 cm tall. Dots twice as tall as they are wide read
	// as dashes, adjacent columns of a glyph smear into each other, and a 5-column letter ends up
	// 9 cm across on a wall the player reads from several metres away. Square lamps, sized to the
	// face, give roughly 19 characters here at about 19 cm each.
	// How many lines of text the string asks for.
	int32 LineCount() const
	{
		TArray<FString> Parts;
		Text.ParseIntoArray(Parts, TEXT("|"), false);
		return FMath::Max(1, Parts.Num());
	}
	// Lamp rows: seven per line of the font, plus the gaps between lines.
	int32 Rows() const
	{
		const int32 N = LineCount();
		return 7 * N + LineGap * (N - 1);
	}
	int32 Columns() const
	{
		if (Characters > 0) { return FMath::Max(6, Characters * 6); }
		const float Aspect = Height > 1.0f ? Width / Height : 16.0f;
		const int32 Wide = FMath::RoundToInt(Rows() * Aspect * FMath::Max(0.5f, LampAspect));
		return FMath::Max(6, (Wide / 6) * 6);   // whole characters only
	}
	// Texels per LED cell in the baked face texture. 8 gives a round dot with a clear gap and
	// keeps even a long sign under 2k wide.
	static constexpr int32 CellPx = 12;

};
