// Copyright (c) 2026 Skitzelles - part of Game Vault, https://github.com/skitzellez/xbox-game-vault
// Modified version of Fallout 2 Community Edition (Sustainable Use License, see LICENSE.md): Xbox port.
//
// First run on the Xbox, no PC tool needed: the player copies their own Fallout and Fallout 2 folders onto a
// USB drive. This finds them (by their master.dat: Fallout 1's is the old DAT format), gets Fallout 1in2
// (a Fallout1in2*.zip on the drive, or the newest release from GitHub), unpacks it into the Fallout 2 folder,
// unpacks Fallout 1's files from its master.dat (the mod's own file list), adds the HQ music (when the download
// works) and the extra details, then hands the folder to the game. A progress screen shows each step.
#include "xbox_firstrun.h"

#include <windows.h>
#include <winhttp.h>

#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <SDL.h>
#include <zlib.h>

#include "../tools/dat_archive.h"
#include "lodepng.h"
#include "svga.h"
#include "xbox_art.h"
#include "xbox_pad.h"

namespace fallout {

static const char* const kFo1in2Api = "https://api.github.com/repos/rotators/Fo1in2/releases/latest";
static const char* const kMusicApi = "https://api.github.com/repos/BGforgeNet/Fallout2-HQ-music/releases/latest";

// ---------- the screen ----------

namespace {

struct Font {
    SDL_Texture* texture = nullptr;
    int cellW = 0;
    int cellH = 0;
    const unsigned char* widths = nullptr;
};

class Screen {
public:
    bool open();
    void close();
    void show(const std::string& title, const std::string& line, const std::string& detail, double progress, bool force = false);
    int waitForButton();

private:
    SDL_Texture* loadPng(const unsigned char* data, size_t size, int* w, int* h);
    int textWidth(Font& font, const std::string& text);
    void text(Font& font, const std::string& text, int x, int y, Uint8 r, Uint8 g, Uint8 b, bool centred);
    void pump();

    SDL_Renderer* renderer_ = nullptr;
    Font small_;
    Font big_;
    SDL_Texture* emblem_ = nullptr;
    Uint32 lastDraw_ = 0;
};

SDL_Texture* Screen::loadPng(const unsigned char* data, size_t size, int* w, int* h)
{
    unsigned char* pixels = nullptr;
    unsigned width = 0;
    unsigned height = 0;
    if (lodepng_decode32(&pixels, &width, &height, data, size) != 0) {
        return nullptr;
    }
    SDL_Texture* texture = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, width, height);
    if (texture != nullptr) {
        SDL_UpdateTexture(texture, nullptr, pixels, width * 4);
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    }
    free(pixels);
    *w = width;
    *h = height;
    return texture;
}

bool Screen::open()
{
    if (gSdlWindow == nullptr) {
        gSdlWindow = SDL_CreateWindow("Fallout 1in2", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1280, 720, SDL_WINDOW_FULLSCREEN | SDL_WINDOW_ALLOW_HIGHDPI);
    }
    if (gSdlWindow == nullptr) {
        xboxLog("first run: no window: %s", SDL_GetError());
        return false;
    }
    renderer_ = SDL_CreateRenderer(gSdlWindow, -1, 0);
    if (renderer_ == nullptr) {
        xboxLog("first run: no renderer: %s", SDL_GetError());
        return false;
    }
    SDL_RenderSetLogicalSize(renderer_, 1280, 720);
    int w;
    int h;
    small_ = { loadPng(kFont_small_png, sizeof(kFont_small_png), &w, &h), kFont_small_cell_w, kFont_small_cell_h, kFont_small_widths };
    big_ = { loadPng(kFont_big_png, sizeof(kFont_big_png), &w, &h), kFont_big_cell_w, kFont_big_cell_h, kFont_big_widths };
    emblem_ = loadPng(kEmblem_png, sizeof(kEmblem_png), &w, &h);
    return true;
}

void Screen::close()
{
    SDL_DestroyTexture(small_.texture);
    SDL_DestroyTexture(big_.texture);
    SDL_DestroyTexture(emblem_);
    SDL_DestroyRenderer(renderer_);
    renderer_ = nullptr;
}

int Screen::textWidth(Font& font, const std::string& s)
{
    int w = 0;
    for (unsigned char c : s) {
        w += (c >= 32 && c < 127) ? font.widths[c - 32] : 0;
    }
    return w;
}

void Screen::text(Font& font, const std::string& s, int x, int y, Uint8 r, Uint8 g, Uint8 b, bool centred)
{
    if (font.texture == nullptr) {
        return;
    }
    if (centred) {
        x -= textWidth(font, s) / 2;
    }
    SDL_SetTextureColorMod(font.texture, r, g, b);
    for (unsigned char c : s) {
        if (c < 32 || c >= 127) {
            continue;
        }
        int i = c - 32;
        SDL_Rect src = { (i % 16) * font.cellW, (i / 16) * font.cellH, font.cellW, font.cellH };
        SDL_Rect dst = { x - 1, y, font.cellW, font.cellH };
        SDL_RenderCopy(renderer_, font.texture, &src, &dst);
        x += font.widths[i];
    }
}

void Screen::pump()
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        xboxPadHandleEvent(&e);
    }
}

void Screen::show(const std::string& title, const std::string& line, const std::string& detail, double progress, bool force)
{
    Uint32 now = SDL_GetTicks();
    if (!force && now - lastDraw_ < 40) {
        return;
    }
    lastDraw_ = now;
    pump();

    SDL_SetRenderDrawColor(renderer_, 11, 18, 11, 255);
    SDL_RenderClear(renderer_);
    if (emblem_ != nullptr) {
        SDL_Rect dst = { 640 - 80, 120, 160, 160 };
        SDL_RenderCopy(renderer_, emblem_, nullptr, &dst);
    }
    text(big_, title, 640, 320, 124, 255, 107, true);
    text(small_, line, 640, 400, 124, 255, 107, true);
    if (progress >= 0) {
        SDL_Rect frame = { 340, 450, 600, 18 };
        SDL_SetRenderDrawColor(renderer_, 46, 90, 46, 255);
        SDL_RenderDrawRect(renderer_, &frame);
        SDL_Rect fill = { 343, 453, (int)(594 * std::min(1.0, progress)), 12 };
        SDL_SetRenderDrawColor(renderer_, 124, 255, 107, 255);
        SDL_RenderFillRect(renderer_, &fill);
    }
    // detail may hold several lines, separated by '|'
    int y = progress >= 0 ? 500 : 450;
    size_t start = 0;
    while (start <= detail.size()) {
        size_t end = detail.find('|', start);
        if (end == std::string::npos) {
            end = detail.size();
        }
        text(small_, detail.substr(start, end - start), 640, y, 95, 168, 87, true);
        y += 34;
        start = end + 1;
    }
    text(small_, "Fan project, not made or endorsed by Bethesda. Uses your own copies of the games.", 640, 670, 60, 110, 56, true);
    SDL_RenderPresent(renderer_);
}

int Screen::waitForButton()
{
    for (;;) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_CONTROLLERDEVICEADDED) {
                xboxPadHandleEvent(&e);
            } else if (e.type == SDL_CONTROLLERBUTTONDOWN) {
                return e.cbutton.button;
            } else if (e.type == SDL_QUIT) {
                return -1;
            }
        }
        SDL_Delay(30);
    }
}

// ---------- files ----------

bool exists(const std::string& path)
{
    DWORD a = GetFileAttributesA(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool isDir(const std::string& path)
{
    DWORD a = GetFileAttributesA(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

long long fileSize(const std::string& path)
{
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &data)) {
        return -1;
    }
    return ((long long)data.nFileSizeHigh << 32) | data.nFileSizeLow;
}

std::vector<std::string> listDir(const std::string& dir, bool dirs, const char* pattern = "*")
{
    std::vector<std::string> names;
    WIN32_FIND_DATAA f;
    HANDLE h = FindFirstFileA((dir + "\\" + pattern).c_str(), &f);
    if (h == INVALID_HANDLE_VALUE) {
        return names;
    }
    do {
        bool d = (f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (d == dirs && strcmp(f.cFileName, ".") != 0 && strcmp(f.cFileName, "..") != 0) {
            names.push_back(f.cFileName);
        }
    } while (FindNextFileA(h, &f));
    FindClose(h);
    return names;
}

void makeDirs(const std::string& path)
{
    for (size_t i = 3; i < path.size(); i++) {
        if (path[i] == '\\' || path[i] == '/') {
            CreateDirectoryA(path.substr(0, i).c_str(), nullptr);
        }
    }
    CreateDirectoryA(path.c_str(), nullptr);
}

std::string lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)tolower(c); });
    return s;
}

// 1 = Fallout (DAT1 master.dat), 2 = Fallout 2 (DAT2), 0 = neither
int whichGame(const std::string& dir)
{
    std::string master = dir + "\\master.dat";
    if (!exists(master) || !exists(dir + "\\critter.dat")) {
        return 0;
    }
    std::unique_ptr<DatArchive> archive = DatArchive::open(master);
    if (archive == nullptr) {
        return 0;
    }
    return strcmp(archive->formatName(), "fallout1") == 0 ? 1 : 2;
}

// Looks a few folders deep, only into folders that could hold the games.
void findGames(const std::string& dir, int depth, std::string* fo1, std::string* fo2)
{
    for (const std::string& name : listDir(dir, true)) {
        std::string l = lower(name);
        std::string path = dir + "\\" + name;
        if (l.find("fallout") != std::string::npos && l != "fallout1in2") {
            int game = whichGame(path);
            if (game == 1 && fo1->empty()) {
                *fo1 = path;
            } else if (game == 2 && fo2->empty()) {
                *fo2 = path;
            } else if (game == 0 && depth < 4) {
                findGames(path, depth + 1, fo1, fo2); // e.g. Epic's "Fallout 2\Fallout 2 English"
            }
        } else if (depth < 4 && (l == "games" || l == "steam" || l == "steamlibrary" || l == "steamapps" || l == "common" || l == "gog games" || l == "epic games" || l == "gog galaxy")) {
            findGames(path, depth + 1, fo1, fo2);
        }
    }
}

// ---------- downloads ----------

typedef std::function<void(long long done, long long total)> Progress;

// GET over HTTPS with WinHTTP (follows GitHub's redirect to its download server). Writes to the file, or into
// *body when file is null.
bool httpGet(const std::string& url, FILE* file, std::string* body, const Progress& progress)
{
    std::wstring wurl(url.begin(), url.end());
    URL_COMPONENTS parts = {};
    parts.dwStructSize = sizeof(parts);
    wchar_t host[256];
    wchar_t path[2048];
    parts.lpszHostName = host;
    parts.dwHostNameLength = 256;
    parts.lpszUrlPath = path;
    parts.dwUrlPathLength = 2048;
    if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &parts)) {
        return false;
    }
    HINTERNET session = WinHttpOpen(L"Fallout1in2-Xbox/0.1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (session == nullptr) {
        xboxLog("first run: WinHttpOpen failed %lu", GetLastError());
        return false;
    }
    bool ok = false;
    HINTERNET connection = WinHttpConnect(session, host, parts.nPort, 0);
    HINTERNET request = connection != nullptr
        ? WinHttpOpenRequest(connection, L"GET", path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)
        : nullptr;
    if (request != nullptr && WinHttpSendRequest(request, L"Accept: */*\r\n", (DWORD)-1L, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
        && WinHttpReceiveResponse(request, nullptr)) {
        DWORD status = 0;
        DWORD size = sizeof(status);
        WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
        wchar_t lengthText[32] = L"";
        DWORD lengthSize = sizeof(lengthText);
        long long total = -1;
        if (WinHttpQueryHeaders(request, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX, lengthText, &lengthSize, WINHTTP_NO_HEADER_INDEX)) {
            total = _wtoi64(lengthText);
        }
        if (status == 200) {
            std::vector<char> buffer(1 << 20);
            long long done = 0;
            ok = true;
            for (;;) {
                DWORD read = 0;
                if (!WinHttpReadData(request, buffer.data(), (DWORD)buffer.size(), &read)) {
                    ok = false;
                    break;
                }
                if (read == 0) {
                    break;
                }
                if (file != nullptr) {
                    if (fwrite(buffer.data(), 1, read, file) != read) {
                        ok = false;
                        break;
                    }
                } else {
                    body->append(buffer.data(), read);
                }
                done += read;
                if (progress) {
                    progress(done, total);
                }
            }
            if (ok && total >= 0 && done != total) {
                ok = false;
            }
        } else {
            xboxLog("first run: %s answered %lu", url.c_str(), status);
        }
    } else {
        xboxLog("first run: request to %s failed %lu", url.c_str(), GetLastError());
    }
    if (request != nullptr) {
        WinHttpCloseHandle(request);
    }
    if (connection != nullptr) {
        WinHttpCloseHandle(connection);
    }
    WinHttpCloseHandle(session);
    return ok;
}

// The download link of the newest release's file whose name starts with prefix and ends with .zip.
std::string latestAssetUrl(const char* api, const char* prefix)
{
    std::string json;
    if (!httpGet(api, nullptr, &json, nullptr)) {
        return std::string();
    }
    const std::string key = "\"browser_download_url\"";
    for (size_t at = json.find(key); at != std::string::npos; at = json.find(key, at + 1)) {
        size_t start = json.find('"', at + key.size() + 1);
        size_t end = start != std::string::npos ? json.find('"', start + 1) : std::string::npos;
        if (end == std::string::npos) {
            break;
        }
        std::string url = json.substr(start + 1, end - start - 1);
        std::string name = url.substr(url.rfind('/') + 1);
        if (name.compare(0, strlen(prefix), prefix) == 0 && name.size() > 4 && name.compare(name.size() - 4, 4, ".zip") == 0) {
            return url;
        }
    }
    return std::string();
}

bool download(const std::string& url, const std::string& target, const Progress& progress)
{
    std::string part = target + ".part";
    FILE* file = fopen(part.c_str(), "wb");
    if (file == nullptr) {
        xboxLog("first run: can't write %s", part.c_str());
        return false;
    }
    bool ok = httpGet(url, file, nullptr, progress);
    fclose(file);
    if (!ok) {
        DeleteFileA(part.c_str());
        return false;
    }
    DeleteFileA(target.c_str());
    return MoveFileA(part.c_str(), target.c_str()) != 0;
}

// ---------- zip ----------

unsigned u16(const unsigned char* p) { return p[0] | (p[1] << 8); }
unsigned u32(const unsigned char* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((unsigned)p[3] << 24); }

// Unpacks every file for which target(name) gives a path (stored and deflated entries, no zip64).
bool unzip(const std::string& zipPath, const std::function<std::string(const std::string&)>& target, const Progress& progress)
{
    FILE* zip = fopen(zipPath.c_str(), "rb");
    if (zip == nullptr) {
        return false;
    }
    std::unique_ptr<FILE, int (*)(FILE*)> closer(zip, fclose);
    _fseeki64(zip, 0, SEEK_END);
    long long zipSize = _ftelli64(zip);
    long long tailSize = std::min<long long>(zipSize, 65536 + 22);
    std::vector<unsigned char> tail((size_t)tailSize);
    _fseeki64(zip, zipSize - tailSize, SEEK_SET);
    if (fread(tail.data(), 1, tail.size(), zip) != tail.size()) {
        return false;
    }
    long long eocd = -1;
    for (long long i = tailSize - 22; i >= 0; i--) {
        if (u32(&tail[(size_t)i]) == 0x06054b50) {
            eocd = i;
            break;
        }
    }
    if (eocd < 0) {
        xboxLog("first run: %s is not a zip", zipPath.c_str());
        return false;
    }
    unsigned count = u16(&tail[(size_t)eocd + 10]);
    unsigned dirSize = u32(&tail[(size_t)eocd + 12]);
    unsigned dirOffset = u32(&tail[(size_t)eocd + 16]);
    std::vector<unsigned char> dir(dirSize);
    _fseeki64(zip, dirOffset, SEEK_SET);
    if (fread(dir.data(), 1, dirSize, zip) != dirSize) {
        return false;
    }

    std::vector<unsigned char> in(1 << 18);
    std::vector<unsigned char> out(1 << 18);
    size_t at = 0;
    for (unsigned n = 0; n < count && at + 46 <= dir.size(); n++) {
        const unsigned char* h = &dir[at];
        if (u32(h) != 0x02014b50) {
            return false;
        }
        unsigned method = u16(h + 10);
        unsigned packed = u32(h + 20);
        unsigned nameLength = u16(h + 28);
        unsigned extraLength = u16(h + 30);
        unsigned commentLength = u16(h + 32);
        unsigned localOffset = u32(h + 42);
        std::string name((const char*)h + 46, nameLength);
        at += 46 + nameLength + extraLength + commentLength;
        if (progress) {
            progress(n, count);
        }
        if (name.empty() || name.back() == '/') {
            continue;
        }
        std::string path = target(name);
        if (path.empty()) {
            continue;
        }
        unsigned char local[30];
        _fseeki64(zip, localOffset, SEEK_SET);
        if (fread(local, 1, 30, zip) != 30 || u32(local) != 0x04034b50) {
            return false;
        }
        _fseeki64(zip, (long long)localOffset + 30 + u16(local + 26) + u16(local + 28), SEEK_SET);

        makeDirs(path.substr(0, path.find_last_of("\\/")));
        FILE* file = fopen(path.c_str(), "wb");
        if (file == nullptr) {
            xboxLog("first run: can't write %s", path.c_str());
            return false;
        }
        bool ok = true;
        unsigned left = packed;
        if (method == 0) {
            while (left > 0 && ok) {
                size_t chunk = std::min<size_t>(left, in.size());
                ok = fread(in.data(), 1, chunk, zip) == chunk && fwrite(in.data(), 1, chunk, file) == chunk;
                left -= (unsigned)chunk;
            }
        } else if (method == 8) {
            z_stream z = {};
            inflateInit2(&z, -MAX_WBITS);
            int rc = Z_OK;
            while (ok && rc != Z_STREAM_END) {
                if (z.avail_in == 0) {
                    size_t chunk = std::min<size_t>(left, in.size());
                    if (chunk == 0 || fread(in.data(), 1, chunk, zip) != chunk) {
                        ok = false;
                        break;
                    }
                    left -= (unsigned)chunk;
                    z.next_in = in.data();
                    z.avail_in = (uInt)chunk;
                }
                z.next_out = out.data();
                z.avail_out = (uInt)out.size();
                rc = inflate(&z, Z_NO_FLUSH);
                if (rc != Z_OK && rc != Z_STREAM_END) {
                    ok = false;
                    break;
                }
                size_t produced = out.size() - z.avail_out;
                ok = fwrite(out.data(), 1, produced, file) == produced;
            }
            inflateEnd(&z);
        } else {
            ok = false;
        }
        fclose(file);
        if (!ok) {
            xboxLog("first run: unpacking %s failed", name.c_str());
            return false;
        }
    }
    return true;
}

// Fallout 1in2's Windows-only files: the Xbox app is the engine.
bool pcOnly(const std::string& rel)
{
    static const char* const names[] = {
        "fallout2-ce.exe", "fallout2.exe", "ddraw.dll", "d3dx9_31.dll", "d3dx9_43.dll", "f2_res.dll",
        "f2_res_config.exe", "f2_res_readme.rtf", "undat.exe", "undat.bat", "undat.sh",
    };
    std::string l = lower(rel);
    if (l.compare(0, 6, "utils/") == 0) {
        return true;
    }
    for (const char* name : names) {
        if (l == name) {
            return true;
        }
    }
    return false;
}

void enableMods(const std::string& file, const std::vector<std::string>& mods)
{
    FILE* in = fopen(file.c_str(), "rb");
    if (in == nullptr) {
        return;
    }
    std::string text;
    char buffer[4096];
    size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), in)) > 0) {
        text.append(buffer, n);
    }
    fclose(in);
    for (const std::string& mod : mods) {
        size_t at = text.find(";" + mod);
        if (at != std::string::npos && (at == 0 || text[at - 1] == '\n')) {
            text.erase(at, 1);
        } else if (text.find(mod) == std::string::npos) {
            text += "\r\n" + mod;
        }
    }
    FILE* out = fopen(file.c_str(), "wb");
    if (out != nullptr) {
        fwrite(text.data(), 1, text.size(), out);
        fclose(out);
    }
}

std::string mb(long long bytes)
{
    return std::to_string(bytes >> 20) + " MB";
}

} // namespace

// ---------- the first run ----------

std::string xboxFirstRun(const std::string& prefPath)
{
    std::string fo1;
    std::string fo2;
    std::vector<std::string> roots;
    if (!prefPath.empty()) {
        roots.push_back(prefPath.substr(0, prefPath.size() - 1));
    }
    for (char letter = 'D'; letter <= 'Z'; letter++) {
        roots.push_back(std::string(1, letter) + ":");
    }
    // a drive Xbox apps may not read yet (NTFS without the "ALL APPLICATION PACKAGES" permission)
    std::string lockedDrive;
    for (const std::string& root : roots) {
        WIN32_FIND_DATAA f;
        HANDLE h = FindFirstFileA((root + "\\*").c_str(), &f);
        if (h != INVALID_HANDLE_VALUE) {
            FindClose(h);
        } else if (GetLastError() == ERROR_ACCESS_DENIED && lockedDrive.empty()) {
            lockedDrive = root;
        }
        findGames(root, 0, &fo1, &fo2);
    }
    xboxLog("first run: Fallout at '%s', Fallout 2 at '%s'", fo1.c_str(), fo2.c_str());
    if (!fo2.empty() && exists(fo2 + "\\Fallout1in2\\xbox_setup.txt")) {
        return fo2 + "\\Fallout1in2"; // made before, in a folder the quick search doesn't look in
    }

    Screen screen;
    if (!screen.open()) {
        return std::string();
    }
    std::unique_ptr<Screen, void (*)(Screen*)> closer(&screen, [](Screen* s) { s->close(); });

    if (fo1.empty() || fo2.empty()) {
        std::string missing = fo1.empty() && fo2.empty() ? "Fallout and Fallout 2 were not found"
            : fo1.empty()                               ? "Fallout (the first game) was not found"
                                                        : "Fallout 2 was not found";
        if (!lockedDrive.empty()) {
            xboxLog("first run: %s can't be read by apps", lockedDrive.c_str());
            screen.show("Let's get Fallout ready", "Your USB drive (" + lockedDrive + ") isn't open to Xbox apps yet",
                "On your PC, run Fallout 1in2 Setup with the drive plugged in:|"
                "it opens the drive to Xbox apps and puts the game on it in one go.|"
                "(Game Vault Setup's games-drive step opens it too.)||Press any button to close",
                -1, true);
            screen.waitForButton();
            return std::string();
        }
        screen.show("Let's get Fallout ready", missing,
            "On your PC, copy your Fallout and Fallout 2 game folders onto your USB drive.|"
            "Steam keeps them in  Steam\\steamapps\\common\\Fallout  and  ...\\Fallout 2|"
            "Plug the drive into your Xbox and start Fallout 1in2 again.||Press any button to close",
            -1, true);
        screen.waitForButton();
        return std::string();
    }

    std::string game = fo2 + "\\Fallout1in2";
    std::string title = "Getting Fallout ready";
    auto fail = [&](const std::string& what, const std::string& help) {
        xboxLog("first run failed: %s", what.c_str());
        screen.show("Setup stopped", what, help + "||Press any button to close", -1, true);
        screen.waitForButton();
        return std::string();
    };

    // 1. Fallout 1in2 itself: a zip the player put on the drive, or the newest release from GitHub
    std::string zip;
    std::vector<std::string> zipPlaces = { fo2, fo2.substr(0, fo2.find_last_of('\\')), fo2.substr(0, 2) };
    for (const std::string& place : zipPlaces) {
        for (const std::string& name : listDir(place, false, "Fallout1in2*.zip")) {
            zip = place + "\\" + name;
            break;
        }
        if (!zip.empty()) {
            break;
        }
    }
    bool downloadedZip = false;
    if (zip.empty()) {
        screen.show(title, "Looking for the newest Fallout 1in2...", "", 0, true);
        std::string url = latestAssetUrl(kFo1in2Api, "Fallout1in2");
        if (url.empty()) {
            return fail("Couldn't reach GitHub to download Fallout 1in2",
                "Check the Xbox's internet connection, or put Fallout1in2.zip|"
                "(from github.com/rotators/Fo1in2/releases) on the USB drive and try again.");
        }
        zip = fo2 + "\\Fallout1in2-download.zip";
        if (!download(url, zip, [&](long long done, long long total) {
                screen.show(title, "Downloading Fallout 1in2", mb(done) + (total > 0 ? " of " + mb(total) : ""), total > 0 ? (double)done / total * 0.4 : 0);
            })) {
            return fail("The Fallout 1in2 download didn't finish", "Check the Xbox's internet connection and the room on the USB drive.");
        }
        downloadedZip = true;
    }
    xboxLog("first run: unpacking %s", zip.c_str());

    // 2. unpack it next to Fallout 2's master.dat (keeping the player's settings and saves when run again)
    bool unpacked = unzip(zip, [&](const std::string& name) -> std::string {
            std::string rel = name.compare(0, 12, "Fallout1in2/") == 0 ? name.substr(12) : name;
            if (pcOnly(rel)) {
                return std::string();
            }
            std::string path = game + "\\" + rel;
            std::replace(path.begin(), path.end(), '/', '\\');
            std::string l = lower(rel);
            if ((l == "fallout2.cfg" || l.compare(0, 13, "data/savegame") == 0) && exists(path)) {
                return std::string();
            }
            return path;
        },
        [&](long long done, long long total) { screen.show(title, "Unpacking Fallout 1in2", "", 0.4 + 0.25 * done / std::max(1LL, total)); });
    if (!unpacked) {
        return fail("Unpacking Fallout 1in2 didn't work", "The zip may be damaged, or the USB drive is full.");
    }
    if (downloadedZip) {
        DeleteFileA(zip.c_str()); // 400 MB the drive doesn't need any more
    }

    // 3. Fallout 1's own files, out of its master.dat: the list the mod's undat tool uses
    std::unique_ptr<DatArchive> master = DatArchive::open(fo1 + "\\master.dat");
    FILE* listFile = fopen((game + "\\undat_files.txt").c_str(), "rb");
    if (master == nullptr || listFile == nullptr) {
        if (listFile != nullptr) {
            fclose(listFile);
        }
        return fail("Fallout's master.dat couldn't be read", "Copy the Fallout folder onto the USB drive again.");
    }
    std::vector<std::string> wanted;
    char line[512];
    while (fgets(line, sizeof(line), listFile) != nullptr) {
        std::string s(line);
        while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ')) {
            s.pop_back();
        }
        if (!s.empty()) {
            wanted.push_back(s);
        }
    }
    fclose(listFile);
    std::vector<unsigned char> buffer(1 << 18);
    for (size_t i = 0; i < wanted.size(); i++) {
        screen.show(title, "Unpacking Fallout's own files", std::to_string(i) + " of " + std::to_string(wanted.size()), 0.65 + 0.2 * i / wanted.size());
        std::unique_ptr<DatArchiveStream> in = master->openEntry(wanted[i]);
        if (in == nullptr) {
            xboxLog("first run: not in Fallout's master.dat: %s", wanted[i].c_str());
            continue;
        }
        std::string path = game + "\\data\\" + wanted[i];
        std::replace(path.begin(), path.end(), '/', '\\');
        makeDirs(path.substr(0, path.find_last_of('\\')));
        FILE* out = fopen(path.c_str(), "wb");
        if (out == nullptr) {
            return fail("Couldn't write to the USB drive", "Is it full? It needs about 2 GB free.");
        }
        size_t n;
        while ((n = in->read(buffer.data(), buffer.size())) > 0) {
            fwrite(buffer.data(), 1, n, out);
        }
        fclose(out);
    }

    // 4. the HQ music: nice to have, so a failed download just keeps the original music
    std::string musicUrl = latestAssetUrl(kMusicApi, "fallout_music_hq_");
    if (!musicUrl.empty()) {
        std::string musicZip = fo2 + "\\hqmusic-download.zip";
        bool got = download(musicUrl, musicZip, [&](long long done, long long total) {
            screen.show(title, "Downloading the HQ music", mb(done) + (total > 0 ? " of " + mb(total) : ""), 0.85 + (total > 0 ? 0.12 * done / total : 0));
        });
        if (got) {
            unzip(musicZip, [&](const std::string& name) -> std::string {
                std::string l = lower(name);
                if (l.size() < 4 || l.compare(l.size() - 4, 4, ".acm") != 0) {
                    return std::string();
                }
                return game + "\\data\\sound\\music\\" + name.substr(name.find_last_of('/') + 1);
            },
                nullptr);
            DeleteFileA(musicZip.c_str());
        } else {
            xboxLog("first run: HQ music download failed, keeping the original music");
        }
    }

    // 5. extra details: footprints in sand, animated force fields, the redone Vault 15 entrance
    enableMods(game + "\\mods\\mods_order.txt", { "fo1_footprints", "fo1_alternative_forcefields", "fo1_v15_rework" });
    FILE* marker = fopen((game + "\\xbox_setup.txt").c_str(), "wb");
    if (marker != nullptr) {
        fprintf(marker, "Made on the Xbox by Fallout 1in2's first run\r\nFallout: %s\r\nFallout 2: %s\r\nHQ music: %s\r\n", fo1.c_str(), fo2.c_str(), musicUrl.empty() ? "no" : "yes");
        fclose(marker);
    }
    screen.show("Ready!", "Starting Fallout...", "", 1, true);
    SDL_Delay(800);
    xboxLog("first run: done, game folder %s", game.c_str());
    return game;
}

} // namespace fallout
