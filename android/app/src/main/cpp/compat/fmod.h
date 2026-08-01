#pragma once
// FMOD 3, as much of it as Blitzkrieg uses.
//
// The engine drives this as a channel mixer: it loads a sample into memory,
// plays it on a free channel, sets that channel's volume, pan and 3D position,
// and asks whether it is still playing. Music is a stream read from a file
// with a callback when it ends. Forty-two entry points in total, and this
// header carries exactly those -- it is not a reimplementation of FMOD, it is
// the surface Blitzkrieg calls.
//
// The implementation behind it is bk1_fmod_audio.cpp: an AAudio output stream and
// a mixer. AAudio is part of the NDK, so the port keeps its property of having
// no third-party dependency at all.
#include "bk1_win32_types.h"

// ---------------------------------------------------------------------------
// Handles
// ---------------------------------------------------------------------------
// Opaque to the engine: it stores them, passes them back, and compares against
// null. Their contents belong to the implementation.
typedef struct FSOUND_SAMPLE FSOUND_SAMPLE;
typedef struct FSOUND_STREAM FSOUND_STREAM;
typedef struct FSOUND_DSPUNIT FSOUND_DSPUNIT;

// ---------------------------------------------------------------------------
// Channel and sample selectors
// ---------------------------------------------------------------------------
#define FSOUND_FREE             (-1)    // pick any channel that is not playing
#define FSOUND_UNMANAGED        (-1)    // the caller owns the sample
#define FSOUND_ALL              (-3)
#define FSOUND_STEREOPAN        (-1)    // pan value meaning "leave stereo alone"

// ---------------------------------------------------------------------------
// Sample and stream modes, or-ed together
// ---------------------------------------------------------------------------
#define FSOUND_LOOP_OFF         0x00000001
#define FSOUND_LOOP_NORMAL      0x00000002
#define FSOUND_LOOP_BIDI        0x00000004
#define FSOUND_8BITS            0x00000008
#define FSOUND_16BITS           0x00000010
#define FSOUND_MONO             0x00000020
#define FSOUND_STEREO           0x00000040
#define FSOUND_UNSIGNED         0x00000080
#define FSOUND_SIGNED           0x00000100
#define FSOUND_DELTA            0x00000200
#define FSOUND_IT214            0x00000400
#define FSOUND_IT215            0x00000800
#define FSOUND_HW3D             0x00001000
#define FSOUND_2D               0x00002000
#define FSOUND_STREAMABLE       0x00004000
#define FSOUND_LOADMEMORY       0x00008000
#define FSOUND_LOADRAW          0x00010000
#define FSOUND_MPEGACCURATE     0x00020000
#define FSOUND_FORCEMONO        0x00040000
#define FSOUND_HW2D             0x00080000
#define FSOUND_3D               0x00100000
#define FSOUND_32BITS           0x00200000
#define FSOUND_NORMAL           ( FSOUND_16BITS | FSOUND_SIGNED | FSOUND_MONO )

// ---------------------------------------------------------------------------
// Output drivers
// ---------------------------------------------------------------------------
// The engine probes these and settles on one. On Android there is a single
// output -- Oboe, which itself picks AAudio or OpenSL ES -- so every value the
// engine may choose lands on the same device. The names are kept because the
// engine writes the chosen one into the user's profile.
enum FSOUND_OUTPUTTYPES
{
    FSOUND_OUTPUT_NOSOUND,
    FSOUND_OUTPUT_WINMM,
    FSOUND_OUTPUT_DSOUND,
    FSOUND_OUTPUT_A3D,
    FSOUND_OUTPUT_OSS,
    FSOUND_OUTPUT_ESD,
    FSOUND_OUTPUT_ALSA,
    FSOUND_OUTPUT_ASIO,
    FSOUND_OUTPUT_XBOX,
    FSOUND_OUTPUT_PS2,
    FSOUND_OUTPUT_MAC,
    FSOUND_OUTPUT_GC,
    FSOUND_OUTPUT_NOSOUND_NONREALTIME
};

enum FSOUND_MIXERTYPES
{
    FSOUND_MIXER_AUTODETECT,
    FSOUND_MIXER_BLENDMODE,
    FSOUND_MIXER_MMXP5,
    FSOUND_MIXER_MMXP6,
    FSOUND_MIXER_QUALITY_AUTODETECT,
    FSOUND_MIXER_QUALITY_FPU,
    FSOUND_MIXER_QUALITY_MMXP5,
    FSOUND_MIXER_QUALITY_MMXP6,
    FSOUND_MIXER_MAX
};

// ---------------------------------------------------------------------------
// Driver capability bits, as returned by FSOUND_GetDriverCaps
// ---------------------------------------------------------------------------
#define FSOUND_CAPS_HARDWARE            0x00000001
#define FSOUND_CAPS_EAX2                0x00000002
#define FSOUND_CAPS_EAX3                0x00000008
#define FSOUND_CAPS_GEOMETRY_OCCLUSIONS  0x00000004
#define FSOUND_CAPS_GEOMETRY_REFLECTIONS 0x00000010

// Initialisation flags.
#define FSOUND_INIT_USEDEFAULTMIDISYNTH 0x00000001
#define FSOUND_INIT_GLOBALFOCUS         0x00000002
#define FSOUND_INIT_ENABLEOUTPUTFX      0x00000004
#define FSOUND_INIT_ACCURATEVULEVELS    0x00000008
#define FSOUND_INIT_PS2_DISABLECORE0REVERB 0x00000010

// The errors the engine reads back from FSOUND_GetError.
enum FMOD_ERRORS
{
    FMOD_ERR_NONE,
    FMOD_ERR_BUSY,
    FMOD_ERR_UNINITIALIZED,
    FMOD_ERR_INIT,
    FMOD_ERR_ALLOCATED,
    FMOD_ERR_PLAY,
    FMOD_ERR_OUTPUT_FORMAT,
    FMOD_ERR_COOPERATIVELEVEL,
    FMOD_ERR_CREATEBUFFER,
    FMOD_ERR_FILE_NOTFOUND,
    FMOD_ERR_FILE_FORMAT,
    FMOD_ERR_FILE_BAD,
    FMOD_ERR_MEMORY,
    FMOD_ERR_VERSION,
    FMOD_ERR_INVALID_PARAM,
    FMOD_ERR_NO_EAX,
    FMOD_ERR_CHANNEL_ALLOC,
    FMOD_ERR_RECORD,
    FMOD_ERR_MEDIAPLAYER,
    FMOD_ERR_CDDEVICE
};

// The version the engine checks against the header it was built with.
#define FMOD_VERSION 3.75f

// The callback a stream fires when it runs out, and the one for synch points.
typedef signed char (*FSOUND_STREAMCALLBACK)( FSOUND_STREAM *pStream, void *pBuff,
                                              int nLen, int nParam );

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------------------
// Setup and teardown
// ---------------------------------------------------------------------------
signed char FSOUND_SetOutput( int nOutputType );
signed char FSOUND_SetDriver( int nDriver );
signed char FSOUND_SetHWND( void *pHWnd );
signed char FSOUND_Init( int nMixRate, int nMaxSoftwareChannels, unsigned int nFlags );
void        FSOUND_Close( void );

int         FSOUND_GetNumDrivers( void );
const char *FSOUND_GetDriverName( int nDriver );
signed char FSOUND_GetDriverCaps( int nDriver, unsigned int *pnCaps );
int         FSOUND_GetOutputHandle( void );
int         FSOUND_GetMixer( void );
int         FSOUND_GetError( void );
float       FSOUND_GetVersion( void );
int         FSOUND_GetChannelsPlaying( void );

// ---------------------------------------------------------------------------
// Samples
// ---------------------------------------------------------------------------
// The engine always loads from a memory block it has already read, passing
// FSOUND_LOADMEMORY and the block's length.
FSOUND_SAMPLE *FSOUND_Sample_Load( int nIndex, const char *pData,
                                   unsigned int nMode, int nLength );
void        FSOUND_Sample_Free( FSOUND_SAMPLE *pSample );
signed char FSOUND_Sample_SetLoopMode( FSOUND_SAMPLE *pSample, unsigned int nLoopMode );
signed char FSOUND_Sample_SetLoopPoints( FSOUND_SAMPLE *pSample, int nLoopStart,
                                         int nLoopEnd );
signed char FSOUND_Sample_SetMinMaxDistance( FSOUND_SAMPLE *pSample, float fMin,
                                             float fMax );
signed char FSOUND_Sample_GetDefaults( FSOUND_SAMPLE *pSample, int *pnDefFreq,
                                       int *pnDefVol, int *pnDefPan, int *pnDefPri );
unsigned int FSOUND_Sample_GetLength( FSOUND_SAMPLE *pSample );

// ---------------------------------------------------------------------------
// Playing
// ---------------------------------------------------------------------------
int  FSOUND_PlaySound( int nChannel, FSOUND_SAMPLE *pSample );
int  FSOUND_PlaySoundEx( int nChannel, FSOUND_SAMPLE *pSample,
                         FSOUND_DSPUNIT *pDSP, signed char bPaused );
int  FSOUND_PlaySound3DAttrib( int nChannel, FSOUND_SAMPLE *pSample, int nFreq,
                               int nVol, int nPan, float *pPos, float *pVel );
signed char FSOUND_StopSound( int nChannel );
signed char FSOUND_SetPaused( int nChannel, signed char bPaused );
signed char FSOUND_SetVolume( int nChannel, int nVolume );
signed char FSOUND_SetPan( int nChannel, int nPan );
signed char FSOUND_IsPlaying( int nChannel );
signed char FSOUND_SetCurrentPosition( int nChannel, unsigned int nOffset );
unsigned int FSOUND_GetCurrentPosition( int nChannel );
FSOUND_SAMPLE *FSOUND_GetCurrentSample( int nChannel );

// ---------------------------------------------------------------------------
// Three dimensions
// ---------------------------------------------------------------------------
signed char FSOUND_3D_SetAttributes( int nChannel, float *pPos, float *pVel );
void        FSOUND_3D_Listener_SetAttributes( float *pPos, float *pVel,
                                              float fFrontX, float fFrontY, float fFrontZ,
                                              float fTopX, float fTopY, float fTopZ );
void        FSOUND_3D_Listener_SetDistanceFactor( float fScale );
void        FSOUND_3D_Listener_SetRolloffFactor( float fScale );
void        FSOUND_3D_Update( void );

// ---------------------------------------------------------------------------
// Streams
// ---------------------------------------------------------------------------
FSOUND_STREAM *FSOUND_Stream_OpenFile( const char *pszFileName, unsigned int nMode,
                                       int nLength );
int         FSOUND_Stream_Play( int nChannel, FSOUND_STREAM *pStream );
signed char FSOUND_Stream_Close( FSOUND_STREAM *pStream );
signed char FSOUND_Stream_SetEndCallback( FSOUND_STREAM *pStream,
                                          FSOUND_STREAMCALLBACK pCallback, int nParam );
signed char FSOUND_Stream_SetSynchCallback( FSOUND_STREAM *pStream,
                                            FSOUND_STREAMCALLBACK pCallback, int nParam );

#ifdef __cplusplus
}
#endif

// The Android side drives the mixer from its own thread; these are not FMOD's
// and exist only for the port.
#ifdef __cplusplus
// Called once the asset directory is known, so streams can find their files.
void Bk1SoundSetRootDirectory( const char *pszPath );
// Releases the output device when the activity loses focus, and takes it back
// when focus returns. Android reclaims audio from a backgrounded app anyway;
// doing it deliberately keeps the mixer's idea of the world honest.
void Bk1SoundSetSuspended( bool bSuspended );
#endif
