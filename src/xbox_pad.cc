// Copyright (c) 2026 Skitzelles - part of Game Vault, https://github.com/skitzellez/xbox-game-vault
// Modified version of Fallout 2 Community Edition (Sustainable Use License, see LICENSE.md): Xbox port.
#include "xbox_pad.h"

#include "xbox_assist.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>

namespace fallout {

static char gXboxLogPath[1024];

// Each line is appended with the file opened and closed again, so the log can be read (Device Portal) while
// the game runs and keeps every line up to a crash.
void xboxLogOpen(const char* path)
{
    snprintf(gXboxLogPath, sizeof(gXboxLogPath), "%s", path);
    FILE* stream = fopen(gXboxLogPath, "wb");
    if (stream != nullptr) {
        fclose(stream);
    }
}

void xboxLog(const char* fmt, ...)
{
    if (gXboxLogPath[0] == '\0') {
        return;
    }
    char line[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(line, sizeof(line) - 2, fmt, args);
    va_end(args);
    FILE* stream = fopen(gXboxLogPath, "ab");
    if (stream != nullptr) {
        fputs(line, stream);
        fputs("\r\n", stream);
        fclose(stream);
    }
}

static SDL_GameController* gPad = nullptr;
static SDL_JoystickID gPadId = -1;
static bool gPadLeft = false;
static bool gPadTrigger = false;
static bool gPadCancel = false;
static bool gSmartCursor = true;
static bool gOverTarget = false;
static bool gPadRight = false;
static bool gTriggerDown[2] = { false, false };
static bool gArrowDown[4] = { false, false, false, false };
static Uint32 gLastMouseTicks = 0;
static float gCursorRestX = 0.0f;
static float gCursorRestY = 0.0f;

// The game's own hotkeys, pressed by the controller.
static SDL_Scancode buttonKey(int button)
{
    switch (button) {
    case SDL_CONTROLLER_BUTTON_B:
        return SDL_SCANCODE_ESCAPE; // close / cancel; in the game view: cancel aiming or end turn
    case SDL_CONTROLLER_BUTTON_Y:
        return SDL_SCANCODE_I; // inventory
    case SDL_CONTROLLER_BUTTON_BACK:
        return SDL_SCANCODE_P; // Pip-Boy
    case SDL_CONTROLLER_BUTTON_START:
        return SDL_SCANCODE_O; // options menu (save, load, quit)
    case SDL_CONTROLLER_BUTTON_DPAD_UP:
        return SDL_SCANCODE_C; // character
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
        return SDL_SCANCODE_TAB; // map
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
        return SDL_SCANCODE_B; // swap hands
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
        return SDL_SCANCODE_N; // weapon mode (single, burst, aimed...)
    case SDL_CONTROLLER_BUTTON_LEFTSTICK:
        return SDL_SCANCODE_F6; // quick save
    case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
        return SDL_SCANCODE_HOME; // centre the view on the player
    }
    return SDL_SCANCODE_UNKNOWN;
}

static void pushKey(SDL_Scancode scancode, bool down)
{
    SDL_Event key;
    SDL_zero(key);
    key.type = down ? SDL_KEYDOWN : SDL_KEYUP;
    key.key.state = down ? SDL_PRESSED : SDL_RELEASED;
    key.key.keysym.scancode = scancode;
    key.key.keysym.sym = SDL_GetKeyFromScancode(scancode);
    SDL_PushEvent(&key);
}

static void openPad(int deviceIndex)
{
    if (gPad != nullptr) {
        return;
    }
    gPad = SDL_GameControllerOpen(deviceIndex);
    if (gPad != nullptr) {
        gPadId = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(gPad));
        xboxLog("controller connected: %s", SDL_GameControllerName(gPad));
    } else {
        xboxLog("controller open failed: %s", SDL_GetError());
    }
}

static float axis(SDL_GameControllerAxis which)
{
    return gPad != nullptr ? SDL_GameControllerGetAxis(gPad, which) / 32767.0f : 0.0f;
}

// Right stick: holds the arrow keys, which scroll the map.
static void updateArrows()
{
    static const SDL_Scancode keys[4] = { SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT, SDL_SCANCODE_UP, SDL_SCANCODE_DOWN };
    float x = axis(SDL_CONTROLLER_AXIS_RIGHTX);
    float y = axis(SDL_CONTROLLER_AXIS_RIGHTY);
    float push[4] = { -x, x, -y, y };
    for (int i = 0; i < 4; i++) {
        bool want = gArrowDown[i] ? push[i] > 0.3f : push[i] > 0.5f;
        if (want != gArrowDown[i]) {
            gArrowDown[i] = want;
            pushKey(keys[i], want);
        }
    }
}

bool xboxPadHandleEvent(const SDL_Event* e)
{
    switch (e->type) {
    case SDL_CONTROLLERDEVICEADDED:
        openPad(e->cdevice.which);
        return true;
    case SDL_CONTROLLERDEVICEREMOVED:
        if (gPad != nullptr && e->cdevice.which == gPadId) {
            SDL_GameControllerClose(gPad);
            gPad = nullptr;
            gPadId = -1;
            gPadLeft = gPadRight = false;
            xboxLog("controller removed");
            for (int i = 0; i < SDL_NumJoysticks(); i++) {
                if (SDL_IsGameController(i)) {
                    openPad(i);
                    break;
                }
            }
        }
        return true;
    case SDL_CONTROLLERBUTTONDOWN:
    case SDL_CONTROLLERBUTTONUP: {
        bool down = e->type == SDL_CONTROLLERBUTTONDOWN;
        if (e->cbutton.button == SDL_CONTROLLER_BUTTON_A) {
            gPadLeft = down;
        } else if (e->cbutton.button == SDL_CONTROLLER_BUTTON_X) {
            gPadRight = down; // right click: the player picks the cursor mode
            if (down) {
                gSmartCursor = false;
            }
        } else if (e->cbutton.button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER || e->cbutton.button == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) {
            if (down) {
                xboxAssistCycle(e->cbutton.button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER ? -1 : 1);
            }
        } else {
            if (e->cbutton.button == SDL_CONTROLLER_BUTTON_B && down) {
                gPadCancel = true;
            }
            SDL_Scancode key = buttonKey(e->cbutton.button);
            if (key != SDL_SCANCODE_UNKNOWN) {
                pushKey(key, down);
            }
        }
        return true;
    }
    case SDL_CONTROLLERAXISMOTION:
        if (e->caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT || e->caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) {
            int t = e->caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT ? 0 : 1;
            bool down = gTriggerDown[t] ? e->caxis.value > 8000 : e->caxis.value > 16000;
            if (down != gTriggerDown[t]) {
                gTriggerDown[t] = down;
                if (t == 0) {
                    pushKey(SDL_SCANCODE_S, down); // LT: Skilldex
                } else {
                    gPadTrigger = down; // RT: the action button (left click), like A
                }
            }
        } else if (e->caxis.axis == SDL_CONTROLLER_AXIS_RIGHTX || e->caxis.axis == SDL_CONTROLLER_AXIS_RIGHTY) {
            updateArrows();
        }
        return true;
    case SDL_JOYAXISMOTION:
    case SDL_JOYBALLMOTION:
    case SDL_JOYHATMOTION:
    case SDL_JOYBUTTONDOWN:
    case SDL_JOYBUTTONUP:
    case SDL_JOYDEVICEADDED:
    case SDL_JOYDEVICEREMOVED:
        return true;
    }
    return false;
}

bool xboxPadSmartCursor()
{
    return gSmartCursor;
}

void xboxPadSetSmartCursor(bool on)
{
    gSmartCursor = on;
}

void xboxPadSetCursorOverTarget(bool over)
{
    gOverTarget = over;
}

bool xboxPadTakeCancel()
{
    bool cancel = gPadCancel;
    gPadCancel = false;
    return cancel;
}

void xboxPadMouse(int* dx, int* dy, unsigned char* leftButton, unsigned char* rightButton)
{
    Uint32 now = SDL_GetTicks();
    Uint32 elapsed = gLastMouseTicks != 0 ? now - gLastMouseTicks : 0;
    gLastMouseTicks = now;
    if (elapsed > 50) {
        elapsed = 50;
    }

    float x = axis(SDL_CONTROLLER_AXIS_LEFTX);
    float y = axis(SDL_CONTROLLER_AXIS_LEFTY);
    float length = sqrtf(x * x + y * y);
    const float deadZone = 0.18f;
    if (length > deadZone) {
        // Curved response: gentle near the centre for aiming, fast at the edge for crossing the screen.
        float t = (fminf(length, 1.0f) - deadZone) / (1.0f - deadZone);
        // logical pixels per second: about two seconds across the 1280-wide screen at full tilt
        float speed = 560.0f * t * t + 50.0f * t;
        if (gOverTarget) {
            speed *= 0.45f; // sticky over people, doors and items
        }
        float scale = speed * elapsed / 1000.0f / length;
        gCursorRestX += x * scale;
        gCursorRestY += y * scale;
    }
    int moveX = (int)gCursorRestX;
    int moveY = (int)gCursorRestY;
    gCursorRestX -= moveX;
    gCursorRestY -= moveY;

    *dx += moveX;
    *dy += moveY;
    *leftButton = *leftButton || gPadLeft || gPadTrigger;
    *rightButton = *rightButton || gPadRight;
}

} // namespace fallout
