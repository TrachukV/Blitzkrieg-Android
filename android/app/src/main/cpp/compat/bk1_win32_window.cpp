// The virtual window and cursor declared in bk1_win32_window.h.
#include "bk1_win32_window.h"

#include <string.h>

namespace {

// The rendering surface. Until the Android layer reports one, this is the
// resolution Blitzkrieg shipped at, so anything that runs before the surface
// exists sees a sane viewport rather than a zero-sized one.
int g_nClientWidth = 1024;
int g_nClientHeight = 768;
// Until the Android side reports the real one, the density that means "one
// pixel is one density-independent pixel".
int g_nDisplayDpi = 160;

// Where the engine believes the mouse is. Starts centred, which is where a
// freshly created window would leave it.
int g_nCursorX = 512;
int g_nCursorY = 384;

// The clip rectangle, in client coordinates. Unset means unclipped.
RECT g_rcClip = { 0, 0, 0, 0 };
bool g_bClipped = false;

// Windows keeps a display count rather than a flag: ShowCursor increments and
// decrements it, and the cursor is visible while it is not negative.
int g_nShowCount = 0;

HCURSOR g_hCurrentCursor = 0;

// The window handle the engine passes around. It is never dereferenced -- only
// compared against zero -- so one distinct non-null value is all it needs.
HWND TheWindow()
{
    static int nToken = 0;
    return (HWND)&nToken;
}

int Clamp( int nValue, int nLow, int nHigh )
{
    if ( nValue < nLow )
        return nLow;
    if ( nValue > nHigh )
        return nHigh;
    return nValue;
}

void ApplyClip()
{
    if ( g_bClipped )
    {
        g_nCursorX = Clamp( g_nCursorX, (int)g_rcClip.left, (int)g_rcClip.right - 1 );
        g_nCursorY = Clamp( g_nCursorY, (int)g_rcClip.top, (int)g_rcClip.bottom - 1 );
    }
    else
    {
        g_nCursorX = Clamp( g_nCursorX, 0, g_nClientWidth - 1 );
        g_nCursorY = Clamp( g_nCursorY, 0, g_nClientHeight - 1 );
    }
}

}   // anonymous namespace

extern "C" {

void Bk1SetClientSize( int nWidth, int nHeight )
{
    if ( nWidth > 0 && nHeight > 0 )
    {
        g_nClientWidth = nWidth;
        g_nClientHeight = nHeight;
        ApplyClip();                        // a rotation can leave it outside
    }
}

void Bk1GetClientSize( int *pnWidth, int *pnHeight )
{
    if ( pnWidth != 0 )
        *pnWidth = g_nClientWidth;
    if ( pnHeight != 0 )
        *pnHeight = g_nClientHeight;
}

void Bk1SetCursorPos( int nX, int nY )
{
    g_nCursorX = nX;
    g_nCursorY = nY;
    ApplyClip();
}

HCURSOR Bk1GetCurrentCursor( void )
{
    return g_hCurrentCursor;
}

// ---------------------------------------------------------------------------
// Window geometry
// ---------------------------------------------------------------------------
// The window is the surface and the surface fills the screen, so the client
// rectangle and the window rectangle are the same and the origin is zero.
// That also makes ScreenToClient and ClientToScreen identities.
BOOL GetClientRect( HWND, RECT *pRect )
{
    if ( pRect == 0 )
        return FALSE;
    pRect->left = 0;
    pRect->top = 0;
    pRect->right = g_nClientWidth;
    pRect->bottom = g_nClientHeight;
    return TRUE;
}

BOOL GetWindowRect( HWND hWnd, RECT *pRect )
{
    return GetClientRect( hWnd, pRect );
}

BOOL ScreenToClient( HWND, POINT *pPoint )
{
    return ( pPoint != 0 ) ? TRUE : FALSE;
}

BOOL ClientToScreen( HWND, POINT *pPoint )
{
    return ( pPoint != 0 ) ? TRUE : FALSE;
}

// ---------------------------------------------------------------------------
// Cursor
// ---------------------------------------------------------------------------
BOOL GetCursorPos( POINT *pPoint )
{
    if ( pPoint == 0 )
        return FALSE;
    pPoint->x = g_nCursorX;
    pPoint->y = g_nCursorY;
    return TRUE;
}

BOOL SetCursorPos( int nX, int nY )
{
    Bk1SetCursorPos( nX, nY );
    return TRUE;
}

BOOL ClipCursor( const RECT *pRect )
{
    if ( pRect == 0 )
    {
        g_bClipped = false;                 // null releases the clip, as on Windows
        return TRUE;
    }
    g_rcClip = *pRect;
    g_bClipped = true;
    ApplyClip();
    return TRUE;
}

BOOL GetClipCursor( RECT *pRect )
{
    if ( pRect == 0 )
        return FALSE;
    if ( g_bClipped )
    {
        *pRect = g_rcClip;
        return TRUE;
    }
    return GetClientRect( 0, pRect );
}

int ShowCursor( BOOL bShow )
{
    g_nShowCount += ( bShow != FALSE ) ? 1 : -1;
    return g_nShowCount;
}

HCURSOR SetCursor( HCURSOR hCursor )
{
    HCURSOR hPrevious = g_hCurrentCursor;
    g_hCurrentCursor = hCursor;
    return hPrevious;
}

HCURSOR LoadCursorA( HINSTANCE, const char *pszCursorName )
{
    // The name is the identity; the engine only compares what it gets back and
    // hands it to SetCursor.
    return (HCURSOR)(void *)pszCursorName;
}

// ---------------------------------------------------------------------------
// Window state
// ---------------------------------------------------------------------------
// The surface fills the screen and the activity owns its stacking, so a move
// or a resize request is accepted and has nothing to do. A size change that
// matters arrives through Bk1SetClientSize instead.
BOOL SetWindowPos( HWND, HWND, int, int, int, int, UINT )
{
    return TRUE;
}

HWND GetActiveWindow( void ) { return TheWindow(); }
HWND GetForegroundWindow( void ) { return TheWindow(); }
BOOL IsWindow( HWND hWnd ) { return ( hWnd != 0 ) ? TRUE : FALSE; }

// The application is never minimised in the sense the engine tests for; it is
// either running or stopped by the activity lifecycle.
BOOL IsIconic( HWND ) { return FALSE; }

// ---------------------------------------------------------------------------
// System metrics
// ---------------------------------------------------------------------------
void Bk1SetDisplayDensity( int nDpi )
{
    if ( nDpi > 0 )
        g_nDisplayDpi = nDpi;
}

int GetSystemMetrics( int nIndex )
{
    switch ( nIndex )
    {
    case SM_CXSCREEN:
    case SM_CXFULLSCREEN:
        return g_nClientWidth;
    case SM_CYSCREEN:
    case SM_CYFULLSCREEN:
        return g_nClientHeight;

    // How far apart two taps may land and still be one double-click. Windows
    // answers with a few pixels, which is right for a mouse and wrong for a
    // finger: a finger lands within its own contact patch, not on a point.
    // Android's own figure for this is 100 density-independent pixels, so that
    // is what is converted here -- otherwise the slop would mean one thing on
    // a phone and another on a tablet.
    case SM_CXDOUBLECLK:
    case SM_CYDOUBLECLK:
        return ( 100 * g_nDisplayDpi ) / 160;

    case SM_CXCURSOR:
    case SM_CYCURSOR:
        return 32;

    default:
        return 0;
    }
}

UINT GetDoubleClickTime( void )
{
    // Android's own double-tap timeout. Taking Windows' 500 ms here would make
    // the engine wait longer than every other application on the device before
    // deciding a second tap was separate.
    return 300;
}

BOOL SystemParametersInfoA( UINT uAction, UINT uParam, void *pvParam, UINT )
{
    switch ( uAction )
    {
    // The repeat settings the engine reads to drive its text fields. These are
    // the indices Windows uses, converted from Android's own repeat timings:
    // 400 ms before the first repeat, then about 20 a second.
    case SPI_GETKEYBOARDDELAY:
        if ( pvParam != 0 )
            *(int *)pvParam = 1;        // index 1 is 500 ms; the nearest step
        return TRUE;
    case SPI_GETKEYBOARDSPEED:
        if ( pvParam != 0 )
            *(int *)pvParam = 31;       // the fastest step, ~30 a second
        return TRUE;

    // Nothing on Android is settable from here, and the engine does not check
    // whether the change took.
    case SPI_SETKEYBOARDDELAY:
    case SPI_SETKEYBOARDSPEED:
        (void)uParam;
        return TRUE;

    default:
        return FALSE;
    }
}

}   // extern "C"
