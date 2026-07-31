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

BOOL GetKeyboardState( BYTE *pKeyState )
{
    if ( pKeyState == 0 )
        return FALSE;
    for ( int i = 0; i < 256; ++i )
    {
        pKeyState[i] = (BYTE)( ( g_keyDown[i] ? 0x80 : 0 ) |
                               ( g_keyToggled[i] ? 0x01 : 0 ) );
    }
    return TRUE;
}

BOOL SetKeyboardState( BYTE *pKeyState )
{
    if ( pKeyState == 0 )
        return FALSE;
    for ( int i = 0; i < 256; ++i )
    {
        g_keyDown[i] = ( pKeyState[i] & 0x80 ) ? 1 : 0;
        g_keyToggled[i] = ( pKeyState[i] & 0x01 ) ? 1 : 0;
    }
    return TRUE;
}

// ---------------------------------------------------------------------------
// Scan codes to characters
// ---------------------------------------------------------------------------
// The engine turns a key press into a typed character in three steps: scan
// code to virtual key, then virtual key plus the shift state to a character.
// This is the table those steps read.
//
// The scan codes are the set the keyboard has always sent and the ones the
// engine's saved bindings hold; the layout is the US one, because that is what
// the engine falls back to and what its own DIK_ names describe. Characters
// come out as single bytes in the active code page, which is what the caller
// expects -- it converts them to wide characters itself, through the code page
// the game data was authored in.
namespace {

struct SKeyEntry
{
    unsigned char nScanCode;
    unsigned char nVirtualKey;
    char          cPlain;       // 0 when the key produces no character
    char          cShifted;
};

const SKeyEntry KEY_TABLE[] = {
    { 0x02, '1', '1', '!' },  { 0x03, '2', '2', '@' },
    { 0x04, '3', '3', '#' },  { 0x05, '4', '4', '$' },
    { 0x06, '5', '5', '%' },  { 0x07, '6', '6', '^' },
    { 0x08, '7', '7', '&' },  { 0x09, '8', '8', '*' },
    { 0x0A, '9', '9', '(' },  { 0x0B, '0', '0', ')' },
    { 0x0C, VK_OEM_MINUS, '-', '_' },
    { 0x0D, VK_OEM_PLUS,  '=', '+' },
    { 0x0E, VK_BACK,   0,   0 },
    { 0x0F, VK_TAB,    '\t', '\t' },

    { 0x10, 'Q', 'q', 'Q' },  { 0x11, 'W', 'w', 'W' },
    { 0x12, 'E', 'e', 'E' },  { 0x13, 'R', 'r', 'R' },
    { 0x14, 'T', 't', 'T' },  { 0x15, 'Y', 'y', 'Y' },
    { 0x16, 'U', 'u', 'U' },  { 0x17, 'I', 'i', 'I' },
    { 0x18, 'O', 'o', 'O' },  { 0x19, 'P', 'p', 'P' },
    { 0x1A, VK_OEM_4, '[', '{' },
    { 0x1B, VK_OEM_6, ']', '}' },
    { 0x1C, VK_RETURN, '\r', '\r' },
    { 0x1D, VK_LCONTROL, 0, 0 },

    { 0x1E, 'A', 'a', 'A' },  { 0x1F, 'S', 's', 'S' },
    { 0x20, 'D', 'd', 'D' },  { 0x21, 'F', 'f', 'F' },
    { 0x22, 'G', 'g', 'G' },  { 0x23, 'H', 'h', 'H' },
    { 0x24, 'J', 'j', 'J' },  { 0x25, 'K', 'k', 'K' },
    { 0x26, 'L', 'l', 'L' },
    { 0x27, VK_OEM_1,      ';', ':' },
    { 0x28, VK_OEM_7,      '\'', '"' },
    { 0x29, VK_OEM_3,      '`', '~' },
    { 0x2A, VK_LSHIFT,     0, 0 },
    { 0x2B, VK_OEM_5,      '\\', '|' },

    { 0x2C, 'Z', 'z', 'Z' },  { 0x2D, 'X', 'x', 'X' },
    { 0x2E, 'C', 'c', 'C' },  { 0x2F, 'V', 'v', 'V' },
    { 0x30, 'B', 'b', 'B' },  { 0x31, 'N', 'n', 'N' },
    { 0x32, 'M', 'm', 'M' },
    { 0x33, VK_OEM_COMMA,  ',', '<' },
    { 0x34, VK_OEM_PERIOD, '.', '>' },
    { 0x35, VK_OEM_2,      '/', '?' },
    { 0x36, VK_RSHIFT,     0, 0 },
    { 0x37, VK_MULTIPLY,   '*', '*' },
    { 0x38, VK_LMENU,      0, 0 },
    { 0x39, VK_SPACE,      ' ', ' ' },
    { 0x3A, VK_CAPITAL,    0, 0 },

    { 0x3B, VK_F1, 0, 0 },  { 0x3C, VK_F2, 0, 0 },
    { 0x3D, VK_F3, 0, 0 },  { 0x3E, VK_F4, 0, 0 },
    { 0x3F, VK_F5, 0, 0 },  { 0x40, VK_F6, 0, 0 },
    { 0x41, VK_F7, 0, 0 },  { 0x42, VK_F8, 0, 0 },
    { 0x43, VK_F9, 0, 0 },  { 0x44, VK_F10, 0, 0 },
    { 0x57, VK_F11, 0, 0 }, { 0x58, VK_F12, 0, 0 },

    { 0x45, VK_NUMLOCK, 0, 0 },
    { 0x46, VK_SCROLL,  0, 0 },
    { 0x47, VK_NUMPAD7, '7', '7' },
    { 0x48, VK_NUMPAD8, '8', '8' },
    { 0x49, VK_NUMPAD9, '9', '9' },
    { 0x4A, VK_SUBTRACT, '-', '-' },
    { 0x4B, VK_NUMPAD4, '4', '4' },
    { 0x4C, VK_NUMPAD5, '5', '5' },
    { 0x4D, VK_NUMPAD6, '6', '6' },
    { 0x4E, VK_ADD,     '+', '+' },
    { 0x4F, VK_NUMPAD1, '1', '1' },
    { 0x50, VK_NUMPAD2, '2', '2' },
    { 0x51, VK_NUMPAD3, '3', '3' },
    { 0x52, VK_NUMPAD0, '0', '0' },
    { 0x53, VK_DECIMAL, '.', '.' },

    { 0x01, VK_ESCAPE, 0, 0 },
};

const int KEY_TABLE_SIZE = (int)( sizeof( KEY_TABLE ) / sizeof( KEY_TABLE[0] ) );

const SKeyEntry *FindByScanCode( UINT nScanCode )
{
    for ( int i = 0; i < KEY_TABLE_SIZE; ++i )
    {
        if ( KEY_TABLE[i].nScanCode == nScanCode )
            return &KEY_TABLE[i];
    }
    return 0;
}

const SKeyEntry *FindByVirtualKey( UINT nVirtualKey )
{
    for ( int i = 0; i < KEY_TABLE_SIZE; ++i )
    {
        if ( KEY_TABLE[i].nVirtualKey == nVirtualKey )
            return &KEY_TABLE[i];
    }
    return 0;
}

}   // anonymous namespace

// One layout, so the handle is a token the engine can hold on to rather than a
// selection between several. It only ever passes it back here.
HKL GetKeyboardLayout( DWORD )
{
    static int nTheLayout = 0;
    return (HKL)&nTheLayout;
}

UINT MapVirtualKeyExA( UINT uCode, UINT uMapType, HKL )
{
    switch ( uMapType )
    {
    case MAPVK_VSC_TO_VK:
    case MAPVK_VSC_TO_VK_EX:
        {
            const SKeyEntry *pEntry = FindByScanCode( uCode );
            // Zero is a meaningful answer: it is how the caller learns that a
            // scan code has no virtual key, and it handles the navigation keys
            // itself when it sees one.
            return ( pEntry != 0 ) ? pEntry->nVirtualKey : 0;
        }

    case MAPVK_VK_TO_VSC:
        {
            const SKeyEntry *pEntry = FindByVirtualKey( uCode );
            return ( pEntry != 0 ) ? pEntry->nScanCode : 0;
        }

    case MAPVK_VK_TO_CHAR:
        {
            const SKeyEntry *pEntry = FindByVirtualKey( uCode );
            return ( pEntry != 0 ) ? (UINT)(unsigned char)pEntry->cShifted : 0;
        }

    default:
        return 0;
    }
}

UINT MapVirtualKeyA( UINT uCode, UINT uMapType )
{
    return MapVirtualKeyExA( uCode, uMapType, GetKeyboardLayout( 0 ) );
}

int ToAsciiEx( UINT uVirtKey, UINT uScanCode, const BYTE *pKeyState,
               WORD *pwChar, UINT, HKL )
{
    if ( pwChar == 0 )
        return 0;
    *pwChar = 0;

    const SKeyEntry *pEntry = FindByScanCode( uScanCode );
    if ( pEntry == 0 )
        pEntry = FindByVirtualKey( uVirtKey );
    if ( pEntry == 0 || pEntry->cPlain == 0 )
        return 0;   // a key that types nothing

    bool bShift = false;
    bool bCaps = false;
    if ( pKeyState != 0 )
    {
        bShift = ( pKeyState[VK_SHIFT] & 0x80 ) != 0;
        bCaps = ( pKeyState[VK_CAPITAL] & 0x01 ) != 0;
    }

    char cResult = bShift ? pEntry->cShifted : pEntry->cPlain;

    // Caps lock is not another shift: it applies to letters and to nothing
    // else, and together with shift it cancels out.
    if ( bCaps && pEntry->cPlain >= 'a' && pEntry->cPlain <= 'z' )
        cResult = bShift ? pEntry->cPlain : pEntry->cShifted;

    *pwChar = (WORD)(unsigned char)cResult;
    return 1;
}

int ToAscii( UINT uVirtKey, UINT uScanCode, const BYTE *pKeyState,
             WORD *pwChar, UINT uFlags )
{
    return ToAsciiEx( uVirtKey, uScanCode, pKeyState, pwChar, uFlags,
                      GetKeyboardLayout( 0 ) );
}

}   // extern "C"
