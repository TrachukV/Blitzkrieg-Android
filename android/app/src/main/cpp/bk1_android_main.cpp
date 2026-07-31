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

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "compat/bk1_win32_window.h"
#include "compat/bk1_win32_keys.h"
#include "compat/bk1_win32_registry.h"
#include "compat/dinput.h"

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

    SAppState()
        : pApp( 0 ), display( EGL_NO_DISPLAY ), surface( EGL_NO_SURFACE ),
          context( EGL_NO_CONTEXT ), nWidth( 0 ), nHeight( 0 ), bReady( false ),
          nActivePointer( -1 ), fLastX( 0.0f ), fLastY( 0.0f ) {}
};

SAppState g_state;

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
// A finger is the cursor. Moving it moves the cursor; putting it down and
// lifting it is a click at that place. The engine's own binding, double-click
// and drag machinery reads the result and needs no changes: to it, a mouse has
// moved and a button has gone down.
int HandleInput( android_app *pApp, AInputEvent *pEvent )
{
    SAppState *pState = (SAppState *)pApp->userData;
    if ( pState == 0 || AInputEvent_getType( pEvent ) != AINPUT_EVENT_TYPE_MOTION )
        return 0;

    const int32_t nAction = AMotionEvent_getAction( pEvent );
    const int32_t nKind = nAction & AMOTION_EVENT_ACTION_MASK;
    const size_t  nIndex =
        (size_t)( ( nAction & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK ) >>
                  AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT );

    const float fX = AMotionEvent_getX( pEvent, nIndex );
    const float fY = AMotionEvent_getY( pEvent, nIndex );

    switch ( nKind )
    {
    case AMOTION_EVENT_ACTION_DOWN:
    case AMOTION_EVENT_ACTION_POINTER_DOWN:
        if ( pState->nActivePointer < 0 )
        {
            pState->nActivePointer = AMotionEvent_getPointerId( pEvent, nIndex );
            pState->fLastX = fX;
            pState->fLastY = fY;
            // The cursor goes to the finger before the press, so the engine
            // sees the button go down where it was touched.
            Bk1SetCursorPos( (int)fX, (int)fY );
            Bk1PushInputEvent( BK1_INPUT_MOUSE, DIMOFS_BUTTON0, 0x80 );

            // Once, so that bringing the port up on a new device shows whether
            // touch is reaching the engine's input at all. Reading the cursor
            // back means the report comes from the layer the engine reads,
            // not from the coordinates handed in here.
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

    case AMOTION_EVENT_ACTION_MOVE:
        {
            const size_t nCount = AMotionEvent_getPointerCount( pEvent );
            for ( size_t i = 0; i < nCount; ++i )
            {
                if ( AMotionEvent_getPointerId( pEvent, i ) != pState->nActivePointer )
                    continue;
                const float fMoveX = AMotionEvent_getX( pEvent, i );
                const float fMoveY = AMotionEvent_getY( pEvent, i );
                const int nDeltaX = (int)( fMoveX - pState->fLastX );
                const int nDeltaY = (int)( fMoveY - pState->fLastY );
                if ( nDeltaX != 0 || nDeltaY != 0 )
                {
                    // Both spellings, because the engine reads the axes as a
                    // relative stream and the cursor as an absolute position.
                    Bk1PushInputEvent( BK1_INPUT_MOUSE, DIMOFS_X, (DWORD)nDeltaX );
                    Bk1PushInputEvent( BK1_INPUT_MOUSE, DIMOFS_Y, (DWORD)nDeltaY );
                    Bk1SetCursorPos( (int)fMoveX, (int)fMoveY );
                    pState->fLastX = fMoveX;
                    pState->fLastY = fMoveY;
                }
            }
        }
        return 1;

    case AMOTION_EVENT_ACTION_UP:
    case AMOTION_EVENT_ACTION_POINTER_UP:
    case AMOTION_EVENT_ACTION_CANCEL:
        if ( AMotionEvent_getPointerId( pEvent, nIndex ) == pState->nActivePointer )
        {
            Bk1PushInputEvent( BK1_INPUT_MOUSE, DIMOFS_BUTTON0, 0 );
            pState->nActivePointer = -1;
        }
        return 1;

    default:
        break;
    }
    return 0;
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

        glViewport( 0, 0, g_state.nWidth, g_state.nHeight );
        glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
        glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );

        eglSwapBuffers( g_state.display, g_state.surface );
        ReportFrameRate();
    }
}
