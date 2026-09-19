#pragma once
#include "CoreMinimal.h"

// THE MODAL SCREENS, IN ONE PLACE.
//
// Ten separate bools used to sit side by side on the player controller -- bPauseMenuOpen,
// bReferenceOpen, bTerminalOpen and the rest -- and two aggregate predicates, IsAnyScreenOpen()
// and IsPageOpen(), each hand-repeated the same ten-term disjunction. Adding a screen therefore
// meant remembering two lists in two places, and forgetting one gave you a screen that opened
// but did not pause the world, lower the weapon, or release the mouse. The set of screens now
// lives here exactly once.
//
// This holds screen STATE, not widgets. It deliberately knows nothing about UMG: the controller
// still owns the widget objects, and this answers only "which screens are up". That separation is
// the point -- it is what lets Core stop asking a widget whether it is visible.

enum class EScreen : uint8
{
	PauseMenu,
	Scenes,
	Settings,
	Reference,
	CharacterSheet,
	Transfer,
	Appearance,
	Terminal,
	SaveLoad,
	HandTune,
	Count
};

struct FScreenStack
{
	bool IsOpen(EScreen S) const { return (Bits & Bit(S)) != 0; }
	void Open(EScreen S) { Bits |= Bit(S); }
	void Close(EScreen S) { Bits &= ~Bit(S); }
	void SetOpen(EScreen S, bool bOpen) { bOpen ? Open(S) : Close(S); }

	// Any modal screen at all. The callers that used to spell out ten flags ask this instead.
	bool AnyOpen() const { return Bits != 0; }

	void CloseAll() { Bits = 0; }

	// For diagnostics: the screens currently up, comma separated, or "none".
	FString Describe() const
	{
		TArray<FString> Up;
		for (uint8 i = 0; i < (uint8)EScreen::Count; ++i)
		{
			if (IsOpen((EScreen)i)) { Up.Add(Name((EScreen)i)); }
		}
		return Up.Num() ? FString::Join(Up, TEXT(", ")) : TEXT("none");
	}

	static const TCHAR* Name(EScreen S)
	{
		switch (S)
		{
		case EScreen::PauseMenu:      return TEXT("PauseMenu");
		case EScreen::Scenes:         return TEXT("Scenes");
		case EScreen::Settings:       return TEXT("Settings");
		case EScreen::Reference:      return TEXT("Reference");
		case EScreen::CharacterSheet: return TEXT("CharacterSheet");
		case EScreen::Transfer:       return TEXT("Transfer");
		case EScreen::Appearance:     return TEXT("Appearance");
		case EScreen::Terminal:       return TEXT("Terminal");
		case EScreen::SaveLoad:       return TEXT("SaveLoad");
		case EScreen::HandTune:       return TEXT("HandTune");
		default:                      return TEXT("?");
		}
	}

private:
	static uint16 Bit(EScreen S) { return (uint16)(1u << (uint8)S); }
	uint16 Bits = 0;
};
