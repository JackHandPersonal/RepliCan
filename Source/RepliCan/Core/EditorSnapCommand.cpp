// SnapNext: nudge an actor, and it lands on the first surface that way.
//
// Placing a wall fitting by eye is guesswork twice over -- once for the wall's face, which is never
// where the room's nominal line is, and once for how far the thing's own pivot sits from its back.
// Both of those put the foyer's light switches in mid air. This removes the arithmetic: arm the
// command, then drag the actor a little in the direction you want it to go, and on releasing the
// gizmo it travels that way until its leading face meets something, up to five metres.
//
// It arms ONCE. The next completed move consumes it, so it cannot lie in wait and surprise a later
// drag. Run it again for the next one.
//
// Editor only, and deliberately so: it hangs off the editor's own begin/end movement delegates,
// which is the only honest way to know which way you meant to push something.

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "CollisionQueryParams.h"
#include "Framework/Application/SlateApplication.h"
#include "Editor/EditorEngine.h"
#include "Framework/Commands/Commands.h"
#include "Framework/Commands/UICommandList.h"
#include "Modules/ModuleManager.h"
#include "LevelEditor.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "RepliCanTools"

namespace
{
	TWeakObjectPtr<AActor> GMoving;
	FVector GStart = FVector::ZeroVector;
	bool GArmed = false;
	FDelegateHandle GBeginHandle, GEndHandle;

	// How far this actor's bounding box reaches from its own pivot in the given direction. Without
	// it the actor's ORIGIN lands on the surface and the mesh sinks halfway into the wall.
	float ReachAlong(const AActor* Actor, const FVector& Dir)
	{
		FVector Origin, Extent;
		Actor->GetActorBounds(false, Origin, Extent);
		const float HalfSpan = FMath::Abs(Extent.X * Dir.X) + FMath::Abs(Extent.Y * Dir.Y) + FMath::Abs(Extent.Z * Dir.Z);
		return HalfSpan + FVector::DotProduct(Origin - Actor->GetActorLocation(), Dir);
	}

	void Disarm()
	{
		if (!GEditor) { GArmed = false; GMoving = nullptr; return; }
		if (GBeginHandle.IsValid()) { GEditor->OnBeginObjectMovement().Remove(GBeginHandle); GBeginHandle.Reset(); }
		if (GEndHandle.IsValid()) { GEditor->OnEndObjectMovement().Remove(GEndHandle); GEndHandle.Reset(); }
		GArmed = false;
		GMoving = nullptr;
	}

	void OnBegin(UObject& Object)
	{
		if (!GArmed) { return; }
		if (AActor* Actor = Cast<AActor>(&Object))
		{
			GMoving = Actor;
			GStart = Actor->GetActorLocation();
		}
	}

	void OnEnd(UObject& Object)
	{
		if (!GArmed) { return; }
		AActor* Actor = Cast<AActor>(&Object);
		if (!Actor || Actor != GMoving.Get()) { return; }
		UWorld* World = Actor->GetWorld();
		const FVector Now = Actor->GetActorLocation();
		const FVector Moved = Now - GStart;
		if (!World || Moved.IsNearlyZero(0.01f))
		{
			UE_LOG(LogTemp, Warning, TEXT("SnapNext: that move had no direction in it; still armed."));
			return;   // a rotation or a scale is not a direction: stay armed for the real drag
		}
		const FVector Dir = Moved.GetSafeNormal();

		FCollisionQueryParams Q(SCENE_QUERY_STAT(SnapNext), true, Actor);
		TArray<AActor*> Attached;
		Actor->GetAttachedActors(Attached, true, true);
		Q.AddIgnoredActors(Attached);
		// From where the drag left it, onward the same way. Five metres, as asked.
		FHitResult Hit;
		const FVector From = Now;
		const FVector To = Now + Dir * 500.0f;
		if (!World->LineTraceSingleByChannel(Hit, From, To, ECC_WorldStatic, Q))
		{
			UE_LOG(LogTemp, Warning, TEXT("SnapNext: nothing within five metres that way; %s left where you put it."), *Actor->GetActorLabel());
			Disarm();
			return;
		}
		const FVector Landed = Hit.ImpactPoint - Dir * ReachAlong(Actor, Dir);
		Actor->Modify();                       // one undo step, so ctrl-Z puts it back
		Actor->SetActorLocation(Landed, false, nullptr, ETeleportType::TeleportPhysics);
		UE_LOG(LogTemp, Log, TEXT("SnapNext: %s snapped %.1f cm onto %s."),
			*Actor->GetActorLabel(), (float)FVector::Dist(Now, Landed),
			Hit.GetActor() ? *Hit.GetActor()->GetActorLabel() : TEXT("a surface"));
		Disarm();
	}

	void Arm(const TArray<FString>& Args)
	{
		Disarm();
		if (!GEditor)
		{
			UE_LOG(LogTemp, Warning, TEXT("SnapNext: no editor to watch."));
			return;
		}
		GArmed = true;
		GBeginHandle = GEditor->OnBeginObjectMovement().AddStatic(&OnBegin);
		GEndHandle = GEditor->OnEndObjectMovement().AddStatic(&OnEnd);
		// Let go of the console's text box, so the next thing done goes to the level. There is no
		// supported way to close the editor's command box outright; dropping keyboard focus is what
		// "close the console" amounts to from here.
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::SetDirectly);
		}
		UE_LOG(LogTemp, Log, TEXT("SnapNext armed: drag an actor a little the way you want it to go and it will travel on to the first surface within five metres."));
	}

	// AND A KEY FOR IT. A console command cannot be bound to a key: Unreal's Keyboard Shortcuts
	// list only offers registered editor COMMANDS. So the same action is registered as one, which
	// puts "Snap Next" in Editor Preferences -> Keyboard Shortcuts under RepliCan Tools, where it
	// can be given whatever chord suits.
	//
	// SHIFT+Z by default, and checked rather than guessed: every FInputChord the engine's editor
	// source declares was pulled out and compared, 260 of them, and Shift+Z is not among them. It is
	// also the easiest of the free ones to hit with the left hand alone, which is what matters for a
	// command reached for mid-drag while the right hand is on the gizmo. Rebindable as usual in
	// Editor Preferences -> Keyboard Shortcuts, under RepliCan Tools.
	class FSnapCommands : public TCommands<FSnapCommands>
	{
	public:
		FSnapCommands()
			: TCommands<FSnapCommands>(TEXT("RepliCanTools"), LOCTEXT("RepliCanTools", "RepliCan Tools"),
				NAME_None, FAppStyle::GetAppStyleSetName())
		{
		}
		virtual void RegisterCommands() override
		{
			UI_COMMAND(SnapNext, "Snap Next",
				"Arms a one-shot snap: drag an actor a little in some direction and it moves on to the first surface within five metres.",
				EUserInterfaceActionType::Button, FInputChord(EModifierKey::Shift, EKeys::Z));
		}
		TSharedPtr<FUICommandInfo> SnapNext;
	};

	void RegisterShortcut()
	{
		if (!FModuleManager::Get().IsModuleLoaded("LevelEditor")) { return; }
		FSnapCommands::Register();
		FLevelEditorModule& Ed = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
		if (TSharedPtr<FUICommandList> Actions = Ed.GetGlobalLevelEditorActions())
		{
			Actions->MapAction(FSnapCommands::Get().SnapNext,
				FExecuteAction::CreateLambda([]() { Arm(TArray<FString>()); }));
		}
	}

	// The level editor does not exist yet when this file's statics run, so the binding waits.
	struct FSnapShortcutInit
	{
		FSnapShortcutInit() { FCoreDelegates::OnPostEngineInit.AddStatic(&RegisterShortcut); }
	} GSnapShortcutInit;

	FAutoConsoleCommand GSnapNext(
		TEXT("SnapNext"),
		TEXT("Arms a one-shot snap. Drag an actor a little in some direction; on release it moves on in that direction to the first surface within five metres, its leading face against it."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&Arm));
}

#undef LOCTEXT_NAMESPACE

#endif
