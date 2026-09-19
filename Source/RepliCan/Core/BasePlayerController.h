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
#include "Narrative/ConversationData.h"
#include "Weapons/WeaponCatalog.h"
#include "Items/ItemInstance.h"
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
	UFUNCTION(BlueprintCallable, Category = "Inspect") void InspectSelectIndex(int32 Index);   // the digit keys: row N
	int32 InspectActionCount() const { return InspectActions.Num(); }
	bool IsInspectMenuFlavour() const { return bInspectMenuFlavour; }   // a name tag with no rows: the keys are not its
	UFUNCTION(BlueprintCallable, Category = "Inspect") void InspectUseSelected();
	// Floating text over an actor for Seconds (a character's line, or an
	// action's note). Speech is shown in quotes/italics.
	UFUNCTION(BlueprintCallable, Category = "Inspect") void ShowCallout(AActor* Anchor, const FString& Text, float Seconds = 3.5f, bool bSpeech = false);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FInspectActionEvent, AActor*, Target, const FString&, Action);
	UPROPERTY(BlueprintAssignable, Category = "Inspect") FInspectActionEvent OnInspectAction;
	UPROPERTY(BlueprintReadOnly, Category = "Inspect") TArray<FString> Inventory;   // what "Take" collected (friendly names)
	// Equipped gear, one string per ItemCatalog::GearSlots entry (empty = nothing there).
	UPROPERTY(BlueprintReadOnly, Category = "Inspect") TArray<FString> Equipped;

	// ---- ITEM INSTANCES ----------------------------------------------------------------------
	// See ItemInstance.h. Inventory and Equipped still hold strings; an instanced item stores a
	// handle that names a row in this registry. Nothing else changed shape, which is why this did
	// not have to touch the 55 places that look an item up.
	// BlueprintReadOnly so the registry can be READ from outside -- a plain UPROPERTY is reflected
	// for saving but invisible to script, which made it impossible to check what instances exist.
	UPROPERTY(BlueprintReadOnly, Category = "Inspect") TArray<FItemInstance> ItemInstances;
	UPROPERTY(BlueprintReadOnly, Category = "Inspect") int32 NextItemInstanceId = 1;

	/** Does a copy of this deserve its own identity? Things you modify do; ammunition does not. */
	static bool WantsInstance(const FString& Name);
	/** Registers a fresh copy and returns the handle to put in Inventory. */
	FString NewItemInstance(const FString& Name);
	FItemInstance* FindItemInstance(const FString& Handle);
	const FItemInstance* FindItemInstance(const FString& Handle) const;
	/** This copy value for Key, or Fallback when it has no opinion -- which is what makes an
	 *  unmodified item behave exactly as it did before instances existed. */
	FString ItemProp(const FString& Handle, const FString& Key, const FString& Fallback = FString()) const;
	/** The optic actually fitted to this copy: its own "optic" prop when it has one, the type's
	 *  default otherwise -- EXCEPT on a weapon whose sight is built in, where the catalogue always
	 *  wins. Use this rather than reading the prop directly, or a stray instance prop bolts a scope
	 *  onto a weapon that has nowhere to put one. */
	FString FittedOptic(const FString& Handle, const WeaponCatalog::FWeapon* W) const;
	void SetItemProp(const FString& Handle, const FString& Key, const FString& Value);
	/** Forgets a copy, so the registry does not grow forever. */
	void ReleaseItemInstance(const FString& Handle);
	/** What is fitted to this copy, ready to print. */
	FString AccessorySummary(const FString& Handle) const;
	UFUNCTION(BlueprintCallable, Category = "Inspect") bool EquipFromInventory(int32 InventoryIndex);
	// The bag keeps its squares: an item taken out leaves an empty square behind, a new one takes
	// the first empty square. These are what the sheet's drag and drop calls.
	// bPickup: it came from the world (a Take, a loot box, a GIVE), so a weapon goes straight to an
	// empty weapon slot, and into the hand if the hands were empty.
	UFUNCTION(BlueprintCallable, Category = "Inspect") bool AddToInventory(const FString& Name, bool bPickup = false);
	void AutoEquipPickup(const FString& Name);
	// DROPPING. A square's item goes out into the world in front of the character, as if let go:
	// the actor Take hid if there is one (its tags and paint intact), else a fresh prop from the
	// catalogue's mesh, tagged so it can be taken again. Tossed if its mesh can simulate, set on
	// the floor if it cannot.
	UFUNCTION(BlueprintCallable, Category = "Inspect") bool DropInventory(int32 InventoryIndex);
	UFUNCTION(BlueprintCallable, Category = "Inspect") bool DropGear(int32 Slot);
	// Gone, not dropped: no mesh left on the floor to pick up again. Drop is for putting something
	// down; this is for getting rid of it. Anything with no mesh can only leave this way.
	UFUNCTION(BlueprintCallable, Category = "Inspect") bool DeleteInventory(int32 InventoryIndex);
	UFUNCTION(BlueprintCallable, Category = "Inspect") bool DeleteGear(int32 Slot);
	AActor* DropToWorld(const FString& Name);
	UFUNCTION(BlueprintPure, Category = "Inspect") int32 InventoryFree() const;
	UFUNCTION(BlueprintCallable, Category = "Inspect") bool MoveInventory(int32 From, int32 To);                       // swap two bag squares
	UFUNCTION(BlueprintCallable, Category = "Inspect") bool EquipFromInventoryToSlot(int32 InventoryIndex, int32 Slot);  // into a named slot; what was there takes the bag square
	UFUNCTION(BlueprintCallable, Category = "Inspect") bool UnequipToInventory(int32 Slot, int32 InventoryIndex);       // into a bag square; a filled square swaps if its item fits the slot
	UFUNCTION(BlueprintCallable, Category = "Inspect") bool SwapGear(int32 A, int32 B);                                 // two gear slots, each item fitting the other's slot
	UFUNCTION(BlueprintPure, Category = "Inspect") bool KindFitsSlot(const FString& Item, int32 Slot) const;
	// CLOTHING. An item with wear_* fields (ItemFields) puts cut-library parts on the wearer while it
	// sits in a body slot; refreshed with the held weapon, since the slots are the only truth.
	bool ClothingFits(const FString& Item, FString& OutWhy) const;
	void RefreshWornClothing();
	TMap<FString, FString> WornBase;   // the body's own parts, remembered the first time clothing covers them
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
	// The HELD weapon's own turn on top of that, saved to its catalogue entry as hand_rot: HandRotWeapon pitch yaw roll. Alone prints it.
	UFUNCTION(Exec) void HandRotWeapon(float Pitch = 1000.0f, float Yaw = 0.0f, float Roll = 0.0f);
	// The hold's numbers on screen (reach, carry pull-in, hand gap, sight off the eye line): toggles.
	UFUNCTION(Exec) void HandDiag();
	// Writes the held weapon's whole state to Saved/ClaudeAssist/hand_dump.json, once, now.
	UFUNCTION(Exec) void HandDump();
	// Pins the carry so a hold can be MEASURED in a state the mouse would otherwise have to
	// hold down: 0 low ready, 1 hip fire, 2 shouldered, 3 ADS, -1 to hand it back to the input.
	UFUNCTION(Exec) void ForceCarry(int32 Carry);
	// FIRST PERSON VIEW TUNING, live. These are the numbers that decide how the weapon follows the
	// eye, and they are the sort you only get right by moving around while you change them -- so
	// they are set from the console and take effect on the next frame. `ViewTune` with no arguments
	// lists them with their current values.
	UFUNCTION(Exec) void ViewTune(const FString& Field, float Value);
	// The SUPPORT hand's wrap on the fore grip, live: HandRotL pitch yaw roll (weapon space; alone prints it), and
	// the held weapon's own turn on top of that, saved as fore_hand_rot: HandRotLWeapon pitch yaw roll.
	UFUNCTION(Exec) void HandRotL(float Pitch = 1000.0f, float Yaw = 0.0f, float Roll = 0.0f);
	UFUNCTION(Exec) void HandRotLWeapon(float Pitch = 1000.0f, float Yaw = 0.0f, float Roll = 0.0f);
	// Writes [pitch, yaw, roll] into one field of the held weapon's catalogue entry and reloads the catalogue.
	bool SaveHeldWeaponRotField(const TCHAR* Field, const FRotator& R);
	// Turns and walks for N seconds measuring the weapon's per-frame jump in camera space (the
	// judder) and the predicted-vs-final camera error; WeaponLagReport reads the result back.
	UFUNCTION(Exec, BlueprintCallable, Category = "Debug") void WeaponLagTest(float Seconds = 4.0f);
	UFUNCTION(BlueprintCallable, Category = "Debug") FString WeaponLagReport() const;
	UFUNCTION(BlueprintCallable, Category = "Inspect") bool UnequipSlot(int32 Slot);
	// The character sheet's mirror: the player's saved likeness in the booth, orbited by dragging.
	UFUNCTION(BlueprintCallable, Category = "Sheet") void OrbitSheetMirror(float DeltaYaw, float DeltaPitch);
	// The sheet's booth with any character file in it (the narrative page's viewer); the sheet's own is "Player".
	UFUNCTION(BlueprintCallable, Category = "Sheet") bool ShowSheetMirrorFor(const FString& ConfigName);
	UFUNCTION(BlueprintCallable, Category = "Sheet") void HideCharacterPreview();
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
	// SCAN: held, every interactable in view with a real action shows its outline, for finding
	// things without the world lighting up on its own.
	UFUNCTION(BlueprintCallable, Category = "Inspect") void SetScanHeld(bool bHeld);
	UFUNCTION(BlueprintPure, Category = "Inspect") bool IsScanHeld() const { return bScanHeld; }
	// Flavour: an interactable with nothing but Inspect to offer. No outline; a quiet name tag after a dwell.
	bool IsFlavourInteractable(AActor* Target);
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
	// TERMINALS. Use on a monitor prop whose mesh UI/Screens.json knows: the character sits at the
	// seat beside it (or squares up to it), the camera goes to a spot looking straight at the
	// screen, and the console page is laid over the screen's projection so the text reads as the
	// monitor's own. Esc, or "exit", puts it all back.
	UFUNCTION(BlueprintCallable, Category = "Terminal") void OpenTerminal(AActor* Target);
	UFUNCTION(BlueprintCallable, Category = "Terminal") void CloseTerminal();
	UFUNCTION(BlueprintPure, Category = "Terminal") bool IsTerminalOpen() const { return bTerminalOpen; }
	bool IsTerminal(const AActor* Target) const;
	bool ScreenQuadFor(const AActor* Target, FVector& OutCentre, FVector& OutNormal, FVector& OutRight, FVector& OutUp, float& OutW, float& OutH) const;
	void PlaceTerminalCamera();   // from the seated eye, once the sit has settled
	class UWidgetInteractionComponent* GetTerminalPointer() const { return TerminalPointer; }
	bool TerminalKeysDirect() const { return bTerminalKeysDirect; }   // the real keyboard is focused on the prompt: the processor lets keys through
	// The screen's own polygons out of the monitor mesh (world space), with the frame they lie in.
	bool ScreenShapeFor(const AActor* Target, TArray<FVector>& OutVerts, TArray<int32>& OutTris, FVector& OutCentre, FVector& OutNormal, FVector& OutRight, FVector& OutUp, float& OutW, float& OutH) const;
	void BindTerminalGlass();      // once the screen has drawn: its picture onto the glass mesh
	void ClickTerminalPrompt();    // the pointer's user clicks the prompt (focus for carried keys)
	void RefocusTerminalPrompt();  // after a link click: the prompt takes the keyboard again
	void SitOnTagged(class ABaseCharacter* Me, AActor* Seat);
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
	// Names every widget alive in this world and says which are in the viewport and how they
	// hit-test -- for finding whatever is tinting the screen or eating the mouse.
	UFUNCTION(Exec, BlueprintCallable, Category = "Debug") void UIDump();
	// A note that clears itself after Seconds unless replaced.
	UFUNCTION(BlueprintCallable, Category = "Debug") void SetDiagNoteTimed(const FString& Text, float Seconds);
	// T: what is under the reticle: actor, mesh, material and its textures, noted for ten seconds.
	UFUNCTION(Exec, BlueprintCallable, Category = "Debug") void NoteTextureUnderReticle();
	// Alt+T: swap whatever is under the reticle to the NEXT of Synty's lettered palette
	// variants, so a material can be judged in the room rather than in the asset browser.
	UFUNCTION(Exec, BlueprintCallable, Category = "Debug") void CycleMaterialUnderReticle();

	// ---- Held weapon -----------------------------------------------------
	// Whichever enabled weapon slot is filled goes into the hand, in slot order. The slots carry no
	// roles -- any weapon fits any enabled one. Called whenever the equipment changes.
	UFUNCTION(BlueprintCallable, Category = "Weapon") void RefreshHeldWeapon();
	// Steps the named weapon to its next texture variant (WeaponSkins), writes it to the catalogue and re-dresses the one in hand.
	UFUNCTION(BlueprintCallable, Category = "Weapon") void CycleWeaponSkin(const FString& ItemName);
	// The paint the preview booth wears instead of the catalogue's. Empty means the catalogue's own,
	// which is what everything outside the Reference page always sees.
	void SetWeaponPreviewSkin(const FString& WeaponName, const FString& Variant);
	const FString& WeaponPreviewSkin() const { return PreviewSkinOverride; }
	// Swaps which of the two filled slots is being carried.
	UFUNCTION(Exec, BlueprintCallable, Category = "Weapon") void SwapWeaponSlot(int32 Direction = 1);
	// Draws the weapon in the Nth weapon slot (1-based, the sheet's "Slot 1", "Slot 2" ...); the keys 1 and 2 for now.
	UFUNCTION(Exec, BlueprintCallable, Category = "Weapon") void EquipWeaponSlot(int32 Ordinal);
	// A change of weapon takes a moment: a short beat, then the new one appears in the hand as its
	// stance's draw clip (MM_<Stance>_Equip) plays, hidden for the clip's first third so it comes
	// up out of nowhere rather than snapping into a hand that is still reaching.
	void BeginWeaponSwap(int32 NewSlot);
	void TickPendingSwap(float DeltaSeconds);
	int32 PendingSwapSlot = -1;
	float PendingSwapLeft = 0.0f;
	float PendingShowLeft = 0.0f;
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
	// THERE IS NO PRIMARY AND NO SIDEARM. There are four weapon slots, two of them enabled for now
	// (FSheetSpec: Slot 3 and Slot 4 are bEnabled = false), and ANY weapon fits ANY enabled slot --
	// the sheet gives the enabled ones identical kind lists. So this is an ordered list, not a pair
	// of roles: each entry goes into the next enabled slot that will take it. Naming one of them
	// the sidearm implied a role the game does not have, and implied a second slot that only
	// accepted small arms, which was never true.
	//
	// SET FOR TESTING THE THREE OPTIC KINDS (2026-09-19), not a considered loadout -- the SMG wears
	// the only ZOOMED sight in the catalogue (1.5x, no overlay, weapon stays in view) and the pistol
	// a RED DOT, so one spawn covers both. Previously "Frontier Assault Rifle 01" and "Street
	// Pistol 01"; put those back when the testing is done.
	UPROPERTY(EditAnywhere, Category = "Weapon")
	TArray<FString> StartingWeapons = { TEXT("Frontier SMG 03"), TEXT("Frontier Pistol 05") };
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
	// The save / load page (USaveLoadWidget): slots by name, over the pause menu.
	// HAND TUNING (UHandTuneWidget): one catalogue weapon put on the pawn, two orthographic captures
	// of the pawn holding it as the game holds it, the hand numbers edited live and written back.
	UFUNCTION(BlueprintCallable, Category = "Reference") void ShowHandTune(const FString& WeaponName);
	UFUNCTION(BlueprintCallable, Category = "Reference") void HideHandTune();
	UFUNCTION(BlueprintPure, Category = "Reference") bool IsHandTuneOpen() const { return bHandTuneOpen; }
	void HandTuneSetCarry(int32 Idx); void HandTuneSetAim(int32 Idx); void HandTuneReset(); bool HandTuneSave();
	// The optic fitted to the weapon being tuned, and where it sits on the rail. The offset belongs
	// to the OPTIC, so it is saved into the optics block and every gun that takes that optic moves
	// with it -- which is the point: you are placing the sight on its mount, once.
	FString HT_OpticKey;
	FVector HT_OpticOff = FVector::ZeroVector, HT_OpticOff0 = FVector::ZeroVector;
	// The paint being TRIED on the optic. Like the weapon own skin cycler this is a fitting, not a
	// decision: it lives on the page until SAVE writes it to the optic entry, so flicking through
	// the colours does not repaint every sight in the level one after another.
	FString HT_OpticSkin, HT_OpticSkin0;
	// The optic being TRIED. Stepping it used to write the catalogue on the spot, so every copy of
	// that weapon in the world changed sight the instant you looked at the next one -- the same
	// fault the paint cycler had, and the same rule broken: the world changes on SAVE and at no
	// other moment. Now the page holds the choice and only SAVE puts it in the file.
	FString HT_Optic, HT_Optic0;
	// HOW BIG THE WEAPON IS DRAWN, as a PERCENTAGE. Held as a percent so the table's existing half
	// step is half a percent -- a useful nudge -- where half a UNIT of scale would be absurd.
	float HT_ScalePct = 100.0f, HT_ScalePct0 = 100.0f;
	void HandTuneFitOptic();   // dresses the stand-in with the tried optic, offset and paint
	void HandTuneStepOpticSkin(int32 Dir);
	FString HandTuneOpticSkinLabel() const;
	// The tuning table's rows: 0 grip (cm), 1 hand_rot (deg), 2 fingers_r (deg: thumb, index, middle,
	// ring, pinky), 3 fore_grip, 4 fore_hand_rot, 5 fingers_l, 6 hunch (cm, at the sights), 7 pull
	// (cm per carry), 8 lean (deg, at the sights), 9 lateral (cm per carry), 10 eyeline (side, forward
	// -- the character's, not the weapon's). A change shows on the stand-in at once.
	float HandTuneValue(int32 Row, int32 Col) const;
	// A picture can be dragged round while a pointer holds it, and snaps back the moment it lets
	// go: a look from another angle, not a camera to get lost in. The drag ORBITS ABOUT THE WEAPON,
	// so the thing being tuned stays put in the middle of the frame and only the view moves.
	void HandTuneOrbit(float DYaw, float DPitch);
	// ONE PICTURE, SEEN FROM A CHOSEN SIDE. Two fixed views cost half the panel and still could not
	// show the one angle a hold needed; a single picture with the sides on call shows more of them.
	// 0 left, 1 right, 2 top, 3 front, 4 three-quarter.
	void HandTuneSetPresetView(int32 Index);
	int32 HandTunePresetView() const { return HT_ViewIdx; }
	// The paint, on the weapon being tuned: stepped and shown without leaving the page.
	// Anything edited and not yet saved. Compared field by field against the values the page opened
	// with, which are also what SAVE writes and RESET restores.
	bool HandTuneDirty() const;
	// Asks before a dirty page changes weapon: save, discard, or stay. Steps by Dir on the first two.
	void ConfirmHandTuneLeave(int32 Dir);
	// The paint, stepped either way. A fitting on the stand-in until SET DEFAULT adopts it.
	void HandTuneStepSkin(int32 Dir);
	void HandTuneCycleSkin() { HandTuneStepSkin(1); }
	// The optic. Unlike the paint this is EQUIPMENT, not a colourway: fitting one moves the sight
	// point to the optic's own eye and changes the whole hold, so it is written straight to the
	// catalogue and pushed out, the way fitting a scope to a real rifle is not a preview.
	void HandTuneStepOptic(int32 Dir);
	FString HandTuneOpticLabel() const;
	bool HandTuneTakesOptic() const;
	/** The weapon's sight is built in and will not be traded. DISTINCT from HandTuneTakesOptic,
	 *  which means there is nowhere to mount one at all: these are different states and must not
	 *  look alike to the player. See the comment on the definition. */
	bool HandTuneOpticFixed() const;
	// Writes the previewed paint to the catalogue as this weapon's default: what it wears wherever
	// it appears in the world, and what the preview booth renders for its inventory icon. Until
	// this is pressed the cycler is only a fitting, on the stand-in and nowhere else.
	void HandTuneSetDefaultSkin();
	bool HandTuneSkinIsDefault() const;
	// The header's arrows: the previous or next tunable weapon, loaded onto the stand-in without
	// leaving the page. Dir is -1 or +1.
	void HandTuneStepWeapon(int32 Dir);
	// Pushes a weapon's freshly saved numbers onto EVERY character in the world holding one, the
	// player included. Returns how many were re-dressed.
	int32 RefreshTunedWeapon(const FString& WeaponName);
	// What the header shows between them.
	FString HandTuneWeaponLabel() const;
	FString HandTuneSkinLabel() const;
	// A step in or out on one picture. Unlike the drag, a zoom is a decision and stays put until
	// the page is opened again.
	void HandTuneZoomStep(int32 Direction);
	// Named hand shapes from the catalogue, applied to one hand.
	int32 HandTunePresetCount() const;
	FString HandTunePresetName(int32 Index) const;
	void HandTuneApplyPreset(bool bSupport, int32 Index);
	void HandTuneResetView();
	void HandTuneAdjust(int32 Row, int32 Col, float Delta);
	int32 HandTuneCarry() const { return HandTuneCarryIdx; }
	int32 HandTuneAim() const { return HandTuneAimIdx; }
	// Writes a list of numbers into one field of a weapon's catalogue entry and reloads the catalogue.
	bool SaveWeaponField(const FString& Key, const TCHAR* Field, const TArray<double>& Values, bool bScalar = false);   // bScalar: one number, written as a number, not a list
	UFUNCTION(BlueprintCallable, Category = "Menu") void ShowSaveLoad(bool bLoad);
	UFUNCTION(BlueprintCallable, Category = "Menu") void HideSaveLoad();
	UFUNCTION(BlueprintPure, Category = "Menu") bool IsSaveLoadOpen() const { return bSaveLoadOpen; }
	void SaveToSlot(const FString& Slot);
	void LoadFromSlot(const FString& Slot);
	UFUNCTION(BlueprintPure, Category = "Menu") bool IsPauseMenuOpen() const { return bPauseMenuOpen; }

	// Is ANY screen up. Firing a weapon because a click landed on a menu button is the kind of
	// bug that only shows up as "I shot my own foot in the inventory", so the trigger asks this
	// one question rather than each caller remembering the current list of panels.
	UFUNCTION(BlueprintPure, Category = "Menu") bool IsAnyScreenOpen() const;
	// What stops a shot: a real page, a cinematic, a conversation, the booth, the remote view. NOT the
	// reticle menu -- a person or a crate under the reticle used to make the trigger dead (2026-09-17).
	bool IsFiringBlocked() const;
	// A page covering the world (a menu, the Reference, the sheet, a transfer, a cinematic, a conversation): the aim mark has nothing to mark. The inspect menu beside the reticle is not one.
	UFUNCTION(BlueprintPure, Category = "Menu") bool IsPageOpen() const;
	void DismissInspectMenu() { HideInspectMenu(); }   // for the input processor: aiming closes the reticle menu

	// Clears free look if the key-up was lost (alt-tab) or a screen opened while it was held.
	void TickFreelookSafety();

	// A shot commanded from low ready waits for the weapon to come up. Seconds remaining.
	void TickPendingFire(float DeltaSeconds);
	float PendingFireLeft = 0.0f;
	// THE SELECTOR. Middle mouse cycles the weapon's fire_modes; the trigger is HELD from the
	// press to the release, and in auto the held trigger fires again every 1/fire_rate seconds.
	UFUNCTION(Exec, BlueprintCallable, Category = "Weapon") void CycleFireMode();
	UFUNCTION(Exec, Category = "Weapon") void AimSens(float Scale);   // the ADS mouse scale, 0.05..1
	UFUNCTION(Exec, Category = "Weapon") void AimSway(float Scale);   // the aim sway scale: 0 none, 1 as tuned
	// VITALITY. The worn armour at a body slot ("Head", "Chest", "Arms", "Hands", "Legs", "Feet"):
	// the catalogue armor_value of what sits in that gear slot. And the test commands: Sever takes
	// a region off whoever is under the reticle (Head ArmL ArmR LegL LegR), Kill drops them,
	// Revive puts the player's own body back together.
	float ArmourValueFor(const FString& BodySlot) const;
	UFUNCTION(Exec, Category = "Vitality") void Sever(const FString& Region);
	UFUNCTION(Exec, Category = "Vitality") void Kill();
	UFUNCTION(Exec, Category = "Vitality") void Revive();
	AActor* PersonUnderReticle(FVector& OutDir) const;
	UFUNCTION(Exec, Category = "Weapon") void ZeroRange(float Cm);     // 0: bore parallel to the sight line; else converge at that range
	UFUNCTION(Exec, Category = "Weapon") void RecoilTune(float RecoverSeconds, float RecoverFraction);   // how long the kick takes to come back, and how much of it does
	UFUNCTION(Exec, Category = "Weapon") void SwayRate(float StridesPerSecond);   // the run sway's stride rate
	void SetTriggerHeld(bool bHeld);
	void TickAutoFire(float DeltaSeconds);
	WeaponCatalog::EFireMode CurrentFireMode() const;
	WeaponCatalog::EFireMode FireMode = WeaponCatalog::EFireMode::Semi;   // what the selector is on; a weapon without that position falls to its first
	int32 BurstLeft = 0;           // rounds still owed by the last pull in burst
	// LASER: a beam from the muzzle down the aim for as long as the trigger is held. Its damage
	// is done over time -- sparks and scorching where it rests, a flinch from a person under it --
	// rather than per shot.
	void TickLaser(float DeltaSeconds);
	// MELEE. Fire on a blade or a hammer is a swing: a clip from the weapon's attack set on the
	// arm override, a hit window part-way through it during which the edge (grip to tip) is swept
	// through the world each tick, and every actor it crosses takes the weapon's damage once. A
	// press inside the recovery window chains the next combo step (A, B, C); mid-swing it is kept.
	void MeleeSwing();
	void StartMeleeStep(class ABaseCharacter* Me, const WeaponCatalog::FWeapon* W);
	void TickMelee(float DeltaSeconds);
	int32 MeleeComboStep = 0;
	float MeleeComboUntil = 0.0f;
	float MeleeSwingClock = -1.0f, MeleeSwingLength = 0.0f;
	bool bMeleeQueued = false;
	TSet<TWeakObjectPtr<AActor>> MeleeHitThisSwing;
	TSet<TWeakObjectPtr<AActor>> MeleeCleaved;   // took a limb off: the edge may meet them once more (arm, then body)
	// THE BUDGET. A swing carries the weapon's damage; a kill or a limb destroyed costs what it
	// took and the rest carries on to the next thing in the arc; anything less, a wall, or a blunt
	// weapon spends it all and the swing STOPS: a beat of frozen time and the arm eases back.
	float MeleeBudget = 0.0f;
	void MeleeStop(class ABaseCharacter* Me, bool bHeavy);
	FTimerHandle MeleeStopTimer;
	TArray<FVector> MeleeLastEdge;
	void StopLaser();
	UPROPERTY() TObjectPtr<class UStaticMeshComponent> LaserBeam;
	UPROPERTY() TObjectPtr<class UStaticMeshComponent> LaserGlow;    // the haze round the core
	UPROPERTY() TObjectPtr<class UPointLightComponent> LaserLight;   // the red on whatever the spot is on
	bool bLaserOn = false;
	TArray<TWeakObjectPtr<class UAudioComponent>> LaserBuzzComps;   // the buzz clips in flight, stopped dead the frame the trigger comes up
	float LaserFxClock = 0.0f, LaserReactClock = 0.0f;
	// THE BATTERY: seconds of beam left per weapon key (a laser's magazine is seconds); a reload
	// puts a fresh cell in. The buzz is a short loop re-lit before it ends.
	TMap<FString, float> LaserCharge;
	// ROUNDS IN THE MAGAZINE, keyed by the equipped HANDLE -- so once weapons carry instances, two
	// rifles in the same bag each keep their own count, and a bare name keeps one, which is all an
	// item with no identity can have. Absent means "full": a weapon is not asked to remember a
	// magazine it has never fired.
	TMap<FString, int32> MagRounds;
	int32 RoundsLeft(const FString& Handle) const;
	/** Range under the reticle in metres, rounds and magazine size. True only while a SMART sight
	 *  is actually up -- the figures belong on the glass, not on the screen. */
	bool SmartOpticInfo(float& OutRangeM, int32& OutRounds, int32& OutMag) const;
	/** The sight being looked through: its reticle kind, colour, magnification, and how far the
	 *  weapon has actually come up (0 off the sights, 1 fully at them) so the mark can fade in with
	 *  the weapon instead of appearing the instant the button goes down. False when none is up. */
	bool OpticReticleInfo(FString& OutKind, FLinearColor& OutColour, float& OutZoom, float& OutAlpha) const;
	/** The scope overlay: how wide the opening is (as a fraction of screen height), how far the mask
	 *  has come in, and whether the sight is jammed against something. False when no tube is up. */
	bool OpticOverlayInfo(float& OutRadius, float& OutAlpha, bool& bOutBlocked) const;
	float LaserBuzzClock = 0.0f;
	bool bLaserStroke = false; FVector LaserStrokeFrom = FVector::ZeroVector, LaserStrokeNormal = FVector::UpVector;   // the heated line: where its last stroke ended, on what
	// The burn on a body: dots pinned to the bone under the spot (MarkScorchOnBody), spaced by travel or time.
	bool bLaserBodyMark = false; FVector LaserBodyMarkFrom = FVector::ZeroVector; float LaserBodyMarkClock = 0.0f;
	void MarkScorchOnBody(const FHitResult& Hit);
	// THE SCORCH: the heated line the beam leaves on a surface, bright metal cooling to black.
	struct FScorch { TWeakObjectPtr<class UDecalComponent> Decal; TWeakObjectPtr<class UMaterialInstanceDynamic> MID; float Age = 0.0f; };
	TArray<FScorch> Scorches;
	void MarkScorchStroke(const FVector& From, const FVector& To, const FVector& Normal);   // one stroke of the line, From to To, laid on a surface facing Normal
	TArray<TWeakObjectPtr<class UDecalComponent>> ScorchRing;   // every scorch alive, oldest first, capped
	void TickScorches(float DeltaSeconds);
	// TRACERS: a streak of light down each shot's line, muzzle to mark, at a speed the eye can follow.
	struct FTracer { int32 Pool = -1; FVector From = FVector::ZeroVector, To = FVector::ZeroVector; float Head = 0.0f; };
	TArray<FTracer> Tracers;
	UPROPERTY() TArray<TObjectPtr<class UStaticMeshComponent>> TracerPool;
	void SpawnTracer(const FVector& From, const FVector& To);
	void TickTracers(float DeltaSeconds);
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
	UFUNCTION(BlueprintCallable, Category = "Menu") void ShowReferenceFor(const FString& ItemName);   // the Reference open at that item's card
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
	bool bTerminalOpen = false;
	bool bSaveLoadOpen = false;
	UPROPERTY() TObjectPtr<class USaveLoadWidget> SaveLoadWidget;
	UPROPERTY() TObjectPtr<class UHandTuneWidget> HandTuneWidget;
	UPROPERTY() TObjectPtr<class ASceneCapture2D> HandTuneCap;
	UPROPERTY() TObjectPtr<class UTextureRenderTarget2D> HandTuneRT;
	UPROPERTY() TArray<TObjectPtr<class APointLight>> HandTuneLights;
	// The stand-in: a copy of the player in a booth off the map, holding the weapon being tuned, with
	// its own controller for the aim. The player and the world are untouched until SAVE.
	UPROPERTY() TObjectPtr<class ABaseCharacter> HandTunePawn;
	UPROPERTY() TObjectPtr<class AHandTuneController> HandTuneController;
	// A rod from the stand-in's aiming eye along the aim: it shows which eye the sight comes to.
	UPROPERTY() TObjectPtr<class UStaticMeshComponent> HandTuneAimLine;
	void TickHandTune();   // the captures re-aimed at the trigger hand
	bool bHandTuneOpen = false, bHandTuneReturnToReference = false;
	TArray<float> HT_FingersR, HT_FingersR0, HT_FingersL, HT_FingersL0;
	float HT_Hunch = 0.0f, HT_Hunch0 = 0.0f, HT_Lean = 0.0f, HT_Lean0 = 0.0f;
	// Pull and lateral, one per carry: low ready, shouldered, sights.
	float HT_Pull3[3] = { 0.0f, 0.0f, 0.0f }, HT_Pull3_0[3] = { 0.0f, 0.0f, 0.0f };
	float HT_Lat3[3] = { 12.0f, 11.0f, 0.0f }, HT_Lat3_0[3] = { 12.0f, 11.0f, 0.0f };
	// The eyeline: the body's, not the weapon's, so SAVE writes it to the character file.
	float HT_EyeSide = 4.25f, HT_EyeSide0 = 4.25f, HT_EyeUp = 0.0f, HT_EyeUp0 = 0.0f, HT_EyeFwd = 0.0f, HT_EyeFwd0 = 0.0f;
	// Low ready's own angles off the aim, and each elbow's swing about its reach line.
	float HT_LowReady[2] = { -30.0f, -30.0f }, HT_LowReady0[2] = { -30.0f, -30.0f };
	float HT_ElbowMainAim[3] = { 0.0f, 0.0f, 0.0f }, HT_ElbowMainAim0[3] = { 0.0f, 0.0f, 0.0f };
	float HT_ElbowSupAim[3] = { 0.0f, 0.0f, 0.0f }, HT_ElbowSupAim0[3] = { 0.0f, 0.0f, 0.0f };
	float HT_ElbowMain[3] = { 0.0f, 0.0f, 0.0f }, HT_ElbowMain0[3] = { 0.0f, 0.0f, 0.0f };
	float HT_ElbowSup[3] = { 0.0f, 0.0f, 0.0f }, HT_ElbowSup0[3] = { 0.0f, 0.0f, 0.0f };
	FVector2D HT_Orbit = FVector2D::ZeroVector;   // yaw, pitch in degrees off the chosen view
	float HT_Zoom = 1.0f;                          // frame width as a fraction of the standard one
	int32 HT_ViewIdx = 1;                          // which side it is seen from; right by default
	int32 HandTuneCarryIdx = 1, HandTuneAimIdx = 1, HandTunePrevZoom = -1;
	FString HandTuneKey, HandTuneName, HT_Stance;
	FString HT_Skin;   // the paint being tried; empty until the cycler is touched
	// Loads one weapon's numbers into the page and onto the stand-in. Shared by opening the page
	// and by the header's arrows, so the two can never drift apart.
	void HandTuneLoadWeapon(const WeaponCatalog::FWeapon* W);
	FVector HT_Grip = FVector::ZeroVector, HT_Grip0 = FVector::ZeroVector, HT_Fore = FVector::ZeroVector, HT_Fore0 = FVector::ZeroVector;
	FRotator HT_HandRot = FRotator::ZeroRotator, HT_HandRot0 = FRotator::ZeroRotator, HT_ForeRot = FRotator::ZeroRotator, HT_ForeRot0 = FRotator::ZeroRotator;
	bool HT_HasFore = false; float HT_ForePitch = 0.0f;
	void HandTuneApply();
	// The hold as RefreshHeldWeapon makes it, for any catalogue entry (the tuning page's way of putting a weapon in the hand).
	void ApplyWeaponToPawn(class ABaseCharacter* Me, const WeaponCatalog::FWeapon* W, const FString& Handle = FString());
	bool bTerminalSeated = false;
	UPROPERTY() TObjectPtr<class UTerminalWidget> TerminalWidget;
	UPROPERTY() TObjectPtr<class ACameraActor> TerminalCamera;
	// The console drawn ON the monitor: a world-space widget component sized to the screen quad,
	// and the pointer that carries the mouse and keyboard to it.
	UPROPERTY() TObjectPtr<class UWidgetComponent> TerminalScreen;
	UPROPERTY() TObjectPtr<class UWidgetInteractionComponent> TerminalPointer;
	TWeakObjectPtr<AActor> TerminalTarget;
	FTimerHandle TerminalPlaceTimer;
	// The glass: a mesh cut from the monitor's own screen polygons wearing the widget's picture,
	// so the console has exactly the screen's outline. The widget component itself is not drawn.
	UPROPERTY() TObjectPtr<class UProceduralMeshComponent> TerminalGlass;
	UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> TerminalGlassMID;
	FTimerHandle TerminalBindTimer;
	bool bTerminalKeysDirect = false;
	bool bTerminalHolstered = false;   // the weapon went away for the terminal; it comes back on leaving

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
	UFUNCTION(BlueprintCallable, Category = "Reference") void ShowWeaponPreview(const FString& MeshPath, const FString& WeaponName = FString());   // the weapon's name, when it is one, puts its skin on the piece
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
	// One-shot F12 capture: traces from the eye along the aim and records whatever the reticle is
	// on. Needs no arming and no edit mode; the armed mode above is the old, UI-driven path.
	UFUNCTION(BlueprintCallable, Category = "Edit Mode") void CaptureAssistUnderReticle();
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
	// The thing under the reticle waits out a dwell before it becomes the target: a short one
	// for something with a real action, a longer one for flavour, so a sweep lights nothing.
	TWeakObjectPtr<AActor> InspectCandidate;
	float InspectCandidateSince = 0.0f;
	bool bCandidateFlavour = false;
	UPROPERTY(EditAnywhere, Category = "Inspect") float InspectDwellSeconds = 0.12f;
	UPROPERTY(EditAnywhere, Category = "Inspect") float InspectDwellFlavourSeconds = 0.4f;
	UPROPERTY(EditAnywhere, Category = "Inspect") float ScanRange = 900.0f;
	bool bScanHeld = false;
	float ScanNextRefresh = 0.0f;
	TArray<TWeakObjectPtr<AActor>> ScanLit;
	void TickScan(float Now);
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
	FString PreviewSkinOverride;
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
	bool bInspectMenuFlavour = false;
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
	void OnInspectUseReleased();
	// CARRYING. E held on a takeable prop lifts it into the off hand and moves it with the view:
	// let go to set it down where it is, tap E while carrying to put it in the bag instead. A tap
	// on E stays the reticle action; only a hold on something that can be carried starts this.
	bool CanCarry(AActor* Target) const;
	void BeginCarry(AActor* Target);
	void DropCarried();
	void StoreCarried();
	void TickCarry(float DeltaSeconds);
	UFUNCTION(BlueprintPure, Category = "Inspect") bool IsCarrying() const { return Carried.IsValid(); }
	TWeakObjectPtr<AActor> Carried;
	TWeakObjectPtr<class UPrimitiveComponent> CarriedComp;
	TWeakObjectPtr<AActor> HoldCandidate;
	bool bInspectUseHeld = false, bCarryBegunThisHold = false;
	float InspectUseHeldSince = 0.0f;
	float CarryDistance = 60.0f, CarriedRadius = 15.0f;
	UPROPERTY(EditAnywhere, Category = "Inspect") float CarryHoldSeconds = 0.3f;   // E held this long on a takeable thing picks it up rather than acting
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
	// Below this height the deck's own bed plays instead (UAmbientPlayer "deck"); the lift's lowest stop is at -5000.
	UPROPERTY(EditAnywhere, Category = "Audio") float DeckAmbientBelowZ = -4000.0f;
	void TickAmbientZone(float DeltaSeconds);
	// Spent casings (BrassFx): thrown on every shot of a cartridge weapon.
	UPROPERTY() TObjectPtr<class UBrassFx> Brass;
	float AmbientZoneClock = 0.0f;
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
