#pragma once
// The window and cursor calls the engine makes, backed by one virtual window
// and one virtual cursor.
//
// Blitzkrieg asks the window system where its client area is and where the
// mouse is, ninety and forty-five times over respectively, and clips the
// cursor to the viewport while scrolling the map. Android has no cursor and no
// window in that sense, but the engine's model is exactly what a touch port
// wants to keep: the finger positions a cursor, and the game goes on believing
// it has one. Selection rectangles, edge scrolling and hover all keep working
// without the game knowing anything changed.
//
// The Android layer drives this from the other side: Bk1SetClientSize when the
// surface is created or rotated, Bk1SetCursorPos as a finger moves.
#include "bk1_win32_types.h"

// Windows packs a numeric resource id into a pointer; the engine passes the
// result of this straight to LoadCursor, which only keeps the identity.
#define MAKEINTRESOURCE( i ) ( (const char *)(size_t)( (WORD)(i) ) )

// A cursor is an opaque identifier here. The engine loads one per shape and
// passes it to SetCursor to say which it wants; the drawing is the engine's
// own, so the identity is all that has to survive.
#define IDC_ARROW       ( (const char *)32512 )
#define IDC_IBEAM       ( (const char *)32513 )
#define IDC_WAIT        ( (const char *)32514 )
#define IDC_CROSS       ( (const char *)32515 )
#define IDC_SIZEALL     ( (const char *)32646 )
#define IDC_NO          ( (const char *)32648 )
#define IDC_HAND        ( (const char *)32649 )

// SetWindowPos' arguments. The surface is the whole screen and its stacking
// is the activity's business, so the call is answered rather than acted on --
// but the engine names these when it asks.
#define HWND_TOP        ( (HWND)0 )
#define HWND_BOTTOM     ( (HWND)1 )
#define HWND_TOPMOST    ( (HWND)-1 )
#define HWND_NOTOPMOST  ( (HWND)-2 )

#define SWP_NOSIZE          0x0001
#define SWP_NOMOVE          0x0002
#define SWP_NOZORDER        0x0004
#define SWP_NOACTIVATE      0x0010
#define SWP_SHOWWINDOW      0x0040
#define SWP_HIDEWINDOW      0x0080
#define SWP_FRAMECHANGED    0x0020

#ifdef __cplusplus
extern "C" {
#endif

BOOL SetWindowPos( HWND hWnd, HWND hWndInsertAfter, int nX, int nY,
                   int nWidth, int nHeight, UINT uFlags );

// --- driven by the Android side ---
// The size of the rendering surface, which is the window's client area.
void Bk1SetClientSize( int nWidth, int nHeight );
void Bk1GetClientSize( int *pnWidth, int *pnHeight );
// Where the finger is. Clipped to the cursor's clip rectangle, as Windows
// clips a real one, so that the engine's edge-scroll logic behaves.
void Bk1SetCursorPos( int nX, int nY );
// Which cursor the engine last asked for, so the renderer can draw it.
HCURSOR Bk1GetCurrentCursor( void );

// --- what the engine calls ---
BOOL GetClientRect( HWND hWnd, RECT *pRect );
BOOL GetWindowRect( HWND hWnd, RECT *pRect );
BOOL ScreenToClient( HWND hWnd, POINT *pPoint );
BOOL ClientToScreen( HWND hWnd, POINT *pPoint );

BOOL GetCursorPos( POINT *pPoint );
BOOL SetCursorPos( int nX, int nY );
BOOL ClipCursor( const RECT *pRect );
BOOL GetClipCursor( RECT *pRect );
int  ShowCursor( BOOL bShow );
HCURSOR SetCursor( HCURSOR hCursor );
HCURSOR LoadCursorA( HINSTANCE hInstance, const char *pszCursorName );

HWND GetActiveWindow( void );
HWND GetForegroundWindow( void );
BOOL IsWindow( HWND hWnd );
BOOL IsIconic( HWND hWnd );

#ifdef __cplusplus
}
#endif

#define LoadCursor LoadCursorA
