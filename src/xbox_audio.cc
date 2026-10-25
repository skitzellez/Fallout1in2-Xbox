// Copyright (c) 2026 Skitzelles - part of Game Vault, https://github.com/skitzellez/xbox-game-vault
// Modified version of Fallout 2 Community Edition (Sustainable Use License, see LICENSE.md): Xbox port.
//
// Sound output for the Xbox app: SDL's audio device never finishes opening in Xbox (UWP) apps, so the game's
// mixer is played through XAudio2 instead. A feeder thread asks the mixer for the next block of samples
// whenever XAudio2 has room, the same way SDL's audio callback would.
#include "xbox_audio.h"

#include <windows.h>
#include <objbase.h>
#include <xaudio2.h>

#include "xbox_pad.h"

namespace fallout {

static const int kBlockFrames = 1024; // ~23 ms at 44.1 kHz
static const int kBlocks = 6;
static const int kQueued = 3; // ~70 ms queued: low latency without gaps

typedef HRESULT(WINAPI* XAudio2CreateFunction)(IXAudio2**, UINT32, XAUDIO2_PROCESSOR);

static IXAudio2SourceVoice* gVoice = nullptr;
static XboxAudioMixer gMixer = nullptr;
static int gFrameBytes = 4;
static volatile bool gPaused = false;
static unsigned char* gBlocks[kBlocks];

static DWORD WINAPI feederThread(LPVOID)
{
    int next = 0;
    int blockBytes = kBlockFrames * gFrameBytes;
    for (;;) {
        XAUDIO2_VOICE_STATE state;
        gVoice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
        if (state.BuffersQueued >= kQueued) {
            Sleep(2);
            continue;
        }
        if (gPaused) {
            memset(gBlocks[next], 0, blockBytes);
        } else {
            gMixer(nullptr, gBlocks[next], blockBytes);
        }
        XAUDIO2_BUFFER buffer = {};
        buffer.AudioBytes = blockBytes;
        buffer.pAudioData = gBlocks[next];
        gVoice->SubmitSourceBuffer(&buffer, nullptr);
        next = (next + 1) % kBlocks;
    }
    return 0;
}

bool xboxAudioStart(int sampleRate, int channels, XboxAudioMixer mixer)
{
    HMODULE library = LoadLibraryExW(L"xaudio2_9.dll", nullptr, 0);
    XAudio2CreateFunction create = library != nullptr
        ? (XAudio2CreateFunction)(void*)GetProcAddress(library, "XAudio2Create")
        : nullptr;
    if (create == nullptr) {
        xboxLog("audio: xaudio2_9.dll not available (error %lu)", GetLastError());
        return false;
    }

    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    IXAudio2* xaudio = nullptr;
    HRESULT hr = create(&xaudio, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(hr)) {
        xboxLog("audio: XAudio2Create failed 0x%08lx", hr);
        return false;
    }

    IXAudio2MasteringVoice* master = nullptr;
    hr = xaudio->CreateMasteringVoice(&master, XAUDIO2_DEFAULT_CHANNELS, XAUDIO2_DEFAULT_SAMPLERATE, 0, nullptr, nullptr, AudioCategory_GameEffects);
    if (FAILED(hr)) {
        xboxLog("audio: CreateMasteringVoice failed 0x%08lx", hr);
        return false;
    }

    gFrameBytes = channels * 2;
    WAVEFORMATEX format = {};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = (WORD)channels;
    format.nSamplesPerSec = sampleRate;
    format.wBitsPerSample = 16;
    format.nBlockAlign = (WORD)gFrameBytes;
    format.nAvgBytesPerSec = sampleRate * gFrameBytes;
    hr = xaudio->CreateSourceVoice(&gVoice, &format, 0, XAUDIO2_DEFAULT_FREQ_RATIO, nullptr, nullptr, nullptr);
    if (FAILED(hr)) {
        xboxLog("audio: CreateSourceVoice failed 0x%08lx", hr);
        return false;
    }

    for (int i = 0; i < kBlocks; i++) {
        gBlocks[i] = new unsigned char[kBlockFrames * gFrameBytes];
    }
    gMixer = mixer;
    gVoice->Start(0, XAUDIO2_COMMIT_NOW);

    HANDLE thread = CreateThread(nullptr, 0, feederThread, nullptr, 0, nullptr);
    if (thread == nullptr) {
        xboxLog("audio: CreateThread failed (error %lu)", GetLastError());
        return false;
    }
    SetThreadPriority(thread, THREAD_PRIORITY_HIGHEST);
    CloseHandle(thread);
    xboxLog("audio: XAudio2 output running (%d Hz, %d channels)", sampleRate, channels);
    return true;
}

void xboxAudioPause(bool paused)
{
    gPaused = paused;
}

} // namespace fallout
