#pragma once
// The DirectInput surface the engine's input layer is written against.
//
// This is where touch enters the port. Blitzkrieg does not read the mouse and
// keyboard directly: it enumerates device objects -- axes and buttons -- binds
// them to commands by their offsets, and then reads a buffered stream of
// (offset, value, timestamp) events. Its whole binding, combo and repeat
// machinery sits on top of that stream.
//
// So the port keeps the stream and changes only what fills it. A finger
// becomes mouse axis and button events at the same offsets the engine already
// binds, and every layer above -- the key bindings the player saved, the
// double-click detection, the command suppression -- goes on working without
// knowing anything changed.
//
// The Android side pushes through Bk1PushInputEvent.
#include "bk1_win32_types.h"
#include "bk1_win32_keys.h"

#define DIRECTINPUT_VERSION 0x0800

// ---------------------------------------------------------------------------
// Object offsets, which are what the engine binds against
// ---------------------------------------------------------------------------
// The mouse state structure DirectInput reports, and the offsets into it. The
// engine stores these numbers in its binding files, so they have to keep their
// values.
typedef struct _DIMOUSESTATE {
    LONG lX;
    LONG lY;
    LONG lZ;
    BYTE rgbButtons[4];
} DIMOUSESTATE;

typedef struct _DIMOUSESTATE2 {
    LONG lX;
    LONG lY;
    LONG lZ;
    BYTE rgbButtons[8];
} DIMOUSESTATE2;

#define DIMOFS_X        0
#define DIMOFS_Y        4
#define DIMOFS_Z        8
#define DIMOFS_BUTTON0  12
#define DIMOFS_BUTTON1  13
#define DIMOFS_BUTTON2  14
#define DIMOFS_BUTTON3  15
#define DIMOFS_BUTTON4  16
#define DIMOFS_BUTTON5  17
#define DIMOFS_BUTTON6  18
#define DIMOFS_BUTTON7  19

// A keyboard object's offset is its scan code, and the engine's saved key
// bindings hold these numbers, so they keep the values the hardware and the
// API assign.

#define DIK_ESCAPE         0x01
#define DIK_1              0x02
#define DIK_2              0x03
#define DIK_3              0x04
#define DIK_4              0x05
#define DIK_5              0x06
#define DIK_6              0x07
#define DIK_7              0x08
#define DIK_8              0x09
#define DIK_9              0x0A
#define DIK_0              0x0B
#define DIK_MINUS          0x0C
#define DIK_EQUALS         0x0D
#define DIK_BACK           0x0E
#define DIK_TAB            0x0F
#define DIK_Q              0x10
#define DIK_W              0x11
#define DIK_E              0x12
#define DIK_R              0x13
#define DIK_T              0x14
#define DIK_Y              0x15
#define DIK_U              0x16
#define DIK_I              0x17
#define DIK_O              0x18
#define DIK_P              0x19
#define DIK_LBRACKET       0x1A
#define DIK_RBRACKET       0x1B
#define DIK_RETURN         0x1C
#define DIK_LCONTROL       0x1D
#define DIK_A              0x1E
#define DIK_S              0x1F
#define DIK_D              0x20
#define DIK_F              0x21
#define DIK_G              0x22
#define DIK_H              0x23
#define DIK_J              0x24
#define DIK_K              0x25
#define DIK_L              0x26
#define DIK_SEMICOLON      0x27
#define DIK_APOSTROPHE     0x28
#define DIK_GRAVE          0x29
#define DIK_LSHIFT         0x2A
#define DIK_BACKSLASH      0x2B
#define DIK_Z              0x2C
#define DIK_X              0x2D
#define DIK_C              0x2E
#define DIK_V              0x2F
#define DIK_B              0x30
#define DIK_N              0x31
#define DIK_M              0x32
#define DIK_COMMA          0x33
#define DIK_PERIOD         0x34
#define DIK_SLASH          0x35
#define DIK_RSHIFT         0x36
#define DIK_MULTIPLY       0x37
#define DIK_LMENU          0x38
#define DIK_SPACE          0x39
#define DIK_CAPITAL        0x3A
#define DIK_F1             0x3B
#define DIK_F2             0x3C
#define DIK_F3             0x3D
#define DIK_F4             0x3E
#define DIK_F5             0x3F
#define DIK_F6             0x40
#define DIK_F7             0x41
#define DIK_F8             0x42
#define DIK_F9             0x43
#define DIK_F10            0x44
#define DIK_NUMLOCK        0x45
#define DIK_SCROLL         0x46
#define DIK_NUMPAD7        0x47
#define DIK_NUMPAD8        0x48
#define DIK_NUMPAD9        0x49
#define DIK_SUBTRACT       0x4A
#define DIK_NUMPAD4        0x4B
#define DIK_NUMPAD5        0x4C
#define DIK_NUMPAD6        0x4D
#define DIK_ADD            0x4E
#define DIK_NUMPAD1        0x4F
#define DIK_NUMPAD2        0x50
#define DIK_NUMPAD3        0x51
#define DIK_NUMPAD0        0x52
#define DIK_DECIMAL        0x53
#define DIK_OEM_102        0x56
#define DIK_F11            0x57
#define DIK_F12            0x58
#define DIK_F13            0x64
#define DIK_F14            0x65
#define DIK_F15            0x66
#define DIK_KANA           0x70
#define DIK_ABNT_C1        0x73
#define DIK_CONVERT        0x79
#define DIK_NOCONVERT      0x7B
#define DIK_YEN            0x7D
#define DIK_ABNT_C2        0x7E
#define DIK_NUMPADEQUALS   0x8D
#define DIK_CIRCUMFLEX     0x90
#define DIK_AT             0x91
#define DIK_COLON          0x92
#define DIK_UNDERLINE      0x93
#define DIK_KANJI          0x94
#define DIK_STOP           0x95
#define DIK_AX             0x96
#define DIK_UNLABELED      0x97
#define DIK_NEXTTRACK      0x99
#define DIK_NUMPADENTER    0x9C
#define DIK_RCONTROL       0x9D
#define DIK_MUTE           0xA0
#define DIK_CALCULATOR     0xA1
#define DIK_PLAYPAUSE      0xA2
#define DIK_MEDIASTOP      0xA4
#define DIK_VOLUMEDOWN     0xAE
#define DIK_VOLUMEUP       0xB0
#define DIK_WEBHOME        0xB2
#define DIK_NUMPADCOMMA    0xB3
#define DIK_DIVIDE         0xB5
#define DIK_SYSRQ          0xB7
#define DIK_RMENU          0xB8
#define DIK_PAUSE          0xC5
#define DIK_HOME           0xC7
#define DIK_UP             0xC8
#define DIK_PRIOR          0xC9
#define DIK_LEFT           0xCB
#define DIK_RIGHT          0xCD
#define DIK_END            0xCF
#define DIK_DOWN           0xD0
#define DIK_NEXT           0xD1
#define DIK_INSERT         0xD2
#define DIK_DELETE         0xD3
#define DIK_LWIN           0xDB
#define DIK_RWIN           0xDC
#define DIK_APPS           0xDD
#define DIK_POWER          0xDE
#define DIK_SLEEP          0xDF
#define DIK_WAKE           0xE3
#define DIK_WEBSEARCH      0xE5
#define DIK_WEBFAVORITES   0xE6
#define DIK_WEBREFRESH     0xE7
#define DIK_WEBSTOP        0xE8
#define DIK_WEBFORWARD     0xE9
#define DIK_WEBBACK        0xEA
#define DIK_MYCOMPUTER     0xEB
#define DIK_MAIL           0xEC
#define DIK_MEDIASELECT    0xED
#define DIK_PREVTRACK    0x90
#define DIK_BACKSPACE    DIK_BACK
#define DIK_NUMPADSTAR   DIK_MULTIPLY
#define DIK_NUMPADMINUS  DIK_SUBTRACT
#define DIK_NUMPADPLUS   DIK_ADD
#define DIK_NUMPADPERIOD DIK_DECIMAL
#define DIK_NUMPADSLASH  DIK_DIVIDE
#define DIK_LALT         DIK_LMENU
#define DIK_RALT         DIK_RMENU
#define DIK_CAPSLOCK     DIK_CAPITAL
#define DIK_PGUP         DIK_PRIOR
#define DIK_PGDN         DIK_NEXT

// ---------------------------------------------------------------------------
// Device kinds and enumeration
// ---------------------------------------------------------------------------
#define DI8DEVCLASS_ALL        0
#define DI8DEVCLASS_DEVICE     1
#define DI8DEVCLASS_POINTER    2
#define DI8DEVCLASS_KEYBOARD   3
#define DI8DEVCLASS_GAMECTRL   4

#define DI8DEVTYPE_MOUSE       0x12
#define DI8DEVTYPE_KEYBOARD    0x13
// The rest of the device types the engine switches on while enumerating. It
// only ever compares them, to decide what to call a controller it found; a
// touch screen is none of these, so on Android the enumeration finds nothing
// and the switch is never reached.
#define DI8DEVTYPE_JOYSTICK    0x14
#define DI8DEVTYPE_GAMEPAD     0x15
#define DI8DEVTYPE_DRIVING     0x16
#define DI8DEVTYPE_FLIGHT      0x17
#define DI8DEVTYPE_1STPERSON   0x18
#define DI8DEVTYPE_DEVICECTRL  0x19
#define DI8DEVTYPE_SCREENPOINTER 0x1A
#define DI8DEVTYPE_REMOTE      0x1B
#define DI8DEVTYPE_SUPPLEMENTAL 0x1C

// The type is the low byte of dwDevType; the byte above it is the subtype.
#define GET_DIDEVICE_TYPE( dwDevType )     ( LOBYTE( dwDevType ) )
#define GET_DIDEVICE_SUBTYPE( dwDevType )  ( HIBYTE( dwDevType ) )

#define DIDEVTYPE_MOUSE        2
#define DIDEVTYPE_KEYBOARD     3

#define DIEDFL_ALLDEVICES      0x00000000
#define DIEDFL_ATTACHEDONLY    0x00000001

#define DIENUM_STOP            0
#define DIENUM_CONTINUE        1

#define DIDFT_ALL              0x00000000
#define DIDFT_RELAXIS          0x00000001
#define DIDFT_ABSAXIS          0x00000002
#define DIDFT_AXIS             0x00000003
#define DIDFT_PSHBUTTON        0x00000004
#define DIDFT_TGLBUTTON        0x00000008
#define DIDFT_BUTTON           0x0000000C
#define DIDFT_POV              0x00000010

#define DIDF_ABSAXIS           0x00000001
#define DIDF_RELAXIS           0x00000002

#define DISCL_EXCLUSIVE        0x00000001
#define DISCL_NONEXCLUSIVE     0x00000002
#define DISCL_FOREGROUND       0x00000004
#define DISCL_BACKGROUND       0x00000008

#define DIPH_DEVICE            0
#define DIPH_BYOFFSET          1
#define DIPH_BYID              2

// The results the engine tests for.
#define DI_OK                  S_OK
#define DI_NOEFFECT            ( (HRESULT)1 )
#define DIERR_INPUTLOST        ( (HRESULT)0x8007001EL )
#define DIERR_NOTACQUIRED      ( (HRESULT)0x8007000CL )
#define DIERR_OTHERAPPHASPRIO  ( (HRESULT)0x80070005L )
#define DIERR_INVALIDPARAM     ( (HRESULT)0x80070057L )
#define DIERR_UNSUPPORTED      ( (HRESULT)0x80004001L )

// ---------------------------------------------------------------------------
// Structures
// ---------------------------------------------------------------------------
typedef struct _DIDEVICEOBJECTDATA {
    DWORD dwOfs;         // which object -- an axis or a button
    DWORD dwData;        // its new value
    DWORD dwTimeStamp;
    DWORD dwSequence;
    UINT_PTR uAppData;
} DIDEVICEOBJECTDATA;

typedef struct _DIPROPHEADER {
    DWORD dwSize;
    DWORD dwHeaderSize;
    DWORD dwObj;
    DWORD dwHow;
} DIPROPHEADER;

typedef struct _DIPROPDWORD {
    DIPROPHEADER diph;
    DWORD        dwData;
} DIPROPDWORD;

typedef struct _DIPROPRANGE {
    DIPROPHEADER diph;
    LONG         lMin;
    LONG         lMax;
} DIPROPRANGE;

// The property identifiers, which DirectInput passes as pointers rather than
// as values.
#define MAKEDIPROP( prop ) ( (const GUID *)(size_t)( prop ) )
#define DIPROP_BUFFERSIZE  MAKEDIPROP( 1 )
#define DIPROP_AXISMODE    MAKEDIPROP( 2 )
#define DIPROP_GRANULARITY MAKEDIPROP( 3 )
#define DIPROP_RANGE       MAKEDIPROP( 4 )
#define DIPROPAXISMODE_ABS 0
#define DIPROPAXISMODE_REL 1

typedef struct _DIOBJECTDATAFORMAT {
    const GUID *pguid;
    DWORD       dwOfs;
    DWORD       dwType;
    DWORD       dwFlags;
} DIOBJECTDATAFORMAT;

typedef struct _DIDATAFORMAT {
    DWORD               dwSize;
    DWORD               dwObjSize;
    DWORD               dwFlags;
    DWORD               dwDataSize;
    DWORD               dwNumObjs;
    DIOBJECTDATAFORMAT *rgodf;
} DIDATAFORMAT;

typedef struct _DIDEVICEINSTANCEA {
    DWORD dwSize;
    GUID  guidInstance;
    GUID  guidProduct;
    DWORD dwDevType;
    char  tszInstanceName[260];
    char  tszProductName[260];
} DIDEVICEINSTANCEA, DIDEVICEINSTANCE;

typedef struct _DIDEVICEOBJECTINSTANCEA {
    DWORD dwSize;
    GUID  guidType;
    DWORD dwOfs;
    DWORD dwType;
    DWORD dwFlags;
    char  tszName[260];
} DIDEVICEOBJECTINSTANCEA, DIDEVICEOBJECTINSTANCE;

// Enumeration callbacks receive these by const pointer.
typedef const DIDEVICEINSTANCE       *LPCDIDEVICEINSTANCE;
typedef const DIDEVICEINSTANCEA      *LPCDIDEVICEINSTANCEA;
typedef const DIDEVICEOBJECTINSTANCE *LPCDIDEVICEOBJECTINSTANCE;
typedef const DIDEVICEOBJECTINSTANCEA *LPCDIDEVICEOBJECTINSTANCEA;

// DIDEVCAPS::dwFlags. The engine tests these two to decide whether a device
// has to be polled before its state can be read. Nothing here does -- touch
// arrives as events and is buffered as it comes -- so neither is ever set.
#define DIDC_ATTACHED           0x00000001
#define DIDC_POLLEDDEVICE       0x00000002
#define DIDC_EMULATED           0x00000004
#define DIDC_POLLEDDATAFORMAT   0x00000008

typedef struct _DIDEVCAPS {
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwDevType;
    DWORD dwAxes;
    DWORD dwButtons;
    DWORD dwPOVs;
} DIDEVCAPS;

// The device identifiers the engine asks for by name.
extern const GUID GUID_SysMouse;
extern const GUID GUID_SysKeyboard;
extern const GUID GUID_XAxis;
extern const GUID GUID_YAxis;
extern const GUID GUID_ZAxis;
extern const GUID GUID_Button;
extern const GUID GUID_Key;
extern const GUID GUID_RxAxis;
extern const GUID GUID_RyAxis;
extern const GUID GUID_RzAxis;
extern const GUID GUID_Slider;
extern const GUID GUID_POV;

// The prepared data formats, as DirectInput publishes them.
extern DIDATAFORMAT c_dfDIMouse;
extern DIDATAFORMAT c_dfDIMouse2;
extern DIDATAFORMAT c_dfDIKeyboard;

struct IDirectInput8;
struct IDirectInputDevice8;

// The engine passes this to DirectInput8Create to say which interface version
// it wants back. There is only one here, so the value is never compared -- but
// it has to be a distinct, stable constant like the others in this layer.
static const IID IID_IDirectInput8 =
    { 0xbf798030, 0x483a, 0x4da2, { 0xaa, 0x99, 0x5d, 0x64, 0xed, 0x36, 0x97, 0x00 } };

typedef IDirectInput8       *LPDIRECTINPUT8;
// the version-7 spellings, which the machine probe names
typedef IDirectInput8       *LPDIRECTINPUT;
typedef IDirectInputDevice8 *LPDIRECTINPUTDEVICE;
typedef IDirectInputDevice8 *LPDIRECTINPUTDEVICE8;

typedef BOOL ( CALLBACK *LPDIENUMDEVICESCALLBACKA )( const DIDEVICEINSTANCEA *, void * );
typedef BOOL ( CALLBACK *LPDIENUMDEVICEOBJECTSCALLBACKA )( const DIDEVICEOBJECTINSTANCEA *, void * );

struct IDirectInputDevice8 : public IUnknown
{
    virtual HRESULT STDCALL GetCapabilities( DIDEVCAPS *pCaps ) = 0;
    virtual HRESULT STDCALL EnumObjects( LPDIENUMDEVICEOBJECTSCALLBACKA pCallback,
                                         void *pRef, DWORD dwFlags ) = 0;
    virtual HRESULT STDCALL GetProperty( const GUID *pProp, DIPROPHEADER *pHeader ) = 0;
    virtual HRESULT STDCALL SetProperty( const GUID *pProp, const DIPROPHEADER *pHeader ) = 0;
    virtual HRESULT STDCALL Acquire() = 0;
    virtual HRESULT STDCALL Unacquire() = 0;
    virtual HRESULT STDCALL GetDeviceState( DWORD cbData, void *pData ) = 0;
    // The buffered stream the engine's bindings read.
    virtual HRESULT STDCALL GetDeviceData( DWORD cbObjectData, DIDEVICEOBJECTDATA *rgdod,
                                           DWORD *pdwInOut, DWORD dwFlags ) = 0;
    virtual HRESULT STDCALL SetDataFormat( const DIDATAFORMAT *pFormat ) = 0;
    virtual HRESULT STDCALL SetCooperativeLevel( HWND hWnd, DWORD dwFlags ) = 0;
    virtual HRESULT STDCALL Poll() = 0;
};

struct IDirectInput8 : public IUnknown
{
    virtual HRESULT STDCALL CreateDevice( const GUID &guid, IDirectInputDevice8 **ppDevice,
                                          IUnknown *pUnkOuter ) = 0;
    virtual HRESULT STDCALL EnumDevices( DWORD dwDevType,
                                         LPDIENUMDEVICESCALLBACKA pCallback,
                                         void *pRef, DWORD dwFlags ) = 0;
};

#ifdef __cplusplus
extern "C" {
#endif

HRESULT DirectInput8Create( HINSTANCE hInstance, DWORD dwVersion, const GUID &riid,
                            void **ppOut, IUnknown *pUnkOuter );
// The version-7 spelling, which the engine's loader also reaches for.
HRESULT DirectInputCreate( HINSTANCE hInstance, DWORD dwVersion,
                           IDirectInput8 **ppOut, IUnknown *pUnkOuter );

// ---------------------------------------------------------------------------
// What the Android side pushes in
// ---------------------------------------------------------------------------
// A device object changing value: an axis moving or a button going down or up.
// 'nOffset' is one of the DIMOFS_ values for the mouse, or a scan code for the
// keyboard, which is what the engine's bindings are expressed in.
#define BK1_INPUT_MOUSE    0
#define BK1_INPUT_KEYBOARD 1

void Bk1PushInputEvent( int nDevice, DWORD nOffset, DWORD nValue );
// Clears the buffered stream, for when the activity loses focus.
void Bk1ClearInputEvents( void );

#ifdef __cplusplus
}
#endif
