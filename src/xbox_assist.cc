// Copyright (c) 2026 Skitzelles - part of Game Vault, https://github.com/skitzellez/xbox-game-vault
// Modified version of Fallout 2 Community Edition (Sustainable Use License, see LICENSE.md): Xbox port.
//
// Controller helpers that know the game world (Xbox app only):
// - smart cursor: over a person, door or item the cursor turns into the "use / talk" hand, over an enemy in
//   combat into the crosshair, over the ground back into "walk", so RT always does the obvious thing;
// - LB / RB jump the cursor to the previous / next thing on screen (enemies only in combat), nearest first;
// - the stick slows down while the cursor is over something, so it is easy to stop on it.
#include "xbox_assist.h"

#include <algorithm>
#include <vector>

#include "combat.h"
#include "critter.h"
#include "game_mouse.h"
#include "interface.h"
#include "map.h"
#include "mouse.h"
#include "object.h"
#include "party_member.h"
#include "proto.h"
#include "svga.h"
#include "xbox_pad.h"

namespace fallout {

static bool isDoor(Object* obj)
{
    Proto* proto;
    return protoGetProto(ProtoId(obj), &proto) != -1 && proto->scenery.type == SCENERY_TYPE_DOOR;
}

static Object* gLastTarget = nullptr;

static bool isEnemy(Object* obj)
{
    return FrmId(obj).objectType() == OBJ_TYPE_CRITTER && obj != gDude && !critterIsDead(obj)
        && !objectIsPartyMember(obj);
}

// Things worth pointing at: people and creatures, items on the ground, doors, and scenery with a script
// (computers, switches, lockers...). In combat only enemies.
static bool isTarget(Object* obj, bool combat)
{
    if (obj == nullptr || obj == gDude || (obj->flags & OBJECT_HIDDEN) != 0) {
        return false;
    }
    if (combat) {
        return isEnemy(obj);
    }
    switch (FrmId(obj).objectType()) {
    case OBJ_TYPE_CRITTER:
        return !objectIsPartyMember(obj) || !critterIsDead(obj);
    case OBJ_TYPE_ITEM:
        return true;
    case OBJ_TYPE_SCENERY:
        return isDoor(obj) || obj->sid != -1;
    default:
        return false;
    }
}

static void targetPoint(Object* obj, int* x, int* y)
{
    Rect rect;
    objectGetRect(obj, &rect);
    *x = (rect.left + rect.right) / 2;
    // critters: aim at the body rather than the feet
    *y = FrmId(obj).objectType() == OBJ_TYPE_CRITTER ? rect.top + (rect.bottom - rect.top) * 2 / 5 : (rect.top + rect.bottom) / 2;
}

void xboxAssistCycle(int direction)
{
    if (!interfaceBarEnabled() || gDude == nullptr) {
        return;
    }
    bool combat = isInCombat();
    int screenRight = screenGetWidth();
    int screenBottom = screenGetVisibleHeight();
    int dudeX;
    int dudeY;
    targetPoint(gDude, &dudeX, &dudeY);

    struct Candidate {
        Object* obj;
        int distance;
    };
    std::vector<Candidate> candidates;
    for (Object* obj = objectFindFirstAtElevation(gElevation); obj != nullptr; obj = objectFindNextAtElevation()) {
        if (!isTarget(obj, combat)) {
            continue;
        }
        int x;
        int y;
        targetPoint(obj, &x, &y);
        if (x < 0 || y < 0 || x >= screenRight || y >= screenBottom) {
            continue;
        }
        int dx = x - dudeX;
        int dy = y - dudeY;
        candidates.push_back({ obj, dx * dx + dy * dy });
    }
    if (candidates.empty()) {
        return;
    }
    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) { return a.distance < b.distance; });

    int index = 0;
    for (size_t i = 0; i < candidates.size(); i++) {
        if (candidates[i].obj == gLastTarget) {
            index = (int)((i + candidates.size() + direction) % candidates.size());
            break;
        }
    }
    gLastTarget = candidates[index].obj;
    int x;
    int y;
    targetPoint(gLastTarget, &x, &y);
    _mouse_set_position(x, y);
    xboxPadSetSmartCursor(true);
}

void xboxAssistTick()
{
    if (!interfaceBarEnabled() || gDude == nullptr) {
        xboxPadSetCursorOverTarget(false);
        return;
    }
    bool combat = isInCombat();
    Object* pointed = gameMouseGetObjectUnderCursor(OBJ_TYPE_INVALID, false, gElevation);
    bool over = pointed != nullptr && (isTarget(pointed, combat) || isTarget(pointed, false));
    xboxPadSetCursorOverTarget(over);
    if (!xboxPadSmartCursor()) {
        return;
    }

    GameMouseMode mode = gameMouseGetMode();
    if (mode != GAME_MOUSE_MODE_MOVE && mode != GAME_MOUSE_MODE_ARROW && mode != GAME_MOUSE_MODE_CROSSHAIR) {
        return; // using a skill or an item on something: leave that cursor alone
    }
    GameMouseMode want = GAME_MOUSE_MODE_MOVE;
    if (combat && pointed != nullptr && isEnemy(pointed)) {
        want = GAME_MOUSE_MODE_CROSSHAIR;
    } else if (pointed != nullptr && isTarget(pointed, false)) {
        want = GAME_MOUSE_MODE_ARROW;
    }
    if (want != mode) {
        gameMouseSetMode(want);
    }
}

} // namespace fallout
