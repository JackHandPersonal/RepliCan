#include "World/SignActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/CanvasRenderTarget2D.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	// 5x7 glyphs, one row per byte (bit 4 = left column), for ' '..'Z' minus what a sign never needs.
	struct FGlyph { TCHAR Ch; uint8 Rows[7]; };
	const FGlyph Glyphs[] = {
		{ TEXT(' '), { 0, 0, 0, 0, 0, 0, 0 } },
		{ TEXT('A'), { 0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 } }, { TEXT('B'), { 0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E } },
		{ TEXT('C'), { 0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E } }, { TEXT('D'), { 0x1C, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1C } },
		{ TEXT('E'), { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F } }, { TEXT('F'), { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10 } },
		{ TEXT('G'), { 0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F } }, { TEXT('H'), { 0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 } },
		{ TEXT('I'), { 0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E } }, { TEXT('J'), { 0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C } },
		{ TEXT('K'), { 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11 } }, { TEXT('L'), { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F } },
		{ TEXT('M'), { 0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11 } }, { TEXT('N'), { 0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11 } },
		{ TEXT('O'), { 0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E } }, { TEXT('P'), { 0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10 } },
		{ TEXT('Q'), { 0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D } }, { TEXT('R'), { 0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11 } },
		{ TEXT('S'), { 0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E } }, { TEXT('T'), { 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 } },
		{ TEXT('U'), { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E } }, { TEXT('V'), { 0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04 } },
		{ TEXT('W'), { 0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A } }, { TEXT('X'), { 0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11 } },
		{ TEXT('Y'), { 0x11, 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04 } }, { TEXT('Z'), { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F } },
		{ TEXT('0'), { 0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E } }, { TEXT('1'), { 0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E } },
		{ TEXT('2'), { 0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F } }, { TEXT('3'), { 0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E } },
		{ TEXT('4'), { 0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02 } }, { TEXT('5'), { 0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E } },
		{ TEXT('6'), { 0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E } }, { TEXT('7'), { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 } },
		{ TEXT('8'), { 0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E } }, { TEXT('9'), { 0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C } },
		{ TEXT('.'), { 0, 0, 0, 0, 0, 0x0C, 0x0C } }, { TEXT(','), { 0, 0, 0, 0, 0x0C, 0x04, 0x08 } },
		{ TEXT(':'), { 0, 0x0C, 0x0C, 0, 0x0C, 0x0C, 0 } }, { TEXT('-'), { 0, 0, 0, 0x1F, 0, 0, 0 } },
		{ TEXT('!'), { 0x04, 0x04, 0x04, 0x04, 0x04, 0, 0x04 } }, { TEXT('?'), { 0x0E, 0x11, 0x01, 0x02, 0x04, 0, 0x04 } },
		{ TEXT('/'), { 0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10 } }, { TEXT('\''), { 0x04, 0x04, 0x08, 0, 0, 0, 0 } },
		{ TEXT('('), { 0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02 } }, { TEXT(')'), { 0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08 } },
		{ TEXT('>'), { 0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08 } }, { TEXT('<'), { 0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02 } },
		{ TEXT('+'), { 0, 0x04, 0x04, 0x1F, 0x04, 0x04, 0 } }, { TEXT('='), { 0, 0, 0x1F, 0, 0x1F, 0, 0 } },
		{ TEXT('&'), { 0x0C, 0x12, 0x14, 0x08, 0x15, 0x12, 0x0D } }, { TEXT('*'), { 0, 0x15, 0x0E, 0x1F, 0x0E, 0x15, 0 } },
	};
	const FGlyph* FindGlyph(TCHAR C)
	{
		C = FChar::ToUpper(C);
		for (const FGlyph& G : Glyphs) { if (G.Ch == C) { return &G; } }
		return nullptr;
	}
}

ASignActor::ASignActor()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Housing = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Housing"));
	Housing->SetupAttachment(Root);
	Housing->SetCollisionProfileName(TEXT("NoCollision"));
	Face = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Face"));
	Face->SetupAttachment(Root);
	Face->SetCollisionProfileName(TEXT("NoCollision"));
	Face->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DotMat(TEXT("/Game/RepliCan/Materials/M_DotMatrix.M_DotMatrix"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DarkMat(TEXT("/Game/RepliCan/Materials/MI_DrabTable.MI_DrabTable"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> White(TEXT("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture"));
	if (Cube.Succeeded()) { Housing->SetStaticMesh(Cube.Object); }
	if (Plane.Succeeded()) { Face->SetStaticMesh(Plane.Object); }
	if (DotMat.Succeeded()) { FaceMaterial = DotMat.Object; }
	if (DarkMat.Succeeded()) { HousingMaterial = DarkMat.Object; }
	if (White.Succeeded()) { WhiteTexture = White.Object; }
}

void ASignActor::SetText(const FString& InText)
{
	Text = InText;
	Rebuild();
}

void ASignActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Rebuild();
}

void ASignActor::BeginPlay()
{
	Super::BeginPlay();
	Rebuild();
}

void ASignActor::Rebuild()
{
	// Geometry: a slim dark box, the LED plane a hair proud of its +X face (local +X = the face normal).
	Housing->SetRelativeLocation(FVector(-Depth * 0.5f, 0.0f, 0.0f));
	Housing->SetRelativeScale3D(FVector(Depth / 100.0f, Width / 100.0f, Height / 100.0f));
	if (HousingMaterial) { Housing->SetMaterial(0, HousingMaterial); }
	// The engine plane is 100 x 100 in XY with +Z its normal. Turn it so the normal is the actor's +X,
	// U (its X) runs along -Y, which is left to right for someone facing the sign (they look down -X,
	// so their right is -Y), and V therefore runs down -Z: DrawText writes rows top-down to match.
	Face->SetRelativeLocation(FVector(0.3f, 0.0f, 0.0f));
	Face->SetRelativeRotation(FRotationMatrix::MakeFromXZ(FVector(0.0f, -1.0f, 0.0f), FVector(1.0f, 0.0f, 0.0f)).Rotator());
	Face->SetRelativeScale3D(FVector(Width / 100.0f, Height / 100.0f, 1.0f));

	// The LED grid is BAKED into the texture at CellPx texels per lamp, rather than stored one
	// texel per lamp and rebuilt into dots by the material. Reconstructing a grid in the shader
	// means sampling the tiny texture once per dot, and any rounding between the dot centres and
	// the texel centres drops whole columns of the font -- which is why the door signs were
	// missing strokes and losing characters. Baked, what is drawn is what is shown, and the
	// texture can be filtered and mipped like any other, which also stops it shimmering at range.
	const int32 Cols = Columns(), GridRows = Rows();
	const int32 W = Cols * CellPx, H = GridRows * CellPx;
	if (Target && Target->SizeX != W) { Target = nullptr; }
	if (!Target) { Target = UCanvasRenderTarget2D::CreateCanvasRenderTarget2D(this, UCanvasRenderTarget2D::StaticClass(), W, H); }
	if (Target)
	{
		Target->Filter = TF_Bilinear;
		Target->ClearColor = FLinearColor::Black;
		// Bind every time, not only on the pass that built it. A play session duplicates this
		// actor with its target still attached, and the copy never rebound: the face then kept
		// no drawn text at all, and an unbound "Text" sampler reads solid white, which lights
		// every LED at once.
		Target->OnCanvasRenderTargetUpdate.RemoveAll(this);
		Target->OnCanvasRenderTargetUpdate.AddDynamic(this, &ASignActor::DrawText);
		Target->UpdateResource();   // draws through OnCanvasRenderTargetUpdate; an immediate update afterwards would wipe it
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Sign '%s': no canvas target, the face will read blank"), *GetActorNameOrLabel());
	}
	if (FaceMaterial && (!FaceMID || FaceMID->Parent != FaceMaterial)) { FaceMID = UMaterialInstanceDynamic::Create(FaceMaterial, this); }
	if (FaceMID)
	{
		FaceMID->SetTextureParameterValue(TEXT("Text"), Target);
		FaceMID->SetScalarParameterValue(TEXT("Intensity"), Target ? Intensity : 0.0f);
		FaceMID->SetVectorParameterValue(TEXT("Color"), Color);
		FaceMID->SetScalarParameterValue(TEXT("Intensity"), Intensity);
		// Left for any older material still expecting them; the baked texture needs neither.
		FaceMID->SetScalarParameterValue(TEXT("Cols"), 1.0f);
		FaceMID->SetScalarParameterValue(TEXT("Rows"), 1.0f);
		Face->SetMaterial(0, FaceMID);
	}
}

void ASignActor::DrawText(UCanvas* Canvas, int32 W, int32 H)
{
	if (!Canvas || !WhiteTexture) { return; }
	const int32 Cols = FMath::Max(1, W / CellPx);
	const int32 GridRows = FMath::Max(1, H / CellPx);

	// The dot itself, centred in its cell with a gap around it: the gap is what reads as a
	// matrix of separate lamps rather than a block of light.
	const float Dot = FMath::Max(1.0f, CellPx * 0.80f);
	const float Pad = (CellPx - Dot) * 0.5f;

	// Every unlit lamp is drawn too, very dim. A real panel shows its whole grid faintly, and
	// without it the lit dots float on black and the sign loses its shape between words.
	Canvas->SetDrawColor(FColor(16, 16, 16, 255));
	for (int32 Row = 0; Row < GridRows; ++Row)
	{
		for (int32 Col = 0; Col < Cols; ++Col)
		{
			Canvas->DrawTile(WhiteTexture, Col * CellPx + Pad, Row * CellPx + Pad, Dot, Dot, 0.0f, 0.0f, 1.0f, 1.0f, BLEND_Opaque);
		}
	}

	// One row of the grid per line of text, stacked with LineGap blank rows between them.
	TArray<FString> Parts;
	Text.ParseIntoArray(Parts, TEXT("|"), false);
	if (Parts.Num() == 0) { Parts.Add(Text); }

	// The usable width, with the bezel margin kept clear at both ends.
	const int32 Usable = FMath::Max(6, Cols - 2 * SideMargin);
	const int32 MaxChars = FMath::Max(1, Usable / 6);

	Canvas->SetDrawColor(FColor::White);
	for (int32 Line = 0; Line < Parts.Num(); ++Line)
	{
		const int32 RowOffset = Line * (7 + LineGap);
		if (RowOffset + 7 > GridRows) { break; }
		FString Shown = Parts[Line].Left(MaxChars);
		const int32 Start = SideMargin + (bCentred ? ((MaxChars - Shown.Len()) / 2) * 6 : 0);
		for (int32 i = 0; i < Shown.Len(); ++i)
		{
			const FGlyph* G = FindGlyph(Shown[i]);
			if (!G) { continue; }
			for (int32 Row = 0; Row < 7; ++Row)
			{
				for (int32 Col = 0; Col < 5; ++Col)
				{
					if (G->Rows[Row] & (0x10 >> Col))
					{
						const int32 Cell = Start + i * 6 + Col;
						if (Cell >= Cols) { continue; }
						// V runs down the face, so glyph row 0 goes to the top row of its line.
						Canvas->DrawTile(WhiteTexture, Cell * CellPx + Pad, (RowOffset + Row) * CellPx + Pad, Dot, Dot, 0.0f, 0.0f, 1.0f, 1.0f, BLEND_Opaque);
					}
				}
			}
		}
	}
}
