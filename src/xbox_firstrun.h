// Copyright (c) 2026 Skitzelles - part of Game Vault, https://github.com/skitzellez/xbox-game-vault
// Modified version of Fallout 2 Community Edition (Sustainable Use License, see LICENSE.md): Xbox port.
#ifndef FALLOUT_XBOX_FIRSTRUN_H_
#define FALLOUT_XBOX_FIRSTRUN_H_

#include <string>

namespace fallout {

// No game folder yet: builds one from the player's Fallout and Fallout 2 folders on a USB drive, with a
// progress screen. Returns the game folder, or an empty string (the player was told what to do).
std::string xboxFirstRun(const std::string& prefPath);

} // namespace fallout

#endif /* FALLOUT_XBOX_FIRSTRUN_H_ */
