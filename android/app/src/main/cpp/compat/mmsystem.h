#pragma once
// Stands in for the MSVC multimedia header. The engine uses the millisecond
// timer from it, and names WAVEFORMATEX where it describes sound buffers.
#include "bk1_win32_types.h"

typedef UINT MMRESULT;

#define MMSYSERR_NOERROR 0
#define TIMERR_NOERROR   0

// timeGetTime and GetTickCount are the same monotonic millisecond clock here.
inline DWORD timeGetTime() { return GetTickCount(); }

inline MMRESULT timeBeginPeriod( UINT ) { return TIMERR_NOERROR; }
inline MMRESULT timeEndPeriod( UINT ) { return TIMERR_NOERROR; }

#define WAVE_FORMAT_PCM 1

typedef struct tWAVEFORMATEX {
    WORD  wFormatTag;
    WORD  nChannels;
    DWORD nSamplesPerSec;
    DWORD nAvgBytesPerSec;
    WORD  nBlockAlign;
    WORD  wBitsPerSample;
    WORD  cbSize;
} WAVEFORMATEX, *PWAVEFORMATEX, *LPWAVEFORMATEX;

typedef struct waveformat_tag {
    WORD  wFormatTag;
    WORD  nChannels;
    DWORD nSamplesPerSec;
    DWORD nAvgBytesPerSec;
    WORD  nBlockAlign;
} WAVEFORMAT, *LPWAVEFORMAT;

typedef struct pcmwaveformat_tag {
    WAVEFORMAT wf;
    WORD       wBitsPerSample;
} PCMWAVEFORMAT, *LPPCMWAVEFORMAT;
