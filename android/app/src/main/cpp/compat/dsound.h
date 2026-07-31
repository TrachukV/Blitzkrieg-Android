#pragma once
// DirectSound. The engine's only use is to hand the sound device to Bink's
// audio, as an opaque pointer it never dereferences -- the port's audio is
// owned by the platform decoder, so there is no device to describe.
#include "bk1_win32_types.h"

struct IDirectSound;
typedef struct IDirectSound *LPDIRECTSOUND;
typedef struct IDirectSoundBuffer *LPDIRECTSOUNDBUFFER;
