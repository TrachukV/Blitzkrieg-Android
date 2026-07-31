// The keyboard state behind bk1_win32_keys.h.
//
// The engine polls rather than waiting for messages, so all this has to do is
// hold what is currently down and answer in the shape the engine tests: the
// high bit of the returned value.
#include "bk1_win32_keys.h"

#include <string.h>

namespace {

// One byte per virtual key code, which is all the range there is.
unsigned char g_keyDown[256] = { 0 };

// Windows' GetKeyState also reports a toggle in the low bit, for Caps Lock and
// the like. The engine never reads it, but the keys that have one are tracked
// so the answer is not simply wrong.
unsigned char g_keyToggled[256] = { 0 };

bool IsToggleKey( int nVirtualKey )
{
    return nVirtualKey == VK_CAPITAL || nVirtualKey == VK_NUMLOCK ||
           nVirtualKey == VK_SCROLL;
}

}   // anonymous namespace

extern "C" {

void Bk1SetKeyState( int nVirtualKey, int bDown )
{
    if ( nVirtualKey < 0 || nVirtualKey > 255 )
        return;

    const bool bWasDown = ( g_keyDown[nVirtualKey] != 0 );
    g_keyDown[nVirtualKey] = ( bDown != 0 ) ? 1 : 0;

    // a toggle flips on the press, not on the release
    if ( bDown != 0 && !bWasDown && IsToggleKey( nVirtualKey ) )
        g_keyToggled[nVirtualKey] ^= 1;

    // The side-specific codes and their generic counterpart move together, as
    // they do on Windows: code that asks for VK_SHIFT sees either shift key.
    switch ( nVirtualKey )
    {
    case VK_LSHIFT:
    case VK_RSHIFT:
        g_keyDown[VK_SHIFT] = ( g_keyDown[VK_LSHIFT] || g_keyDown[VK_RSHIFT] ) ? 1 : 0;
        break;
    case VK_LCONTROL:
    case VK_RCONTROL:
        g_keyDown[VK_CONTROL] = ( g_keyDown[VK_LCONTROL] || g_keyDown[VK_RCONTROL] ) ? 1 : 0;
        break;
    case VK_LMENU:
    case VK_RMENU:
        g_keyDown[VK_MENU] = ( g_keyDown[VK_LMENU] || g_keyDown[VK_RMENU] ) ? 1 : 0;
        break;
    default:
        break;
    }
}

void Bk1ClearKeyStates( void )
{
    memset( g_keyDown, 0, sizeof( g_keyDown ) );
}

SHORT GetAsyncKeyState( int nVirtualKey )
{
    if ( nVirtualKey < 0 || nVirtualKey > 255 )
        return 0;
    return (SHORT)( g_keyDown[nVirtualKey] ? (SHORT)0x8000 : 0 );
}

SHORT GetKeyState( int nVirtualKey )
{
    if ( nVirtualKey < 0 || nVirtualKey > 255 )
        return 0;
    SHORT nResult = g_keyDown[nVirtualKey] ? (SHORT)0x8000 : 0;
    if ( g_keyToggled[nVirtualKey] )
        nResult |= 1;
    return nResult;
}

}   // extern "C"
