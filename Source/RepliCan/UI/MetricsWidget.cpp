#include "UI/MetricsWidget.h"
#include "Core/BasePlayerController.h"
#include "Characters/BaseCharacter.h"
#include "UI/CrtStyle.h"
#include "World/FlickerLightActor.h"
#include "World/SlidingDoorActor.h"
#include "Blueprint/WidgetTree.h"
#include "Components/SizeBox.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "HAL/PlatformMemory.h"
#include "RenderCore.h"
#include "RHI.h"
#include "Styling/CoreStyle.h"

void UMetricsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Box"));
	Box->SetWidthOverride(330.0f);
	Box->SetHeightOverride(10.0f);   // painted; the box only anchors the corner
	WidgetTree->RootWidget = Box;
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UMetricsWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Clock += InDeltaTime;
	SmoothedDelta = FMath::Lerp(SmoothedDelta, InDeltaTime, 0.08f);
	WorstDelta = FMath::Max(WorstDelta, InDeltaTime);
	Refresh -= InDeltaTime;
	if (Refresh <= 0.0f) { Refresh = 0.25f; Sample(); WorstDelta = 0.0f; }
}

void UMetricsWidget::Sample()
{
	Lines.Reset();
	UWorld* World = GetWorld();
	const float Fps = SmoothedDelta > KINDA_SMALL_NUMBER ? 1.0f / SmoothedDelta : 0.0f;
	const float GameMs = FPlatformTime::ToMilliseconds(GGameThreadTime);
	const float RenderMs = FPlatformTime::ToMilliseconds(GRenderThreadTime);
	const float GpuMs = FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
	const int32 DrawCalls = GNumDrawCallsRHI[0];
	const int32 Prims = GNumPrimitivesDrawnRHI[0];
	const FPlatformMemoryStats Mem = FPlatformMemory::GetStats();
	const float UsedGb = Mem.UsedPhysical / (1024.0f * 1024.0f * 1024.0f);

	Lines.Add({ FString::Printf(TEXT("%3.0f fps   %5.1f ms   worst %5.1f"), Fps, SmoothedDelta * 1000.0f, WorstDelta * 1000.0f) });
	Lines.Add({ FString::Printf(TEXT("game %5.1f  draw %5.1f  gpu %5.1f ms"), GameMs, RenderMs, GpuMs), false, true });
	Lines.Add({ FString::Printf(TEXT("draws %5d   tris %6.0fk   mem %4.1f GB"), DrawCalls, Prims / 1000.0f, UsedGb), false, true });

	int32 Characters = 0, SkelComps = 0, Lamps = 0, Doors = 0, DoorsOpen = 0, Actors = 0;
	if (World)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			++Actors;
			if (ABaseCharacter* C = Cast<ABaseCharacter>(*It))
			{
				++Characters;
				TInlineComponentArray<USkeletalMeshComponent*> Comps(C);
				SkelComps += Comps.Num();
			}
			else if (Cast<AFlickerLightActor>(*It)) { ++Lamps; }
			else if (ASlidingDoorActor* D = Cast<ASlidingDoorActor>(*It)) { ++Doors; if (D->GetOpenAmount() > 0.01f) { ++DoorsOpen; } }
		}
	}
	Lines.Add({ FString::Printf(TEXT("actors %d  chars %d (%d skel)  lamps %d  doors %d/%d open"), Actors, Characters, SkelComps, Lamps, DoorsOpen, Doors), false, true });

	if (OwnerController)
	{
		const FString Seq = OwnerController->GetSequenceState();
		Lines.Add({ FString::Printf(TEXT("director  %s"), Seq.IsEmpty() ? TEXT("idle") : *Seq), false, true });
		if (!OwnerController->GetDiagNote().IsEmpty()) { Lines.Add({ FString::Printf(TEXT(">> %s"), *OwnerController->GetDiagNote()), true, false }); }   // who is driving the game right now
		TArray<FString> Live;
		if (OwnerController->IsInConversation()) { Live.Add(TEXT("conversation")); }
		if (OwnerController->IsVoicePlaying()) { Live.Add(TEXT("voice")); }
		if (OwnerController->IsRemoteViewActive()) { Live.Add(TEXT("remote-view")); }
		if (OwnerController->IsBoothActive()) { Live.Add(TEXT("booth")); }
		if (OwnerController->IsAppearanceOpen()) { Live.Add(TEXT("chooser")); }
		if (OwnerController->IsPaused()) { Live.Add(TEXT("paused")); }
		if (OwnerController->IsInCinematic()) { Live.Add(TEXT("cinematic")); }
		Lines.Add({ FString::Printf(TEXT("live      %s"), Live.Num() ? *FString::Join(Live, TEXT(" ")) : TEXT("-")), false, true });
	}

	// Budgets: a 60 Hz frame with headroom.
	if (Fps < 30.0f) { Lines.Add({ TEXT("! LOW FRAME RATE"), true }); }
	if (WorstDelta > 0.1f) { Lines.Add({ FString::Printf(TEXT("! HITCH %.0f ms"), WorstDelta * 1000.0f), true }); }
	if (GpuMs > 25.0f) { Lines.Add({ TEXT("! GPU BOUND"), true }); }
	if (GameMs > 20.0f) { Lines.Add({ TEXT("! GAME THREAD BOUND"), true }); }
	if (RenderMs > 20.0f) { Lines.Add({ TEXT("! RENDER THREAD BOUND"), true }); }
	if (DrawCalls > 2500) { Lines.Add({ TEXT("! DRAW CALLS"), true }); }
	if (UsedGb > 6.0f) { Lines.Add({ TEXT("! MEMORY"), true }); }
	if (SkelComps > 40) { Lines.Add({ TEXT("! SKELETAL MESH COUNT"), true }); }
	if (OwnerController && OwnerController->IsRemoteViewActive() && !OwnerController->IsAppearanceOpen() && !OwnerController->IsInConversation()) { Lines.Add({ TEXT("! REMOTE CAPTURE LEFT RUNNING"), true }); }
}

int32 UMetricsWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	const FSlateFontInfo Font = Crt::Mono(12);
	const float LineH = 16.0f;
	const float Height = 10.0f + Lines.Num() * LineH;
	// A faint ground so the text reads over any scene.
	FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(FVector2f(330.0f, Height), FSlateLayoutTransform()), FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, FLinearColor(0, 0, 0, 0.45f));
	float Y = 5.0f;
	for (const FLine& L : Lines)
	{
		const FLinearColor Color = L.bWarning ? Crt::Amber : (L.bDim ? Crt::DimGreen : Crt::Green);
		const float Blink = L.bWarning ? (0.55f + 0.45f * FMath::Abs(FMath::Sin(Clock * 5.0f))) : 1.0f;
		FSlateDrawElement::MakeText(OutDrawElements, LayerId + 2, AllottedGeometry.ToOffsetPaintGeometry(FVector2D(8.0f, Y)), L.Text, Font, ESlateDrawEffect::None, FLinearColor(Color.R, Color.G, Color.B, Blink));
		Y += LineH;
	}
	return LayerId + 3;
}
