// The gameplay PlayerController, and the home of edit mode.
//
// SELECTION and CONTROL are separate things:
//  - Selected = the character whose properties the Character Manager is
//    editing (clicking a character in edit mode selects it). It is NOT
//    necessarily the one being played.
//  - Controlled = the possessed pawn: the character the player's inputs
//    drive in first/third person. The manager's "Control" button hands
//    control to the selected character; with no character controlled the
//    player flies a spectator pawn (WASD, hold RMB to look).
//
// TAB toggles EDIT MODE: the cursor is free (RMB held = look), hovering a
// character highlights it and shows its name tag, clicking selects it, and
// the edit-mode pages show bottom-left -- the EditTool ("New Character") and
// the Character Manager (the selected character, with Back / Control /
// Remove / Deselect). The controller is the one object that survives
// switching pawns, which is why the pages, the selection, placement mode
// and the ghost preview all live here rather than on a character.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ConversationData.h"
#include "BasePlayerController.generated.h"

class ABaseCharacter;
class UTextureRenderTarget2D;
class ASpectatorPawn;
class UCharacterBuilderWidget;
class UEditToolWidget;
class ACharacterGhostActor;

UENUM()
enum class EEditPage : uint8
{
	None,
	EditTool,
	CharacterManager,
	FaceManager,
	AnimBrowser,
};

UCLASS()
class REPLICAN_API ABasePlayerController : public APlayerController
{
	GENERATED_BODY()

public:

	ABasePlayerController();

	// Everything here is BlueprintCallable so the flow can be driven and
	// verified from the Python remote-exec harness in PIE (no mouse there).

	// ---- Edit mode and its pages
	UFUNCTION(BlueprintCallable, Category = "Edit Mode") void ToggleEditMode();
	UFUNCTION(BlueprintCallable, Category = "Edit Mode") void EnterEditMode();
	UFUNCTION(BlueprintCallable, Category = "Edit Mode") void ExitEditMode();
	UFUNCTION(BlueprintPure, Category = "Edit Mode") bool IsEditMode() const { return bEditMode; }

	// ---- Character / Inventory sheet (Tab) ---------------------------------
	// A paused modal over most of the view: the unit record and inventory.
	UFUNCTION(BlueprintCallable, Category = "Sheet") void ShowCharacterSheet();
	UFUNCTION(BlueprintCallable, Category = "Sheet") void HideCharacterSheet();
	UFUNCTION(BlueprintCallable, Category = "Sheet") void ToggleCharacterSheet();
	UFUNCTION(BlueprintPure, Category = "Sheet") bool IsCharacterSheetOpen() const { return bCharacterSheetOpen; }
	// The account balance the sheet shows; the replicant starts in debt.
	UPROPERTY(BlueprintReadWrite, Category = "Sheet") int32 Credits = -13000;

	// ---- Inspectables ------------------------------------------------------
	// Any actor tagged InspectableTag (a character's "inspectable" tag lands
	// on the actor via SyncActorTagsFromConfig; props get it as an actor
	// tag) is outlined while the reticle rests on it and it is within
	// InspectDistance of the player's capsule -- 1st or 3rd person, not in
	// edit/fly mode. The outline is a post-process pass over the custom
	// depth stencil (PP_InspectOutline): a thin pulsing, marching line
	// around the silhouette.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inspect")
	float InspectDistance = 300.0f;
	// Gap between the conversation panel (right third of the screen) and the
	// screen edges, in Slate units.
	UPROPERTY(EditAnywhere, Category = "Conversation")
	float ConversationPanelPadding = 24.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inspect")
	float InspectTraceRange = 2500.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inspect")
	FName InspectableTag = TEXT("inspectable");
	UFUNCTION(BlueprintPure, Category = "Inspect")
	AActor* GetInspectTarget() const { return InspectTarget.Get(); }

	// ---- Inspect context menu ------------------------------------------------
	// While something is inspected a small menu beside the reticle lists its
	// friendly name, description and actions; the mouse wheel moves the
	// selection, E uses it. Characters offer Inspect (+ Talk when they have a
	// Comment); props offer whatever "action:<Label>" tags they carry (Inspect
	// by default), with "name:<Friendly Name>" / "desc:<text>" tags for the
	// header. Talk / Inspect / Take have stub behaviours here; every action
	// also fires OnInspectAction for gameplay to hook.
	UFUNCTION(BlueprintPure, Category = "Inspect") bool IsInspectMenuOpen() const;
	// Where the inspect menu sits: 0 beside the thing, 1 fixed top-centre, 2 fixed top-right with a
	// leader line to the thing, 3 just above the reticle. F8 cycles; RepliCan.InspectMenu overrides.
	UFUNCTION(BlueprintCallable, Category = "Inspect") void CycleInspectMenuMode();
	UFUNCTION(BlueprintPure, Category = "Inspect") int32 GetInspectMenuMode() const;
	UFUNCTION(BlueprintPure, Category = "Inspect") int32 GetInspectSelection() const { return InspectSelection; }
	UFUNCTION(BlueprintPure, Category = "Inspect") TArray<FString> GetInspectActions() const { return InspectActions; }
	UFUNCTION(BlueprintCallable, Category = "Inspect") void InspectSelectNext(int32 Delta);
	UFUNCTION(BlueprintCallable, Category = "Inspect") void InspectUseSelected();
	// Floating text over an actor for Seconds (a character's line, or an
	// action's note). Speech is shown in quotes/italics.
	UFUNCTION(BlueprintCallable, Category = "Inspect") void ShowCallout(AActor* Anchor, const FString& Text, float Seconds = 3.5f, bool bSpeech = false);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FInspectActionEvent, AActor*, Target, const FString&, Action);
	UPROPERTY(BlueprintAssignable, Category = "Inspect") FInspectActionEvent OnInspectAction;
	UPROPERTY(BlueprintReadOnly, Category = "Inspect") TArray<FString> Inventory;   // what "Take" collected (friendly names)
	// Equipped gear, one string per ItemCatalog::GearSlots entry (empty = nothing there).
	UPROPERTY(BlueprintReadOnly, Category = "Inspect") TArray<FString> Equipped;
	UFUNCTION(BlueprintCallable, Category = "Inspect") bool EquipFromInventory(int32 InventoryIndex);
	// Drops the cached UI widgets so the next open rebuilds them (after a Live Coding patch or a spec edit).
	UFUNCTION(Exec, BlueprintCallable, Category = "Debug") void ReloadUI();
	// Every widget on the open console page that wants more room than it was given, or that
	// reaches past the page's edge -- the layout's overflow, as numbers. UIAudit in the console;
	// ui_audit() from Python, which gets the text back.
	UFUNCTION(Exec, BlueprintCallable, Category = "Debug") FString UIAudit();
	// Gets the pawn out of geometry: the pause menu's UNSTUCK, also a console command.
	UFUNCTION(Exec, BlueprintCallable, Category = "Player") void Unstuck();
	// The trigger hand's correction on the weapon, live: HandRot pitch yaw roll (weapon space). HandRot alone prints it.
	UFUNCTION(Exec) void HandRot(float Pitch = 1000.0f, float Yaw = 0.0f, float Roll = 0.0f);
	// Turns and walks for N seconds measuring the weapon's per-frame jump in camera space (the
	// judder) and the predicted-vs-final camera error; WeaponLagReport reads the result back.
	UFUNCTION(Exec, BlueprintCallable, Category = "Debug") void WeaponLagTest(float Seconds = 4.0f);
	UFUNCTION(BlueprintCallable, Category = "Debug") FString WeaponLagReport() const;
	UFUNCTION(BlueprintCallable, Category = "Inspect") bool UnequipSlot(int32 Slot);
	// The character sheet's mirror: the player's saved likeness in the booth, orbited by dragging.
	UFUNCTION(BlueprintCallable, Category = "Sheet") void OrbitSheetMirror(float DeltaYaw, float DeltaPitch);
	UFUNCTION(BlueprintPure, Category = "Sheet") class UTextureRenderTarget2D* GetSheetFeed() const { return bSheetMirror ? SheetTarget : nullptr; }
	// A click on the mirror's face zooms to the head; a click while zoomed goes back to the figure.
	UFUNCTION(BlueprintCallable, Category = "Sheet") void SetSheetMirrorZoom(bool bHead);
	UFUNCTION(BlueprintPure, Category = "Sheet") bool IsSheetMirrorZoomed() const { return bSheetHead; }
	static constexpr int32 InventoryCapacity = 20;
	// The container transfer screen (a loot box's "Open").
	UFUNCTION(BlueprintCallable, Category = "Inspect") void OpenTransfer(class ALootBoxActor* Box);
	UFUNCTION(BlueprintCallable, Category = "Inspect") void CloseTransfer();
	UFUNCTION(BlueprintPure, Category = "Inspect") bool IsTransferOpen() const { return bTransferOpen; }
	// The hover highlight and inspect menu are off until the story hands them over (Hannah's call in the cabin).
	UFUNCTION(BlueprintCallable, Category = "Inspect") void SetInspectEnabled(bool bEnabled) { bInspectEnabled = bEnabled; }
	UFUNCTION(BlueprintPure, Category = "Inspect") bool IsInspectEnabled() const { return bInspectEnabled; }
	UFUNCTION(BlueprintCallable, Category = "Inspect") void ExecuteInspectActionOn(AActor* Target, const FString& Action) { ExecuteInspectAction(Target, Action); }

	// ---- Conversations ------------------------------------------------------
	// Talk on a character with a <Project>/Conversations/<Name>.json tree
	// (see ConversationData.h) opens the split-screen conversation: the
	// camera frames the NPC in the left half, the exchange and the numbered
	// choices fill the right. Click a choice or press 1-9; Tab/Esc leave.
	// Characters without a tree just say their Comment.
	UFUNCTION(BlueprintCallable, Category = "Conversation") bool StartConversation(ABaseCharacter* With);
	// The same, from a named tree (Conversations/<TreeName>.json) rather than the character's own.
	UFUNCTION(BlueprintCallable, Category = "Conversation") bool StartConversationTree(ABaseCharacter* With, const FString& TreeName);
	UFUNCTION(BlueprintCallable, Category = "Conversation") void EndConversation();
	// "Remote communication": a live camera feed of Who, cropped to the head
	// from the angle their config gives, in a square left of the panel.
	UFUNCTION(BlueprintCallable, Category = "Conversation") void ShowRemoteView(ABaseCharacter* Who, bool bWithSquare = true);
	UFUNCTION(BlueprintCallable, Category = "Conversation") void HideRemoteView();
	UFUNCTION(BlueprintPure, Category = "Conversation") bool IsRemoteViewOpen() const { return RemoteSubject.IsValid(); }
	void UpdateRemoteView();
	// Side of the square feed's render target; set before the first remote
	// view for a sharper capture (the poster shoot uses 1024).
	UPROPERTY(BlueprintReadWrite, Category = "Conversation") int32 RemoteViewResolution = 320;
	UFUNCTION(BlueprintPure, Category = "Conversation") UTextureRenderTarget2D* GetRemoteTarget() const { return RemoteTarget; }
	// A call from someone who is not in this world: they are spawned from
	// their config into a hidden booth far below the level (a lit backdrop),
	// and the remote camera watches them there. Returns false if the config
	// does not exist. Someone already in the world is used where they stand.
	UFUNCTION(BlueprintCallable, Exec, Category = "Conversation") bool ShowRemoteCall(const FString& ConfigName);
	// A remote conversation: that character's tree, their feed in the square,
	// the player's own camera left alone.
	UFUNCTION(BlueprintCallable, Exec, Category = "Conversation") bool StartRemoteConversation(const FString& ConfigName);
	UFUNCTION(BlueprintPure, Category = "Conversation") bool IsBoothActive() const { return BoothCharacter.IsValid(); }
	// Dressing the booth (poster shoots, dramatic calls): props, Niagara FX
	// and lights placed relative to the booth origin, torn down with it.
	UFUNCTION(BlueprintCallable, Category = "Conversation") AActor* AddBoothProp(const FString& MeshPath, FVector Location, FRotator Rotation, FVector Scale, bool bGrime = false);
	UFUNCTION(BlueprintCallable, Category = "Conversation") bool AddBoothFX(const FString& SystemPath, FVector Location, FRotator Rotation, FVector Scale);
	UFUNCTION(BlueprintCallable, Category = "Conversation") AActor* AddBoothLight(FVector Location, float Candelas, float Radius, FLinearColor Color, bool bFlicker = false);
	UFUNCTION(BlueprintCallable, Category = "Conversation") void SetBoothSetVisible(bool bVisible);
	UFUNCTION(BlueprintPure, Category = "Conversation") FVector GetBoothOrigin() const { return FVector(0.0f, 0.0f, -30000.0f); }

	// ---- Appearance chooser --------------------------------------------------
	// Body / head / skin / hair / beard / stubble / height / build on a preview
	// in the booth, shown as a mirror feed beside the panel. Accepting writes
	// Characters/Player.json and applies it to the player's pawn.
	UFUNCTION(Exec, BlueprintCallable, Category = "Appearance") void BeginAppearance();
	UFUNCTION(BlueprintCallable, Category = "Appearance") void FinishAppearance();
	UFUNCTION(BlueprintCallable, Category = "Appearance") void AppearanceStep(const FString& Row, int32 Delta);
	UFUNCTION(BlueprintCallable, Category = "Appearance") void RandomiseAppearance();
	// Back to the stock cryo body of the selected sex: cryo head, painted skin, nothing else.
	UFUNCTION(BlueprintCallable, Category = "Appearance") void DefaultAppearance();
	UFUNCTION(BlueprintCallable, Category = "Appearance") void SetAppearanceName(const FString& First, const FString& Last);
	// The CRT cursor is installed from code as well as the ini, so it cannot be lost to config parsing.
	UFUNCTION(BlueprintPure, Category = "UI") bool HasSoftwareCursor() const;
	// The top-left performance readout (F11).
	UFUNCTION(Exec, BlueprintCallable, Category = "UI") void ToggleMetrics();
	// Test hooks: press a reply button through Slate, count the buttons on offer.
	UFUNCTION(Exec, BlueprintCallable, Category = "Conversation") void ClickChoice(int32 Index);
	UFUNCTION(BlueprintPure, Category = "Conversation") int32 CountChoiceButtons() const;
	UFUNCTION(BlueprintPure, Category = "Conversation") FString DescribeChoices() const;
	UFUNCTION(BlueprintPure, Category = "Conversation") bool IsRemoteViewActive() const { return RemoteSubject.IsValid(); }
	// Debug: moves the mouse to the viewport centre (a cursor query follows) / reports the platform cursor type.
	UFUNCTION(Exec, BlueprintCallable, Category = "UI") void CursorProbe();
	UFUNCTION(BlueprintPure, Category = "UI") FString CursorState() const;
	void EnsureSoftwareCursor();
	UFUNCTION(BlueprintPure, Category = "Appearance") FString GetAppearanceValue(const FString& Row) const;
	UFUNCTION(BlueprintPure, Category = "Appearance") bool IsAppearanceOpen() const { return bAppearanceOpen; }
	// The Appearance page frames the head or the whole figure; clicking the face swaps them,
	// the way the character sheet's mirror does.
	UFUNCTION(BlueprintPure, Category = "Appearance") bool IsAppearanceHeadView() const { return Appearance.View == 0; }
	UFUNCTION(BlueprintCallable, Category = "Appearance") void SetAppearanceHeadView(bool bHead);
	// The name the player chose (the pawn's display name, falling back to the saved Player.json).
	UFUNCTION(BlueprintPure, Category = "Appearance") FString GetPlayerDisplayName() const;
	// Rows that do not apply to the current body (beards on a female body) are dimmed and ignored.
	UFUNCTION(BlueprintPure, Category = "Appearance") bool IsAppearanceRowEnabled(const FString& Row) const;
	// Debug: 0 = set + sky, 1 = no set, 2 = no set and no atmosphere/fog flags in the capture.
	UFUNCTION(Exec, BlueprintCallable, Category = "Appearance") void AppearanceBackdrop(int32 Mode);
	// Drag on the mirror: swing the booth camera around the preview (degrees).
	UFUNCTION(BlueprintCallable, Category = "Appearance") void OrbitAppearanceCamera(float DeltaYaw, float DeltaPitch);
	UFUNCTION(BlueprintCallable, Category = "Conversation") void ChooseConversationOption(int32 Index);
	UFUNCTION(BlueprintPure, Category = "Conversation") bool IsInConversation() const { return ConversationPartner.IsValid(); }
	UFUNCTION(BlueprintPure, Category = "Conversation") FString GetConversationNode() const { return ConversationNodeId; }
	UFUNCTION(BlueprintPure, Category = "Conversation") TArray<FString> GetConversationChoices() const { return ConversationChoiceTexts; }
	UFUNCTION(BlueprintPure, Category = "Conversation") TArray<FString> GetConversationHistory() const { return ConversationHistory; }
	// True while the NPC's baked voice line (see VoiceLines.h) is playing.
	UFUNCTION(BlueprintPure, Category = "Conversation") bool IsVoicePlaying() const;

	// ---- Scripted sequences (SequenceData.h / SequenceDirector.h) ---------
	// <Project>/Sequences/<Name>.json played through the conversation panel
	// and a cinematic camera; the New Game intro is "NewGame", started when
	// the level is entered from the title's New Game (or `PlaySequence
	// NewGame` in the console). Esc skips.
	// Played when the level starts (new game from the title, or PIE directly
	// on the level) unless a save is being loaded or RepliCan.SkipNewGameIntro=1.
	UPROPERTY(EditAnywhere, Category = "Sequence") FString StartSequence = TEXT("NewGame");
	UFUNCTION(Exec, BlueprintCallable, Category = "Sequence") void PlaySequence(const FString& Name);
	UFUNCTION(Exec, BlueprintCallable, Category = "Sequence") void SkipSequence();
	UFUNCTION(Exec, BlueprintCallable, Category = "Sequence") void SkipAhead();      // Space: to the sequence's next skip mark
	UFUNCTION(BlueprintPure, Category = "Sequence") bool CanSkipAhead() const;
	// Debugging the input path: what has Slate focus, what the input stack and mappings look like.
	UFUNCTION(Exec, BlueprintCallable, Category = "Debug") FString DescribeInput() const;
	UFUNCTION(Exec, BlueprintCallable, Category = "Debug") void RefocusViewport();
	// Feeds a key straight into PlayerInput (below Slate), to test the input path without a hand on the keyboard.
	UFUNCTION(Exec, BlueprintCallable, Category = "Debug") void DebugKey(const FString& KeyName, bool bDown);
	// A note in the top-left corner: who is driving the game right now (Claude's scripts or the player) and what is expected.
	UFUNCTION(Exec, BlueprintCallable, Category = "Debug") void SetDiagNote(const FString& Text);
	// A note that clears itself after Seconds unless replaced.
	UFUNCTION(BlueprintCallable, Category = "Debug") void SetDiagNoteTimed(const FString& Text, float Seconds);
	// T: what is under the reticle: actor, mesh, material and its textures, noted for ten seconds.
	UFUNCTION(Exec, BlueprintCallable, Category = "Debug") void NoteTextureUnderReticle();
	// Alt+T: swap whatever is under the reticle to the NEXT of Synty's lettered palette
	// variants, so a material can be judged in the room rather than in the asset browser.
	UFUNCTION(Exec, BlueprintCallable, Category = "Debug") void CycleMaterialUnderReticle();

	// ---- Held weapon -----------------------------------------------------
	// Slot 1 is the primary and Slot 2 the sidearm; whichever is filled goes into the hand,
	// primary first. Called whenever the equipment changes.
	UFUNCTION(BlueprintCallable, Category = "Weapon") void RefreshHeldWeapon();
	// Swaps which of the two filled slots is being carried.
	UFUNCTION(Exec, BlueprintCallable, Category = "Weapon") void SwapWeaponSlot(int32 Direction = 1);
	// Fires whatever is in the hand: hitscan down the camera's own line, since that is what the
	// reticle promises. Ignored for a melee weapon or an empty hand.
	UFUNCTION(Exec, BlueprintCallable, Category = "Weapon") void FireHeldWeapon();
	// Both go through the held weapon stance, so a weapon with no such clip simply says so.
	UFUNCTION(Exec, BlueprintCallable, Category = "Weapon") void ReloadHeldWeapon();
	UFUNCTION(Exec, BlueprintCallable, Category = "Weapon") void MeleeHeldWeapon();
	// Puts the weapon away, or takes it back out. The slot stays selected while stowed, so the
	// same weapon comes back rather than whatever is in the first filled slot.
	UFUNCTION(Exec, BlueprintCallable, Category = "Weapon") void ToggleHolster();
	UFUNCTION(BlueprintPure, Category = "Weapon") bool IsHolstered() const { return bHolstered; }
	// The slot the player last chose, remembered across a holster. HeldSlot is -1 while stowed.
	int32 StowedSlot = -1;
	bool bHolstered = false;
	// Called when a screen opens: sights come down so the player is not left in ADS behind a panel.
	void LowerWeaponForScreen();
	// Which of Slot 1 / Slot 2 is in the hand, as a slot index; -1 for empty handed.
	int32 HeldSlot = -1;
	// What the player starts holding while the game is being built, by catalogue name. Empty
	// strings mean start empty handed. These are placeholders: once there is a real loadout or
	// a save to restore from, that should win and this should go.
	UPROPERTY(EditAnywhere, Category = "Weapon") FString StartingPrimary = TEXT("Frontier Assault Rifle 01");
	UPROPERTY(EditAnywhere, Category = "Weapon") FString StartingSidearm = TEXT("Street Pistol 01");
	void GiveStartingWeapons();
	FTimerHandle DiagNoteFadeTimer;
	UFUNCTION(BlueprintPure, Category = "Debug") FString GetDiagNote() const { return DiagNote; }
	UPROPERTY() FString DiagNote;
	// False while an unskippable sequence / conversation runs: the keys that
	// would cut it short are swallowed instead.
	UFUNCTION(BlueprintPure, Category = "Sequence") bool CanSkipSequence() const;
	UFUNCTION(BlueprintPure, Category = "Conversation") bool CanLeaveConversation() const;
	UFUNCTION(Exec, BlueprintCallable, Category = "Sequence") void AdvanceSequence();
	UFUNCTION(BlueprintPure, Category = "Sequence") bool IsInCinematic() const;
	UFUNCTION(BlueprintPure, Category = "Sequence") FString GetSequenceState() const;
	// Used by the director: the panel, voice, camera and control hand-over.
	void ShowConversationPanel(const FString& Speaker);
	void HideConversationPanel();
	void ConversationSay(const FString& Speaker, const FString& Text, bool bPlayer);
	void SetConversationChoices(const TArray<FString>& Choices);
	float PlayVoiceFile(ABaseCharacter* From, const FString& Path);   // seconds, 0 if missing
	void SetCinematicCamera(const FVector& Eye, const FVector& LookAt, float Fov, float Blend);
	class ACameraActor* GetCinematicCamera() const { return FaceCamera; }
	void BeginCinematic();
	void EndCinematic(float CameraBlend, bool bKeepBlink = false);
	// The groggy walk after the intro: first person, forward/back only, yaw
	// look with the pitch pinned, a slow sway, the sleepy eyelids kept on;
	// released when the pawn gets within Radius of Goal.
	void BeginGroggy(const FVector& Goal, float Radius, float Speed);
	void EndGroggy(bool bFadeBlink = false);
	// The blink overlay lets the eyes open fully and the veil clear over Seconds, then goes.
	void FadeOutBlinkOverlay(float Seconds);
	UFUNCTION(BlueprintPure, Category = "Sequence") bool IsGroggy() const { return bGroggy; }
	void ShowBlinkOverlay(const struct FBlinkParams& Params);
	void HideBlinkOverlay();
	void ShockBlinkOverlay(float Seconds);
	UFUNCTION(BlueprintPure, Category = "Sequence") float GetBlinkOpenness() const;
	UFUNCTION(BlueprintPure, Category = "Sequence") int32 GetBlinkCount() const;
	// Story flags set by choices ("set"), tested by "if"; saved with the game.
	UPROPERTY(BlueprintReadWrite, Category = "Conversation") TSet<FString> ConversationFlags;
	// "once" choices already taken, keyed "<Character>/<node>/<choice index>".
	UPROPERTY(BlueprintReadWrite, Category = "Conversation") TSet<FString> ConversationChoicesTaken;
	UFUNCTION(BlueprintPure, Category = "Edit Mode") EEditPage GetEditPage() const { return CurrentPage; }
	UFUNCTION(BlueprintCallable, Category = "Edit Mode") void ShowEditTool();
	UFUNCTION(BlueprintCallable, Category = "Edit Mode") void ShowCharacterManager();
	// The Face Manager page (face controls only) -- the view locks to a
	// camera a couple of metres in front of the selected character's face,
	// slightly above and to its right, until the page is left.
	UFUNCTION(BlueprintCallable, Category = "Edit Mode") void ShowFaceManager();
	UFUNCTION(BlueprintPure, Category = "Edit Mode") bool IsFaceManagerOpen() const { return bEditMode && CurrentPage == EEditPage::FaceManager; }
	// The Animation Browser page: try any imported clip on the selected character.
	UFUNCTION(BlueprintCallable, Category = "Edit Mode") void ShowAnimationBrowser();

	// Leaving the character pages (Back, Tab, picking another character)
	// with unsaved changes on the selected character asks Save / Discard /
	// Cancel first; Continue runs once that's settled (or at once when
	// nothing is dirty). "Save & Close" saves and returns to the Edit Tool.
	void LeaveCharacterPagesThen(TFunction<void()> Continue);
	UFUNCTION(BlueprintCallable, Category = "Edit Mode") void SaveSelectedAndClose();

	// ---- In-game menu (Esc) -------------------------------------------------
	// Pauses the game under a centred "Menu" box: Back unpauses and hides
	// it, Quit ends the game (the PIE session in the editor). Esc toggles it.
	UFUNCTION(BlueprintCallable, Category = "Menu") void ShowPauseMenu();
	UFUNCTION(BlueprintCallable, Category = "Menu") void HidePauseMenu();
	UFUNCTION(BlueprintCallable, Category = "Menu") void QuitGame();
	// Save / load the running game (USaveGameSubsystem, slot "Quick" from
	// the menu buttons and F5 / F9).
	UFUNCTION(BlueprintCallable, Category = "Menu") void QuickSave();
	UFUNCTION(BlueprintCallable, Category = "Menu") void QuickLoad();
	UFUNCTION(BlueprintPure, Category = "Menu") bool IsPauseMenuOpen() const { return bPauseMenuOpen; }

	// Is ANY screen up. Firing a weapon because a click landed on a menu button is the kind of
	// bug that only shows up as "I shot my own foot in the inventory", so the trigger asks this
	// one question rather than each caller remembering the current list of panels.
	UFUNCTION(BlueprintPure, Category = "Menu") bool IsAnyScreenOpen() const;

	// Clears free look if the key-up was lost (alt-tab) or a screen opened while it was held.
	void TickFreelookSafety();

	// A shot commanded from low ready waits for the weapon to come up. Seconds remaining.
	void TickPendingFire(float DeltaSeconds);
	float PendingFireLeft = 0.0f;
	// THE SELECTOR. Middle mouse cycles the weapon's fire_modes; the trigger is HELD from the
	// press to the release, and in auto the held trigger fires again every 1/fire_rate seconds.
	UFUNCTION(Exec, BlueprintCallable, Category = "Weapon") void CycleFireMode();
	void SetTriggerHeld(bool bHeld);
	void TickAutoFire(float DeltaSeconds);
	FString CurrentFireMode() const;
	FString FireMode;              // what the selector is on; "" = the weapon's first mode
	bool bTriggerHeld = false;
	float AutoFireClock = 0.0f;
	// The player's own shot in first person, and the impacts it makes: the report should be the
	// loudest thing on the deck, so it goes up and the impact sound comes down.
	UPROPERTY(EditAnywhere, Category = "Weapon|Sound") float ReportVolumeFirstPerson = 1.6f;
	UPROPERTY(EditAnywhere, Category = "Weapon|Sound") float ReportVolumeThirdPerson = 1.0f;
	UPROPERTY(EditAnywhere, Category = "Weapon|Sound") float ImpactScaleFirstPerson = 0.45f;
	// How far the rest of the mix drops on a shot and how long it takes to come back.
	UPROPERTY(EditAnywhere, Category = "Weapon|Sound") float ShotDuckDepth = 0.85f;
	UPROPERTY(EditAnywhere, Category = "Weapon|Sound") float ShotDuckSeconds = 1.1f;
	// The Reference screen (the item catalogue), reached from the menu; closing returns to the menu.
	UFUNCTION(BlueprintCallable, Category = "Menu") void ShowReference();
	UFUNCTION(BlueprintCallable, Category = "Menu") void HideReference();
	UFUNCTION(BlueprintPure, Category = "Menu") bool IsReferenceOpen() const { return bReferenceOpen; }
	// The console pages share a tab strip: 0 the unit's sheet, 1 Reference, 2 Appearance. Closes whatever is open, opens the target.
	// The settings page, shown over the paused game from the menu. The same widget the title
	// screen uses, so the two can never drift apart.
	UFUNCTION(BlueprintCallable, Category = "Menu") void ShowSettingsPanel();
	UFUNCTION(BlueprintCallable, Category = "Menu") void HideSettingsPanel();
	UPROPERTY() TObjectPtr<class USettingsWidget> SettingsWidget;

	// The scene browser: every Sequences/*.json with a button that plays it. A development
	// tool that ships in the menu deliberately -- being able to jump to any scene is as
	// useful to whoever is testing the game as it is to whoever is writing it.
	UFUNCTION(BlueprintCallable, Category = "Menu") void ShowScenesPanel();
	UFUNCTION(BlueprintCallable, Category = "Menu") void HideScenesPanel();
	UFUNCTION(BlueprintPure, Category = "Menu") bool IsScenesOpen() const { return bScenesOpen; }
	UPROPERTY() TObjectPtr<class UScenesWidget> ScenesWidget;
	bool bScenesOpen = false;
	bool bSettingsOpen = false;

	UFUNCTION(BlueprintCallable, Category = "Menu") void ShowConsolePage(int32 Tab);
	// Where a console page sits: the screen less the spec's margins, never smaller than the
	// spec's minimum -- below that the frame keeps its size and its far edges leave the screen
	// rather than the content squeezing. Re-applied whenever the viewport changes size.
	FMargin ConsolePageRect() const;
	void PlaceConsolePage(class UUserWidget* Page, int32 ZOrder);
	void KeepConsolePagesFitted();
	TArray<TWeakObjectPtr<class UUserWidget>> ConsolePages;
	FVector2D LastPageSize = FVector2D::ZeroVector;
	// A tab switch keeps the page already on screen up until the incoming page has laid out and
	// painted, so the panel never shows a frame that is empty or half-built. RetirePage parks the
	// outgoing widget (still in the viewport); the incoming page calls PageSettled once it has
	// settled, which is when the parked one is finally dropped. See ShowConsolePage.
	void RetirePage(class UUserWidget* Old);
	void PageSettled(const class UUserWidget* Incoming);
	// Every console page is added above the one it replaces, so the incoming page covers the
	// outgoing one the moment it becomes visible.
	int32 ConsolePageZ = 100;
	// Set while a page is being swapped: the Hide paths leave their widget in the viewport.
	bool bRetainPageWidget = false;
	UPROPERTY() TObjectPtr<class UUserWidget> RetiredPage;
	// Editor only, while RepliCan.PlayAtCamera is on: moves the pawn to wherever the level
	// viewport was looking from when Play was pressed, so you start where you were building.
	void PlaceAtEditorCamera();
	// The weapon booth behind the Reference's detail column: one static mesh far from the level,
	// lit like the icon shots, captured every frame (ticks while paused). Drag turns it, FIRE kicks it.
	UFUNCTION(BlueprintCallable, Category = "Reference") void ShowWeaponPreview(const FString& MeshPath);
	UFUNCTION(BlueprintCallable, Category = "Reference") void HideWeaponPreview();
	UFUNCTION(BlueprintCallable, Category = "Reference") void OrbitWeaponPreview(float DeltaYaw, float DeltaPitch);
	UFUNCTION(BlueprintCallable, Category = "Reference") void FireWeaponPreview(const FString& SoundFile);
	UFUNCTION(BlueprintCallable, Category = "Reference") void ResetWeaponPreviewView();
	// Places the preview camera at the distance that just fits the piece as currently turned.
	void FrameWeaponPreview();
	// Clicking the render steps between the fitted view and a closer look.
	UFUNCTION(BlueprintCallable, Category = "Reference") void ToggleWeaponPreviewZoom();
	// How much room to leave around the piece at the fitted distance, and how far in a click goes.
	float WeaponFrameMargin = 1.16f;
	// Measured: 0.55 crops a rifle to little more than its receiver. 0.72 lands the close
	// view at about a third larger than the fitted one, which still shows the silhouette.
	float WeaponZoomClose = 0.72f;
	float WeaponZoom = 1.0f;
	void TickWeaponPreview(float DeltaSeconds);
	UFUNCTION(BlueprintPure, Category = "Reference") class UTextureRenderTarget2D* GetWeaponPreviewFeed() const { return WeaponBoothActor ? WeaponTarget : nullptr; }
	// The booth's camera (world), its horizontal FOV and the piece's transform, for a page that
	// draws over the render. False when no piece is up.
	bool GetWeaponPreviewFrame(FTransform& OutCamera, float& OutFovDeg, FTransform& OutPiece) const;
	UFUNCTION(BlueprintPure, Category = "Edit Mode") bool IsCharacterManagerOpen() const { return bEditMode && CurrentPage == EEditPage::CharacterManager; }

	// ---- Selection (the character being edited) vs. control (the pawn)
	UFUNCTION(BlueprintPure, Category = "Edit Mode") ABaseCharacter* GetSelectedCharacter() const;
	// Makes Target the selected character: highlight + its Character Manager
	// page (in edit mode). Does NOT change which pawn is controlled. Null
	// clears the selection.
	UFUNCTION(BlueprintCallable, Category = "Edit Mode") void SelectCharacter(ABaseCharacter* Target);
	// Hands the player's inputs to Target (possesses it, leaving fly mode)
	// and selects it. The manager's "Control" button.
	UFUNCTION(BlueprintCallable, Category = "Edit Mode") void ControlCharacter(ABaseCharacter* Target);
	// Unpossesses into the spectator pawn and clears the selection.
	UFUNCTION(BlueprintCallable, Category = "Edit Mode") void EnterFlyMode();
	UFUNCTION(BlueprintPure, Category = "Edit Mode") bool IsFlying() const { return bFlying; }
	// Called by a possessed ABaseCharacter when the click action fires in
	// edit mode: the character's own binding consumes the action before the
	// controller's would run, so it forwards the click here.
	void NotifyEditClick();

	// ---- Placement ("New"): the cursor becomes a crosshair and a
	// translucent ghost of the default character follows it, green where
	// the spot is a horizontal surface with room for the capsule, red
	// otherwise. A click on a green spot spawns the character and selects
	// it; leaving edit mode cancels.
	UFUNCTION(BlueprintCallable, Category = "Edit Mode") void BeginPlacingCharacter();
	UFUNCTION(BlueprintCallable, Category = "Edit Mode") void CancelPlacingCharacter();
	UFUNCTION(BlueprintPure, Category = "Edit Mode") bool IsPlacingCharacter() const { return bPlacingCharacter; }
	UFUNCTION(BlueprintCallable, Category = "Edit Mode") ABaseCharacter* PlaceCharacterAt(const FVector& FloorPoint);

	// Destroys the selected character instance (no file is touched) and
	// drops into fly mode. False if nothing is selected.
	UFUNCTION(BlueprintCallable, Category = "Edit Mode") bool RemoveSelectedCharacter();

	// ---- Claude Assist: the next click in the world is captured -- hit
	// point, normal, actor / component / mesh asset / material under the
	// cursor, camera, selection -- and written to
	// <Project>/Saved/ClaudeAssist/last_click.json (and appended to
	// clicks.jsonl) with a screenshot request next to it, so the assistant
	// can read the click as context. One click per activation.
	UFUNCTION(BlueprintCallable, Category = "Edit Mode") void BeginClaudeAssist();
	UFUNCTION(BlueprintCallable, Category = "Edit Mode") void CancelClaudeAssist();
	UFUNCTION(BlueprintPure, Category = "Edit Mode") bool IsClaudeAssistActive() const { return bAssistMode; }
	// What a click does, for a given ray (also lets the harness "click").
	UFUNCTION(BlueprintCallable, Category = "Edit Mode") bool RecordAssistRay(const FVector& Start, const FVector& End);

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;

private:

	// Enhanced Input handlers (see SetupInputComponent for why not BindKey).
	void OnToggleEditInput(const struct FInputActionValue& Value);
	void OnClickInput(const struct FInputActionValue& Value);
	void OnFlyForward(const struct FInputActionValue& Value);
	void OnFlyRight(const struct FInputActionValue& Value);
	void OnFlyUp(const struct FInputActionValue& Value);
	void OnFlyDown(const struct FInputActionValue& Value);
	void OnFlyYaw(const struct FInputActionValue& Value);
	void OnFlyPitch(const struct FInputActionValue& Value);

	void ShowPage(EEditPage Page);
	void ApplyInputMode();
	void HandleManagerClick();
	// The character under the mouse (other than the possessed one), resolving
	// hits on attached props/cosmetics back to their character.
	ABaseCharacter* FindCharacterUnderCursor() const;
	// Moves the hover highlight to the character now under the cursor.
	void UpdateHoverHighlight(ABaseCharacter* Hovered);
	// Name tags: shown on every character (except the controlled one) while
	// edit mode is up, hidden otherwise.
	void RefreshNameLabels();
	TWeakObjectPtr<ABaseCharacter> HoveredCharacter;
	TWeakObjectPtr<ABaseCharacter> SelectedCharacter;
	uint64 LastClickFrame = 0;   // see HandleManagerClick
	// Slate-level left-click catcher for edit mode -- see FEditClickProcessor
	// in the .cpp for why the viewport never hands us LMB itself.
	TSharedPtr<class IInputProcessor> ClickProcessor;

	// Inspectables (see the Inspect properties above).
	void UpdateInspectTarget();
	AActor* ResolveInspectable(AActor* Hit, const UPrimitiveComponent* HitComponent = nullptr) const;
	void SetInspectHighlight(AActor* Target, bool bOn);
	void EnsureInspectOutlineVolume();
	TWeakObjectPtr<AActor> InspectTarget;
	UPROPERTY() TObjectPtr<class APostProcessVolume> InspectOutlineVolume;
	bool FindDropPointUnderCursor(FVector& OutFloorPoint, bool& bOutHit) const;
	ABaseCharacter* SpawnManagedCharacter(const FVector& FloorPoint);
	void SetSelectionHighlight(ABaseCharacter* Target, bool bVisible);

	UPROPERTY() TObjectPtr<UCharacterBuilderWidget> ManagerWidget;
	UPROPERTY() TObjectPtr<UCharacterBuilderWidget> FaceManagerWidget;
	UPROPERTY() TObjectPtr<UCharacterBuilderWidget> AnimBrowserWidget;
	UPROPERTY() TObjectPtr<UEditToolWidget> EditToolWidget;
	UPROPERTY() TObjectPtr<class UPauseMenuWidget> PauseMenuWidget;
	UPROPERTY() TObjectPtr<class UReferenceWidget> ReferenceWidget;
	UPROPERTY() TObjectPtr<class AStaticMeshActor> WeaponBoothActor;
	UPROPERTY() TObjectPtr<class ASceneCapture2D> WeaponCapture;
	UPROPERTY() TObjectPtr<class UTextureRenderTarget2D> WeaponTarget;
	UPROPERTY() TArray<TObjectPtr<class APointLight>> WeaponLights;
	UPROPERTY() TObjectPtr<class APointLight> WeaponFlash;
	void PlaceWeaponPreview();
	float WeaponYaw = 70.0f, WeaponPitch = 0.0f, WeaponExtent = 100.0f, WeaponKick = 0.0f;
	bool bReferenceOpen = false;
	UPROPERTY() TObjectPtr<class UConfirmDialogWidget> ConfirmDialog;
	bool bBypassSavePrompt = false;   // set while a prompt's choice is being carried out
	void ShowEditToolNow();
	void ExitEditModeNow();
	bool bPauseMenuOpen = false;
	void OnMenuKey();
	UPROPERTY() TObjectPtr<class UCharacterSheetWidget> CharacterSheetWidget;
	UPROPERTY() TObjectPtr<class URemoteViewWidget> RemoteViewWidget;
	UPROPERTY() TObjectPtr<class ASceneCapture2D> RemoteCapture;
	UPROPERTY() TObjectPtr<class UTextureRenderTarget2D> RemoteTarget;
	TWeakObjectPtr<ABaseCharacter> RemoteSubject;
	ABaseCharacter* FindCharacterByConfig(const FString& ConfigName) const;
	ABaseCharacter* SpawnBoothCharacter(const FString& ConfigName);
	void DressBoothAsMirror();   // black backdrop and studio lights for the chooser and the sheet
	void ShowSheetMirror();
	void HideSheetMirror();
	void PlaceSheetCamera();
	bool bSheetMirror = false; bool bSheetHead = false;
	float SheetOrbitYaw = 0.0f, SheetOrbitPitch = 0.0f, SheetBaseYaw = 0.0f;
	UPROPERTY() TObjectPtr<class UTextureRenderTarget2D> SheetTarget;   // portrait: the figure is taller than wide
	void DestroyBooth();
	UPROPERTY() TArray<TObjectPtr<AActor>> BoothActors;
	UPROPERTY() TArray<TObjectPtr<AActor>> BoothSet;   // the floor and wall, hideable for keyed shots
	UPROPERTY() TArray<TObjectPtr<class UNiagaraComponent>> BoothFX;
	TWeakObjectPtr<ABaseCharacter> BoothCharacter;
	bool bRemoteConversation = false;
	UPROPERTY() TObjectPtr<class UAppearanceWidget> AppearanceWidget;
	UPROPERTY() TObjectPtr<class UMetricsWidget> MetricsWidget;
	bool bAppearanceOpen = false;
	bool bFreshPlayerName = false;   // a new game: the chooser starts as "Repli Can" whatever was saved
	bool bGroggy = false;
	bool bGroggyArrived = false;   // reached the goal: the cabin call has been placed
	bool bTransferOpen = false;
	bool bInspectEnabled = false;
	UPROPERTY() TObjectPtr<class UInventoryTransferWidget> TransferWidget;
	FTimerHandle BlinkFadeTimer;
	FVector GroggyGoal = FVector::ZeroVector;
	float GroggyRadius = 150.0f;
	float GroggyPitchMin = -89.0f, GroggyPitchMax = 89.0f;
	bool bAppearanceInputMode = false;
	bool bPlayerConfigApplied = false;
	struct FAppearanceState { int32 Body = 1; int32 Head = 0; int32 Skin = 2; int32 Hair = -1; int32 Beard = -1; int32 Stubble = 0; int32 HeadStubble = 0; int32 Height = 4; int32 Build = 2; int32 View = 1; int32 Nose = 0; int32 Brow = 0; int32 HairColor = 0; };
	// Base brow position (head-bone space, right side); presets add raise/spread to it.
	FVector BrowBase = FVector(12.0f, 12.3f, 4.3f);
	FString AppearanceFirst, AppearanceLast;
	FAppearanceState Appearance;
	float AppearanceOrbitYaw = 0.0f;
	float AppearanceBaseYaw = 0.0f;   // the preview's spawn yaw; the drag turns them on the spot
	float AppearanceOrbitPitch = 0.0f;
	void ApplyAppearance();
	ABaseCharacter* AppearancePreview() const { return BoothCharacter.Get(); }
	bool bCharacterSheetOpen = false;
	// Where the player was when F12 opened edit mode: the pawn to re-possess
	// and the view direction to restore when edit mode closes.
	TWeakObjectPtr<APawn> PreEditPawn;
	FRotator PreEditControlRotation = FRotator::ZeroRotator;

	// Inspect context menu state (see the Inspect section above).
	UPROPERTY() TObjectPtr<class UInspectMenuWidget> InspectMenuWidget;
	UPROPERTY() TObjectPtr<class UCalloutWidget> CalloutWidget;
	TArray<FString> InspectActions;
	int32 InspectSelection = 0;
	// How long the current inspect target survives the reticle slipping off it (see UpdateInspectTarget).
	UPROPERTY(EditAnywhere, Category = "Inspect") float InspectGraceSeconds = 0.35f;
	float InspectLastSeen = -1.0f;
	FVector InspectAnchorWorld = FVector::ZeroVector;   // where the reticle trace hit the target (the leader's end)
	bool bAnchorValid = false;
	int32 InspectMenuMode = 0;
	TWeakObjectPtr<AActor> CalloutAnchor;
	float CalloutEndTime = 0.0f;
	float CalloutFadeSeconds = 0.6f;
	void RefreshInspectMenu(AActor* Target);
	void HideInspectMenu();
	// Keeps the menu pinned beside the inspectable's world position (not the
	// reticle), so it stays put while the reticle wanders over the thing.
	void UpdateInspectMenuPosition();
	void UpdateCallout();
	void OnInspectWheel(float Value);
	void OnInspectUse();
	void ExecuteInspectAction(AActor* Target, const FString& Action);
	bool ToggleTaggedLights(AActor* Target, const FString& Action);
	// What the menu shows for an actor: friendly name, description, actions.
	static void DescribeInspectable(AActor* Target, FString& OutName, FString& OutDescription, TArray<FString>& OutActions);

	// Conversation state (see the Conversation section above).
	UPROPERTY() TObjectPtr<class UConversationWidget> ConversationWidget;
	TWeakObjectPtr<ABaseCharacter> ConversationPartner;
	FConversation Conversation;
	FString ConversationNodeId;
	TArray<int32> ConversationChoiceIndices;   // node choice index per shown option
	TArray<FString> ConversationChoiceTexts;
	TArray<FString> ConversationHistory;
	void EnterConversationNode(const FString& NodeId);
	// The baked voice for the line being spoken, attached to the NPC's head;
	// stopped when the next line starts, the talk ends, or its length is up
	// (a procedural wave never finishes on its own). Returns its seconds.
	float PlayVoiceLine(ABaseCharacter* With, const FString& NodeId, int32 LineIndex);
	void StopConversationVoice();
	UPROPERTY() TObjectPtr<class USequenceDirector> Sequence;
	UPROPERTY() TObjectPtr<class UBlinkOverlayWidget> BlinkOverlay;
	// Background sound bed started on arrival (AmbientPlayer.h); empty = none.
	UPROPERTY(EditAnywhere, Category = "Audio") FString AmbientProfile = TEXT("facility");
	UPROPERTY() TObjectPtr<class UAmbientPlayer> Ambient;
public:
	UFUNCTION(BlueprintPure, Category = "Audio") FString GetAmbientState() const;
	UFUNCTION(BlueprintCallable, Category = "Audio") void FadeAmbient(float Level, float Seconds);
private:
	bool bCinematic = false;
	bool bHudWasShown = true;
	UPROPERTY() TObjectPtr<class UAudioComponent> ConversationVoice;
	UPROPERTY() TObjectPtr<class USoundAttenuation> VoiceAttenuation;
	FTimerHandle ConversationVoiceTimer;
	void OnConversationDigit(FKey Key);
	// Edit-mode viewpoint on the selected character: a clear full-body look
	// when a character is picked, a portrait for the Face Manager, and None
	// to hand the view back to the pawn's own camera.
	enum class EViewFrame : uint8 { None, Body, Face, Conversation };
	UPROPERTY() TObjectPtr<class ACameraActor> FaceCamera;
	void UpdateViewCamera(EViewFrame Frame);
	UPROPERTY() TObjectPtr<ASpectatorPawn> FlyPawn;
	UPROPERTY() TObjectPtr<ACharacterGhostActor> GhostActor;

	void RecordAssistHit(const FHitResult& Hit, const FVector& RayStart);
	FString GetAssistDirectory() const;

	bool bEditMode = false;
	bool bAssistMode = false;
	bool bLogEditToolGeometry = false;
	int32 AssistClickCount = 0;
	EEditPage CurrentPage = EEditPage::None;
	bool bFlying = false;
	bool bPlacingCharacter = false;
	bool bDropPointValid = false;
	bool bDropPointHit = false;
	FVector DropPoint = FVector::ZeroVector;
	int32 SpawnedCharacterCount = 0;
};
