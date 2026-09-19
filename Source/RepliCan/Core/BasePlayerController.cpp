#include "Core/BasePlayerController.h"
#include "Core/JsonDataFile.h"
#include "Characters/BaseCharacter.h"
#include "UI/CharacterBuilderWidget.h"
#include "UI/EditToolWidget.h"
#include "UI/CharacterSheetWidget.h"
#include "UI/RemoteViewWidget.h"
#include "UI/TerminalWidget.h"
#include "Components/WidgetComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "ProceduralMeshComponent.h"
#include "StaticMeshResources.h"
#include "UI/AppearanceWidget.h"
#include "Engine/StaticMeshActor.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "World/FlickerLightActor.h"
#include "Engine/PointLight.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/SceneCapture2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Characters/CharacterGhostActor.h"
#include "Characters/CharacterConfig.h"
#include "UI/CrtCursorWidget.h"
#include "UI/MetricsWidget.h"
#include "Components/AudioComponent.h"
#include "Engine/GameViewportClient.h"
#include "Characters/FaceController.h"
#include "GameFramework/SpectatorPawn.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputActionValue.h"
#include "Camera/PlayerCameraManager.h"
#include "Misc/PackageName.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "PhysicsEngine/BodySetup.h"
#include "UI/ContextMenuWidget.h"
#include "UObject/UObjectIterator.h"
#include "Blueprint/UserWidget.h"
#include "CollisionShape.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"
#include "Engine/PostProcessVolume.h"
#include "Components/MeshComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "UI/FaceManagerWidget.h"
#include "UI/AnimationBrowserWidget.h"
#include "UI/PauseMenuWidget.h"
#include "UI/SettingsWidget.h"
#include "UI/ScenesWidget.h"
#include "UI/ConfirmDialogWidget.h"
#include "UI/InspectMenuWidget.h"
#include "UI/CalloutWidget.h"
#include "UI/ConversationWidget.h"
#include "Narrative/VoiceLines.h"
#include "Weapons/BrassFx.h"
#include "Narrative/SequenceData.h"
#include "Narrative/SequenceDirector.h"
#include "Weapons/WeaponCatalog.h"
#include "Weapons/WeaponSkins.h"
#include "Weapons/ShotReactions.h"
#include "Characters/Alertness.h"
#include "UI/SaveLoadWidget.h"
#include "UI/HandTuneWidget.h"
#include "UI/PaneShape.h"
#include "Weapons/HandTuneController.h"
#include "Weapons/SparkFx.h"
#include "Components/PointLightComponent.h"
#include "Components/DecalComponent.h"
#include "World/ElevatorActor.h"
#include "Weapons/ImpactEffects.h"
#include "Core/InputBindings.h"
#include "World/AmbientPlayer.h"
#include "UI/BlinkOverlayWidget.h"
#include "GameFramework/HUD.h"
#include "HAL/IConsoleManager.h"
#include "Blueprint/GameViewportSubsystem.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundWave.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Core/SaveGameSubsystem.h"
#include "Engine/GameInstance.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/IInputProcessor.h"
#include "Layout/WidgetPath.h"
#include "Widgets/SViewport.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/ScrollBox.h"
#include "Blueprint/WidgetTree.h"
#include "UI/CrtStyle.h"
#include "Items/ItemCatalog.h"
#include "UI/SheetSpec.h"
#if WITH_EDITOR
#include "Editor.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#endif
#include "Engine/Texture.h"
#include "UI/ReferenceWidget.h"
#include "Components/LightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/PlayerInput.h"
#include "World/SlidingDoorActor.h"
#include "World/LootBoxActor.h"
#include "UI/InventoryTransferWidget.h"
#include "World/InspectSurface.h"

static void LockCaptureExposure(USceneCaptureComponent2D* Cap);   // defined with the preview captures below; the hand-tuning captures use it too

static TAutoConsoleVariable<int32> CVarVoices(
	TEXT("RepliCan.Voices"), 0,
	TEXT("1 = play baked voice lines; 0 = text only (the default for now)."));
static TAutoConsoleVariable<int32> CVarInspectMenu(TEXT("RepliCan.InspectMenu"), -1, TEXT("Inspect menu placement: 0 floated up-right with a leader to the hit point, 1 beside, 2 top-centre, 3 top-right + leader, 4 above the reticle; -1 = F6's pick"), ECVF_Default);
static TAutoConsoleVariable<int32> CVarInspect(
	TEXT("RepliCan.Inspect"), 0,
	TEXT("1 = the hover highlight and inspect menu; 0 = off (the default for now)."));


namespace
{
	// Why this exists: in edit mode the viewport's capture mode is
	// CaptureDuringRightMouseDown (RMB looks, LMB is a free cursor), and
	// FSceneViewport::OnMouseButtonDown only forwards a button to the game
	// (InputKey -> click events / input actions) when it's the capturing
	// button or the viewport already has capture. So a left click on a
	// character never reached the controller by ANY of the usual routes --
	// input action, actor OnClicked, or key polling. Catch it in Slate
	// before routing instead, and only when the press lands on the game
	// viewport itself (not on one of our panels).
	class FEditClickProcessor : public IInputProcessor
	{
	public:
		explicit FEditClickProcessor(ABasePlayerController* InController) : Controller(InController) {}

		virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}

		// Esc belongs to the game (the in-game menu), never to the editor's
		// "Stop PIE" chord: consuming it here, ahead of Slate routing, is the
		// only place that reliably beats the editor's command binding.
		// Free look is HELD, so it needs the release too. Returning false on both means the key
		// still reaches everything else -- Alt is also the modifier for Alt+T, and swallowing it
		// here would break that.
		virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& KeyEvent) override
		{
			{ const int32 T = TerminalKey(KeyEvent, false); if (T == 1) { return true; } if (T == 2) { return false; } }
			ABasePlayerController* PC = Controller.Get();
			if (!PC || !PC->GetWorld() || !PC->GetWorld()->IsGameWorld()) { return false; }
			if (InputBindings::KeyFor(TEXT("Freelook")) == KeyEvent.GetKey())
			{
				if (ABaseCharacter* Ch = Cast<ABaseCharacter>(PC->GetPawn())) { Ch->SetFreelook(false); }
			}
			if (InputBindings::KeyFor(TEXT("Scan")) == KeyEvent.GetKey()) { PC->SetScanHeld(false); }
			return false;
		}

		// THE TERMINAL owns the keyboard and the mouse while it is up: every key and click is carried
		// to the screen in the world through the interaction pointer; Esc alone is the way out.
		// The pointer INJECTS what it is given back into Slate, which comes through this processor
		// again: while forwarding, the injected copy is let through untouched, or the two chase
		// each other off the end of the stack.
		bool bTerminalForwarding = false;
		struct FForwardScope { bool& Flag; FForwardScope(bool& F) : Flag(F) { Flag = true; } ~FForwardScope() { Flag = false; } };
		// 0: not the terminal's. 1: taken by the terminal. 2: the terminal's, and Slate carries it to
		// the prompt itself (the real keyboard is focused there), so the game must not act on it.
		int32 TerminalKey(const FKeyEvent& KeyEvent, bool bDown)
		{
			ABasePlayerController* PC = Controller.Get();
			if (!PC || !PC->IsTerminalOpen() || bTerminalForwarding) { return 0; }
			if (bDown && KeyEvent.GetKey() == EKeys::Escape) { PC->CloseTerminal(); return 1; }
			if (PC->TerminalKeysDirect()) { return 2; }
			FForwardScope Scope(bTerminalForwarding);
			if (UWidgetInteractionComponent* Pointer = PC->GetTerminalPointer())
			{
				// The processor sees keys, not characters. A printable key goes as its character (PressKey
				// would type one of its own as well: two of every letter); the rest go as keys.
				uint32 Ch = bDown ? KeyEvent.GetCharacter() : 0;
				if (Ch >= 'A' && Ch <= 'Z' && !KeyEvent.IsShiftDown()) { Ch += 'a' - 'A'; }   // key codes are upper case; the shift state says
				if (bDown && Ch >= 32 && Ch < 127 && !KeyEvent.IsControlDown() && !KeyEvent.IsAltDown()) { Pointer->SendKeyChar(FString::Chr((TCHAR)Ch), KeyEvent.IsRepeat()); }
				else if (bDown) { Pointer->PressKey(KeyEvent.GetKey(), KeyEvent.IsRepeat()); }
				else { Pointer->ReleaseKey(KeyEvent.GetKey()); }
			}
			return 1;
		}
		virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& KeyEvent) override
		{
			{ const int32 T = TerminalKey(KeyEvent, true); if (T == 1) { return true; } if (T == 2) { return false; } }
			ABasePlayerController* PC = Controller.Get();
			if (!PC || KeyEvent.IsRepeat() || !PC->GetWorld() || !PC->GetWorld()->IsGameWorld()) { return false; }
			const FKey Key = KeyEvent.GetKey();
			if (InputBindings::KeyFor(TEXT("Freelook")) == Key && !PC->IsAnyScreenOpen() && !PC->IsEditMode())
			{
				// Not down the sights: the two want opposite things from the camera, and the sights won.
				if (ABaseCharacter* Ch = Cast<ABaseCharacter>(PC->GetPawn())) { if (!Ch->IsAiming()) { Ch->SetFreelook(true); } }
				return false;   // Alt is still a modifier for other commands
			}
			if (Key == EKeys::F11) { PC->ToggleMetrics(); return true; }
			if (Key == EKeys::F6) { PC->CycleInspectMenuMode(); return true; }
			// C steps the camera zoom table; Ctrl+C swaps the camera to the other shoulder.
			if (Key == EKeys::C && !PC->IsInCinematic() && !PC->IsInConversation() && !PC->IsPauseMenuOpen() && !PC->IsEditMode())
			{
				if (ABaseCharacter* Ch = Cast<ABaseCharacter>(PC->GetPawn()))
				{
					if (InputBindings::Matches(TEXT("ShoulderSide"), Key, KeyEvent.IsShiftDown(), KeyEvent.IsControlDown(), KeyEvent.IsAltDown()))
					{
						Ch->ToggleShoulderSide(); PC->SetDiagNote(Ch->IsShoulderLeft() ? TEXT("Camera: left shoulder") : TEXT("Camera: right shoulder"));
					}
					else { Ch->StepZoomLevel(); PC->SetDiagNote(FString::Printf(TEXT("Zoom %d / %d"), Ch->GetZoomLevelIndex() + 1, Ch->GetNumZoomLevels())); }
				}
				return true;
			}
			const bool bShift = KeyEvent.IsShiftDown(), bCtrl = KeyEvent.IsControlDown(), bAlt = KeyEvent.IsAltDown();
			auto Bound = [&](const TCHAR* Id) { return InputBindings::Matches(FName(Id), Key, bShift, bCtrl, bAlt); };
			// A covering page blocks the play keys; the reticle menu beside the aim does not, now that
			// most of the station answers the reticle and the menu is up whenever something is looked at.
			const bool bPlaying = !PC->IsPageOpen();

			if (Bound(TEXT("CyclePalette")) && bPlaying) { PC->CycleMaterialUnderReticle(); return true; }
			if (Bound(TEXT("InspectSurface")) && bPlaying) { PC->NoteTextureUnderReticle(); return true; }
			if (Key == EKeys::One || Key == EKeys::Two || Key == EKeys::G) { UE_LOG(LogTemp, Log, TEXT("Keys: %s playing=%d page=%d menu=%d rows=%d flavour=%d"), *Key.ToString(), bPlaying ? 1 : 0, PC->IsPageOpen() ? 1 : 0, PC->IsInspectMenuOpen() ? 1 : 0, PC->InspectActionCount(), PC->IsInspectMenuFlavour() ? 1 : 0); }
			if (Bound(TEXT("NextWeapon")) && bPlaying) { PC->SwapWeaponSlot(); return true; }
			// With the reticle menu up, a digit does the row with that number; the weapon keys wait
			// for the menu to close (or a digit past the menu's rows falls through to them).
			if (PC->IsInspectMenuOpen() && !PC->IsInspectMenuFlavour() && !bShift && !bCtrl && !bAlt)
			{
				static const FKey MenuDigits[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
				for (int32 d = 0; d < 9; ++d) { if (Key == MenuDigits[d] && d < PC->InspectActionCount()) { PC->InspectSelectIndex(d); PC->InspectUseSelected(); return true; } }   // the digit picks AND uses the row: one press, like a conversation reply
			}
			if (Bound(TEXT("Weapon1")) && bPlaying) { PC->EquipWeaponSlot(1); return true; }
			if (Bound(TEXT("Weapon2")) && bPlaying) { PC->EquipWeaponSlot(2); return true; }
			if (Bound(TEXT("InspectPrev")) && PC->IsInspectMenuOpen()) { PC->InspectSelectNext(-1); return true; }
			if (Bound(TEXT("InspectNext")) && PC->IsInspectMenuOpen()) { PC->InspectSelectNext(1); return true; }
			if (Bound(TEXT("Holster")) && bPlaying) { PC->ToggleHolster(); return true; }
			if (Bound(TEXT("Reload")) && bPlaying) { PC->ReloadHeldWeapon(); return true; }
			if (Bound(TEXT("MeleeBash")) && bPlaying) { PC->MeleeHeldWeapon(); return true; }
			if (Bound(TEXT("Scan")) && bPlaying) { PC->SetScanHeld(true); return true; }
			if (Bound(TEXT("Headlamp")) && !PC->IsPauseMenuOpen() && !PC->IsInCinematic())
			{
				if (ABaseCharacter* Ch = Cast<ABaseCharacter>(PC->GetPawn())) { Ch->ToggleHeadlamp(); PC->SetDiagNote(Ch->IsHeadlampOn() ? TEXT("Headlamp on") : TEXT("Headlamp off")); }
				return true;
			}
			const bool bMenu = Bound(TEXT("PauseMenu")), bSheet = Bound(TEXT("CharacterSheet"));
			const bool bAssist = Bound(TEXT("ClaudeAssist")), bEdit = Bound(TEXT("EditMode"));
			if (!bMenu && !bSheet && !bAssist && !bEdit) { return false; }
			// Tab or Esc leaves a conversation (or skips a sequence) before anything else sees them.
			if (!bAssist && !bEdit && (PC->IsInCinematic() || PC->IsInConversation()))
			{
				// Skippable: Tab or Esc breaks out. Unskippable: Esc still pauses
				// (the menu sits over the scene, which resumes on Back); Tab is swallowed.
				const bool bCanBreak = PC->IsInCinematic() ? PC->CanSkipSequence() : PC->CanLeaveConversation();
				if (bCanBreak) { if (PC->IsInCinematic()) { PC->SkipSequence(); } else { PC->EndConversation(); } }
				else if (bMenu) { if (PC->IsReferenceOpen()) { PC->HideReference(); } else if (PC->IsPauseMenuOpen()) { PC->HidePauseMenu(); } else { PC->ShowPauseMenu(); } }
				return true;
			}
			if (bAssist)
			{
				// Claude Assist is a one-shot capture of whatever is under the reticle: no arming,
				// no edit mode, no UI, no mouse click, and it works during normal play. The old
				// armed-cursor mode (BeginClaudeAssist/CancelClaudeAssist) is left intact because the
				// Edit Tool button and the amber marking cursor still drive it -- F12 just no longer
				// binds to it.
				if (PC->IsInCinematic() || PC->IsPauseMenuOpen()) { return true; }
				PC->CaptureAssistUnderReticle();
				return true;
			}
			if (bEdit)
			{
				// Edit mode (scene tuning) on its own, without the assist reticle.
				if (PC->IsInCinematic() || PC->IsPauseMenuOpen()) { return true; }
				PC->HideCharacterSheet();
				PC->ToggleEditMode();
				return true;
			}
			if (bSheet)
			{
				if (!PC->IsPauseMenuOpen()) { PC->ToggleCharacterSheet(); }
				return true;
			}
			// Esc: the sheet closes first; otherwise the pause menu toggles.
			if (PC->IsSaveLoadOpen()) { PC->HideSaveLoad(); return true; }
			if (PC->IsTransferOpen()) { PC->CloseTransfer(); return true; }
			if (PC->IsCharacterSheetOpen()) { PC->HideCharacterSheet(); return true; }
			if (PC->IsReferenceOpen()) { PC->HideReference(); } else if (PC->IsPauseMenuOpen()) { PC->HidePauseMenu(); } else { PC->ShowPauseMenu(); }
			return true;
		}

		// The wheel changes weapon rather than camera distance. Consuming it here keeps it away
		// from the Zoom input action, so the two do not both fire off one scroll; zoom stays on C.
		virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& WheelEvent, const FPointerEvent* GestureEvent) override
		{
			if (ABasePlayerController* TPC = Controller.Get()) { if (TPC->IsTerminalOpen() && !bTerminalForwarding) { FForwardScope Scope(bTerminalForwarding); if (UWidgetInteractionComponent* Pointer = TPC->GetTerminalPointer()) { Pointer->ScrollWheel(WheelEvent.GetWheelDelta()); } return true; } }
			ABasePlayerController* PC = Controller.Get();
			if (!PC || !PC->GetWorld() || !PC->GetWorld()->IsGameWorld()) { return false; }
			if (PC->IsEditMode() || PC->IsPageOpen() || (PC->IsInspectMenuOpen() && PC->InspectActionCount() > 1)) { return false; }   // a reticle menu with a choice to make takes the wheel (the axis binding below); otherwise it changes weapon
			const float Delta = WheelEvent.GetWheelDelta();
			if (FMath::IsNearlyZero(Delta)) { return false; }
			UE_LOG(LogTemp, Log, TEXT("Wheel: %.1f -> swap"), Delta);
			PC->SwapWeaponSlot(Delta > 0.0f ? 1 : -1);
			return true;
		}

		// Aiming is the one command that is HELD rather than pressed, so it needs the release
		// too. Letting go anywhere -- including over a panel that just opened -- lowers the
		// weapon, because the alternative is a player stuck in ADS with no way out.
		virtual bool HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
		{
			ABasePlayerController* PC = Controller.Get();
			if (!PC) { return false; }
			if (PC->IsTerminalOpen() && !bTerminalForwarding) { FForwardScope Scope(bTerminalForwarding); if (UWidgetInteractionComponent* Pointer = PC->GetTerminalPointer()) { Pointer->ReleasePointerKey(MouseEvent.GetEffectingButton()); } return true; }
			if (InputBindings::KeyFor(TEXT("Fire")) == MouseEvent.GetEffectingButton()) { PC->SetTriggerHeld(false); }
			if (InputBindings::KeyFor(TEXT("Aim")) == MouseEvent.GetEffectingButton())
			{
				if (ABaseCharacter* Ch = Cast<ABaseCharacter>(PC->GetPawn())) { Ch->SetAiming(false); }
			}
			return false;
		}

		virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
		{
			ABasePlayerController* PC = Controller.Get();
			if (!PC) { return false; }
			if (PC->IsTerminalOpen() && !bTerminalForwarding) { FForwardScope Scope(bTerminalForwarding); if (UWidgetInteractionComponent* Pointer = PC->GetTerminalPointer()) { Pointer->PressPointerKey(MouseEvent.GetEffectingButton()); } return true; }
			const FKey Button = MouseEvent.GetEffectingButton();
			// Only a real page blocks these (the reticle menu is not one): aiming is a combat intent, and it closes that menu.
			const bool bCanAct = !PC->IsEditMode() && !PC->IsPageOpen();

			if (bCanAct && InputBindings::KeyFor(TEXT("Aim")) == Button)
			{
				if (ABaseCharacter* Ch = Cast<ABaseCharacter>(PC->GetPawn()))
				{
					if (PC->HeldSlot >= 0) { PC->DismissInspectMenu(); Ch->SetAiming(true); return true; }
				}
				return false;
			}
			if (bCanAct && InputBindings::KeyFor(TEXT("FireMode")) == Button && PC->HeldSlot >= 0) { PC->CycleFireMode(); return true; }
			if (InputBindings::KeyFor(TEXT("Fire")) != Button) { return false; }
			// Outside edit mode a click is a trigger pull, provided something is in the hand and
			// no screen is up. Edit mode keeps the click for placing things.
			if (!PC->IsEditMode())
			{
				// A click that lands on a panel is a click on the panel, never a shot. The reticle menu
				// is not a panel: with a person or a crate under the reticle the trigger was dead, which
				// is exactly when it is wanted. The pull closes the menu, the way aiming does.
				if (PC->IsFiringBlocked()) { return false; }
				if (PC->HeldSlot < 0) { return false; }
				PC->DismissInspectMenu();
				PC->FireHeldWeapon();
				PC->SetTriggerHeld(true);   // and stays pulled until the release: auto cycles on it
				return true;
			}

			const FWidgetPath Path = SlateApp.LocateWindowUnderMouse(MouseEvent.GetScreenSpacePosition(), SlateApp.GetInteractiveTopLevelWindows(), false, MouseEvent.GetUserIndex());
			if (!Path.IsValid()) { return false; }
			// The deepest hit-tested widget is the SViewport when the cursor is
			// over open world; anything else means a panel/button took it.
			const TSharedRef<SWidget> Hit = Path.GetLastWidget();
			if (Hit->GetType() != TEXT("SViewport")) { return false; }

			PC->NotifyEditClick();
			return false;   // let the viewport still focus/capture as usual
		}

	private:
		TWeakObjectPtr<ABasePlayerController> Controller;
	};
}

ABasePlayerController::ABasePlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ABasePlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Ambient) { Ambient->Stop(); }
	if (ClickProcessor.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(ClickProcessor);
	}
	ClickProcessor.Reset();
	Super::EndPlay(EndPlayReason);
}

namespace
{
	// The same Enhanced Input assets the characters use. Bound here as well
	// because Enhanced Input CONSUMES the keys it maps: a plain BindKey(Tab)
	// on the controller never fires while IMC_Dungeon maps Tab, and the
	// spectator pawn's legacy mouse-look axes are eaten the same way.
	constexpr const TCHAR* MappingContextPath = TEXT("/Game/Input/IMC_Dungeon.IMC_Dungeon");
	constexpr const TCHAR* ToggleEditActionPath = TEXT("/Game/Input/IA_ToggleCharacterBuilder.IA_ToggleCharacterBuilder");
	constexpr const TCHAR* ClickActionPath = TEXT("/Game/Input/IA_PlaySelectedAttack.IA_PlaySelectedAttack");
	constexpr const TCHAR* MoveForwardActionPath = TEXT("/Game/Input/IA_MoveForward.IA_MoveForward");
	constexpr const TCHAR* MoveRightActionPath = TEXT("/Game/Input/IA_MoveRight.IA_MoveRight");
	constexpr const TCHAR* LookYawActionPath = TEXT("/Game/Input/IA_LookYaw.IA_LookYaw");
	constexpr const TCHAR* LookPitchActionPath = TEXT("/Game/Input/IA_LookPitch.IA_LookPitch");
	constexpr const TCHAR* FlyUpActionPath = TEXT("/Game/Input/IA_Jump.IA_Jump");
	constexpr const TCHAR* FlyDownActionPath = TEXT("/Game/Input/IA_Crouch.IA_Crouch");
	constexpr float FlyLookScale = 1.0f;
}

void ABasePlayerController::BeginPlay()
{
	Super::BeginPlay();
	EnsureSoftwareCursor();
	if (IsLocalController()) { ToggleMetrics(); }   // on by default while the game is being built

	if (IsLocalController() && FSlateApplication::IsInitialized())
	{
		ClickProcessor = MakeShared<FEditClickProcessor>(this);
		FSlateApplication::Get().RegisterInputPreProcessor(ClickProcessor);
	}
	if (IsLocalController()) { EnsureInspectOutlineVolume(); }
	if (IsLocalController()) { GiveStartingWeapons(); }
	if (IsLocalController() && !AmbientProfile.IsEmpty())
	{
		if (!Ambient) { Ambient = NewObject<UAmbientPlayer>(this); }
		Ambient->Start(GetWorld(), AmbientProfile);
	}

	// Arrived here from the title screen's Load Game: apply the slot once
	// every actor in the level has begun play.
	if (USaveGameSubsystem* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<USaveGameSubsystem>() : nullptr)
	{
		FString Slot;
		if (Saves->ConsumePendingLoad(Slot))
		{
			GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, Slot]()
			{
				if (USaveGameSubsystem* S = GetGameInstance() ? GetGameInstance()->GetSubsystem<USaveGameSubsystem>() : nullptr) { S->LoadGame(Slot); } bInspectEnabled = true;
			}));
		}
		// Fresh game from the title -- or plain PIE on a level with a start
		// sequence -- plays the waking-up intro once everyone has begun play.
		else
		{
			Saves->ConsumePendingNewGame();
			bFreshPlayerName = true;
			static const auto* SkipIntro = IConsoleManager::Get().FindConsoleVariable(TEXT("RepliCan.SkipNewGameIntro"));
			if (SkipIntro && SkipIntro->GetInt() != 0) { bInspectEnabled = true; }
			if (!StartSequence.IsEmpty() && (!SkipIntro || SkipIntro->GetInt() == 0) && SequenceFile::Exists(StartSequence))
			{
				// Black from the very first frame; the sequence's eyelids take
				// over before this is released (its "blink" step).
				if (PlayerCameraManager) { PlayerCameraManager->SetManualCameraFade(1.0f, FLinearColor::Black, false); }
				GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]() { PlaySequence(StartSequence); }));
			}
		}
	}

	PlaceAtEditorCamera();

	// Make sure the mapping context is up even before any character adds
	// it (a level with no auto-possessed character starts in fly mode).
	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			if (UInputMappingContext* Context = LoadObject<UInputMappingContext>(nullptr, MappingContextPath))
			{
				Subsystem->AddMappingContext(Context, 0);
			}
		}
	}
	ApplyInputMode();
}

void ABasePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EIC) { return; }

	auto Bind = [EIC, this](const TCHAR* Path, ETriggerEvent Event, auto Handler)
	{
		if (UInputAction* Action = LoadObject<UInputAction>(nullptr, Path)) { EIC->BindAction(Action, Event, this, Handler); }
	};
	// (IA_ToggleCharacterBuilder / Tab is handled by FEditClickProcessor above: it opens the character sheet.)
	Bind(ClickActionPath, ETriggerEvent::Started, &ABasePlayerController::OnClickInput);
	// Fly-mode movement/look, driven from the same actions the characters
	// use; the handlers do nothing unless flying (a possessed character has
	// its own bindings for these).
	Bind(MoveForwardActionPath, ETriggerEvent::Triggered, &ABasePlayerController::OnFlyForward);
	Bind(MoveRightActionPath, ETriggerEvent::Triggered, &ABasePlayerController::OnFlyRight);
	Bind(LookYawActionPath, ETriggerEvent::Triggered, &ABasePlayerController::OnFlyYaw);
	Bind(LookPitchActionPath, ETriggerEvent::Triggered, &ABasePlayerController::OnFlyPitch);
	Bind(FlyUpActionPath, ETriggerEvent::Triggered, &ABasePlayerController::OnFlyUp);
	Bind(FlyDownActionPath, ETriggerEvent::Triggered, &ABasePlayerController::OnFlyDown);

	// Esc: the in-game menu. A plain key binding (Enhanced Input runs beside
	// it) that keeps firing while paused, so Esc also closes the menu.
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ABasePlayerController::OnMenuKey).bExecuteWhenPaused = true;

	// Inspect context menu: wheel moves the selection, E uses it. Neither
	// consumes, so the wheel still zooms when no menu is up (the character's
	// zoom handler checks IsInspectMenuOpen).
	InputComponent->BindAxisKey(EKeys::MouseWheelAxis, this, &ABasePlayerController::OnInspectWheel).bConsumeInput = false;   // Up / Down do the same (see the input processor)
	InputComponent->BindKey(EKeys::E, IE_Pressed, this, &ABasePlayerController::OnInspectUse).bConsumeInput = false;
	InputComponent->BindKey(EKeys::E, IE_Released, this, &ABasePlayerController::OnInspectUseReleased).bConsumeInput = false;

	// Conversation replies 1-9 (Tab/Esc are caught in FEditClickProcessor).
	for (const FKey& Digit : { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine })
	{
		InputComponent->BindKey(Digit, IE_Pressed, this, &ABasePlayerController::OnConversationDigit).bConsumeInput = false;
	}

	// Quick save / load.
	InputComponent->BindKey(EKeys::F5, IE_Pressed, this, &ABasePlayerController::QuickSave);
	InputComponent->BindKey(EKeys::F9, IE_Pressed, this, &ABasePlayerController::QuickLoad);
}

void ABasePlayerController::SaveToSlot(const FString& Slot)
{
	USaveGameSubsystem* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<USaveGameSubsystem>() : nullptr;
	const bool bOk = Saves && Saves->SaveGame(Slot);
	if (APawn* P = GetPawn()) { ShowCallout(P, bOk ? TEXT("Game saved.") : TEXT("Save failed (see log)."), 2.0f, false); }
}

void ABasePlayerController::LoadFromSlot(const FString& Slot)
{
	HideSaveLoad();
	USaveGameSubsystem* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<USaveGameSubsystem>() : nullptr;
	if (!Saves || !Saves->SlotExists(Slot))
	{
		if (APawn* P = GetPawn()) { ShowCallout(P, *FString::Printf(TEXT("Nothing in %s yet."), *Slot), 2.0f, false); }
		return;
	}
	HidePauseMenu();
	const bool bOk = Saves->LoadGame(Slot);
	if (APawn* P = GetPawn()) { ShowCallout(P, bOk ? TEXT("Game loaded.") : TEXT("Load failed (see log)."), 2.0f, false); }
}

void ABasePlayerController::QuickSave() { SaveToSlot(TEXT("Quick")); }
void ABasePlayerController::QuickLoad() { LoadFromSlot(TEXT("Quick")); }

static const float HandTuneFrameCm = 185.0f;   // the width of each tuning picture in the world: the upper body and a rifle, with room round them
static const FVector HandTuneBoothOrigin(60000.0f, -63000.0f, 0.0f);   // the stand-in's booth: off the map, clear of the weapon booth and the character booth
static const float HandTuneBoothYaw = 0.0f;                             // the stand-in faces +X; the side picture looks along -Y, the top picture down

void ABasePlayerController::UIDump()
{
	int32 InViewport = 0;
	FString Note;
	for (TObjectIterator<UUserWidget> It; It; ++It)
	{
		UUserWidget* W = *It;
		if (!W || W->GetWorld() != GetWorld()) { continue; }
		const bool bIn = W->IsInViewport();
		UE_LOG(LogTemp, Log, TEXT("UIDump: %-32s inViewport=%d visibility=%d opacity=%.2f"), *W->GetClass()->GetName(), bIn ? 1 : 0, (int32)W->GetVisibility(), W->GetRenderOpacity());
		if (bIn) { ++InViewport; Note += (Note.IsEmpty() ? TEXT("") : TEXT(", ")) + W->GetClass()->GetName(); }
	}
	UE_LOG(LogTemp, Log, TEXT("UIDump: %d widget(s) in the viewport | %s"), InViewport, *DescribeInput());
	SetDiagNoteTimed(FString::Printf(TEXT("UI: %s"), Note.IsEmpty() ? TEXT("nothing in the viewport") : *Note), 8.0f);
}

void ABasePlayerController::HandTuneLoadWeapon(const WeaponCatalog::FWeapon* W)
{
	if (!W) { return; }
	HandTuneKey = W->Key;   // the Weapons.json entry SAVE writes
	HandTuneName = W->Name;
	HT_Skin.Empty();   // a weapon arrives wearing its own default, not the last one's fitting
	HT_Grip0 = HT_Grip = W->Grip; HT_HandRot0 = HT_HandRot = W->HandRot; HT_Fore0 = HT_Fore = W->ForeGrip; HT_ForeRot0 = HT_ForeRot = W->ForeHandRot;
	HT_HasFore = W->bHasForeGrip; HT_ForePitch = W->ForeGripPitch; HT_Stance = W->Stance;
	auto Five = [](const TArray<float>& In) { TArray<float> Out = In; Out.SetNumZeroed(5); return Out; };
	HT_FingersR0 = HT_FingersR = Five(W->FingersR); HT_FingersL0 = HT_FingersL = Five(W->FingersL);
	HT_Hunch0 = HT_Hunch = W->Hunch; HT_Lean0 = HT_Lean = W->LeanDeg;
	for (int32 i = 0; i < 3; ++i) { HT_Pull3_0[i] = HT_Pull3[i] = W->PullCm[i]; HT_Lat3_0[i] = HT_Lat3[i] = W->LateralCm[i]; HT_Pos3_0[i] = HT_Pos3[i] = W->PositionCm[i]; HT_Grip3_0[i] = HT_Grip3[i] = W->GripCm[i]; HT_Fore3_0[i] = HT_Fore3[i] = W->ForeCm[i]; }
	HT_LowReady0[0] = HT_LowReady[0] = W->LowReadyPitch; HT_LowReady0[1] = HT_LowReady[1] = W->LowReadyYaw;
	for (int32 i = 0; i < 3; ++i) { HT_ElbowMain0[i] = HT_ElbowMain[i] = W->ElbowMain[i]; HT_ElbowSup0[i] = HT_ElbowSup[i] = W->ElbowSupport[i]; }
	HT_OpticKey = W->Optic;
	HT_Optic = W->Optic;
	HT_ScalePct0 = HT_ScalePct = W->Scale * 100.0f;
	{
		HT_Optic0 = HT_Optic = W->Optic;
		const WeaponCatalog::FOptic* O = WeaponCatalog::FindOptic(HT_OpticKey);
		HT_OpticOff0 = HT_OpticOff = O ? O->Offset : FVector::ZeroVector;
		HT_OpticSkin0 = HT_OpticSkin = O ? O->Skin : FString();
	}
	for (int32 i = 0; i < 3; ++i) { HT_ElbowMainAim0[i] = HT_ElbowMainAim[i] = W->ElbowMainAim[i]; HT_ElbowSupAim0[i] = HT_ElbowSupAim[i] = W->ElbowSupportAim[i]; }
	// The stand-in, if it is already standing there: opening the page loads the numbers before the
	// booth exists, and the arrows load them when it is already in front of you.
	if (IsValid(HandTunePawn)) { ApplyWeaponToPawn(HandTunePawn, W); HandTunePawn->SetAiming(false); }
}

FString ABasePlayerController::HandTuneWeaponLabel() const
{
	// THE MODEL, NOT "MAKE MODEL". DisplayName prepends the maker, which on this page is both
	// redundant -- every weapon in a pack shares one -- and long enough to overrun the selector's
	// label and collide with its arrows.
	if (const WeaponCatalog::FWeapon* W = WeaponCatalog::Find(HandTuneName))
	{
		if (!W->Model.IsEmpty()) { return W->Model; }
	}
	return HandTuneName;
}

int32 ABasePlayerController::RefreshTunedWeapon(const FString& WeaponName)
{
	// Nothing in the bag needs touching: Inventory and Equipped are lists of NAMES, so every weapon
	// of a type shares the catalogue's one set of numbers and cannot drift from it. What can drift
	// is the copy pushed onto a character when the weapon was put in its hands -- the player's, an
	// NPC's, and the tuning page's own stand-in. Those are the ones re-dressed here.
	if (WeaponName.IsEmpty() || !GetWorld()) { return 0; }
	const WeaponCatalog::FWeapon* W = WeaponCatalog::Find(WeaponName);
	if (!W) { return 0; }
	int32 Dressed = 0;
	for (TActorIterator<ABaseCharacter> It(GetWorld()); It; ++It)
	{
		ABaseCharacter* Who = *It;
		if (!IsValid(Who) || Who->GetHeldWeaponName() != WeaponName) { continue; }
		// NOT THE TUNING PAGE'S STAND-IN. That figure is driven by the page's live values, not by
		// the catalogue, and re-dressing it from the file rebuilds its weapon and stance from
		// scratch -- which is why saving appeared to throw the edit away and RESET appeared to bring
		// it back. (RESET restores the page's values, which after a save ARE the saved ones, so it
		// looked like the fix when it was really the second half of the bug.)
		if (Who == HandTunePawn) { continue; }
		ApplyWeaponToPawn(Who, W);
		++Dressed;
	}
	return Dressed;
}

void ABasePlayerController::HandTuneStepWeapon(int32 Dir)
{
	if (!Screens.IsOpen(EScreen::HandTune)) { return; }
	const TArray<FString> Names = WeaponCatalog::TunableNames();
	if (Names.Num() == 0) { return; }
	int32 At = Names.IndexOfByKey(HandTuneName);
	if (At == INDEX_NONE) { At = 0; }
	// Wraps, so the far end of the list is one press from the near end rather than a dead button.
	At = (At + (Dir >= 0 ? 1 : Names.Num() - 1)) % Names.Num();
	if (const WeaponCatalog::FWeapon* W = WeaponCatalog::Find(Names[At]))
	{
		HandTuneLoadWeapon(W);
		HandTuneResetView();   // a different weapon is a different shape: start from the standing view
	}
}

void ABasePlayerController::ShowHandTune(const FString& WeaponName)
{
	ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn());
	const WeaponCatalog::FWeapon* W = WeaponCatalog::Find(WeaponName);
	if (!Me || !W || !GetWorld()) { SetDiagNoteTimed(FString::Printf(TEXT("HandTune: no pawn, or no weapon named %s"), *WeaponName), 4.0f); return; }
	if (Screens.IsOpen(EScreen::HandTune)) { HideHandTune(); }
	bHandTuneReturnToReference = Screens.IsOpen(EScreen::Reference);
	if (Screens.IsOpen(EScreen::Reference)) { HideReference(); }
	HandTuneLoadWeapon(W);
	HT_EyeSide0 = HT_EyeSide = Me->GetEyeSideCm(); HT_EyeUp0 = HT_EyeUp = Me->GetEyeUpCm(); HT_EyeFwd0 = HT_EyeFwd = Me->GetEyeForwardCm();   // the eyeline is the body's, not the weapon's -- reseeded below from whichever body the booth ends up as
	// THE STAND-IN: a saved likeness (Characters/<name>.json) in the booth the character
	// sheet's mirror uses. Spawned through SpawnBoothCharacter, which spawns DEFERRED and sets the
	// config name before FinishSpawning so BeginPlay builds the body from the file -- a config
	// applied after an ordinary spawn leaves the bare crew rig standing there in its overalls.
	// The player is not touched: the world changes on SAVE and at no other moment.
	// THE DEFAULT BODY IS THE ONE BEING PLAYED -- nose, hair, brow and all. Read off the live pawn
	// rather than assuming "Player", so the page opens on whoever is actually in the world. Stepping
	// the CHARACTER selector moves off it; reopening the page comes back to it.
	if (const ABaseCharacter* LivePlayer = Cast<ABaseCharacter>(GetPawn()))
	{
		const FString LiveName = LivePlayer->GetCharacterConfig().Name;
		if (!LiveName.IsEmpty() && IFileManager::Get().FileExists(*CharacterConfigFile::GetPath(LiveName))) { HandTuneCharacter = LiveName; }
	}
	if (!HandTuneSpawnBooth(W))
	{
		SetDiagNoteTimed(FString::Printf(TEXT("HandTune: no saved likeness to stand in (Characters/%s.json)"), *HandTuneCharacter), 5.0f);
		if (bHandTuneReturnToReference) { bHandTuneReturnToReference = false; ShowReference(); }
		return;
	}
	ABaseCharacter* Booth = HandTunePawn;
	// The eyeline belongs to the BODY, so it comes from whichever body the booth ended up as rather
	// than from the pawn the page was opened from. They are the same character by default and differ
	// the moment the CHARACTER selector is stepped.
	{
		FCharacterConfig BoothCfg;
		if (CharacterConfigFile::Load(HandTuneCharacter, BoothCfg))
		{
			HT_EyeSide0 = HT_EyeSide = BoothCfg.EyeSideCm; HT_EyeUp0 = HT_EyeUp = BoothCfg.EyeUpCm; HT_EyeFwd0 = HT_EyeFwd = BoothCfg.EyeForwardCm;
		}
		if (Booth) { Booth->SetEyeTune(HT_EyeSide, HT_EyeUp, HT_EyeFwd); }
	}
	HandTuneResetView();
	HT_Zoom = 1.0f; HT_ViewIdx = 1;
	HandTuneSetCarry(1);
	HandTuneSetAim(1);
	// THE EYE LINE: a thin rod from the aiming eye straight down the aim. A component of the
	// stand-in, so the captures show it (their show-only list is taken from the pawn's components,
	// which is also why it is made BEFORE they are).
	HandTuneAimLine = NewObject<UStaticMeshComponent>(Booth, TEXT("HandTuneAimLine"));
	if (HandTuneAimLine)
	{
		HandTuneAimLine->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
		if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
		{
			UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, this);
			M->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.0f, 0.15f, 0.1f, 1.0f));
			HandTuneAimLine->SetMaterial(0, M);
		}
		HandTuneAimLine->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		HandTuneAimLine->SetCastShadow(false);
		HandTuneAimLine->SetAbsolute(true, true, true);
		HandTuneAimLine->SetupAttachment(Booth->GetRootComponent());
		HandTuneAimLine->RegisterComponent();
	}
	// The captures: orthographic, one from the stand-in's right and one from above, each seeing only
	// the stand-in (and what hangs off it), exposure locked like the booth's. TickHandTune aims them
	// and asks each for a frame outright, so nothing depends on a capture ticking under a pause.
	FActorSpawnParameters P; P.Owner = this; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto MakeCapture = [&](TObjectPtr<ASceneCapture2D>& Actor, TObjectPtr<UTextureRenderTarget2D>& RT)
	{
		// MADE TO THE SHAPE OF THE FRAME IT IS SHOWN IN. A square target displayed in a frame that is
		// not square is stretched on one axis, and an orthographic capture takes its vertical extent
		// from the target's aspect -- so a square target was both rendering and displaying the wrong
		// proportions. Derived from the widget's own numbers so the two cannot drift apart.
		if (!RT)
		{
			const int32 RTW = 900;
			RT = UKismetRenderingLibrary::CreateRenderTarget2D(this, RTW, PaneShape::TargetHeight(PaneShape::HandTune, RTW), ETextureRenderTargetFormat::RTF_RGBA8_SRGB, FLinearColor::Black, false);
		}
		Actor = GetWorld()->SpawnActor<ASceneCapture2D>(Booth->GetActorLocation(), FRotator::ZeroRotator, P);
		if (!Actor) { return; }
		Actor->SetTickableWhenPaused(true);
		USceneCaptureComponent2D* Cap = Actor->GetCaptureComponent2D();
		if (!Cap) { return; }
		Cap->SetTickableWhenPaused(true);
		Cap->TextureTarget = RT; Cap->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
		Cap->ProjectionType = ECameraProjectionMode::Orthographic; Cap->OrthoWidth = HandTuneFrameCm;
		Cap->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
		Cap->ShowOnlyActorComponents(Booth, true);
		Cap->bCaptureEveryFrame = false; Cap->bCaptureOnMovement = false; Cap->bAlwaysPersistRenderingState = true;
		LockCaptureExposure(Cap);
	};
	MakeCapture(HandTuneCap, HandTuneRT);
	// A three-light rig around the stand-in: key from the side camera's quarter, fill from the other
	// side, a top light for the top view. The booth's own lights are set for a portrait, not this.
	const FRotator Face(0.0f, HandTuneBoothYaw, 0.0f);
	const FVector LightRel[3] = { FVector(120.0f, 220.0f, 160.0f), FVector(80.0f, -200.0f, 60.0f), FVector(60.0f, 0.0f, 260.0f) };
	const float LightCd[3] = { 220.0f, 90.0f, 140.0f };
	for (int32 i = 0; i < 3; ++i)
	{
		APointLight* Light = GetWorld()->SpawnActor<APointLight>(Booth->GetActorLocation() + Face.RotateVector(LightRel[i]), FRotator::ZeroRotator, P);
		if (!Light) { continue; }
		Light->SetMobility(EComponentMobility::Movable);
		Light->PointLightComponent->SetIntensityUnits(ELightUnits::Candelas);
		Light->PointLightComponent->SetIntensity(LightCd[i]);
		Light->PointLightComponent->SetAttenuationRadius(800.0f);
		Light->PointLightComponent->SetCastShadows(false);
		Light->SetTickableWhenPaused(true);
		HandTuneLights.Add(Light);
	}
	if (!HandTuneWidget)
	{
		HandTuneWidget = CreateWidget<UHandTuneWidget>(this, UHandTuneWidget::StaticClass());
		if (HandTuneWidget) { HandTuneWidget->OnClose.BindUObject(this, &ABasePlayerController::HideHandTune); }
	}
	Screens.Open(EScreen::HandTune);
	TickHandTune();   // the captures on the subject before the first frame is drawn
	if (HandTuneWidget)
	{
		// THROUGH THE SAME PLACER AS THE CHARACTER SHEET. AddToViewport gives a widget the whole
		// screen, which is why this page came up full-bleed while every other console page sits in
		// the same framed rectangle. PlaceConsolePage puts it in ConsolePageRect() and registers it
		// so KeepConsolePagesFitted re-fits it when the window changes size. In the viewport first,
		// either way: focus only lands on a widget that has been built.
		PlaceConsolePage(HandTuneWidget, 70);
		HandTuneWidget->Open(this, HandTuneRT);
	}
	ApplyInputMode();
	if (HandTuneWidget) { HandTuneWidget->SetKeyboardFocus(); }
}

void ABasePlayerController::TickHandTune()
{
	if (!Screens.IsOpen(EScreen::HandTune) || !HandTunePawn || !HandTunePawn->GetMesh()) { return; }
	ABaseCharacter* Booth = HandTunePawn;
	// THE PIVOT IS THE WEAPON. It is the thing being tuned, so it holds the middle of both pictures
	// and a drag swings the camera round it -- the weapon does not slide about the frame while you
	// are trying to look at where a hand meets it. Its own bounds centre, not the hand: a rifle
	// tuned by its grip would sit half out of frame.
	const FRotator Face(0.0f, HandTuneBoothYaw, 0.0f);
	const FVector Right = FRotationMatrix(Face).GetUnitAxis(EAxis::Y);
	const FVector Fwd = Face.Vector();
	// The eye line, from the aiming eye down the aim, a little past the muzzle.
	if (HandTuneAimLine)
	{
		FVector EyeLoc; FRotator EyeRot;
		if (Booth->GetAimEye(EyeLoc, EyeRot))
		{
			const FVector To = EyeLoc + EyeRot.Vector() * 140.0f;
			HandTuneAimLine->SetVisibility(true);
			HandTuneAimLine->SetWorldLocation((EyeLoc + To) * 0.5f);
			HandTuneAimLine->SetWorldRotation(FRotationMatrix::MakeFromZ(To - EyeLoc).Rotator());
			HandTuneAimLine->SetWorldScale3D(FVector(0.006f, 0.006f, 1.4f));   // the engine cylinder is 100 long and 100 across
		}
		else { HandTuneAimLine->SetVisibility(false); }
	}
	FVector Pivot;
	if (Booth->WeaponMeshComponent && Booth->WeaponMeshComponent->GetStaticMesh()) { Pivot = Booth->WeaponMeshComponent->Bounds.Origin; }
	else if (Booth->GetMesh()->DoesSocketExist(TEXT("hand_r"))) { Pivot = Booth->GetMesh()->GetSocketLocation(TEXT("hand_r")); }
	else { Pivot = Booth->GetActorLocation() + FVector(0.0f, 0.0f, 40.0f); }
	// The drag turns the camera's offset about the pivot IN THE PICTURE'S OWN FRAME: a sideways
	// drag swings it about the picture's up axis, a vertical drag tips it about the picture's
	// right axis. Each picture must bring its own two axes -- crossing the offset with the world's
	// up worked for the side view and gave the top view a ZERO axis, because it looks straight
	// down: that is why dragging the top picture did nothing at all.
	// FRAME THE MAN AND THE GUN. The pivot is the weapon, but a frame centred on it alone leaves the
	// body half out of picture. So the camera slides across its own view by half the way to the
	// head: the orbit still turns about the weapon, and what you see is a man holding one. No
	// attempt is made to fit the whole eye line -- it runs a metre and a half past the muzzle.
	const FVector HeadAt = Booth->GetMesh()->DoesSocketExist(TEXT("head")) ? Booth->GetMesh()->GetSocketLocation(TEXT("head")) : Pivot;
	// WHERE IT IS SEEN FROM. Five standing places round the stand-in; the drag turns whichever is
	// chosen about the weapon, and lets go back to it.
	const FVector Up = FVector::UpVector;
	FVector Offset;
	switch (HT_ViewIdx)
	{
	case 0:  Offset = -Right * 260.0f; break;                                        // from its left
	case 2:  Offset = Up * 330.0f; break;                                            // from above
	case 3:  Offset = Fwd * 260.0f; break;                                           // from in front, down the barrel
	case 4:  Offset = (Fwd * 0.62f + Right * 0.66f + Up * 0.42f).GetSafeNormal() * 285.0f; break;   // three-quarter
	default: Offset = Right * 260.0f; break;                                         // from its right
	}
	// The picture's own two axes, so a drag turns it the way the pointer went whichever way it
	// faces. Looking straight down there is no horizon to use, so the body's forward stands in.
	const FVector ViewDir = (-Offset).GetSafeNormal();
	const FVector PicUp = FMath::Abs(FVector::DotProduct(ViewDir, Up)) > 0.9f ? Fwd : Up;
	const FVector PicRight = FVector::CrossProduct(PicUp, ViewDir).GetSafeNormal();
	{
		ASceneCapture2D* Actor = HandTuneCap;
		if (Actor)
		{
			const FVector Turned = Offset.RotateAngleAxis(HT_Orbit.X, PicUp).RotateAngleAxis(HT_Orbit.Y, PicRight);
			const FVector View = (-Turned).GetSafeNormal();
			const FVector ToHead = (HeadAt - Pivot) * 0.5f;
			const FVector Across = ToHead - View * FVector::DotProduct(ToHead, View);   // only what shows in this picture
			Actor->SetActorLocationAndRotation(Pivot + Turned + Across, (-Turned).Rotation());
			if (USceneCaptureComponent2D* Cap = Actor->GetCaptureComponent2D())
			{
				Cap->OrthoWidth = HandTuneFrameCm * HT_Zoom;
				Cap->CaptureScene();   // one frame a tick, asked for outright
			}
		}
	}
}

void ABasePlayerController::HideHandTune()
{
	if (!Screens.IsOpen(EScreen::HandTune)) { return; }
	Screens.Close(EScreen::HandTune);
	if (HandTuneWidget && HandTuneWidget->IsInViewport()) { HandTuneWidget->RemoveFromParent(); }
	if (HandTuneCap) { HandTuneCap->Destroy(); HandTuneCap = nullptr; }
	if (HandTuneAimLine) { HandTuneAimLine->DestroyComponent(); HandTuneAimLine = nullptr; }
	for (TObjectPtr<APointLight>& L : HandTuneLights) { if (L) { L->Destroy(); } }
	HandTuneLights.Reset();
	if (HandTuneController) { HandTuneController->UnPossess(); HandTuneController->Destroy(); HandTuneController = nullptr; }
	HandTunePawn = nullptr;
	DestroyBooth();   // the stand-in and its set
	// The player was never touched: only SAVE changes the world.
	ApplyInputMode();
	if (bHandTuneReturnToReference) { bHandTuneReturnToReference = false; ShowReference(); }
}

void ABasePlayerController::HandTuneSetCarry(int32 Idx)
{
	HandTuneCarryIdx = FMath::Clamp(Idx, 0, 2);
	static const int32 Carries[] = { 0 /*LowReady*/, 2 /*Shouldered*/, 3 /*ADS*/ };
	if (HandTunePawn) { HandTunePawn->SetCarryOverride(Carries[HandTuneCarryIdx]); }
}

void ABasePlayerController::HandTuneSetAim(int32 Idx)
{
	HandTuneAimIdx = FMath::Clamp(Idx, 0, 2);
	static const float Pitches[] = { 28.0f, 0.0f, -42.0f };
	if (HandTuneController) { HandTuneController->SetControlRotation(FRotator(Pitches[HandTuneAimIdx], HandTuneBoothYaw, 0.0f)); }
}

void ABasePlayerController::HandTuneApply()
{
	ABaseCharacter* Me = HandTunePawn;   // the stand-in, never the player
	if (!Me) { return; }
	{
		// PER CARRY, with the stance nudge on each: the character blends between them, so the hands
		// travel with the gun on a carry change instead of snapping at the end of it.
		FVector G[3], F[3];
		const FVector Nudge = WeaponCatalog::StanceGripNudge(HT_Stance);
		for (int32 i = 0; i < 3; ++i) { G[i] = HT_Grip3[i] + Nudge; F[i] = HT_Fore3[i]; }
		Me->SetWeaponGripPerCarry(G, F);
		Me->SetWeaponGrip(G[FMath::Clamp(HandTuneCarryIdx, 0, 2)]);   // the un-blended fallback, for when the sight solve is not running
	}
	Me->SetWeaponHandRotation(HT_HandRot);
	Me->SetWeaponForeGrip(HT_Fore3[FMath::Clamp(HandTuneCarryIdx, 0, 2)], HT_HasFore || !HT_Fore3[1].IsNearlyZero(), HT_ForePitch);
	Me->SetWeaponForeHandRotation(HT_ForeRot);
	Me->SetWeaponFingers(HT_FingersR, HT_FingersL);
	Me->SetWeaponHunch(HT_Hunch);
	Me->SetWeaponLean(HT_Lean);
	Me->SetWeaponCarryPos(HT_Pos3);
	Me->SetEyeTune(HT_EyeSide, HT_EyeUp, HT_EyeFwd);
	Me->SetWeaponLowReady(HT_LowReady[0], HT_LowReady[1]);
	Me->SetWeaponElbowTwist(HT_ElbowMain, HT_ElbowSup);
	Me->SetWeaponElbowAim(HT_ElbowMainAim, HT_ElbowSupAim);
	Me->SetWeaponDrawScale(HT_ScalePct * 0.01f);   // size first: the grip offset is measured on the scaled mesh
	HandTuneFitOptic();   // the sight, its offset, its paint and the eye line that follows them
}

float ABasePlayerController::HandTuneValue(int32 Row, int32 Col) const
{
	auto V = [](const FVector& P, int32 C) { return C == 0 ? P.X : C == 1 ? P.Y : P.Z; };
	auto R = [](const FRotator& P, int32 C) { return C == 0 ? P.Pitch : C == 1 ? P.Yaw : P.Roll; };
	auto F = [](const TArray<float>& A, int32 C) { return A.IsValidIndex(C) ? A[C] : 0.0f; };
	switch (Row)
	{
	case 0: return (float)V(HT_Grip3[FMath::Clamp(HandTuneCarryIdx, 0, 2)], Col);
	case 1: return (float)R(HT_HandRot, Col);
	case 2: return F(HT_FingersR, Col);
	case 3: return (float)V(HT_Fore3[FMath::Clamp(HandTuneCarryIdx, 0, 2)], Col);
	case 4: return (float)R(HT_ForeRot, Col);
	case 5: return F(HT_FingersL, Col);
	case 6: return HT_Hunch;
	case 7: return (Col >= 0 && Col < 3) ? HT_Pull3[Col] : 0.0f;
	case 8: return HT_Lean;
	case 9: return (Col >= 0 && Col < 3) ? HT_Lat3[Col] : 0.0f;
	case 10: return Col == 0 ? HT_EyeSide : Col == 1 ? HT_EyeUp : HT_EyeFwd;
	case 11: return (Col >= 0 && Col < 2) ? HT_LowReady[Col] : 0.0f;
	case 12: return (Col >= 0 && Col < 3) ? HT_ElbowMain[Col] : 0.0f;
	case 14: return (Col >= 0 && Col < 3) ? HT_ElbowMainAim[Col] : 0.0f;
	case 15: return (Col >= 0 && Col < 3) ? HT_ElbowSupAim[Col] : 0.0f;
	case 13: return (Col >= 0 && Col < 3) ? HT_ElbowSup[Col] : 0.0f;
	case 16: return (Col == 0) ? HT_OpticOff.X : (Col == 1) ? HT_OpticOff.Y : HT_OpticOff.Z;
	case 17: return HT_ScalePct;
	case 18: { const int32 C = FMath::Clamp(HandTuneCarryIdx, 0, 2); return (Col == 0) ? (float)HT_Pos3[C].X : (Col == 1) ? (float)HT_Pos3[C].Y : (float)HT_Pos3[C].Z; }
	default: return 0.0f;
	}
}

void ABasePlayerController::HandTuneAdjust(int32 Row, int32 Col, float Delta)
{
	auto V = [](FVector& P, int32 C, float D) { (C == 0 ? P.X : C == 1 ? P.Y : P.Z) += D; };
	auto R = [](FRotator& P, int32 C, float D) { (C == 0 ? P.Pitch : C == 1 ? P.Yaw : P.Roll) += D; };
	auto F = [](TArray<float>& A, int32 C, float D) { if (A.Num() < 5) { A.SetNumZeroed(5); } if (A.IsValidIndex(C)) { A[C] = FMath::Clamp(A[C] + D, -90.0f, 90.0f); } };
	switch (Row)
	{
	case 0: V(HT_Grip3[FMath::Clamp(HandTuneCarryIdx, 0, 2)], Col, Delta); break;   // the hold for the carry in view
	case 1: R(HT_HandRot, Col, Delta); break;
	case 2: F(HT_FingersR, Col, Delta); break;
	case 3: V(HT_Fore3[FMath::Clamp(HandTuneCarryIdx, 0, 2)], Col, Delta); break;
	case 4: R(HT_ForeRot, Col, Delta); break;
	case 5: F(HT_FingersL, Col, Delta); break;
	case 6: HT_Hunch = FMath::Clamp(HT_Hunch + Delta, -8.0f, 16.0f); break;   // centimetres of shrug, at the sights
	case 7: if (Col >= 0 && Col < 3) { HT_Pull3[Col] = FMath::Clamp(HT_Pull3[Col] + Delta, -30.0f, 30.0f); } break;
	case 8: HT_Lean = FMath::Clamp(HT_Lean + Delta, -20.0f, 40.0f); break;   // degrees at the waist, at the sights
	case 9: if (Col >= 0 && Col < 3) { HT_Lat3[Col] = FMath::Clamp(HT_Lat3[Col] + Delta, -20.0f, 40.0f); } break;
	case 10: if (Col == 0) { HT_EyeSide = FMath::Clamp(HT_EyeSide + Delta, -14.0f, 14.0f); } else if (Col == 1) { HT_EyeUp = FMath::Clamp(HT_EyeUp + Delta, -14.0f, 14.0f); } else { HT_EyeFwd = FMath::Clamp(HT_EyeFwd + Delta, -14.0f, 14.0f); } break;
	case 11: if (Col >= 0 && Col < 2) { HT_LowReady[Col] = FMath::Clamp(HT_LowReady[Col] + Delta, -90.0f, 90.0f); } break;
	// The optic, along the weapon own axes: x down the barrel, y across, z up off the rail.
	case 17: HT_ScalePct = FMath::Clamp(HT_ScalePct + Delta, 25.0f, 400.0f); break;
	// THE WEAPON'S PLACE. Only the carry the page is showing is touched -- the row is one carry wide.
	case 18:
	{
		const int32 C = FMath::Clamp(HandTuneCarryIdx, 0, 2);
		if (Col == 0) { HT_Pos3[C].X = FMath::Clamp(HT_Pos3[C].X + Delta, -40.0f, 40.0f); }
		else if (Col == 1) { HT_Pos3[C].Y = FMath::Clamp(HT_Pos3[C].Y + Delta, -20.0f, 40.0f); }
		else { HT_Pos3[C].Z = FMath::Clamp(HT_Pos3[C].Z + Delta, -40.0f, 40.0f); }
		break;
	}
	case 16:
		if (Col == 0) { HT_OpticOff.X = FMath::Clamp(HT_OpticOff.X + Delta, -40.0f, 40.0f); }
		else if (Col == 1) { HT_OpticOff.Y = FMath::Clamp(HT_OpticOff.Y + Delta, -20.0f, 20.0f); }
		else { HT_OpticOff.Z = FMath::Clamp(HT_OpticOff.Z + Delta, -20.0f, 20.0f); }
		break;
	case 12: if (Col >= 0 && Col < 3) { HT_ElbowMain[Col] = FMath::Clamp(HT_ElbowMain[Col] + Delta, -180.0f, 180.0f); } break;
	case 14: if (Col >= 0 && Col < 3) { HT_ElbowMainAim[Col] = FMath::Clamp(HT_ElbowMainAim[Col] + Delta, -180.0f, 180.0f); } break;
	case 15: if (Col >= 0 && Col < 3) { HT_ElbowSupAim[Col] = FMath::Clamp(HT_ElbowSupAim[Col] + Delta, -180.0f, 180.0f); } break;
	case 13: if (Col >= 0 && Col < 3) { HT_ElbowSup[Col] = FMath::Clamp(HT_ElbowSup[Col] + Delta, -180.0f, 180.0f); } break;
	default: return;
	}
	HandTuneApply();
	// MEASURING, NOT GUESSING. The position row reports what it just set and where the weapon
	// actually ended up, so one wheel click separates the two possible faults: if pos[] changes and
	// the weapon world does not, the value reaches the pawn and the solve is ignoring it; if pos[]
	// does not change, the break is before the pawn. (The world reading is one frame behind -- the
	// solve runs on Tick -- so compare successive clicks rather than one.)
	if (Row == 18 && HandTunePawn && HandTunePawn->WeaponMeshComponent)
	{
		const int32 C = FMath::Clamp(HandTuneCarryIdx, 0, 2);
		const FVector Wp = HandTunePawn->WeaponMeshComponent->GetComponentLocation();
		SetDiagNoteTimed(FString::Printf(TEXT("pos[%d] %.1f %.1f %.1f   weapon %.1f %.1f %.1f"),
			C, HT_Pos3[C].X, HT_Pos3[C].Y, HT_Pos3[C].Z, Wp.X, Wp.Y, Wp.Z), 6.0f);
	}
}


// THE PICTURE'S OWN AXES AND ITS SCALE. The hand-tune capture is ORTHOGRAPHIC, so a pixel is a
// fixed number of centimetres and no unprojection is needed: right and up come off the camera's
// rotation, so this is correct in every one of the five views AND after an orbit, rather than
// needing a case per view.

// WHERE THE TWO HANDS ARE, in world space, so the page can tell which one the pointer is over.
// Taken from the grip points ON THE WEAPON rather than from the hand bones: the bones lag the solve
// by an animation frame, and what the drag is about to edit is the grip, not the bone.
bool ABasePlayerController::HandTuneHandPoints(FVector& OutMain, FVector& OutSupport, bool& bOutHasSupport) const
{
	if (!HandTunePawn || !HandTunePawn->WeaponMeshComponent) { return false; }
	const FTransform WT = HandTunePawn->WeaponMeshComponent->GetComponentTransform();
	const int32 C = FMath::Clamp(HandTuneCarryIdx, 0, 2);
	OutMain = WT.TransformPosition(HT_Grip3[C]);
	OutSupport = WT.TransformPosition(HT_Fore3[C]);
	bOutHasSupport = HT_HasFore || !HT_Fore3[C].IsNearlyZero();
	return true;
}

// DRAGGING A HAND moves where it holds the weapon, which is the grip -- expressed in the WEAPON's
// own local space, so the world movement is unrotated by the weapon and divided by its draw scale.
// The weapon itself does not move: the solve still puts it where the carry position says, and it is
// the hand that slides along it. That is the opposite of dragging the gun, and it is why the two
// need different targets rather than one drag with a modifier.
void ABasePlayerController::HandTuneDragGrip(const FVector& WorldDelta, bool bSupport, int32 LockAxis)
{
	if (!HandTunePawn || !HandTunePawn->WeaponMeshComponent) { return; }
	const FTransform WT = HandTunePawn->WeaponMeshComponent->GetComponentTransform();
	FVector D = WT.GetRotation().UnrotateVector(WorldDelta);
	const FVector S = WT.GetScale3D();
	D = FVector(S.X > KINDA_SMALL_NUMBER ? D.X / S.X : D.X, S.Y > KINDA_SMALL_NUMBER ? D.Y / S.Y : D.Y, S.Z > KINDA_SMALL_NUMBER ? D.Z / S.Z : D.Z);
	if (LockAxis == 0) { D.Y = D.Z = 0.0f; }
	else if (LockAxis == 1) { D.X = D.Z = 0.0f; }
	else if (LockAxis == 2) { D.X = D.Y = 0.0f; }
	const int32 C = FMath::Clamp(HandTuneCarryIdx, 0, 2);
	FVector& G = bSupport ? HT_Fore3[C] : HT_Grip3[C];
	G += D;
	HandTuneApply();
}

bool ABasePlayerController::HandTuneDragAxes(FVector& OutRight, FVector& OutUp, float& OutOrthoWidth, FVector& OutCamLoc) const
{
	if (!HandTuneCap) { return false; }
	const USceneCaptureComponent2D* Cap = HandTuneCap->GetCaptureComponent2D();
	if (!Cap) { return false; }
	const FRotator R = HandTuneCap->GetActorRotation();
	OutRight = R.RotateVector(FVector::RightVector);
	OutUp = R.RotateVector(FVector::UpVector);
	OutOrthoWidth = Cap->OrthoWidth;
	OutCamLoc = HandTuneCap->GetActorLocation();
	return OutOrthoWidth > KINDA_SMALL_NUMBER;
}

// DRAGGING THE WEAPON. The world movement the pointer asked for, turned into the carry frame the
// position is stored in, and added to the carry in view. LockAxis 0/1/2 pins it to x, y or z --
// held X, Y or Z on the keyboard -- and -1 is a free drag. Same clamps as the wheel, so the two
// controls cannot disagree about what is reachable.
void ABasePlayerController::HandTuneDragPosition(const FVector& WorldDelta, int32 LockAxis)
{
	if (!HandTunePawn) { return; }
	FVector D = HandTunePawn->WorldToCarryFrame(WorldDelta);
	if (LockAxis == 0) { D.Y = D.Z = 0.0f; }
	else if (LockAxis == 1) { D.X = D.Z = 0.0f; }
	else if (LockAxis == 2) { D.X = D.Y = 0.0f; }
	const int32 C = FMath::Clamp(HandTuneCarryIdx, 0, 2);
	HT_Pos3[C].X = FMath::Clamp(HT_Pos3[C].X + D.X, -40.0f, 40.0f);
	HT_Pos3[C].Y = FMath::Clamp(HT_Pos3[C].Y + D.Y, -20.0f, 40.0f);
	HT_Pos3[C].Z = FMath::Clamp(HT_Pos3[C].Z + D.Z, -40.0f, 40.0f);
	HandTuneApply();
}

void ABasePlayerController::HandTuneOrbit(float DYaw, float DPitch)
{
	// Forty degrees off the chosen view either way: enough to see round the hand, not enough to
	// lose which way you are looking at it from.
	HT_Orbit.X = FMath::Clamp(HT_Orbit.X + DYaw, -40.0f, 40.0f);
	HT_Orbit.Y = FMath::Clamp(HT_Orbit.Y + DPitch, -40.0f, 40.0f);
}

void ABasePlayerController::ConfirmHandTuneLeave(int32 Dir)
{
	if (!ConfirmDialog) { ConfirmDialog = CreateWidget<UConfirmDialogWidget>(this, UConfirmDialogWidget::StaticClass()); }
	TWeakObjectPtr<ABasePlayerController> WeakThis(this);
	// The step happens AFTER the choice, whichever it was -- the dialog's callbacks run once it has
	// taken itself off the screen, so the page is not being rebuilt underneath a live prompt.
	const auto Go = [WeakThis, Dir]()
	{
		if (!WeakThis.IsValid()) { return; }
		WeakThis->HandTuneStepWeapon(Dir);
		if (WeakThis->HandTuneWidget) { WeakThis->HandTuneWidget->Refresh(); }
	};
	ConfirmDialog->Setup(TEXT("Unsaved changes"),
		FString::Printf(TEXT("Save changes to %s?"), *HandTuneWeaponLabel()),
		[WeakThis, Go]() { if (WeakThis.IsValid()) { WeakThis->HandTuneSave(); } Go(); },
		[WeakThis, Go]() { if (WeakThis.IsValid()) { WeakThis->HandTuneReset(); } Go(); },
		[]() {});   // Cancel: stay on this weapon, edits intact
	if (!ConfirmDialog->IsInViewport())
	{
		ConfirmDialog->AddToViewport(95);
		ConfirmDialog->SetAnchorsInViewport(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		ConfirmDialog->SetAlignmentInViewport(FVector2D::ZeroVector);
	}
}

bool ABasePlayerController::HandTuneDirty() const
{
	auto Same3 = [](const float* A, const float* B) { return FMath::IsNearlyEqual(A[0], B[0]) && FMath::IsNearlyEqual(A[1], B[1]) && FMath::IsNearlyEqual(A[2], B[2]); };
	auto SameArr = [](const TArray<float>& A, const TArray<float>& B)
	{
		if (A.Num() != B.Num()) { return false; }
		for (int32 i = 0; i < A.Num(); ++i) { if (!FMath::IsNearlyEqual(A[i], B[i])) { return false; } }
		return true;
	};
	if (!HT_Grip.Equals(HT_Grip0) || !HT_Fore.Equals(HT_Fore0)) { return true; }
	for (int32 i = 0; i < 3; ++i) { if (!HT_Grip3[i].Equals(HT_Grip3_0[i], 0.005) || !HT_Fore3[i].Equals(HT_Fore3_0[i], 0.005)) { return true; } }
	if (!HT_OpticOff.Equals(HT_OpticOff0) || HT_OpticSkin != HT_OpticSkin0 || HT_Optic != HT_Optic0) { return true; }
	if (!FMath::IsNearlyEqual(HT_ScalePct, HT_ScalePct0)) { return true; }
	if (!HT_HandRot.Equals(HT_HandRot0) || !HT_ForeRot.Equals(HT_ForeRot0)) { return true; }
	if (!SameArr(HT_FingersR, HT_FingersR0) || !SameArr(HT_FingersL, HT_FingersL0)) { return true; }
	if (!FMath::IsNearlyEqual(HT_Hunch, HT_Hunch0) || !FMath::IsNearlyEqual(HT_Lean, HT_Lean0)) { return true; }
	if (!Same3(HT_Pull3, HT_Pull3_0) || !Same3(HT_Lat3, HT_Lat3_0)) { return true; }
	// The weapon's place, all three carries: one row on the page, nine numbers behind it.
	for (int32 i = 0; i < 3; ++i) { if (!HT_Pos3[i].Equals(HT_Pos3_0[i], 0.005)) { return true; } }
	if (!Same3(HT_ElbowMain, HT_ElbowMain0) || !Same3(HT_ElbowSup, HT_ElbowSup0)) { return true; }
	if (!Same3(HT_ElbowMainAim, HT_ElbowMainAim0) || !Same3(HT_ElbowSupAim, HT_ElbowSupAim0)) { return true; }
	if (!FMath::IsNearlyEqual(HT_LowReady[0], HT_LowReady0[0]) || !FMath::IsNearlyEqual(HT_LowReady[1], HT_LowReady0[1])) { return true; }
	if (!FMath::IsNearlyEqual(HT_EyeSide, HT_EyeSide0) || !FMath::IsNearlyEqual(HT_EyeUp, HT_EyeUp0) || !FMath::IsNearlyEqual(HT_EyeFwd, HT_EyeFwd0)) { return true; }
	return false;
}

bool ABasePlayerController::HandTuneTakesOptic() const
{
	// Somewhere to bolt one on. Without a mount point an optic has no position of its own and lands
	// at the weapon's origin, inside the grip, which is worse than no optic at all.
	const WeaponCatalog::FWeapon* W = WeaponCatalog::Find(HandTuneName);
	return W && !W->OpticMount.IsNearlyZero();
}

FString ABasePlayerController::FittedOptic(const FString& Handle, const WeaponCatalog::FWeapon* W) const
{
	if (!W) { return FString(); }
	// A BUILT-IN SIGHT IGNORES THE COPY'S OPINION. An item instance can carry its own "optic" prop
	// and it normally wins over the type default -- that is how a modified rifle keeps the scope
	// somebody fitted to it. A weapon whose sight is part of its own geometry has no such freedom:
	// there is nothing to unclip and nowhere for a different sight to clamp, so the catalogue's
	// answer stands whatever the prop says. Without this the refusal is only skin deep: the menus
	// decline to change it while a stray or stale instance prop bolts a scope on anyway.
	if (W->bOpticFixed) { return W->Optic; }
	return ItemProp(Handle, TEXT("optic"), W->Optic);
}

bool ABasePlayerController::HandTuneOpticFixed() const
{
	// DELIBERATELY NOT FOLDED INTO HandTuneTakesOptic. That one means "there is nowhere to bolt a
	// sight on", and it makes the label read NO MOUNT and greys the row out. A fixed optic is the
	// opposite case: the weapon HAS a sight, mounted and working, and simply will not trade it.
	// Reporting one as the other would tell the player their BugBuster has no mount, which is false.
	const WeaponCatalog::FWeapon* W = WeaponCatalog::Find(HandTuneName);
	return W && W->bOpticFixed;
}

FString ABasePlayerController::HandTuneOpticLabel() const
{
	const WeaponCatalog::FWeapon* W = WeaponCatalog::Find(HandTuneName);
	if (!W) { return FString(); }
	// WHAT IS FITTED FIRST, THEN WHETHER IT HAS A MOUNT. This asked about the mount and returned
	// before ever looking at the sight, and the two are independent: only 49 of the catalogue's
	// weapons declare an optic_mount, so plenty of rifles carry a scope while failing that test.
	// Reporting "NO MOUNT" for one of those says the sight is ABSENT when it is fitted and working.
	// It cost hours: a marksman rifle was diagnosed as having no optic, then as needing a new tuning
	// control, when the real fault was that optic's eye point sitting 22 cm off its own glass. A
	// label that is confidently wrong is worse than an empty one -- it reads as information, and
	// nobody re-checks a screen that has already answered them.
	const FString Fitted = HT_Optic.IsEmpty() ? FString() : WeaponCatalog::OpticDisplayName(HT_Optic);   // what is being TRIED, not what is filed
	if (!HandTuneTakesOptic())
	{
		return Fitted.IsEmpty() ? TEXT("NO MOUNT") : FString::Printf(TEXT("%s - NO MOUNT"), *Fitted);
	}
	return Fitted.IsEmpty() ? TEXT("IRON SIGHTS") : Fitted;
}

// REAL CHARACTERS ONLY. Characters/ also holds render stand-ins and weapon test rigs, which are
// bodies but not people; offering them here would make the selector mostly scaffolding. The rule is
// the naming convention rather than a hand-kept list, so a new character appears without edits here
// -- at the cost that a real person named "...Test..." would be filtered out. Rename or amend.

// THE STAND-IN, and everything that has to be true about it: the body from the chosen character
// file, its own controller so the page's AIM can set a view rotation, the weapon in its hands, and
// ticking while the page has the game paused. Pulled out of ShowHandTune so that changing character
// rebuilds exactly the same thing rather than a half-set-up copy of it.
bool ABasePlayerController::HandTuneSpawnBooth(const WeaponCatalog::FWeapon* W)
{
	if (HandTuneController) { HandTuneController->Destroy(); HandTuneController = nullptr; }   // or the old one keeps possessing a destroyed pawn
	ABaseCharacter* Booth = SpawnBoothCharacter(HandTuneCharacter);   // spawns DEFERRED and sets the config before FinishSpawning, so BeginPlay builds the body from the file
	if (!Booth) { return false; }
	HandTunePawn = Booth;
	Booth->SetZoomLevel(2);   // third person: the head stays drawn
	FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	HandTuneController = GetWorld()->SpawnActor<AHandTuneController>(AHandTuneController::StaticClass(), Booth->GetActorLocation(), FRotator::ZeroRotator, SP);
	if (HandTuneController) { HandTuneController->Possess(Booth); HandTuneController->SetTickableWhenPaused(true); }
	if (W) { ApplyWeaponToPawn(Booth, W); }
	Booth->SetAiming(false);
	Booth->SetTicksWhenPaused(true);
	return true;
}

TArray<FString> ABasePlayerController::HandTuneCharacterList()
{
	TArray<FString> All = CharacterConfigFile::List();
	All.RemoveAll([](const FString& N)
	{
		return N.Contains(TEXT("Test")) || N.Contains(TEXT("Preview")) || N.Contains(TEXT("Poster"));
	});
	return All;
}

FString ABasePlayerController::HandTuneCharacterLabel() const
{
	return HandTuneCharacter.IsEmpty() ? TEXT("PLAYER") : HandTuneCharacter.ToUpper();
}

void ABasePlayerController::HandTuneStepCharacter(int32 Dir)
{
	// THE EYELINE IS THE ONE THING ON THIS PAGE THAT BELONGS TO THE BODY, so stepping the body
	// throws it away if it is unsaved. Refuse rather than lose it -- the weapon numbers are not at
	// risk, they belong to the weapon and the weapon is not changing.
	if (!FMath::IsNearlyEqual(HT_EyeSide, HT_EyeSide0) || !FMath::IsNearlyEqual(HT_EyeUp, HT_EyeUp0) || !FMath::IsNearlyEqual(HT_EyeFwd, HT_EyeFwd0))
	{
		SetDiagNoteTimed(TEXT("EYELINE IS UNSAVED -- SAVE OR RESET BEFORE CHANGING CHARACTER"), 5.0f);
		return;
	}
	const TArray<FString> All = HandTuneCharacterList();
	if (All.Num() == 0) { return; }
	int32 At = All.IndexOfByKey(HandTuneCharacter);
	if (At == INDEX_NONE) { At = 0; }
	HandTuneCharacter = All[(At + (Dir >= 0 ? 1 : All.Num() - 1)) % All.Num()];
	// The body in the booth and the eyeline it is read against change together, or the page would
	// be showing one character's numbers over another character's shoulders.
	const WeaponCatalog::FWeapon* W = WeaponCatalog::Find(HandTuneName);
	HandTuneSpawnBooth(W);
	FCharacterConfig Cfg;
	if (CharacterConfigFile::Load(HandTuneCharacter, Cfg))
	{
		HT_EyeSide0 = HT_EyeSide = Cfg.EyeSideCm; HT_EyeUp0 = HT_EyeUp = Cfg.EyeUpCm; HT_EyeFwd0 = HT_EyeFwd = Cfg.EyeForwardCm;
	}
	if (HandTunePawn) { HandTunePawn->SetEyeTune(HT_EyeSide, HT_EyeUp, HT_EyeFwd); }
	RefreshTunedWeapon(HandTuneName);
}

void ABasePlayerController::HandTuneStepOptic(int32 Dir)
{
	if (!HandTuneTakesOptic()) { return; }
	// AND THE WEAPON HAS TO BE WILLING TO TRADE IT. The widgets refuse this too, with a reason the
	// player can read -- but a refusal that lives only in the UI is a refusal any console command,
	// any later caller, walks straight past.
	if (HandTuneOpticFixed()) { return; }
	const WeaponCatalog::FWeapon* W = WeaponCatalog::Find(HandTuneName);
	if (!W) { return; }
	// The empty string first: iron sights are a choice, not the absence of one.
	TArray<FString> Choices; Choices.Add(FString());
	Choices.Append(WeaponCatalog::OpticNames());
	const int32 At = FMath::Max(0, Choices.IndexOfByKey(HT_Optic));
	HT_Optic = Choices[(At + (Dir >= 0 ? 1 : Choices.Num() - 1)) % Choices.Num()];
	HT_OpticKey = HT_Optic;
	// A different sight has its own offset and its own paint; start from what that optic says.
	if (const WeaponCatalog::FOptic* O = WeaponCatalog::FindOptic(HT_OpticKey))
	{
		HT_OpticOff = O->Offset;
		HT_OpticSkin = O->Skin;
	}
	else { HT_OpticOff = FVector::ZeroVector; HT_OpticSkin.Empty(); }
	HandTuneFitOptic();   // the STAND-IN only. Nothing is written until SAVE.
	RefreshHeldWeapon();
	RefreshTunedWeapon(HandTuneName);
}

void ABasePlayerController::HandTuneStepSkin(int32 Dir)
{
	// A FITTING, NOT A DECISION. This used to write the catalogue on every press, so flicking
	// through the paints to see them repainted every copy of the weapon in the world, one after
	// another, and there was no way to look without committing. Now the choice lives on the page
	// until SET DEFAULT adopts it.
	const WeaponCatalog::FWeapon* W = WeaponCatalog::Find(HandTuneName);
	if (!W || !HandTunePawn) { return; }
	const TArray<FString> V = WeaponSkins::Variants(*W);
	if (V.Num() < 2) { return; }
	const FString Wearing = HT_Skin.IsEmpty() ? WeaponSkins::Current(*W) : HT_Skin;
	const int32 At = FMath::Max(0, V.IndexOfByKey(Wearing));
	HT_Skin = V[(At + (Dir >= 0 ? 1 : V.Num() - 1)) % V.Num()];
	WeaponSkins::ApplyNamed(HandTunePawn->WeaponMeshComponent, *W, HT_Skin);
}

bool ABasePlayerController::HandTuneSkinIsDefault() const
{
	const WeaponCatalog::FWeapon* W = WeaponCatalog::Find(HandTuneName);
	if (!W) { return true; }
	return HT_Skin.IsEmpty() || HT_Skin == WeaponSkins::Current(*W);
}

void ABasePlayerController::HandTuneSetDefaultSkin()
{
	const WeaponCatalog::FWeapon* W = WeaponCatalog::Find(HandTuneName);
	if (!W || HT_Skin.IsEmpty()) { return; }
	const FString Key = W->Key;   // the write re-reads the catalogue, so W is stale after it
	if (!WeaponCatalog::WriteStringField(Key, TEXT("skin"), HT_Skin))
	{
		SetDiagNoteTimed(FString::Printf(TEXT("Could not write the paint for %s"), *Key), 4.0f);
		return;
	}
	// Everywhere it appears: the player's hands, every NPC carrying one, and the preview booth,
	// which is where the inventory icon is rendered from.
	RefreshHeldWeapon();
	RefreshTunedWeapon(HandTuneName);
	SetDiagNoteTimed(FString::Printf(TEXT("%s now wears %s by default"), *HandTuneName, *HT_Skin), 3.0f);
}

// The optic mesh the page is showing, or null when no sight is fitted.
static UStaticMesh* TunedOpticMesh(const FString& OpticKey)
{
	const WeaponCatalog::FOptic* O = WeaponCatalog::FindOptic(OpticKey);
	return (O && !O->MeshPath.IsEmpty()) ? LoadObject<UStaticMesh>(nullptr, *O->MeshPath, nullptr, LOAD_NoWarn | LOAD_Quiet) : nullptr;
}

void ABasePlayerController::HandTuneFitOptic()
{
	if (!IsValid(HandTunePawn)) { return; }
	const WeaponCatalog::FWeapon* W = WeaponCatalog::Find(HandTuneName);
	if (!W) { return; }
	const WeaponCatalog::FOptic* O = WeaponCatalog::FindOptic(HT_OpticKey);
	UStaticMesh* Mesh = (O && !O->MeshPath.IsEmpty()) ? LoadObject<UStaticMesh>(nullptr, *O->MeshPath, nullptr, LOAD_NoWarn | LOAD_Quiet) : nullptr;
	HandTunePawn->SetWeaponOptic(Mesh, (Mesh && O) ? W->OpticMount - O->Mount : FVector::ZeroVector,
		(Mesh && O) ? O->Rot : FRotator::ZeroRotator);
	HandTunePawn->SetWeaponOpticOffset(HT_OpticOff);
	HandTunePawn->SetWeaponOpticOptics(O ? O->Zoom : 1.0f, O && O->bSmart, O ? O->Reticle : FString(), O ? O->ReticleColour : FLinearColor(0.45f, 1.0f, 0.65f, 1.0f));
	// The eye goes where the glass is NOW, offset included -- otherwise the page aims through the
	// place the sight used to be.
	if (Mesh && O && !O->Eye.IsNearlyZero()) { HandTunePawn->SetWeaponSight(W->OpticMount - O->Mount + HT_OpticOff + O->Eye, true, 0.0f); }
	else { HandTunePawn->SetWeaponSight(W->Sight, W->bHasSight, W->SightPitch); }
	if (Mesh && HandTunePawn->OpticMeshComponent) { WeaponSkins::ApplyVariant(HandTunePawn->OpticMeshComponent, HT_OpticSkin); }
}

void ABasePlayerController::HandTuneStepOpticSkin(int32 Dir)
{
	UStaticMesh* Mesh = TunedOpticMesh(HT_OpticKey);
	if (!Mesh || !HandTunePawn) { return; }
	const TArray<FString> V = WeaponSkins::VariantsOfMesh(Mesh);
	if (V.Num() < 2) { return; }
	const FString Wearing = WeaponSkins::CurrentOfMesh(Mesh, HT_OpticSkin);
	const int32 At = FMath::Max(0, V.IndexOfByKey(Wearing));
	HT_OpticSkin = V[(At + (Dir >= 0 ? 1 : V.Num() - 1)) % V.Num()];
	WeaponSkins::ApplyVariant(HandTunePawn->OpticMeshComponent, HT_OpticSkin);
}

FString ABasePlayerController::HandTuneOpticSkinLabel() const
{
	UStaticMesh* Mesh = TunedOpticMesh(HT_OpticKey);
	const TArray<FString> V = Mesh ? WeaponSkins::VariantsOfMesh(Mesh) : TArray<FString>();
	if (V.Num() < 2) { return FString(); }   // one colourway is not a choice
	const FString Wearing = WeaponSkins::CurrentOfMesh(Mesh, HT_OpticSkin);
	return FString::Printf(TEXT("[ PAINT %d/%d ]"), V.IndexOfByKey(Wearing) + 1, V.Num());
}

FString ABasePlayerController::HandTuneSkinLabel() const
{
	const WeaponCatalog::FWeapon* W = WeaponCatalog::Find(HandTuneName);
	const TArray<FString> V = W ? WeaponSkins::Variants(*W) : TArray<FString>();
	if (V.Num() < 2) { return FString(); }   // nothing to choose between
	const FString Wearing = HT_Skin.IsEmpty() ? WeaponSkins::Current(*W) : HT_Skin;
	return FString::Printf(TEXT("[ SKIN %d/%d ]"), V.IndexOfByKey(Wearing) + 1, V.Num());
}

int32 ABasePlayerController::HandTunePresetCount() const { return WeaponCatalog::FingerPresets().Num(); }

FString ABasePlayerController::HandTunePresetName(int32 Index) const
{
	const TArray<WeaponCatalog::FFingerPreset>& P = WeaponCatalog::FingerPresets();
	return P.IsValidIndex(Index) ? P[Index].Name : FString();
}

void ABasePlayerController::HandTuneApplyPreset(bool bSupport, int32 Index)
{
	const TArray<WeaponCatalog::FFingerPreset>& P = WeaponCatalog::FingerPresets();
	if (!P.IsValidIndex(Index)) { return; }
	TArray<float>& Into = bSupport ? HT_FingersL : HT_FingersR;
	Into = P[Index].Values;
	Into.SetNumZeroed(5);
	HandTuneApply();
}

void ABasePlayerController::HandTuneSetPresetView(int32 Index)
{
	HT_ViewIdx = FMath::Clamp(Index, 0, 4);
	HT_Orbit = FVector2D::ZeroVector;   // a chosen side is a fresh start, not the old drag carried over
}

void ABasePlayerController::HandTuneZoomStep(int32 Direction)
{
	HT_Zoom = FMath::Clamp(HT_Zoom * FMath::Pow(0.8f, (float)FMath::Clamp(Direction, -1, 1)), 0.25f, 2.0f);   // in to a quarter of the frame, out to double
}

// The DRAG snaps back to the chosen view; the zoom does not.
void ABasePlayerController::HandTuneResetView() { HT_Orbit = FVector2D::ZeroVector; }

void ABasePlayerController::HandTuneReset()
{
	HT_Grip = HT_Grip0; HT_HandRot = HT_HandRot0; HT_Fore = HT_Fore0; HT_ForeRot = HT_ForeRot0; HT_FingersR = HT_FingersR0; HT_FingersL = HT_FingersL0;
	for (int32 i = 0; i < 3; ++i) { HT_Grip3[i] = HT_Grip3_0[i]; HT_Fore3[i] = HT_Fore3_0[i]; }
	HT_Hunch = HT_Hunch0; HT_Lean = HT_Lean0; HT_EyeSide = HT_EyeSide0; HT_EyeUp = HT_EyeUp0; HT_EyeFwd = HT_EyeFwd0;
	for (int32 i = 0; i < 3; ++i) { HT_Pull3[i] = HT_Pull3_0[i]; HT_Lat3[i] = HT_Lat3_0[i]; HT_Pos3[i] = HT_Pos3_0[i]; }
	for (int32 i = 0; i < 2; ++i) { HT_LowReady[i] = HT_LowReady0[i]; }
	for (int32 i = 0; i < 3; ++i) { HT_ElbowMain[i] = HT_ElbowMain0[i]; HT_ElbowSup[i] = HT_ElbowSup0[i]; }
	HT_OpticOff = HT_OpticOff0; HT_OpticSkin = HT_OpticSkin0; HT_Optic = HT_Optic0; HT_OpticKey = HT_Optic0; HT_ScalePct = HT_ScalePct0;
	for (int32 i = 0; i < 3; ++i) { HT_ElbowMainAim[i] = HT_ElbowMainAim0[i]; HT_ElbowSupAim[i] = HT_ElbowSupAim0[i]; }
	HandTuneApply();
}

bool ABasePlayerController::HandTuneSave()
{
	auto Doubles = [](const TArray<float>& A) { TArray<double> D; for (float V : A) { D.Add(V); } return D; };
	// PER-CARRY GRIPS, three triples each. The single "grip" and "fore_grip" stay as they are: they
	// are the legacy seed for a weapon that has never been tuned per carry, and overwriting them with
	// one of the three would quietly change what an untuned weapon falls back to.
	bool bOk = SaveWeaponTriples(HandTuneKey, TEXT("grip_carry"), HT_Grip3);
	bOk &= SaveWeaponTriples(HandTuneKey, TEXT("fore_carry"), HT_Fore3);
	bOk &= SaveWeaponField(HandTuneKey, TEXT("hand_rot"), { HT_HandRot.Pitch, HT_HandRot.Yaw, HT_HandRot.Roll });
	bOk &= SaveWeaponField(HandTuneKey, TEXT("fore_hand_rot"), { HT_ForeRot.Pitch, HT_ForeRot.Yaw, HT_ForeRot.Roll });
	bOk &= SaveWeaponField(HandTuneKey, TEXT("fingers_r"), Doubles(HT_FingersR));
	bOk &= SaveWeaponField(HandTuneKey, TEXT("fingers_l"), Doubles(HT_FingersL));
	bOk &= SaveWeaponField(HandTuneKey, TEXT("hunch"), { HT_Hunch }, true);
	// POSITION REPLACES PULL AND LATERAL. Both still parse as legacy input and seed a weapon that has
	// no position of its own; once this is written it is the only thing the solve reads.
	bOk &= SaveWeaponTriples(HandTuneKey, TEXT("position"), HT_Pos3);
	bOk &= SaveWeaponField(HandTuneKey, TEXT("lean"), { HT_Lean }, true);
	bOk &= SaveWeaponField(HandTuneKey, TEXT("low_ready"), { HT_LowReady[0], HT_LowReady[1] });
	bOk &= SaveWeaponField(HandTuneKey, TEXT("elbow_main"), { HT_ElbowMain[0], HT_ElbowMain[1], HT_ElbowMain[2] });
	bOk &= SaveWeaponField(HandTuneKey, TEXT("elbow_main_aim"), { HT_ElbowMainAim[0], HT_ElbowMainAim[1], HT_ElbowMainAim[2] });
	bOk &= SaveWeaponField(HandTuneKey, TEXT("elbow_support_aim"), { HT_ElbowSupAim[0], HT_ElbowSupAim[1], HT_ElbowSupAim[2] });
	bOk &= SaveWeaponField(HandTuneKey, TEXT("elbow_support"), { HT_ElbowSup[0], HT_ElbowSup[1], HT_ElbowSup[2] });
	// The optic offset goes to the OPTIC, not to this weapon. Only when one is fitted -- an iron
	// sighted gun has nothing to place.
	// THE OPTIC ITSELF, and only now. Which sight is fitted belongs to the weapon; where it sits and
	// what colour it is belong to the optic.
	if (HT_Optic != HT_Optic0) { bOk &= WeaponCatalog::WriteStringField(HandTuneKey, TEXT("optic"), HT_Optic); }
	bOk &= SaveWeaponField(HandTuneKey, TEXT("scale"), { HT_ScalePct * 0.01 }, true);
	if (!HT_OpticKey.IsEmpty())
	{
		bOk &= WeaponCatalog::WriteOpticNumbers(HT_OpticKey, TEXT("offset"), { HT_OpticOff.X, HT_OpticOff.Y, HT_OpticOff.Z });
		if (HT_OpticSkin != HT_OpticSkin0) { bOk &= WeaponCatalog::WriteOpticString(HT_OpticKey, TEXT("skin"), HT_OpticSkin); }
	}
	// THE EYELINE IS THE BODY'S. It goes to the character file, not the weapon: loaded, the two
	// numbers replaced, saved, so nothing else in the likeness is touched by a hand-tuning save.
	{
		FCharacterConfig Cfg;
		if (CharacterConfigFile::Load(HandTuneCharacter, Cfg))   // the body the page is tuning, not always the player
		{
			Cfg.EyeSideCm = HT_EyeSide; Cfg.EyeUpCm = HT_EyeUp; Cfg.EyeForwardCm = HT_EyeFwd;
			bOk &= CharacterConfigFile::Save(Cfg);
		}
		if (ABaseCharacter* PlayerPawn = Cast<ABaseCharacter>(GetPawn())) { PlayerPawn->SetEyeTune(HT_EyeSide, HT_EyeUp, HT_EyeFwd); }   // the player aims by it at once
	}
	if (bOk)
	{
		// AND EVERY COPY OF THAT GUN TAKES THE NEW NUMBERS NOW. ApplyWeaponToPawn pushes the
		// catalogue onto a character once, when the weapon is put in its hands, and nothing ever
		// pushed it again: a weapon picked up before a tuning session went on wearing the old values
		// for as long as it was carried. That is most of why the hold kept looking right in the
		// tuner and wrong in the world. SaveWeaponField has already dropped the catalogue's cache,
		// so this re-reads from disk and re-dresses the player, every NPC carrying the same weapon,
		// and the page's own stand-in.
		RefreshHeldWeapon();
		const int32 Dressed = RefreshTunedWeapon(HandTuneName);
		HandTuneApply();   // and the stand-in goes on showing the page, which is the only thing it should ever show
		UE_LOG(LogTemp, Log, TEXT("HandTune: saved %s and re-dressed %d character(s) holding one."), *HandTuneName, Dressed);
		HT_Grip0 = HT_Grip; HT_HandRot0 = HT_HandRot; HT_Fore0 = HT_Fore; HT_ForeRot0 = HT_ForeRot; HT_FingersR0 = HT_FingersR; HT_FingersL0 = HT_FingersL;
		for (int32 i = 0; i < 3; ++i) { HT_Grip3_0[i] = HT_Grip3[i]; HT_Fore3_0[i] = HT_Fore3[i]; }
		HT_Hunch0 = HT_Hunch; HT_Lean0 = HT_Lean; HT_EyeSide0 = HT_EyeSide; HT_EyeUp0 = HT_EyeUp; HT_EyeFwd0 = HT_EyeFwd;
		for (int32 i = 0; i < 3; ++i) { HT_Pull3_0[i] = HT_Pull3[i]; HT_Lat3_0[i] = HT_Lat3[i]; HT_Pos3_0[i] = HT_Pos3[i]; }
		for (int32 i = 0; i < 2; ++i) { HT_LowReady0[i] = HT_LowReady[i]; }
		for (int32 i = 0; i < 3; ++i) { HT_ElbowMain0[i] = HT_ElbowMain[i]; HT_ElbowSup0[i] = HT_ElbowSup[i]; }
		HT_OpticOff0 = HT_OpticOff; HT_OpticSkin0 = HT_OpticSkin; HT_Optic0 = HT_Optic; HT_ScalePct0 = HT_ScalePct;
		for (int32 i = 0; i < 3; ++i) { HT_ElbowMainAim0[i] = HT_ElbowMainAim[i]; HT_ElbowSupAim0[i] = HT_ElbowSupAim[i]; }
		RefreshHeldWeapon();   // NOW the world changes: the player's held weapon takes the saved numbers
	}
	return bOk;
}

void ABasePlayerController::ShowSaveLoad(bool bLoad)
{
	if (!SaveLoadWidget)
	{
		SaveLoadWidget = CreateWidget<USaveLoadWidget>(this, USaveLoadWidget::StaticClass());
		if (!SaveLoadWidget) { return; }
		SaveLoadWidget->OnClose.BindUObject(this, &ABasePlayerController::HideSaveLoad);
	}
	SaveLoadWidget->Open(this, bLoad);
	if (!SaveLoadWidget->IsInViewport()) { SaveLoadWidget->AddToViewport(60); }
	Screens.Open(EScreen::SaveLoad);
	ApplyInputMode();
}

void ABasePlayerController::HideSaveLoad()
{
	if (!Screens.IsOpen(EScreen::SaveLoad)) { return; }
	Screens.Close(EScreen::SaveLoad);
	if (SaveLoadWidget && SaveLoadWidget->IsInViewport()) { SaveLoadWidget->RemoveFromParent(); }
	if (!IsPageOpen()) { SetPause(false); }   // see HideReference: a page that pauses must unpause
	ApplyInputMode();
}

void ABasePlayerController::OnConversationDigit(FKey Key)
{
	if (!IsInConversation() && !IsInCinematic()) { return; }
	static const FKey Digits[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
	for (int32 i = 0; i < 9; ++i) { if (Key == Digits[i]) { ChooseConversationOption(i); return; } }
}

bool ABasePlayerController::IsInspectMenuOpen() const
{
	return InspectMenuWidget && InspectMenuWidget->IsInViewport();
}

void ABasePlayerController::OnInspectWheel(float Value)
{
	if (Value == 0.0f || !IsInspectMenuOpen()) { return; }
	InspectSelectNext(Value > 0.0f ? -1 : 1);   // wheel up = previous option
}

void ABasePlayerController::OnInspectUse()
{
	if (Carried.IsValid()) { StoreCarried(); return; }   // a tap while carrying puts it away
	if (!IsInspectMenuOpen()) { return; }
	AActor* Target = InspectTarget.Get();
	if (CanCarry(Target))
	{
		// Perhaps a hold: the action waits for the release (a tap), or a carry begins (a hold).
		bInspectUseHeld = true; bCarryBegunThisHold = false; HoldCandidate = Target;
		InspectUseHeldSince = GetWorld()->GetTimeSeconds();
		return;
	}
	InspectUseSelected();
}

void ABasePlayerController::OnInspectUseReleased()
{
	if (!bInspectUseHeld) { return; }
	bInspectUseHeld = false;
	if (bCarryBegunThisHold) { DropCarried(); return; }
	if (HoldCandidate.IsValid() && InspectTarget.Get() == HoldCandidate.Get()) { InspectUseSelected(); }   // a tap: the action it would have been
	HoldCandidate = nullptr;
}

bool ABasePlayerController::CanCarry(AActor* Target) const
{
	// A loose prop: tagged for the reticle, offered Take (or already knocked loose), with simple
	// collision to simulate, and no heavier than a person lifts one-handed.
	const AStaticMeshActor* Prop = Cast<AStaticMeshActor>(Target);
	if (!Prop || !Prop->ActorHasTag(TEXT("inspectable"))) { return false; }
	bool bTake = false;
	for (const FName& T : Prop->Tags) { if (T == TEXT("action:Take") || T == TEXT("loose")) { bTake = true; break; } }
	if (!bTake) { return false; }
	const UStaticMeshComponent* C = Prop->GetStaticMeshComponent();
	const UStaticMesh* Mesh = C ? C->GetStaticMesh() : nullptr;
	const UBodySetup* Body = Mesh ? Mesh->GetBodySetup() : nullptr;
	return Body && Body->CollisionTraceFlag != CTF_UseComplexAsSimple && Body->AggGeom.GetElementCount() > 0;
}

void ABasePlayerController::BeginCarry(AActor* Target)
{
	AStaticMeshActor* Prop = Cast<AStaticMeshActor>(Target);
	ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn());
	if (!Prop || !Me || !CanCarry(Prop)) { return; }
	UStaticMeshComponent* C = Prop->GetStaticMeshComponent();
	// Mass as a shot would find it: the catalogue's, else a guess from the size; too heavy stays put.
	FString Name; for (const FName& T : Prop->Tags) { const FString S = T.ToString(); if (S.StartsWith(TEXT("name:"))) { Name = S.Mid(5); break; } }
	const ItemCatalog::FRecord* R = Name.IsEmpty() ? nullptr : ItemCatalog::FindRecord(Name);
	float Mass = R ? (float)R->Number(TEXT("mass_kg"), 0.0) : 0.0f;
	const FBoxSphereBounds B = C->GetStaticMesh()->GetBounds();
	if (Mass <= 0.0f) { const FVector E = B.BoxExtent * 2.0f * Prop->GetActorScale3D(); Mass = FMath::Clamp(E.X * E.Y * E.Z * 1e-6f * 220.0f, 0.2f, 200.0f); }
	if (Mass > 25.0f) { ShowCallout(Prop, TEXT("Too heavy to lift."), 2.0f, false); return; }
	C->SetMobility(EComponentMobility::Movable);
	C->SetCollisionProfileName(TEXT("PhysicsActor"));
	C->SetSimulatePhysics(true);
	C->SetMassOverrideInKg(NAME_None, Mass, true);
	C->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);   // it must not shove its bearer
	C->SetEnableGravity(false);
	C->SetLinearDamping(4.0f); C->SetAngularDamping(6.0f);
	Prop->Tags.AddUnique(TEXT("loose"));
	Carried = Prop; CarriedComp = C;
	CarriedRadius = B.SphereRadius * Prop->GetActorScale3D().GetAbsMax();
	CarryDistance = FMath::Clamp(CarriedRadius * 1.3f + 35.0f, 45.0f, 90.0f);
	HideInspectMenu();
	SetInspectHighlight(Prop, false);
	if (InspectTarget.Get() == Prop) { InspectTarget = nullptr; }
	Me->SetCarried(C, CarriedRadius);
	bCarryBegunThisHold = true;
	SetDiagNoteTimed(FString::Printf(TEXT("carrying %s: let go of E to set it down, tap E to bag it"), *(Name.IsEmpty() ? Prop->GetActorNameOrLabel() : Name)), 3.0f);
}

void ABasePlayerController::TickCarry(float DeltaSeconds)
{
	if (bInspectUseHeld && !bCarryBegunThisHold && HoldCandidate.IsValid() && GetWorld()->GetTimeSeconds() - InspectUseHeldSince >= CarryHoldSeconds) { BeginCarry(HoldCandidate.Get()); }
	if (!Carried.IsValid() || !CarriedComp.IsValid()) { if (Carried.IsValid() || CarriedComp.IsValid()) { DropCarried(); } return; }
	ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn());
	if (!Me) { DropCarried(); return; }
	// Held out in front, a little below the eye line, and pulled in before anything in the way.
	const FVector Eye = Me->GetAimOrigin();
	const FVector Dir = Me->GetAimRotation().Vector();
	FVector Target = Eye + Dir * CarryDistance - FVector(0.0f, 0.0f, 12.0f);
	FHitResult Hit; FCollisionQueryParams Q(SCENE_QUERY_STAT(Carry), false, Me); Q.AddIgnoredActor(Carried.Get());
	if (GetWorld()->LineTraceSingleByChannel(Hit, Eye, Target, ECC_Visibility, Q)) { Target = Hit.ImpactPoint - Dir * CarriedRadius; }
	const FVector Pos = CarriedComp->GetComponentLocation();
	CarriedComp->SetPhysicsLinearVelocity(((Target - Pos) * 10.0f).GetClampedToMaxSize(900.0f));
	CarriedComp->SetPhysicsAngularVelocityInDegrees(CarriedComp->GetPhysicsAngularVelocityInDegrees() * 0.85f);
	if (FVector::Dist(Pos, Target) > 250.0f) { DropCarried(); }   // torn out of the hand
}

void ABasePlayerController::DropCarried()
{
	if (UPrimitiveComponent* C = CarriedComp.Get())
	{
		C->SetEnableGravity(true);
		C->SetLinearDamping(0.01f); C->SetAngularDamping(0.0f);
		C->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	}
	Carried = nullptr; CarriedComp = nullptr; HoldCandidate = nullptr;
	bCarryBegunThisHold = false;
	if (ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn())) { Me->SetCarried(nullptr, 0.0f); }
}

void ABasePlayerController::StoreCarried()
{
	AActor* A = Carried.Get();
	DropCarried();
	if (A) { ExecuteInspectAction(A, TEXT("Take")); }
}

void ABasePlayerController::InspectSelectNext(int32 Delta)
{
	if (InspectActions.Num() == 0) { return; }
	InspectSelection = ((InspectSelection + Delta) % InspectActions.Num() + InspectActions.Num()) % InspectActions.Num();
	if (InspectMenuWidget) { InspectMenuWidget->SetSelectedIndex(InspectSelection); }
}

void ABasePlayerController::InspectSelectIndex(int32 Index)
{
	if (!InspectActions.IsValidIndex(Index)) { return; }
	InspectSelection = Index;
	if (InspectMenuWidget) { InspectMenuWidget->SetSelectedIndex(InspectSelection); }
}

void ABasePlayerController::InspectUseSelected()
{
	AActor* Target = InspectTarget.Get();
	if (!Target || !InspectActions.IsValidIndex(InspectSelection)) { return; }
	ExecuteInspectAction(Target, InspectActions[InspectSelection]);
}

void ABasePlayerController::DescribeInspectable(AActor* Target, FString& OutName, FString& OutDescription, TArray<FString>& OutActions)
{
	OutName.Reset(); OutDescription.Reset(); OutActions.Reset();
	if (!Target) { return; }
	if (const ABaseCharacter* AsCharacter = Cast<ABaseCharacter>(Target))
	{
		OutName = AsCharacter->GetCharacterConfig().Name;
		OutDescription = AsCharacter->GetDescription();
		OutActions.Add(TEXT("Inspect"));
		if (!AsCharacter->GetComment().IsEmpty()) { OutActions.Add(TEXT("Talk")); }
		return;
	}
	if (const AElevatorActor* Lift = Cast<AElevatorActor>(Target))
	{
		OutName = TEXT("Service lift");
		// The panel only offers floors to somebody standing in the car. From outside it is a
		// call button, which is the only thing a lift panel in a corridor can honestly be.
		if (Lift->IsSomeoneAboard())
		{
			OutDescription = FString::Printf(TEXT("At %s."), *Lift->FloorName(Lift->GetCurrentFloor()));
			OutActions = Lift->FloorMenu();
		}
		else
		{
			OutDescription = Lift->IsMoving() ? TEXT("Moving. The indicator is counting.")
			                                  : FString::Printf(TEXT("Waiting at %s."), *Lift->FloorName(Lift->GetCurrentFloor()));
			OutActions.Add(TEXT("Call"));
		}
		return;
	}
	if (const ASlidingDoorActor* Door = Cast<ASlidingDoorActor>(Target))
	{
		OutName = Door->GetDoorName();
		OutDescription = Door->IsManual() ? TEXT("A cabin door. It waits to be told.") : TEXT("It opens for whoever walks up to it.");
		if (Door->IsLocked()) { OutActions.Add(TEXT("Locked")); }
		else if (Door->IsManual()) { OutActions.Add(Door->IsHeldOpen() ? TEXT("Close") : TEXT("Open")); }
		return;   // no Inspect on doors: Open / Close / Locked is the whole menu
	}
	if (const AInspectSurface* Surface = Cast<AInspectSurface>(Target))
	{
		OutName = Surface->Name;
		OutDescription = Surface->Description;
		OutActions = Surface->Actions;
		if (OutActions.Num() == 0) { OutActions.Add(TEXT("Inspect")); }
		return;
	}
	if (const ALootBoxActor* Box = Cast<ALootBoxActor>(Target))
	{
		OutName = Box->DisplayName;
		OutDescription = Box->Description;
		OutActions.Add(TEXT("Open"));
		OutActions.Add(TEXT("Inspect"));
		return;
	}
	// Props: "name:", "desc:" and "action:" tags; a readable label otherwise.
	for (const FName& Tag : Target->Tags)
	{
		const FString T = Tag.ToString();
		if (T.StartsWith(TEXT("name:"))) { OutName = T.Mid(5); }
		else if (T.StartsWith(TEXT("desc:"))) { OutDescription = T.Mid(5); }
		else if (T.StartsWith(TEXT("action:"))) { OutActions.AddUnique(T.Mid(7)); }
	}
	// The seat we are on offers Stand instead of Sit.
	if (ABaseCharacter* Me = Target ? Cast<ABaseCharacter>(UGameplayStatics::GetPlayerPawn(Target->GetWorld(), 0)) : nullptr) { if (Me->IsSitting() && Me->GetSeat() == Target) { OutActions.Reset(); OutActions.Add(TEXT("Stand")); } }
	if (OutName.IsEmpty())
	{
		// "Camp_Cart_01" / "SM_Prop_Chair_01" -> "Cart" / "Chair".
		FString Label = Target->GetActorNameOrLabel();   // GetActorLabel is editor-only
		for (const TCHAR* Prefix : { TEXT("Camp_"), TEXT("SM_Prop_"), TEXT("SM_") }) { Label.RemoveFromStart(Prefix); }
		while (Label.Len() > 3 && FChar::IsDigit(Label[Label.Len() - 1])) { Label.LeftChopInline(1); }
		Label.RemoveFromEnd(TEXT("_"));
		Label.ReplaceInline(TEXT("_"), TEXT(" "));
		OutName = Label;
	}
	if (Target->ActorHasTag(TEXT("dead"))) { OutActions.Remove(TEXT("Talk")); OutActions.Remove(TEXT("Call")); }   // the dead do not talk
	if (OutActions.Num() == 0) { OutActions.Add(TEXT("Inspect")); }
}

void ABasePlayerController::RefreshInspectMenu(AActor* Target)
{
	if (!Target) { HideInspectMenu(); return; }
	FString Name, Description;
	DescribeInspectable(Target, Name, Description, InspectActions);
	InspectSelection = 0;
	if (!InspectMenuWidget) { InspectMenuWidget = CreateWidget<UInspectMenuWidget>(this, UInspectMenuWidget::StaticClass()); }
	const bool bFlavour = InspectActions.Num() == 1 && InspectActions[0] == TEXT("Inspect");
	bInspectMenuFlavour = bFlavour;
	InspectMenuWidget->SetContent(Name, FString(), bFlavour ? TArray<FString>() : InspectActions, InspectSelection);   // the description shows through Inspect, not in the menu; flavour is a name tag with no rows
	if (!InspectMenuWidget->IsInViewport()) { InspectMenuWidget->AddToViewport(50); }
	UpdateInspectMenuPosition();
}

int32 ABasePlayerController::GetInspectMenuMode() const
{
	const int32 Forced = CVarInspectMenu.GetValueOnGameThread();
	return (Forced >= 0 ? Forced : InspectMenuMode) % 5;
}

void ABasePlayerController::CycleInspectMenuMode()
{
	static const TCHAR* Names[] = { TEXT("floated up-right, leader"), TEXT("beside the thing"), TEXT("top centre"), TEXT("top right, leader line"), TEXT("above the reticle") };
	InspectMenuMode = (GetInspectMenuMode() + 1) % 5;
	ShowCallout(GetPawn(), FString::Printf(TEXT("Inspect menu: %s"), Names[InspectMenuMode]), 2.0f, false);
	if (InspectMenuWidget && InspectMenuWidget->IsInViewport()) { UpdateInspectMenuPosition(); }
}

void ABasePlayerController::UpdateInspectMenuPosition()
{
	AActor* Target = InspectTarget.Get();
	if (!Target || !InspectMenuWidget || !InspectMenuWidget->IsInViewport()) { return; }
	// A world point on the entity: chest height for characters, the centre
	// of anything else; then a fixed screen offset to its right so the panel
	// sits beside the outline rather than over it.
	FVector World;
	float ScreenOffsetX = 60.0f;
	if (Target->IsA<ABaseCharacter>())
	{
		World = Target->GetActorLocation() + FVector(0, 0, 40.0f);
		ScreenOffsetX = 70.0f;
	}
	else
	{
		FVector Origin, Extent;
		Target->GetActorBounds(false, Origin, Extent);
		World = Origin;
		ScreenOffsetX = 40.0f;
	}
	FVector2D Screen;
	const bool bOnScreen = UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(this, World, Screen, false);
	const FVector2D View = UWidgetLayoutLibrary::GetViewportSize(this);
	const int32 Mode = GetInspectMenuMode();
	FVector2D Pos, Align;
	// The leader lands on the exact point the reticle trace hit, not the entity's centre.
	FVector2D Anchor = Screen;
	if (bAnchorValid) { FVector2D A; if (UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(this, InspectAnchorWorld, A, false)) { Anchor = A; } }
	switch (Mode)
	{
	default:
	case 0:   // floated up and to the right of the thing, just outside its silhouette, clamped to the screen
	{
		if (!bOnScreen) { return; }
		// The thing's apparent radius in pixels: the panel keeps a fixed gap from its edge, so a
		// far thing gets a close panel and a near one a wide berth. First person puts the thing at
		// the reticle, so the panel sits up-right of the reticle instead.
		FVector Origin, Extent;
		Target->GetActorBounds(false, Origin, Extent);
		const float Dist = PlayerCameraManager ? FMath::Max(1.0f, FVector::Dist(PlayerCameraManager->GetCameraLocation(), Origin)) : 300.0f;
		const float HalfFov = FMath::DegreesToRadians(PlayerCameraManager ? PlayerCameraManager->GetFOVAngle() * 0.5f : 45.0f);
		const float RadiusPx = FMath::Clamp(Extent.Size() * (View.Y * 0.5f) / (Dist * FMath::Tan(HalfFov)), 12.0f, 260.0f);
		const ABaseCharacter* Ch = Cast<ABaseCharacter>(GetPawn());
		const bool bFirstPerson = Ch && Ch->GetZoomLevelIndex() == 0;
		const FVector2D From = bFirstPerson ? FVector2D(View.X * 0.5f, View.Y * 0.5f) : Screen;
		Pos = From + FVector2D(RadiusPx * 0.8f + 48.0f, -(RadiusPx * 0.55f + 36.0f));
		Pos.X = FMath::Clamp(Pos.X, 40.0f, View.X - 340.0f);
		Pos.Y = FMath::Clamp(Pos.Y, 60.0f, View.Y * 0.7f);
		Align = FVector2D(0.0f, 1.0f);
		break;
	}
	case 1: if (!bOnScreen) { return; } Pos = Screen + FVector2D(ScreenOffsetX, 0.0f); Align = FVector2D(0.0f, 0.5f); break;   // beside the thing
	case 2: Pos = FVector2D(View.X * 0.5f, View.Y * 0.16f); Align = FVector2D(0.5f, 0.0f); break;                              // fixed, high, centred
	case 3: Pos = FVector2D(View.X - 48.0f, View.Y * 0.14f); Align = FVector2D(1.0f, 0.0f); break;                             // fixed top-right, leader to the thing
	case 4: Pos = FVector2D(View.X * 0.5f, View.Y * 0.5f - 140.0f); Align = FVector2D(0.5f, 1.0f); break;                      // just above the reticle
	}
	InspectMenuWidget->SetAlignmentInViewport(Align);
	InspectMenuWidget->SetPositionInViewport(Pos, true);
	InspectMenuWidget->SetLeader(Mode == 3 && bOnScreen, Anchor, Pos, Align);   // no leader on the floated default: it never quite met the thing
}

void ABasePlayerController::HideInspectMenu()
{
	if (InspectMenuWidget && InspectMenuWidget->IsInViewport()) { InspectMenuWidget->RemoveFromParent(); }
	InspectActions.Reset();
	InspectSelection = 0;
}

bool ABasePlayerController::ToggleTaggedLights(AActor* Target, const FString& Action)
{
	if (!Target || !GetWorld()) { return false; }
	const FString Prefix = TEXT("toggle:") + Action + TEXT("=");
	FString Id;
	for (const FName& Tag : Target->Tags) { const FString S = Tag.ToString(); if (S.StartsWith(Prefix)) { Id = S.Mid(Prefix.Len()); break; } }
	if (Id.IsEmpty()) { return false; }
	const FName Want(*(TEXT("lightid:") + Id));
	int32 Flipped = 0;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (!It->ActorHasTag(Want)) { continue; }
		TInlineComponentArray<ULightComponent*> Lights(*It);
		for (ULightComponent* L : Lights) { L->SetVisibility(!L->IsVisible()); ++Flipped; }
	}
	if (Flipped == 0) { ShowCallout(Target, TEXT("Nothing answers."), 2.0f, false); }
	else { UAmbientPlayer::PlayOneShot(this, GetWorld(), TEXT("switch_click.wav"), 0.55f, 1.0f); }   // the rocker going over
	return true;
}

void ABasePlayerController::ExecuteInspectAction(AActor* Target, const FString& Action)
{
	FString Name, Description;
	TArray<FString> Unused;
	DescribeInspectable(Target, Name, Description, Unused);
	if (AElevatorActor* Lift = Cast<AElevatorActor>(Target))
	{
		if (Action == TEXT("Call"))
		{
			// Called from outside: bring it to whichever floor the caller is standing on.
			const APawn* Me = GetPawn();
			const float Rise = Me ? (Me->GetActorLocation().Z - Lift->GetActorLocation().Z) : 0.0f;
			Lift->GoToFloor(Lift->GetCurrentFloor() + FMath::RoundToInt(Rise / FMath::Max(Lift->FloorHeight, 1.0f)));
			return;
		}
		const int32 Floor = Lift->FloorFromName(Action);
		if (Floor > -1000)
		{
			Lift->GoToFloor(Floor);
			SetDiagNoteTimed(FString::Printf(TEXT("Lift: %s"), *Lift->FloorName(Floor)), 3.0f);
		}
		return;
	}
	if (Action == TEXT("Talk"))
	{
		// A conversation tree if the character has one, else the stub remark.
		if (ABaseCharacter* AsCharacter = Cast<ABaseCharacter>(Target))
		{
			if (!StartConversation(AsCharacter)) { ShowCallout(Target, AsCharacter->GetComment(), 4.0f, true); }
		}
	}
	else if (Action == TEXT("Inspect"))
	{
		ShowCallout(Target, Description.IsEmpty() ? FString::Printf(TEXT("%s. Nothing more to see."), *Name) : Description, 3.5f, false);
	}
	else if (Action == TEXT("Open") || Action == TEXT("Close"))
	{
		if (ASlidingDoorActor* Door = Cast<ASlidingDoorActor>(Target)) { Door->SetHoldOpen(Action == TEXT("Open")); }
		else if (ALootBoxActor* Box = Cast<ALootBoxActor>(Target)) { OpenTransfer(Box); }   // the lid stays on for now (SetOpen swings it off)
		else { ShowCallout(Target, TEXT("It does not open."), 2.0f, false); }   // a crate or locker with nothing behind it yet
		if (InspectTarget.IsValid()) { RefreshInspectMenu(InspectTarget.Get()); }
	}
	else if (Action == TEXT("Use") && IsTerminal(Target))
	{
		OpenTerminal(Target);
	}
	else if (Action == TEXT("Loot"))
	{
		// A body's pockets: the hidden loot box riding it (ShotReactions::CorpseLoot).
		TArray<AActor*> Attached; Target->GetAttachedActors(Attached, true, true);
		ALootBoxActor* Box = nullptr;
		for (AActor* A : Attached) { if (A && A->ActorHasTag(TEXT("corpse_loot"))) { Box = Cast<ALootBoxActor>(A); if (Box) { break; } } }
		if (Box) { OpenTransfer(Box); } else { ShowCallout(Target, TEXT("Nothing on them."), 2.0f, false); }
	}
	else if (Action == TEXT("Read"))
	{
		// A note, a sign, a pad: its line is what it says.
		ShowCallout(Target, Description.IsEmpty() ? TEXT("Nothing legible.") : Description, 4.0f, false);
	}
	else if (Action == TEXT("Locked"))
	{
		ShowCallout(Target, TEXT("Locked."), 2.0f, false);
	}
	else if (Action == TEXT("Sit") || Action == TEXT("Stand"))
	{
		if (ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn()))
		{
			if (Action == TEXT("Stand")) { Me->StandUp(); }
			else
			{
				SitOnTagged(Me, Target);
			}
			if (InspectTarget.IsValid()) { RefreshInspectMenu(InspectTarget.Get()); }
		}
	}
	else if (ToggleTaggedLights(Target, Action))
	{
		// handled: a light switch panel (tags toggle:<Action>=<id> / lightid:<id>)
	}
	else if (Action == TEXT("Wear"))
	{
		// Straight onto the body: taken, then into the square its kind equips; what was worn takes
		// the bag square it left. A garment cut for the other body says so and stays where it is.
		FString Why;
		if (!ClothingFits(Name, Why)) { ShowCallout(Target, Why, 2.5f, false); return; }
		if (!AddToInventory(Name, false)) { ShowCallout(GetPawn(), TEXT("No room."), 2.0f, false); return; }
		const int32 At = Inventory.FindLast(Name);
		if (At != INDEX_NONE && EquipFromInventory(At)) { ShowCallout(GetPawn(), FString::Printf(TEXT("Wearing %s"), *Name), 2.5f, false); }
		else { ShowCallout(GetPawn(), FString::Printf(TEXT("Took %s"), *Name), 2.5f, false); }
		Target->SetActorHiddenInGame(true);
		Target->SetActorEnableCollision(false);
		SetInspectHighlight(Target, false);
		InspectTarget = nullptr;
		HideInspectMenu();
	}
	else if (Action == TEXT("Take"))
	{
		if (!AddToInventory(Name, true)) { ShowCallout(GetPawn(), TEXT("No room."), 2.0f, false); return; }
		ShowCallout(GetPawn(), FString::Printf(TEXT("Took %s"), *Name), 2.5f, false);
		// Off the world, kept around for gameplay to reclaim.
		Target->SetActorHiddenInGame(true);
		Target->SetActorEnableCollision(false);
		SetInspectHighlight(Target, false);
		InspectTarget = nullptr;
		HideInspectMenu();
	}
	else
	{
		// A line of its own: an actor tagged say:<text> answers any of its other actions with it,
		// as a speech bubble -- the dead robots on the deck. Otherwise a note; gameplay hooks the event.
		FString Line;
		for (const FName& T : Target->Tags) { const FString S = T.ToString(); if (S.StartsWith(TEXT("say:"))) { Line = S.Mid(4); break; } }
		if (!Line.IsEmpty()) { ShowCallout(Target, Line, 3.5f, true); }
		else { ShowCallout(Target, FString::Printf(TEXT("%s: %s"), *Action, *Name), 2.5f, false); }
	}
	OnInspectAction.Broadcast(Target, Action);
}

void ABasePlayerController::ShowCallout(AActor* Anchor, const FString& Text, float Seconds, bool bSpeech)
{
	if (!Anchor || Text.IsEmpty()) { return; }
	if (!CalloutWidget) { CalloutWidget = CreateWidget<UCalloutWidget>(this, UCalloutWidget::StaticClass()); }
	CalloutWidget->SetCalloutText(Text, bSpeech);
	CalloutWidget->SetRenderOpacity(1.0f);
	if (!CalloutWidget->IsInViewport())
	{
		CalloutWidget->AddToViewport(60);
		CalloutWidget->SetAlignmentInViewport(FVector2D(0.5f, 1.0f));   // bottom-centre sits on the anchor point
	}
	CalloutAnchor = Anchor;
	CalloutEndTime = (GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.0f) + Seconds;   // REAL seconds: a page that pauses the game must not freeze a message on screen
	UpdateCallout();
}

void ABasePlayerController::UpdateCallout()
{
	if (!CalloutWidget || !CalloutWidget->IsInViewport()) { return; }
	AActor* Anchor = CalloutAnchor.Get();
	const float Now = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.0f;
	if (!Anchor || Now >= CalloutEndTime)
	{
		CalloutWidget->RemoveFromParent();
		CalloutAnchor = nullptr;
		return;
	}
	// Above the head for characters, above the bounds for anything else.
	FVector World;
	if (const ABaseCharacter* AsCharacter = Cast<ABaseCharacter>(Anchor))
	{
		const USkeletalMeshComponent* Mesh = AsCharacter->GetMesh();
		World = (Mesh && Mesh->DoesSocketExist(TEXT("head"))) ? Mesh->GetSocketLocation(TEXT("head")) + FVector(0, 0, 35.0f) : Anchor->GetActorLocation() + FVector(0, 0, 110.0f);
	}
	else
	{
		FVector Origin, Extent;
		Anchor->GetActorBounds(false, Origin, Extent);
		World = Origin + FVector(0, 0, Extent.Z + 15.0f);
	}
	FVector2D Screen;
	if (UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(this, World, Screen, false))
	{
		CalloutWidget->SetPositionInViewport(Screen, true);
	}
	CalloutWidget->SetRenderOpacity(FMath::Clamp((CalloutEndTime - Now) / CalloutFadeSeconds, 0.0f, 1.0f));
}

void ABasePlayerController::OnMenuKey()
{
	if (Screens.IsOpen(EScreen::Terminal)) { CloseTerminal(); return; }   // Esc leaves the terminal before it means the menu
	if (Screens.IsOpen(EScreen::PauseMenu)) { HidePauseMenu(); } else { ShowPauseMenu(); }
}

void ABasePlayerController::SitOnTagged(ABaseCharacter* Me, AActor* Seat)
{
	if (!Me || !Seat) { return; }
	// seat:<top height>,<facing yaw>,<lean>,<hunch> on the prop; a stool top at 66 facing +x by default.
	float Height = 66.0f, Yaw = Me->GetActorRotation().Yaw, Lean = 0.0f, Hunch = 0.0f;
	for (const FName& Tag : Seat->Tags)
	{
		const FString T = Tag.ToString();
		if (!T.StartsWith(TEXT("seat:"))) { continue; }
		TArray<FString> Parts; T.Mid(5).ParseIntoArray(Parts, TEXT(","));
		if (Parts.Num() > 0) { Height = FCString::Atof(*Parts[0]); }
		if (Parts.Num() > 1) { Yaw = FCString::Atof(*Parts[1]); }
		if (Parts.Num() > 2) { Lean = FCString::Atof(*Parts[2]); }
		if (Parts.Num() > 3) { Hunch = FCString::Atof(*Parts[3]); }
	}
	Me->BeginSit(Seat, Height, Yaw, Lean, Hunch);
}

// ---------------------------------------------------------------- terminals
static TAutoConsoleVariable<int32> CVarTerminalFlip(TEXT("RepliCan.TerminalFlip"), 0, TEXT("1 turns the in-world console screen to face the other way (if the text reads mirrored)"));
namespace
{
	// UI/Screens.json: a monitor mesh's screen as fractions of its local bounds.
	struct FScreenSpec { FString Front = TEXT("+y"); float Inset = 0.08f; float InsetRight = -1.0f; float Bottom = 0.16f; float Top = 0.06f; float Recess = 0.0f; };   // InsetRight < 0: same as Inset; Recess: cm the glass sits behind the front face
	bool ScreenSpecFor(const FString& MeshName, FScreenSpec& Out)
	{
		static TMap<FString, FScreenSpec> Table; static bool bLoaded = false;
		if (!bLoaded)
		{
			bLoaded = true;
			FString Json; TSharedPtr<FJsonObject> Root;
			if (FFileHelper::LoadFileToString(Json, *FPaths::Combine(JsonData::DataDir(), TEXT("UI"), TEXT("Screens.json"))))
			{
				TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
				if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
				{
					for (const auto& Pair : Root->Values)
					{
						const FString Key(Pair.Key.ToView());
						if (Key.StartsWith(TEXT("_"))) { continue; }
						const TSharedPtr<FJsonObject>* O = nullptr; if (!Pair.Value->TryGetObject(O) || !O) { continue; }
						FScreenSpec S; (*O)->TryGetStringField(TEXT("front"), S.Front);
						double N = 0.0;
						if ((*O)->TryGetNumberField(TEXT("inset"), N)) { S.Inset = (float)N; }
						if ((*O)->TryGetNumberField(TEXT("bottom"), N)) { S.Bottom = (float)N; }
						if ((*O)->TryGetNumberField(TEXT("top"), N)) { S.Top = (float)N; }
						if ((*O)->TryGetNumberField(TEXT("inset_right"), N)) { S.InsetRight = (float)N; }
						if ((*O)->TryGetNumberField(TEXT("recess"), N)) { S.Recess = (float)N; }
						Table.Add(Key, S);
					}
				}
			}
		}
		if (const FScreenSpec* S = Table.Find(MeshName)) { Out = *S; return true; }
		return false;
	}
}

namespace
{
	// UI/Terminals.json: a node is locked unless its entry says "locked": false. No entry at all
	// (the "default" banner) is a locked node too: only the cabins the story has given the player
	// credentials for open.
	bool TerminalLocked(const FString& Id)
	{
		FString Json; TSharedPtr<FJsonObject> Root;
		if (!FFileHelper::LoadFileToString(Json, *FPaths::Combine(JsonData::DataDir(), TEXT("UI"), TEXT("Terminals.json")))) { return true; }
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) { return true; }
		const TSharedPtr<FJsonObject>* Entry = nullptr;
		if (!Root->TryGetObjectField(Id, Entry) || !Entry) { return true; }
		bool bLocked = true; (*Entry)->TryGetBoolField(TEXT("locked"), bLocked);
		return bLocked;
	}
}

bool ABasePlayerController::IsTerminal(const AActor* Target) const
{
	FVector C, N, R, U; float W, H;
	return ScreenQuadFor(Target, C, N, R, U, W, H);
}

namespace
{
	// THE SCREEN'S OWN POLYGONS. A monitor prop's screen is a flat patch of the mesh facing the
	// spec's front axis, recessed behind the bezel. Read straight off the render data: every
	// triangle facing that way, joined into patches by shared vertices in the same plane, and
	// the patch chosen is the deepest one of any size that sits where the spec's box estimate
	// expects the screen. Cached per mesh in the mesh's local space.
	struct FScreenShape
	{
		bool bOk = false;
		TArray<FVector> Verts; TArray<int32> Tris;   // local, three vertices per triangle
		FVector Normal, Right;                        // local: the face, and the viewer's right
		float MinR = 0, MaxR = 0, MinU = 0, MaxU = 0, Depth = 0;
	};
	const FScreenShape& ShapeOf(const UStaticMesh* Mesh, const FScreenSpec& S)
	{
		static TMap<FString, FScreenShape> Cache;
		const FString Key = Mesh->GetPathName();
		if (const FScreenShape* Found = Cache.Find(Key)) { return *Found; }
		FScreenShape& Out = Cache.Add(Key);
		FVector LN(0, 1, 0);
		const FString F = S.Front.ToLower();
		if (F == TEXT("-y")) { LN = FVector(0, -1, 0); } else if (F == TEXT("+x")) { LN = FVector(1, 0, 0); } else if (F == TEXT("-x")) { LN = FVector(-1, 0, 0); }
		const FVector LR = FVector::CrossProduct(LN, FVector::UpVector).GetSafeNormal();   // the viewer faces -N; their right is N x up
		Out.Normal = LN; Out.Right = LR;
		const FStaticMeshRenderData* RD = Mesh->GetRenderData();
		if (!RD || RD->LODResources.Num() == 0) { return Out; }
		const FStaticMeshLODResources& LOD = RD->LODResources[0];
		const FPositionVertexBuffer& Pos = LOD.VertexBuffers.PositionVertexBuffer;
		const FStaticMeshVertexBuffer& SV = LOD.VertexBuffers.StaticMeshVertexBuffer;
		const FRawStaticIndexBuffer& IB = LOD.IndexBuffer;
		if (!Pos.GetAllowCPUAccess() || !SV.GetAllowCPUAccess() || !IB.GetAllowCPUAccess() || Pos.GetNumVertices() == 0 || IB.GetNumIndices() < 3)
		{
			UE_LOG(LogTemp, Warning, TEXT("Terminal: %s has no CPU-readable geometry (bAllowCPUAccess); the box estimate is used"), *Mesh->GetName());
			return Out;
		}
		// Where the spec's box estimate puts the screen, as a sanity bound on the patch chosen.
		const FBoxSphereBounds B = Mesh->GetBounds();
		const FVector Min = B.Origin - B.BoxExtent, Max = B.Origin + B.BoxExtent;
		const float Width = FMath::Abs(FVector::DotProduct(Max - Min, LR.GetAbs())), Height = Max.Z - Min.Z;
		const float InsetL = S.Inset, InsetR = S.InsetRight >= 0.0f ? S.InsetRight : S.Inset;
		const float EstU = Min.Z + Height * (S.Bottom + (1.0f - S.Bottom - S.Top) * 0.5f);
		const float EstR = FVector::DotProduct(B.Origin, LR) + Width * (InsetL - InsetR) * 0.5f * -1.0f;
		const float EstW = Width * (1.0f - InsetL - InsetR), EstH = Height * (1.0f - S.Bottom - S.Top);
		struct FTri { FVector P[3]; float Depth, Area; };
		TArray<FTri> Facing;
		const int32 NumTris = IB.GetNumIndices() / 3;
		for (int32 t = 0; t < NumTris; ++t)
		{
			const uint32 I[3] = { IB.GetIndex(t * 3), IB.GetIndex(t * 3 + 1), IB.GetIndex(t * 3 + 2) };
			if (I[0] >= Pos.GetNumVertices() || I[1] >= Pos.GetNumVertices() || I[2] >= Pos.GetNumVertices()) { continue; }
			FTri Tri;
			for (int32 k = 0; k < 3; ++k) { const FVector3f& P = Pos.VertexPosition(I[k]); Tri.P[k] = FVector(P.X, P.Y, P.Z); }
			const FVector Cross = FVector::CrossProduct(Tri.P[1] - Tri.P[0], Tri.P[2] - Tri.P[0]);
			Tri.Area = (float)Cross.Size() * 0.5f;
			if (Tri.Area < 0.05f) { continue; }
			if (FMath::Abs(FVector::DotProduct(Cross.GetSafeNormal(), LN)) < 0.9f) { continue; }   // not in the screen's plane
			FVector VN(0, 0, 0);
			for (int32 k = 0; k < 3; ++k) { const FVector4f Z = SV.VertexTangentZ(I[k]); VN += FVector(Z.X, Z.Y, Z.Z); }
			if (FVector::DotProduct(VN.GetSafeNormal(), LN) < 0.5f) { continue; }   // the back of something
			Tri.Depth = (float)FVector::DotProduct((Tri.P[0] + Tri.P[1] + Tri.P[2]) / 3.0, LN);
			Facing.Add(Tri);
		}
		if (Facing.Num() == 0) { return Out; }
		// Patches: triangles sharing a vertex position in the same plane.
		TArray<int32> Parent; Parent.SetNum(Facing.Num()); for (int32 i = 0; i < Parent.Num(); ++i) { Parent[i] = i; }
		auto Find = [&Parent](int32 a) { while (Parent[a] != a) { Parent[a] = Parent[Parent[a]]; a = Parent[a]; } return a; };
		TMap<FIntVector, TArray<int32>> AtKey;
		for (int32 i = 0; i < Facing.Num(); ++i) { for (int32 k = 0; k < 3; ++k) { const FVector& P = Facing[i].P[k]; AtKey.FindOrAdd(FIntVector(FMath::RoundToInt(P.X * 20.0), FMath::RoundToInt(P.Y * 20.0), FMath::RoundToInt(P.Z * 20.0))).Add(i); } }
		for (const auto& Pair : AtKey)
		{
			for (int32 j = 1; j < Pair.Value.Num(); ++j)
			{
				const int32 a = Pair.Value[0], b = Pair.Value[j];
				if (FMath::Abs(Facing[a].Depth - Facing[b].Depth) > 0.3f) { continue; }
				const int32 Ra = Find(a), Rb = Find(b); if (Ra != Rb) { Parent[Ra] = Rb; }
			}
		}
		struct FPatch { float Area = 0, Depth = 0, MinR = 1e9f, MaxR = -1e9f, MinU = 1e9f, MaxU = -1e9f; TArray<int32> Tris; };
		TMap<int32, FPatch> Patches;
		for (int32 i = 0; i < Facing.Num(); ++i)
		{
			FPatch& P = Patches.FindOrAdd(Find(i));
			P.Area += Facing[i].Area; P.Depth += Facing[i].Depth * Facing[i].Area; P.Tris.Add(i);
			for (int32 k = 0; k < 3; ++k) { const float R = (float)FVector::DotProduct(Facing[i].P[k], LR), U = (float)Facing[i].P[k].Z; P.MinR = FMath::Min(P.MinR, R); P.MaxR = FMath::Max(P.MaxR, R); P.MinU = FMath::Min(P.MinU, U); P.MaxU = FMath::Max(P.MaxU, U); }
		}
		float MaxArea = 0.0f; for (auto& Pair : Patches) { Pair.Value.Depth /= FMath::Max(Pair.Value.Area, KINDA_SMALL_NUMBER); MaxArea = FMath::Max(MaxArea, Pair.Value.Area); }
		const FPatch* Best = nullptr; const FPatch* Biggest = nullptr;
		for (const auto& Pair : Patches)
		{
			const FPatch& P = Pair.Value;
			if (!Biggest || P.Area > Biggest->Area) { Biggest = &P; }
			if (P.Area < MaxArea * 0.2f) { continue; }
			const float CR = (P.MinR + P.MaxR) * 0.5f, CU = (P.MinU + P.MaxU) * 0.5f;
			if (FMath::Abs(CR - EstR) > EstW * 0.75f || FMath::Abs(CU - EstU) > EstH * 0.75f) { continue; }   // nowhere near where the screen was said to be
			if (!Best || P.Depth < Best->Depth - 0.3f || (FMath::Abs(P.Depth - Best->Depth) <= 0.3f && P.Area > Best->Area)) { Best = &P; }
		}
		if (!Best) { Best = Biggest; }
		if (!Best) { return Out; }
		for (int32 i : Best->Tris) { for (int32 k = 0; k < 3; ++k) { Out.Tris.Add(Out.Verts.Num()); Out.Verts.Add(Facing[i].P[k]); } }
		Out.MinR = Best->MinR; Out.MaxR = Best->MaxR; Out.MinU = Best->MinU; Out.MaxU = Best->MaxU; Out.Depth = Best->Depth;
		Out.bOk = Out.Verts.Num() >= 3 && (Out.MaxR - Out.MinR) > 1.0f && (Out.MaxU - Out.MinU) > 1.0f;
		UE_LOG(LogTemp, Log, TEXT("Terminal: %s screen = %d of %d facing triangles in %d patches; %.1f x %.1f at depth %.1f (estimate %.1f x %.1f, %d patches total)"), *Mesh->GetName(), Best->Tris.Num(), Facing.Num(), Patches.Num(), Out.MaxR - Out.MinR, Out.MaxU - Out.MinU, Out.Depth, EstW, EstH, Patches.Num());
		return Out;
	}
}

bool ABasePlayerController::ScreenShapeFor(const AActor* Target, TArray<FVector>& OutVerts, TArray<int32>& OutTris, FVector& OutCentre, FVector& OutNormal, FVector& OutRight, FVector& OutUp, float& OutW, float& OutH) const
{
	const UStaticMeshComponent* C = Target ? Target->FindComponentByClass<UStaticMeshComponent>() : nullptr;
	const UStaticMesh* Mesh = C ? C->GetStaticMesh() : nullptr;
	if (!Mesh) { return false; }
	FScreenSpec S;
	if (!ScreenSpecFor(Mesh->GetName(), S)) { return false; }
	const FScreenShape& Shape = ShapeOf(Mesh, S);
	if (!Shape.bOk) { return false; }
	const FTransform& T = C->GetComponentTransform();
	OutVerts.Reset(Shape.Verts.Num()); OutTris = Shape.Tris;
	for (const FVector& V : Shape.Verts) { OutVerts.Add(T.TransformPosition(V + Shape.Normal * 0.15)); }   // a hair proud of the glass
	const FVector LocalCentre = Shape.Right * ((Shape.MinR + Shape.MaxR) * 0.5f) + FVector::UpVector * ((Shape.MinU + Shape.MaxU) * 0.5f) + Shape.Normal * Shape.Depth;
	OutCentre = T.TransformPosition(LocalCentre + Shape.Normal * 0.15);
	OutNormal = T.TransformVectorNoScale(Shape.Normal).GetSafeNormal();
	OutRight = T.TransformVectorNoScale(Shape.Right).GetSafeNormal();
	OutUp = T.TransformVectorNoScale(FVector::UpVector).GetSafeNormal();
	const float Scale = T.GetScale3D().GetAbsMax();
	OutW = (Shape.MaxR - Shape.MinR) * Scale; OutH = (Shape.MaxU - Shape.MinU) * Scale;
	return true;
}

bool ABasePlayerController::ScreenQuadFor(const AActor* Target, FVector& OutCentre, FVector& OutNormal, FVector& OutRight, FVector& OutUp, float& OutW, float& OutH) const
{
	const UStaticMeshComponent* C = Target ? Target->FindComponentByClass<UStaticMeshComponent>() : nullptr;
	const UStaticMesh* Mesh = C ? C->GetStaticMesh() : nullptr;
	if (!Mesh) { return false; }
	FScreenSpec S;
	if (!ScreenSpecFor(Mesh->GetName(), S)) { return false; }
	// The mesh's own screen polygons when they can be read; the box estimate below otherwise.
	{ TArray<FVector> V; TArray<int32> I; if (ScreenShapeFor(Target, V, I, OutCentre, OutNormal, OutRight, OutUp, OutW, OutH)) { return true; } }
	// The screen: the front face of the mesh's local box, less the stand at the bottom and the
	// bezel round it, taken into the world through the component's transform.
	const FBoxSphereBounds B = Mesh->GetBounds();
	const FVector Min = B.Origin - B.BoxExtent, Max = B.Origin + B.BoxExtent;
	FVector LocalNormal(0, 1, 0);
	const FString F = S.Front.ToLower();
	if (F == TEXT("-y")) { LocalNormal = FVector(0, -1, 0); }
	else if (F == TEXT("+x")) { LocalNormal = FVector(1, 0, 0); }
	else if (F == TEXT("-x")) { LocalNormal = FVector(-1, 0, 0); }
	// The VIEWER's right, standing in front of the face looking at it: up crossed with the normal.
	const FVector LocalRight = FVector::CrossProduct(FVector::UpVector, LocalNormal).GetSafeNormal();
	const float Width = FMath::Abs(FVector::DotProduct(Max - Min, LocalRight.GetAbs()));
	const float Height = Max.Z - Min.Z;
	FVector LocalCentre = B.Origin;
	if (F == TEXT("-y")) { LocalCentre.Y = Min.Y; } else if (F == TEXT("+x")) { LocalCentre.X = Max.X; } else if (F == TEXT("-x")) { LocalCentre.X = Min.X; } else { LocalCentre.Y = Max.Y; }
	LocalCentre.Z = Min.Z + Height * (S.Bottom + (1.0f - S.Bottom - S.Top) * 0.5f);
	// Insets can differ per side (a control strip down one edge): the centre shifts toward the wider bezel.
	const float InsetL = S.Inset, InsetR = S.InsetRight >= 0.0f ? S.InsetRight : S.Inset;
	LocalCentre += LocalRight * (Width * (InsetL - InsetR) * 0.5f);
	const FTransform& T = C->GetComponentTransform();
	OutCentre = T.TransformPosition(LocalCentre + LocalNormal * (0.5f - S.Recess));
	OutNormal = T.TransformVectorNoScale(LocalNormal).GetSafeNormal();
	OutRight = T.TransformVectorNoScale(LocalRight).GetSafeNormal();
	OutUp = FVector::CrossProduct(OutNormal, OutRight).GetSafeNormal();
	if (OutUp.Z < 0.0f) { OutUp *= -1.0f; }
	OutW = Width * (1.0f - InsetL - InsetR) * T.GetScale3D().GetAbsMax();
	OutH = Height * (1.0f - S.Bottom - S.Top) * T.GetScale3D().GetAbsMax();
	return OutW > 1.0f && OutH > 1.0f;
}

void ABasePlayerController::OpenTerminal(AActor* Target)
{
	if (Screens.IsOpen(EScreen::Terminal) || !Target) { return; }
	FVector Centre, Normal, Right, Up; float W, H;
	if (!ScreenQuadFor(Target, Centre, Normal, Right, Up, W, H)) { return; }
	// Which node this is: its terminal: tag, else its label. Only a node with credentials opens;
	// every other monitor says so, briefly, and stays a monitor.
	FString Id = Target->GetActorNameOrLabel();
	for (const FName& Tag : Target->Tags) { const FString T = Tag.ToString(); if (T.StartsWith(TEXT("terminal:"))) { Id = T.Mid(9); } }
	if (TerminalLocked(Id))
	{
		ShowCallout(Target, TEXT("TERMINAL LOCKED -- no credentials for this node."), 2.5f, false);
		UAmbientPlayer::PlayOneShot(this, GetWorld(), TEXT("switch_click.wav"), 0.5f, 0.6f);
		return;
	}
	HideInspectMenu();
	SetInspectHighlight(Target, false);
	if (InspectTarget.Get() == Target) { InspectTarget = nullptr; }
	TerminalTarget = Target;
	bTerminalSeated = false;
	if (ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn()))
	{
		Me->SetAiming(false);
		// Hands free for the keyboard: whatever is carried goes down and the weapon is put away
		// (it comes back on leaving). Using a thing in the world is not done over a gun.
		if (Carried.IsValid()) { DropCarried(); }
		if (!bHolstered && HeldSlot >= 0) { bTerminalHolstered = true; bHolstered = true; RefreshHeldWeapon(); }
		// The seat beside it, if there is one and the character is not already sitting; else square up.
		if (!Me->IsSitting())
		{
			AActor* Seat = nullptr; float Best = 200.0f;
			for (TActorIterator<AActor> It(GetWorld()); It; ++It)
			{
				bool bSeat = false; for (const FName& Tag : It->Tags) { if (Tag.ToString().StartsWith(TEXT("seat:"))) { bSeat = true; break; } }
				if (!bSeat) { continue; }
				const float D = FVector::Dist2D(It->GetActorLocation(), Centre);
				if (D < Best) { Best = D; Seat = *It; }
			}
			if (Seat) { SitOnTagged(Me, Seat); bTerminalSeated = true; }
			else { Me->SetActorRotation(FRotator(0.0f, (Centre - Me->GetActorLocation()).Rotation().Yaw, 0.0f)); }
		}
	}
	// THE SCREEN IN THE WORLD: the console widget drawn on a world-space widget component the size
	// of the screen quad, a hair off its face. It is the monitor's picture, from any camera.
	if (!TerminalWidget)
	{
		TerminalWidget = CreateWidget<UTerminalWidget>(this, UTerminalWidget::StaticClass());
		TerminalWidget->OnExit.BindUObject(this, &ABasePlayerController::CloseTerminal);
		TerminalWidget->OnNeedFocus.BindUObject(this, &ABasePlayerController::RefocusTerminalPrompt);
	}
	TerminalWidget->Open(this, Id);
	if (TerminalWidget->IsInViewport()) { TerminalWidget->RemoveFromParent(); }
	const float DrawW = 1280.0f, DrawH = FMath::RoundToFloat(1280.0f * H / FMath::Max(1.0f, W));
	TerminalScreen = NewObject<UWidgetComponent>(Target, TEXT("TerminalScreen"));
	TerminalScreen->SetWidgetSpace(EWidgetSpace::World);
	TerminalScreen->SetDrawSize(FVector2D(DrawW, DrawH));
	TerminalScreen->SetBlendMode(EWidgetBlendMode::Transparent);
	TerminalScreen->SetTwoSided(false);
	TerminalScreen->SetCollisionProfileName(TEXT("UI"));
	TerminalScreen->SetTickWhenOffscreen(true);   // it is never on screen itself: the glass mesh wears its picture
	TerminalScreen->SetWidget(TerminalWidget);
	static const auto* FlipVar = IConsoleManager::Get().FindConsoleVariable(TEXT("RepliCan.TerminalFlip"));
	const bool bFlip = FlipVar && FlipVar->GetInt() != 0;
	const FVector Face = bFlip ? -Normal : Normal;
	// The quad is the pointer's hit surface and the picture's renderer, not a thing that is seen:
	// the glass below shows the picture in the screen's own outline.
	TerminalScreen->SetWorldTransform(FTransform(FRotationMatrix::MakeFromXZ(Face, Up).ToQuat(), Centre + Normal * 0.6f, FVector(1.0f, W / DrawW, W / DrawW)));
	TerminalScreen->AttachToComponent(Target->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
	TerminalScreen->RegisterComponent();
	TerminalScreen->SetRenderInMainPass(false);
	TerminalScreen->SetTintColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.0f));
	// THE GLASS: the screen's own polygons, a hair proud of the monitor, wearing the widget's
	// picture once it has drawn (BindTerminalGlass). Whatever shape the modeller gave the screen,
	// the console has it: chamfers, notches, the lot.
	{
		TArray<FVector> Verts; TArray<int32> Tris; FVector SC, SN, SR, SU; float SW = 0.0f, SH = 0.0f;
		if (ScreenShapeFor(Target, Verts, Tris, SC, SN, SR, SU, SW, SH) && Verts.Num() >= 3)
		{
			TArray<FVector> Normals; TArray<FVector2D> UVs; TArray<FLinearColor> Colours; TArray<FProcMeshTangent> Tangents;
			Normals.Init(SN, Verts.Num()); Colours.Init(FLinearColor::White, Verts.Num()); Tangents.Init(FProcMeshTangent(SR, false), Verts.Num());
			const FVector Corner = SC - SR * (SW * 0.5f) + SU * (SH * 0.5f);   // the viewer's top left, where the picture starts
			for (const FVector& V : Verts)
			{
				const FVector D = V - Corner;
				float U = (float)FVector::DotProduct(D, SR) / SW; if (bFlip) { U = 1.0f - U; }
				UVs.Add(FVector2D(U, -(float)FVector::DotProduct(D, SU) / SH));
			}
			TerminalGlass = NewObject<UProceduralMeshComponent>(Target, TEXT("TerminalGlass"));
			TerminalGlass->SetWorldTransform(FTransform::Identity);
			TerminalGlass->AttachToComponent(Target->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
			TerminalGlass->RegisterComponent();
			TerminalGlass->CreateMeshSection_LinearColor(0, Verts, Tris, Normals, UVs, Colours, Tangents, false);
			TerminalGlass->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // the pointer's trace goes through to the quad behind
			TerminalGlass->SetCastShadow(false);
			TerminalGlass->SetVisibility(false);   // until the picture is bound
		}
		else { UE_LOG(LogTemp, Warning, TEXT("Terminal: no screen shape for %s; the picture stays on the quad"), *Target->GetActorNameOrLabel()); TerminalScreen->SetRenderInMainPass(true); TerminalScreen->SetTintColorAndOpacity(FLinearColor::White); }
	}
	GetWorld()->GetTimerManager().SetTimer(TerminalBindTimer, this, &ABasePlayerController::BindTerminalGlass, 0.05f, true);
	// THE POINTER: the mouse traced from the camera onto the screen, and the keys sent through it,
	// as a virtual Slate user of its own.
	if (APawn* MyPawn = GetPawn())
	{
		TerminalPointer = NewObject<UWidgetInteractionComponent>(MyPawn, TEXT("TerminalPointer"));
		TerminalPointer->InteractionSource = EWidgetInteractionSource::Mouse;
		TerminalPointer->VirtualUserIndex = 1;
		TerminalPointer->PointerIndex = 0;
		TerminalPointer->InteractionDistance = 800.0f;
		TerminalPointer->AttachToComponent(MyPawn->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		TerminalPointer->RegisterComponent();
	}
	Screens.Open(EScreen::Terminal);
	ApplyInputMode();
	// The camera goes to the seated eye once the sit has settled; the prompt takes the pointer's focus then.
	GetWorld()->GetTimerManager().SetTimer(TerminalPlaceTimer, this, &ABasePlayerController::PlaceTerminalCamera, 0.45f, false);
}

void ABasePlayerController::PlaceTerminalCamera()
{
	if (!Screens.IsOpen(EScreen::Terminal) || !TerminalTarget.IsValid()) { return; }
	FVector Centre, Normal, Right, Up; float W, H;
	if (!ScreenQuadFor(TerminalTarget.Get(), Centre, Normal, Right, Up, W, H)) { return; }
	// From the character's own eye, a hand's width in front of the face so the head is behind the
	// lens and the shoulders below the frame; the field of view fitted so the screen fills most of
	// the height. No character: from the screen's normal instead.
	FVector Eye = Centre + Normal * 45.0f;
	if (const ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn()))
	{
		const USkeletalMeshComponent* M = Me->GetMesh();
		const FVector Head = (M && M->DoesSocketExist(TEXT("head"))) ? M->GetSocketLocation(TEXT("head")) : Me->GetActorLocation() + FVector(0, 0, 60.0f);
		// Along the line from the head to the screen, a screen-and-a-quarter out from the glass,
		// and always a hand's width in front of the head so the head is behind the lens.
		const FVector Toward = (Head - Centre).GetSafeNormal();
		const float HeadDist = (float)FVector::Dist(Head, Centre);
		Eye = Centre + Toward * FMath::Min(FMath::Clamp(H * 1.25f, 20.0f, 48.0f), FMath::Max(12.0f, HeadDist - 12.0f));
	}
	const float Dist = FMath::Max(12.0f, (float)FVector::Dist(Eye, Centre));
	const FVector2D ViewSize = UWidgetLayoutLibrary::GetViewportSize(this);
	const float Aspect = ViewSize.Y > 1.0f ? ViewSize.X / ViewSize.Y : 1.777f;
	const float VNeeded = 2.0f * FMath::Atan((H * 0.5f) / Dist) / 0.9f;   // the screen fills nine tenths of the height
	const float Fov = FMath::Clamp(FMath::RadiansToDegrees(2.0f * FMath::Atan(FMath::Tan(VNeeded * 0.5f) * Aspect)), 30.0f, 90.0f);
	FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACameraActor* Previous = TerminalCamera;
	TerminalCamera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), Eye, (Centre - Eye).Rotation(), Params);
	if (TerminalCamera && TerminalCamera->GetCameraComponent()) { TerminalCamera->GetCameraComponent()->SetFieldOfView(Fov); }
	if (TerminalCamera) { SetViewTargetWithBlend(TerminalCamera, 0.35f, VTBlend_EaseInOut, 2.0f); }
	if (Previous) { Previous->SetLifeSpan(1.5f); }
	// THE KEYBOARD. The real keyboard's user is focused on the prompt (Slate finds the widget in
	// the screen's virtual window), so every key and character goes straight there with its
	// proper case and symbols. Should that fail, the pointer's own user is given the focus by a
	// click on the prompt and the keys are carried through the pointer instead.
	bTerminalKeysDirect = TerminalWidget && TerminalWidget->FocusPromptFor(0);
	UE_LOG(LogTemp, Log, TEXT("Terminal: keys %s"), bTerminalKeysDirect ? TEXT("direct to the prompt") : TEXT("carried through the pointer"));
	if (!bTerminalKeysDirect) { ClickTerminalPrompt(); }
}

void ABasePlayerController::ClickTerminalPrompt()
{
	if (!TerminalPointer || !TerminalScreen || !TerminalWidget) { return; }
	FVector2D Px;
	if (!TerminalWidget->PromptCentrePx(Px)) { return; }
	// The prompt's pixel on the picture, taken back through the component the way its hit test
	// maps a world point to a pixel (GetLocalHitLocation: x = -Y, y = -Z, less the pivot), so
	// the click lands on the prompt wherever the layout put it.
	const FVector2D Draw = TerminalScreen->GetDrawSize(), Pivot = TerminalScreen->GetPivot();
	const FVector Local(0.0, -(Px.X - Draw.X * Pivot.X), -(Px.Y - Draw.Y * Pivot.Y));
	const FTransform& T = TerminalScreen->GetComponentTransform();
	const FVector World = T.TransformPosition(Local), Face = T.GetUnitAxis(EAxis::X);
	FHitResult Click;
	Click.bBlockingHit = true;
	Click.Component = TerminalScreen;
	Click.HitObjectHandle = FActorInstanceHandle(TerminalScreen->GetOwner());
	Click.Location = Click.ImpactPoint = World;
	Click.ImpactNormal = Click.Normal = Face;
	Click.TraceStart = World + Face * 30.0f; Click.TraceEnd = World - Face * 5.0f;
	TerminalPointer->InteractionSource = EWidgetInteractionSource::Custom;
	TerminalPointer->SetCustomHitResult(Click);
	TWeakObjectPtr<UWidgetInteractionComponent> WeakPointer(TerminalPointer);
	FTimerHandle Press, Release, Back;
	GetWorld()->GetTimerManager().SetTimer(Press, FTimerDelegate::CreateLambda([WeakPointer]() { if (WeakPointer.IsValid()) { WeakPointer->PressPointerKey(EKeys::LeftMouseButton); } }), 0.1f, false);
	GetWorld()->GetTimerManager().SetTimer(Release, FTimerDelegate::CreateLambda([WeakPointer]() { if (WeakPointer.IsValid()) { WeakPointer->ReleasePointerKey(EKeys::LeftMouseButton); } }), 0.2f, false);
	GetWorld()->GetTimerManager().SetTimer(Back, FTimerDelegate::CreateLambda([WeakPointer]() { if (WeakPointer.IsValid()) { WeakPointer->InteractionSource = EWidgetInteractionSource::Mouse; } }), 0.3f, false);
}

void ABasePlayerController::RefocusTerminalPrompt()
{
	if (!Screens.IsOpen(EScreen::Terminal) || !TerminalWidget) { return; }
	if (bTerminalKeysDirect) { TerminalWidget->FocusPromptFor(0); } else { ClickTerminalPrompt(); }
}

void ABasePlayerController::BindTerminalGlass()
{
	if (!Screens.IsOpen(EScreen::Terminal) || !TerminalScreen) { GetWorld()->GetTimerManager().ClearTimer(TerminalBindTimer); return; }
	UTextureRenderTarget2D* RT = TerminalScreen->GetRenderTarget();
	if (!RT) { return; }   // not drawn yet; the timer asks again
	GetWorld()->GetTimerManager().ClearTimer(TerminalBindTimer);
	if (!TerminalGlass) { return; }
	// The engine's own pass-through material for world widgets, on the glass, fed the same picture.
	static UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Translucent.Widget3DPassThrough_Translucent"));
	if (!Parent) { UE_LOG(LogTemp, Warning, TEXT("Terminal: no Widget3DPassThrough_Translucent; the glass stays dark")); TerminalScreen->SetRenderInMainPass(true); TerminalScreen->SetTintColorAndOpacity(FLinearColor::White); return; }
	TerminalGlassMID = UMaterialInstanceDynamic::Create(Parent, this);
	TerminalGlassMID->SetTextureParameterValue(TEXT("SlateUI"), RT);
	TerminalGlassMID->SetVectorParameterValue(TEXT("TintColorAndOpacity"), FLinearColor::White);
	TerminalGlassMID->SetScalarParameterValue(TEXT("OpacityFromTexture"), 1.0f);
	TerminalGlass->SetMaterial(0, TerminalGlassMID);
	TerminalGlass->SetVisibility(true);
}

void ABasePlayerController::CloseTerminal()
{
	if (!Screens.IsOpen(EScreen::Terminal)) { return; }
	Screens.Close(EScreen::Terminal);
	GetWorld()->GetTimerManager().ClearTimer(TerminalPlaceTimer);
	GetWorld()->GetTimerManager().ClearTimer(TerminalBindTimer);
	bTerminalKeysDirect = false;
	if (TerminalWidget && TerminalWidget->IsInViewport()) { TerminalWidget->RemoveFromParent(); }
	if (TerminalGlass) { TerminalGlass->DestroyComponent(); TerminalGlass = nullptr; }
	TerminalGlassMID = nullptr;
	if (TerminalScreen) { TerminalScreen->SetWidget(nullptr); TerminalScreen->DestroyComponent(); TerminalScreen = nullptr; }
	if (TerminalPointer) { TerminalPointer->DestroyComponent(); TerminalPointer = nullptr; }
	if (ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn())) { if (bTerminalSeated && Me->IsSitting()) { Me->StandUp(); } }
	bTerminalSeated = false;
	if (bTerminalHolstered) { bTerminalHolstered = false; bHolstered = false; RefreshHeldWeapon(); }   // the weapon back in hand
	if (APawn* P = GetPawn()) { SetViewTargetWithBlend(P, 0.35f, VTBlend_EaseInOut, 2.0f); }
	if (TerminalCamera) { TerminalCamera->SetLifeSpan(1.5f); TerminalCamera = nullptr; }
	TerminalTarget = nullptr;
	ApplyInputMode();
}

void ABasePlayerController::ShowPauseMenu()
{
	LowerWeaponForScreen();
	if (Screens.IsOpen(EScreen::PauseMenu)) { return; }
	if (!PauseMenuWidget)
	{
		PauseMenuWidget = CreateWidget<UPauseMenuWidget>(this, UPauseMenuWidget::StaticClass());
		PauseMenuWidget->SetOwnerController(this);
	}
	if (!PauseMenuWidget->IsInViewport())
	{
		PauseMenuWidget->AddToViewport(100);   // above the edit pages
		PauseMenuWidget->SetAnchorsInViewport(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		PauseMenuWidget->SetAlignmentInViewport(FVector2D::ZeroVector);
	}
	Screens.Open(EScreen::PauseMenu);
	SetPause(true);
	ApplyInputMode();
}

void ABasePlayerController::ShowReferenceFor(const FString& ItemName)
{
	ShowConsolePage(1);
	if (ReferenceWidget && !ReferenceWidget->OpenEntryByName(ItemName)) { SetDiagNoteTimed(FString::Printf(TEXT("No card for %s"), *ItemName), 3.0f); }
}

void ABasePlayerController::ShowReference()
{
	if (Screens.IsOpen(EScreen::Reference)) { return; }
	// The menu steps aside (still paused); the catalogue takes the sheet's panel inset.
	if (PauseMenuWidget && PauseMenuWidget->IsInViewport()) { PauseMenuWidget->RemoveFromParent(); }
	if (!ReferenceWidget)
	{
		ReferenceWidget = CreateWidget<UReferenceWidget>(this, UReferenceWidget::StaticClass());
		ReferenceWidget->SetOwnerController(this);
		ReferenceWidget->OnClose.BindUObject(this, &ABasePlayerController::HideReference);
	}
	ReferenceWidget->Rebuild();
	PlaceConsolePage(ReferenceWidget, ConsolePageZ);
	Screens.Open(EScreen::Reference);
	SetPause(true);
	ApplyInputMode();
}

void ABasePlayerController::ShowScenesPanel()
{
	if (Screens.IsOpen(EScreen::Scenes)) { return; }
	if (PauseMenuWidget && PauseMenuWidget->IsInViewport()) { PauseMenuWidget->RemoveFromParent(); }
	if (!ScenesWidget)
	{
		ScenesWidget = CreateWidget<UScenesWidget>(this, UScenesWidget::StaticClass());
		ScenesWidget->Setup(this);
		ScenesWidget->OnClose.BindUObject(this, &ABasePlayerController::HideScenesPanel);
	}
	ScenesWidget->Rebuild();
	PlaceConsolePage(ScenesWidget, ++ConsolePageZ);
	Screens.Open(EScreen::Scenes);
	SetPause(true);
	ApplyInputMode();
}

void ABasePlayerController::HideScenesPanel()
{
	if (!Screens.IsOpen(EScreen::Scenes)) { return; }
	Screens.Close(EScreen::Scenes);
	HideSheetMirror();   // the viewer's booth goes with the page
	if (ScenesWidget && ScenesWidget->IsInViewport()) { ScenesWidget->RemoveFromParent(); }
	// Straight back to the menu it came from -- unless a scene is starting, in which case
	// PlaySequence takes over and closing the menu is exactly what it wants.
	if (Screens.IsOpen(EScreen::PauseMenu) && PauseMenuWidget && !PauseMenuWidget->IsInViewport())
	{
		PauseMenuWidget->AddToViewport(100);
		PauseMenuWidget->SetAnchorsInViewport(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		PauseMenuWidget->SetAlignmentInViewport(FVector2D::ZeroVector);
	}
	if (!IsPageOpen()) { SetPause(false); }   // see HideReference: a page that pauses must unpause
	ApplyInputMode();
}

void ABasePlayerController::ShowSettingsPanel()
{
	if (Screens.IsOpen(EScreen::Settings)) { return; }
	// The menu steps aside while the page is up, the same way Reference does.
	if (PauseMenuWidget && PauseMenuWidget->IsInViewport()) { PauseMenuWidget->RemoveFromParent(); }
	if (!SettingsWidget)
	{
		SettingsWidget = CreateWidget<USettingsWidget>(this, USettingsWidget::StaticClass());
		SettingsWidget->OnClose.BindUObject(this, &ABasePlayerController::HideSettingsPanel);
	}
	SettingsWidget->SetShowBack(true);
	SettingsWidget->Rebuild(/*bTitleContext=*/false);
	PlaceConsolePage(SettingsWidget, ++ConsolePageZ);
	Screens.Open(EScreen::Settings);
	SetPause(true);
	ApplyInputMode();
}

void ABasePlayerController::HideSettingsPanel()
{
	if (!Screens.IsOpen(EScreen::Settings)) { return; }
	Screens.Close(EScreen::Settings);
	if (SettingsWidget && SettingsWidget->IsInViewport()) { SettingsWidget->RemoveFromParent(); }
	// Back to the menu it came from.
	if (Screens.IsOpen(EScreen::PauseMenu) && PauseMenuWidget && !PauseMenuWidget->IsInViewport())
	{
		PauseMenuWidget->AddToViewport(100);
		PauseMenuWidget->SetAnchorsInViewport(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		PauseMenuWidget->SetAlignmentInViewport(FVector2D::ZeroVector);
	}
	if (!IsPageOpen()) { SetPause(false); }   // see HideReference: a page that pauses must unpause
	ApplyInputMode();
}

void ABasePlayerController::RetirePage(UUserWidget* Old)
{
	if (RetiredPage && RetiredPage != Old && RetiredPage->IsInViewport()) { RetiredPage->RemoveFromParent(); }
	RetiredPage = Old;
}

void ABasePlayerController::PageSettled(const UUserWidget* Incoming)
{
	if (!RetiredPage || RetiredPage == Incoming) { return; }
	if (RetiredPage->IsInViewport()) { RetiredPage->RemoveFromParent(); }
	RetiredPage = nullptr;
}

void ABasePlayerController::ShowConsolePage(int32 Tab)
{
	if (Screens.IsOpen(EScreen::PauseMenu)) { HidePauseMenu(); }
	// Pulling the outgoing page out first leaves a frame with no panel at all, and the incoming
	// page paints once before its ASCII rules have measured themselves against their own text and
	// before the booth capture has written a frame into the render target. Both read as the panel
	// tearing as the tab changes. So the page on screen is held there -- bRetainPageWidget stops
	// the Hide paths removing its widget -- while the next one is built and stays invisible; the
	// held page is dropped only when the new one reports it has settled.
	UUserWidget* Outgoing = nullptr;
	bRetainPageWidget = true;
	if (Screens.IsOpen(EScreen::Reference)) { Screens.Close(EScreen::Reference); Outgoing = ReferenceWidget; }
	if (Screens.IsOpen(EScreen::CharacterSheet)) { Outgoing = CharacterSheetWidget; HideCharacterSheet(); }
	if (Screens.IsOpen(EScreen::Appearance)) { Outgoing = AppearanceWidget; FinishAppearance(); }
	bRetainPageWidget = false;
	RetirePage(Outgoing);
	++ConsolePageZ;
	switch (Tab)
	{
	case 0: ShowCharacterSheet(); break;
	case 1: ShowReference(); break;
	case 2: BeginAppearance(); break;
	default: break;
	}
}

// A booth capture exposes for its own lighting: the level's auto exposure is adapted to a
// dark facility, and letting it through washes the picture out.
static TAutoConsoleVariable<float> CVarPreviewExposure(
	TEXT("RepliCan.PreviewExposure"), 8.5f,
	TEXT("EV compensation for the booth captures (character sheet, appearance, weapon preview). Reopen the screen to apply."));

static void LockCaptureExposure(USceneCaptureComponent2D* Cap)
{
	if (!Cap) { return; }
	FPostProcessSettings& PP = Cap->PostProcessSettings;
	PP.bOverride_AutoExposureMethod = true;
	PP.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
	PP.bOverride_AutoExposureBias = true;
	PP.AutoExposureBias = CVarPreviewExposure.GetValueOnGameThread();   // manual exposure is ISO based: this sets the working level
	PP.bOverride_AutoExposureMinBrightness = true;
	PP.bOverride_AutoExposureMaxBrightness = true;
	PP.AutoExposureMinBrightness = 1.0f;
	PP.AutoExposureMaxBrightness = 1.0f;
	PP.bOverride_BloomIntensity = true;
	PP.BloomIntensity = 0.03f;                   // a whisper, so bright edges do not smear

	// A booth capture runs the whole post chain, so anything not overridden here arrives at
	// the engine's DEFAULTS -- and those include motion blur at 0.5 and a vignette at 0.4.
	// On a preview that spins under the mouse that reads as the item being blurred and the
	// corners being dirty. There is no post-process volume in this level to have set them,
	// which is exactly why they had to be turned off at the capture instead.
	PP.bOverride_MotionBlurAmount = true;        PP.MotionBlurAmount = 0.0f;
	PP.bOverride_MotionBlurMax = true;           PP.MotionBlurMax = 0.0f;
	PP.bOverride_VignetteIntensity = true;       PP.VignetteIntensity = 0.0f;
	PP.bOverride_SceneFringeIntensity = true;    PP.SceneFringeIntensity = 0.0f;
	PP.bOverride_FilmGrainIntensity = true;      PP.FilmGrainIntensity = 0.0f;
	PP.bOverride_LensFlareIntensity = true;      PP.LensFlareIntensity = 0.0f;
	// No depth of field: a focal distance of zero is how the engine spells "off".
	PP.bOverride_DepthOfFieldFocalDistance = true;  PP.DepthOfFieldFocalDistance = 0.0f;
	PP.bOverride_DepthOfFieldDepthBlurAmount = true; PP.DepthOfFieldDepthBlurAmount = 0.0f;
}

static const FVector WeaponBoothOrigin(60000.0f, -60000.0f, 0.0f);   // well away from the level and the character booth
static const float WeaponKickTime = 0.26f;

void ABasePlayerController::ShowWeaponPreview(const FString& MeshPath, const FString& WeaponName)
{
	HideWeaponPreview();
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
	if (!Mesh || !GetWorld()) { return; }
	FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	WeaponBoothActor = GetWorld()->SpawnActor<AStaticMeshActor>(WeaponBoothOrigin, FRotator::ZeroRotator, Params);
	if (!WeaponBoothActor) { return; }
	WeaponBoothActor->SetMobility(EComponentMobility::Movable);
	WeaponBoothActor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
	// The booth wears the paint being TRIED, when one is; otherwise the catalogue's own.
	if (const WeaponCatalog::FWeapon* Skinned = WeaponName.IsEmpty() ? nullptr : WeaponCatalog::Find(WeaponName))
	{
		WeaponSkins::ApplyNamed(WeaponBoothActor->GetStaticMeshComponent(), *Skinned,
			PreviewSkinOverride.IsEmpty() ? Skinned->Skin : PreviewSkinOverride);
	}
	WeaponBoothActor->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponBoothActor->SetTickableWhenPaused(true);
	// The same rim-glow shell the inspect highlight draws over a character (M_CharacterHighlight:
	// unlit, fresnel opacity) goes over the piece here, in the terminal's green, so every edge
	// reads against the black backdrop.
	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Characters/M_CharacterHighlight.M_CharacterHighlight")))
	{
		if (UMaterialInstanceDynamic* Rim = UMaterialInstanceDynamic::Create(Base, WeaponBoothActor))
		{
			Rim->SetVectorParameterValue(TEXT("HighlightColor"), FLinearColor(0.32f, 1.0f, 0.45f));
			Rim->SetScalarParameterValue(TEXT("Intensity"), 0.9f);
			WeaponBoothActor->GetStaticMeshComponent()->SetOverlayMaterial(Rim);
		}
	}
	const FBox Bounds = Mesh->GetBoundingBox();
	WeaponExtent = FMath::Max(40.0f, Bounds.GetSize().GetMax());
	ResetWeaponPreviewView();
	// The icon rig's three lights, scaled to the piece.
	const float L = WeaponExtent / 100.0f;
	const FVector LightPos[3] = { FVector(-160, -120, 120), FVector(-140, 140, 60), FVector(-60, 0, -120) };
	const float LightCd[3] = { 120.0f, 60.0f, 30.0f };
	for (int32 i = 0; i < 3; ++i)
	{
		APointLight* Light = GetWorld()->SpawnActor<APointLight>(WeaponBoothOrigin + LightPos[i] * L, FRotator::ZeroRotator, Params);
		if (!Light) { continue; }
		Light->SetMobility(EComponentMobility::Movable);
		Light->PointLightComponent->SetIntensityUnits(ELightUnits::Candelas);
		Light->PointLightComponent->SetIntensity(LightCd[i]);
		Light->PointLightComponent->SetAttenuationRadius(600.0f * L);
		Light->SetTickableWhenPaused(true);
		WeaponLights.Add(Light);
	}
	// The muzzle flash: off until FIRE.
	WeaponFlash = GetWorld()->SpawnActor<APointLight>(WeaponBoothOrigin, FRotator::ZeroRotator, Params);
	if (WeaponFlash)
	{
		WeaponFlash->SetMobility(EComponentMobility::Movable);
		WeaponFlash->PointLightComponent->SetIntensityUnits(ELightUnits::Candelas);
		WeaponFlash->PointLightComponent->SetIntensity(300.0f);
		WeaponFlash->PointLightComponent->SetAttenuationRadius(300.0f * L);
		WeaponFlash->PointLightComponent->SetLightColor(FLinearColor(1.0f, 0.7f, 0.3f));
		WeaponFlash->PointLightComponent->SetVisibility(false);
		WeaponFlash->SetTickableWhenPaused(true);
	}
	// The camera: side on, a little above, looking +X at the piece.
	if (!WeaponTarget) { WeaponTarget = UKismetRenderingLibrary::CreateRenderTarget2D(this, 1024, PaneShape::TargetHeight(PaneShape::WeaponPreview, 1024), ETextureRenderTargetFormat::RTF_RGBA8_SRGB, FLinearColor::Black, false); }
	WeaponCapture = GetWorld()->SpawnActor<ASceneCapture2D>(WeaponBoothOrigin + FVector(-2.6f * WeaponExtent, 0.0f, 0.35f * WeaponExtent), FRotator(-7.5f, 0.0f, 0.0f), Params);
	if (USceneCaptureComponent2D* Cap = WeaponCapture ? WeaponCapture->GetCaptureComponent2D() : nullptr)
	{
		Cap->TextureTarget = WeaponTarget;
		Cap->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
		Cap->FOVAngle = 34.0f;   // horizontal, on a landscape target
		Cap->bCaptureEveryFrame = true;
		Cap->bAlwaysPersistRenderingState = true;
		LockCaptureExposure(Cap);
		Cap->SetTickableWhenPaused(true);
		WeaponCapture->SetTickableWhenPaused(true);
	}
	PlaceWeaponPreview();
	FrameWeaponPreview();   // the default view is the one that fits this piece
}

void ABasePlayerController::HideWeaponPreview()
{
	if (WeaponBoothActor) { WeaponBoothActor->Destroy(); WeaponBoothActor = nullptr; }
	if (WeaponCapture) { WeaponCapture->Destroy(); WeaponCapture = nullptr; }
	if (WeaponFlash) { WeaponFlash->Destroy(); WeaponFlash = nullptr; }
	for (APointLight* Light : WeaponLights) { if (Light) { Light->Destroy(); } }
	WeaponLights.Reset();
}

// Turns the piece about the centre of its bounds (not its grip pivot) and applies the recoil kick.
bool ABasePlayerController::GetWeaponPreviewFrame(FTransform& OutCamera, float& OutFovDeg, FTransform& OutPiece) const
{
	const USceneCaptureComponent2D* Cap = WeaponCapture ? WeaponCapture->GetCaptureComponent2D() : nullptr;
	if (!WeaponBoothActor || !Cap) { return false; }
	OutCamera = Cap->GetComponentTransform();
	OutFovDeg = Cap->FOVAngle;
	OutPiece = WeaponBoothActor->GetActorTransform();
	return true;
}

void ABasePlayerController::PlaceWeaponPreview()
{
	if (!WeaponBoothActor) { return; }
	const float Phase0 = WeaponKick > 0.0f ? FMath::Sin(PI * (1.0f - WeaponKick / WeaponKickTime)) : 0.0f;
	const FRotator Rot(WeaponPitch - 9.0f * Phase0, WeaponYaw, 0.0f);   // muzzle rise while it kicks
	WeaponBoothActor->SetActorLocationAndRotation(WeaponBoothOrigin, Rot);
	FVector Origin, Extent;
	WeaponBoothActor->GetActorBounds(false, Origin, Extent);
	// HAC1 pieces run down their local +X: the muzzle is +X. The kick is back along that, then home.
	const FVector Muzzle = Rot.RotateVector(FVector(1.0f, 0.0f, 0.0f));
	WeaponBoothActor->SetActorLocation(WeaponBoothOrigin + (WeaponBoothOrigin - Origin) - Muzzle * (0.16f * WeaponExtent * Phase0));
	if (WeaponFlash)
	{
		WeaponFlash->SetActorLocation(WeaponBoothOrigin + Muzzle * (0.55f * WeaponExtent));
		WeaponFlash->PointLightComponent->SetVisibility(WeaponKick > WeaponKickTime - 0.05f);
	}
}

void ABasePlayerController::OrbitWeaponPreview(float DeltaYaw, float DeltaPitch)
{
	WeaponYaw += DeltaYaw;
	WeaponPitch = FMath::Clamp(WeaponPitch + DeltaPitch, -80.0f, 80.0f);
	PlaceWeaponPreview();
}

// Pulls the camera back to exactly the distance that fits the piece as it is currently turned,
// rather than to a fixed multiple of its longest dimension. A dagger and a launcher have wildly
// different shapes, and framing both off one number left the small ones lost in the middle of
// the pane and the long ones running out of it.
void ABasePlayerController::FrameWeaponPreview()
{
	if (!WeaponBoothActor || !WeaponCapture) { return; }
	USceneCaptureComponent2D* Cap = WeaponCapture->GetCaptureComponent2D();
	if (!Cap) { return; }
	FVector Origin, Extent;
	WeaponBoothActor->GetActorBounds(false, Origin, Extent);

	// The capture looks down its own forward axis, so the piece's extent across the screen is
	// its Y, its height is its Z, and its depth along the view is its X. The FOV is horizontal
	// on a landscape target, so the vertical half angle is the narrower of the two and usually
	// the one that decides the distance.
	const float Aspect = (WeaponTarget && WeaponTarget->SizeY > 0) ? float(WeaponTarget->SizeX) / float(WeaponTarget->SizeY) : 1.618f;
	const float TanH = FMath::Tan(FMath::DegreesToRadians(FMath::Max(1.0f, Cap->FOVAngle) * 0.5f));
	const float TanV = TanH / FMath::Max(0.01f, Aspect);
	const float ForWidth = Extent.Y / FMath::Max(0.01f, TanH);
	const float ForHeight = Extent.Z / FMath::Max(0.01f, TanV);
	// Plus the piece's own half depth, so the near end is not pushed through the lens.
	const float Distance = FMath::Max(10.0f, (FMath::Max(ForWidth, ForHeight) * WeaponFrameMargin + Extent.X) * WeaponZoom);

	const FRotator Look(-7.5f, 0.0f, 0.0f);   // a shade above the piece, looking at its centre
	WeaponCapture->SetActorLocationAndRotation(WeaponBoothOrigin - Look.Vector() * Distance, Look);
}

void ABasePlayerController::ToggleWeaponPreviewZoom()
{
	WeaponZoom = FMath::IsNearlyEqual(WeaponZoom, 1.0f) ? WeaponZoomClose : 1.0f;
	FrameWeaponPreview();
}

void ABasePlayerController::ResetWeaponPreviewView()
{
	// Side on (the muzzle to the camera's right), turned twenty degrees toward the lens.
	WeaponYaw = 70.0f; WeaponPitch = 8.0f; WeaponKick = 0.0f; WeaponZoom = 1.0f;
	PlaceWeaponPreview();
	FrameWeaponPreview();
}

void ABasePlayerController::FireWeaponPreview(const FString& SoundFile)
{
	if (!WeaponBoothActor) { return; }
	WeaponKick = WeaponKickTime;
	PlaceWeaponPreview();
	// The family's report (Tools/make_weapon_sounds.py), a little pitch either way so a
	// second press does not sound like a recording of the first.
	const FString Wav = SoundFile.IsEmpty() ? TEXT("wep_pistol.wav") : (SoundFile.EndsWith(TEXT(".wav")) ? SoundFile : SoundFile + TEXT(".wav"));
	UAmbientPlayer::PlayOneShot(this, GetWorld(), Wav, 0.85f, FMath::FRandRange(0.94f, 1.06f));
}

void ABasePlayerController::TickWeaponPreview(float DeltaSeconds)
{
	if (WeaponKick <= 0.0f) { return; }
	WeaponKick = FMath::Max(0.0f, WeaponKick - DeltaSeconds);
	PlaceWeaponPreview();
}

void ABasePlayerController::HideReference()
{
	if (!Screens.IsOpen(EScreen::Reference)) { return; }
	Screens.Close(EScreen::Reference);
	HideWeaponPreview();
	if (ReferenceWidget && ReferenceWidget->IsInViewport()) { ReferenceWidget->RemoveFromParent(); }
	// Back to the menu it came from.
	if (Screens.IsOpen(EScreen::PauseMenu) && PauseMenuWidget && !PauseMenuWidget->IsInViewport())
	{
		PauseMenuWidget->AddToViewport(100);
		PauseMenuWidget->SetAnchorsInViewport(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		PauseMenuWidget->SetAlignmentInViewport(FVector2D::ZeroVector);
	}
	// NOTHING OPEN, NOTHING PAUSED. Every page pauses the game on the way in, and four of them --
	// this one, the scenes panel, the settings panel and save/load -- never unpaused on the way out.
	// The world then stayed frozen behind a screen that was no longer there, with whatever the page
	// had left in front of it, until the pause menu happened to unpause it: "I exit the UI and it
	// stays; I have to hit escape a few times". Asking IsPageOpen rather than a flag means a page
	// closed on top of another leaves the pause where it belongs.
	if (!IsPageOpen()) { SetPause(false); }
	ApplyInputMode();
}

void ABasePlayerController::HidePauseMenu()
{
	HideSaveLoad();
	if (!Screens.IsOpen(EScreen::PauseMenu)) { return; }
	if (Screens.IsOpen(EScreen::Settings)) { Screens.Close(EScreen::Settings); if (SettingsWidget && SettingsWidget->IsInViewport()) { SettingsWidget->RemoveFromParent(); } }
	if (Screens.IsOpen(EScreen::Reference)) { Screens.Close(EScreen::Reference); if (ReferenceWidget && ReferenceWidget->IsInViewport()) { ReferenceWidget->RemoveFromParent(); } }
	Screens.Close(EScreen::PauseMenu);
	if (PauseMenuWidget && PauseMenuWidget->IsInViewport()) { PauseMenuWidget->RemoveFromParent(); }
	SetPause(false);
	ApplyInputMode();
}

void ABasePlayerController::ShowCharacterSheet()
{
	LowerWeaponForScreen();
	if (Screens.IsOpen(EScreen::CharacterSheet) || Screens.IsOpen(EScreen::PauseMenu)) { return; }
	if (!CharacterSheetWidget)
	{
		CharacterSheetWidget = CreateWidget<UCharacterSheetWidget>(this, UCharacterSheetWidget::StaticClass());
		CharacterSheetWidget->SetOwnerController(this);
	}
	ShowSheetMirror();
	CharacterSheetWidget->Rebuild();   // from UI/CharacterSheet.json, re-read if it changed
	CharacterSheetWidget->Refresh();
	PlaceConsolePage(CharacterSheetWidget, ConsolePageZ);
	Screens.Open(EScreen::CharacterSheet);
	SetPause(true);
	ApplyInputMode();
}

void ABasePlayerController::HideCharacterSheet()
{
	if (!Screens.IsOpen(EScreen::CharacterSheet)) { return; }
	Screens.Close(EScreen::CharacterSheet);
	HideSheetMirror();
	for (TObjectIterator<UContextMenuWidget> It; It; ++It) { if (It->IsInViewport()) { It->Close(); } }   // a DROP menu left open closes with the sheet
	if (CharacterSheetWidget && CharacterSheetWidget->IsInViewport() && !bRetainPageWidget) { CharacterSheetWidget->RemoveFromParent(); }
	if (!Screens.IsOpen(EScreen::PauseMenu)) { SetPause(false); }
	ApplyInputMode();
}

void ABasePlayerController::ReloadUI()
{
	HideCharacterSheet(); CloseTransfer(); HideInspectMenu();
	if (CharacterSheetWidget) { CharacterSheetWidget->RemoveFromParent(); CharacterSheetWidget = nullptr; }
	if (InspectMenuWidget) { InspectMenuWidget->RemoveFromParent(); InspectMenuWidget = nullptr; }
	FSheetSpec::Get(true);
	SetDiagNote(TEXT("UI reloaded"));
}

void ABasePlayerController::ToggleCharacterSheet()
{
	if (Screens.IsOpen(EScreen::CharacterSheet)) { HideCharacterSheet(); } else { ShowCharacterSheet(); }
}

void ABasePlayerController::QuitGame()
{
	// In a packaged game this closes it; in the editor it ends the PIE session.
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}

void ABasePlayerController::OnToggleEditInput(const FInputActionValue& Value)
{
	UE_LOG(LogTemp, Log, TEXT("EditMode: TAB received (edit mode was %d)"), bEditMode);
	ToggleEditMode();
}
void ABasePlayerController::OnClickInput(const FInputActionValue& Value) { HandleManagerClick(); }

void ABasePlayerController::OnFlyForward(const FInputActionValue& Value)
{
	if (bFlying && FlyPawn) { FlyPawn->AddMovementInput(GetControlRotation().Vector(), Value.Get<float>()); }
}

void ABasePlayerController::OnFlyRight(const FInputActionValue& Value)
{
	if (bFlying && FlyPawn) { FlyPawn->AddMovementInput(FRotationMatrix(GetControlRotation()).GetScaledAxis(EAxis::Y), Value.Get<float>()); }
}

void ABasePlayerController::OnFlyUp(const FInputActionValue& Value)
{
	if (bFlying && FlyPawn) { FlyPawn->AddMovementInput(FVector::UpVector, 1.0f); }
}

void ABasePlayerController::OnFlyDown(const FInputActionValue& Value)
{
	if (bFlying && FlyPawn) { FlyPawn->AddMovementInput(FVector::UpVector, -1.0f); }
}

void ABasePlayerController::OnFlyYaw(const FInputActionValue& Value)
{
	// Only while the mouse is captured (RMB held) -- otherwise moving the
	// free cursor over the pages would spin the view.
	if (bFlying && IsInputKeyDown(EKeys::RightMouseButton)) { AddYawInput(Value.Get<float>() * FlyLookScale); }
}

void ABasePlayerController::OnFlyPitch(const FInputActionValue& Value)
{
	if (bFlying && IsInputKeyDown(EKeys::RightMouseButton)) { AddPitchInput(Value.Get<float>() * FlyLookScale); }
}

void ABasePlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	// The player's chosen likeness, if one has been accepted.
	if (!bPlayerConfigApplied)
	{
		if (ABaseCharacter* C = Cast<ABaseCharacter>(InPawn))
		{
			FCharacterConfig Saved;
			if (IFileManager::Get().FileExists(*CharacterConfigFile::GetPath(TEXT("Player"))) && CharacterConfigFile::Load(TEXT("Player"), Saved))
			{
				C->ApplyCharacterConfig(Saved);
				bPlayerConfigApplied = true;
			}
		}
	}
	bFlying = InPawn && InPawn->IsA<ASpectatorPawn>();
	// Taking control of a character with nothing selected (the level's
	// auto-possess at start) selects it so the manager has something to show.
	if (ABaseCharacter* AsCharacter = Cast<ABaseCharacter>(InPawn))
	{
		if (!SelectedCharacter.IsValid()) { SelectedCharacter = AsCharacter; }
	}
	if (ManagerWidget) { ManagerWidget->SetTargetCharacter(GetSelectedCharacter()); }
	if (FaceManagerWidget) { FaceManagerWidget->SetTargetCharacter(GetSelectedCharacter()); }
	if (AnimBrowserWidget) { AnimBrowserWidget->SetTargetCharacter(GetSelectedCharacter()); }
	if (EditToolWidget) { EditToolWidget->RefreshFromController(); }
	RefreshNameLabels();
	ApplyInputMode();
}

// ---- Edit mode / pages ------------------------------------------------------

void ABasePlayerController::ToggleEditMode()
{
	if (bEditMode) { ExitEditMode(); } else { EnterEditMode(); }
}

void ABasePlayerController::EnterEditMode()
{
	if (bEditMode) { return; }
	bEditMode = true;
	// Remember the player's pawn and view so leaving edit mode returns there,
	// then lift off into the fly camera from where the camera already is.
	if (!bFlying)
	{
		PreEditPawn = GetPawn();
		PreEditControlRotation = GetControlRotation();
	}
	ShowPage(EEditPage::EditTool);
	if (!bFlying) { EnterFlyMode(); }
	ApplyInputMode();
	RefreshNameLabels();
}

void ABasePlayerController::ExitEditMode()
{
	if (!bEditMode) { return; }
	LeaveCharacterPagesThen([this]() { ExitEditModeNow(); });
}

void ABasePlayerController::ExitEditModeNow()
{
	if (!bEditMode) { return; }
	CancelPlacingCharacter();
	CancelClaudeAssist();
	SetSelectionHighlight(GetSelectedCharacter(), false);
	UpdateHoverHighlight(nullptr);
	ShowPage(EEditPage::None);
	bEditMode = false;
	// Back to the pawn F12 left, unless a character was taken over meanwhile.
	if (bFlying && PreEditPawn.IsValid())
	{
		APawn* Back = PreEditPawn.Get();
		Possess(Back);
		if (FlyPawn) { FlyPawn->Destroy(); FlyPawn = nullptr; }
		bFlying = false;
		SetControlRotation(PreEditControlRotation);
	}
	PreEditPawn = nullptr;
	ApplyInputMode();
	RefreshNameLabels();
}

void ABasePlayerController::ShowEditTool()
{
	LeaveCharacterPagesThen([this]() { ShowEditToolNow(); });
}

void ABasePlayerController::ShowEditToolNow()
{
	EnterEditMode();
	ShowPage(EEditPage::EditTool);
	SetSelectionHighlight(GetSelectedCharacter(), false);
	ApplyInputMode();
}

void ABasePlayerController::LeaveCharacterPagesThen(TFunction<void()> Continue)
{
	const bool bOnCharacterPage = bEditMode
		&& (CurrentPage == EEditPage::CharacterManager || CurrentPage == EEditPage::FaceManager || CurrentPage == EEditPage::AnimBrowser);
	ABaseCharacter* Selected = GetSelectedCharacter();
	if (bBypassSavePrompt || !bOnCharacterPage || !Selected || !Selected->IsConfigDirty())
	{
		// Guarded too: a continuation that re-enters SelectCharacter must not
		// come back through here.
		TGuardValue<bool> Bypass(bBypassSavePrompt, true);
		if (Continue) { Continue(); }
		return;
	}

	if (!ConfirmDialog)
	{
		ConfirmDialog = CreateWidget<UConfirmDialogWidget>(this, UConfirmDialogWidget::StaticClass());
	}
	TWeakObjectPtr<ABaseCharacter> WeakSelected(Selected);
	TWeakObjectPtr<ABasePlayerController> WeakThis(this);
	// Each choice runs with the prompt suppressed so the continuation can
	// call straight back into ShowEditTool/ExitEditMode/SelectCharacter.
	const auto Run = [WeakThis, Continue]()
	{
		if (!WeakThis.IsValid()) { return; }
		TGuardValue<bool> Bypass(WeakThis->bBypassSavePrompt, true);
		if (Continue) { Continue(); }
	};
	ConfirmDialog->Setup(TEXT("Unsaved changes"), FString::Printf(TEXT("Save changes to %s?"), *Selected->GetCharacterConfig().Name),
		[WeakSelected, Run]() { if (WeakSelected.IsValid()) { WeakSelected->SaveCharacterConfig(); } Run(); },
		[WeakSelected, Run]() { if (WeakSelected.IsValid()) { WeakSelected->RevertToSavedConfig(); } Run(); },
		[]() {});
	if (!ConfirmDialog->IsInViewport())
	{
		ConfirmDialog->AddToViewport(95);
		ConfirmDialog->SetAnchorsInViewport(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		ConfirmDialog->SetAlignmentInViewport(FVector2D::ZeroVector);
	}
}

void ABasePlayerController::SaveSelectedAndClose()
{
	if (ABaseCharacter* Selected = GetSelectedCharacter()) { Selected->SaveCharacterConfig(); }
	ShowEditToolNow();
}

void ABasePlayerController::ShowCharacterManager()
{
	EnterEditMode();
	ShowPage(EEditPage::CharacterManager);
	SetSelectionHighlight(GetSelectedCharacter(), true);
	ApplyInputMode();
}

void ABasePlayerController::ShowFaceManager()
{
	EnterEditMode();
	ShowPage(EEditPage::FaceManager);
	SetSelectionHighlight(GetSelectedCharacter(), true);
	ApplyInputMode();
}

void ABasePlayerController::ShowAnimationBrowser()
{
	EnterEditMode();
	ShowPage(EEditPage::AnimBrowser);
	SetSelectionHighlight(GetSelectedCharacter(), true);
	ApplyInputMode();
}

void ABasePlayerController::UpdateViewCamera(EViewFrame Frame)
{
	ABaseCharacter* Target = (Frame == EViewFrame::Conversation) ? ConversationPartner.Get() : GetSelectedCharacter();
	if (Frame != EViewFrame::None && Target && GetWorld())
	{
		const USkeletalMeshComponent* Mesh = Target->GetMesh();
		const FVector Head = (Mesh && Mesh->DoesSocketExist(TEXT("head"))) ? Mesh->GetSocketLocation(TEXT("head")) : Target->GetActorLocation() + FVector(0, 0, 70);
		const FVector Forward = Target->GetActorForwardVector();
		const FVector Right = Target->GetActorRightVector();
		FVector Eye, LookAt;
		float Fov;
		// The panels cover the left third of the screen, so both framings aim
		// a little to the camera's LEFT of the character (= the character's
		// own right, since the camera faces it), which puts the character in
		// the clear right-hand part of the view.
		if (Frame == EViewFrame::Face)
		{
			// Portrait: ~1.8m in front of the face, off to the character's
			// right and a little above eye level, looking at the head.
			Eye = Head + Forward * 180.0f + Right * 65.0f + FVector(0, 0, 35.0f);
			LookAt = Head + Forward * 8.0f + Right * 28.0f;
			Fov = 45.0f;
		}
		else if (Frame == EViewFrame::Conversation)
		{
			// Conversation: the whole character, centred in the LEFT two
			// thirds (the panel takes the right third), so aim to the
			// camera's right = the character's own left. 4m out at 55deg
			// shows ~2.3m of height, so head to feet fit with room to spare.
			const FVector Chest = Target->GetActorLocation() + FVector(0, 0, 30.0f);
			Eye = Chest + Forward * 400.0f + FVector(0, 0, 40.0f);
			LookAt = Chest - Right * 70.0f - FVector(0, 0, 10.0f);
			Fov = 55.0f;
		}
		else
		{
			// Whole character: ~3.2m out, a step to its right, above eye
			// level and looking at the chest, so head to feet fit the frame.
			const FVector Chest = Target->GetActorLocation() + FVector(0, 0, 30.0f);
			Eye = Chest + Forward * 320.0f + Right * 90.0f + FVector(0, 0, 90.0f);
			LookAt = Chest + Right * 65.0f;
			Fov = 60.0f;
		}
		// Don't put the camera inside a wall: sweep out from the character and
		// stop short of whatever is in the way (the character and the player
		// themselves don't count).
		{
			FCollisionQueryParams Query(SCENE_QUERY_STAT(ViewCamera), false);
			Query.AddIgnoredActor(Target);
			if (APawn* P = GetPawn()) { Query.AddIgnoredActor(P); }
			const FVector From = Target->GetActorLocation() + FVector(0, 0, 40.0f);
			FHitResult Hit;
			if (GetWorld()->SweepSingleByChannel(Hit, From, Eye, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(18.0f), Query) && Hit.bBlockingHit)
			{
				Eye = Hit.Location;
			}
		}
		const FRotator Look = (LookAt - Eye).Rotation();
		// A fresh camera actor per framing so the view BLENDS from the old
		// spot to the new one (moving the same actor would snap); the old one
		// lingers just long enough for the blend to finish.
		ACameraActor* Previous = FaceCamera;
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		FaceCamera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), Eye, Look, Params);
		if (!FaceCamera) { FaceCamera = Previous; return; }
		if (FaceCamera->GetCameraComponent()) { FaceCamera->GetCameraComponent()->SetFieldOfView(Fov); }
		SetViewTargetWithBlend(FaceCamera, 0.45f, VTBlend_EaseInOut, 2.0f);
		if (Previous) { Previous->SetLifeSpan(1.5f); }
	}
	else if (FaceCamera && GetViewTarget() == FaceCamera)
	{
		// Back to whatever we are controlling (the pawn's own camera).
		if (APawn* P = GetPawn()) { SetViewTargetWithBlend(P, 0.35f, VTBlend_EaseInOut, 2.0f); }
	}
}

void ABasePlayerController::ShowPage(EEditPage Page)
{
	// Pages are mutually exclusive and all sit bottom-left.
	// Leaving the Animation Browser puts the character back on its own
	// animation behaviour (any try-out clip stops).
	if (CurrentPage == EEditPage::AnimBrowser && Page != EEditPage::AnimBrowser)
	{
		if (ABaseCharacter* Selected = GetSelectedCharacter()) { Selected->StopAnimationClip(); }
	}
	if (Page != EEditPage::EditTool && EditToolWidget && EditToolWidget->IsInViewport()) { EditToolWidget->RemoveFromParent(); }
	if (Page != EEditPage::CharacterManager && ManagerWidget && ManagerWidget->IsInViewport()) { ManagerWidget->RemoveFromParent(); }
	if (Page != EEditPage::FaceManager && FaceManagerWidget && FaceManagerWidget->IsInViewport()) { FaceManagerWidget->RemoveFromParent(); }
	if (Page != EEditPage::AnimBrowser && AnimBrowserWidget && AnimBrowserWidget->IsInViewport()) { AnimBrowserWidget->RemoveFromParent(); }
	// Portrait on the face page, the whole character again on the way back
	// out of it, and the pawn's own camera once edit mode closes.
	if (Page == EEditPage::FaceManager) { UpdateViewCamera(EViewFrame::Face); }
	else if (Page == EEditPage::None) { UpdateViewCamera(EViewFrame::None); }
	else if (CurrentPage == EEditPage::FaceManager) { UpdateViewCamera(EViewFrame::Body); }

	if (Page == EEditPage::EditTool)
	{
		if (!EditToolWidget)
		{
			EditToolWidget = CreateWidget<UEditToolWidget>(this, UEditToolWidget::StaticClass());
			EditToolWidget->SetOwnerController(this);
		}
		if (!EditToolWidget->IsInViewport())
		{
			EditToolWidget->AddToViewport();
			// Bottom-left region, stretched (min != max) exactly like the
			// Character Manager -- the same placement that is known to render.
			EditToolWidget->SetAnchorsInViewport(FAnchors(0.0f, 0.84f, 0.24f, 1.0f));
		}
		EditToolWidget->RefreshFromController();
		bLogEditToolGeometry = true;
		UE_LOG(LogTemp, Log, TEXT("EditMode: EditTool page shown (in viewport: %d)"), EditToolWidget->IsInViewport());
	}
	else if (Page == EEditPage::CharacterManager)
	{
		if (!ManagerWidget)
		{
			ManagerWidget = CreateWidget<UCharacterBuilderWidget>(this, UCharacterBuilderWidget::StaticClass());
			ManagerWidget->SetOwnerController(this);
		}
		ManagerWidget->SetTargetCharacter(GetSelectedCharacter());
		if (!ManagerWidget->IsInViewport())
		{
			ManagerWidget->AddToViewport();
			// Bottom-left: left 34% of the width, bottom 85% of the height;
			// the panel scrolls inside it.
			ManagerWidget->SetAnchorsInViewport(FAnchors(0.0f, 0.15f, 0.34f, 1.0f));
		}
	}
	else if (Page == EEditPage::FaceManager)
	{
		if (!FaceManagerWidget)
		{
			FaceManagerWidget = CreateWidget<UFaceManagerWidget>(this, UFaceManagerWidget::StaticClass());
			FaceManagerWidget->SetOwnerController(this);
		}
		FaceManagerWidget->SetTargetCharacter(GetSelectedCharacter());
		if (!FaceManagerWidget->IsInViewport())
		{
			FaceManagerWidget->AddToViewport();
			FaceManagerWidget->SetAnchorsInViewport(FAnchors(0.0f, 0.15f, 0.34f, 1.0f));
		}
	}
	else if (Page == EEditPage::AnimBrowser)
	{
		if (!AnimBrowserWidget)
		{
			AnimBrowserWidget = CreateWidget<UAnimationBrowserWidget>(this, UAnimationBrowserWidget::StaticClass());
			AnimBrowserWidget->SetOwnerController(this);
		}
		AnimBrowserWidget->SetTargetCharacter(GetSelectedCharacter());
		if (!AnimBrowserWidget->IsInViewport())
		{
			AnimBrowserWidget->AddToViewport();
			AnimBrowserWidget->SetAnchorsInViewport(FAnchors(0.0f, 0.15f, 0.34f, 1.0f));
		}
	}
	CurrentPage = Page;
}

// ---- Selection / fly mode -------------------------------------------------

ABaseCharacter* ABasePlayerController::GetSelectedCharacter() const
{
	return SelectedCharacter.Get();
}

void ABasePlayerController::SetSelectionHighlight(ABaseCharacter* Target, bool bVisible)
{
	if (!Target) { return; }
	// No visual any more: the selected character is shown by framing the
	// camera on it (see UpdateViewCamera) rather than a gold shell / beam --
	// the hover highlight stays the only overlay. Kept as the single place
	// to switch a selection cue back on.
	Target->SetSelectedHighlight(false);
	if (Target->GetFaceController()) { Target->GetFaceController()->SetHighlightVisible(false); }
}

void ABasePlayerController::SelectCharacter(ABaseCharacter* Target)
{
	const bool bNewPick = Target && Target != GetSelectedCharacter();
	if (bNewPick && !bBypassSavePrompt)
	{
		// Picking another character while this one has unsaved changes goes
		// through the Save / Discard / Cancel prompt first.
		TWeakObjectPtr<ABaseCharacter> WeakTarget(Target);
		LeaveCharacterPagesThen([this, WeakTarget]() { if (WeakTarget.IsValid()) { SelectCharacter(WeakTarget.Get()); } });
		return;
	}
	if (ABaseCharacter* Previous = GetSelectedCharacter())
	{
		if (Previous != Target) { SetSelectionHighlight(Previous, false); }
	}
	SelectedCharacter = Target;
	if (ManagerWidget) { ManagerWidget->SetTargetCharacter(Target); }
	if (FaceManagerWidget) { FaceManagerWidget->SetTargetCharacter(Target); }
	if (AnimBrowserWidget) { AnimBrowserWidget->SetTargetCharacter(Target); }

	if (!Target)
	{
		if (bEditMode && CurrentPage != EEditPage::EditTool && CurrentPage != EEditPage::None) { ShowPage(EEditPage::EditTool); }
		if (EditToolWidget) { EditToolWidget->RefreshFromController(); }
		ApplyInputMode();
		return;
	}
	if (!bEditMode) { ApplyInputMode(); }
	else if (CurrentPage == EEditPage::FaceManager) { ShowFaceManager(); }   // stays on the face page, re-aimed at the new pick
	else if (CurrentPage == EEditPage::AnimBrowser) { ShowAnimationBrowser(); if (bNewPick) { UpdateViewCamera(EViewFrame::Body); } }
	else
	{
		ShowCharacterManager();
		// A fresh pick brings the view round to a clear look at the whole
		// character; re-clicking the same one leaves the camera where it is.
		if (bNewPick) { UpdateViewCamera(EViewFrame::Body); }
	}
}

void ABasePlayerController::ControlCharacter(ABaseCharacter* Target)
{
	if (!Target) { return; }
	if (Target != GetPawn())
	{
		Possess(Target);
		if (FlyPawn) { FlyPawn->Destroy(); FlyPawn = nullptr; }
		bFlying = false;
		// Camera starts behind the character it just took over.
		SetControlRotation(Target->GetActorRotation());
	}
	SelectCharacter(Target);
	// Playing it now: the view goes back to its own camera, not the framing.
	UpdateViewCamera(EViewFrame::None);
	RefreshNameLabels();
}

void ABasePlayerController::NotifyEditClick()
{
	if (bEditMode) { HandleManagerClick(); }
}

void ABasePlayerController::RefreshNameLabels()
{
	if (!GetWorld()) { return; }
	for (TActorIterator<ABaseCharacter> It(GetWorld()); It; ++It)
	{
		// Your own tag would just hover over the camera in first person.
		It->SetNameLabelVisible(bEditMode && *It != GetPawn());
	}
}

void ABasePlayerController::EnterFlyMode()
{
	if (bFlying || !GetWorld()) { return; }
	const FVector CameraLocation = PlayerCameraManager ? PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
	const FRotator CameraRotation = PlayerCameraManager ? PlayerCameraManager->GetCameraRotation() : FRotator::ZeroRotator;

	// "Deselect": the selection goes too.
	SetSelectionHighlight(GetSelectedCharacter(), false);
	SelectedCharacter = nullptr;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// Deferred so bAddDefaultMovementBindings is off BEFORE the pawn sets up
	// its input: the spectator's legacy WASD/mouse axes would double up with
	// (or be consumed underneath) the Enhanced Input bindings above.
	FlyPawn = GetWorld()->SpawnActorDeferred<ASpectatorPawn>(ASpectatorPawn::StaticClass(), FTransform(CameraRotation, CameraLocation), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!FlyPawn) { return; }
	FlyPawn->bAddDefaultMovementBindings = false;
	FlyPawn->FinishSpawning(FTransform(CameraRotation, CameraLocation));
	Possess(FlyPawn);
	SetControlRotation(CameraRotation);
	bFlying = true;
	if (ManagerWidget) { ManagerWidget->SetTargetCharacter(nullptr); }
	// Nothing to manage -- back to the root page if the manager was up.
	if (bEditMode && CurrentPage == EEditPage::CharacterManager) { ShowPage(EEditPage::EditTool); }
	if (EditToolWidget) { EditToolWidget->RefreshFromController(); }
	ApplyInputMode();
}

bool ABasePlayerController::RemoveSelectedCharacter()
{
	ABaseCharacter* Selected = GetSelectedCharacter();
	if (!Selected) { return false; }
	if (Selected == GetPawn())
	{
		EnterFlyMode();   // unpossesses first, from where the camera was (and clears the selection)
	}
	else
	{
		SetSelectionHighlight(Selected, false);
		SelectedCharacter = nullptr;
		if (ManagerWidget) { ManagerWidget->SetTargetCharacter(nullptr); }
		if (bEditMode && CurrentPage == EEditPage::CharacterManager) { ShowPage(EEditPage::EditTool); }
		if (EditToolWidget) { EditToolWidget->RefreshFromController(); }
	}
	Selected->Destroy();
	return true;
}

// ---- Input mode -----------------------------------------------------------

void ABasePlayerController::ApplyInputMode()
{
	UGameViewportClient* Viewport = GetLocalPlayer() ? GetLocalPlayer()->ViewportClient : nullptr;

	if (Screens.IsOpen(EScreen::PauseMenu) || Screens.IsOpen(EScreen::CharacterSheet) || bAppearanceInputMode || Screens.IsOpen(EScreen::Transfer) || Screens.IsOpen(EScreen::Reference) || Screens.IsOpen(EScreen::Terminal) || Screens.IsOpen(EScreen::SaveLoad) || Screens.IsOpen(EScreen::HandTune))
	{
		// Menu only: free cursor, nothing reaches the game until Back.
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		SetInputMode(Mode);
		SetShowMouseCursor(true);
		if (Viewport) { Viewport->SetMouseCaptureMode(EMouseCaptureMode::NoCapture); }
		CurrentMouseCursor = EMouseCursor::Default;
		return;
	}

	if (IsInConversation() || IsInCinematic())
	{
		// The panel's replies need a free cursor; nothing else takes input.
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		SetInputMode(Mode);
		SetShowMouseCursor(true);
		if (Viewport) { Viewport->SetMouseCaptureMode(EMouseCaptureMode::NoCapture); }
		CurrentMouseCursor = EMouseCursor::Default;
		return;
	}

	if (bFlying || bEditMode)
	{
		// Free cursor for the pages and for clicking characters; the mouse
		// is captured (look) only while RMB is held, editor-style.
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(true);
		SetInputMode(Mode);
		SetShowMouseCursor(true);
		// So the controller receives cursor clicks/hover on world actors even
		// though the mouse isn't captured (used by the edit-mode select poll).
		bEnableClickEvents = true;
		bEnableMouseOverEvents = true;
		if (Viewport) { Viewport->SetMouseCaptureMode(EMouseCaptureMode::CaptureDuringRightMouseDown); }
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);
		if (Viewport) { Viewport->SetMouseCaptureMode(EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown); }
	}
	CurrentMouseCursor = bPlacingCharacter ? EMouseCursor::Crosshairs : (bAssistMode ? EMouseCursor::EyeDropper : EMouseCursor::Default);

	// The Character Manager is the one page tall enough to cover the
	// character; slide the camera only while it is up, not for edit mode as
	// a whole (the Edit Tool is a single small button).
	if (ABaseCharacter* Selected = GetSelectedCharacter()) { Selected->SetManagerCameraOffset(bEditMode && CurrentPage == EEditPage::CharacterManager); }
}

// ---- Claude Assist ----------------------------------------------------------

FString ABasePlayerController::GetAssistDirectory() const
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ClaudeAssist")));
}

void ABasePlayerController::BeginClaudeAssist()
{
	EnterEditMode();
	CancelPlacingCharacter();
	bAssistMode = true;
	if (EditToolWidget) { EditToolWidget->SetStatus(TEXT("Claude Assist: click anything in the world.")); }
	ApplyInputMode();
}

void ABasePlayerController::CancelClaudeAssist()
{
	if (!bAssistMode) { return; }
	bAssistMode = false;
	ApplyInputMode();
}

bool ABasePlayerController::RecordAssistRay(const FVector& Start, const FVector& End)
{
	UWorld* World = GetWorld();
	if (!World) { return false; }
	FCollisionQueryParams Params(TEXT("ClaudeAssist"), true);
	if (GhostActor) { Params.AddIgnoredActor(GhostActor); }
	// The player and anything riding them. Without this the ECC_Pawn trace below hits our own
	// CollisionCylinder at distance 0 and wins as the nearest hit -- every capture recorded the
	// player instead of what the reticle was on. The attached actors matter now the ray starts at
	// the eye: the held weapon would otherwise block it at point-blank range.
	if (APawn* Me = GetPawn())
	{
		Params.AddIgnoredActor(Me);
		TArray<AActor*> Attached;
		Me->GetAttachedActors(Attached, true, true);
		Params.AddIgnoredActors(Attached);
	}
	FHitResult Hit;
	// Characters: their capsule blocks Pawn, not Visibility, and their
	// visible parts have no collision -- so take the nearer of the two.
	FHitResult VisibilityHit, PawnHit;
	const bool bVis = World->LineTraceSingleByChannel(VisibilityHit, Start, End, ECC_Visibility, Params);
	const bool bPawn = World->LineTraceSingleByChannel(PawnHit, Start, End, ECC_Pawn, Params);
	if (bVis && (!bPawn || VisibilityHit.Distance <= PawnHit.Distance)) { Hit = VisibilityHit; }
	else if (bPawn) { Hit = PawnHit; }
	else
	{
		// Nothing solid under the cursor -- still record the ray so "the sky
		// over there" is a usable answer.
		Hit.Location = End;
		Hit.ImpactNormal = FVector::ZeroVector;
		Hit.Distance = (End - Start).Size();
	}
	RecordAssistHit(Hit, Start);
	return Hit.GetActor() != nullptr;
}

void ABasePlayerController::CaptureAssistUnderReticle()
{
	if (!GetWorld()) { return; }
	// Prefer the character's aim accessors over the raw camera: the reticle is not always
	// camera-centre (see BaseCharacter.h GetAimRotation), and starting at the eye rather than
	// the near plane under the cursor is what keeps the ray out of our own capsule while flying.
	FVector Start = PlayerCameraManager ? PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
	FRotator Dir = PlayerCameraManager ? PlayerCameraManager->GetCameraRotation() : FRotator::ZeroRotator;
	if (ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn()))
	{
		Start = Me->GetAimOrigin();
		Dir = Me->GetAimRotation();
	}
	RecordAssistRay(Start, Start + Dir.Vector() * 100000.0f);
}

void ABasePlayerController::RecordAssistHit(const FHitResult& Hit, const FVector& RayStart)
{
	auto VecObject = [](const FVector& V)
	{
		TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetNumberField(TEXT("x"), FMath::RoundToDouble(V.X * 100.0) / 100.0);
		O->SetNumberField(TEXT("y"), FMath::RoundToDouble(V.Y * 100.0) / 100.0);
		O->SetNumberField(TEXT("z"), FMath::RoundToDouble(V.Z * 100.0) / 100.0);
		return O;
	};
	auto RotObject = [](const FRotator& R)
	{
		TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetNumberField(TEXT("pitch"), FMath::RoundToDouble(R.Pitch * 100.0) / 100.0);
		O->SetNumberField(TEXT("yaw"), FMath::RoundToDouble(R.Yaw * 100.0) / 100.0);
		O->SetNumberField(TEXT("roll"), FMath::RoundToDouble(R.Roll * 100.0) / 100.0);
		return O;
	};

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("index"), ++AssistClickCount);
	Root->SetStringField(TEXT("time"), FDateTime::Now().ToIso8601());
	Root->SetStringField(TEXT("level"), GetWorld() ? GetWorld()->GetMapName() : TEXT(""));
	Root->SetObjectField(TEXT("location"), VecObject(Hit.Location));
	Root->SetObjectField(TEXT("normal"), VecObject(Hit.ImpactNormal));
	Root->SetNumberField(TEXT("distance"), FMath::RoundToDouble(Hit.Distance * 10.0) / 10.0);
	Root->SetObjectField(TEXT("ray_start"), VecObject(RayStart));

	FString Summary;
	if (AActor* Actor = Hit.GetActor())
	{
		TSharedPtr<FJsonObject> ActorObject = MakeShared<FJsonObject>();
		ActorObject->SetStringField(TEXT("name"), Actor->GetName());
		ActorObject->SetStringField(TEXT("label"), Actor->GetActorNameOrLabel());
		ActorObject->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
		ActorObject->SetStringField(TEXT("path"), Actor->GetPathName());
		ActorObject->SetObjectField(TEXT("location"), VecObject(Actor->GetActorLocation()));
		ActorObject->SetObjectField(TEXT("rotation"), RotObject(Actor->GetActorRotation()));
		ActorObject->SetObjectField(TEXT("scale"), VecObject(Actor->GetActorScale3D()));
		if (const ABaseCharacter* HitCharacter = Cast<ABaseCharacter>(Actor))
		{
			ActorObject->SetStringField(TEXT("character_config"), HitCharacter->GetCharacterConfig().Name);
		}
		Root->SetObjectField(TEXT("actor"), ActorObject);
		Summary = Actor->GetActorNameOrLabel();

		if (UPrimitiveComponent* Comp = Hit.GetComponent())
		{
			TSharedPtr<FJsonObject> CompObject = MakeShared<FJsonObject>();
			CompObject->SetStringField(TEXT("name"), Comp->GetName());
			CompObject->SetStringField(TEXT("class"), Comp->GetClass()->GetName());
			if (const UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Comp))
			{
				if (SMC->GetStaticMesh()) { CompObject->SetStringField(TEXT("mesh"), SMC->GetStaticMesh()->GetPathName()); Summary += TEXT(" / ") + SMC->GetStaticMesh()->GetName(); }
			}
			else if (const USkeletalMeshComponent* SKC = Cast<USkeletalMeshComponent>(Comp))
			{
				if (SKC->GetSkeletalMeshAsset()) { CompObject->SetStringField(TEXT("mesh"), SKC->GetSkeletalMeshAsset()->GetPathName()); Summary += TEXT(" / ") + SKC->GetSkeletalMeshAsset()->GetName(); }
			}
			if (Comp->GetNumMaterials() > 0 && Comp->GetMaterial(0)) { CompObject->SetStringField(TEXT("material"), Comp->GetMaterial(0)->GetPathName()); }
			if (!Hit.BoneName.IsNone()) { CompObject->SetStringField(TEXT("bone"), Hit.BoneName.ToString()); }
			Root->SetObjectField(TEXT("component"), CompObject);
		}
	}
	else
	{
		Summary = TEXT("nothing (open space)");
	}

	TSharedPtr<FJsonObject> CameraObject = MakeShared<FJsonObject>();
	if (PlayerCameraManager)
	{
		CameraObject->SetObjectField(TEXT("location"), VecObject(PlayerCameraManager->GetCameraLocation()));
		CameraObject->SetObjectField(TEXT("rotation"), RotObject(PlayerCameraManager->GetCameraRotation()));
	}
	Root->SetObjectField(TEXT("camera"), CameraObject);
	Root->SetStringField(TEXT("selected_character"), GetSelectedCharacter() ? GetSelectedCharacter()->GetCharacterConfig().Name : TEXT(""));
	Root->SetBoolField(TEXT("flying"), bFlying);

	const FString Dir = GetAssistDirectory();
	IFileManager::Get().MakeDirectory(*Dir, true);
	// Timestamped, not indexed: AssistClickCount restarts at 1 every PIE session, so click_N.png
	// was overwritten run to run -- five logged captures shared one PNG on disk. Matters more now
	// F12 is a fast repeatable one-shot.
	const FString ScreenshotPath = FPaths::Combine(Dir, FString::Printf(TEXT("click_%s_%d.png"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")), AssistClickCount));
	Root->SetStringField(TEXT("screenshot"), ScreenshotPath);
	// Requested, not guaranteed -- it lands a frame later if the viewport
	// cooperates; the JSON is the reliable part.
	FScreenshotRequest::RequestScreenshot(ScreenshotPath, /*bInShowUI=*/false, /*bAddFilenameSuffix=*/false);

	FString Json;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
	FFileHelper::SaveStringToFile(Json, *FPaths::Combine(Dir, TEXT("last_click.json")));

	FString Line;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> LineWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Line);
	FJsonSerializer::Serialize(Root.ToSharedRef(), LineWriter);
	FFileHelper::SaveStringToFile(Line + LINE_TERMINATOR, *FPaths::Combine(Dir, TEXT("clicks.jsonl")), FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append);

	UE_LOG(LogTemp, Log, TEXT("ClaudeAssist: #%d %s at (%.0f, %.0f, %.0f) -> %s"), AssistClickCount, *Summary, Hit.Location.X, Hit.Location.Y, Hit.Location.Z, *FPaths::Combine(Dir, TEXT("last_click.json")));
	if (EditToolWidget)
	{
		EditToolWidget->SetStatus(FString::Printf(TEXT("Sent to Claude: %s at (%.0f, %.0f, %.0f)"), *Summary, Hit.Location.X, Hit.Location.Y, Hit.Location.Z));
	}
	// The F12 path has no EditToolWidget, so this is the only confirmation the player gets that
	// the capture happened and what it caught.
	SetDiagNoteTimed(FString::Printf(TEXT("Sent to Claude: %s"), *Summary), 6.0f);
}

// ---- Placement -------------------------------------------------------------

void ABasePlayerController::BeginPlacingCharacter()
{
	if (!GetWorld()) { return; }
	EnterEditMode();
	CancelClaudeAssist();
	bPlacingCharacter = true;
	bDropPointValid = false;
	bDropPointHit = false;
	if (!GhostActor)
	{
		GhostActor = GetWorld()->SpawnActor<ACharacterGhostActor>(ACharacterGhostActor::StaticClass());
	}
	if (GhostActor)
	{
		GhostActor->BuildFromConfig(ModularHero::MakeDefaultConfig(TEXT("New")));
		GhostActor->SetActorHiddenInGame(true);
	}
	ApplyInputMode();
}

void ABasePlayerController::CancelPlacingCharacter()
{
	if (!bPlacingCharacter) { return; }
	bPlacingCharacter = false;
	if (GhostActor) { GhostActor->Destroy(); GhostActor = nullptr; }
	ApplyInputMode();
}

bool ABasePlayerController::FindDropPointUnderCursor(FVector& OutFloorPoint, bool& bOutHit) const
{
	bOutHit = false;
	UWorld* World = GetWorld();
	if (!World) { return false; }

	FVector Origin, Direction;
	if (!DeprojectMousePositionToWorld(Origin, Direction)) { return false; }

	FCollisionQueryParams Params(TEXT("CharacterDrop"), false);
	if (GhostActor) { Params.AddIgnoredActor(GhostActor); }
	if (APawn* Current = GetPawn())
	{
		Params.AddIgnoredActor(Current);
		TArray<AActor*> Attached;
		Current->GetAttachedActors(Attached, true, true);
		Params.AddIgnoredActors(Attached);
	}

	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, Origin, Origin + Direction * 20000.0f, ECC_Visibility, Params)) { return false; }
	bOutHit = true;
	OutFloorPoint = Hit.Location;

	// Horizontal enough to stand on.
	if (Hit.ImpactNormal.Z < 0.95f) { return false; }

	const UCapsuleComponent* Capsule = GetDefault<ABaseCharacter>()->GetCapsuleComponent();
	const float Radius = Capsule ? Capsule->GetUnscaledCapsuleRadius() : 34.0f;
	const float HalfHeight = Capsule ? Capsule->GetUnscaledCapsuleHalfHeight() : 88.0f;

	// Room for the capsule (2cm up so the floor itself doesn't count).
	const FVector Center = Hit.Location + FVector(0.0f, 0.0f, HalfHeight + 2.0f);
	if (World->OverlapBlockingTestByChannel(Center, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Radius, HalfHeight), Params)) { return false; }

	// The whole footprint has floor under it at (about) the same height --
	// rules out ledges and table edges.
	static const FVector2D Offsets[4] = { FVector2D(1, 0), FVector2D(-1, 0), FVector2D(0, 1), FVector2D(0, -1) };
	for (const FVector2D& Offset : Offsets)
	{
		const FVector Start = Hit.Location + FVector(Offset.X * Radius * 0.8f, Offset.Y * Radius * 0.8f, 30.0f);
		FHitResult FootHit;
		if (!World->LineTraceSingleByChannel(FootHit, Start, Start - FVector(0, 0, 60.0f), ECC_Visibility, Params)) { return false; }
		if (FMath::Abs(FootHit.Location.Z - Hit.Location.Z) > 12.0f) { return false; }
	}
	return true;
}

ABaseCharacter* ABasePlayerController::SpawnManagedCharacter(const FVector& FloorPoint)
{
	UWorld* World = GetWorld();
	if (!World) { return nullptr; }
	const UCapsuleComponent* Capsule = GetDefault<ABaseCharacter>()->GetCapsuleComponent();
	const float HalfHeight = Capsule ? Capsule->GetUnscaledCapsuleHalfHeight() : 88.0f;

	// Facing the camera that placed it.
	const FVector Location = FloorPoint + FVector(0.0f, 0.0f, HalfHeight + 1.0f);
	const FVector CameraLocation = PlayerCameraManager ? PlayerCameraManager->GetCameraLocation() : Location + FVector(100, 0, 0);
	FRotator Rotation = (CameraLocation - Location).Rotation();
	Rotation.Pitch = 0.0f;
	Rotation.Roll = 0.0f;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	ABaseCharacter* NewCharacter = World->SpawnActor<ABaseCharacter>(ABaseCharacter::StaticClass(), Location, Rotation, Params);
	if (!NewCharacter) { return nullptr; }

	NewCharacter->bRuntimeSpawned = true;
	NewCharacter->ApplyCharacterConfig(ModularHero::MakeDefaultConfig(FString::Printf(TEXT("Character_%d"), ++SpawnedCharacterCount)));
	return NewCharacter;
}

ABaseCharacter* ABasePlayerController::PlaceCharacterAt(const FVector& FloorPoint)
{
	ABaseCharacter* NewCharacter = SpawnManagedCharacter(FloorPoint);
	CancelPlacingCharacter();
	if (NewCharacter) { SelectCharacter(NewCharacter); }
	RefreshNameLabels();
	return NewCharacter;
}

void ABasePlayerController::HandleManagerClick()
{
	if (!bEditMode) { return; }
	// One click can arrive by more than one route in the same frame (the
	// click input action AND the actor OnClicked event) -- act on it once.
	if (LastClickFrame == GFrameCounter) { return; }
	LastClickFrame = GFrameCounter;
	UE_LOG(LogTemp, Log, TEXT("EditMode: click (assist=%d placing=%d)"), bAssistMode, bPlacingCharacter);

	if (bAssistMode)
	{
		FVector Origin, Direction;
		if (DeprojectMousePositionToWorld(Origin, Direction))
		{
			RecordAssistRay(Origin, Origin + Direction * 100000.0f);
		}
		CancelClaudeAssist();
		return;
	}

	if (bPlacingCharacter)
	{
		if (bDropPointValid) { PlaceCharacterAt(DropPoint); }
		return;
	}

	if (ABaseCharacter* Clicked = FindCharacterUnderCursor())
	{
		SelectCharacter(Clicked);
	}
}

ABaseCharacter* ABasePlayerController::FindCharacterUnderCursor() const
{
	// A click can land on the capsule (Pawn channel) or on something riding
	// on the character -- the weapon, the nose/mouth cosmetics that live on
	// the attached AFaceController -- so resolve whatever was hit up its
	// attachment chain, and try the Visibility channel too (that is what the
	// mesh and the attachments actually block).
	const auto Resolve = [](AActor* Actor) -> ABaseCharacter*
	{
		for (AActor* A = Actor; A; A = A->GetAttachParentActor())
		{
			if (ABaseCharacter* AsCharacter = Cast<ABaseCharacter>(A)) { return AsCharacter; }
		}
		return nullptr;
	};
	for (ECollisionChannel Channel : { ECC_Pawn, ECC_Visibility })
	{
		FHitResult Hit;
		if (GetHitResultUnderCursor(Channel, false, Hit))
		{
			if (ABaseCharacter* Found = Resolve(Hit.GetActor()))
			{
				return Found;   // the controlled character is selectable too (to edit yourself)
			}
		}
	}
	return nullptr;
}

void ABasePlayerController::Tick(float DeltaSeconds)
{
	KeepConsolePagesFitted();
	TickFreelookSafety();
	TickPendingFire(DeltaSeconds);
	TickPendingSwap(DeltaSeconds);
	TickAutoFire(DeltaSeconds);
	TickLaser(DeltaSeconds);
	TickScorches(DeltaSeconds);
	TickAmbientZone(DeltaSeconds);
	TickHandTune();
	TickTracers(DeltaSeconds);
	if (Brass) { Brass->Tick(DeltaSeconds); }
	TickMelee(DeltaSeconds);
	TickCarry(DeltaSeconds);
	Super::Tick(DeltaSeconds);
	if (bGroggy && !bGroggyArrived && GetPawn() && FVector::Dist2D(GetPawn()->GetActorLocation(), GroggyGoal) <= GroggyRadius)
	{
		// In the cabin: Hannah calls. The eyes clear and the inspect menu is handed over when the call ends.
		bGroggyArrived = true;
		ABaseCharacter* Hannah = nullptr;
		for (TActorIterator<ABaseCharacter> It(GetWorld()); It; ++It) { if (It->GetCharacterConfig().Name == TEXT("Hannah Martinez")) { Hannah = *It; break; } }
		if (!Hannah || !StartConversationTree(Hannah, TEXT("Hannah Call"))) { EndGroggy(true); bInspectEnabled = true; }
	}
	UpdateRemoteView();
	if (Sequence && Sequence->IsActive() && !IsPaused()) { Sequence->Tick(DeltaSeconds); }
	UpdateInspectTarget();
	if (InspectMenuWidget && InspectMenuWidget->IsInViewport()) { UpdateInspectMenuPosition(); }
	UpdateInspectMenuPosition();
	UpdateCallout();
	if (!bEditMode) { return; }

	// One-shot diagnostic: where did Slate actually put the EditTool?
	if (bLogEditToolGeometry && EditToolWidget)
	{
		const FGeometry& Geometry = EditToolWidget->GetCachedGeometry();
		const FVector2D Size = Geometry.GetAbsoluteSize();
		if (Size.X > 0.0f)
		{
			const FVector2D Pos = Geometry.GetAbsolutePosition();
			UE_LOG(LogTemp, Log, TEXT("EditMode: EditTool geometry pos=(%.0f, %.0f) size=(%.0f, %.0f) visible=%d"), Pos.X, Pos.Y, Size.X, Size.Y, EditToolWidget->IsVisible());
			bLogEditToolGeometry = false;
		}
	}

	// Clicks arrive via OnClickInput (fly mode: our own binding) or via the
	// possessed character forwarding its consumed click action to
	// NotifyEditClick -- never by polling the button state, which the
	// viewport doesn't reliably expose while the cursor is free.
	if (bPlacingCharacter)
	{
		bDropPointValid = FindDropPointUnderCursor(DropPoint, bDropPointHit);
		if (GhostActor)
		{
			const UCapsuleComponent* Capsule = GetDefault<ABaseCharacter>()->GetCapsuleComponent();
			const float HalfHeight = Capsule ? Capsule->GetUnscaledCapsuleHalfHeight() : 88.0f;
			GhostActor->SetActorHiddenInGame(!bDropPointHit);
			if (bDropPointHit)
			{
				const FVector CameraLocation = PlayerCameraManager ? PlayerCameraManager->GetCameraLocation() : DropPoint + FVector(100, 0, 0);
				FRotator Facing = (CameraLocation - DropPoint).Rotation();
				Facing.Pitch = 0.0f;
				Facing.Roll = 0.0f;
				GhostActor->SetActorLocationAndRotation(DropPoint + FVector(0.0f, 0.0f, HalfHeight + 1.0f), Facing);
			}
			GhostActor->SetValid(bDropPointValid);
		}
		CurrentMouseCursor = EMouseCursor::Crosshairs;
		return;
	}

	// Highlight and hand-cursor the character under the mouse (the click that
	// selects it comes through HandleManagerClick).
	ABaseCharacter* Hovered = FindCharacterUnderCursor();
	UpdateHoverHighlight(Hovered);
	CurrentMouseCursor = Hovered ? EMouseCursor::Hand : EMouseCursor::Default;
}

// ---- Inspectables --------------------------------------------------------------

void ABasePlayerController::EnsureInspectOutlineVolume()
{
	if (InspectOutlineVolume || !GetWorld()) { return; }
	UMaterialInterface* Outline = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Characters/PP_InspectOutline.PP_InspectOutline"));
	if (!Outline) { return; }
	// One unbound volume carrying the outline pass, so it applies whichever
	// camera is active (third person, first person, fly).
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	InspectOutlineVolume = GetWorld()->SpawnActor<APostProcessVolume>(APostProcessVolume::StaticClass(), FTransform::Identity, Params);
	if (!InspectOutlineVolume) { return; }
	InspectOutlineVolume->bUnbound = true;
	InspectOutlineVolume->Priority = 10.0f;
	UMaterialInstanceDynamic* OutlineMID = UMaterialInstanceDynamic::Create(Outline, this);
	if (OutlineMID) { OutlineMID->SetVectorParameterValue(TEXT("OutlineColor"), Crt::Green); }
	InspectOutlineVolume->Settings.WeightedBlendables.Array.Add(FWeightedBlendable(1.0f, OutlineMID ? static_cast<UMaterialInterface*>(OutlineMID) : Outline));
}

AActor* ABasePlayerController::ResolveInspectable(AActor* Hit, const UPrimitiveComponent* HitComponent) const
{
	// A door is only inspectable on its leaves: a hit on the frame is a hit on the wall.
	if (const ASlidingDoorActor* Door = Cast<ASlidingDoorActor>(Hit)) { if (HitComponent && !Door->IsLeaf(HitComponent)) { return nullptr; } }
	// The lift is inspectable at its PANEL from inside and at its doors from outside. A hit on
	// the car's wall or floor is a hit on a wall or a floor.
	if (const AElevatorActor* Lift = Cast<AElevatorActor>(Hit)) { if (!Lift->IsInspectPoint(HitComponent)) { return nullptr; } }
	// The thing under the reticle may be a part of a larger entity (a
	// character's weapon or face actor, a prop attached to a cart). Every
	// character is inspectable by nature (opt out with a "noinspect" tag);
	// anything else opts in with the InspectableTag.
	static const FName NoInspectTag(TEXT("noinspect"));
	for (AActor* A = Hit; A; A = A->GetAttachParentActor())
	{
		if (A->ActorHasTag(InspectableTag)) { return A; }
		if (A->IsA<ABaseCharacter>() && !A->ActorHasTag(NoInspectTag)) { return A; }
	}
	return nullptr;
}

void ABasePlayerController::UpdateInspectTarget()
{
	if (!bInspectEnabled && CVarInspect.GetValueOnGameThread() == 0)
	{
		if (InspectTarget.IsValid()) { SetInspectHighlight(InspectTarget.Get(), false); InspectTarget = nullptr; HideInspectMenu(); }
		return;
	}
	AActor* NewTarget = nullptr;
	APawn* P = GetPawn();
	if (!bEditMode && !bFlying && !IsInConversation() && !IsInCinematic() && P && PlayerCameraManager && GetWorld())
	{
		const FVector Origin = PlayerCameraManager->GetCameraLocation();
		const FVector End = Origin + PlayerCameraManager->GetCameraRotation().Vector() * InspectTraceRange;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(InspectTrace), true);
		Params.AddIgnoredActor(P);
		TArray<AActor*> Attached;
		P->GetAttachedActors(Attached, true, true);
		Params.AddIgnoredActors(Attached);

		// Nearest hit across the two channels the world's things block.
		FHitResult Best;
		bool bHit = false;
		for (ECollisionChannel Channel : { ECC_Visibility, ECC_Pawn })
		{
			FHitResult Hit;
			if (GetWorld()->LineTraceSingleByChannel(Hit, Origin, End, Channel, Params) && (!bHit || Hit.Distance < Best.Distance)) { Best = Hit; bHit = true; }
		}
		if (bHit)
		{
			if (AActor* Found = ResolveInspectable(Best.GetActor(), Best.GetComponent()))
			{
				InspectAnchorWorld = Best.ImpactPoint; bAnchorValid = true;
				// Reach is measured from the capsule, not the camera (a third-
				// person camera sits well behind the character).
				float Radius = 0.0f, HalfHeight = 0.0f;
				P->GetSimpleCollisionCylinder(Radius, HalfHeight);
				FVector ToPoint = Best.ImpactPoint - P->GetActorLocation();
				ToPoint.Z = FMath::Max(0.0f, FMath::Abs(ToPoint.Z) - HalfHeight);
				if (ToPoint.Size() - Radius <= InspectDistance) { NewTarget = Found; }
			}
		}
	}

	// Hysteresis: a raised panel's edges let the trace slip onto the wall behind for a frame
	// as the reticle crosses it, which flickered the menu. Losing the target (no inspectable
	// under the reticle) only takes effect after InspectGraceSeconds; a different inspectable
	// still takes over at once.
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (NewTarget && NewTarget == InspectTarget.Get()) { InspectLastSeen = Now; }
	else if (!NewTarget && InspectTarget.IsValid() && Now - InspectLastSeen < InspectGraceSeconds) { NewTarget = InspectTarget.Get(); }
	else if (NewTarget) { InspectLastSeen = Now; }
	if (!NewTarget) { bAnchorValid = false; }
	// Tiers and dwell. A thing with a real action (Open, Use, Sit, Take, Read, Talk ...) gets the
	// outline and the menu after a short dwell, so a sweep across a shelf does not strobe. A
	// thing with only Inspect to offer -- flavour -- never gets the outline; after a longer
	// dwell a quiet name tag appears, and E still reads its line. Hold Scan for the whole picture.
	if (NewTarget != InspectCandidate.Get())
	{
		InspectCandidate = NewTarget; InspectCandidateSince = Now;
		bCandidateFlavour = NewTarget && IsFlavourInteractable(NewTarget);
	}
	const float Dwell = bCandidateFlavour ? InspectDwellFlavourSeconds : InspectDwellSeconds;
	const ABaseCharacter* Aimer = Cast<ABaseCharacter>(GetPawn());
	AActor* Want = (NewTarget && NewTarget != Carried.Get() && !Screens.IsOpen(EScreen::Terminal) && Now - InspectCandidateSince >= Dwell && !(Aimer && Aimer->IsAiming())) ? NewTarget : nullptr;   // no menu down the sights, none on what is in the hand, none at a terminal
	if (Want != InspectTarget.Get())
	{
		if (!ScanLit.Contains(InspectTarget)) { SetInspectHighlight(InspectTarget.Get(), false); }
		InspectTarget = Want;
		if (Want && !bCandidateFlavour) { SetInspectHighlight(Want, true); }
		RefreshInspectMenu(Want);
	}
	TickScan(Now);
}

bool ABasePlayerController::IsFlavourInteractable(AActor* Target)
{
	if (!Target) { return false; }
	FString Name, Description; TArray<FString> Actions;
	DescribeInspectable(Target, Name, Description, Actions);
	return Actions.Num() == 1 && Actions[0] == TEXT("Inspect");
}

void ABasePlayerController::SetScanHeld(bool bHeld)
{
	if (bScanHeld == bHeld) { return; }
	bScanHeld = bHeld;
	ScanNextRefresh = 0.0f;
	if (!bHeld)
	{
		for (const TWeakObjectPtr<AActor>& A : ScanLit) { if (A.IsValid() && A != InspectTarget) { SetInspectHighlight(A.Get(), false); } }
		ScanLit.Reset();
	}
}

void ABasePlayerController::TickScan(float Now)
{
	if (!bScanHeld || Now < ScanNextRefresh || !GetWorld() || !PlayerCameraManager) { return; }
	ScanNextRefresh = Now + 0.25f;
	const APawn* P = GetPawn();
	const FVector Eye = PlayerCameraManager->GetCameraLocation();
	const FVector Fwd = PlayerCameraManager->GetCameraRotation().Vector();
	const FVector From = P ? P->GetActorLocation() : Eye;
	static const FName NoInspectTag(TEXT("noinspect"));
	TArray<TWeakObjectPtr<AActor>> Lit;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* A = *It;
		if (A == P || A->IsHidden()) { continue; }
		const bool bTagged = A->ActorHasTag(InspectableTag) || (A->IsA<ABaseCharacter>() && !A->ActorHasTag(NoInspectTag));
		if (!bTagged) { continue; }
		if (FVector::Dist(A->GetActorLocation(), From) > ScanRange) { continue; }
		if (FVector::DotProduct((A->GetActorLocation() - Eye).GetSafeNormal(), Fwd) < 0.15f) { continue; }   // in front of the camera
		if (IsFlavourInteractable(A)) { continue; }
		Lit.Add(A);
	}
	for (const TWeakObjectPtr<AActor>& A : ScanLit) { if (A.IsValid() && !Lit.Contains(A) && A != InspectTarget) { SetInspectHighlight(A.Get(), false); } }
	for (const TWeakObjectPtr<AActor>& A : Lit) { if (A.IsValid() && !ScanLit.Contains(A)) { SetInspectHighlight(A.Get(), true); } }
	ScanLit = MoveTemp(Lit);
}

void ABasePlayerController::SetInspectHighlight(AActor* Target, bool bOn)
{
	if (!Target) { return; }
	if (AInspectSurface* Surface = Cast<AInspectSurface>(Target)) { Surface->SetHovered(bOn); return; }   // a marker slab: shown only while hovered
	if (ASlidingDoorActor* Door = Cast<ASlidingDoorActor>(Target))
	{
		TInlineComponentArray<UMeshComponent*> Meshes(Door);
		for (UMeshComponent* Mesh : Meshes) { const bool bLeaf = Door->IsLeaf(Mesh); Mesh->SetCustomDepthStencilValue(bOn && bLeaf ? 1 : 0); Mesh->SetRenderCustomDepth(bOn && bLeaf); }
		return;
	}
	// Every mesh of the entity and of whatever rides on it (weapon, face
	// actor, nested props) into the custom depth stencil -> one silhouette.
	TArray<AActor*> Actors;
	Actors.Add(Target);
	Target->GetAttachedActors(Actors, false, true);
	for (AActor* A : Actors)
	{
		TInlineComponentArray<UMeshComponent*> Meshes(A);
		for (UMeshComponent* Mesh : Meshes)
		{
			Mesh->SetCustomDepthStencilValue(bOn ? 1 : 0);
			Mesh->SetRenderCustomDepth(bOn);
		}
	}
}

// ---- Conversations ----------------------------------------------------------

bool ABasePlayerController::StartConversation(ABaseCharacter* With)
{
	return With && StartConversationTree(With, With->GetCharacterConfig().Name);
}

bool ABasePlayerController::StartConversationTree(ABaseCharacter* With, const FString& TreeName)
{
	if (!With || IsInConversation()) { return false; }
	const FString& Name = With->GetCharacterConfig().Name;
	if (!ConversationFile::Exists(TreeName) || !ConversationFile::Load(TreeName, Conversation)) { return false; }

	ConversationPartner = With;
	ConversationHistory.Reset();
	HideInspectMenu();
	SetInspectHighlight(InspectTarget.Get(), false);
	InspectTarget = nullptr;

	ShowConversationPanel(Name);
	ConversationWidget->ClearHistory();
	if (Conversation.bRemote) { ShowRemoteView(With); }

	// The NPC turns its attention to the player; the player stands still.
	// On a remote call the caller looks into their camera instead (the
	// remote view pins their gaze) and the player's camera is left alone.
	if (APawn* P = GetPawn())
	{
		if (!bRemoteConversation) { With->SetLookAtTarget(P->GetActorLocation() + FVector(0, 0, 60.0f)); }
		SetIgnoreMoveInput(true);
		SetIgnoreLookInput(true);
	}
	if (!bRemoteConversation) { UpdateViewCamera(EViewFrame::Conversation); }
	EnterConversationNode(Conversation.StartNode);
	return true;
}

void ABasePlayerController::EndConversation()
{
	if (!IsInConversation()) { return; }
	if (bGroggy && bGroggyArrived) { EndGroggy(true); bInspectEnabled = true; }   // the cabin call is over: eyes clear, the menus are the player's
	if (ABaseCharacter* With = ConversationPartner.Get()) { With->ClearLookAtTarget(); }
	ConversationPartner = nullptr;
	ConversationNodeId.Reset();
	ConversationChoiceIndices.Reset();
	ConversationChoiceTexts.Reset();
	HideConversationPanel();
	HideRemoteView();
	bRemoteConversation = false;
	StopConversationVoice();
	SetIgnoreMoveInput(false);
	SetIgnoreLookInput(false);
	UpdateViewCamera(EViewFrame::None);
	ApplyInputMode();
}

// ---- Panel / cinematic plumbing shared by conversations and sequences ------

void ABasePlayerController::ShowConversationPanel(const FString& Speaker)
{
	if (!ConversationWidget)
	{
		ConversationWidget = CreateWidget<UConversationWidget>(this, UConversationWidget::StaticClass());
		ConversationWidget->SetOwnerController(this);
	}
	if (!Speaker.IsEmpty())
	{
		// The description comes from whichever character carries that name.
		FString Description;
		for (TActorIterator<ABaseCharacter> It(GetWorld()); It; ++It)
		{
			if (It->MatchesConfigName(Speaker)) { Description = It->GetCharacterConfig().Description; break; }
		}
		ConversationWidget->SetSpeaker(Speaker, Description);
	}
	if (!ConversationWidget->IsInViewport())
	{
		ConversationWidget->ClearHistory();
		// Right third of the screen, full height, inset from the edges. Set
		// the whole slot up front: SetPositionInViewport & co. silently reset
		// the anchors to (0,0), which is why the panel used to sit top-left.
		if (UGameViewportSubsystem* Viewport = UGameViewportSubsystem::Get())
		{
			FGameViewportWidgetSlot PanelSlot;
			PanelSlot.ZOrder = 70;
			PanelSlot.Anchors = FAnchors(2.0f / 3.0f, 0.0f, 1.0f, 1.0f);
			PanelSlot.Offsets = FMargin(ConversationPanelPadding, ConversationPanelPadding, ConversationPanelPadding, ConversationPanelPadding);
			PanelSlot.Alignment = FVector2D::ZeroVector;
			Viewport->AddWidget(ConversationWidget, PanelSlot);
		}
	}
	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
	SetShowMouseCursor(true);
	if (UGameViewportClient* Viewport = GetLocalPlayer() ? GetLocalPlayer()->ViewportClient : nullptr) { Viewport->SetMouseCaptureMode(EMouseCaptureMode::NoCapture); }
}

void ABasePlayerController::HideConversationPanel()
{
	if (ConversationWidget && ConversationWidget->IsInViewport()) { ConversationWidget->RemoveFromParent(); }
}

void ABasePlayerController::ConversationSay(const FString& Speaker, const FString& Text, bool bPlayer)
{
	ConversationHistory.Add(FString::Printf(TEXT("%s: %s"), *Speaker, *Text));
	if (ConversationWidget) { ConversationWidget->AddLine(Speaker, Text, bPlayer); }
}

void ABasePlayerController::SetConversationChoices(const TArray<FString>& Choices)
{
	ConversationChoiceTexts = Choices;
	if (ConversationWidget) { ConversationWidget->SetChoices(Choices); }
}

void ABasePlayerController::SetCinematicCamera(const FVector& Eye, const FVector& LookAt, float Fov, float Blend)
{
	if (!GetWorld()) { return; }
	ACameraActor* Previous = FaceCamera;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	FaceCamera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), Eye, (LookAt - Eye).Rotation(), Params);
	if (!FaceCamera) { FaceCamera = Previous; return; }
	if (FaceCamera->GetCameraComponent()) { FaceCamera->GetCameraComponent()->SetFieldOfView(Fov); }
	SetViewTargetWithBlend(FaceCamera, Blend, VTBlend_EaseInOut, 2.0f);
	if (Previous) { Previous->SetLifeSpan(FMath::Max(0.1f, Blend + 0.5f)); }
}

void ABasePlayerController::BeginCinematic()
{
	if (bCinematic) { return; }
	bCinematic = true;
	EndConversation();
	HideInspectMenu();
	SetInspectHighlight(InspectTarget.Get(), false);
	InspectTarget = nullptr;
	SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);
	if (AHUD* Hud = GetHUD()) { bHudWasShown = Hud->bShowHUD; Hud->bShowHUD = false; }
	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
	SetShowMouseCursor(true);
}

void ABasePlayerController::EndCinematic(float CameraBlend, bool bKeepBlink)
{
	if (!bCinematic) { return; }
	bCinematic = false;
	HideConversationPanel();
	if (!bKeepBlink) { HideBlinkOverlay(); }
	StopConversationVoice();
	ConversationChoiceTexts.Reset();
	SetIgnoreMoveInput(false);
	SetIgnoreLookInput(false);
	if (AHUD* Hud = GetHUD()) { Hud->bShowHUD = bHudWasShown; }
	if (APawn* P = GetPawn()) { SetViewTargetWithBlend(P, CameraBlend, VTBlend_EaseInOut, 2.0f); }
	if (FaceCamera) { FaceCamera->SetLifeSpan(FMath::Max(0.1f, CameraBlend + 0.5f)); FaceCamera = nullptr; }
	ApplyInputMode();
	// The panel's buttons held Slate focus; hand it straight back to the viewport rather than
	// waiting for the next event the viewport happens to receive.
	FSlateApplication::Get().SetAllUserFocusToGameViewport();
}

void ABasePlayerController::BeginGroggy(const FVector& Goal, float Radius, float Speed)
{
	bGroggy = true; bGroggyArrived = false; GroggyGoal = Goal; GroggyRadius = FMath::Max(30.0f, Radius);
	if (PlayerCameraManager)
	{
		GroggyPitchMin = PlayerCameraManager->ViewPitchMin; GroggyPitchMax = PlayerCameraManager->ViewPitchMax;
		PlayerCameraManager->ViewPitchMin = -5.0f; PlayerCameraManager->ViewPitchMax = 5.0f;
	}
	FRotator R = GetControlRotation(); R.Pitch = 0.0f; R.Roll = 0.0f; SetControlRotation(R);
	// Whatever the intro stacked on the input-ignore counters is over now.
	ResetIgnoreMoveInput(); ResetIgnoreLookInput();
	if (ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn())) { Me->SetGroggy(true, Speed); }
}

void ABasePlayerController::EndGroggy(bool bFadeBlink)
{
	if (!bGroggy) { return; }
	bGroggy = false;
	if (PlayerCameraManager) { PlayerCameraManager->ViewPitchMin = GroggyPitchMin; PlayerCameraManager->ViewPitchMax = GroggyPitchMax; }
	if (ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn())) { Me->SetGroggy(false, 0.0f); }
	if (bFadeBlink) { FadeOutBlinkOverlay(3.0f); } else { HideBlinkOverlay(); }
}

// ---- Sequences ---------------------------------------------------------------

static TAutoConsoleVariable<int32> CVarSkipNewGameIntro(
	TEXT("RepliCan.SkipNewGameIntro"), 0,
	TEXT("1 = don't play the NewGame sequence when a new game starts (debugging)."));

void ABasePlayerController::PlaySequence(const FString& Name)
{
	FSequence Loaded;
	if (!SequenceFile::Load(Name, Loaded))
	{
		UE_LOG(LogTemp, Warning, TEXT("Sequence: '%s' not found or empty (%s)"), *Name, *SequenceFile::GetPath(Name));
		return;
	}
	if (Sequence && Sequence->IsActive()) { Sequence->Skip(); }
	if (!Sequence) { Sequence = NewObject<USequenceDirector>(this); }
	Sequence->Start(this, Loaded);
}

bool ABasePlayerController::CanLeaveConversation() const
{
	return IsInConversation() && !Conversation.bUnskippable && !(Sequence && Sequence->IsUnskippable());
}

bool ABasePlayerController::CanSkipSequence() const
{
	return Sequence && Sequence->IsActive() && !Sequence->IsUnskippable();
}

void ABasePlayerController::SkipSequence()
{
	if (Sequence && Sequence->IsActive()) { Sequence->Skip(); }
}

FString ABasePlayerController::DescribeInput() const
{
	FString Out;
	FSlateApplication& Slate = FSlateApplication::Get();
	TSharedPtr<SWidget> Focused = Slate.GetUserFocusedWidget(0);
	Out += FString::Printf(TEXT("focused=%s"), Focused.IsValid() ? *Focused->GetTypeAsString() : TEXT("none"));
	{
		TSharedPtr<SViewport> ViewportWidget = (GetLocalPlayer() && GetLocalPlayer()->ViewportClient) ? GetLocalPlayer()->ViewportClient->GetGameViewportWidget() : nullptr;
		Out += FString::Printf(TEXT(" viewportHasFocus=%d"), (ViewportWidget.IsValid() && ViewportWidget->HasUserFocus(0).IsSet()) ? 1 : 0);
	}
	Out += FString::Printf(TEXT(" moveIgnored=%d lookIgnored=%d showCursor=%d cinematic=%d groggy=%d"), IsMoveInputIgnored() ? 1 : 0, IsLookInputIgnored() ? 1 : 0, bShowMouseCursor ? 1 : 0, bCinematic ? 1 : 0, bGroggy ? 1 : 0);
	Out += FString::Printf(TEXT(" stack=%d"), CurrentInputStack.Num());
	if (const APawn* P = GetPawn())
	{
		Out += FString::Printf(TEXT(" pawn=%s pawnInputComp=%d pawnInputEnabled=%d pcInputEnabled=%d"), *P->GetName(), P->InputComponent ? 1 : 0, P->InputEnabled() ? 1 : 0, InputEnabled() ? 1 : 0);
		if (const ABaseCharacter* C = Cast<ABaseCharacter>(P)) { Out += TEXT(" | ") + C->DescribeMoveGuards(); }
	}
	if (PlayerInput) { Out += FString::Printf(TEXT(" W=%d"), PlayerInput->IsPressed(EKeys::W) ? 1 : 0); }
	if (const ULocalPlayer* LP = GetLocalPlayer())
	{
		if (const UEnhancedInputLocalPlayerSubsystem* Sub = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			if (const UInputMappingContext* Context = LoadObject<UInputMappingContext>(nullptr, MappingContextPath)) { Out += FString::Printf(TEXT(" mappingContext=%d"), Sub->HasMappingContext(Context) ? 1 : 0); }
		}
	}
	return Out;
}

void ABasePlayerController::OpenTransfer(ALootBoxActor* Box)
{
	if (!Box || Screens.IsOpen(EScreen::Transfer) || Screens.IsOpen(EScreen::PauseMenu)) { return; }
	HideInspectMenu();
	if (!TransferWidget) { TransferWidget = CreateWidget<UInventoryTransferWidget>(this, UInventoryTransferWidget::StaticClass()); }
	TransferWidget->Open(this, Box);
	PlaceConsolePage(TransferWidget, 90);
	Screens.Open(EScreen::Transfer);
	SetPause(true);
	ApplyInputMode();
}

void ABasePlayerController::CloseTransfer()
{
	if (!Screens.IsOpen(EScreen::Transfer)) { return; }
	Screens.Close(EScreen::Transfer);
	if (TransferWidget && TransferWidget->IsInViewport()) { TransferWidget->RemoveFromParent(); }
	SetPause(false);
	ApplyInputMode();
}

void ABasePlayerController::FadeOutBlinkOverlay(float Seconds)
{
	if (!BlinkOverlay || !BlinkOverlay->IsInViewport()) { return; }
	FBlinkParams P;
	P.OpenMin = 1.0f; P.OpenMax = 1.0f; P.ClosedSeconds = 0.01f; P.OpenSeconds = 999.0f;
	P.DarkStart = 0.12f; P.DarkEnd = 0.0f; P.DarkSeconds = Seconds; P.LiftSeconds = Seconds * 0.7f;
	BlinkOverlay->Configure(P);
	GetWorldTimerManager().SetTimer(BlinkFadeTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { HideBlinkOverlay(); }), Seconds + 0.3f, false);
}

void ABasePlayerController::DebugKey(const FString& KeyName, bool bDown)
{
	const FKey Key(*KeyName);
	const FInputDeviceId Device = FInputDeviceId::CreateFromInternalId(0);   // the keyboard
	InputKey(FInputKeyEventArgs(nullptr, Device, Key, bDown ? IE_Pressed : IE_Released, bDown ? 1.0f : 0.0f, false, FPlatformTime::Cycles64()));
}

static TAutoConsoleVariable<int32> CVarPlayAtCamera(
	TEXT("RepliCan.PlayAtCamera"), 1,
	TEXT("Editor: 1 = PIE starts the pawn at the level viewport's camera instead of the player start."));

void ABasePlayerController::PlaceAtEditorCamera()
{
#if WITH_EDITOR
	if (CVarPlayAtCamera.GetValueOnGameThread() == 0 || !GEditor || !GetWorld() || GetWorld()->WorldType != EWorldType::PIE) { return; }
	// The viewport the play session came from; its camera is where the builder was looking.
	// A perspective viewport only: an orthographic one (top, front, side) sits at the origin and
	// would drop the pawn into the void. The active one wins when several are open.
	FLevelEditorViewportClient* View = nullptr;
	for (FLevelEditorViewportClient* Client : GEditor->GetLevelViewportClients())
	{
		if (!Client || !Client->IsPerspective()) { continue; }
		if (!View) { View = Client; }
		if (Client->Viewport == GEditor->GetActiveViewport()) { View = Client; break; }
	}
	if (!View) { return; }
	const FVector Where = View->GetViewLocation();
	const FRotator Facing(0.0f, View->GetViewRotation().Yaw, 0.0f);
	// Next tick: the pawn is possessed and the movement component settled by then.
	GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, Where, Facing]()
	{
		APawn* P = GetPawn();
		if (!P) { return; }
		P->SetActorLocationAndRotation(Where, Facing, false, nullptr, ETeleportType::TeleportPhysics);
		SetControlRotation(Facing);
		SetDiagNoteTimed(FString::Printf(TEXT("Play at camera: %s"), *Where.ToCompactString()), 6.0f);
	}));
#endif
}

void ABasePlayerController::SetDiagNoteTimed(const FString& Text, float Seconds)
{
	SetDiagNote(Text);
	if (!GetWorld()) { return; }
	GetWorldTimerManager().SetTimer(DiagNoteFadeTimer, FTimerDelegate::CreateWeakLambda(this, [this, Text]() { if (DiagNote == Text) { SetDiagNote(FString()); } }), Seconds, false);
}

void ABasePlayerController::NoteTextureUnderReticle()
{
	if (!GetWorld() || !PlayerCameraManager) { return; }
	const FVector Start = PlayerCameraManager->GetCameraLocation();
	const FVector End = Start + PlayerCameraManager->GetActorForwardVector() * 5000.0f;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TextureNote), true);
	if (GetPawn()) { Params.AddIgnoredActor(GetPawn()); }
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params) || !Hit.GetComponent()) { SetDiagNoteTimed(TEXT("T: nothing under the reticle"), 10.0f); return; }
	UPrimitiveComponent* Comp = Hit.GetComponent();
	FString Mesh;
	if (const UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Comp)) { Mesh = SMC->GetStaticMesh() ? SMC->GetStaticMesh()->GetName() : TEXT("-"); }
	else if (const USkeletalMeshComponent* SKC = Cast<USkeletalMeshComponent>(Comp)) { Mesh = SKC->GetSkeletalMeshAsset() ? SKC->GetSkeletalMeshAsset()->GetName() : TEXT("-"); }
	else { Mesh = Comp->GetClass()->GetName(); }
	// The material at the hit face's section, then whatever textures it samples.
	UMaterialInterface* Mat = Comp->GetMaterial(0);
	if (Hit.FaceIndex >= 0) { int32 Section = 0; if (UMaterialInterface* M = Comp->GetMaterialFromCollisionFaceIndex(Hit.FaceIndex, Section)) { Mat = M; } }
	TArray<FString> Tex;
	if (Mat)
	{
		TArray<UTexture*> Textures;
		Mat->GetUsedTextures(Textures);
		for (UTexture* T : Textures) { if (T) { Tex.AddUnique(T->GetName()); } }
	}
	const FString Note = FString::Printf(TEXT("%s | %s | %s | %s"), *Hit.GetActor()->GetActorNameOrLabel(), *Mesh, Mat ? *Mat->GetName() : TEXT("no material"), Tex.Num() ? *FString::Join(Tex, TEXT(", ")) : TEXT("no textures"));
	SetDiagNoteTimed(Note, 10.0f);
	UE_LOG(LogTemp, Log, TEXT("TextureNote: %s"), *Note);
}

// Synty ships each pack's atlas as a family: M_PolygonSciFiSpace_01_A, _01_B, and so on, where
// the letter is a whole-palette recolour. Which letter turns a given mesh a given colour cannot
// be read off the name, because it depends where that mesh's UVs land in the atlas -- so the only
// way to choose is to look. This walks the family in place, in the room, with the lighting that
// will actually be used.
//
// The folder layout is not uniform: the base variant sits in Materials/ while the rest live in
// Materials/Alternates/, so each candidate is tried in both places.
static UMaterialInterface* FindPaletteVariant(const FString& Directory, const FString& BaseName, int32 Group, TCHAR Letter)
{
	const FString Name = FString::Printf(TEXT("%s_%02d_%c"), *BaseName, Group, Letter);
	FString Parent = Directory;
	if (Parent.EndsWith(TEXT("/Alternates"))) { Parent = Parent.LeftChop(11); }
	const FString Candidates[3] = { Directory, Parent + TEXT("/Alternates"), Parent };
	for (const FString& Dir : Candidates)
	{
		const FString Path = FString::Printf(TEXT("%s/%s.%s"), *Dir, *Name, *Name);
		if (UMaterialInterface* Found = LoadObject<UMaterialInterface>(nullptr, *Path)) { return Found; }
	}
	return nullptr;
}

void ABasePlayerController::CycleMaterialUnderReticle()
{
	if (!GetWorld() || !PlayerCameraManager) { return; }
	// FROM THE AIM, NOT THE CAMERA. The reticle is not the middle of the camera: it is the
	// middle of the camera only when nothing else is going on. Hold the freelook key -- which is
	// ALT, the same ALT in this action's own chord -- and the camera swings while the aim stays
	// put, so a trace down the camera's forward vector goes somewhere the player is not pointing
	// and reports "nothing under the reticle" while the reticle is sitting on a wall. Third
	// person offsets the camera from the shoulder and does the same thing more quietly.
	FVector Start = PlayerCameraManager->GetCameraLocation();
	FRotator Dir = PlayerCameraManager->GetCameraRotation();
	if (ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn()))
	{
		Start = Me->GetAimOrigin();
		Dir = Me->GetAimRotation();
	}
	const FVector End = Start + Dir.Vector() * 5000.0f;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TextureCycle), true);
	if (GetPawn()) { Params.AddIgnoredActor(GetPawn()); }
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params) || !Hit.GetComponent())
	{
		SetDiagNoteTimed(TEXT("Cycle palette: nothing under the reticle"), 6.0f); return;
	}
	UPrimitiveComponent* Comp = Hit.GetComponent();

	// The face the reticle is on decides WHICH slot is swapped: a wall piece and its trim are
	// different sections of one component, and swapping slot 0 would change the wrong thing.
	int32 Slot = 0;
	UMaterialInterface* Mat = Comp->GetMaterial(0);
	if (Hit.FaceIndex >= 0)
	{
		int32 Section = 0;
		if (UMaterialInterface* M = Comp->GetMaterialFromCollisionFaceIndex(Hit.FaceIndex, Section)) { Mat = M; Slot = Section; }
	}
	const FString HitOn = Hit.GetActor() ? Hit.GetActor()->GetActorNameOrLabel() : TEXT("?");
	if (!Mat) { SetDiagNoteTimed(FString::Printf(TEXT("Cycle palette: no material on %s"), *HitOn), 6.0f); return; }

	// Split "<anything>_<NN>_<L>" into its parts. Anything that does not match is not one of the
	// versioned atlases and is left alone rather than guessed at.
	const FString Name = Mat->GetName();
	FString Base; int32 Group = 0; TCHAR Letter = 0;
	{
		int32 LastUnderscore = INDEX_NONE;
		if (!Name.FindLastChar(TEXT('_'), LastUnderscore) || LastUnderscore < 4 || Name.Len() - LastUnderscore != 2)
		{
			SetDiagNoteTimed(FString::Printf(TEXT("Cycle palette: %s on %s is not a versioned Synty material"), *Name, *HitOn), 6.0f); return;
		}
		Letter = Name[LastUnderscore + 1];
		const FString Head = Name.Left(LastUnderscore);
		int32 GroupUnderscore = INDEX_NONE;
		if (!Head.FindLastChar(TEXT('_'), GroupUnderscore) || Head.Len() - GroupUnderscore != 3)
		{
			SetDiagNoteTimed(FString::Printf(TEXT("Cycle palette: %s on %s is not a versioned Synty material"), *Name, *HitOn), 6.0f); return;
		}
		Group = FCString::Atoi(*Head.RightChop(GroupUnderscore + 1));
		Base = Head.Left(GroupUnderscore);
	}
	if (!FChar::IsAlpha(Letter)) { SetDiagNoteTimed(FString::Printf(TEXT("Cycle palette: %s on %s has no letter to step"), *Name, *HitOn), 6.0f); return; }

	const FString Directory = FPackageName::GetLongPackagePath(Mat->GetPathName());

	// Load EVERY variant in the family the first time one is touched, and keep them referenced.
	// A material that has never been used has no compiled shader, and the renderer draws black
	// until one exists -- which is why stepping through the soldiers showed a run of black
	// suits. Loading them together starts every compile at once, so the wait happens once, at
	// the first press, instead of once per variant. The cache also stops them being collected
	// again between presses.
	static TMap<FString, TArray<TObjectPtr<UMaterialInterface>>> Warmed;
	const FString FamilyKey = FString::Printf(TEXT("%s|%s|%02d"), *Directory, *Base, Group);
	bool bWarmedNow = false;
	if (!Warmed.Contains(FamilyKey))
	{
		TArray<TObjectPtr<UMaterialInterface>>& Family = Warmed.Add(FamilyKey);
		for (int32 i = 0; i < 8; ++i)
		{
			if (UMaterialInterface* V = FindPaletteVariant(Directory, Base, Group, static_cast<TCHAR>(TEXT('A') + i)))
			{
				Family.Add(V);
			}
		}
		bWarmedNow = Family.Num() > 1;
		if (bWarmedNow)
		{
			UE_LOG(LogTemp, Log, TEXT("MaterialCycle: warmed %d variants of %s_%02d"), Family.Num(), *Base, Group);
		}
	}

	// Walk forward from the current letter and take the first that exists, wrapping round. Only
	// letters that resolve are visited, so a family with gaps still cycles cleanly.
	const TCHAR First = TEXT('A');
	const int32 Span = 8;   // A..H covers every Synty pack seen so far
	const int32 CurrentIndex = FChar::ToUpper(Letter) - First;
	for (int32 Step = 1; Step <= Span; ++Step)
	{
		const TCHAR Next = static_cast<TCHAR>(First + (CurrentIndex + Step) % Span);
		if (UMaterialInterface* Variant = FindPaletteVariant(Directory, Base, Group, Next))
		{
			if (Variant == Mat) { continue; }
			Comp->SetMaterial(Slot, Variant);
			SetDiagNoteTimed(FString::Printf(TEXT("%s  slot %d:  %s  ->  %s%s"),
				*Hit.GetActor()->GetActorNameOrLabel(), Slot, *Name, *Variant->GetName(),
				bWarmedNow ? TEXT("   (compiling the family, give it a moment)") : TEXT("")), 10.0f);
			UE_LOG(LogTemp, Log, TEXT("MaterialCycle: %s slot %d %s -> %s"), *Hit.GetActor()->GetActorNameOrLabel(), Slot, *Name, *Variant->GetName());
			return;
		}
	}
	SetDiagNoteTimed(FString::Printf(TEXT("Cycle palette: no other variant of %s found"), *Name), 6.0f);
}

void ABasePlayerController::SetDiagNote(const FString& Text)
{
	// Shown as the top line of the metrics HUD (MetricsWidget), amber; the HUD is raised if it is down.
	DiagNote = Text;
	if (!Text.IsEmpty() && !(MetricsWidget && MetricsWidget->IsInViewport())) { ToggleMetrics(); }
}

void ABasePlayerController::RefocusViewport()
{
	ApplyInputMode();
	FSlateApplication::Get().SetAllUserFocusToGameViewport();
}

void ABasePlayerController::SkipAhead()
{
	if (Sequence && Sequence->IsActive()) { Sequence->SkipAhead(); }
}

bool ABasePlayerController::CanSkipAhead() const
{
	return Sequence && Sequence->CanSkipAhead();
}

void ABasePlayerController::AdvanceSequence()
{
	if (Sequence && Sequence->IsActive()) { Sequence->Advance(); }
}

bool ABasePlayerController::IsInCinematic() const
{
	return bCinematic;
}

void ABasePlayerController::ShowBlinkOverlay(const FBlinkParams& Params)
{
	if (!BlinkOverlay)
	{
		BlinkOverlay = CreateWidget<UBlinkOverlayWidget>(this, UBlinkOverlayWidget::StaticClass());
	}
	BlinkOverlay->Configure(Params);
	if (!BlinkOverlay->IsInViewport()) { BlinkOverlay->AddToViewport(60); }   // under the conversation panel (70) so the text stays readable
}

void ABasePlayerController::HideBlinkOverlay()
{
	if (BlinkOverlay && BlinkOverlay->IsInViewport()) { BlinkOverlay->RemoveFromParent(); }
}

float ABasePlayerController::GetBlinkOpenness() const
{
	return (BlinkOverlay && BlinkOverlay->IsInViewport()) ? BlinkOverlay->GetOpenness() : -1.0f;
}

int32 ABasePlayerController::GetBlinkCount() const
{
	return (BlinkOverlay && BlinkOverlay->IsInViewport()) ? BlinkOverlay->GetBlinkCount() : 0;
}

void ABasePlayerController::FadeAmbient(float Level, float Seconds)
{
	if (Ambient) { Ambient->SetFade(Level, Seconds); }
}

FString ABasePlayerController::GetAmbientState() const
{
	return Ambient ? Ambient->Describe() : TEXT("none");
}

FString ABasePlayerController::GetSequenceState() const
{
	return Sequence ? Sequence->Describe() : TEXT("idle");
}

void ABasePlayerController::EnterConversationNode(const FString& NodeId)
{
	ABaseCharacter* With = ConversationPartner.Get();
	const FConversationNode* Node = Conversation.Nodes.Find(NodeId);
	if (!With || !Node) { EndConversation(); return; }
	ConversationNodeId = NodeId;

	// The NPC's line for this node.
	if (Node->Lines.Num() > 0)
	{
		const int32 LineIndex = FMath::RandRange(0, Node->Lines.Num() - 1);
		const FString& Line = Node->Lines[LineIndex];
		ConversationHistory.Add(FString::Printf(TEXT("%s: %s"), *Conversation.Character, *Line));
		if (ConversationWidget) { ConversationWidget->AddLine(Conversation.Character, Line, false); }
		const float Spoken = PlayVoiceLine(With, NodeId, LineIndex);
		ShowCallout(With, Line, FMath::Max(6.0f, Spoken + 1.0f), true);
	}

	// The player's options: skip "once" choices already taken and choices
	// whose flag condition fails.
	ConversationChoiceIndices.Reset();
	ConversationChoiceTexts.Reset();
	for (int32 i = 0; i < Node->Choices.Num() && ConversationChoiceTexts.Num() < 9; ++i)
	{
		const FConversationChoice& Choice = Node->Choices[i];
		if (Choice.bOnce && ConversationChoicesTaken.Contains(FString::Printf(TEXT("%s/%s/%d"), *Conversation.Character, *NodeId, i))) { continue; }
		if (!Choice.RequireFlag.IsEmpty())
		{
			const bool bNegate = Choice.RequireFlag.StartsWith(TEXT("!"));
			const bool bHas = ConversationFlags.Contains(bNegate ? Choice.RequireFlag.Mid(1) : Choice.RequireFlag);
			if (bHas == bNegate) { continue; }
		}
		ConversationChoiceIndices.Add(i);
		ConversationChoiceTexts.Add(Choice.Text);
	}
	if (Node->bEnd || ConversationChoiceTexts.Num() == 0)
	{
		// Nothing more to say: leave after the line has been read.
		if (ConversationWidget) { ConversationWidget->SetChoices({ TEXT("(leave)") }); }
		ConversationChoiceTexts = { TEXT("(leave)") };
		ConversationChoiceIndices = { -1 };
		return;
	}
	if (ConversationWidget) { ConversationWidget->SetChoices(ConversationChoiceTexts); }
}

float ABasePlayerController::PlayVoiceLine(ABaseCharacter* With, const FString& NodeId, int32 LineIndex)
{
	const float Seconds = PlayVoiceFile(With, ConversationFile::GetVoicePath(Conversation.Character, NodeId, LineIndex));
	if (Seconds > 0.0f) { UE_LOG(LogTemp, Log, TEXT("Voice: %s/%s[%d] %.2fs"), *Conversation.Character, *NodeId, LineIndex, Seconds); }
	return Seconds;
}

float ABasePlayerController::PlayVoiceFile(ABaseCharacter* With, const FString& Path)
{
	StopConversationVoice();
	if (CVarVoices.GetValueOnGameThread() == 0) { return 0.0f; }   // text only for now
	if (!With || !With->GetMesh()) { return 0.0f; }
	float Seconds = 0.0f;
	USoundWave* Sound = VoiceLines::LoadWav(this, Path, Seconds);
	if (!Sound) { return 0.0f; }   // no bake for this line: text only

	if (!VoiceAttenuation)
	{
		// Spoken from the NPC's head: full volume within arm's reach, gone
		// across the hall.
		VoiceAttenuation = NewObject<USoundAttenuation>(this);
		FSoundAttenuationSettings& A = VoiceAttenuation->Attenuation;
		A.bAttenuate = true;
		A.bSpatialize = true;
		A.AttenuationShape = EAttenuationShape::Sphere;
		A.AttenuationShapeExtents = FVector(250.0f);
		A.FalloffDistance = 1500.0f;
	}
	ConversationVoice = UGameplayStatics::SpawnSoundAttached(Sound, With->GetMesh(), TEXT("head"), FVector::ZeroVector,
		EAttachLocation::SnapToTarget, true, 1.0f, 1.0f, 0.0f, VoiceAttenuation, nullptr, false);
	if (!ConversationVoice) { return 0.0f; }
	GetWorldTimerManager().SetTimer(ConversationVoiceTimer, this, &ABasePlayerController::StopConversationVoice, Seconds + 0.2f, false);
	return Seconds;
}

void ABasePlayerController::StopConversationVoice()
{
	GetWorldTimerManager().ClearTimer(ConversationVoiceTimer);
	if (ConversationVoice)
	{
		ConversationVoice->Stop();
		ConversationVoice->DestroyComponent();
		ConversationVoice = nullptr;
	}
}

bool ABasePlayerController::IsVoicePlaying() const
{
	return ConversationVoice && ConversationVoice->IsPlaying();
}

void ABasePlayerController::ChooseConversationOption(int32 Index)
{
	if (Sequence && Sequence->IsActive()) { Sequence->OnChoice(Index); return; }
	if (!IsInConversation() || !ConversationChoiceIndices.IsValidIndex(Index)) { return; }
	const int32 ChoiceIndex = ConversationChoiceIndices[Index];
	if (ChoiceIndex < 0) { EndConversation(); return; }   // "(leave)"
	const FConversationNode* Node = Conversation.Nodes.Find(ConversationNodeId);
	if (!Node || !Node->Choices.IsValidIndex(ChoiceIndex)) { EndConversation(); return; }
	const FConversationChoice Choice = Node->Choices[ChoiceIndex];

	ConversationHistory.Add(FString::Printf(TEXT("You: %s"), *Choice.Text));
	if (ConversationWidget) { ConversationWidget->AddLine(TEXT("You"), Choice.Text, true); }
	if (Choice.bOnce) { ConversationChoicesTaken.Add(FString::Printf(TEXT("%s/%s/%d"), *Conversation.Character, *ConversationNodeId, ChoiceIndex)); }
	if (!Choice.SetFlag.IsEmpty()) { ConversationFlags.Add(Choice.SetFlag); }

	if (Choice.bEnd || Choice.Next.IsEmpty() || !Conversation.Nodes.Contains(Choice.Next)) { EndConversation(); return; }
	EnterConversationNode(Choice.Next);
}

void ABasePlayerController::UpdateHoverHighlight(ABaseCharacter* Hovered)
{
	if (HoveredCharacter.Get() == Hovered) { return; }
	if (ABaseCharacter* Prev = HoveredCharacter.Get()) { Prev->SetHoverHighlight(false); }
	HoveredCharacter = Hovered;
	if (Hovered) { Hovered->SetHoverHighlight(true); }
}

void ABasePlayerController::ShockBlinkOverlay(float Seconds)
{
	if (BlinkOverlay) { BlinkOverlay->Jolt(Seconds); }
}

// ---- Remote communication view ---------------------------------------------


void ABasePlayerController::ShowRemoteView(ABaseCharacter* Who, bool bWithSquare)
{
	if (!Who || !GetWorld()) { return; }
	RemoteSubject = Who;
	const int32 WantSize = FMath::Clamp(RemoteViewResolution, 64, 2048);
	if (RemoteTarget && RemoteTarget->SizeX != WantSize) { RemoteTarget = nullptr; }   // resolution changed: a fresh target
	if (!RemoteTarget)
	{
		RemoteTarget = UKismetRenderingLibrary::CreateRenderTarget2D(this, FMath::Clamp(RemoteViewResolution, 64, 2048), FMath::Clamp(RemoteViewResolution, 64, 2048), ETextureRenderTargetFormat::RTF_RGBA8_SRGB, FLinearColor::Black, false);
	}
	if (!RemoteCapture)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		RemoteCapture = GetWorld()->SpawnActor<ASceneCapture2D>(ASceneCapture2D::StaticClass(), FTransform::Identity, Params);
		if (USceneCaptureComponent2D* Cap = RemoteCapture ? RemoteCapture->GetCaptureComponent2D() : nullptr)
		{
			Cap->TextureTarget = RemoteTarget;
			Cap->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
			Cap->bCaptureEveryFrame = true;
			Cap->bCaptureOnMovement = false;
			Cap->bAlwaysPersistRenderingState = true;
			LockCaptureExposure(Cap);
		}
	}
	if (USceneCaptureComponent2D* Cap = RemoteCapture ? RemoteCapture->GetCaptureComponent2D() : nullptr) { Cap->TextureTarget = RemoteTarget; LockCaptureExposure(Cap); }
	if (!bWithSquare) { UpdateRemoteView(); return; }
	if (!RemoteViewWidget)
	{
		RemoteViewWidget = CreateWidget<URemoteViewWidget>(this, URemoteViewWidget::StaticClass());
	}
	RemoteViewWidget->SetFeed(RemoteTarget, FString::Printf(TEXT("%s  //  feed 0%d"), *Who->GetCharacterConfig().Name.ToUpper(), 1 + (Who->GetUniqueID() % 8)));
	if (!RemoteViewWidget->IsInViewport())
	{
		// A square just left of the conversation panel, sized from the screen height.
		float Side = 300.0f;
		if (GEngine && GEngine->GameViewport)
		{
			FVector2D ViewSize; GEngine->GameViewport->GetViewportSize(ViewSize);
			if (ViewSize.Y > 0.0f) { Side = FMath::Clamp(static_cast<float>(ViewSize.Y) * 0.3f, 200.0f, 480.0f); }
		}
		if (UGameViewportSubsystem* Viewport = UGameViewportSubsystem::Get())
		{
			FGameViewportWidgetSlot SquareSlot;
			SquareSlot.ZOrder = 71;
			SquareSlot.Anchors = FAnchors(2.0f / 3.0f, 0.0f, 2.0f / 3.0f, 0.0f);
			SquareSlot.Alignment = FVector2D(1.0f, 0.0f);   // right edge on the panel's left edge
			SquareSlot.Offsets = FMargin(-8.0f, ConversationPanelPadding, Side, Side);
			Viewport->AddWidget(RemoteViewWidget, SquareSlot);
		}
	}
	UpdateRemoteView();
}

void ABasePlayerController::HideRemoteView()
{
	if (ABaseCharacter* Who = RemoteSubject.Get()) { Who->SetGazeLock(false); }
	RemoteSubject = nullptr;
	DestroyBooth();
	if (RemoteViewWidget && RemoteViewWidget->IsInViewport()) { RemoteViewWidget->RemoveFromParent(); }
	if (USceneCaptureComponent2D* Cap = RemoteCapture ? RemoteCapture->GetCaptureComponent2D() : nullptr) { Cap->bCaptureEveryFrame = false; }
}

void ABasePlayerController::UpdateRemoteView()
{
	ABaseCharacter* Who = RemoteSubject.Get();
	if (!Who || !RemoteCapture) { return; }
	const FRemoteCameraConfig& Cam = Who->GetCharacterConfig().RemoteCamera;
	const FVector Head = Who->GetMesh() && Who->GetMesh()->DoesSocketExist(TEXT("head")) ? Who->GetMesh()->GetSocketLocation(TEXT("head")) : Who->GetActorLocation() + FVector(0, 0, 60.0f);
	const FRotator Facing(0.0f, Who->GetActorRotation().Yaw, 0.0f);
	const FVector Eye = Head + Facing.RotateVector(Cam.Offset);
	const FVector LookAt = Head + Facing.RotateVector(Cam.LookOffset);
	FRotator Rot = (LookAt - Eye).Rotation();
	Rot.Roll = Cam.Roll;
	RemoteCapture->SetActorLocationAndRotation(Eye, Rot);
	// Eyes on the camera, as on a call; the chooser preview just idles instead.
	if (!Screens.IsOpen(EScreen::Appearance) && !bSheetMirror) { Who->SetGazeLock(true, Eye); } else if (Who->IsGazeLocked()) { Who->SetGazeLock(false, Eye); }   // the chooser's and the sheet's figure just idle
	if (USceneCaptureComponent2D* Cap = RemoteCapture->GetCaptureComponent2D())
	{
		Cap->FOVAngle = Cam.Fov;
		Cap->bCaptureEveryFrame = true;
	}
}

// ---- The call booth ----------------------------------------------------------

ABaseCharacter* ABasePlayerController::FindCharacterByConfig(const FString& ConfigName) const
{
	if (!GetWorld()) { return nullptr; }
	for (TActorIterator<ABaseCharacter> It(GetWorld()); It; ++It)
	{
		if (*It != GetPawn() && It->MatchesConfigName(ConfigName)) { return *It; }
	}
	return nullptr;
}

ABaseCharacter* ABasePlayerController::SpawnBoothCharacter(const FString& ConfigName)
{
	UWorld* World = GetWorld();
	if (!World || !IFileManager::Get().FileExists(*CharacterConfigFile::GetPath(ConfigName))) { return nullptr; }
	DestroyBooth();
	// A small set far below anything the player can reach: a floor tile, a
	// wall panel behind the caller, a key light, a cool fill and a rim.
	const FVector O(0.0f, 0.0f, -30000.0f);
	UMaterialInterface* Grime = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/RepliCan/Materials/M_Facility_Grime.M_Facility_Grime"));
	auto Mesh = [&](const TCHAR* Path, const FVector& Loc, const FRotator& Rot)
	{
		UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, Path);
		AStaticMeshActor* A = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), O + Loc, Rot);
		if (!A || !M) { return; }
		A->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
		A->GetStaticMeshComponent()->SetStaticMesh(M);
		if (Grime) { for (int32 i = 0; i < A->GetStaticMeshComponent()->GetNumMaterials(); ++i) { A->GetStaticMeshComponent()->SetMaterial(i, Grime); } }
		BoothActors.Add(A);
	};
	auto Light = [&](const FVector& Loc, float Cd, float Radius, const FLinearColor& Color)
	{
		APointLight* L = World->SpawnActor<APointLight>(APointLight::StaticClass(), O + Loc, FRotator::ZeroRotator);
		if (!L) { return; }
		L->PointLightComponent->SetMobility(EComponentMobility::Movable);
		L->PointLightComponent->SetIntensityUnits(ELightUnits::Candelas);
		L->PointLightComponent->SetIntensity(Cd);
		L->PointLightComponent->SetAttenuationRadius(Radius);
		L->PointLightComponent->SetLightColor(Color);
		L->PointLightComponent->SetCastShadows(false);
		BoothActors.Add(L);
	};
	Mesh(TEXT("/Game/PolygonSciFiSpace/Meshes/Buildings/SM_Bld_Floor_04.SM_Bld_Floor_04"), FVector(-250.0f, -250.0f, 0.0f), FRotator::ZeroRotator);
	Mesh(TEXT("/Game/PolygonSciFiSpace/Meshes/Buildings/SM_Bld_Wall_01_Alt.SM_Bld_Wall_01_Alt"), FVector(-150.0f, 250.0f, 0.0f), FRotator(0.0f, -90.0f, 0.0f));   // room side toward +X
	BoothSet = BoothActors;   // what SetBoothSetVisible toggles
	Light(FVector(130.0f, 70.0f, 195.0f), 30.0f, 600.0f, FLinearColor(1.0f, 0.86f, 0.68f));
	Light(FVector(110.0f, -90.0f, 150.0f), 9.0f, 500.0f, FLinearColor(0.6f, 0.9f, 1.0f));
	Light(FVector(-90.0f, 0.0f, 240.0f), 7.0f, 400.0f, FLinearColor(0.7f, 1.0f, 0.8f));

	const UCapsuleComponent* Capsule = GetDefault<ABaseCharacter>()->GetCapsuleComponent();
	const float HalfHeight = Capsule ? Capsule->GetUnscaledCapsuleHalfHeight() : 88.0f;
	const FTransform Spawn(FRotator::ZeroRotator, O + FVector(0.0f, 0.0f, HalfHeight + 2.0f));   // facing +X, the camera side
	ABaseCharacter* Who = World->SpawnActorDeferred<ABaseCharacter>(ABaseCharacter::StaticClass(), Spawn, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Who) { DestroyBooth(); return nullptr; }
	Who->DefaultCharacterConfigName = ConfigName;
	Who->bRuntimeSpawned = true;
	Who->FinishSpawning(Spawn);
#if WITH_EDITOR
	Who->SetActorLabel(ConfigName + TEXT("_Call"));
#endif
	BoothCharacter = Who;
	return Who;
}

void ABasePlayerController::DestroyBooth()
{
	for (UNiagaraComponent* C : BoothFX) { if (C) { C->DestroyComponent(); } }
	BoothFX.Reset();
	BoothSet.Reset();
	for (AActor* A : BoothActors) { if (A) { A->Destroy(); } }
	BoothActors.Reset();
	if (ABaseCharacter* Who = BoothCharacter.Get()) { Who->Destroy(); }
	BoothCharacter = nullptr;
}

bool ABasePlayerController::ShowRemoteCall(const FString& ConfigName)
{
	ABaseCharacter* Who = FindCharacterByConfig(ConfigName);
	if (!Who) { Who = SpawnBoothCharacter(ConfigName); }
	if (!Who) { UE_LOG(LogTemp, Warning, TEXT("RemoteCall: no character config '%s'"), *ConfigName); return false; }
	ShowRemoteView(Who);
	return true;
}

bool ABasePlayerController::StartRemoteConversation(const FString& ConfigName)
{
	if (IsInConversation()) { return false; }
	ABaseCharacter* Who = FindCharacterByConfig(ConfigName);
	if (!Who) { Who = SpawnBoothCharacter(ConfigName); }
	if (!Who) { return false; }
	bRemoteConversation = true;
	if (!StartConversation(Who))
	{
		bRemoteConversation = false;
		DestroyBooth();
		return false;
	}
	ShowRemoteView(Who);
	return true;
}

// ---- Booth dressing ------------------------------------------------------------

AActor* ABasePlayerController::AddBoothProp(const FString& MeshPath, FVector Location, FRotator Rotation, FVector Scale, bool bGrime)
{
	UWorld* World = GetWorld();
	UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *MeshPath);
	if (!World || !M) { return nullptr; }
	AStaticMeshActor* A = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), GetBoothOrigin() + Location, Rotation);
	if (!A) { return nullptr; }
	A->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	A->GetStaticMeshComponent()->SetStaticMesh(M);
	A->SetActorScale3D(Scale.IsNearlyZero() ? FVector::OneVector : Scale);
	if (bGrime)
	{
		if (UMaterialInterface* Grime = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/RepliCan/Materials/M_Facility_Grime.M_Facility_Grime")))
		{
			for (int32 i = 0; i < A->GetStaticMeshComponent()->GetNumMaterials(); ++i) { A->GetStaticMeshComponent()->SetMaterial(i, Grime); }
		}
	}
	BoothActors.Add(A);
	return A;
}

bool ABasePlayerController::AddBoothFX(const FString& SystemPath, FVector Location, FRotator Rotation, FVector Scale)
{
	UNiagaraSystem* Sys = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
	if (!Sys || !GetWorld()) { UE_LOG(LogTemp, Warning, TEXT("Booth: Niagara system '%s' not found"), *SystemPath); return false; }
	UNiagaraComponent* C = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), Sys, GetBoothOrigin() + Location, Rotation, Scale.IsNearlyZero() ? FVector::OneVector : Scale, false, true, ENCPoolMethod::None, true);
	if (!C) { return false; }
	BoothFX.Add(C);
	return true;
}

AActor* ABasePlayerController::AddBoothLight(FVector Location, float Candelas, float Radius, FLinearColor Color, bool bFlicker)
{
	UWorld* World = GetWorld();
	if (!World) { return nullptr; }
	AActor* Result = nullptr;
	if (bFlicker)
	{
		AFlickerLightActor* F = World->SpawnActor<AFlickerLightActor>(AFlickerLightActor::StaticClass(), GetBoothOrigin() + Location, FRotator::ZeroRotator);
		if (!F) { return nullptr; }
		F->BaseIntensity = Candelas; F->AttenuationRadius = Radius; F->Color = Color; F->FlaresPerSecond = 1.5f; F->Speed = 1.6f;
		Result = F;
	}
	else
	{
		APointLight* L = World->SpawnActor<APointLight>(APointLight::StaticClass(), GetBoothOrigin() + Location, FRotator::ZeroRotator);
		if (!L) { return nullptr; }
		L->PointLightComponent->SetMobility(EComponentMobility::Movable);
		L->PointLightComponent->SetIntensityUnits(ELightUnits::Candelas);
		L->PointLightComponent->SetIntensity(Candelas);
		L->PointLightComponent->SetAttenuationRadius(Radius);
		L->PointLightComponent->SetLightColor(Color);
		L->PointLightComponent->SetCastShadows(false);
		Result = L;
	}
	BoothActors.Add(Result);
	return Result;
}

void ABasePlayerController::SetBoothSetVisible(bool bVisible)
{
	for (AActor* A : BoothSet) { if (A) { A->SetActorHiddenInGame(!bVisible); } }
}

// ---- Appearance chooser -------------------------------------------------------------

static const float AppearanceHeights[] = { 0.92f, 0.94f, 0.96f, 0.98f, 1.0f, 1.02f, 1.04f, 1.06f, 1.08f };
static const float AppearanceBuilds[] = { 0.90f, 0.95f, 1.0f, 1.05f, 1.10f, 1.15f };

void ABasePlayerController::DressBoothAsMirror()
{
	// Nothing behind the likeness but black: the set goes and unlit black planes close off
	// the view behind and beneath the figure. The fog flag stays on: it carries a good part
	// of the ambient lift here, and the black planes absorb its haze anyway.
	SetBoothSetVisible(false);
	// The room fog had been lifting the booth; without it the figure needs real light: a front key, a soft high fill and a cool rim.
	AddBoothLight(FVector(190.0f, 60.0f, 150.0f), 90.0f, 700.0f, FLinearColor(1.0f, 0.94f, 0.86f), false);
	AddBoothLight(FVector(120.0f, -120.0f, 220.0f), 35.0f, 700.0f, FLinearColor(0.85f, 0.92f, 1.0f), false);
	AddBoothLight(FVector(-80.0f, 40.0f, 200.0f), 30.0f, 500.0f, FLinearColor(0.7f, 0.95f, 0.85f), false);
	if (UMaterialInterface* Black = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/RepliCan/Materials/M_Black.M_Black")))
	{
		for (const TPair<FVector, FRotator>& Plane : { TPair<FVector, FRotator>(FVector(-450.0f, 0.0f, 300.0f), FRotator(-90.0f, 0.0f, 0.0f)), TPair<FVector, FRotator>(FVector(0.0f, 0.0f, -1.0f), FRotator(0.0f, 0.0f, 0.0f)) })
		{
			AActor* P = AddBoothProp(TEXT("/Engine/BasicShapes/Plane.Plane"), Plane.Key, Plane.Value, FVector(30.0f, 30.0f, 1.0f), false);
			if (AStaticMeshActor* SM = Cast<AStaticMeshActor>(P)) { SM->GetStaticMeshComponent()->SetMaterial(0, Black); }
		}
	}
}

bool ABasePlayerController::ShowSheetMirrorFor(const FString& ConfigName)
{
	if (Screens.IsOpen(EScreen::Appearance) || !GetWorld()) { return false; }
	// One booth at a time: whatever was in it goes first.
	if (bSheetMirror) { HideSheetMirror(); }
	if (BoothCharacter.IsValid()) { DestroyBooth(); }
	ABaseCharacter* Preview = SpawnBoothCharacter(ConfigName);
	if (!Preview) { return false; }
	bSheetMirror = true;
	SheetOrbitYaw = 0.0f; SheetOrbitPitch = 0.0f; SheetBaseYaw = Preview->GetActorRotation().Yaw;
	// The sheet pauses the game; the figure keeps idling and the capture keeps running.
	Preview->SetTickableWhenPaused(true);
	if (Preview->GetMesh()) { Preview->GetMesh()->SetTickableWhenPaused(true); }
	RemoteViewResolution = 1024;
	ShowRemoteView(Preview, false);
	// The sheet's own portrait target (5:8) replaces the square one on the capture while the sheet is up.
	if (!SheetTarget) { SheetTarget = UKismetRenderingLibrary::CreateRenderTarget2D(this, 640, PaneShape::TargetHeight(PaneShape::Mirror, 640), ETextureRenderTargetFormat::RTF_RGBA8_SRGB, FLinearColor::Black, false); }
	if (RemoteCapture) { RemoteCapture->SetTickableWhenPaused(true); if (USceneCaptureComponent2D* Cap = RemoteCapture->GetCaptureComponent2D()) { Cap->SetTickableWhenPaused(true); Cap->TextureTarget = SheetTarget; } }
	bSheetHead = false;
	DressBoothAsMirror();
	PlaceSheetCamera();
	return true;
}

// The sheet's own: the saved likeness (Player.json); a fresh player without one gets no mirror yet.
void ABasePlayerController::ShowSheetMirror()
{
	if (bSheetMirror || BoothCharacter.IsValid()) { return; }
	ShowSheetMirrorFor(TEXT("Player"));
}

void ABasePlayerController::HideCharacterPreview() { HideSheetMirror(); }

void ABasePlayerController::PlaceSheetCamera()
{
	ABaseCharacter* P = BoothCharacter.Get();
	if (!bSheetMirror || !P) { return; }
	// The chooser's framings, swung by the orbit angles; the FOV here is horizontal on a portrait
	// target, so it runs narrower than the chooser's square one to frame the same height.
	const FSheetSpec& Spec = FSheetSpec::Get();
	FRemoteCameraConfig Cam;
	if (bSheetHead) { Cam.Offset = Spec.HeadOffset; Cam.LookOffset = Spec.HeadLook; Cam.Fov = Spec.HeadFov; }
	else { Cam.Offset = Spec.BodyOffset; Cam.LookOffset = Spec.BodyLook; Cam.Fov = Spec.BodyFov; }
	const FVector Arm = FRotator(SheetOrbitPitch, 0.0f, 0.0f).RotateVector(Cam.Offset - Cam.LookOffset);
	Cam.Offset = Cam.LookOffset + FRotator(0.0f, -SheetOrbitYaw, 0.0f).RotateVector(Arm);
	P->SetRemoteCamera(Cam);
	P->SetActorRotation(FRotator(0.0f, SheetBaseYaw + SheetOrbitYaw, 0.0f));
	UpdateRemoteView();
}

void ABasePlayerController::OrbitSheetMirror(float DeltaYaw, float DeltaPitch)
{
	if (!bSheetMirror) { return; }
	SheetOrbitYaw = FMath::Fmod(SheetOrbitYaw + DeltaYaw, 360.0f);
	SheetOrbitPitch = FMath::Clamp(SheetOrbitPitch + DeltaPitch, -30.0f, 40.0f);
	PlaceSheetCamera();
}

void ABasePlayerController::SetSheetMirrorZoom(bool bHead)
{
	if (!bSheetMirror || bSheetHead == bHead) { return; }
	bSheetHead = bHead;
	PlaceSheetCamera();
}

void ABasePlayerController::HideSheetMirror()
{
	if (!bSheetMirror) { return; }
	bSheetMirror = false; bSheetHead = false;
	RemoteSubject = nullptr;
	if (RemoteCapture) { if (USceneCaptureComponent2D* Cap = RemoteCapture->GetCaptureComponent2D()) { Cap->bCaptureEveryFrame = false; Cap->TextureTarget = RemoteTarget; } }
	DestroyBooth();
}

bool ABasePlayerController::EquipFromInventory(int32 InventoryIndex)
{
	if (!Inventory.IsValidIndex(InventoryIndex)) { return false; }
	const FSheetSpec& Spec = FSheetSpec::Get();
	if (Equipped.Num() != Spec.Slots.Num()) { Equipped.SetNum(Spec.Slots.Num()); }
	const FString Item = Inventory[InventoryIndex];
	const int32 Slot = Spec.SlotForKind(ItemCatalog::Kind(Item));
	if (Slot < 0) { ShowCallout(GetPawn(), FString::Printf(TEXT("%s does not equip."), *Item), 2.0f, false); return false; }
	if (!Spec.Slots[Slot].bEnabled) { ShowCallout(GetPawn(), TEXT("That slot is not available yet."), 2.0f, false); return false; }
	{ FString Why; if (!ClothingFits(Item, Why)) { ShowCallout(GetPawn(), Why, 2.5f, false); return false; } }
	Inventory[InventoryIndex] = Equipped[Slot];   // the old one takes the square the new one left (empty when there was none)
	Equipped[Slot] = Item;
	RefreshHeldWeapon();
	return true;
}

// ---- Held weapon -----------------------------------------------------------
// The equipment slots are the only truth about what is being carried: nothing else sets the
// weapon, so a slot changing anywhere -- the sheet, a script, a pickup -- shows up in the hand.
// A weapon in each hand slot at the start, so the firing path is exercised without hunting for
// a pickup first. Only ever fills EMPTY slots, so a loaded save or anything equipped earlier is
// left alone.
void ABasePlayerController::GiveStartingWeapons()
{
	const FSheetSpec& Spec = FSheetSpec::Get();
	if (Equipped.Num() != Spec.Slots.Num()) { Equipped.SetNum(Spec.Slots.Num()); }
	auto Fit = [&](const FString& Item)
	{
		if (Item.IsEmpty()) { return; }
		const WeaponCatalog::FWeapon* W = WeaponCatalog::Find(Item);
		if (!W) { UE_LOG(LogTemp, Warning, TEXT("GiveStartingWeapons: %s is not in the catalogue"), *Item); return; }
		// THE FIRST SLOT THAT TAKES IT **AND IS FREE**. Slot 1 and Slot 2 carry identical kind
		// lists, and SlotForKind answers with the first slot accepting the kind whatever is already
		// sitting in it -- so the first weapon took Slot 1, the second resolved to that same slot,
		// found it occupied, and was dropped without a word. Only one starting weapon ever arrived.
		int32 Slot = -1;
		for (int32 i = 0; i < Spec.Slots.Num() && i < Equipped.Num(); ++i)
		{
			// ENABLED, asked explicitly. A disabled slot is not a place a weapon may go, and saying
			// so beats relying on Slot 3 and Slot 4 happening to carry empty kind lists -- the day
			// one of them is earned and given kinds, this would start filling a slot the player
			// cannot use.
			if (!Spec.Slots[i].bEnabled || !Equipped[i].IsEmpty()) { continue; }
			const bool bTakesIt = Spec.Slots[i].Kinds.ContainsByPredicate(
				[&](const FString& K) { return K.Equals(W->Kind, ESearchCase::IgnoreCase); });
			if (bTakesIt) { Slot = i; break; }
		}
		if (Slot < 0)
		{
			// Which of the two it is, rather than the silence this used to fail with.
			UE_LOG(LogTemp, Warning, TEXT("GiveStartingWeapons: %s (%s) %s"), *Item, *W->Kind,
				Spec.SlotForKind(W->Kind) < 0 ? TEXT("fits no slot") : TEXT("has no free slot that takes it"));
			return;
		}
		Equipped[Slot] = Item;
	};
	for (const FString& Item : StartingWeapons) { Fit(Item); }
	// AND THE PACK. bPickup false so nothing auto-equips: these are meant to be reached for, which
	// is the point of putting a thing under test in the inventory rather than into the hands.
	for (const FString& Item : StartingInventory)
	{
		if (Item.IsEmpty()) { continue; }
		if (!WeaponCatalog::Find(Item) && !ItemCatalog::FindRecord(Item))
		{
			UE_LOG(LogTemp, Warning, TEXT("GiveStartingWeapons: %s is in StartingInventory but in no catalogue"), *Item);
			continue;
		}
		if (!AddToInventory(Item, false))
		{
			UE_LOG(LogTemp, Warning, TEXT("GiveStartingWeapons: no room in the pack for %s"), *Item);
		}
	}
	RefreshHeldWeapon();
}

void ABasePlayerController::SetWeaponPreviewSkin(const FString& WeaponName, const FString& Variant)
{
	PreviewSkinOverride = Variant;
	if (!WeaponBoothActor || WeaponName.IsEmpty()) { return; }
	if (const WeaponCatalog::FWeapon* W = WeaponCatalog::Find(WeaponName))
	{
		WeaponSkins::ApplyNamed(WeaponBoothActor->GetStaticMeshComponent(), *W,
			PreviewSkinOverride.IsEmpty() ? W->Skin : PreviewSkinOverride);
	}
}

void ABasePlayerController::CycleWeaponSkin(const FString& ItemName)
{
	const WeaponCatalog::FWeapon* W = WeaponCatalog::Find(ItemName);
	if (!W) { return; }
	const TArray<FString> V = WeaponSkins::Variants(*W);
	if (V.Num() < 2) { return; }
	const int32 At = V.IndexOfByKey(WeaponSkins::Current(*W));
	const FString Next = V[(At + 1) % V.Num()];
	const FString Key = W->Key;   // the write re-reads the catalogue, so W is stale after it
	if (!WeaponCatalog::WriteStringField(Key, TEXT("skin"), Next)) { UE_LOG(LogTemp, Warning, TEXT("CycleWeaponSkin: could not write %s"), *Key); return; }
	RefreshHeldWeapon();
}

void ABasePlayerController::RefreshHeldWeapon()
{
	ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn());
	if (!Me) { return; }
	const FSheetSpec& Spec = FSheetSpec::Get();
	RefreshWornClothing();

	// The enabled weapon slots in order -- they carry no roles, any weapon fits any of them. A held
	// slot that is still filled stays held, so picking something up does not snatch the weapon out
	// of the player's hand.
	int32 Want = -1;
	if (Equipped.IsValidIndex(StowedSlot) && !Equipped[StowedSlot].IsEmpty()) { Want = StowedSlot; }
	else if (Equipped.IsValidIndex(HeldSlot) && !Equipped[HeldSlot].IsEmpty()) { Want = HeldSlot; }
	else
	{
		for (int32 i = 0; i < Spec.Slots.Num() && i < Equipped.Num(); ++i)
		{
			if (!Spec.Slots[i].Name.StartsWith(TEXT("Slot "))) { continue; }
			if (!Equipped[i].IsEmpty()) { Want = i; break; }
		}
	}
	StowedSlot = Want;
	// Hands empty, but the slot remembered.
	if (bHolstered)
	{
		HeldSlot = -1;
		Me->SetWeaponMesh(nullptr);
		Me->SetWeaponOptic(nullptr, FVector::ZeroVector);
		Me->SetWeaponStance(FString());
		Me->SetAiming(false);
		return;
	}
	HeldSlot = Want;

	const WeaponCatalog::FWeapon* W = (Want >= 0) ? WeaponCatalog::Find(Equipped[Want]) : nullptr;
	if (!W) { Me->SetWeaponMesh(nullptr); Me->SetWeaponStance(FString()); Me->SetAiming(false); return; }
	ApplyWeaponToPawn(Me, W, (Want >= 0 && Equipped.IsValidIndex(Want)) ? Equipped[Want] : FString());
}

bool ABasePlayerController::WantsInstance(const FString& Name)
{
	// Weapons, for now: they are what carries accessories, paint and wear. Deliberately narrow --
	// every instance is a row that has to be saved and eventually released, and there is no sense
	// paying that for a box of ammunition where one is indistinguishable from the next.
	return WeaponCatalog::Find(Name) != nullptr;
}

FString ABasePlayerController::NewItemInstance(const FString& Name)
{
	if (Name.IsEmpty() || !WantsInstance(Name)) { return Name; }
	FItemInstance I;
	I.Id = NextItemInstanceId++;
	I.Name = ItemHandle::NameOf(Name);
	ItemInstances.Add(I);
	return ItemHandle::Make(I.Name, I.Id);
}

FItemInstance* ABasePlayerController::FindItemInstance(const FString& Handle)
{
	const int32 Id = ItemHandle::IdOf(Handle);
	if (Id <= 0) { return nullptr; }
	return ItemInstances.FindByPredicate([Id](const FItemInstance& I) { return I.Id == Id; });
}

const FItemInstance* ABasePlayerController::FindItemInstance(const FString& Handle) const
{
	return const_cast<ABasePlayerController*>(this)->FindItemInstance(Handle);
}

FString ABasePlayerController::ItemProp(const FString& Handle, const FString& Key, const FString& Fallback) const
{
	if (const FItemInstance* I = FindItemInstance(Handle))
	{
		if (const FString* V = I->Props.Find(Key)) { return *V; }
	}
	return Fallback;
}

void ABasePlayerController::SetItemProp(const FString& Handle, const FString& Key, const FString& Value)
{
	if (FItemInstance* I = FindItemInstance(Handle)) { I->Props.Add(Key, Value); }
}

void ABasePlayerController::ReleaseItemInstance(const FString& Handle)
{
	const int32 Id = ItemHandle::IdOf(Handle);
	if (Id > 0) { ItemInstances.RemoveAll([Id](const FItemInstance& I) { return I.Id == Id; }); }
}

FString ABasePlayerController::AccessorySummary(const FString& Handle) const
{
	const WeaponCatalog::FWeapon* W = WeaponCatalog::Find(Handle);
	if (!W) { return FString(); }
	// The optic this COPY carries, falling back to the type own default. More accessory kinds slot
	// in here as they arrive; the shape is already right for them.
	const FString OpticKey = FittedOptic(Handle, W);
	return FString::Printf(TEXT("FITTED   OPTIC %s"),
		OpticKey.IsEmpty() ? TEXT("none") : *WeaponCatalog::OpticDisplayName(OpticKey));
}

void ABasePlayerController::ApplyWeaponToPawn(ABaseCharacter* Me, const WeaponCatalog::FWeapon* W, const FString& Handle)
{
	if (!Me || !W) { return; }
	Me->SetHeldWeaponName(W->Name);   // so a later tuning save can find everyone holding this one
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *W->MeshPath);
	if (!Mesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("RefreshHeldWeapon: %s has no mesh at %s"), *W->Name, *W->MeshPath);
		Me->SetWeaponMesh(nullptr); Me->SetWeaponStance(FString()); return;
	}
	// A FITTED OPTIC IS LOOKED THROUGH. The catalogue names it and where it bolts on. Two things
	// follow that were both missing: the weapon's OWN modelled scope, painted opaque, sat in the
	// eye line in front of the optic -- so the body with that scope stripped is used
	// (Tools/strip_scopes.py writes body_mesh; the weapon's full mesh stays for everything
	// else) -- and the sight point is the optic's EYE, on the optic's glass, not the weapon's
	// own rear sight; collimated glass has no pitch to correct for.
	// THIS COPY optic if it has one of its own, otherwise the type default. An unmodified rifle has
	// no opinion and so behaves exactly as it did before instances existed.
	const WeaponCatalog::FOptic* Optic = WeaponCatalog::FindOptic(FittedOptic(Handle, W));
	UStaticMesh* OpticMesh = Optic ? LoadObject<UStaticMesh>(nullptr, *Optic->MeshPath, nullptr, LOAD_NoWarn | LOAD_Quiet) : nullptr;
	// THE CUT-DOWN MESH WHENEVER THERE IS ONE, optic or no optic. This used to fall back to the
	// UN-cut mesh when nothing was fitted, which meant "no optic" quietly put the weapon's moulded-on
	// factory scope back -- so the option reading IRON SIGHTS actually showed a scope, and that scope
	// could not be chosen deliberately because it was not an optic at all. Those parts are optics
	// now (Tools/promote_factory_scopes.py), each weapon wears its own by default so nothing looks
	// different, and no optic finally means no optic.
	if (!W->BodyMeshPath.IsEmpty())
	{
		if (UStaticMesh* Body = LoadObject<UStaticMesh>(nullptr, *W->BodyMeshPath, nullptr, LOAD_NoWarn | LOAD_Quiet)) { Mesh = Body; }
	}
	// Clear any offset a saved character config is still carrying from before the socket existed.
	// A HAC1 weapon on a measured socket needs nothing added to it, and an old config quietly
	// putting the rifle back where it used to be is very hard to see from the outside.
	Me->SetWeaponRelativeLocation(FVector::ZeroVector);
	Me->SetWeaponRelativeRotation(FRotator::ZeroRotator);
	Me->SetWeaponMesh(Mesh);
	WeaponSkins::Apply(Me->WeaponMeshComponent, *W);   // its chosen paint
	// THE EYE FOLLOWS THE GLASS. This left Offset out, so nudging a sight along the rail moved the
	// optic and left the eye line behind on the old spot -- you were aiming through where the scope
	// used to be. The sight point is the optic position (rail, less its mount, plus its offset) and
	// then the eye on top of that.
	if (OpticMesh && !Optic->Eye.IsNearlyZero()) { Me->SetWeaponSight(W->OpticMount - Optic->Mount + Optic->Offset + Optic->Eye, true, 0.0f); }
	else { Me->SetWeaponSight(W->Sight, W->bHasSight, W->SightPitch); }
	Me->SetWeaponHandRotation(W->HandRot);   // this weapon's own turn of the hand on its grip (hand_rot), on top of the character's correction
	Me->SetWeaponForeHandRotation(W->ForeHandRot);   // and the support hand's on the fore grip (fore_hand_rot)
	Me->SetWeaponFingers(W->FingersR, W->FingersL);   // and how far each finger closes on it
	Me->SetWeaponHunch(W->Hunch); Me->SetWeaponLean(W->LeanDeg); Me->SetWeaponCarryPos(W->PositionCm); Me->SetWeaponShoulderPoint(W->Shoulder);   // and the posture it asks for
	Me->SetWeaponLowReady(W->LowReadyPitch, W->LowReadyYaw); Me->SetWeaponElbowTwist(W->ElbowMain, W->ElbowSupport);
	Me->SetWeaponElbowAim(W->ElbowMainAim, W->ElbowSupportAim);
	Me->SetWeaponGrip(W->Grip + WeaponCatalog::StanceGripNudge(W->Stance));   // the stance may move the hand along the grip
	{
		// AND THE PER-CARRY HOLDS. Seeded from the single grip in the catalogue, so a weapon that has
		// never been tuned this way is identical to what it was; the character blends between them.
		FVector G[3], F[3];
		const FVector Nudge = WeaponCatalog::StanceGripNudge(W->Stance);
		for (int32 i = 0; i < 3; ++i) { G[i] = W->GripCm[i] + Nudge; F[i] = W->ForeCm[i]; }
		Me->SetWeaponGripPerCarry(G, F);
	}
	Me->SetWeaponDrawScale(W->Scale);   // before the grip and sight are read off it
	Me->SetWeaponRecoil(W->Recoil);
	Me->SetWeaponMass(W->MassKg);
	Me->SetWeaponMuzzle(W->Muzzle);
	Me->SetWeaponForeGrip(W->ForeGrip, W->bHasForeGrip, W->ForeGripPitch);
	Me->SetWeaponHipFire(W->bHipFire);
	Me->SetWeaponMelee(!W->bRanged);
	// The rail point and the optic own offset go over separately, so the page can move one of them
	// without having to recompute the other.
	Me->SetWeaponOptic(OpticMesh,
		(OpticMesh && Optic) ? W->OpticMount - Optic->Mount : FVector::ZeroVector,
		(OpticMesh && Optic) ? Optic->Rot : FRotator::ZeroRotator);
	Me->SetWeaponOpticOffset((OpticMesh && Optic) ? Optic->Offset : FVector::ZeroVector);
	Me->SetWeaponOpticOptics((OpticMesh && Optic) ? Optic->Zoom : 1.0f, OpticMesh && Optic && Optic->bSmart,
		(OpticMesh && Optic) ? Optic->Reticle : FString(),
		(OpticMesh && Optic) ? Optic->ReticleColour : FLinearColor(0.45f, 1.0f, 0.65f, 1.0f),
		(OpticMesh && Optic) ? Optic->ZoomLevels : TArray<float>(),
		OpticMesh && Optic && Optic->bOverlay, OpticMesh && Optic && Optic->bPiP);
	// And the sight own paint, which is its property and not the gun it is bolted to.
	// THE SIGHT WEARS THE GUN'S PAINT UNLESS IT HAS ITS OWN. A factory scope is geometry cut off this
	// weapon and now shares its material family, so leaving it on the family's default while the gun
	// wears its eighth colourway puts a differently coloured lump on top of the rifle. An optic that
	// names a skin keeps it -- a bought sight is its own object and need not match anything.
	if (OpticMesh && Optic && Me->OpticMeshComponent)
	{
		WeaponSkins::ApplyVariant(Me->OpticMeshComponent, Optic->Skin.IsEmpty() ? W->Skin : Optic->Skin);
	}
	// How the body holds it is the weapon's own business: it carries a stance name and the
	// character composes clip paths from it. Nothing here decides what a rifle looks like.
	Me->SetWeaponStance(W->Stance);
	if (!W->Space.Equals(TEXT("hac1")))
	{
		UE_LOG(LogTemp, Warning, TEXT("RefreshHeldWeapon: %s has not been normalised (Tools/normalise_weapons.py); it will hang wrong"), *W->Name);
	}
}

void ABasePlayerController::EquipWeaponSlot(int32 Ordinal)
{
	const FSheetSpec& Spec = FSheetSpec::Get();
	// By NAME: the spec lists Slot 1, Slot 3, Slot 2, Slot 4 (the grid's layout), so counting them in order sent key 2 to Slot 3.
	const FString WantName = FString::Printf(TEXT("Slot %d"), Ordinal);
	UE_LOG(LogTemp, Log, TEXT("EquipWeaponSlot %d: held %d"), Ordinal, HeldSlot);
	for (int32 i = 0; i < Spec.Slots.Num() && i < Equipped.Num(); ++i)
	{
		if (Spec.Slots[i].Name != WantName) { continue; }
		if (Equipped[i].IsEmpty()) { SetDiagNoteTimed(FString::Printf(TEXT("Slot %d is empty"), Ordinal), 2.0f); return; }
		bHolstered = false;
		if (HeldSlot == i) { StowedSlot = i; RefreshHeldWeapon(); return; }   // already in hand (or holstered): just make sure it is drawn
		BeginWeaponSwap(i);
		SetDiagNoteTimed(FString::Printf(TEXT("Drew %s"), *WeaponCatalog::DisplayName(Equipped[HeldSlot])), 3.0f);
		return;
	}
	SetDiagNoteTimed(FString::Printf(TEXT("No weapon slot %d"), Ordinal), 2.0f);
}

void ABasePlayerController::SwapWeaponSlot(int32 Direction)
{
	// Asking for the next weapon is asking to be holding one.
	bHolstered = false;
	const FSheetSpec& Spec = FSheetSpec::Get();
	// The next filled weapon slot after the one in hand, wrapping round.
	TArray<int32> Filled;
	for (int32 i = 0; i < Spec.Slots.Num() && i < Equipped.Num(); ++i)
	{
		if (Spec.Slots[i].Name.StartsWith(TEXT("Slot ")) && !Equipped[i].IsEmpty()) { Filled.Add(i); }
	}
	UE_LOG(LogTemp, Log, TEXT("SwapWeaponSlot: %d filled, held %d"), Filled.Num(), HeldSlot);
	if (Filled.Num() < 2) { SetDiagNoteTimed(TEXT("Nothing else to draw"), 3.0f); return; }
	const int32 At = FMath::Max(0, Filled.IndexOfByKey(HeldSlot));
	const int32 Step = (Direction >= 0) ? 1 : Filled.Num() - 1;   // wrap both ways
	// The re-equip trusts StowedSlot over HeldSlot (a pickup must not snatch the weapon out of the
	// hand), so the swap has to move BOTH or the next refresh puts the old one back -- which is
	// exactly what the wheel was doing: swapping, then refreshing back to slot 1.
	BeginWeaponSwap(Filled[(At + Step) % Filled.Num()]);
	SetDiagNoteTimed(FString::Printf(TEXT("Drew %s"), *Equipped[HeldSlot]), 3.0f), void();
}

void ABasePlayerController::FireHeldWeapon()
{
	ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn());
	if (!Me || !GetWorld() || !PlayerCameraManager) { return; }
	if (!Equipped.IsValidIndex(HeldSlot)) { return; }
	const WeaponCatalog::FWeapon* W = WeaponCatalog::Find(Equipped[HeldSlot]);
	if (!W) { return; }
	if (!W->bRanged) { MeleeSwing(); return; }   // a blade or a hammer: a swing, not a shot

	// Told to shoot with the muzzle at the floor: bring it up first and fire when it arrives.
	// Firing straight from low ready would either put a round into the deck or snap the weapon
	// to the shoulder instantly, and both read as the game ignoring the position it was in.
	// Any posture change in flight delays the shot, not just the raise out of low ready. A
	// trigger pull is never thrown away; it is queued and goes off the moment the weapon is
	// settled somewhere it can be fired from.
	Me->RaiseToShoulder();
	if (const float Wait = Me->SecondsUntilReadyToFire())
	{
		PendingFireLeft = Wait;
		return;
	}
	// A laser is a beam, not a shot: TickLaser runs it for as long as the trigger stays held.
	if (CurrentFireMode() == WeaponCatalog::EFireMode::Laser) { return; }

	// ONE ROUND LEAVES. Counted here, where the shot is committed and once per trigger pull, rather
	// than beside the trace -- a shotgun sends several pellets down that path and would otherwise
	// empty the magazine in a single pull. The count stops at zero and does NOT stop the weapon
	// firing: whether an empty gun should click instead is a gameplay decision, not a readout one.
	if (W->Magazine > 0.0f)
	{
		int32& Left = MagRounds.FindOrAdd(Equipped[HeldSlot], FMath::Max(0, (int32)W->Magazine));
		Left = FMath::Max(0, Left - 1);
	}

	// Hitscan down the CAMERA's line, not the muzzle's. The reticle is what the player aimed
	// with, and a shot that leaves along the barrel instead lands somewhere else whenever the
	// weapon is not perfectly aligned with the view -- which, held in a hand, it never is.
	// Down the AIM, which is normally the camera but is not while free looking -- there the
	// camera has swung away and the rifle is still pointing where it was.
	FVector Start = Me->GetAimOrigin();
	FVector Along = Me->GetAimRotation().Vector();
	// HEIGHT OVER BORE. Down the sights the camera looks along the sight line and the barrel runs
	// parallel to it a few centimetres lower -- the catalogue's muzzle point against its optic
	// eye -- so the round leaves the MUZZLE along the BORE, and a near wall takes it that much
	// under the point of aim. ZeroRangeCm converges the two at a range instead. Off the sights
	// the shot follows the reticle's cone as before: the barrel is nowhere near the aim line then.
	if (Me->bHeightOverBore && Me->GetCarry() == EWeaponCarry::ADS && Me->WeaponMeshComponent)
	{
		const FTransform& WT = Me->WeaponMeshComponent->GetComponentTransform();
		const FVector Muzzle = WT.TransformPosition(W->Muzzle);
		Along = Me->ZeroRangeCm > 1.0f ? (Start + Along * Me->ZeroRangeCm - Muzzle).GetSafeNormal() : WT.GetUnitAxis(EAxis::X);
		Start = Muzzle;
	}
	// Inside the cone the reticle is drawing. Using the same number for both means the ring on
	// screen is a promise: a shot can land anywhere inside it and nowhere outside.
	const float SpreadDeg = Me->GetWeaponSpreadDegrees();
	// The stance's cone, then the weapon's own: a rifle that groups 5 MOA puts rounds in a circle 5
	// minutes of angle across (half that from the centre) however still the shooter is.
	const FVector Held = FMath::VRandCone(Along, FMath::DegreesToRadians(SpreadDeg));
	const FVector Aim = FMath::VRandCone(Held, FMath::DegreesToRadians(W->MechanicalMoa / 60.0f * 0.5f));
	const FVector End = Start + Aim * 20000.0f;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(WeaponFire), true, Me);
	TArray<AActor*> Attached;
	Me->GetAttachedActors(Attached, true, true);
	Params.AddIgnoredActors(Attached);
	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
	// The round's streak, muzzle to mark, and the noise of it for anyone listening.
	{
		const FVector MuzzleWorld = Me->WeaponMeshComponent ? Me->WeaponMeshComponent->GetComponentTransform().TransformPosition(W->Muzzle) : Start;
		SpawnTracer(MuzzleWorld, bHit ? Hit.ImpactPoint : End);
		UAlertnessComponent::ReportNoise(GetWorld(), Start, 2600.0f, Me);
	}

	Me->OnWeaponFired(W->Muzzle);
	// SPENT BRASS: a cartridge weapon throws its case out of the port -- right of the receiver,
	// above the grip unless the catalogue says where -- to tumble, bounce once with a tink and lie
	// there. Cells, rockets and beams leave nothing.
	{
		using WeaponCatalog::EAmmoKind;
		const bool bCartridge = W->Ammo == EAmmoKind::Light || W->Ammo == EAmmoKind::Medium || W->Ammo == EAmmoKind::Heavy || W->Ammo == EAmmoKind::Shell;
		if (bCartridge && Me->WeaponMeshComponent)
		{
			if (!Brass) { Brass = NewObject<UBrassFx>(this); }
			const FTransform& WT = Me->WeaponMeshComponent->GetComponentTransform();
			const FVector Port = W->Eject.IsNearlyZero() ? FVector(4.0f, 3.5f, 11.0f) : W->Eject;
			Brass->Eject(Me, WT.TransformPosition(Port), WT.GetUnitAxis(EAxis::Y), WT.GetUnitAxis(EAxis::Z), WT.GetUnitAxis(EAxis::X), W->Ammo == EAmmoKind::Shell);
		}
	}
	// The shot has to arrive somewhere. Without this the muzzle flashes and the world does not
	// react at all, which reads as the gun not working rather than as a miss.
	ImpactEffects::Play(GetWorld(), Hit, Me, Me->IsFirstPerson() ? ImpactScaleFirstPerson : 1.0f);
	if (bHit) { ShotReactions::React(GetWorld(), Hit, Aim, Me, W->Damage); }   // a person flinches and bleeds; a loose thing is knocked about or bursts
	const FString Wav = W->Sound.IsEmpty() ? TEXT("wep_pistol.wav")
		: (W->Sound.EndsWith(TEXT(".wav")) ? W->Sound : W->Sound + TEXT(".wav"));
	// The report, then the overpressure: everything else ducks and comes back over a second.
	UAmbientPlayer::NoteShot(ShotDuckDepth, ShotDuckSeconds);
	if (Ambient) { Ambient->Duck(GetWorld(), ShotDuckDepth, ShotDuckSeconds); }
	UAmbientPlayer::PlayOneShot(this, GetWorld(), Wav, Me->IsFirstPerson() ? ReportVolumeFirstPerson : ReportVolumeThirdPerson, FMath::FRandRange(0.94f, 1.06f), /*bIgnoreDuck=*/true);   // the report is the loudest thing the player does

	SetDiagNoteTimed(bHit
		? FString::Printf(TEXT("%s -> %s at %.0f m%s"), *W->Name, *Hit.GetActor()->GetActorNameOrLabel(), Hit.Distance / 100.0f, *(ShotReactions::LastRegion().IsEmpty() ? FString() : FString::Printf(TEXT(" (%s)"), *ShotReactions::LastRegion())))
		: FString::Printf(TEXT("%s -> miss"), *W->Name), 4.0f);
}

WeaponCatalog::EFireMode ABasePlayerController::CurrentFireMode() const
{
	using WeaponCatalog::EFireMode;
	if (!Equipped.IsValidIndex(HeldSlot)) { return EFireMode::Semi; }
	const WeaponCatalog::FWeapon* W = WeaponCatalog::Find(Equipped[HeldSlot]);
	if (!W || W->FireModes.Num() == 0) { return EFireMode::Semi; }
	return W->FireModes.Contains(FireMode) ? FireMode : W->FireModes[0];
}

void ABasePlayerController::RecoilTune(float RecoverSeconds, float RecoverFraction)
{
	if (ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn()))
	{
		Me->RecoilRecoverSeconds = FMath::Max(0.0f, RecoverSeconds); Me->RecoilRecoverFraction = FMath::Clamp(RecoverFraction, 0.0f, 1.0f);
		SetDiagNoteTimed(FString::Printf(TEXT("recoil returns %.0f%% over %.2f s"), Me->RecoilRecoverFraction * 100.0f, Me->RecoilRecoverSeconds), 3.0f);
	}
}

void ABasePlayerController::ZeroRange(float Cm)
{
	// -1 switches height over bore off, 0 on with the bore parallel, a range on and converging there.
	if (ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn())) { Me->bHeightOverBore = Cm >= 0.0f; Me->ZeroRangeCm = FMath::Max(0.0f, Cm); SetDiagNoteTimed(!Me->bHeightOverBore ? TEXT("height over bore off: shots on the sight line") : (Cm > 1.0f ? FString::Printf(TEXT("zeroed at %.0f m"), Cm / 100.0f) : TEXT("bore parallel to the sight line")), 3.0f); }
}

void ABasePlayerController::AimSens(float Scale)
{
	if (ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn())) { Me->AimSensitivityScale = FMath::Clamp(Scale, 0.05f, 1.0f); SetDiagNoteTimed(FString::Printf(TEXT("ADS mouse scale %.2f"), Me->AimSensitivityScale), 3.0f); }
}

void ABasePlayerController::AimSway(float Scale)
{
	if (ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn())) { Me->AimSwayScale = FMath::Clamp(Scale, 0.0f, 5.0f); SetDiagNoteTimed(FString::Printf(TEXT("aim sway x%.2f (now %.2f deg)"), Me->AimSwayScale, Me->AimSwayTargetDeg()), 3.0f); }
}

float ABasePlayerController::ArmourValueFor(const FString& BodySlot) const
{
	const FSheetSpec& Spec = FSheetSpec::Get();
	for (int32 i = 0; i < Spec.Slots.Num() && i < Equipped.Num(); ++i)
	{
		if (Spec.Slots[i].Name != BodySlot || Equipped[i].IsEmpty()) { continue; }
		if (const ItemCatalog::FRecord* R = ItemCatalog::FindRecord(Equipped[i])) { return (float)R->Number(TEXT("armor_value"), 0.0); }
	}
	return 0.0f;
}

AActor* ABasePlayerController::PersonUnderReticle(FVector& OutDir) const
{
	const ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn());
	if (!Me) { return nullptr; }
	FCollisionQueryParams P(SCENE_QUERY_STAT(PersonUnderReticle), true, Me);
	const FVector Start = Me->GetAimOrigin();
	OutDir = Me->GetAimRotation().Vector();
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, Start + OutDir * 6000.0f, ECC_Visibility, P)) { return nullptr; }
	return (Cast<USkeletalMeshComponent>(Hit.GetComponent()) || Cast<ABaseCharacter>(Hit.GetActor())) ? Hit.GetActor() : nullptr;
}

void ABasePlayerController::Sever(const FString& Region)
{
	static const TCHAR* Names[] = { TEXT("Head"), TEXT("ArmL"), TEXT("ArmR"), TEXT("LegL"), TEXT("LegR") };
	FString Want;
	for (const TCHAR* N : Names) { if (Region.Equals(N, ESearchCase::IgnoreCase)) { Want = N; } }
	if (Want.IsEmpty()) { SetDiagNoteTimed(TEXT("Sever: Head, ArmL, ArmR, LegL or LegR"), 4.0f); return; }
	FVector Dir;
	AActor* Who = PersonUnderReticle(Dir);
	if (!Who) { SetDiagNoteTimed(TEXT("Sever: no one under the reticle"), 3.0f); return; }
	const bool bOk = ShotReactions::Sever(GetWorld(), Who, Want, Dir);
	SetDiagNoteTimed(FString::Printf(TEXT("Sever %s: %s"), *Want, bOk ? TEXT("off") : TEXT("nothing there to take off")), 4.0f);
}

void ABasePlayerController::Kill()
{
	FVector Dir;
	AActor* Who = PersonUnderReticle(Dir);
	if (!Who) { SetDiagNoteTimed(TEXT("Kill: no one under the reticle"), 3.0f); return; }
	ShotReactions::Kill(GetWorld(), Who, Dir);
	SetDiagNoteTimed(FString::Printf(TEXT("%s is down"), *Who->GetActorNameOrLabel()), 4.0f);
}

void ABasePlayerController::Revive()
{
	if (ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn())) { Me->Revive(); SetDiagNoteTimed(TEXT("revived: vitality full, limbs back"), 4.0f); }
}

void ABasePlayerController::SwayRate(float StridesPerSecond)
{
	if (ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn())) { Me->SwayStepsPerSecond = FMath::Clamp(StridesPerSecond, 0.2f, 5.0f); SetDiagNoteTimed(FString::Printf(TEXT("sway %.2f strides/s"), Me->SwayStepsPerSecond), 3.0f); }
}

void ABasePlayerController::CycleFireMode()
{
	if (!Equipped.IsValidIndex(HeldSlot)) { return; }
	const WeaponCatalog::FWeapon* W = WeaponCatalog::Find(Equipped[HeldSlot]);
	if (!W || !W->bRanged) { return; }
	if (W->FireModes.Num() < 2) { SetDiagNoteTimed(FString::Printf(TEXT("%s: %s only"), *W->Name, *FString(WeaponCatalog::FireModeName(CurrentFireMode())).ToUpper()), 3.0f); return; }
	const int32 At = W->FireModes.IndexOfByKey(CurrentFireMode());
	FireMode = W->FireModes[(At + 1) % W->FireModes.Num()];
	AutoFireClock = 0.0f;
	UAmbientPlayer::PlayOneShot(this, GetWorld(), TEXT("switch_click.wav"), 0.5f, 1.15f);
	SetDiagNoteTimed(FString::Printf(TEXT("FIRE MODE: %s"), *FString(WeaponCatalog::FireModeName(FireMode)).ToUpper()), 3.0f);
}

void ABasePlayerController::SetTriggerHeld(bool bHeld)
{
	bTriggerHeld = bHeld;
	// A burst is the press and two more, and a released trigger does not cut it short. The beam
	// goes out with the trigger.
	if (bHeld && CurrentFireMode() == WeaponCatalog::EFireMode::Burst) { BurstLeft = 2; }
	if (!bHeld) { StopLaser(); }
	// The first shot of a held trigger was the press itself; the clock starts after it.
	AutoFireClock = 0.0f;
}

void ABasePlayerController::TickAutoFire(float DeltaSeconds)
{
	using WeaponCatalog::EFireMode;
	if (IsEditMode() || IsFiringBlocked() || !Equipped.IsValidIndex(HeldSlot)) { return; }
	const EFireMode Mode = CurrentFireMode();
	// Auto runs while the trigger is held; a burst finishes its rounds whether or not it still is.
	if (!((Mode == EFireMode::Auto && bTriggerHeld) || (Mode == EFireMode::Burst && BurstLeft > 0))) { return; }
	const WeaponCatalog::FWeapon* W = WeaponCatalog::Find(Equipped[HeldSlot]);
	if (!W || !W->bRanged) { return; }
	const float Rate = W->FireRate > 0.1f ? W->FireRate : 8.0f;
	AutoFireClock += DeltaSeconds;
	const float Interval = 1.0f / Rate;
	if (AutoFireClock < Interval) { return; }
	AutoFireClock -= Interval;
	// A shot already queued behind a posture change is not doubled up.
	if (PendingFireLeft > 0.0f) { return; }
	FireHeldWeapon();
	if (Mode == EFireMode::Burst) { --BurstLeft; }
}

void ABasePlayerController::TickLaser(float DeltaSeconds)
{
	using WeaponCatalog::EFireMode;
	ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn());
	const WeaponCatalog::FWeapon* W = (Me && Equipped.IsValidIndex(HeldSlot)) ? WeaponCatalog::Find(Equipped[HeldSlot]) : nullptr;
	const bool bWant = W && W->bRanged && bTriggerHeld && !bHolstered && !IsEditMode() && !IsFiringBlocked()
		&& CurrentFireMode() == EFireMode::Laser && Me->WeaponMeshComponent && Me->SecondsUntilReadyToFire() <= 0.0f;
	if (!bWant) { StopLaser(); return; }
	// THE BATTERY. A laser's magazine is seconds of beam; flat, it will not light again until a
	// reload puts a fresh cell in (ReloadHeldWeapon).
	const float Full = W->Magazine > 0.0f ? W->Magazine : 10.0f;
	float& Charge = LaserCharge.FindOrAdd(W->Key, Full);
	if (Charge <= 0.0f)
	{
		StopLaser();
		LaserBuzzClock -= DeltaSeconds;
		if (LaserBuzzClock <= 0.0f)
		{
			LaserBuzzClock = 0.7f;
			UAmbientPlayer::PlayOneShot(this, GetWorld(), TEXT("laser_flat.wav"), 0.6f, 1.0f);
			SetDiagNoteTimed(FString::Printf(TEXT("%s: battery flat -- reload"), *W->Name), 1.5f);
		}
		return;
	}
	Charge = FMath::Max(0.0f, Charge - DeltaSeconds);
	// From the muzzle, down the aim -- the swaying aim, so the beam wanders as the arms do -- to
	// whatever it meets. No cone: a beam is exactly as straight as its bearer is steady.
	const FTransform& WT = Me->WeaponMeshComponent->GetComponentTransform();
	const FVector Muzzle = WT.TransformPosition(W->Muzzle);
	const FVector Start = Me->GetAimOrigin();
	const FVector Dir = Me->GetAimRotation().Vector();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(LaserBeam), true, Me);
	TArray<AActor*> Attached; Me->GetAttachedActors(Attached, true, true); Params.AddIgnoredActors(Attached);
	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, Start + Dir * 20000.0f, ECC_Visibility, Params);
	const FVector End = bHit ? Hit.ImpactPoint : Start + Dir * 20000.0f;
	// THE BEAM'S PARTS belong to the character: a controller is a hidden actor, and nothing it owns
	// is ever drawn (which is why the first beam was invisible). A hot core, a wide soft glow round
	// it, and a red light where the spot is.
	if (!LaserBeam || LaserBeam->GetOwner() != Me)
	{
		static UMaterialInterface* Hot = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/RepliCan/Materials/M_LaserBeam.M_LaserBeam"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		auto MakeBeam = [&](const TCHAR* Name, float Heat, const FLinearColor& Colour) -> UStaticMeshComponent*
		{
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Me, Name);
			C->SetStaticMesh(Cylinder);
			if (Hot) { UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Hot, C); M->SetVectorParameterValue(TEXT("Colour"), Colour); M->SetScalarParameterValue(TEXT("Heat"), Heat); C->SetMaterial(0, M); }
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->SetCastShadow(false);
			C->SetAbsolute(true, true, true);
			C->RegisterComponent();
			return C;
		};
		LaserBeam = MakeBeam(TEXT("LaserBeam"), 34.0f, FLinearColor(1.0f, 0.3f, 0.16f, 1.0f));   // the core: red pushed nearly to white
		LaserGlow = MakeBeam(TEXT("LaserGlow"), 2.2f, FLinearColor(1.0f, 0.08f, 0.04f, 1.0f));   // the haze: deep red, soft-edged
		LaserLight = NewObject<UPointLightComponent>(Me, TEXT("LaserLight"));
		LaserLight->SetIntensityUnits(ELightUnits::Candelas);
		LaserLight->SetIntensity(6.0f);
		LaserLight->SetAttenuationRadius(280.0f);
		LaserLight->SetLightColor(FLinearColor(1.0f, 0.18f, 0.1f));
		LaserLight->SetCastShadows(false);
		LaserLight->SetAbsolute(true, true, true);
		LaserLight->RegisterComponent();
	}
	if (!bLaserOn)
	{
		bLaserOn = true; LaserFxClock = 1.0f; LaserReactClock = 1.0f; LaserBuzzClock = 0.0f; bLaserStroke = false;
		UAlertnessComponent::ReportNoise(GetWorld(), Muzzle, 900.0f, Me);   // the beam lighting is heard, not far
	}
	// THE BUZZ: half a second of it, re-lit before it ends, so it reads as one continuous note.
	LaserBuzzClock -= DeltaSeconds;
	if (LaserBuzzClock <= 0.0f)
	{
		LaserBuzzClock = 0.42f;
		// Kept in hand, not fired and forgotten: the release silences it THAT frame (StopLaser). A
		// one-shot left to run on buzzed for up to half a second after the trigger came up.
		LaserBuzzComps.RemoveAll([](const TWeakObjectPtr<UAudioComponent>& C) { return !C.IsValid() || !C->IsPlaying(); });
		float Seconds = 0.0f;
		if (USoundWave* Buzz = VoiceLines::LoadWav(this, FPaths::Combine(UAmbientPlayer::RawAudioDir(), TEXT("laser_buzz.wav")), Seconds))
		{
			const float Pitch = FMath::FRandRange(0.985f, 1.015f);
			if (UAudioComponent* Comp = UGameplayStatics::SpawnSound2D(GetWorld(), Buzz, Me->IsFirstPerson() ? 0.55f : 0.42f, Pitch, 0.0f, nullptr, false, false))
			{
				LaserBuzzComps.Add(Comp);
				TWeakObjectPtr<UAudioComponent> Weak = Comp; FTimerHandle H;   // a procedural wave never ends by itself
				GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([Weak]() { if (UAudioComponent* C = Weak.Get()) { C->Stop(); C->DestroyComponent(); } }), Seconds / FMath::Max(0.1f, Pitch) + 0.15f, false);
			}
		}
	}
	SetDiagNoteTimed(FString::Printf(TEXT("%s: battery %.0f%%"), *W->Name, 100.0f * Charge / Full), 0.4f);
	// The engine cylinder stands 100 cm tall on Z with a 50 cm radius, laid along the beam: the core
	// two and a half centimetres across, the glow sixteen.
	const float Len = FVector::Dist(Muzzle, End);
	const FQuat Along = FRotationMatrix::MakeFromZ((End - Muzzle).GetSafeNormal()).ToQuat();
	LaserBeam->SetWorldTransform(FTransform(Along, (Muzzle + End) * 0.5f, FVector(0.05f, 0.05f, Len / 100.0f)));
	LaserBeam->SetVisibility(true);
	if (LaserGlow) { LaserGlow->SetWorldTransform(FTransform(Along, (Muzzle + End) * 0.5f, FVector(0.16f, 0.16f, Len / 100.0f))); LaserGlow->SetVisibility(true); }
	if (LaserLight) { LaserLight->SetWorldLocation(bHit ? Hit.ImpactPoint + Hit.ImpactNormal * 6.0f : End); LaserLight->SetVisibility(bHit); }
	if (!bHit) { bLaserStroke = false; return; }
	// THE SPOT. Smoke off whatever it rests on; on a surface, the heated LINE it leaves as it moves
	// -- one stroke from where the last one ended to here, so the track is continuous, bright metal
	// cooling to black (MarkScorchStroke, TickScorches); on a body, the catalogue's damage per
	// second, landed twice a second.
	LaserFxClock += DeltaSeconds; LaserReactClock += DeltaSeconds;
	if (LaserFxClock >= 0.11f)
	{
		LaserFxClock = 0.0f;
		// Small white puffs, a hand back along the beam so a wall does not swallow them, and a spit
		// of red and yellow sparks off the spot.
		ImpactEffects::SpawnBurst(GetWorld(), { TEXT("/Game/PolygonSciFiWorlds/FX/Niagara/NS_Smoke_Large_White_01"), 0.12f, 0.08f }, Hit.ImpactPoint - Dir * 9.0f + Hit.ImpactNormal * 2.0f, Hit.ImpactNormal.Rotation());
		SparkFx::Burst(GetWorld(), Hit.ImpactPoint - Dir * 3.0f, Hit.ImpactNormal, 4, 0.7f, FLinearColor(1.0f, 0.12f, 0.05f, 1.0f), FLinearColor(1.0f, 0.8f, 0.2f, 1.0f));
	}
	const bool bBody = Hit.GetComponent() && Hit.GetComponent()->IsA<USkeletalMeshComponent>();
	if (!bBody)
	{
		const float Moved = bLaserStroke ? (float)FVector::Dist(Hit.ImpactPoint, LaserStrokeFrom) : 0.0f;
		if (bLaserStroke && Moved < 3.0f) { if (Scorches.Num() > 0) { Scorches.Last().Age = 0.0f; } }   // resting on one spot keeps it hot
		else
		{
			// Along one surface the stroke joins the last point; a jump (off an edge, onto another
			// thing) starts a new line from a dot.
			const bool bJoin = bLaserStroke && Moved < 60.0f && FVector::DotProduct(Hit.ImpactNormal, LaserStrokeNormal) > 0.9f;
			MarkScorchStroke(bJoin ? LaserStrokeFrom : Hit.ImpactPoint, Hit.ImpactPoint, Hit.ImpactNormal);
			bLaserStroke = true; LaserStrokeFrom = Hit.ImpactPoint; LaserStrokeNormal = Hit.ImpactNormal;
		}
	}
	else
	{
		bLaserStroke = false;
		// The burn on a body: a scorch dot every few centimetres of travel (or every tenth of a
		// second resting), attached to the bone under the spot so it rides the body -- a world
		// decal would stay in the air where the pilot was standing. Same material and cooling as
		// the surface track; robots and people alike.
		const float MovedOnBody = bLaserBodyMark ? (float)FVector::Dist(Hit.ImpactPoint, LaserBodyMarkFrom) : 1000.0f;
		if (MovedOnBody >= 4.0f || LaserBodyMarkClock >= 0.1f)
		{
			MarkScorchOnBody(Hit); LaserBodyMarkClock = 0.0f; bLaserBodyMark = true; LaserBodyMarkFrom = Hit.ImpactPoint;
		}
		else { LaserBodyMarkClock += DeltaSeconds; }
		if (LaserReactClock >= 0.5f) { LaserReactClock = 0.0f; ShotReactions::React(GetWorld(), Hit, Dir, Me, W->Damage * 0.5f); }
	}
}

void ABasePlayerController::MarkScorchOnBody(const FHitResult& Hit)
{
	static UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/RepliCan/Materials/M_LaserScorch.M_LaserScorch"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	UPrimitiveComponent* Comp = Hit.GetComponent();
	if (!Mat || !Comp || !GetWorld()) { return; }
	const FRotator Rot = FRotationMatrix::MakeFromX(-Hit.ImpactNormal).Rotator();
	// A round dot (the stroke's mask, unstretched), a little larger than the beam so it reads on
	// cloth and plate; pinned to the bone so it moves with the limb.
	UDecalComponent* D = UGameplayStatics::SpawnDecalAttached(Mat, FVector(4.0f, 2.6f, 2.6f), Comp, Hit.BoneName, Hit.ImpactPoint, Rot, EAttachLocation::KeepWorldPosition, 140.0f);
	if (!D) { return; }
	D->SetFadeOut(120.0f, 20.0f, false);
	D->SetFadeScreenSize(0.0003f);
	UMaterialInstanceDynamic* MID = D->CreateDynamicMaterialInstance();
	if (MID) { MID->SetScalarParameterValue(TEXT("Heat"), 1.0f); }
	FScorch Sc; Sc.Decal = D; Sc.MID = MID; Scorches.Add(Sc);
	ScorchRing.Add(D);
	while (ScorchRing.Num() > 360) { if (ScorchRing[0].IsValid()) { ScorchRing[0]->DestroyComponent(); } ScorchRing.RemoveAt(0); }
}

void ABasePlayerController::MarkScorchStroke(const FVector& From, const FVector& To, const FVector& Normal)
{
	static UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/RepliCan/Materials/M_LaserScorch.M_LaserScorch"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Mat || !GetWorld()) { return; }
	// A decal box: its X projects into the surface (the inverse normal, see ImpactEffects), its Y
	// runs along the stroke, its Z is the line's width. The soft round mask stretched along Y is a
	// soft bar, and consecutive bars overlap by the width, so the track reads as one line.
	const float Len = (float)FVector::Dist(From, To);
	FVector Dir = (To - From).GetSafeNormal();
	if (Dir.IsNearlyZero()) { Dir = FVector::CrossProduct(Normal, FVector::UpVector).GetSafeNormal(); }
	if (Dir.IsNearlyZero()) { Dir = FVector::RightVector; }
	const FRotator Rot = FRotationMatrix::MakeFromXY(-Normal, Dir).Rotator();
	const float HalfWidth = 2.1f;
	UDecalComponent* D = UGameplayStatics::SpawnDecalAtLocation(GetWorld(), Mat, FVector(3.0f, Len * 0.5f + HalfWidth, HalfWidth), (From + To) * 0.5f, Rot, 140.0f);
	if (!D) { return; }
	D->SetFadeOut(120.0f, 20.0f, false);
	D->SetFadeScreenSize(0.0003f);
	UMaterialInstanceDynamic* MID = D->CreateDynamicMaterialInstance();
	if (MID) { MID->SetScalarParameterValue(TEXT("Heat"), 1.0f); }
	FScorch Sc; Sc.Decal = D; Sc.MID = MID; Scorches.Add(Sc);
	// A long sweep is a lot of decals: past a few hundred the oldest go.
	ScorchRing.Add(D);
	while (ScorchRing.Num() > 360) { if (ScorchRing[0].IsValid()) { ScorchRing[0]->DestroyComponent(); } ScorchRing.RemoveAt(0); }
}

void ABasePlayerController::TickScorches(float DeltaSeconds)
{
	for (int32 i = Scorches.Num() - 1; i >= 0; --i)
	{
		FScorch& S = Scorches[i];
		S.Age += DeltaSeconds;
		if (!S.Decal.IsValid() || !S.MID.IsValid() || S.Age > 4.0f) { if (S.MID.IsValid()) { S.MID->SetScalarParameterValue(TEXT("Heat"), 0.0f); } Scorches.RemoveAt(i); continue; }
		S.MID->SetScalarParameterValue(TEXT("Heat"), FMath::Exp(-S.Age * 2.4f));   // white-hot to black in a second and a half
	}
}

void ABasePlayerController::SpawnTracer(const FVector& From, const FVector& To)
{
	ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn());
	if (!Me || !GetWorld() || FVector::DistSquared(From, To) < 40.0f * 40.0f) { return; }   // nothing to see in under half a metre
	// The streaks belong to the character: a controller is a hidden actor and nothing it owns is drawn.
	if (TracerPool.Num() > 0 && TracerPool[0] && TracerPool[0]->GetOwner() != Me)
	{
		for (const TObjectPtr<UStaticMeshComponent>& C : TracerPool) { if (C) { C->DestroyComponent(); } }
		TracerPool.Reset(); Tracers.Reset();
	}
	int32 Slot = -1;
	for (int32 i = 0; i < TracerPool.Num() && Slot < 0; ++i)
	{
		bool bBusy = false;
		for (const FTracer& T : Tracers) { if (T.Pool == i) { bBusy = true; break; } }
		if (!bBusy) { Slot = i; }
	}
	if (Slot < 0)
	{
		if (TracerPool.Num() >= 24) { return; }
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Me, *FString::Printf(TEXT("Tracer_%d"), TracerPool.Num()));
		C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
		// A DARK GREY STREAK (user's call, 2026-09-17), not a hot one: M_TracerStreak is unlit and
		// translucent, because the beam's additive material cannot draw anything darker than what
		// is behind it. The beam material stays as the fallback if the streak one is missing.
		static UMaterialInterface* Streak = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/RepliCan/Materials/M_TracerStreak.M_TracerStreak"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		static UMaterialInterface* Hot = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/RepliCan/Materials/M_LaserBeam.M_LaserBeam"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (Streak)
		{
			UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Streak, this);
			M->SetVectorParameterValue(TEXT("Colour"), FLinearColor(0.07f, 0.07f, 0.07f, 1.0f));
			M->SetScalarParameterValue(TEXT("Opacity"), 0.8f);
			C->SetMaterial(0, M);
		}
		else if (Hot)
		{
			UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Hot, this);
			M->SetVectorParameterValue(TEXT("Colour"), FLinearColor(0.25f, 0.22f, 0.2f, 1.0f));
			M->SetScalarParameterValue(TEXT("Heat"), 1.0f);
			C->SetMaterial(0, M);
		}
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		C->SetAbsolute(true, true, true);
		C->RegisterComponent();
		C->SetVisibility(false);
		Slot = TracerPool.Add(C);
	}
	FTracer T; T.Pool = Slot; T.From = From; T.To = To; T.Head = 0.0f;
	Tracers.Add(T);
}

void ABasePlayerController::TickTracers(float DeltaSeconds)
{
	// A streak a metre long moving at ninety metres a second: a round crossing a room is seen for
	// a tenth of a second, which is about what a real tracer gives the eye.
	const float Speed = 9000.0f, Length = 110.0f;
	for (int32 i = Tracers.Num() - 1; i >= 0; --i)
	{
		FTracer& T = Tracers[i];
		UStaticMeshComponent* C = TracerPool.IsValidIndex(T.Pool) ? TracerPool[T.Pool].Get() : nullptr;
		const float Total = FVector::Dist(T.From, T.To);
		T.Head += Speed * DeltaSeconds;
		const float Tail = T.Head - Length;
		if (!C || Tail >= Total) { if (C) { C->SetVisibility(false); } Tracers.RemoveAt(i); continue; }
		const float A = FMath::Max(0.0f, Tail), B = FMath::Min(Total, T.Head);
		if (B - A < 1.0f) { continue; }
		const FVector Dir = (T.To - T.From) / Total;
		const FVector P0 = T.From + Dir * A, P1 = T.From + Dir * B;
		C->SetWorldTransform(FTransform(FRotationMatrix::MakeFromZ(Dir).ToQuat(), (P0 + P1) * 0.5f, FVector(0.024f, 0.024f, (B - A) / 100.0f)));
		C->SetVisibility(true);
	}
}

void ABasePlayerController::StopLaser()
{
	if (!bLaserOn) { return; }
	bLaserOn = false; bLaserStroke = false; bLaserBodyMark = false;
	for (const TWeakObjectPtr<UAudioComponent>& C : LaserBuzzComps) { if (UAudioComponent* A = C.Get()) { A->Stop(); A->DestroyComponent(); } }   // silent the instant the click ends
	LaserBuzzComps.Reset();
	if (LaserBeam) { LaserBeam->SetVisibility(false); }
	if (LaserGlow) { LaserGlow->SetVisibility(false); }
	if (LaserLight) { LaserLight->SetVisibility(false); }
}

void ABasePlayerController::MeleeSwing()
{
	ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn());
	const WeaponCatalog::FWeapon* W = (Me && Equipped.IsValidIndex(HeldSlot)) ? WeaponCatalog::Find(Equipped[HeldSlot]) : nullptr;
	if (!Me || !W || W->bRanged || bHolstered || Me->IsDead() || W->Kind == TEXT("Shield")) { return; }
	// Mid-swing the press is kept, and becomes the next step when this one lands.
	if (MeleeSwingClock >= 0.0f) { bMeleeQueued = true; return; }
	if (Me->IsWeaponBusy()) { return; }   // a reload, a draw
	// Another press inside the recovery window takes the next step of the combo; otherwise it starts over.
	MeleeComboStep = (GetWorld()->GetTimeSeconds() < MeleeComboUntil) ? (MeleeComboStep + 1) % 3 : 0;
	StartMeleeStep(Me, W);
}

void ABasePlayerController::StartMeleeStep(ABaseCharacter* Me, const WeaponCatalog::FWeapon* W)
{
	static const TCHAR* Letters[] = { TEXT("A"), TEXT("B"), TEXT("C") };
	const FString Family = W->AttackSet == TEXT("heavy") ? TEXT("HeavyCombo01") : TEXT("LightCombo01");
	const FString Path = FString::Printf(TEXT("/Game/Characters/Animations/SyntySwordCombat/Attack/%s/A_Attack_%s%s_Sword"), *Family, *Family, Letters[MeleeComboStep]);
	UAnimSequence* Clip = LoadObject<UAnimSequence>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Clip || !Me->PlayMeleeClip(Clip)) { SetDiagNoteTimed(FString::Printf(TEXT("no swing clip for %s"), *W->Name), 3.0f); return; }
	MeleeSwingLength = Clip->GetPlayLength();
	MeleeSwingClock = 0.0f;
	MeleeComboUntil = GetWorld()->GetTimeSeconds() + MeleeSwingLength + 0.6f;
	bMeleeQueued = false;
	MeleeHitThisSwing.Reset(); MeleeCleaved.Reset(); MeleeLastEdge.Reset();
	MeleeBudget = W->Damage;
	Me->SetAiming(false);
	UAmbientPlayer::PlayOneShot(this, GetWorld(), W->AttackSet == TEXT("heavy") ? TEXT("swing_heavy.wav") : TEXT("swing_light.wav"), 0.7f, FMath::FRandRange(0.92f, 1.08f));
}

void ABasePlayerController::MeleeStop(ABaseCharacter* Me, bool bHeavy)
{
	// The blade stopped in it: a beat of frozen time for the character, then the arm eases back to
	// the stance and the recovery window runs from here, so the next step can follow the stop.
	MeleeSwingClock = -1.0f; MeleeLastEdge.Reset();
	MeleeComboUntil = GetWorld()->GetTimeSeconds() + 0.6f;
	if (!Me) { return; }
	Me->CustomTimeDilation = 0.05f;
	Me->WeaponActionLeft = FMath::Min(Me->WeaponActionLeft, 0.08f);
	TWeakObjectPtr<ABaseCharacter> Weak(Me);
	GetWorld()->GetTimerManager().SetTimer(MeleeStopTimer, FTimerDelegate::CreateLambda([Weak]() { if (Weak.IsValid()) { Weak->CustomTimeDilation = 1.0f; } }), bHeavy ? 0.12f : 0.06f, false);
}

void ABasePlayerController::TickMelee(float DeltaSeconds)
{
	if (MeleeSwingClock < 0.0f) { return; }
	MeleeSwingClock += DeltaSeconds;
	ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn());
	const WeaponCatalog::FWeapon* W = (Me && Equipped.IsValidIndex(HeldSlot)) ? WeaponCatalog::Find(Equipped[HeldSlot]) : nullptr;
	if (!Me || !W || W->bRanged || !Me->WeaponMeshComponent) { MeleeSwingClock = -1.0f; return; }
	const float T = MeleeSwingClock / FMath::Max(0.05f, MeleeSwingLength);
	const bool bHeavy = W->AttackSet == TEXT("heavy");
	// The window in which the edge is live: the middle of the swing, later and longer for a heavy one.
	if (T >= (bHeavy ? 0.3f : 0.22f) && T <= (bHeavy ? 0.7f : 0.6f))
	{
		const FTransform& WT = Me->WeaponMeshComponent->GetComponentTransform();
		const FVector Grip = WT.GetLocation();
		const FVector Tip = WT.TransformPosition(W->Muzzle.IsNearlyZero() ? FVector(60.0f, 0.0f, 0.0f) : W->Muzzle);
		// Five points along the outer two thirds of the edge, each swept from where it was last frame.
		TArray<FVector> Edge;
		for (int32 i = 0; i < 5; ++i) { Edge.Add(FMath::Lerp(Grip, Tip, 0.35f + 0.65f * i / 4.0f)); }
		if (MeleeLastEdge.Num() == Edge.Num())
		{
			FCollisionQueryParams P(SCENE_QUERY_STAT(MeleeSweep), true, Me);
			TArray<AActor*> Attached; Me->GetAttachedActors(Attached, true, true); P.AddIgnoredActors(Attached);
			bool bStopped = false;
			for (int32 i = 0; i < Edge.Num() && !bStopped; ++i)
			{
				// Everything the point crossed this frame, nearest first: the edge goes through as far as the budget lasts.
				TArray<FHitResult> Hits;
				GetWorld()->LineTraceMultiByChannel(Hits, MeleeLastEdge[i], Edge[i], ECC_Visibility, P, FCollisionResponseParams(ECR_Overlap));
				for (const FHitResult& Hit : Hits)
				{
					AActor* A = Hit.GetActor();
					if (!A) { continue; }
					if (MeleeHitThisSwing.Contains(A) && !MeleeCleaved.Contains(A)) { continue; }
					MeleeHitThisSwing.Add(A); MeleeCleaved.Remove(A);
					FVector Swing = (Edge[i] - MeleeLastEdge[i]).GetSafeNormal();
					if (Swing.IsNearlyZero()) { Swing = Me->GetActorForwardVector(); }
					ImpactEffects::Play(GetWorld(), Hit, Me, 0.6f);
					const ShotReactions::FStrike St = ShotReactions::Strike(GetWorld(), Hit, Swing, Me, MeleeBudget, W->bBlunt);
					const FString Region = ShotReactions::LastRegion();
					MeleeBudget -= St.Spent;
					if (St.bDestroyed && !St.bDied) { MeleeCleaved.Add(A); }
					const bool bThrough = MeleeBudget > 0.5f && !W->bBlunt;
					SetDiagNoteTimed(FString::Printf(TEXT("%s -> %s%s: %s%.0f"), *W->Name, *A->GetActorNameOrLabel(), *(Region.IsEmpty() ? FString() : FString::Printf(TEXT(" (%s)"), *Region)), bThrough ? TEXT("through, ") : TEXT("stopped, "), FMath::Max(0.0f, MeleeBudget)), 4.0f);
					if (!bThrough) { MeleeStop(Me, bHeavy); bStopped = true; break; }
				}
			}
			if (bStopped) { return; }
		}
		MeleeLastEdge = Edge;
	}
	else { MeleeLastEdge.Reset(); }
	if (MeleeSwingClock >= MeleeSwingLength)
	{
		MeleeSwingClock = -1.0f;
		if (bMeleeQueued && GetWorld()->GetTimeSeconds() < MeleeComboUntil) { MeleeComboStep = (MeleeComboStep + 1) % 3; StartMeleeStep(Me, W); }
		bMeleeQueued = false;
	}
}

void ABasePlayerController::BeginWeaponSwap(int32 NewSlot)
{
	PendingSwapSlot = NewSlot;
	PendingSwapLeft = (HeldSlot >= 0 && !bHolstered) ? 0.12f : 0.0f;   // a beat to put the old one away; none when the hands are empty
	if (PendingSwapLeft <= 0.0f) { TickPendingSwap(0.0f); }
}

void ABasePlayerController::TickPendingSwap(float DeltaSeconds)
{
	ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn());
	if (PendingShowLeft > 0.0f)
	{
		PendingShowLeft -= DeltaSeconds;
		if (PendingShowLeft <= 0.0f && Me)
		{
			PendingShowLeft = 0.0f;
			if (Me->WeaponMeshComponent) { Me->WeaponMeshComponent->SetVisibility(true); }
			if (Me->OpticMeshComponent) { Me->OpticMeshComponent->SetVisibility(Me->HasOpticSight()); }
		}
	}
	if (PendingSwapSlot < 0) { return; }
	PendingSwapLeft -= DeltaSeconds;
	if (PendingSwapLeft > 0.0f) { return; }
	const int32 Slot = PendingSwapSlot;
	PendingSwapSlot = -1; PendingSwapLeft = 0.0f;
	if (!Equipped.IsValidIndex(Slot) || Equipped[Slot].IsEmpty()) { return; }
	bHolstered = false;
	// The re-equip trusts StowedSlot over HeldSlot (a pickup must not snatch the weapon out of the
	// hand), so both move, or the next refresh puts the old one back.
	StowedSlot = HeldSlot = Slot;
	RefreshHeldWeapon();
	if (Me && Me->PlayWeaponAction(TEXT("Equip")))
	{
		// Out of nowhere for the clip's first third, then in the hand for the rest of the draw.
		if (Me->WeaponMeshComponent) { Me->WeaponMeshComponent->SetVisibility(false); }
		if (Me->OpticMeshComponent) { Me->OpticMeshComponent->SetVisibility(false); }
		PendingShowLeft = FMath::Max(0.05f, Me->WeaponActionLeft * 0.3f);
	}
}

void ABasePlayerController::TickPendingFire(float DeltaSeconds)
{
	if (PendingFireLeft <= 0.0f) { return; }
	PendingFireLeft -= DeltaSeconds;
	if (PendingFireLeft > 0.0f) { return; }
	PendingFireLeft = 0.0f;
	// The weapon is up now. Only if the finger is still on the trigger: a pull that was let go
	// while the weapon came up was a change of mind, not a shot to deliver late (user's call,
	// 2026-09-17). Auto and burst are served by the held trigger from here on anyway.
	if (!bTriggerHeld && !IsInputKeyDown(InputBindings::KeyFor(TEXT("Fire")))) { return; }
	FireHeldWeapon();
}

void ABasePlayerController::TickFreelookSafety()
{
	ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn());
	if (!Me || !Me->IsFreelook()) { return; }
	// Alt-tabbing away eats the key-up, and without this he would stay in free look for the
	// rest of the session with no way to notice why aiming had stopped working.
	const FKey Key = InputBindings::KeyFor(TEXT("Freelook"));
	if (!Key.IsValid() || !IsInputKeyDown(Key) || IsAnyScreenOpen())
	{
		Me->SetFreelook(false);
	}
}

bool ABasePlayerController::IsPageOpen() const
{
	return Screens.AnyOpen()
		|| IsInCinematic() || IsInConversation() || IsBoothActive();
}

void ABasePlayerController::TickAmbientZone(float DeltaSeconds)
{
	// Twice a second: which bed this height wants, and a restart of the ambient player if it is
	// not the one playing. A restart is a hard cut between two beds; the lift ride covers it.
	AmbientZoneClock += DeltaSeconds;
	if (AmbientZoneClock < 0.5f || !Ambient || !GetPawn() || AmbientProfile.IsEmpty()) { return; }
	AmbientZoneClock = 0.0f;
	const FString Want = GetPawn()->GetActorLocation().Z < DeckAmbientBelowZ ? FString(TEXT("deck")) : AmbientProfile;
	if (Ambient->GetProfile() != Want) { Ambient->Start(GetWorld(), Want); }
}

bool ABasePlayerController::IsFiringBlocked() const
{
	return IsPageOpen() || IsRemoteViewOpen();
}

bool ABasePlayerController::IsAnyScreenOpen() const
{
	return Screens.AnyOpen()
		|| IsInCinematic() || IsInConversation() || IsInspectMenuOpen() || IsRemoteViewOpen() || IsBoothActive();
}

void ABasePlayerController::LowerWeaponForScreen()
{
	if (ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn())) { Me->SetAiming(false); }
}

void ABasePlayerController::ToggleHolster()
{
	bHolstered = !bHolstered;
	RefreshHeldWeapon();
	SetDiagNoteTimed(bHolstered ? TEXT("Weapon stowed") : TEXT("Weapon drawn"), 2.0f);
}

int32 ABasePlayerController::RoundsLeft(const FString& Handle) const
{
	if (const int32* At = MagRounds.Find(Handle)) { return *At; }
	const WeaponCatalog::FWeapon* W = WeaponCatalog::Find(Handle);
	return W ? FMath::Max(0, (int32)W->Magazine) : 0;
}

bool ABasePlayerController::OpticOverlayInfo(float& OutRadius, float& OutAlpha, bool& bOutBlocked) const
{
	const ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn());
	if (!Me || !Me->OpticUsesOverlay()) { return false; }
	const float Up = FMath::Clamp(Me->AdsAlpha(), 0.0f, 1.0f);
	if (Up <= 0.35f) { return false; }   // below this the weapon is still on its way up
	// The mask arrives over the last part of the raise, so the picture opens rather than appearing.
	OutAlpha = FMath::Clamp((Up - 0.35f) / 0.35f, 0.0f, 1.0f);
	// THE OPENING CLOSES WHEN THE WEAPON IS NOT SETTLED -- see ABaseCharacter::OpticSettle. A strong
	// scope has a tighter eyebox than a weak one, so its ring closes further for the same wobble.
	const float Settle = Me->OpticSettle();
	const float Tight = FMath::Clamp(0.38f - 0.02f * FMath::Max(0.0f, Me->GetWeaponOpticZoom() - 2.0f), 0.24f, 0.40f);
	OutRadius = Tight * (0.62f + 0.38f * Settle);
	bOutBlocked = Me->IsOpticBlocked();
	return true;
}

bool ABasePlayerController::OpticReticleInfo(FString& OutKind, FLinearColor& OutColour, float& OutZoom, float& OutAlpha) const
{
	const ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn());
	if (!Me) { return false; }
	OutKind = Me->GetOpticReticle();
	if (OutKind.IsEmpty()) { return false; }          // no sight fitted, or one with nothing to show
	OutColour = Me->GetOpticReticleColour();
	OutZoom = Me->GetWeaponOpticZoom();
	// Fades with the weapon coming up rather than snapping on with the button: the glass is not in
	// front of your eye until the weapon is.
	OutAlpha = FMath::Clamp(Me->AdsAlpha(), 0.0f, 1.0f);
	return OutAlpha > 0.02f;
}

bool ABasePlayerController::SmartOpticInfo(float& OutRangeM, int32& OutRounds, int32& OutMag) const
{
	const ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn());
	if (!Me || !Me->HasSmartOptic() || Me->GetCarry() != EWeaponCarry::ADS) { return false; }
	if (!Equipped.IsValidIndex(HeldSlot)) { return false; }
	const FString Handle = Equipped[HeldSlot];
	const WeaponCatalog::FWeapon* W = WeaponCatalog::Find(Handle);
	if (!W) { return false; }
	OutMag = FMath::Max(0, (int32)W->Magazine);
	OutRounds = RoundsLeft(Handle);
	// RANGE TO WHAT IS UNDER THE RETICLE, down the same line the shot will take. Nothing in front
	// reads as no range rather than as the far end of the trace, which would be a lie dressed as a
	// number.
	OutRangeM = 0.0f;
	if (const UWorld* World = GetWorld())
	{
		const FVector Start = Me->GetAimOrigin();
		const FVector Along = Me->GetAimRotation().Vector();
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(SmartOptic), true, Me);
		if (World->LineTraceSingleByChannel(Hit, Start, Start + Along * 50000.0f, ECC_Visibility, Q))
		{
			OutRangeM = Hit.Distance * 0.01f;
		}
	}
	return true;
}

void ABasePlayerController::ReloadHeldWeapon()
{
	ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn());
	if (!Me || HeldSlot < 0) { return; }
	if (Me->IsWeaponBusy()) { return; }
	const WeaponCatalog::FWeapon* W = Equipped.IsValidIndex(HeldSlot) ? WeaponCatalog::Find(Equipped[HeldSlot]) : nullptr;
	if (!W || !W->bRanged) { return; }
	// A laser: a fresh cell, whatever was left in the old one.
	if (W->FireModes.Contains(WeaponCatalog::EFireMode::Laser)) { LaserCharge.FindOrAdd(W->Key) = W->Magazine > 0.0f ? W->Magazine : 10.0f; StopLaser(); }
	if (W->Magazine > 0.0f) { MagRounds.FindOrAdd(Equipped[HeldSlot]) = (int32)W->Magazine; }   // a fresh magazine
	if (!Me->PlayWeaponAction(TEXT("Reload")))
	{
		SetDiagNoteTimed(FString::Printf(TEXT("No reload animation for a %s"), *W->Stance), 3.0f);
		return;
	}
	// Reloading lowers the weapon, so it cancels the sights rather than fighting them.
	Me->SetAiming(false);
	// The sound of it: a mag out, a mag in, the action worked (RawAudio/reload_<stance>.wav, Tools/make_reload_sounds.py).
	const FString Family = W->Stance.StartsWith(TEXT("Pistol")) ? TEXT("pistol") : W->Stance == TEXT("Shotgun") ? TEXT("shotgun") : TEXT("rifle");
	UAmbientPlayer::PlayOneShot(this, GetWorld(), FString::Printf(TEXT("reload_%s.wav"), *Family), Me->IsFirstPerson() ? 0.9f : 0.7f, FMath::FRandRange(0.97f, 1.03f));
	SetDiagNoteTimed(FString::Printf(TEXT("Reloading %s"), *W->Name), 2.0f);
}

void ABasePlayerController::MeleeHeldWeapon()
{
	ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn());
	if (!Me || HeldSlot < 0 || Me->IsWeaponBusy()) { return; }
	Me->PlayWeaponAction(TEXT("Melee"));
}

void ABasePlayerController::AutoEquipPickup(const FString& Name)
{
	// A weapon picked up goes straight to an empty weapon slot rather than the bag, and if the
	// hands were empty -- nothing held, nothing stowed -- it is drawn.
	const WeaponCatalog::FWeapon* W = WeaponCatalog::Find(Name);
	if (!W || W->Kind == TEXT("Shield")) { return; }
	const FSheetSpec& Spec = FSheetSpec::Get();
	if (Equipped.Num() != Spec.Slots.Num()) { Equipped.SetNum(Spec.Slots.Num()); }
	int32 Slot = -1;
	for (int32 i = 0; i < Spec.Slots.Num() && i < Equipped.Num(); ++i)
	{
		if (Spec.Slots[i].Name.StartsWith(TEXT("Slot ")) && Spec.Slots[i].bEnabled && Equipped[i].IsEmpty()) { Slot = i; break; }
	}
	if (Slot < 0) { return; }
	const int32 At = Inventory.FindLast(Name);
	if (At == INDEX_NONE) { return; }
	if (At == Inventory.Num() - 1) { Inventory.RemoveAt(At); } else { Inventory[At].Empty(); }
	Equipped[Slot] = Name;
	const bool bHandsEmpty = HeldSlot < 0 && StowedSlot < 0;
	if (bHandsEmpty) { EquipWeaponSlot(FCString::Atoi(*Spec.Slots[Slot].Name.Mid(5))); }   // drawn, with the draw
	else { RefreshHeldWeapon(); }
	ShowCallout(GetPawn(), FString::Printf(TEXT("%s to %s%s"), *WeaponCatalog::DisplayName(Name), *Spec.Slots[Slot].Name, bHandsEmpty ? TEXT(", in hand") : TEXT("")), 2.5f, false);
}

// ONE GARMENT FOR EITHER BODY (2026-09-17): a jacket is "Junker jacket", not a men's and a
// women's; its entry carries the cut-library part for each body (wear_torso_male,
// wear_torso_female, ...) and the wearer's sex picks. A plain wear_torso still works for a
// garment cut for one body only, with wear_sex saying which.
static FString WearFieldFor(const ItemCatalog::FRecord* R, const TCHAR* Base, const FString& Sex)
{
	if (!R) { return FString(); }
	if (!Sex.IsEmpty())
	{
		const FString Sexed = R->Get(*(FString(Base) + TEXT("_") + Sex.ToLower()));
		if (!Sexed.IsEmpty()) { return Sexed; }
	}
	return R->Get(Base);
}
static bool IsWearable(const ItemCatalog::FRecord* R)
{
	static const TCHAR* Keys[] = { TEXT("wear_torso"), TEXT("wear_arms"), TEXT("wear_legs"), TEXT("wear_torso_male"), TEXT("wear_torso_female"), TEXT("wear_arms_male"), TEXT("wear_arms_female"), TEXT("wear_legs_male"), TEXT("wear_legs_female") };
	for (const TCHAR* K : Keys) { if (R && !R->Get(K).IsEmpty()) { return true; } }
	return false;
}

bool ABasePlayerController::ClothingFits(const FString& Item, FString& OutWhy) const
{
	const ItemCatalog::FRecord* R = ItemCatalog::FindRecord(Item);
	if (!R || !IsWearable(R)) { return true; }
	const ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn());
	if (!Me || !Me->GetCharacterConfig().WearsCutParts()) { OutWhy = TEXT("Nothing to put it on."); return false; }
	const FString Sex = Me->GetCharacterConfig().Gender;
	// A garment cut for one body only: wear_sex names it and there is no part for the other.
	const FString Only = R->Get(TEXT("wear_sex"));
	const bool bHasMine = !WearFieldFor(R, TEXT("wear_torso"), Sex).IsEmpty() || !WearFieldFor(R, TEXT("wear_arms"), Sex).IsEmpty() || !WearFieldFor(R, TEXT("wear_legs"), Sex).IsEmpty();
	if (!Only.IsEmpty() && !Only.Equals(Sex, ESearchCase::IgnoreCase) && !bHasMine) { OutWhy = TEXT("It would not fit you."); return false; }
	return true;
}

void ABasePlayerController::RefreshWornClothing()
{
	ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn());
	if (!Me || !Me->GetCharacterConfig().WearsCutParts()) { return; }
	// What the slots say the body wears, part by part; a part no garment covers goes back to the
	// body's own, remembered the first time something covered it. The wearer's sex picks the
	// part where a garment carries one per body.
	static const TCHAR* Slots[] = { TEXT("CutTorso"), TEXT("CutArms"), TEXT("CutLegs") };
	static const TCHAR* Fields[] = { TEXT("wear_torso"), TEXT("wear_arms"), TEXT("wear_legs") };
	const FString Sex = Me->GetCharacterConfig().Gender;
	for (int32 k = 0; k < 3; ++k)
	{
		FString Want;
		for (const FString& Item : Equipped)
		{
			if (Item.IsEmpty()) { continue; }
			const ItemCatalog::FRecord* R = ItemCatalog::FindRecord(Item);
			const FString P = WearFieldFor(R, Fields[k], Sex);
			if (!P.IsEmpty()) { Want = P; }
		}
		const FString Now = Me->GetCharacterConfig().Parts.FindRef(Slots[k]);
		if (!Want.IsEmpty())
		{
			if (!WornBase.Contains(Slots[k])) { WornBase.Add(Slots[k], Now); }
			if (Now != Want) { Me->SetPart(Slots[k], Want); }
		}
		else if (const FString* Base = WornBase.Find(Slots[k]))
		{
			if (!Base->IsEmpty() && Now != *Base) { Me->SetPart(Slots[k], *Base); }
			WornBase.Remove(Slots[k]);
		}
	}
}

int32 ABasePlayerController::InventoryFree() const
{
	int32 Used = 0;
	for (const FString& It : Inventory) { if (!It.IsEmpty()) { ++Used; } }
	return InventoryCapacity - Used;
}

AActor* ABasePlayerController::DropToWorld(const FString& Name)
{
	APawn* Me = GetPawn(); UWorld* World = GetWorld();
	if (!Me || !World || Name.IsEmpty()) { return nullptr; }
	// Where it goes: a forearm's length out in front at waist height, then a gentle toss on from there.
	const FRotator Facing(0.0f, Me->GetActorRotation().Yaw, 0.0f);
	const FVector Fwd = Facing.Vector();
	const FVector Spot = Me->GetActorLocation() + Fwd * 55.0f + FVector(0.0f, 0.0f, 10.0f);
	const FName NameTag(*(TEXT("name:") + Name));
	AActor* Prop = nullptr;
	// The actor Take put away, if this is one of those: everything about it comes back as it was.
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->IsHidden() && It->ActorHasTag(TEXT("inspectable")) && It->ActorHasTag(NameTag)) { Prop = *It; break; }
	}
	UStaticMeshComponent* C = nullptr;
	if (Prop)
	{
		Prop->SetActorHiddenInGame(false);
		Prop->SetActorEnableCollision(true);
		C = Prop->FindComponentByClass<UStaticMeshComponent>();
	}
	else
	{
		FString MeshPath;
		if (const WeaponCatalog::FWeapon* W = WeaponCatalog::Find(Name)) { MeshPath = W->MeshPath; }
		else if (const ItemCatalog::FRecord* R = ItemCatalog::FindRecord(Name)) { MeshPath = R->Mesh; }
		if (!MeshPath.IsEmpty() && !MeshPath.Contains(TEXT("."))) { MeshPath += TEXT(".") + FPackageName::GetShortName(MeshPath); }
		UStaticMesh* Mesh = MeshPath.IsEmpty() ? nullptr : LoadObject<UStaticMesh>(nullptr, *MeshPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Mesh) { return nullptr; }
		FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* SM = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Spot, Facing, P);
		if (!SM) { return nullptr; }
		SM->SetMobility(EComponentMobility::Movable);
		C = SM->GetStaticMeshComponent();
		C->SetStaticMesh(Mesh);
		SM->Tags.Add(TEXT("inspectable")); SM->Tags.Add(NameTag); SM->Tags.Add(TEXT("action:Take"));
		Prop = SM;
	}
	Prop->SetActorLocationAndRotation(Spot, FRotator(0.0f, Facing.Yaw + 90.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
	if (C && C->GetStaticMesh())
	{
		UBodySetup* Body = C->GetStaticMesh()->GetBodySetup();
		const bool bCanSimulate = Body && Body->CollisionTraceFlag != CTF_UseComplexAsSimple && Body->AggGeom.GetElementCount() > 0;
		if (bCanSimulate)
		{
			C->SetMobility(EComponentMobility::Movable);
			C->SetCollisionProfileName(TEXT("PhysicsActor"));
			C->SetSimulatePhysics(true);
			C->WakeAllRigidBodies();
			// IT HAS TO COME TO REST. Dropped with a couple of hundred degrees of spin on every axis
			// and no damping at all, a rifle skates and pirouettes on the floor for as long as you
			// watch it -- and a thin, flat shape whose collision is a handful of boxes grinds its way
			// part into the floor while it does. A gentler tumble plus damping lets it turn over once
			// or twice and stop, which is what dropping something looks like.
			C->SetLinearDamping(1.2f);
			C->SetAngularDamping(4.5f);
			C->AddImpulse(Fwd * 150.0f + FVector(0.0f, 0.0f, 55.0f), NAME_None, true);
			C->AddAngularImpulseInDegrees(FVector(FMath::FRandRange(-70.f, 70.f), FMath::FRandRange(-70.f, 70.f), FMath::FRandRange(-70.f, 70.f)), NAME_None, true);
			Prop->Tags.AddUnique(TEXT("loose"));
		}
		else
		{
			// No simple collision to fall with: set down on whatever is under the spot, resting on its bounds.
			FHitResult Floor; FCollisionQueryParams Q(SCENE_QUERY_STAT(Drop), false, Me); Q.AddIgnoredActor(Prop);
			if (World->LineTraceSingleByChannel(Floor, Spot + FVector(0.0f, 0.0f, 50.0f), Spot - FVector(0.0f, 0.0f, 300.0f), ECC_Visibility, Q))
			{
				const FBox B = C->Bounds.GetBox();
				Prop->SetActorLocation(FVector(Spot.X, Spot.Y, Floor.ImpactPoint.Z + (Prop->GetActorLocation().Z - B.Min.Z) + 0.5f));
			}
		}
	}
	ShowCallout(Me, FString::Printf(TEXT("Dropped %s"), *WeaponCatalog::DisplayName(Name)), 2.5f, false);
	return Prop;
}

bool ABasePlayerController::DropInventory(int32 InventoryIndex)
{
	if (!Inventory.IsValidIndex(InventoryIndex) || Inventory[InventoryIndex].IsEmpty()) { return false; }
	if (!DropToWorld(Inventory[InventoryIndex])) { SetDiagNoteTimed(FString::Printf(TEXT("%s has no mesh to drop"), *Inventory[InventoryIndex]), 3.0f); return false; }
	Inventory[InventoryIndex].Empty();
	return true;
}

bool ABasePlayerController::DeleteInventory(int32 InventoryIndex)
{
	if (!Inventory.IsValidIndex(InventoryIndex) || Inventory[InventoryIndex].IsEmpty()) { return false; }
	SetDiagNoteTimed(FString::Printf(TEXT("Discarded %s"), *Inventory[InventoryIndex]), 2.5f);
	Inventory[InventoryIndex].Empty();
	return true;
}

bool ABasePlayerController::DeleteGear(int32 Slot)
{
	if (!Equipped.IsValidIndex(Slot) || Equipped[Slot].IsEmpty()) { return false; }
	SetDiagNoteTimed(FString::Printf(TEXT("Discarded %s"), *Equipped[Slot]), 2.5f);
	Equipped[Slot].Empty();
	RefreshHeldWeapon();   // it may have been what was in the hands
	return true;
}

bool ABasePlayerController::DropGear(int32 Slot)
{
	if (!Equipped.IsValidIndex(Slot) || Equipped[Slot].IsEmpty()) { return false; }
	if (!DropToWorld(Equipped[Slot])) { SetDiagNoteTimed(FString::Printf(TEXT("%s has no mesh to drop"), *Equipped[Slot]), 3.0f); return false; }
	Equipped[Slot].Empty();
	RefreshHeldWeapon();
	return true;
}

bool ABasePlayerController::AddToInventory(const FString& Name, bool bPickup)
{
	for (FString& It : Inventory) { if (It.IsEmpty()) { It = Name; return true; } }
	if (Inventory.Num() >= InventoryCapacity) { return false; }
	// A weapon picked up becomes a particular one, with somewhere to keep what is done to it.
	Inventory.Add(WantsInstance(Name) ? NewItemInstance(Name) : Name);
	if (bPickup) { AutoEquipPickup(Name); }
	return true;
}

bool ABasePlayerController::KindFitsSlot(const FString& Item, int32 Slot) const
{
	const FSheetSpec& Spec = FSheetSpec::Get();
	if (Item.IsEmpty() || !Spec.Slots.IsValidIndex(Slot) || !Spec.Slots[Slot].bEnabled) { return false; }
	const FString Kind = ItemCatalog::Kind(Item);
	for (const FString& K : Spec.Slots[Slot].Kinds) { if (K.Equals(Kind, ESearchCase::IgnoreCase)) { return true; } }
	return false;
}

bool ABasePlayerController::MoveInventory(int32 From, int32 To)
{
	if (!Inventory.IsValidIndex(From) || Inventory[From].IsEmpty() || To < 0 || To >= InventoryCapacity || From == To) { return false; }
	if (Inventory.Num() <= To) { Inventory.SetNum(To + 1); }
	Swap(Inventory[From], Inventory[To]);
	return true;
}

bool ABasePlayerController::EquipFromInventoryToSlot(int32 InventoryIndex, int32 Slot)
{
	if (!Inventory.IsValidIndex(InventoryIndex) || Inventory[InventoryIndex].IsEmpty()) { return false; }
	const FSheetSpec& Spec = FSheetSpec::Get();
	if (Equipped.Num() != Spec.Slots.Num()) { Equipped.SetNum(Spec.Slots.Num()); }
	if (!KindFitsSlot(Inventory[InventoryIndex], Slot)) { return false; }
	{ FString Why; if (!ClothingFits(Inventory[InventoryIndex], Why)) { ShowCallout(GetPawn(), Why, 2.5f, false); return false; } }
	const FString Old = Equipped[Slot];
	Equipped[Slot] = Inventory[InventoryIndex];
	Inventory[InventoryIndex] = Old;
	RefreshHeldWeapon();
	return true;
}

bool ABasePlayerController::UnequipToInventory(int32 Slot, int32 InventoryIndex)
{
	if (!Equipped.IsValidIndex(Slot) || Equipped[Slot].IsEmpty() || InventoryIndex < 0 || InventoryIndex >= InventoryCapacity) { return false; }
	if (Inventory.Num() <= InventoryIndex) { Inventory.SetNum(InventoryIndex + 1); }
	const FString There = Inventory[InventoryIndex];
	if (!There.IsEmpty() && !KindFitsSlot(There, Slot)) { return false; }
	Inventory[InventoryIndex] = Equipped[Slot];
	Equipped[Slot] = There;
	RefreshHeldWeapon();
	return true;
}

bool ABasePlayerController::SwapGear(int32 A, int32 B)
{
	if (!Equipped.IsValidIndex(A) || !Equipped.IsValidIndex(B) || A == B || Equipped[A].IsEmpty()) { return false; }
	if (!KindFitsSlot(Equipped[A], B) || (!Equipped[B].IsEmpty() && !KindFitsSlot(Equipped[B], A))) { return false; }
	Swap(Equipped[A], Equipped[B]);
	RefreshHeldWeapon();
	return true;
}

bool ABasePlayerController::UnequipSlot(int32 Slot)
{
	if (!Equipped.IsValidIndex(Slot) || Equipped[Slot].IsEmpty()) { return false; }
	if (!AddToInventory(Equipped[Slot])) { ShowCallout(GetPawn(), TEXT("No room."), 2.0f, false); return false; }
	Equipped[Slot].Reset();
	RefreshHeldWeapon();
	return true;
}

void ABasePlayerController::BeginAppearance()
{
	if (Screens.IsOpen(EScreen::Appearance) || !GetWorld()) { return; }
	// A preview config to spawn the booth character from; the rows then edit it live.
	if (!IFileManager::Get().FileExists(*CharacterConfigFile::GetPath(TEXT("PlayerPreview"))))
	{
		FCharacterConfig Cfg;
		Cfg.Name = TEXT("PlayerPreview"); Cfg.Description = TEXT("Preview unit."); Cfg.Type = CharacterType::Modular; Cfg.Kit = TEXT("SciFi");
		const Appearance::FBody& Body = Appearance::Bodies()[1];
		Cfg.Parts.Add(TEXT("CutTorso"), Body.Torso); Cfg.Parts.Add(TEXT("CutArms"), Body.Arms); Cfg.Parts.Add(TEXT("CutLegs"), Body.Legs);
		Cfg.Parts.Add(TEXT("CutHead"), Appearance::Bodies()[1].Head);
		Cfg.Face.bEnabled = false;
		CharacterConfigFile::Save(Cfg);
	}
	// Start from the saved likeness when there is one.
	FCharacterConfig Saved;
	const bool bHasSaved = IFileManager::Get().FileExists(*CharacterConfigFile::GetPath(TEXT("Player"))) && CharacterConfigFile::Load(TEXT("Player"), Saved);
	Appearance = FAppearanceState();
	if (bHasSaved)
	{
		const FString* Torso = Saved.Parts.Find(TEXT("CutTorso"));
		const FString* Head = Saved.Parts.Find(TEXT("CutHead"));
		// Bodies share torsos across head variants, so the saved head decides between them.
		for (int32 i = 0; i < Appearance::Bodies().Num(); ++i) { if (Torso && *Torso == Appearance::Bodies()[i].Torso && Head && *Head == Appearance::Bodies()[i].Head) { Appearance.Body = i; break; } }
		if (Appearance.Body == 0) { for (int32 i = 0; i < Appearance::Bodies().Num(); ++i) { if (Torso && *Torso == Appearance::Bodies()[i].Torso) { Appearance.Body = i; break; } } }
		const TArray<Appearance::FOption>& HeadList = Appearance::HeadsFor(Appearance::Bodies()[Appearance.Body].Label.StartsWith(TEXT("Female")));
		for (int32 i = 0; i < HeadList.Num(); ++i) { if (Head && *Head == HeadList[i].Path) { Appearance.Head = i; } }
		for (int32 i = 0; i < Appearance::Noses().Num(); ++i) { if (Saved.Face.NoseMesh == Appearance::Noses()[i].Mesh && Saved.Face.NoseScale.Equals(Appearance::Noses()[i].Scale, 0.01f)) { Appearance.Nose = i; } }
		for (int32 i = 0; i < Appearance::HairColors().Num(); ++i) { if (Saved.Face.HairColor.Equals(Appearance::HairColors()[i].Color, 0.01f)) { Appearance.HairColor = i; } }
		{ const bool bF = Appearance::Bodies()[Appearance.Body].Label.StartsWith(TEXT("Female")); const TArray<Appearance::FBrow>& BL = Appearance::BrowsFor(bF); for (int32 i = 0; i < BL.Num(); ++i) { if (Saved.Face.BrowMesh == BL[i].Mesh && FMath::IsNearlyEqual(Saved.Face.BrowTilt, BL[i].Tilt) && Saved.Face.BrowScale.Equals(BL[i].Scale, 0.01f)) { Appearance.Brow = i; } } }
		AppearanceFirst.Reset(); AppearanceLast.Reset();
		if (!Saved.DisplayName.IsEmpty() && !Saved.DisplayName.Split(TEXT(" "), &AppearanceFirst, &AppearanceLast)) { AppearanceFirst = Saved.DisplayName; }
		{ const bool bF = Appearance::Bodies()[Appearance.Body].Label.StartsWith(TEXT("Female")); const TArray<Appearance::FOption>& HL = Appearance::HairFor(bF); const TArray<Appearance::FOption>& BL = Appearance::BeardsFor(bF);
		for (int32 i = 0; i < HL.Num(); ++i) { if (Saved.Face.HairMesh == HL[i].Path) { Appearance.Hair = i; } }
		for (int32 i = 0; i < BL.Num(); ++i) { if (Saved.Face.FacialHairMesh == BL[i].Path) { Appearance.Beard = i; } } }
		for (int32 i = 0; i < Appearance::Skins().Num(); ++i) { if (Saved.SkinTint.Equals(Appearance::Skins()[i].Tint, 0.01f)) { Appearance.Skin = i; } }
		Appearance.Stubble = FMath::RoundToInt(Saved.StubbleFade * 10.0f);
		Appearance.HeadStubble = FMath::RoundToInt(Saved.HeadStubbleFade * 10.0f);
		for (int32 i = 0; i < UE_ARRAY_COUNT(AppearanceHeights); ++i) { if (FMath::IsNearlyEqual(Saved.Scale.Z, AppearanceHeights[i], 0.005f)) { Appearance.Height = i; } }
		for (int32 i = 0; i < UE_ARRAY_COUNT(AppearanceBuilds); ++i) { if (FMath::IsNearlyEqual(Saved.Scale.X, AppearanceBuilds[i], 0.005f)) { Appearance.Build = i; } }
	}
	// A new player starts as "Repli Can" (whatever an older save said); the intro has a word to say if they keep it.
	if (bFreshPlayerName || (AppearanceFirst.TrimStartAndEnd().IsEmpty() && AppearanceLast.TrimStartAndEnd().IsEmpty())) { AppearanceFirst = TEXT("Repli"); AppearanceLast = TEXT("Can"); }
	ABaseCharacter* Preview = SpawnBoothCharacter(TEXT("PlayerPreview"));
	if (!Preview) { UE_LOG(LogTemp, Warning, TEXT("Appearance: could not spawn the preview")); return; }
	Screens.Open(EScreen::Appearance);
	AppearanceOrbitYaw = 0.0f; AppearanceOrbitPitch = 0.0f; AppearanceBaseYaw = Preview->GetActorRotation().Yaw;
	ApplyAppearance();
	RemoteViewResolution = 1024;          // the mirror is large here; a sharp capture
	ShowRemoteView(Preview, false);       // the feed lives inside the page, not the square
	// The same portrait target the character sheet uses: both pages frame the figure 5:8.
	if (!SheetTarget) { SheetTarget = UKismetRenderingLibrary::CreateRenderTarget2D(this, 640, PaneShape::TargetHeight(PaneShape::Mirror, 640), ETextureRenderTargetFormat::RTF_RGBA8_SRGB, FLinearColor::Black, false); }
	if (USceneCaptureComponent2D* Cap = RemoteCapture ? RemoteCapture->GetCaptureComponent2D() : nullptr) { Cap->TextureTarget = SheetTarget; }
	if (!AppearanceWidget)
	{
		AppearanceWidget = CreateWidget<UAppearanceWidget>(this, UAppearanceWidget::StaticClass());
		AppearanceWidget->SetOwnerController(this);
	}
	DressBoothAsMirror();
	AppearanceWidget->SetFeed(SheetTarget);
	AppearanceWidget->Refresh();
	if (!AppearanceWidget->IsInViewport())
	{
			AppearanceWidget->BeginSettle();   // blank until laid out; holds the outgoing page up meanwhile
		PlaceConsolePage(AppearanceWidget, ConsolePageZ);
	}
	SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);
	bAppearanceInputMode = true;
	ApplyInputMode();
}

void ABasePlayerController::ApplyAppearance()
{
	ABaseCharacter* P = AppearancePreview();
	if (!P) { return; }
	const TArray<Appearance::FBody>& Bodies = Appearance::Bodies();
	const TArray<Appearance::FOption>& Heads = Appearance::Heads();
	const bool bFemaleBody = Bodies.IsValidIndex(Appearance.Body) && Bodies[Appearance.Body].Label.StartsWith(TEXT("Female"));
	const TArray<Appearance::FOption>& Hair = Appearance::HairFor(bFemaleBody);
	const TArray<Appearance::FOption>& Beards = Appearance::BeardsFor(bFemaleBody);
	const TArray<Appearance::FSkin>& Skins = Appearance::Skins();
	if (Bodies.IsValidIndex(Appearance.Body))
	{
		P->SetPart(TEXT("CutTorso"), Bodies[Appearance.Body].Torso);
		P->SetPart(TEXT("CutArms"), Bodies[Appearance.Body].Arms);
		P->SetPart(TEXT("CutLegs"), Bodies[Appearance.Body].Legs);
	}
	if (Bodies.IsValidIndex(Appearance.Body)) { P->SetPart(TEXT("CutHead"), Bodies[Appearance.Body].Head); }
	const TArray<Appearance::FNose>& Noses = Appearance::Noses();
	if (Noses.IsValidIndex(Appearance.Nose)) { P->SetNose(Noses[Appearance.Nose].Mesh, Noses[Appearance.Nose].Scale); }
	const TArray<Appearance::FBrow>& Brows = Appearance::BrowsFor(bFemaleBody);
	if (Brows.IsValidIndex(Appearance.Brow))
	{
		const Appearance::FBrow& Bw = Brows[Appearance.Brow];
		const FVector Base = Bodies.IsValidIndex(Appearance.Body) ? Bodies[Appearance.Body].Brow : BrowBase;
		P->SetBrows(Bw.Mesh, Base + FVector(Bw.Raise, 0.0f, Bw.Spread), Bw.Tilt, Bw.Scale);
	}
	if (Appearance::HairColors().IsValidIndex(Appearance.HairColor)) { P->SetHairColor(Appearance::HairColors()[Appearance.HairColor].Color); }
	P->SetHairMeshPath(Hair.IsValidIndex(Appearance.Hair) ? Hair[Appearance.Hair].Path : FString());
	P->SetFacialHairMeshPath(Beards.IsValidIndex(Appearance.Beard) ? Beards[Appearance.Beard].Path : FString());
	if (Skins.IsValidIndex(Appearance.Skin)) { P->SetSkinTint(Skins[Appearance.Skin].Tint, Appearance.Stubble / 10.0f, Appearance.HeadStubble / 10.0f); }
	const float H = AppearanceHeights[FMath::Clamp(Appearance.Height, 0, (int32)UE_ARRAY_COUNT(AppearanceHeights) - 1)];
	const float B = AppearanceBuilds[FMath::Clamp(Appearance.Build, 0, (int32)UE_ARRAY_COUNT(AppearanceBuilds) - 1)];
	P->SetCharacterScale(FVector(B, B, H));
	FRemoteCameraConfig Cam;
	// The sheet's framings, from UI/CharacterSheet.json: both pages show the same 5:8 picture.
	const FSheetSpec& Spec = FSheetSpec::Get();
	if (Appearance.View == 0) { Cam.Offset = Spec.HeadOffset; Cam.LookOffset = Spec.HeadLook; Cam.Fov = Spec.HeadFov; }
	else { Cam.Offset = Spec.BodyOffset; Cam.LookOffset = Spec.BodyLook; Cam.Fov = Spec.BodyFov; }
	// A turntable, not an orbit: the preview turns on the spot and the camera
	// stays where the booth wall is behind them, so it never swings into the set.
	// The pitch tilts the camera; the offset is expressed in the actor frame,
	// so it is counter-rotated to hold still in the world.
	const FVector Arm = FRotator(AppearanceOrbitPitch, 0.0f, 0.0f).RotateVector(Cam.Offset - Cam.LookOffset);
	Cam.Offset = Cam.LookOffset + FRotator(0.0f, -AppearanceOrbitYaw, 0.0f).RotateVector(Arm);
	P->SetRemoteCamera(Cam);
	P->SetActorRotation(FRotator(0.0f, AppearanceBaseYaw + AppearanceOrbitYaw, 0.0f));
}

void ABasePlayerController::OrbitAppearanceCamera(float DeltaYaw, float DeltaPitch)
{
	if (!Screens.IsOpen(EScreen::Appearance)) { return; }
	AppearanceOrbitYaw = FMath::Fmod(AppearanceOrbitYaw + DeltaYaw, 360.0f);
	AppearanceOrbitPitch = FMath::Clamp(AppearanceOrbitPitch + DeltaPitch, -30.0f, 40.0f);
	ApplyAppearance();
}

void ABasePlayerController::SetAppearanceHeadView(bool bHead)
{
	const int32 Want = bHead ? 0 : 1;
	if (!Screens.IsOpen(EScreen::Appearance) || Appearance.View == Want) { return; }
	Appearance.View = Want;
	ApplyAppearance();
}

void ABasePlayerController::AppearanceStep(const FString& Row, int32 Delta)
{
	auto Cycle = [](int32& V, int32 Count, int32 D, bool bAllowNone) { const int32 Lo = bAllowNone ? -1 : 0; const int32 N = Count + (bAllowNone ? 1 : 0); if (N <= 0) { return; } V = Lo + ((V - Lo + D) % N + N) % N; };
	const bool bFemale = Appearance::Bodies().IsValidIndex(Appearance.Body) && Appearance::Bodies()[Appearance.Body].Label.StartsWith(TEXT("Female"));
	if (Row == TEXT("Body")) { Cycle(Appearance.Body, Appearance::Bodies().Num(), Delta, false); Appearance.Head = 0; Appearance.Hair = -1; Appearance.Beard = -1; Appearance.Brow = 0; }
	else if (!IsAppearanceRowEnabled(Row)) { return; }
	else if (Row == TEXT("Head")) { Cycle(Appearance.Head, Appearance::HeadsFor(bFemale).Num(), Delta, false); }
	else if (Row == TEXT("Nose")) { Cycle(Appearance.Nose, Appearance::Noses().Num(), Delta, false); }
	else if (Row == TEXT("Brows")) { Cycle(Appearance.Brow, Appearance::BrowsFor(bFemale).Num(), Delta, false); }
	else if (Row == TEXT("HairColor")) { Cycle(Appearance.HairColor, Appearance::HairColors().Num(), Delta, false); }
	else if (Row == TEXT("Default")) { DefaultAppearance(); return; }
	else if (Row == TEXT("Skin")) { Cycle(Appearance.Skin, Appearance::Skins().Num(), Delta, false); }
	else if (Row == TEXT("Hair")) { Cycle(Appearance.Hair, Appearance::HairFor(bFemale).Num(), Delta, true); }
	else if (Row == TEXT("Beard")) { Cycle(Appearance.Beard, Appearance::BeardsFor(bFemale).Num(), Delta, true); }
	else if (Row == TEXT("Stubble")) { Appearance.Stubble = FMath::Clamp(Appearance.Stubble + Delta, 0, 10); }
	else if (Row == TEXT("HeadStubble")) { Appearance.HeadStubble = FMath::Clamp(Appearance.HeadStubble + Delta, 0, 10); }
	else if (Row == TEXT("Height")) { Appearance.Height = FMath::Clamp(Appearance.Height + Delta, 0, (int32)UE_ARRAY_COUNT(AppearanceHeights) - 1); }
	else if (Row == TEXT("Build")) { Appearance.Build = FMath::Clamp(Appearance.Build + Delta, 0, (int32)UE_ARRAY_COUNT(AppearanceBuilds) - 1); }
	else if (Row == TEXT("View")) { Appearance.View = 1 - Appearance.View; }
	ApplyAppearance();
}

void ABasePlayerController::RandomiseAppearance()
{
	Appearance.Body = FMath::RandRange(0, Appearance::Bodies().Num() - 1);
	Appearance.Head = FMath::RandRange(0, Appearance::HeadsFor(Appearance::Bodies()[Appearance.Body].Label.StartsWith(TEXT("Female"))).Num() - 1);
	Appearance.Nose = FMath::RandRange(0, Appearance::Noses().Num() - 2);   // never "none" at random
	{ const bool bF = Appearance::Bodies()[Appearance.Body].Label.StartsWith(TEXT("Female")); Appearance.Brow = FMath::RandRange(0, FMath::Max(0, Appearance::BrowsFor(bF).Num() - 2)); }
	Appearance.HairColor = FMath::RandRange(0, Appearance::HairColors().Num() - 1);
	Appearance.Skin = FMath::RandRange(0, Appearance::Skins().Num() - 1);
	{ const bool bF = Appearance::Bodies()[Appearance.Body].Label.StartsWith(TEXT("Female"));
	Appearance.Hair = FMath::RandRange(-1, Appearance::HairFor(bF).Num() - 1);
	Appearance.Beard = (bF || FMath::FRand() < 0.6f) ? -1 : FMath::RandRange(0, Appearance::BeardsFor(bF).Num() - 1); }
	Appearance.Stubble = FMath::RandRange(0, 10);
	Appearance.HeadStubble = FMath::RandRange(0, 10);
	Appearance.Height = FMath::RandRange(1, (int32)UE_ARRAY_COUNT(AppearanceHeights) - 2);
	Appearance.Build = FMath::RandRange(0, (int32)UE_ARRAY_COUNT(AppearanceBuilds) - 1);
	ApplyAppearance();
}

void ABasePlayerController::DefaultAppearance()
{
	const bool bFemale = Appearance::Bodies().IsValidIndex(Appearance.Body) && Appearance::Bodies()[Appearance.Body].Label.StartsWith(TEXT("Female"));
	const TArray<Appearance::FOption>& HeadList = Appearance::HeadsFor(bFemale);
	Appearance.Head = 0;
	for (int32 i = 0; i < HeadList.Num(); ++i) { if (HeadList[i].Path.Contains(TEXT("Cryo_"))) { Appearance.Head = i; break; } }
	Appearance.Skin = 0;
	for (int32 i = 0; i < Appearance::Skins().Num(); ++i) { if (Appearance::Skins()[i].Label == TEXT("Fair")) { Appearance.Skin = i; break; } }
	Appearance.Hair = -1; Appearance.Beard = -1; Appearance.Stubble = 0; Appearance.HeadStubble = 0; Appearance.Nose = 0; Appearance.Brow = 0; Appearance.HairColor = 0;
	Appearance.Brow = 0;   // the first brow of the body's own list
	for (int32 i = 0; i < UE_ARRAY_COUNT(AppearanceHeights); ++i) { if (FMath::IsNearlyEqual(AppearanceHeights[i], 1.0f)) { Appearance.Height = i; } }
	for (int32 i = 0; i < UE_ARRAY_COUNT(AppearanceBuilds); ++i) { if (FMath::IsNearlyEqual(AppearanceBuilds[i], 1.0f)) { Appearance.Build = i; } }
	ApplyAppearance();
}

void ABasePlayerController::SetAppearanceName(const FString& First, const FString& Last)
{
	AppearanceFirst = First.TrimStartAndEnd();
	AppearanceLast = Last.TrimStartAndEnd();
}

void ABasePlayerController::ToggleMetrics()
{
	if (MetricsWidget && MetricsWidget->IsInViewport()) { MetricsWidget->RemoveFromParent(); return; }
	if (!MetricsWidget)
	{
		MetricsWidget = CreateWidget<UMetricsWidget>(this, UMetricsWidget::StaticClass());
		MetricsWidget->SetOwnerController(this);
	}
	if (UGameViewportSubsystem* Viewport = UGameViewportSubsystem::Get())
	{
		FGameViewportWidgetSlot MetricsSlot;
		MetricsSlot.ZOrder = 95;
		MetricsSlot.Anchors = FAnchors(0.0f, 0.0f, 0.0f, 0.0f);
		MetricsSlot.Offsets = FMargin(12.0f, 12.0f, 330.0f, 10.0f);
		MetricsSlot.Alignment = FVector2D::ZeroVector;
		Viewport->AddWidget(MetricsWidget, MetricsSlot);
	}
}

void ABasePlayerController::ClickChoice(int32 Index)
{
	if (ConversationWidget) { ConversationWidget->ClickChoice(Index); }
}

FString ABasePlayerController::DescribeChoices() const
{
	return ConversationWidget ? ConversationWidget->DescribeChoices() : TEXT("no widget");
}

int32 ABasePlayerController::CountChoiceButtons() const
{
	return ConversationWidget ? ConversationWidget->CountChoiceButtons() : -1;
}

void ABasePlayerController::EnsureSoftwareCursor()
{
	UGameViewportClient* VC = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	if (!VC || !IsLocalController()) { return; }
	for (EMouseCursor::Type T : { EMouseCursor::Default, EMouseCursor::Crosshairs, EMouseCursor::Hand, EMouseCursor::EyeDropper, EMouseCursor::TextEditBeam, EMouseCursor::GrabHand, EMouseCursor::GrabHandClosed })
	{
		if (VC->GetSoftwareCursorWidget(T).IsValid()) { continue; }
		if (UCrtCursorWidget* W = CreateWidget<UCrtCursorWidget>(GetGameInstance(), UCrtCursorWidget::StaticClass())) { VC->SetSoftwareCursorWidget(T, W); }
	}
}

void ABasePlayerController::CursorProbe()
{
	if (!FSlateApplication::IsInitialized() || !GetWorld() || !GetWorld()->GetGameViewport()) { return; }
	FVector2D Size; GetWorld()->GetGameViewport()->GetViewportSize(Size);
	const FVector2D Origin = GetWorld()->GetGameViewport()->GetWindow().IsValid() ? GetWorld()->GetGameViewport()->GetGameViewportWidget()->GetCachedGeometry().GetAbsolutePosition() : FVector2D::ZeroVector;
	FSlateApplication::Get().SetCursorPos(Origin + Size * 0.5);
}

FString ABasePlayerController::CursorState() const
{
	if (!FSlateApplication::IsInitialized()) { return TEXT("no slate"); }
	const TSharedPtr<ICursor> Cursor = FSlateApplication::Get().GetPlatformCursor();
	return FString::Printf(TEXT("platform cursor type %d (Custom=%d None=%d Default=%d) show=%d software=%d pos=%s"), Cursor ? (int32)Cursor->GetType() : -1, (int32)EMouseCursor::Custom, (int32)EMouseCursor::None, (int32)EMouseCursor::Default, bShowMouseCursor ? 1 : 0, HasSoftwareCursor() ? 1 : 0, *FSlateApplication::Get().GetCursorPos().ToString());
}

bool ABasePlayerController::HasSoftwareCursor() const
{
	UGameViewportClient* VC = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	return VC && VC->GetSoftwareCursorWidget(EMouseCursor::Default).IsValid();
}

void ABasePlayerController::AppearanceBackdrop(int32 Mode)
{
	SetBoothSetVisible(Mode == 0);
	if (USceneCaptureComponent2D* Cap = RemoteCapture ? RemoteCapture->GetCaptureComponent2D() : nullptr)
	{
		const bool bFlags = Mode < 2;
		Cap->ShowFlags.SetAtmosphere(bFlags); Cap->ShowFlags.SetFog(bFlags); Cap->ShowFlags.SetVolumetricFog(bFlags);
	}
}

bool ABasePlayerController::IsAppearanceRowEnabled(const FString& Row) const
{
	const bool bFemale = Appearance::Bodies().IsValidIndex(Appearance.Body) && Appearance::Bodies()[Appearance.Body].Label.StartsWith(TEXT("Female"));
	if (Row == TEXT("Beard") || Row == TEXT("Stubble") || Row == TEXT("HeadStubble")) { return !bFemale; }
	return true;
}

FString ABasePlayerController::GetAppearanceValue(const FString& Row) const
{
	const bool bFemale = Appearance::Bodies().IsValidIndex(Appearance.Body) && Appearance::Bodies()[Appearance.Body].Label.StartsWith(TEXT("Female"));
	const TArray<Appearance::FOption>& HeadList = Appearance::HeadsFor(bFemale);
	if (Row == TEXT("Nose")) { return Appearance::Noses().IsValidIndex(Appearance.Nose) ? Appearance::Noses()[Appearance.Nose].Label : TEXT("-"); }
	if (Row == TEXT("Brows")) { const TArray<Appearance::FBrow>& BL = Appearance::BrowsFor(bFemale); return BL.IsValidIndex(Appearance.Brow) ? BL[Appearance.Brow].Label : TEXT("-"); }
	if (Row == TEXT("HairColor")) { return Appearance::HairColors().IsValidIndex(Appearance.HairColor) ? Appearance::HairColors()[Appearance.HairColor].Label : TEXT("-"); }
	if (Row == TEXT("First")) { return AppearanceFirst; }
	if (Row == TEXT("Last")) { return AppearanceLast; }
	if (Row == TEXT("Body")) { return Appearance::Bodies().IsValidIndex(Appearance.Body) ? Appearance::Bodies()[Appearance.Body].Label : TEXT("-"); }
	if (Row == TEXT("Head")) { return HeadList.IsValidIndex(Appearance.Head) ? FString::Printf(TEXT("%s  (%d/%d)"), *HeadList[Appearance.Head].Label, Appearance.Head + 1, HeadList.Num()) : TEXT("-"); }
	if (Row == TEXT("Skin")) { return Appearance::Skins().IsValidIndex(Appearance.Skin) ? Appearance::Skins()[Appearance.Skin].Label : TEXT("-"); }
	if (Row == TEXT("Hair")) { const TArray<Appearance::FOption>& HL = Appearance::HairFor(bFemale); return HL.IsValidIndex(Appearance.Hair) ? FString::Printf(TEXT("%s  %d / %d"), *HL[Appearance.Hair].Label, Appearance.Hair + 1, HL.Num()) : TEXT("none"); }
	if (Row == TEXT("Beard")) { if (bFemale) { return TEXT("-"); } const TArray<Appearance::FOption>& BL = Appearance::BeardsFor(bFemale); return BL.IsValidIndex(Appearance.Beard) ? FString::Printf(TEXT("%d / %d"), Appearance.Beard + 1, BL.Num()) : TEXT("none"); }
	if (Row == TEXT("Stubble")) { return Appearance.Stubble == 0 ? FString(TEXT("none")) : FString::Printf(TEXT("%d%%"), Appearance.Stubble * 10); }
	if (Row == TEXT("HeadStubble")) { return Appearance.HeadStubble == 0 ? FString(TEXT("none")) : FString::Printf(TEXT("%d%%"), Appearance.HeadStubble * 10); }
	if (Row == TEXT("Height")) { return FString::Printf(TEXT("%d%%"), FMath::RoundToInt(AppearanceHeights[FMath::Clamp(Appearance.Height, 0, (int32)UE_ARRAY_COUNT(AppearanceHeights) - 1)] * 100.0f)); }
	if (Row == TEXT("Build")) { return FString::Printf(TEXT("%d%%"), FMath::RoundToInt(AppearanceBuilds[FMath::Clamp(Appearance.Build, 0, (int32)UE_ARRAY_COUNT(AppearanceBuilds) - 1)] * 100.0f)); }
	if (Row == TEXT("View")) { return Appearance.View == 0 ? TEXT("head") : TEXT("full"); }
	return TEXT("");
}

FString ABasePlayerController::GetPlayerDisplayName() const
{
	if (const ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn()))
	{
		if (!Me->GetCharacterConfig().DisplayName.IsEmpty()) { return Me->GetCharacterConfig().DisplayName; }
	}
	FCharacterConfig Saved;
	if (IFileManager::Get().FileExists(*CharacterConfigFile::GetPath(TEXT("Player"))) && CharacterConfigFile::Load(TEXT("Player"), Saved)) { return Saved.DisplayName; }
	return FString();
}

void ABasePlayerController::FinishAppearance()
{
	if (!Screens.IsOpen(EScreen::Appearance)) { return; }
	if (ABaseCharacter* P = AppearancePreview())
	{
		FCharacterConfig Cfg = P->GetCharacterConfig();
		const FString Full = (AppearanceFirst + TEXT(" ") + AppearanceLast).TrimStartAndEnd();
		Cfg.Name = TEXT("Player");   // the file; the chosen name is the display name
		Cfg.DisplayName = Full;
		Cfg.Description = TEXT("You. Approximately.");
		Cfg.Comment.Reset();
		Cfg.RemoteCamera = FRemoteCameraConfig();
		Cfg.Tags = { TEXT("player") };
		CharacterConfigFile::Save(Cfg);
		if (ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn())) { Me->ApplyCharacterConfig(Cfg); bPlayerConfigApplied = true; }
	}
	Screens.Close(EScreen::Appearance);
	bFreshPlayerName = false;   // from here on the chooser reopens with the chosen name
	if (AppearanceWidget && AppearanceWidget->IsInViewport() && !bRetainPageWidget) { AppearanceWidget->RemoveFromParent(); }
	HideRemoteView();   // tears the booth and the preview down
	RemoteViewResolution = 320;
	bAppearanceInputMode = false;
	SetIgnoreMoveInput(false);
	SetIgnoreLookInput(false);
	ApplyInputMode();
}

void ABasePlayerController::HandRot(float Pitch, float Yaw, float Roll)
{
	ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn());
	if (!Me) { return; }
	if (Pitch < 999.0f) { Me->SetTriggerHandRotation(FRotator(Pitch, Yaw, Roll)); }
	const FRotator R = Me->GetTriggerHandRotation();
	SetDiagNoteTimed(FString::Printf(TEXT("HandRot pitch %.1f yaw %.1f roll %.1f (weapon space)"), R.Pitch, R.Yaw, R.Roll), 8.0f);
}

bool ABasePlayerController::SaveHeldWeaponRotField(const TCHAR* Field, const FRotator& R)
{
	const WeaponCatalog::FWeapon* W = Equipped.IsValidIndex(HeldSlot) ? WeaponCatalog::Find(Equipped[HeldSlot]) : nullptr;
	return W && SaveWeaponField(W->Key, Field, { R.Pitch, R.Yaw, R.Roll });
}

bool ABasePlayerController::SaveWeaponField(const FString& Key, const TCHAR* Field, const TArray<double>& Values, bool bScalar)
{
	// Saved on the entry, so the tune outlives the session and every hold of this weapon gets it.
	FString Json;
	const FString File = FPaths::Combine(JsonData::DataDir(), TEXT("UI"), TEXT("Weapons.json"));
	TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Json, *File)) { return false; }
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	const TSharedPtr<FJsonObject>* Weapons = nullptr; const TSharedPtr<FJsonObject>* Entry = nullptr;
	if (!(FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid() && Root->TryGetObjectField(TEXT("weapons"), Weapons) && Weapons && (*Weapons)->TryGetObjectField(Key, Entry) && Entry)) { return false; }
	if (bScalar && Values.Num() == 1) { (*Entry)->SetNumberField(Field, FMath::RoundToDouble(Values[0] * 100.0) / 100.0); }
	else
	{
		TArray<TSharedPtr<FJsonValue>> Arr;
		for (double V : Values) { Arr.Add(MakeShared<FJsonValueNumber>(FMath::RoundToDouble(V * 100.0) / 100.0)); }
		(*Entry)->SetArrayField(Field, Arr);
	}
	FString Out;
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Out);
	if (!(FJsonSerializer::Serialize(Root.ToSharedRef(), Writer) && FFileHelper::SaveStringToFile(Out, *File, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))) { return false; }
	WeaponCatalog::Reload();
	return true;
}

// THE WEAPON'S PLACE: three triples, low ready / shouldered / sights, in the same order as the old
// pull and lateral so nobody has to learn a second convention. SaveWeaponField writes flat arrays
// and this one is nested, so it has its own writer -- same read-modify-write from disk, so a tool
// editing the file between two saves is not clobbered by a stale in-memory copy.
bool ABasePlayerController::SaveWeaponTriples(const FString& Key, const TCHAR* Field, const FVector* Pos)
{
	FString Json;
	const FString File = FPaths::Combine(JsonData::DataDir(), TEXT("UI"), TEXT("Weapons.json"));
	TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Json, *File)) { return false; }
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	const TSharedPtr<FJsonObject>* Weapons = nullptr; const TSharedPtr<FJsonObject>* Entry = nullptr;
	if (!(FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid() && Root->TryGetObjectField(TEXT("weapons"), Weapons) && Weapons && (*Weapons)->TryGetObjectField(Key, Entry) && Entry)) { return false; }
	auto Round = [](double V) { return FMath::RoundToDouble(V * 100.0) / 100.0; };
	TArray<TSharedPtr<FJsonValue>> Rows;
	for (int32 i = 0; i < 3; ++i)
	{
		TArray<TSharedPtr<FJsonValue>> Axis;
		Axis.Add(MakeShared<FJsonValueNumber>(Round(Pos[i].X)));
		Axis.Add(MakeShared<FJsonValueNumber>(Round(Pos[i].Y)));
		Axis.Add(MakeShared<FJsonValueNumber>(Round(Pos[i].Z)));
		Rows.Add(MakeShared<FJsonValueArray>(Axis));
	}
	(*Entry)->SetArrayField(Field, Rows);
	FString Out;
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Out);
	if (!(FJsonSerializer::Serialize(Root.ToSharedRef(), Writer) && FFileHelper::SaveStringToFile(Out, *File, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))) { return false; }
	WeaponCatalog::Reload();
	return true;
}

void ABasePlayerController::HandRotWeapon(float Pitch, float Yaw, float Roll)
{
	ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn());
	if (!Me) { return; }
	const WeaponCatalog::FWeapon* W = Equipped.IsValidIndex(HeldSlot) ? WeaponCatalog::Find(Equipped[HeldSlot]) : nullptr;
	if (!W) { SetDiagNoteTimed(TEXT("HandRotWeapon: nothing in hand"), 4.0f); return; }
	if (Pitch < 999.0f) { Me->SetWeaponHandRotation(FRotator(Pitch, Yaw, Roll)); SaveHeldWeaponRotField(TEXT("hand_rot"), FRotator(Pitch, Yaw, Roll)); }
	const FRotator R = Me->GetWeaponHandRotation();
	SetDiagNoteTimed(FString::Printf(TEXT("%s hand_rot pitch %.1f yaw %.1f roll %.1f (on top of HandRot)"), *W->Name, R.Pitch, R.Yaw, R.Roll), 8.0f);
}

void ABasePlayerController::HandRotL(float Pitch, float Yaw, float Roll)
{
	ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn());
	if (!Me) { return; }
	if (Pitch < 999.0f) { Me->SetSupportHandRotation(FRotator(Pitch, Yaw, Roll)); }
	const FRotator R = Me->GetSupportHandRotation();
	SetDiagNoteTimed(FString::Printf(TEXT("HandRotL pitch %.1f yaw %.1f roll %.1f (the support hand's wrap, weapon space)"), R.Pitch, R.Yaw, R.Roll), 8.0f);
}

void ABasePlayerController::HandRotLWeapon(float Pitch, float Yaw, float Roll)
{
	ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn());
	if (!Me) { return; }
	const WeaponCatalog::FWeapon* W = Equipped.IsValidIndex(HeldSlot) ? WeaponCatalog::Find(Equipped[HeldSlot]) : nullptr;
	if (!W) { SetDiagNoteTimed(TEXT("HandRotLWeapon: nothing in hand"), 4.0f); return; }
	if (Pitch < 999.0f) { Me->SetWeaponForeHandRotation(FRotator(Pitch, Yaw, Roll)); SaveHeldWeaponRotField(TEXT("fore_hand_rot"), FRotator(Pitch, Yaw, Roll)); }
	const FRotator R = Me->GetWeaponForeHandRotation();
	SetDiagNoteTimed(FString::Printf(TEXT("%s fore_hand_rot pitch %.1f yaw %.1f roll %.1f (on top of HandRotL)"), *W->Name, R.Pitch, R.Yaw, R.Roll), 8.0f);
}

// The one table both reading and writing go through, so a name can never mean two different things
// and a field cannot be listed but not settable.
static bool ForEachViewField(ABaseCharacter* C, const FString& Want, float* Set, FString* Report)
{
	if (!C) { return false; }
	bool bHit = false;
	auto Field = [&](const TCHAR* Name, float& Ref, const TCHAR* What)
	{
		if (Report) { Report->Append(FString::Printf(TEXT("  %-10s %8.3f   %s\n"), Name, Ref, What)); }
		if (Set && Want.Equals(Name, ESearchCase::IgnoreCase)) { Ref = *Set; bHit = true; }
	};
	float Lead = C->bViewLeadFromSolve ? 1.0f : 0.0f;
	Field(TEXT("lead"), Lead, TEXT("1 = the weapon follows the SOLVE (smooth), 0 = it rides the animated hand"));
	C->bViewLeadFromSolve = Lead > 0.5f;
	Field(TEXT("spring"),    C->ViewSpringRate,    TEXT("rad/s, how hard position follows"));
	Field(TEXT("springrot"), C->ViewSpringRateRot, TEXT("rad/s, how hard rotation follows"));
	Field(TEXT("maxcm"),     C->ViewMaxOffsetCm,   TEXT("cm, the net: furthest the weapon may lag"));
	Field(TEXT("maxdeg"),    C->ViewMaxOffsetDeg,  TEXT("deg, the same for rotation"));
	Field(TEXT("sway"),      C->ViewSwayScale,     TEXT("deg of lag per deg/s of turn"));
	Field(TEXT("swaymax"),   C->ViewSwayMaxDeg,    TEXT("deg, the most sway allowed"));
	Field(TEXT("swayshift"), C->ViewSwayShiftCm,   TEXT("cm sideways at full sway"));
	Field(TEXT("bob"),       C->ViewBobCm,         TEXT("cm, stride bob at full speed"));
	Field(TEXT("bobhz"),     C->ViewBobHz,         TEXT("strides a second"));
	Field(TEXT("bobref"),    C->ViewBobSpeedRef,   TEXT("cm/s that counts as a full stride"));
	return bHit;
}

void ABasePlayerController::ViewTune(const FString& Field, float Value)
{
	ABaseCharacter* C = Cast<ABaseCharacter>(GetPawn());
	if (!C) { return; }
	if (Field.IsEmpty())
	{
		FString Report = TEXT("ViewTune -- first person view:\n");
		ForEachViewField(C, FString(), nullptr, &Report);
		Report.Append(TEXT("  usage: ViewTune <name> <value>"));
		UE_LOG(LogTemp, Log, TEXT("%s"), *Report);
		if (GEngine) { GEngine->AddOnScreenDebugMessage(-1, 14.0f, FColor::Green, Report); }
		return;
	}
	float V = Value;
	if (!ForEachViewField(C, Field, &V, nullptr))
	{
		if (GEngine) { GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, FString::Printf(TEXT("ViewTune: no field '%s' -- run ViewTune with no arguments for the list"), *Field)); }
		return;
	}
	if (GEngine) { GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Green, FString::Printf(TEXT("ViewTune %s = %.3f"), *Field, V)); }
}

void ABasePlayerController::ForceCarry(int32 Carry)
{
	if (ABaseCharacter* C = Cast<ABaseCharacter>(GetPawn()))
	{
		C->SetCarryOverride(Carry);
		UE_LOG(LogTemp, Log, TEXT("ForceCarry: %d"), Carry);
	}
}

void ABasePlayerController::HandDump()
{
	ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn());
	if (!Me) { SetDiagNoteTimed(TEXT("HandDump: no pawn"), 3.0f); return; }
	const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ClaudeAssist"), TEXT("hand_dump.json"));
	if (FFileHelper::SaveStringToFile(Me->DumpHold(), *Path))
	{
		SetDiagNoteTimed(TEXT("HandDump written to Saved/ClaudeAssist/hand_dump.json"), 4.0f);
	}
	else
	{
		SetDiagNoteTimed(TEXT("HandDump: could not write the file"), 4.0f);
	}
}

void ABasePlayerController::HandDiag()
{
	ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn());
	if (!Me) { return; }
	Me->bHandDiag = !Me->bHandDiag;
	SetDiagNoteTimed(Me->bHandDiag ? TEXT("HandDiag on: reach, carry pull-in, hand gap, sight off the eye line") : TEXT("HandDiag off"), 4.0f);
}

void ABasePlayerController::Unstuck()
{
	ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn());
	if (!Me) { return; }
	const bool bOk = Me->TryUnstuck();
	SetDiagNoteTimed(bOk ? TEXT("Unstuck: moved to the last solid ground") : TEXT("Unstuck: nowhere to go"), 5.0f);
}

FString ABasePlayerController::UIAudit()
{
	UUserWidget* Page = Screens.IsOpen(EScreen::CharacterSheet) ? Cast<UUserWidget>(CharacterSheetWidget)
		: Screens.IsOpen(EScreen::Reference) ? Cast<UUserWidget>(ReferenceWidget)
		: Screens.IsOpen(EScreen::Appearance) ? Cast<UUserWidget>(AppearanceWidget) : nullptr;
	if (!Page || !Page->WidgetTree) { return TEXT("UIAudit: no console page is open"); }
	const FGeometry& PG = Page->GetCachedGeometry();
	const FVector2D PageBR = PG.LocalToAbsolute(FVector2D(PG.GetLocalSize()));
	FString Out = FString::Printf(TEXT("UIAudit: %s laid out at %.0f x %.0f (viewport %s px, scale %.3f)\n"),
		*Page->GetClass()->GetName(), PG.GetLocalSize().X, PG.GetLocalSize().Y, *UWidgetLayoutLibrary::GetViewportSize(this).ToString(), UWidgetLayoutLibrary::GetViewportScale(this));
	int32 Bad = 0, Seen = 0;
	// A widget is OVER when its own rectangle reaches past its parent's (a box that had to give
	// a child more than it had) or past the page. Inside a scroll box the bottom is the point.
	// A text given less width than its glyphs need draws past its box without moving any
	// geometry, so leaf text is checked against its desired width as well; a wrapping text and
	// an image (whose native size is not a demand) are not.
	TFunction<void(UWidget*)> Visit = [&](UWidget* W)
	{
		if (!W || !W->IsVisible()) { return; }
		++Seen;
		const FGeometry& G = W->GetCachedGeometry();
		const FVector2D Size(G.GetLocalSize());
		if (Size.X >= 1.0 || Size.Y >= 1.0)
		{
			const float Sc = FMath::Max(0.01f, G.Scale);
			const FVector2D BR = G.LocalToAbsolute(FVector2D(G.GetLocalSize()));
			bool bScrolls = false;
			for (UPanelWidget* P = W->GetParent(); P; P = P->GetParent()) { if (P->IsA<UScrollBox>()) { bScrolls = true; break; } }
			FVector2D ParentBR = PageBR;
			if (UPanelWidget* Parent = W->GetParent())
			{
				const FGeometry& PGm = Parent->GetCachedGeometry();
				if (PGm.GetLocalSize().X > 1.0f) { ParentBR = PGm.LocalToAbsolute(FVector2D(PGm.GetLocalSize())); }
			}
			const double PastParentX = (BR.X - ParentBR.X) / Sc;
			const double PastParentY = bScrolls ? 0.0 : (BR.Y - ParentBR.Y) / Sc;
			const double PastPageX = (BR.X - PageBR.X) / Sc;
			const double PastPageY = bScrolls ? 0.0 : (BR.Y - PageBR.Y) / Sc;
			double TextOver = 0.0;
			FString Text;
			if (const UTextBlock* T = Cast<UTextBlock>(W))
			{
				if (!T->GetAutoWrapText()) { TextOver = T->GetDesiredSize().X - Size.X; }
				Text = T->GetText().ToString().Left(30);
			}
			if (PastParentX > 1.0 || PastParentY > 1.0 || PastPageX > 1.0 || PastPageY > 1.0 || TextOver > 1.0)
			{
				++Bad;
				Out += FString::Printf(TEXT("  %-26s %-20s in %-20s %5.0f x %4.0f  past parent %+.0f,%+.0f  past page %+.0f,%+.0f  text over %+.0f  %s\n"),
					*W->GetName(), *W->GetClass()->GetName(), W->GetParent() ? *W->GetParent()->GetName() : TEXT("-"),
					Size.X, Size.Y, PastParentX, PastParentY, PastPageX, PastPageY, TextOver, *Text);
			}
		}
		// Into a nested page (the grids, the tab strip, the rules): its own tree is its own.
		if (UUserWidget* Sub = Cast<UUserWidget>(W)) { if (Sub->WidgetTree) { Sub->WidgetTree->ForEachWidget([&](UWidget* C) { Visit(C); }); } }
	};
	Page->WidgetTree->ForEachWidget([&](UWidget* W) { Visit(W); });
	Out += FString::Printf(TEXT("%d of %d widgets over their room or past the page\n"), Bad, Seen);
	UE_LOG(LogTemp, Log, TEXT("%s"), *Out);
	return Out;
}

void ABasePlayerController::WeaponLagTest(float Seconds)
{
	if (ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn())) { Me->BeginWeaponLagTest(Seconds); SetDiagNoteTimed(TEXT("Weapon lag test running -- hands off"), Seconds); }
}

FString ABasePlayerController::WeaponLagReport() const
{
	const ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn());
	return Me ? Me->LastLagReport : TEXT("no pawn");
}

FMargin ABasePlayerController::ConsolePageRect() const
{
	const FSheetSpec& S = FSheetSpec::Get();
	const float Scale = FMath::Max(0.01f, UWidgetLayoutLibrary::GetViewportScale(this));
	const FVector2D Logical = UWidgetLayoutLibrary::GetViewportSize(this) / Scale;
	// Anchored top-left, so the offsets are a position and a SIZE: the margins, then the screen
	// less the margins but never less than the spec's minimum.
	return FMargin(S.Panel.Left, S.Panel.Top,
		FMath::Max(Logical.X - S.Panel.Left - S.Panel.Right, S.PanelMinWidth),
		FMath::Max(Logical.Y - S.Panel.Top - S.Panel.Bottom, S.PanelMinHeight));
}

void ABasePlayerController::PlaceConsolePage(UUserWidget* Page, int32 ZOrder)
{
	UGameViewportSubsystem* Viewport = UGameViewportSubsystem::Get();
	if (!Page || !Viewport) { return; }
	FGameViewportWidgetSlot Slot;
	Slot.ZOrder = ZOrder;
	Slot.Anchors = FAnchors(0.0f, 0.0f, 0.0f, 0.0f);
	Slot.Alignment = FVector2D::ZeroVector;
	Slot.Offsets = ConsolePageRect();
	LastPageSize = FVector2D(Slot.Offsets.Right, Slot.Offsets.Bottom);
	Viewport->AddWidget(Page, Slot);
	ConsolePages.AddUnique(Page);
}

void ABasePlayerController::KeepConsolePagesFitted()
{
	if (ConsolePages.Num() == 0) { return; }
	const FMargin Rect = ConsolePageRect();
	if (FMath::IsNearlyEqual(Rect.Right, LastPageSize.X, 0.5f) && FMath::IsNearlyEqual(Rect.Bottom, LastPageSize.Y, 0.5f)) { return; }
	LastPageSize = FVector2D(Rect.Right, Rect.Bottom);
	UGameViewportSubsystem* Viewport = UGameViewportSubsystem::Get();
	for (int32 i = ConsolePages.Num() - 1; i >= 0; --i)
	{
		UUserWidget* Page = ConsolePages[i].Get();
		if (!Page) { ConsolePages.RemoveAt(i); continue; }
		if (!Viewport || !Page->IsInViewport()) { continue; }
		FGameViewportWidgetSlot Slot = Viewport->GetWidgetSlot(Page);
		Slot.Offsets = Rect;
		Viewport->SetWidgetSlot(Page, Slot);
	}
}
