// Copyright (c) 2026 Skitzelles - part of Game Vault, https://github.com/skitzellez/xbox-game-vault
// Modified version of Fallout 2 Community Edition (Sustainable Use License, see LICENSE.md): Xbox port.
#ifndef FALLOUT_XBOX_ASSIST_H_
#define FALLOUT_XBOX_ASSIST_H_

namespace fallout {

// Every frame in the game view: smart cursor mode and "cursor is over something" for the stick slow-down.
void xboxAssistTick();

// LB (-1) / RB (+1): moves the cursor to the previous / next thing on screen.
void xboxAssistCycle(int direction);

} // namespace fallout

#endif /* FALLOUT_XBOX_ASSIST_H_ */
