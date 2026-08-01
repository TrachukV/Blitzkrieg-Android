// The Android side of the port: the surface the game draws on, the frame loop
// that drives it, and the touch handling that feeds the engine's input.
//
// Everything the engine believes about its environment is answered by the
// compatibility layer -- a window, a cursor, a keyboard, a Direct3D device.
// This file is what stands behind those answers: it owns the EGL surface, it
// tells the window layer how big the surface is, it turns fingers into the
// mouse events the engine's bindings already read, and it presents each frame.
#include <android/log.h>
#include <android/native_window.h>
#include <android_native_app_glue.h>

#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "compat/bk1_win32_window.h"
#include "compat/bk1_win32_keys.h"
#include "compat/bk1_win32_registry.h"
#include "compat/dinput.h"
#include "compat/fmod.h"
#include "bk1_game_startup.h"
#include "bk1_touch_gestures.h"

#define LOG_TAG "Blitzkrieg"
#define LOGI( ... ) __android_log_print( ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__ )
#define LOGE( ... ) __android_log_print( ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__ )

namespace {

struct SAppState
{
    android_app *pApp;

    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    int        nWidth;
    int        nHeight;
    bool       bReady;

    // The finger that is currently down, and where it started. A tap becomes a
    // click; a drag moves the cursor first so the engine sees the press where
    // the finger is.
    int   nActivePointer;
    float fLastX;
    float fLastY;

    // What the gesture layer needs to remember between events.
    float fPressX;
    float fPressY;
    long long nPressTime;
    bool  bMovedPastSlop;
    bool  bLongPressFired;
    bool  bLeftDown;
    bool  bTwoFinger;
    float fPinchCentreX;
    float fPinchCentreY;
    float fPinchSpread;
    bool  bScrollHeld[4];
    bool  bScrollLogged;
    int   nDensityDpi;

    // The engine starts on the first frame that has a surface, and the data
    // directory has to be known before then.
    bool  bGameStarted;
    bool  bGameFailed;
    char  szDataDirectory[1024];

    SAppState()
        : pApp( 0 ), display( EGL_NO_DISPLAY ), surface( EGL_NO_SURFACE ),
          context( EGL_NO_CONTEXT ), nWidth( 0 ), nHeight( 0 ), bReady( false ),
          nActivePointer( -1 ), fLastX( 0.0f ), fLastY( 0.0f ),
          fPressX( 0.0f ), fPressY( 0.0f ), nPressTime( 0 ),
          bMovedPastSlop( false ), bLongPressFired( false ), bLeftDown( false ),
          bTwoFinger( false ), fPinchCentreX( 0.0f ), fPinchCentreY( 0.0f ),
          fPinchSpread( 0.0f ), bScrollLogged( false ), nDensityDpi( 0 ),
          bGameStarted( false ), bGameFailed( false )
    {
        szDataDirectory[0] = 0;
        for ( int i = 0; i < 4; ++i )
            bScrollHeld[i] = false;
    }
};

SAppState g_state;

// The gesture machine. Declared here rather than beside the touch handling
// because the surface reports the display density before the first event.
NBk1Touch::CRecogniser g_gestures;

// ---------------------------------------------------------------------------
// The surface
// ---------------------------------------------------------------------------
bool CreateSurface( SAppState *pState )
{
    if ( pState->pApp == 0 || pState->pApp->window == 0 )
        return false;

    pState->display = eglGetDisplay( EGL_DEFAULT_DISPLAY );
    if ( pState->display == EGL_NO_DISPLAY )
    {
        LOGE( "no EGL display" );
        return false;
    }
    if ( !eglInitialize( pState->display, 0, 0 ) )
    {
        LOGE( "eglInitialize failed" );
        return false;
    }

    // A depth and stencil buffer, because the engine asks for both: the ground
    // draws with depth off but units do not, and the stencil is used for the
    // shadow and fog work.
    const EGLint attributes[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
        EGL_BLUE_SIZE,       8,
        EGL_GREEN_SIZE,      8,
        EGL_RED_SIZE,        8,
        EGL_ALPHA_SIZE,      8,
        EGL_DEPTH_SIZE,      24,
        EGL_STENCIL_SIZE,    8,
        EGL_NONE
    };

    EGLConfig config;
    EGLint    nConfigs = 0;
    if ( !eglChooseConfig( pState->display, attributes, &config, 1, &nConfigs ) ||
         nConfigs < 1 )
    {
        // Fall back to a 16-bit depth buffer, which some older devices are
        // limited to.
        const EGLint fallback[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
            EGL_BLUE_SIZE,       8,
            EGL_GREEN_SIZE,      8,
            EGL_RED_SIZE,        8,
            EGL_DEPTH_SIZE,      16,
            EGL_STENCIL_SIZE,    8,
            EGL_NONE
        };
        if ( !eglChooseConfig( pState->display, fallback, &config, 1, &nConfigs ) ||
             nConfigs < 1 )
        {
            LOGE( "no EGL config with a depth and stencil buffer" );
            return false;
        }
    }

    EGLint nFormat = 0;
    eglGetConfigAttrib( pState->display, config, EGL_NATIVE_VISUAL_ID, &nFormat );
    ANativeWindow_setBuffersGeometry( pState->pApp->window, 0, 0, nFormat );

    pState->surface = eglCreateWindowSurface( pState->display, config,
                                              pState->pApp->window, 0 );
    if ( pState->surface == EGL_NO_SURFACE )
    {
        LOGE( "eglCreateWindowSurface failed" );
        return false;
    }

    const EGLint contextAttributes[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    pState->context = eglCreateContext( pState->display, config, EGL_NO_CONTEXT,
                                        contextAttributes );
    if ( pState->context == EGL_NO_CONTEXT )
    {
        LOGE( "eglCreateContext failed" );
        return false;
    }
    if ( !eglMakeCurrent( pState->display, pState->surface, pState->surface,
                          pState->context ) )
    {
        LOGE( "eglMakeCurrent failed" );
        return false;
    }

    eglQuerySurface( pState->display, pState->surface, EGL_WIDTH, &pState->nWidth );
    eglQuerySurface( pState->display, pState->surface, EGL_HEIGHT, &pState->nHeight );

    // The engine asks the window layer how big its client area is; this is the
    // answer, and it is what the pre-transformed vertex path maps against.
    Bk1SetClientSize( pState->nWidth, pState->nHeight );

    // And how dense the display is, which is what turns the double-tap
    // distance into pixels. Without it the slop would be a phone's on a
    // tablet.
    if ( pState->pApp->config != 0 )
    {
        const int32_t nDensity = AConfiguration_getDensity( pState->pApp->config );
        if ( nDensity > 0 && nDensity != ACONFIGURATION_DENSITY_ANY &&
             nDensity != ACONFIGURATION_DENSITY_NONE )
        {
            Bk1SetDisplayDensity( nDensity );
            g_gestures.SetDisplayDensity( (int)nDensity );
        }
    }

    // Present on the display's own rhythm. The game's frame pacing is its own,
    // and asking for one swap an interval is what keeps it at the refresh rate
    // rather than ahead of it.
    eglSwapInterval( pState->display, 1 );

    LOGI( "surface %dx%d, renderer %s", pState->nWidth, pState->nHeight,
          (const char *)glGetString( GL_RENDERER ) );
    pState->bReady = true;
    return true;
}

void DestroySurface( SAppState *pState )
{
    if ( pState->display != EGL_NO_DISPLAY )
    {
        eglMakeCurrent( pState->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
        if ( pState->context != EGL_NO_CONTEXT )
            eglDestroyContext( pState->display, pState->context );
        if ( pState->surface != EGL_NO_SURFACE )
            eglDestroySurface( pState->display, pState->surface );
        eglTerminate( pState->display );
    }
    pState->display = EGL_NO_DISPLAY;
    pState->context = EGL_NO_CONTEXT;
    pState->surface = EGL_NO_SURFACE;
    pState->bReady = false;
}

// ---------------------------------------------------------------------------
// Touch
// ---------------------------------------------------------------------------
// The gestures themselves live in bk1_touch_gestures.cpp, with no Android in
// them, because two-finger gestures cannot be injected on a stock emulator --
// /dev/input refuses a non-root writer -- and the only way to test them is to
// drive the machine directly. This file does the two things that do need
// Android: it turns motion events into the machine's own vocabulary, and it
// performs what the machine decides.
long long NowMilliseconds()
{
    timespec now;
    clock_gettime( CLOCK_MONOTONIC, &now );
    return (long long)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

// Performing an action means pushing it at the devices the engine's bindings
// read -- a buffered mouse and a keyboard -- so whatever the player has bound
// in the options still applies. Nothing here reaches into the engine.
void Perform( const NBk1Touch::SAction &action )
{
    switch ( action.kind )
    {
    case NBk1Touch::ACTION_CURSOR_TO:
        Bk1SetCursorPos( action.nX, action.nY );
        break;
    case NBk1Touch::ACTION_MOUSE_MOVE:
        Bk1PushInputEvent( BK1_INPUT_MOUSE, DIMOFS_X, (DWORD)action.nX );
        Bk1PushInputEvent( BK1_INPUT_MOUSE, DIMOFS_Y, (DWORD)action.nY );
        break;
    case NBk1Touch::ACTION_LEFT_DOWN:
        Bk1PushInputEvent( BK1_INPUT_MOUSE, DIMOFS_BUTTON0, 0x80 );
        break;
    case NBk1Touch::ACTION_LEFT_UP:
        Bk1PushInputEvent( BK1_INPUT_MOUSE, DIMOFS_BUTTON0, 0 );
        break;
    case NBk1Touch::ACTION_RIGHT_CLICK:
        Bk1PushInputEvent( BK1_INPUT_MOUSE, DIMOFS_BUTTON1, 0x80 );
        Bk1PushInputEvent( BK1_INPUT_MOUSE, DIMOFS_BUTTON1, 0 );
        LOGI( "gesture: hold -> right click at %d, %d", action.nX, action.nY );
        break;
    case NBk1Touch::ACTION_WHEEL:
        Bk1PushInputEvent( BK1_INPUT_MOUSE, DIMOFS_Z, (DWORD)action.nX );
        break;
    case NBk1Touch::ACTION_SCROLL_LEFT:
        Bk1PushInputEvent( BK1_INPUT_KEYBOARD, DIK_LEFT, action.nX ? 0x80 : 0 );
        break;
    case NBk1Touch::ACTION_SCROLL_RIGHT:
        Bk1PushInputEvent( BK1_INPUT_KEYBOARD, DIK_RIGHT, action.nX ? 0x80 : 0 );
        break;
    case NBk1Touch::ACTION_SCROLL_UP:
        Bk1PushInputEvent( BK1_INPUT_KEYBOARD, DIK_UP, action.nX ? 0x80 : 0 );
        break;
    case NBk1Touch::ACTION_SCROLL_DOWN:
        Bk1PushInputEvent( BK1_INPUT_KEYBOARD, DIK_DOWN, action.nX ? 0x80 : 0 );
        break;
    }
}

void PerformAll( const NBk1Touch::SAction *pActions, int nCount )
{
    for ( int i = 0; i < nCount; ++i )
        Perform( pActions[i] );
}

int HandleInput( android_app *pApp, AInputEvent *pEvent )
{
    SAppState *pState = (SAppState *)pApp->userData;
    if ( pState == 0 || AInputEvent_getType( pEvent ) != AINPUT_EVENT_TYPE_MOTION )
        return 0;

    const int32_t nAction = AMotionEvent_getAction( pEvent );
    const int32_t nKind = nAction & AMOTION_EVENT_ACTION_MASK;
    const size_t  nCount = AMotionEvent_getPointerCount( pEvent );

    NBk1Touch::SFinger fingers[8];
    const size_t nFingers = ( nCount < 8 ) ? nCount : 8;
    for ( size_t i = 0; i < nFingers; ++i )
    {
        fingers[i].nId = AMotionEvent_getPointerId( pEvent, i );
        // Into the engine's coordinates. It draws at its own size and the
        // device scales that onto the surface; a finger has to travel the
        // same road backwards or every tap lands somewhere else.
        int nEngineX = 0, nEngineY = 0;
        Bk1SurfaceToEngine( (int)AMotionEvent_getX( pEvent, i ),
                            (int)AMotionEvent_getY( pEvent, i ),
                            &nEngineX, &nEngineY );
        fingers[i].fX = (float)nEngineX;
        fingers[i].fY = (float)nEngineY;
    }

    NBk1Touch::EPhase phase;
    switch ( nKind )
    {
    case AMOTION_EVENT_ACTION_DOWN:         phase = NBk1Touch::PHASE_DOWN; break;
    case AMOTION_EVENT_ACTION_POINTER_DOWN: phase = NBk1Touch::PHASE_SECOND_DOWN; break;
    case AMOTION_EVENT_ACTION_MOVE:         phase = NBk1Touch::PHASE_MOVE; break;
    case AMOTION_EVENT_ACTION_POINTER_UP:   phase = NBk1Touch::PHASE_SECOND_UP; break;
    case AMOTION_EVENT_ACTION_UP:           phase = NBk1Touch::PHASE_UP; break;
    case AMOTION_EVENT_ACTION_CANCEL:       phase = NBk1Touch::PHASE_CANCEL; break;
    default:                                return 0;
    }

    NBk1Touch::SAction actions[NBk1Touch::MAX_ACTIONS];
    const int nProduced = g_gestures.Handle( phase, fingers, nFingers,
                                             NowMilliseconds(), actions );
    PerformAll( actions, nProduced );

    if ( phase == NBk1Touch::PHASE_DOWN )
    {
        static bool bReported = false;
        if ( !bReported )
        {
            bReported = true;
            POINT cursor = { 0, 0 };
            GetCursorPos( &cursor );
            LOGI( "touch reaches the engine: cursor at %ld, %ld",
                  (long)cursor.x, (long)cursor.y );
        }
    }
    return 1;
}

// ---------------------------------------------------------------------------
// What is actually in the frame
// ---------------------------------------------------------------------------
// The emulator's screencap does not capture this surface -- clearing the whole
// screen to magenta produced a capture without a magenta pixel in it, byte for
// byte identical to the one before. So the only trustworthy way to see what
// the port has drawn is to ask the context itself.
//
// Once a second, a coarse grid of pixels is read back and the commonest
// colours reported. Black everywhere means nothing is being drawn; anything
// else is the frame, whatever a screenshot may claim.
// The whole frame, once, written out where it can be fetched from. Raw RGBA
// rather than an image format: there is no encoder here, and the host can turn
// four bytes a pixel into a picture without one.
void DumpFrame()
{
    const size_t nBytes = (size_t)g_state.nWidth * g_state.nHeight * 4;
    unsigned char *pPixels = (unsigned char *)malloc( nBytes );
    if ( pPixels == 0 )
        return;
    glReadPixels( 0, 0, g_state.nWidth, g_state.nHeight, GL_RGBA, GL_UNSIGNED_BYTE,
                  pPixels );

    char szPath[1200];
    snprintf( szPath, sizeof( szPath ), "%s/frame.rgba", g_state.szDataDirectory );
    if ( FILE *pFile = fopen( szPath, "wb" ) )
    {
        fwrite( pPixels, 1, nBytes, pFile );
        fclose( pFile );
        LOGI( "frame written: %s, %dx%d", szPath, g_state.nWidth, g_state.nHeight );
    }
    else
    {
        LOGE( "cannot write %s", szPath );
    }
    free( pPixels );
}

void SampleFrame()
{
    // One dump, a few seconds in, once the interface has settled.
    static int nFrames = 0;
    if ( ++nFrames == 240 )
        DumpFrame();

    static timespec last = { 0, 0 };
    timespec now;
    clock_gettime( CLOCK_MONOTONIC, &now );
    if ( last.tv_sec != 0 && now.tv_sec - last.tv_sec < 2 )
        return;
    last = now;

    const int nAcross = 12, nDown = 8;
    struct SCount { unsigned int nColour; int nTimes; };
    SCount counts[nAcross * nDown];
    int nDistinct = 0;

    for ( int y = 0; y < nDown; ++y )
    {
        for ( int x = 0; x < nAcross; ++x )
        {
            unsigned char rgba[4] = { 0, 0, 0, 0 };
            glReadPixels( g_state.nWidth * ( 2 * x + 1 ) / ( 2 * nAcross ),
                          g_state.nHeight * ( 2 * y + 1 ) / ( 2 * nDown ),
                          1, 1, GL_RGBA, GL_UNSIGNED_BYTE, rgba );
            const unsigned int nColour =
                ( (unsigned int)rgba[0] << 16 ) | ( (unsigned int)rgba[1] << 8 ) | rgba[2];
            int i = 0;
            for ( ; i < nDistinct; ++i )
            {
                if ( counts[i].nColour == nColour ) { ++counts[i].nTimes; break; }
            }
            if ( i == nDistinct )
            {
                counts[nDistinct].nColour = nColour;
                counts[nDistinct].nTimes = 1;
                ++nDistinct;
            }
        }
    }

    // The three commonest, which is enough to tell a blank frame from a drawn one.
    for ( int nShown = 0; nShown < 3 && nShown < nDistinct; ++nShown )
    {
        int nBest = 0;
        for ( int i = 1; i < nDistinct; ++i )
        {
            if ( counts[i].nTimes > counts[nBest].nTimes )
                nBest = i;
        }
        __android_log_print( ANDROID_LOG_INFO, LOG_TAG, "frame: #%06x on %d of %d samples",
                             counts[nBest].nColour, counts[nBest].nTimes, nAcross * nDown );
        counts[nBest].nTimes = -1;
    }
}

// ---------------------------------------------------------------------------
// Frame rate
// ---------------------------------------------------------------------------
// The port is meant to hold the display's refresh rate, so it is measured
// rather than assumed. This reports the average and the worst frame of each
// second: an average of 60 with a 40ms straggler is a stutter somebody will
// feel, and only the second number shows it.
void ReportFrameRate()
{
    static timespec lastReport = { 0, 0 };
    static timespec lastFrame = { 0, 0 };
    static int      nFrames = 0;
    static double   fWorstMs = 0.0;

    timespec now;
    clock_gettime( CLOCK_MONOTONIC, &now );

    if ( lastReport.tv_sec == 0 )
    {
        lastReport = now;
        lastFrame = now;
        return;
    }

    const double fFrameMs = ( now.tv_sec - lastFrame.tv_sec ) * 1000.0 +
                            ( now.tv_nsec - lastFrame.tv_nsec ) / 1000000.0;
    lastFrame = now;
    if ( fFrameMs > fWorstMs )
        fWorstMs = fFrameMs;
    ++nFrames;

    const double fElapsed = ( now.tv_sec - lastReport.tv_sec ) +
                            ( now.tv_nsec - lastReport.tv_nsec ) / 1000000000.0;
    if ( fElapsed >= 1.0 )
    {
        LOGI( "%.1f fps, worst frame %.1f ms", nFrames / fElapsed, fWorstMs );
        lastReport = now;
        nFrames = 0;
        fWorstMs = 0.0;
    }
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
void HandleCommand( android_app *pApp, int32_t nCommand )
{
    SAppState *pState = (SAppState *)pApp->userData;
    if ( pState == 0 )
        return;

    switch ( nCommand )
    {
    case APP_CMD_INIT_WINDOW:
        if ( pApp->window != 0 )
            CreateSurface( pState );
        break;

    case APP_CMD_TERM_WINDOW:
        DestroySurface( pState );
        break;

    case APP_CMD_GAINED_FOCUS:
        Bk1SoundSetSuspended( false );
        break;

    case APP_CMD_WINDOW_RESIZED:
    case APP_CMD_CONFIG_CHANGED:
        if ( pState->bReady )
        {
            eglQuerySurface( pState->display, pState->surface, EGL_WIDTH, &pState->nWidth );
            eglQuerySurface( pState->display, pState->surface, EGL_HEIGHT, &pState->nHeight );
            Bk1SetClientSize( pState->nWidth, pState->nHeight );
        }
        break;

    case APP_CMD_LOST_FOCUS:
        // Anything held down is released, or the engine keeps believing a
        // finger is on the screen after the activity goes away.
        Bk1ClearInputEvents();
        Bk1ClearKeyStates();
        Bk1SoundSetSuspended( true );
        break;

    case APP_CMD_SAVE_STATE:
    case APP_CMD_PAUSE:
        // Settings are written through on every change, so there is nothing
        // to flush that has not been written; this makes sure of it anyway.
        Bk1FlushRegistry();
        break;

    default:
        break;
    }
}

}   // anonymous namespace

// ---------------------------------------------------------------------------
// The frame
// ---------------------------------------------------------------------------
// The engine's own loop is not started here yet. What runs is the surface, the
// input and the presentation -- the frame this clears and swaps is proof that
// the EGL side is right, and it is where the engine's frame will be called
// once its startup path is ported.
extern "C" void android_main( android_app *pApp )
{
    g_state.pApp = pApp;
    pApp->userData = &g_state;
    pApp->onAppCmd = HandleCommand;
    pApp->onInputEvent = HandleInput;

    // The settings store lives in the application's own directory.
    if ( pApp->activity != 0 && pApp->activity->internalDataPath != 0 )
    {
        char szPath[1024];
        snprintf( szPath, sizeof( szPath ), "%s/registry.txt",
                  pApp->activity->internalDataPath );
        Bk1SetRegistryFile( szPath );
        LOGI( "settings at %s", szPath );

        // The game's own data -- Data/*.pak, movies, saves -- is far too large
        // to package, so it lives in the app's external directory where the
        // player can copy it without root.
        if ( pApp->activity->externalDataPath != 0 )
            snprintf( g_state.szDataDirectory, sizeof( g_state.szDataDirectory ),
                      "%s", pApp->activity->externalDataPath );
        else
            snprintf( g_state.szDataDirectory, sizeof( g_state.szDataDirectory ),
                      "%s", pApp->activity->internalDataPath );
        Bk1SoundSetRootDirectory( g_state.szDataDirectory );
        LOGI( "game data expected under %s", g_state.szDataDirectory );
    }

    while ( true )
    {
        int nEvents = 0;
        android_poll_source *pSource = 0;
        // Block only when there is nothing to draw; otherwise drain what is
        // waiting and get on with the frame. The timeout is decided on each
        // poll and not once before the loop, because the event that creates
        // the surface arrives inside it -- deciding once means still blocking
        // after there is something to draw, and no frame is ever reached.
        while ( ALooper_pollOnce( g_state.bReady ? 0 : -1, 0, &nEvents,
                                  (void **)&pSource ) >= 0 )
        {
            if ( pSource != 0 )
                pSource->process( pApp, pSource );
            if ( pApp->destroyRequested != 0 )
            {
                DestroySurface( &g_state );
                return;
            }
        }

        if ( !g_state.bReady )
            continue;

        // The engine comes up on the first frame that has a surface, because
        // its graphics setup wants the real size and there is no size before
        // then. If it cannot start -- almost always missing game data -- the
        // reason is already in the log and there is nothing to loop over.
        if ( !g_state.bGameStarted )
        {
            g_state.bGameStarted = true;
            if ( !Bk1GameStartup( g_state.szDataDirectory, g_state.nWidth,
                                  g_state.nHeight ) )
            {
                LOGE( "the engine did not start; leaving the surface up so the "
                      "log can be read" );
                g_state.bGameFailed = true;
            }
        }

        if ( !g_state.bGameFailed )
        {

            // The engine draws the whole frame, clear included. Clearing here
            // as well would only cost fill rate.
            if ( !Bk1GameStep( true ) )
            {
                LOGI( "the game asked to exit" );
                Bk1GameShutdown();
                DestroySurface( &g_state );
                ANativeActivity_finish( pApp->activity );
                return;
            }
        }
        else
        {
            glViewport( 0, 0, g_state.nWidth, g_state.nHeight );
            glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
            glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );
        }

        {
            // A finger held perfectly still sends no motion events, so the
            // order gesture can only be noticed from here.
            NBk1Touch::SAction held[NBk1Touch::MAX_ACTIONS];
            PerformAll( held, g_gestures.Tick( NowMilliseconds(), held ) );
        }

        SampleFrame();
        eglSwapBuffers( g_state.display, g_state.surface );
        ReportFrameRate();
    }
}
