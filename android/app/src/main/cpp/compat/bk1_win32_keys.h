#pragma once
// Windows virtual key codes.
//
// The engine's UI and its key bindings are written against these, and the port
// keeps them: an on-screen control or a hardware key on the device is
// translated into one of these codes and fed to the same handlers the game
// always used. Keeping the engine's own key model is what lets the original
// bindings, the menus and the shortcut bar behave as they did.
//
// The values are the ones Windows assigns, because the game's saved key
// bindings hold them by number.
#include "bk1_win32_types.h"

#define VK_LBUTTON      0x01
#define VK_RBUTTON      0x02
#define VK_CANCEL       0x03
#define VK_MBUTTON      0x04

#define VK_BACK         0x08
#define VK_TAB          0x09
#define VK_CLEAR        0x0C
#define VK_RETURN       0x0D
#define VK_SHIFT        0x10
#define VK_CONTROL      0x11
#define VK_MENU         0x12
#define VK_PAUSE        0x13
#define VK_CAPITAL      0x14
#define VK_ESCAPE       0x1B
#define VK_SPACE        0x20
#define VK_PRIOR        0x21
#define VK_NEXT         0x22
#define VK_END          0x23
#define VK_HOME         0x24
#define VK_LEFT         0x25
#define VK_UP           0x26
#define VK_RIGHT        0x27
#define VK_DOWN         0x28
#define VK_SELECT       0x29
#define VK_PRINT        0x2A
#define VK_EXECUTE      0x2B
#define VK_SNAPSHOT     0x2C
#define VK_INSERT       0x2D
#define VK_DELETE       0x2E
#define VK_HELP         0x2F

// 0x30..0x39 are '0'..'9' and 0x41..0x5A are 'A'..'Z', by their character
// codes; the engine writes them as characters and needs no names for them.

#define VK_LWIN         0x5B
#define VK_RWIN         0x5C
#define VK_APPS         0x5D

#define VK_NUMPAD0      0x60
#define VK_NUMPAD1      0x61
#define VK_NUMPAD2      0x62
#define VK_NUMPAD3      0x63
#define VK_NUMPAD4      0x64
#define VK_NUMPAD5      0x65
#define VK_NUMPAD6      0x66
#define VK_NUMPAD7      0x67
#define VK_NUMPAD8      0x68
#define VK_NUMPAD9      0x69
#define VK_MULTIPLY     0x6A
#define VK_ADD          0x6B
#define VK_SEPARATOR    0x6C
#define VK_SUBTRACT     0x6D
#define VK_DECIMAL      0x6E
#define VK_DIVIDE       0x6F

#define VK_F1           0x70
#define VK_F2           0x71
#define VK_F3           0x72
#define VK_F4           0x73
#define VK_F5           0x74
#define VK_F6           0x75
#define VK_F7           0x76
#define VK_F8           0x77
#define VK_F9           0x78
#define VK_F10          0x79
#define VK_F11          0x7A
#define VK_F12          0x7B

#define VK_NUMLOCK      0x90
#define VK_SCROLL       0x91

#define VK_LSHIFT       0xA0
#define VK_RSHIFT       0xA1
#define VK_LCONTROL     0xA2
#define VK_RCONTROL     0xA3
#define VK_LMENU        0xA4
#define VK_RMENU        0xA5

#define VK_OEM_1        0xBA
#define VK_OEM_PLUS     0xBB
#define VK_OEM_COMMA    0xBC
#define VK_OEM_MINUS    0xBD
#define VK_OEM_PERIOD   0xBE
#define VK_OEM_2        0xBF
#define VK_OEM_3        0xC0
#define VK_OEM_4        0xDB
#define VK_OEM_5        0xDC
#define VK_OEM_6        0xDD
#define VK_OEM_7        0xDE

#ifdef __cplusplus
extern "C" {
#endif

// --- driven by the Android side ---
// Reports a key going down or coming up. The engine polls the state through
// GetAsyncKeyState and GetKeyState, so this is where a device key or an
// on-screen control enters.
void Bk1SetKeyState( int nVirtualKey, int bDown );
void Bk1ClearKeyStates( void );

// --- what the engine calls ---
// The high bit is set while the key is down, which is the bit the engine tests.
SHORT GetAsyncKeyState( int nVirtualKey );
SHORT GetKeyState( int nVirtualKey );

// The whole key state at once, in the form Windows hands it over: the high bit
// of each byte says the key is down, the low bit says a toggle key is lit.
BOOL GetKeyboardState( BYTE *pKeyState );
BOOL SetKeyboardState( BYTE *pKeyState );

// The engine turns key presses into typed characters through these three, and
// text entry -- player names, saved games, chat -- goes through that path.
//
// There is one layout on Android, so the handle is a token rather than a
// selection; what matters is that the same one comes back every time, because
// the engine holds on to it across calls.
typedef void *HKL;

HKL  GetKeyboardLayout( DWORD dwThreadId );

// MapVirtualKey's translation directions. The engine asks for the second one.
#define MAPVK_VK_TO_VSC     0
#define MAPVK_VSC_TO_VK     1
#define MAPVK_VK_TO_CHAR    2
#define MAPVK_VSC_TO_VK_EX  3

UINT MapVirtualKeyA( UINT uCode, UINT uMapType );
UINT MapVirtualKeyExA( UINT uCode, UINT uMapType, HKL hkl );
#ifndef MapVirtualKey
#define MapVirtualKey   MapVirtualKeyA
#endif
#ifndef MapVirtualKeyEx
#define MapVirtualKeyEx MapVirtualKeyExA
#endif

// Virtual key plus the shift state to the character it produces. Returns the
// number of characters written, as Windows does.
int ToAscii( UINT uVirtKey, UINT uScanCode, const BYTE *pKeyState,
             WORD *pwChar, UINT uFlags );
int ToAsciiEx( UINT uVirtKey, UINT uScanCode, const BYTE *pKeyState,
               WORD *pwChar, UINT uFlags, HKL hkl );

#ifdef __cplusplus
}
#endif
