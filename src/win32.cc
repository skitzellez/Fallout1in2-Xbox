#include "win32.h"

#include <stdlib.h>

#include <SDL.h>

#if __APPLE__
#include <TargetConditionals.h>
#endif

#ifndef _WIN32
#include <unistd.h>
#endif

#if __APPLE__ && TARGET_OS_OSX
#include <CoreFoundation/CoreFoundation.h>
#endif

#ifdef FALLOUT_MAPPER
#include "mapper/mapper.h"
#else
#include "main.h"
#endif
#include "svga.h"
#include "window_manager.h"

#include "scan_unimplemented.h"

#if __APPLE__ && TARGET_OS_IOS
#include "platform/ios/paths.h"
#endif

// explicit `atexit` group
#include "audio_engine.h"

#ifdef FALLOUT_XBOX
#include <direct.h>
#include <string>
#include "xbox_firstrun.h"
#include "xbox_pad.h"
#endif

namespace fallout {

// 0x51E444 GNW95_isActive
bool gProgramIsActive = false;

#if __APPLE__ && TARGET_OS_OSX
static char gMacOsBundleResourcesPath[COMPAT_MAX_PATH];
#endif

#ifdef _WIN32
HANDLE GNW95_mutex = nullptr;
#endif

#if __APPLE__ && TARGET_OS_OSX
const char* getMacOsBundleResourcesPath()
{
    return gMacOsBundleResourcesPath[0] != '\0' ? gMacOsBundleResourcesPath : nullptr;
}
#endif

#ifdef FALLOUT_XBOX
static bool xboxFileExists(const std::string& path)
{
    DWORD attributes = GetFileAttributesA(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

// The game lives in a "Fallout1in2" folder (made by the PC setup) inside the Fallout 2 folder, which holds
// master.dat and critter.dat. It is looked for in the app's own storage and on every drive (USB sticks).
static std::string xboxFindGameFolder(const std::string& prefPath)
{
    static const char* const places[] = {
        "Fallout1in2",
        "Fallout/Fallout1in2",
        "Fallout 2/Fallout1in2",
        "Fallout2/Fallout1in2",
        "Games/Fallout/Fallout1in2",
        "Games/Fallout 2/Fallout1in2",
        "Games/Fallout1in2",
    };
    std::string roots[32];
    int rootCount = 0;
    if (!prefPath.empty()) {
        roots[rootCount++] = prefPath;
    }
    for (char letter = 'D'; letter <= 'Z'; letter++) {
        roots[rootCount++] = std::string(1, letter) + ":/";
    }
    for (int r = 0; r < rootCount; r++) {
        for (const char* place : places) {
            std::string folder = roots[r] + place;
            if (xboxFileExists(folder + "/fallout2.cfg") && xboxFileExists(folder + "/../master.dat")) {
                return folder;
            }
        }
    }
    return std::string();
}
#endif

int main(int argc, char* argv[])
{
    scanUnimplementdParseCommandLineArguments(argc, argv);

#ifdef FALLOUT_XBOX
    SDL_SetHint("SDL_JOYSTICK_WGI", "1"); // Windows.Gaming.Input: the only controller API in Xbox apps
    std::string prefPath;
    char* pref = SDL_GetPrefPath("Skitzelles", "Fallout1in2");
    if (pref != nullptr) {
        prefPath = pref;
        SDL_free(pref);
    }
    xboxLogOpen((prefPath + "fallout_xbox_log.txt").c_str());
    xboxLog("Fallout 1in2 for Xbox starting, app storage %s", prefPath.c_str());
    xboxLog("looking for the game folder");
    std::string gameFolder = xboxFindGameFolder(prefPath);
    if (gameFolder.empty()) {
        // first run: build the game folder from the player's own Fallout and Fallout 2 on the USB drive
        xboxLog("no Fallout1in2 folder found, first run");
        SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER);
        gameFolder = xboxFirstRun(prefPath);
        if (gameFolder.empty()) {
            SDL_Quit();
            return EXIT_FAILURE;
        }
    }
    xboxLog("game folder %s", gameFolder.c_str());
    _chdir(gameFolder.c_str());
#endif

    int rc;

#if _WIN32
    GNW95_mutex = CreateMutexA(0, TRUE, "GNW95MUTEX");
    if (GetLastError() != ERROR_SUCCESS) {
        return 0;
    }
#ifdef SDL_HINT_WINDOWS_DPI_AWARENESS
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
#endif
#endif

#if __APPLE__ && TARGET_OS_IOS
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    chdir(iOSGetDocumentsPath());
#endif

#if __APPLE__ && TARGET_OS_OSX
    CFBundleRef mainBundle = CFBundleGetMainBundle();
    if (mainBundle != nullptr) {
        CFURLRef resourcesUrl = CFBundleCopyResourcesDirectoryURL(mainBundle);
        if (resourcesUrl != nullptr) {
            if (CFURLGetFileSystemRepresentation(resourcesUrl,
                    true,
                    reinterpret_cast<UInt8*>(gMacOsBundleResourcesPath),
                    sizeof(gMacOsBundleResourcesPath))) {
                gMacOsBundleResourcesPath[sizeof(gMacOsBundleResourcesPath) - 1] = '\0';
            }
            CFRelease(resourcesUrl);
        }
    }

    char* basePath = SDL_GetBasePath();
    chdir(basePath);
    SDL_free(basePath);
#endif

#if __ANDROID__
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    chdir(SDL_AndroidGetExternalStoragePath());
#endif

#ifdef FALLOUT_XBOX
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER) != 0) {
        xboxLog("SDL_Init failed: %s", SDL_GetError());
        return EXIT_FAILURE;
    }
    xboxLog("video driver %s, audio driver %s", SDL_GetCurrentVideoDriver(), SDL_GetCurrentAudioDriver());
#else
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        return EXIT_FAILURE;
    }
#endif

    atexit([] {
        audioEngineExit();
        SDL_Quit();
    });

    SDL_ShowCursor(SDL_DISABLE);

    gProgramIsActive = true;
#ifdef FALLOUT_MAPPER
    rc = mapper_main(argc, argv);
#else
    rc = falloutMain(argc, argv);
#endif

#if _WIN32
    CloseHandle(GNW95_mutex);
#endif

    return rc;
}

} // namespace fallout

#ifdef FALLOUT_XBOX
// Xbox (UWP): the app must start through SDL3's SDL_RunApp, which sets up the Xbox window and then calls
// the game on its own thread. SDL3.dll is the library under sdl2-compat; looked up by name so the two
// libraries' same-named functions never mix.
static int xboxMain(int argc, char* argv[])
{
    (void)argc;
    (void)argv;
    static char exeName[] = "fallout2-ce.exe";
    static char* args[] = { exeName, nullptr };
    return fallout::main(1, args);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR commandLine, int show)
{
    (void)instance;
    (void)previous;
    (void)commandLine;
    (void)show;
    typedef int (*MainFunction)(int, char**);
    typedef int (*RunAppFunction)(int, char**, MainFunction, void*);
    HMODULE sdl3 = LoadLibraryA("SDL3.dll");
    RunAppFunction runApp = sdl3 != nullptr ? (RunAppFunction)(void*)GetProcAddress(sdl3, "SDL_RunApp") : nullptr;
    if (runApp == nullptr) {
        return EXIT_FAILURE;
    }
    return runApp(0, nullptr, xboxMain, nullptr);
}
#else
int main(int argc, char* argv[])
{
    return fallout::main(argc, argv);
}
#endif
