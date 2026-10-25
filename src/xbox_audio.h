// Copyright (c) 2026 Skitzelles - part of Game Vault, https://github.com/skitzellez/xbox-game-vault
// Modified version of Fallout 2 Community Edition (Sustainable Use License, see LICENSE.md): Xbox port.
#ifndef FALLOUT_XBOX_AUDIO_H_
#define FALLOUT_XBOX_AUDIO_H_

namespace fallout {

// Fills the buffer with the next mixed samples (16-bit, interleaved), like an SDL audio callback.
typedef void (*XboxAudioMixer)(void* userData, unsigned char* stream, int length);

// Starts sound output through XAudio2 (Xbox app only).
bool xboxAudioStart(int sampleRate, int channels, XboxAudioMixer mixer);

// Paused: silence is played instead of the game's mix.
void xboxAudioPause(bool paused);

} // namespace fallout

#endif /* FALLOUT_XBOX_AUDIO_H_ */
