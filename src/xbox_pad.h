// Copyright (c) 2026 Skitzelles - part of Game Vault, https://github.com/skitzellez/xbox-game-vault
// Modified version of Fallout 2 Community Edition (Sustainable Use License, see LICENSE.md): Xbox port.
#ifndef FALLOUT_XBOX_PAD_H_
#define FALLOUT_XBOX_PAD_H_

#include <SDL.h>

namespace fallout {

// Xbox controller support: the left stick drives the mouse cursor, buttons click or press the game's hotkeys,
// the right stick scrolls the map. Only built into the Xbox app (FALLOUT_XBOX).

// Called for every SDL event; returns true when the event was a controller event (and is used up).
bool xboxPadHandleEvent(const SDL_Event* e);

// Adds the stick's cursor movement since the last call and the A / X "mouse buttons".
void xboxPadMouse(int* dx, int* dy, unsigned char* leftButton, unsigned char* rightButton);

// True (once) when the last Escape came from the controller's B button: the game view turns it into
// "cancel aiming" or "end turn" instead of opening the options menu.
bool xboxPadTakeCancel();

// Smart cursor (xbox_assist.cc) is on until the player picks a cursor mode with X; B or LB / RB turn it on again.
bool xboxPadSmartCursor();
void xboxPadSetSmartCursor(bool on);

// Set every frame: the cursor is over something, so the stick moves it slower.
void xboxPadSetCursorOverTarget(bool over);

// Writes a line to fallout_xbox_log.txt next to the game (opened by xboxLogOpen).
void xboxLog(const char* fmt, ...);
void xboxLogOpen(const char* path);

} // namespace fallout

#endif /* FALLOUT_XBOX_PAD_H_ */
