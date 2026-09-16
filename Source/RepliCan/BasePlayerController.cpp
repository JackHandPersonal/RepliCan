#include "BasePlayerController.h"
#include "BaseCharacter.h"
#include "CharacterBuilderWidget.h"
#include "EditToolWidget.h"
#include "CharacterSheetWidget.h"
#include "RemoteViewWidget.h"
#include "AppearanceWidget.h"
#include "Engine/StaticMeshActor.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "FlickerLightActor.h"
#include "Engine/PointLight.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/SceneCapture2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "CharacterGhostActor.h"
#include "CharacterConfig.h"
#include "CrtCursorWidget.h"
#include "MetricsWidget.h"
#include "Components/AudioComponent.h"
#include "Engine/GameViewportClient.h"
#include "FaceController.h"
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
#include "FaceManagerWidget.h"
#include "AnimationBrowserWidget.h"
#include "PauseMenuWidget.h"
#include "SettingsWidget.h"
#include "ScenesWidget.h"
#include "ConfirmDialogWidget.h"
#include "InspectMenuWidget.h"
#include "CalloutWidget.h"
#include "ConversationWidget.h"
#include "VoiceLines.h"
#include "SequenceData.h"
#include "SequenceDirector.h"
#include "WeaponCatalog.h"
#include "ElevatorActor.h"
#include "ImpactEffects.h"
#include "InputBindings.h"
#include "AmbientPlayer.h"
#include "BlinkOverlayWidget.h"
#include "GameFramework/HUD.h"
#include "HAL/IConsoleManager.h"
#include "Blueprint/GameViewportSubsystem.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundWave.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "SaveGameSubsystem.h"
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
#include "CrtStyle.h"
#include "ItemCatalog.h"
#include "SheetSpec.h"
#if WITH_EDITOR
#include "Editor.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#endif
#include "Engine/Texture.h"
#include "ReferenceWidget.h"
#include "Components/LightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/PlayerInput.h"
#include "SlidingDoorActor.h"
#include "LootBoxActor.h"
#include "InventoryTransferWidget.h"
#include "InspectSurface.h"

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
			ABasePlayerController* PC = Controller.Get();
			if (!PC || !PC->GetWorld() || !PC->GetWorld()->IsGameWorld()) { return false; }
			if (InputBindings::KeyFor(TEXT("Freelook")) == KeyEvent.GetKey())
			{
				if (ABaseCharacter* Ch = Cast<ABaseCharacter>(PC->GetPawn())) { Ch->SetFreelook(false); }
			}
			return false;
		}

		virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& KeyEvent) override
		{
			ABasePlayerController* PC = Controller.Get();
			if (!PC || KeyEvent.IsRepeat() || !PC->GetWorld() || !PC->GetWorld()->IsGameWorld()) { return false; }
			const FKey Key = KeyEvent.GetKey();
			if (InputBindings::KeyFor(TEXT("Freelook")) == Key && !PC->IsAnyScreenOpen() && !PC->IsEditMode())
			{
				if (ABaseCharacter* Ch = Cast<ABaseCharacter>(PC->GetPawn())) { Ch->SetFreelook(true); }
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
			const bool bPlaying = !PC->IsAnyScreenOpen();

			if (Bound(TEXT("CyclePalette")) && bPlaying) { PC->CycleMaterialUnderReticle(); return true; }
			if (Bound(TEXT("InspectSurface")) && bPlaying) { PC->NoteTextureUnderReticle(); return true; }
			if (Bound(TEXT("NextWeapon")) && bPlaying) { PC->SwapWeaponSlot(); return true; }
			if (Bound(TEXT("Holster")) && bPlaying) { PC->ToggleHolster(); return true; }
			if (Bound(TEXT("Reload")) && bPlaying) { PC->ReloadHeldWeapon(); return true; }
			if (Bound(TEXT("MeleeBash")) && bPlaying) { PC->MeleeHeldWeapon(); return true; }
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
				// Claude Assist, straight in: the cursor becomes the marking reticle and every click goes to Claude. F12 again ends it.
				if (PC->IsInCinematic() || PC->IsPauseMenuOpen()) { return true; }
				if (PC->IsClaudeAssistActive()) { PC->CancelClaudeAssist(); PC->ExitEditMode(); }
				else { PC->HideCharacterSheet(); PC->BeginClaudeAssist(); }
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
			if (PC->IsTransferOpen()) { PC->CloseTransfer(); return true; }
			if (PC->IsCharacterSheetOpen()) { PC->HideCharacterSheet(); return true; }
			if (PC->IsReferenceOpen()) { PC->HideReference(); } else if (PC->IsPauseMenuOpen()) { PC->HidePauseMenu(); } else { PC->ShowPauseMenu(); }
			return true;
		}

		// The wheel changes weapon rather than camera distance. Consuming it here keeps it away
		// from the Zoom input action, so the two do not both fire off one scroll; zoom stays on C.
		virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& WheelEvent, const FPointerEvent* GestureEvent) override
		{
			ABasePlayerController* PC = Controller.Get();
			if (!PC || !PC->GetWorld() || !PC->GetWorld()->IsGameWorld()) { return false; }
			if (PC->IsEditMode() || PC->IsAnyScreenOpen()) { return false; }
			const float Delta = WheelEvent.GetWheelDelta();
			if (FMath::IsNearlyZero(Delta)) { return false; }
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
			const FKey Button = MouseEvent.GetEffectingButton();
			const bool bCanAct = !PC->IsEditMode() && !PC->IsAnyScreenOpen();

			if (bCanAct && InputBindings::KeyFor(TEXT("Aim")) == Button)
			{
				if (ABaseCharacter* Ch = Cast<ABaseCharacter>(PC->GetPawn()))
				{
					if (PC->HeldSlot >= 0) { Ch->SetAiming(true); return true; }
				}
				return false;
			}
			if (InputBindings::KeyFor(TEXT("Fire")) != Button) { return false; }
			// Outside edit mode a click is a trigger pull, provided something is in the hand and
			// no screen is up. Edit mode keeps the click for placing things.
			if (!PC->IsEditMode())
			{
				// A click that lands on a panel is a click on the panel, never a shot.
				if (PC->IsAnyScreenOpen()) { return false; }
				if (PC->HeldSlot < 0) { return false; }
				PC->FireHeldWeapon();
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
	InputComponent->BindAxisKey(EKeys::MouseWheelAxis, this, &ABasePlayerController::OnInspectWheel).bConsumeInput = false;
	InputComponent->BindKey(EKeys::E, IE_Pressed, this, &ABasePlayerController::OnInspectUse).bConsumeInput = false;

	// Conversation replies 1-9 (Tab/Esc are caught in FEditClickProcessor).
	for (const FKey& Digit : { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine })
	{
		InputComponent->BindKey(Digit, IE_Pressed, this, &ABasePlayerController::OnConversationDigit).bConsumeInput = false;
	}

	// Quick save / load.
	InputComponent->BindKey(EKeys::F5, IE_Pressed, this, &ABasePlayerController::QuickSave);
	InputComponent->BindKey(EKeys::F9, IE_Pressed, this, &ABasePlayerController::QuickLoad);
}

void ABasePlayerController::QuickSave()
{
	USaveGameSubsystem* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<USaveGameSubsystem>() : nullptr;
	const bool bOk = Saves && Saves->SaveGame(TEXT("Quick"));
	if (APawn* P = GetPawn()) { ShowCallout(P, bOk ? TEXT("Game saved.") : TEXT("Save failed (see log)."), 2.0f, false); }
}

void ABasePlayerController::QuickLoad()
{
	USaveGameSubsystem* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<USaveGameSubsystem>() : nullptr;
	if (!Saves || !Saves->SlotExists(TEXT("Quick")))
	{
		if (APawn* P = GetPawn()) { ShowCallout(P, TEXT("No quick save yet."), 2.0f, false); }
		return;
	}
	HidePauseMenu();
	const bool bOk = Saves->LoadGame(TEXT("Quick"));
	if (APawn* P = GetPawn()) { ShowCallout(P, bOk ? TEXT("Game loaded.") : TEXT("Load failed (see log)."), 2.0f, false); }
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
	if (IsInspectMenuOpen()) { InspectUseSelected(); }
}

void ABasePlayerController::InspectSelectNext(int32 Delta)
{
	if (InspectActions.Num() == 0) { return; }
	InspectSelection = ((InspectSelection + Delta) % InspectActions.Num() + InspectActions.Num()) % InspectActions.Num();
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
		FString Label = Target->GetActorLabel();
		for (const TCHAR* Prefix : { TEXT("Camp_"), TEXT("SM_Prop_"), TEXT("SM_") }) { Label.RemoveFromStart(Prefix); }
		while (Label.Len() > 3 && FChar::IsDigit(Label[Label.Len() - 1])) { Label.LeftChopInline(1); }
		Label.RemoveFromEnd(TEXT("_"));
		Label.ReplaceInline(TEXT("_"), TEXT(" "));
		OutName = Label;
	}
	if (OutActions.Num() == 0) { OutActions.Add(TEXT("Inspect")); }
}

void ABasePlayerController::RefreshInspectMenu(AActor* Target)
{
	if (!Target) { HideInspectMenu(); return; }
	FString Name, Description;
	DescribeInspectable(Target, Name, Description, InspectActions);
	InspectSelection = 0;
	if (!InspectMenuWidget) { InspectMenuWidget = CreateWidget<UInspectMenuWidget>(this, UInspectMenuWidget::StaticClass()); }
	InspectMenuWidget->SetContent(Name, FString(), InspectActions, InspectSelection);   // the description shows through Inspect, not in the menu
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
		if (InspectTarget.IsValid()) { RefreshInspectMenu(InspectTarget.Get()); }
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
				// seat:<top height>,<facing yaw> on the prop; a stool top at 66 facing +x by default.
				float Height = 66.0f, Yaw = Me->GetActorRotation().Yaw, Lean = 0.0f, Hunch = 0.0f;
				for (const FName& Tag : Target->Tags)
				{
					const FString T = Tag.ToString();
					if (!T.StartsWith(TEXT("seat:"))) { continue; }
					TArray<FString> Parts; T.Mid(5).ParseIntoArray(Parts, TEXT(","));
					if (Parts.Num() > 0) { Height = FCString::Atof(*Parts[0]); }
					if (Parts.Num() > 1) { Yaw = FCString::Atof(*Parts[1]); }
					if (Parts.Num() > 2) { Lean = FCString::Atof(*Parts[2]); }
					if (Parts.Num() > 3) { Hunch = FCString::Atof(*Parts[3]); }
				}
				Me->BeginSit(Target, Height, Yaw, Lean, Hunch);
			}
			if (InspectTarget.IsValid()) { RefreshInspectMenu(InspectTarget.Get()); }
		}
	}
	else if (ToggleTaggedLights(Target, Action))
	{
		// handled: a light switch panel (tags toggle:<Action>=<id> / lightid:<id>)
	}
	else if (Action == TEXT("Take"))
	{
		if (Inventory.Num() >= InventoryCapacity) { ShowCallout(GetPawn(), TEXT("No room."), 2.0f, false); return; }
		Inventory.Add(Name);
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
		// Hack, Press Button, ...: a note for now; gameplay hooks the event.
		ShowCallout(Target, FString::Printf(TEXT("%s: %s"), *Action, *Name), 2.5f, false);
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
	CalloutEndTime = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f) + Seconds;
	UpdateCallout();
}

void ABasePlayerController::UpdateCallout()
{
	if (!CalloutWidget || !CalloutWidget->IsInViewport()) { return; }
	AActor* Anchor = CalloutAnchor.Get();
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
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
	if (bPauseMenuOpen) { HidePauseMenu(); } else { ShowPauseMenu(); }
}

void ABasePlayerController::ShowPauseMenu()
{
	LowerWeaponForScreen();
	if (bPauseMenuOpen) { return; }
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
	bPauseMenuOpen = true;
	SetPause(true);
	ApplyInputMode();
}

void ABasePlayerController::ShowReference()
{
	if (bReferenceOpen) { return; }
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
	bReferenceOpen = true;
	SetPause(true);
	ApplyInputMode();
}

void ABasePlayerController::ShowScenesPanel()
{
	if (bScenesOpen) { return; }
	if (PauseMenuWidget && PauseMenuWidget->IsInViewport()) { PauseMenuWidget->RemoveFromParent(); }
	if (!ScenesWidget)
	{
		ScenesWidget = CreateWidget<UScenesWidget>(this, UScenesWidget::StaticClass());
		ScenesWidget->Setup(this);
		ScenesWidget->OnClose.BindUObject(this, &ABasePlayerController::HideScenesPanel);
	}
	ScenesWidget->Rebuild();
	PlaceConsolePage(ScenesWidget, ++ConsolePageZ);
	bScenesOpen = true;
	SetPause(true);
	ApplyInputMode();
}

void ABasePlayerController::HideScenesPanel()
{
	if (!bScenesOpen) { return; }
	bScenesOpen = false;
	if (ScenesWidget && ScenesWidget->IsInViewport()) { ScenesWidget->RemoveFromParent(); }
	// Straight back to the menu it came from -- unless a scene is starting, in which case
	// PlaySequence takes over and closing the menu is exactly what it wants.
	if (bPauseMenuOpen && PauseMenuWidget && !PauseMenuWidget->IsInViewport())
	{
		PauseMenuWidget->AddToViewport(100);
		PauseMenuWidget->SetAnchorsInViewport(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		PauseMenuWidget->SetAlignmentInViewport(FVector2D::ZeroVector);
	}
	ApplyInputMode();
}

void ABasePlayerController::ShowSettingsPanel()
{
	if (bSettingsOpen) { return; }
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
	bSettingsOpen = true;
	SetPause(true);
	ApplyInputMode();
}

void ABasePlayerController::HideSettingsPanel()
{
	if (!bSettingsOpen) { return; }
	bSettingsOpen = false;
	if (SettingsWidget && SettingsWidget->IsInViewport()) { SettingsWidget->RemoveFromParent(); }
	// Back to the menu it came from.
	if (bPauseMenuOpen && PauseMenuWidget && !PauseMenuWidget->IsInViewport())
	{
		PauseMenuWidget->AddToViewport(100);
		PauseMenuWidget->SetAnchorsInViewport(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		PauseMenuWidget->SetAlignmentInViewport(FVector2D::ZeroVector);
	}
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
	if (bPauseMenuOpen) { HidePauseMenu(); }
	// Pulling the outgoing page out first leaves a frame with no panel at all, and the incoming
	// page paints once before its ASCII rules have measured themselves against their own text and
	// before the booth capture has written a frame into the render target. Both read as the panel
	// tearing as the tab changes. So the page on screen is held there -- bRetainPageWidget stops
	// the Hide paths removing its widget -- while the next one is built and stays invisible; the
	// held page is dropped only when the new one reports it has settled.
	UUserWidget* Outgoing = nullptr;
	bRetainPageWidget = true;
	if (bReferenceOpen) { bReferenceOpen = false; Outgoing = ReferenceWidget; }
	if (bCharacterSheetOpen) { Outgoing = CharacterSheetWidget; HideCharacterSheet(); }
	if (bAppearanceOpen) { Outgoing = AppearanceWidget; FinishAppearance(); }
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

void ABasePlayerController::ShowWeaponPreview(const FString& MeshPath)
{
	HideWeaponPreview();
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
	if (!Mesh || !GetWorld()) { return; }
	FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	WeaponBoothActor = GetWorld()->SpawnActor<AStaticMeshActor>(WeaponBoothOrigin, FRotator::ZeroRotator, Params);
	if (!WeaponBoothActor) { return; }
	WeaponBoothActor->SetMobility(EComponentMobility::Movable);
	WeaponBoothActor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
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
	if (!WeaponTarget) { WeaponTarget = UKismetRenderingLibrary::CreateRenderTarget2D(this, 1024, 633, ETextureRenderTargetFormat::RTF_RGBA8_SRGB, FLinearColor::Black, false); }   // 1.618, the shape of the pane's box
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
	if (!bReferenceOpen) { return; }
	bReferenceOpen = false;
	HideWeaponPreview();
	if (ReferenceWidget && ReferenceWidget->IsInViewport()) { ReferenceWidget->RemoveFromParent(); }
	// Back to the menu it came from.
	if (bPauseMenuOpen && PauseMenuWidget && !PauseMenuWidget->IsInViewport())
	{
		PauseMenuWidget->AddToViewport(100);
		PauseMenuWidget->SetAnchorsInViewport(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		PauseMenuWidget->SetAlignmentInViewport(FVector2D::ZeroVector);
	}
	ApplyInputMode();
}

void ABasePlayerController::HidePauseMenu()
{
	if (!bPauseMenuOpen) { return; }
	if (bSettingsOpen) { bSettingsOpen = false; if (SettingsWidget && SettingsWidget->IsInViewport()) { SettingsWidget->RemoveFromParent(); } }
	if (bReferenceOpen) { bReferenceOpen = false; if (ReferenceWidget && ReferenceWidget->IsInViewport()) { ReferenceWidget->RemoveFromParent(); } }
	bPauseMenuOpen = false;
	if (PauseMenuWidget && PauseMenuWidget->IsInViewport()) { PauseMenuWidget->RemoveFromParent(); }
	SetPause(false);
	ApplyInputMode();
}

void ABasePlayerController::ShowCharacterSheet()
{
	LowerWeaponForScreen();
	if (bCharacterSheetOpen || bPauseMenuOpen) { return; }
	if (!CharacterSheetWidget)
	{
		CharacterSheetWidget = CreateWidget<UCharacterSheetWidget>(this, UCharacterSheetWidget::StaticClass());
		CharacterSheetWidget->SetOwnerController(this);
	}
	ShowSheetMirror();
	CharacterSheetWidget->Rebuild();   // from UI/CharacterSheet.json, re-read if it changed
	CharacterSheetWidget->Refresh();
	PlaceConsolePage(CharacterSheetWidget, ConsolePageZ);
	bCharacterSheetOpen = true;
	SetPause(true);
	ApplyInputMode();
}

void ABasePlayerController::HideCharacterSheet()
{
	if (!bCharacterSheetOpen) { return; }
	bCharacterSheetOpen = false;
	HideSheetMirror();
	if (CharacterSheetWidget && CharacterSheetWidget->IsInViewport() && !bRetainPageWidget) { CharacterSheetWidget->RemoveFromParent(); }
	if (!bPauseMenuOpen) { SetPause(false); }
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
	if (bCharacterSheetOpen) { HideCharacterSheet(); } else { ShowCharacterSheet(); }
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

	if (bPauseMenuOpen || bCharacterSheetOpen || bAppearanceInputMode || bTransferOpen || bReferenceOpen)
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
	const FString ScreenshotPath = FPaths::Combine(Dir, FString::Printf(TEXT("click_%d.png"), AssistClickCount));
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
	if (NewTarget != InspectTarget.Get())
	{
		SetInspectHighlight(InspectTarget.Get(), false);
		InspectTarget = NewTarget;
		SetInspectHighlight(NewTarget, true);
		RefreshInspectMenu(NewTarget);
	}
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
	if (!Box || bTransferOpen || bPauseMenuOpen) { return; }
	HideInspectMenu();
	if (!TransferWidget) { TransferWidget = CreateWidget<UInventoryTransferWidget>(this, UInventoryTransferWidget::StaticClass()); }
	TransferWidget->Open(this, Box);
	PlaceConsolePage(TransferWidget, 90);
	bTransferOpen = true;
	SetPause(true);
	ApplyInputMode();
}

void ABasePlayerController::CloseTransfer()
{
	if (!bTransferOpen) { return; }
	bTransferOpen = false;
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
	if (!bAppearanceOpen && !bSheetMirror) { Who->SetGazeLock(true, Eye); } else if (Who->IsGazeLocked()) { Who->SetGazeLock(false, Eye); }   // the chooser's and the sheet's figure just idle
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

void ABasePlayerController::ShowSheetMirror()
{
	if (bSheetMirror || bAppearanceOpen || BoothCharacter.IsValid() || !GetWorld()) { return; }
	// The saved likeness (Player.json) in the booth; a fresh player without one gets no mirror yet.
	ABaseCharacter* Preview = SpawnBoothCharacter(TEXT("Player"));
	if (!Preview) { return; }
	bSheetMirror = true;
	SheetOrbitYaw = 0.0f; SheetOrbitPitch = 0.0f; SheetBaseYaw = Preview->GetActorRotation().Yaw;
	// The sheet pauses the game; the figure keeps idling and the capture keeps running.
	Preview->SetTickableWhenPaused(true);
	if (Preview->GetMesh()) { Preview->GetMesh()->SetTickableWhenPaused(true); }
	RemoteViewResolution = 1024;
	ShowRemoteView(Preview, false);
	// The sheet's own portrait target (5:8) replaces the square one on the capture while the sheet is up.
	if (!SheetTarget) { SheetTarget = UKismetRenderingLibrary::CreateRenderTarget2D(this, 640, 1024, ETextureRenderTargetFormat::RTF_RGBA8_SRGB, FLinearColor::Black, false); }
	if (RemoteCapture) { RemoteCapture->SetTickableWhenPaused(true); if (USceneCaptureComponent2D* Cap = RemoteCapture->GetCaptureComponent2D()) { Cap->SetTickableWhenPaused(true); Cap->TextureTarget = SheetTarget; } }
	bSheetHead = false;
	DressBoothAsMirror();
	PlaceSheetCamera();
}

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
	Inventory.RemoveAt(InventoryIndex);
	if (!Equipped[Slot].IsEmpty()) { Inventory.Add(Equipped[Slot]); }   // swap the old one back into the bag
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
		const int32 Slot = Spec.SlotForKind(W->Kind);
		if (Slot < 0 || !Equipped.IsValidIndex(Slot)) { UE_LOG(LogTemp, Warning, TEXT("GiveStartingWeapons: %s (%s) fits no slot"), *Item, *W->Kind); return; }
		if (!Equipped[Slot].IsEmpty()) { return; }
		Equipped[Slot] = Item;
	};
	Fit(StartingPrimary);
	Fit(StartingSidearm);
	RefreshHeldWeapon();
}

void ABasePlayerController::RefreshHeldWeapon()
{
	ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn());
	if (!Me) { return; }
	const FSheetSpec& Spec = FSheetSpec::Get();

	// Slot 1 is the primary, Slot 2 the sidearm. A held slot that is still filled stays held, so
	// picking something up does not snatch the weapon out of the player's hand.
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
	const WeaponCatalog::FOptic* Optic = WeaponCatalog::FindOptic(W->Optic);
	UStaticMesh* OpticMesh = Optic ? LoadObject<UStaticMesh>(nullptr, *Optic->MeshPath, nullptr, LOAD_NoWarn | LOAD_Quiet) : nullptr;
	if (OpticMesh && !W->BodyMeshPath.IsEmpty())
	{
		if (UStaticMesh* Body = LoadObject<UStaticMesh>(nullptr, *W->BodyMeshPath, nullptr, LOAD_NoWarn | LOAD_Quiet)) { Mesh = Body; }
	}
	// Clear any offset a saved character config is still carrying from before the socket existed.
	// A HAC1 weapon on a measured socket needs nothing added to it, and an old config quietly
	// putting the rifle back where it used to be is very hard to see from the outside.
	Me->SetWeaponRelativeLocation(FVector::ZeroVector);
	Me->SetWeaponRelativeRotation(FRotator::ZeroRotator);
	Me->SetWeaponMesh(Mesh);
	if (OpticMesh && !Optic->Eye.IsNearlyZero()) { Me->SetWeaponSight(W->OpticMount + Optic->Eye, true, 0.0f); }
	else { Me->SetWeaponSight(W->Sight, W->bHasSight, W->SightPitch); }
	Me->SetWeaponMuzzle(W->Muzzle);
	Me->SetWeaponForeGrip(W->ForeGrip, W->bHasForeGrip, W->ForeGripPitch);
	Me->SetWeaponHipFire(W->bHipFire);
	Me->SetWeaponOptic(OpticMesh, OpticMesh ? W->OpticMount : FVector::ZeroVector);
	// How the body holds it is the weapon's own business: it carries a stance name and the
	// character composes clip paths from it. Nothing here decides what a rifle looks like.
	Me->SetWeaponStance(W->Stance);
	if (!W->Space.Equals(TEXT("hac1")))
	{
		UE_LOG(LogTemp, Warning, TEXT("RefreshHeldWeapon: %s has not been normalised (Tools/normalise_weapons.py); it will hang wrong"), *W->Name);
	}
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
	if (Filled.Num() < 2) { SetDiagNoteTimed(TEXT("Nothing else to draw"), 3.0f); return; }
	const int32 At = FMath::Max(0, Filled.IndexOfByKey(HeldSlot));
	const int32 Step = (Direction >= 0) ? 1 : Filled.Num() - 1;   // wrap both ways
	HeldSlot = Filled[(At + Step) % Filled.Num()];
	RefreshHeldWeapon();
	SetDiagNoteTimed(FString::Printf(TEXT("Drew %s"), *Equipped[HeldSlot]), 3.0f), void();
}

void ABasePlayerController::FireHeldWeapon()
{
	ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn());
	if (!Me || !GetWorld() || !PlayerCameraManager) { return; }
	if (!Equipped.IsValidIndex(HeldSlot)) { return; }
	const WeaponCatalog::FWeapon* W = WeaponCatalog::Find(Equipped[HeldSlot]);
	if (!W) { return; }
	if (!W->bRanged) { SetDiagNoteTimed(FString::Printf(TEXT("%s is not a firearm"), *W->Name), 3.0f); return; }

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

	// Hitscan down the CAMERA's line, not the muzzle's. The reticle is what the player aimed
	// with, and a shot that leaves along the barrel instead lands somewhere else whenever the
	// weapon is not perfectly aligned with the view -- which, held in a hand, it never is.
	// Down the AIM, which is normally the camera but is not while free looking -- there the
	// camera has swung away and the rifle is still pointing where it was.
	const FVector Start = Me->GetAimOrigin();
	// Inside the cone the reticle is drawing. Using the same number for both means the ring on
	// screen is a promise: a shot can land anywhere inside it and nowhere outside.
	const float SpreadDeg = Me->GetWeaponSpreadDegrees();
	const FVector Aim = FMath::VRandCone(Me->GetAimRotation().Vector(), FMath::DegreesToRadians(SpreadDeg));
	const FVector End = Start + Aim * 20000.0f;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(WeaponFire), true, Me);
	TArray<AActor*> Attached;
	Me->GetAttachedActors(Attached, true, true);
	Params.AddIgnoredActors(Attached);
	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);

	Me->OnWeaponFired(W->Muzzle);
	// The shot has to arrive somewhere. Without this the muzzle flashes and the world does not
	// react at all, which reads as the gun not working rather than as a miss.
	ImpactEffects::Play(GetWorld(), Hit, Me);
	const FString Wav = W->Sound.IsEmpty() ? TEXT("wep_pistol.wav")
		: (W->Sound.EndsWith(TEXT(".wav")) ? W->Sound : W->Sound + TEXT(".wav"));
	UAmbientPlayer::PlayOneShot(this, GetWorld(), Wav, 1.0f, FMath::FRandRange(0.94f, 1.06f));   // full: the report is the loudest thing the player does

	SetDiagNoteTimed(bHit
		? FString::Printf(TEXT("%s -> %s at %.0f m"), *W->Name, *Hit.GetActor()->GetActorNameOrLabel(), Hit.Distance / 100.0f)
		: FString::Printf(TEXT("%s -> miss"), *W->Name), 4.0f);
}

void ABasePlayerController::TickPendingFire(float DeltaSeconds)
{
	if (PendingFireLeft <= 0.0f) { return; }
	PendingFireLeft -= DeltaSeconds;
	if (PendingFireLeft > 0.0f) { return; }
	PendingFireLeft = 0.0f;
	// The weapon is up now, so this call takes the normal path.
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

bool ABasePlayerController::IsAnyScreenOpen() const
{
	return bPauseMenuOpen || bScenesOpen || bSettingsOpen || bReferenceOpen || bCharacterSheetOpen || bTransferOpen || bAppearanceOpen
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

void ABasePlayerController::ReloadHeldWeapon()
{
	ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn());
	if (!Me || HeldSlot < 0) { return; }
	if (Me->IsWeaponBusy()) { return; }
	const WeaponCatalog::FWeapon* W = Equipped.IsValidIndex(HeldSlot) ? WeaponCatalog::Find(Equipped[HeldSlot]) : nullptr;
	if (!W || !W->bRanged) { return; }
	if (!Me->PlayWeaponAction(TEXT("Reload")))
	{
		SetDiagNoteTimed(FString::Printf(TEXT("No reload animation for a %s"), *W->Stance), 3.0f);
		return;
	}
	// Reloading lowers the weapon, so it cancels the sights rather than fighting them.
	Me->SetAiming(false);
	SetDiagNoteTimed(FString::Printf(TEXT("Reloading %s"), *W->Name), 2.0f);
}

void ABasePlayerController::MeleeHeldWeapon()
{
	ABaseCharacter* Me = Cast<ABaseCharacter>(GetPawn());
	if (!Me || HeldSlot < 0 || Me->IsWeaponBusy()) { return; }
	Me->PlayWeaponAction(TEXT("Melee"));
}

bool ABasePlayerController::UnequipSlot(int32 Slot)
{
	if (!Equipped.IsValidIndex(Slot) || Equipped[Slot].IsEmpty()) { return false; }
	if (Inventory.Num() >= InventoryCapacity) { ShowCallout(GetPawn(), TEXT("No room."), 2.0f, false); return false; }
	Inventory.Add(Equipped[Slot]);
	Equipped[Slot].Reset();
	RefreshHeldWeapon();
	return true;
}

void ABasePlayerController::BeginAppearance()
{
	if (bAppearanceOpen || !GetWorld()) { return; }
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
	bAppearanceOpen = true;
	AppearanceOrbitYaw = 0.0f; AppearanceOrbitPitch = 0.0f; AppearanceBaseYaw = Preview->GetActorRotation().Yaw;
	ApplyAppearance();
	RemoteViewResolution = 1024;          // the mirror is large here; a sharp capture
	ShowRemoteView(Preview, false);       // the feed lives inside the page, not the square
	// The same portrait target the character sheet uses: both pages frame the figure 5:8.
	if (!SheetTarget) { SheetTarget = UKismetRenderingLibrary::CreateRenderTarget2D(this, 640, 1024, ETextureRenderTargetFormat::RTF_RGBA8_SRGB, FLinearColor::Black, false); }
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
	if (!bAppearanceOpen) { return; }
	AppearanceOrbitYaw = FMath::Fmod(AppearanceOrbitYaw + DeltaYaw, 360.0f);
	AppearanceOrbitPitch = FMath::Clamp(AppearanceOrbitPitch + DeltaPitch, -30.0f, 40.0f);
	ApplyAppearance();
}

void ABasePlayerController::SetAppearanceHeadView(bool bHead)
{
	const int32 Want = bHead ? 0 : 1;
	if (!bAppearanceOpen || Appearance.View == Want) { return; }
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
	if (!bAppearanceOpen) { return; }
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
	bAppearanceOpen = false;
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

FString ABasePlayerController::UIAudit()
{
	UUserWidget* Page = bCharacterSheetOpen ? Cast<UUserWidget>(CharacterSheetWidget)
		: bReferenceOpen ? Cast<UUserWidget>(ReferenceWidget)
		: bAppearanceOpen ? Cast<UUserWidget>(AppearanceWidget) : nullptr;
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
