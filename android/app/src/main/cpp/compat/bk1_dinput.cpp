// The input devices behind dinput.h.
//
// Two devices exist -- a mouse and a keyboard -- and both are fed from the
// Android side rather than from any hardware. Each keeps the buffered stream
// of (offset, value, timestamp) events that the engine's bindings read, plus
// the immediate state its polling path asks for.
#include "dinput.h"

#include <string.h>

#include <deque>
#include <mutex>

namespace {

// ---------------------------------------------------------------------------
// The identifiers the engine asks for. Their values are never interpreted --
// only compared -- so each is simply distinct.
// ---------------------------------------------------------------------------
GUID MakeGuid( DWORD n )
{
    GUID g;
    memset( &g, 0, sizeof( g ) );
    g.Data1 = n;
    return g;
}

// The engine's buffer size request arrives through DIPROP_BUFFERSIZE; this is
// the cap, so a burst of touch events cannot grow the queue without bound.
const size_t MAX_BUFFERED = 4096;

struct SEvent
{
    DWORD nOffset;
    DWORD nValue;
    DWORD nTime;
    DWORD nSequence;
};

std::mutex g_mutex;
DWORD      g_nSequence = 0;

struct SDeviceState
{
    std::deque<SEvent> events;
    size_t             nBufferSize;

    // the immediate state, for the polling path
    LONG lX, lY, lZ;
    BYTE buttons[8];
    BYTE keys[256];

    SDeviceState() : nBufferSize( 64 ), lX( 0 ), lY( 0 ), lZ( 0 )
    {
        memset( buttons, 0, sizeof( buttons ) );
        memset( keys, 0, sizeof( keys ) );
    }
};

SDeviceState g_mouse;
SDeviceState g_keyboard;

SDeviceState &StateFor( int nDevice )
{
    return ( nDevice == BK1_INPUT_KEYBOARD ) ? g_keyboard : g_mouse;
}

// ---------------------------------------------------------------------------
// The device
// ---------------------------------------------------------------------------
struct SInputDevice : public IDirectInputDevice8
{
    int  nDevice;
    LONG nRefCount;
    bool bAcquired;

    explicit SInputDevice( int _nDevice )
        : nDevice( _nDevice ), nRefCount( 1 ), bAcquired( false ) {}

    // --- IUnknown ---
    HRESULT STDCALL QueryInterface( REFIID, void **ppvObject ) override
    {
        if ( ppvObject == 0 )
            return E_INVALIDARG;
        *ppvObject = this;
        ++nRefCount;
        return S_OK;
    }
    ULONG STDCALL AddRef() override { return (ULONG)++nRefCount; }
    ULONG STDCALL Release() override
    {
        const LONG n = --nRefCount;
        if ( n <= 0 )
            delete this;
        return (ULONG)n;
    }

    // --- capabilities ---
    HRESULT STDCALL GetCapabilities( DIDEVCAPS *pCaps ) override
    {
        if ( pCaps == 0 )
            return E_INVALIDARG;
        const DWORD dwSize = pCaps->dwSize;
        memset( pCaps, 0, dwSize );
        pCaps->dwSize = dwSize;
        if ( nDevice == BK1_INPUT_KEYBOARD )
        {
            pCaps->dwDevType = DIDEVTYPE_KEYBOARD;
            pCaps->dwButtons = 256;
        }
        else
        {
            pCaps->dwDevType = DIDEVTYPE_MOUSE;
            pCaps->dwAxes = 3;
            pCaps->dwButtons = 4;
        }
        return DI_OK;
    }

    // The engine enumerates objects to learn which offsets exist and what to
    // call them; those names end up in the key-binding screen.
    HRESULT STDCALL EnumObjects( LPDIENUMDEVICEOBJECTSCALLBACKA pCallback,
                                 void *pRef, DWORD ) override
    {
        if ( pCallback == 0 )
            return E_INVALIDARG;

        DIDEVICEOBJECTINSTANCEA instance;
        if ( nDevice == BK1_INPUT_MOUSE )
        {
            static const struct { DWORD nOfs; DWORD dwType; const char *pszName; } AXES[] = {
                { DIMOFS_X, DIDFT_RELAXIS, "X Axis" },
                { DIMOFS_Y, DIDFT_RELAXIS, "Y Axis" },
                { DIMOFS_Z, DIDFT_RELAXIS, "Wheel" },
                { DIMOFS_BUTTON0, DIDFT_PSHBUTTON, "Button 0" },
                { DIMOFS_BUTTON1, DIDFT_PSHBUTTON, "Button 1" },
                { DIMOFS_BUTTON2, DIDFT_PSHBUTTON, "Button 2" },
                { DIMOFS_BUTTON3, DIDFT_PSHBUTTON, "Button 3" },
            };
            for ( unsigned i = 0; i < sizeof( AXES ) / sizeof( AXES[0] ); ++i )
            {
                memset( &instance, 0, sizeof( instance ) );
                instance.dwSize = sizeof( instance );
                instance.dwOfs = AXES[i].nOfs;
                instance.dwType = AXES[i].dwType;
                instance.guidType = ( AXES[i].dwType == DIDFT_RELAXIS )
                                        ? ( AXES[i].nOfs == DIMOFS_X ? GUID_XAxis
                                            : AXES[i].nOfs == DIMOFS_Y ? GUID_YAxis : GUID_ZAxis )
                                        : GUID_Button;
                snprintf( instance.tszName, sizeof( instance.tszName ), "%s", AXES[i].pszName );
                if ( pCallback( &instance, pRef ) == DIENUM_STOP )
                    break;
            }
            return DI_OK;
        }

        // the keyboard: one object per scan code
        for ( DWORD nKey = 1; nKey < 256; ++nKey )
        {
            memset( &instance, 0, sizeof( instance ) );
            instance.dwSize = sizeof( instance );
            instance.dwOfs = nKey;
            instance.dwType = DIDFT_PSHBUTTON;
            instance.guidType = GUID_Key;
            snprintf( instance.tszName, sizeof( instance.tszName ), "Key %u", (unsigned)nKey );
            if ( pCallback( &instance, pRef ) == DIENUM_STOP )
                break;
        }
        return DI_OK;
    }

    // --- properties ---
    HRESULT STDCALL GetProperty( const GUID *pProp, DIPROPHEADER *pHeader ) override
    {
        if ( pProp == DIPROP_BUFFERSIZE && pHeader != 0 )
        {
            std::lock_guard<std::mutex> lock( g_mutex );
            ( (DIPROPDWORD *)pHeader )->dwData = (DWORD)StateFor( nDevice ).nBufferSize;
            return DI_OK;
        }
        return DIERR_UNSUPPORTED;
    }

    HRESULT STDCALL SetProperty( const GUID *pProp, const DIPROPHEADER *pHeader ) override
    {
        if ( pProp == DIPROP_BUFFERSIZE && pHeader != 0 )
        {
            std::lock_guard<std::mutex> lock( g_mutex );
            size_t n = ( (const DIPROPDWORD *)pHeader )->dwData;
            if ( n > MAX_BUFFERED )
                n = MAX_BUFFERED;
            StateFor( nDevice ).nBufferSize = n;
            return DI_OK;
        }
        // the axis mode and the ranges have no meaning for a fed device
        return DI_OK;
    }

    // --- acquisition ---
    HRESULT STDCALL Acquire() override
    {
        bAcquired = true;
        return DI_OK;
    }

    HRESULT STDCALL Unacquire() override
    {
        bAcquired = false;
        return DI_OK;
    }

    // --- immediate state ---
    HRESULT STDCALL GetDeviceState( DWORD cbData, void *pData ) override
    {
        if ( pData == 0 )
            return E_INVALIDARG;
        if ( !bAcquired )
            return DIERR_NOTACQUIRED;

        std::lock_guard<std::mutex> lock( g_mutex );
        SDeviceState &state = StateFor( nDevice );

        if ( nDevice == BK1_INPUT_KEYBOARD )
        {
            const DWORD n = cbData < sizeof( state.keys ) ? cbData : (DWORD)sizeof( state.keys );
            memcpy( pData, state.keys, n );
            return DI_OK;
        }

        // The axes are relative, so reading them consumes the movement, as
        // DirectInput's relative mode does.
        DIMOUSESTATE2 mouse;
        memset( &mouse, 0, sizeof( mouse ) );
        mouse.lX = state.lX;
        mouse.lY = state.lY;
        mouse.lZ = state.lZ;
        memcpy( mouse.rgbButtons, state.buttons, sizeof( mouse.rgbButtons ) );
        state.lX = state.lY = state.lZ = 0;

        const DWORD n = cbData < sizeof( mouse ) ? cbData : (DWORD)sizeof( mouse );
        memcpy( pData, &mouse, n );
        return DI_OK;
    }

    // --- the buffered stream ---
    HRESULT STDCALL GetDeviceData( DWORD cbObjectData, DIDEVICEOBJECTDATA *rgdod,
                                   DWORD *pdwInOut, DWORD dwFlags ) override
    {
        if ( pdwInOut == 0 )
            return E_INVALIDARG;
        if ( !bAcquired )
            return DIERR_NOTACQUIRED;

        std::lock_guard<std::mutex> lock( g_mutex );
        SDeviceState &state = StateFor( nDevice );

        // A null buffer asks how much is waiting.
        if ( rgdod == 0 )
        {
            *pdwInOut = (DWORD)state.events.size();
            return DI_OK;
        }

        DWORD nCopied = 0;
        unsigned char *pOut = (unsigned char *)rgdod;
        while ( nCopied < *pdwInOut && !state.events.empty() )
        {
            const SEvent &e = state.events.front();
            DIDEVICEOBJECTDATA *pEntry = (DIDEVICEOBJECTDATA *)( pOut + nCopied * cbObjectData );
            pEntry->dwOfs = e.nOffset;
            pEntry->dwData = e.nValue;
            pEntry->dwTimeStamp = e.nTime;
            pEntry->dwSequence = e.nSequence;
            pEntry->uAppData = 0;
            ++nCopied;
            // DIGDD_PEEK leaves the events in place; anything else consumes.
            if ( ( dwFlags & 1 ) == 0 )
                state.events.pop_front();
            else
                break;
        }
        *pdwInOut = nCopied;
        return DI_OK;
    }

    HRESULT STDCALL SetDataFormat( const DIDATAFORMAT * ) override { return DI_OK; }
    HRESULT STDCALL SetCooperativeLevel( HWND, DWORD ) override { return DI_OK; }
    HRESULT STDCALL Poll() override { return DI_OK; }
};

// ---------------------------------------------------------------------------
// The factory
// ---------------------------------------------------------------------------
struct SDirectInput : public IDirectInput8
{
    LONG nRefCount;

    SDirectInput() : nRefCount( 1 ) {}

    HRESULT STDCALL QueryInterface( REFIID, void **ppvObject ) override
    {
        if ( ppvObject == 0 )
            return E_INVALIDARG;
        *ppvObject = this;
        ++nRefCount;
        return S_OK;
    }
    ULONG STDCALL AddRef() override { return (ULONG)++nRefCount; }
    ULONG STDCALL Release() override
    {
        const LONG n = --nRefCount;
        if ( n <= 0 )
            delete this;
        return (ULONG)n;
    }

    HRESULT STDCALL CreateDevice( const GUID &guid, IDirectInputDevice8 **ppDevice,
                                  IUnknown * ) override
    {
        if ( ppDevice == 0 )
            return E_INVALIDARG;
        const int nDevice = ( guid == GUID_SysKeyboard ) ? BK1_INPUT_KEYBOARD
                                                         : BK1_INPUT_MOUSE;
        *ppDevice = new SInputDevice( nDevice );
        return DI_OK;
    }

    HRESULT STDCALL EnumDevices( DWORD dwDevType, LPDIENUMDEVICESCALLBACKA pCallback,
                                 void *pRef, DWORD ) override
    {
        if ( pCallback == 0 )
            return E_INVALIDARG;

        DIDEVICEINSTANCEA instance;
        const bool bWantMouse = ( dwDevType == DI8DEVCLASS_ALL ||
                                  dwDevType == DI8DEVCLASS_POINTER ||
                                  dwDevType == DI8DEVTYPE_MOUSE );
        const bool bWantKeyboard = ( dwDevType == DI8DEVCLASS_ALL ||
                                     dwDevType == DI8DEVCLASS_KEYBOARD ||
                                     dwDevType == DI8DEVTYPE_KEYBOARD );

        if ( bWantMouse )
        {
            memset( &instance, 0, sizeof( instance ) );
            instance.dwSize = sizeof( instance );
            instance.guidInstance = GUID_SysMouse;
            instance.guidProduct = GUID_SysMouse;
            instance.dwDevType = DIDEVTYPE_MOUSE;
            snprintf( instance.tszInstanceName, sizeof( instance.tszInstanceName ), "Touch" );
            snprintf( instance.tszProductName, sizeof( instance.tszProductName ), "Touch" );
            if ( pCallback( &instance, pRef ) == DIENUM_STOP )
                return DI_OK;
        }
        if ( bWantKeyboard )
        {
            memset( &instance, 0, sizeof( instance ) );
            instance.dwSize = sizeof( instance );
            instance.guidInstance = GUID_SysKeyboard;
            instance.guidProduct = GUID_SysKeyboard;
            instance.dwDevType = DIDEVTYPE_KEYBOARD;
            snprintf( instance.tszInstanceName, sizeof( instance.tszInstanceName ), "Keyboard" );
            snprintf( instance.tszProductName, sizeof( instance.tszProductName ), "Keyboard" );
            pCallback( &instance, pRef );
        }
        return DI_OK;
    }
};

// The prepared formats' object tables. The engine reads the offsets out of
// these to learn its bindings.
DIOBJECTDATAFORMAT g_mouseObjects[] = {
    { &GUID_XAxis, DIMOFS_X, DIDFT_RELAXIS, 0 },
    { &GUID_YAxis, DIMOFS_Y, DIDFT_RELAXIS, 0 },
    { &GUID_ZAxis, DIMOFS_Z, DIDFT_RELAXIS, 0 },
    { &GUID_Button, DIMOFS_BUTTON0, DIDFT_PSHBUTTON, 0 },
    { &GUID_Button, DIMOFS_BUTTON1, DIDFT_PSHBUTTON, 0 },
    { &GUID_Button, DIMOFS_BUTTON2, DIDFT_PSHBUTTON, 0 },
    { &GUID_Button, DIMOFS_BUTTON3, DIDFT_PSHBUTTON, 0 },
};

DIOBJECTDATAFORMAT g_keyboardObjects[256];

struct SKeyboardObjectsInit
{
    SKeyboardObjectsInit()
    {
        for ( int i = 0; i < 256; ++i )
        {
            g_keyboardObjects[i].pguid = &GUID_Key;
            g_keyboardObjects[i].dwOfs = (DWORD)i;
            g_keyboardObjects[i].dwType = DIDFT_PSHBUTTON;
            g_keyboardObjects[i].dwFlags = 0;
        }
    }
};
SKeyboardObjectsInit g_keyboardObjectsInit;

}   // anonymous namespace

// ---------------------------------------------------------------------------
// The published identifiers and formats
// ---------------------------------------------------------------------------
const GUID GUID_SysMouse    = MakeGuid( 1 );
const GUID GUID_SysKeyboard = MakeGuid( 2 );
const GUID GUID_XAxis       = MakeGuid( 3 );
const GUID GUID_YAxis       = MakeGuid( 4 );
const GUID GUID_ZAxis       = MakeGuid( 5 );
const GUID GUID_Button      = MakeGuid( 6 );
const GUID GUID_Key         = MakeGuid( 7 );
// The remaining object types the engine compares against while classifying the
// axes of a controller it enumerated. Their values only have to differ from
// one another: they are never matched against anything outside this port, and
// on Android the enumeration finds no such device, so the comparisons that
// read them are never reached.
const GUID GUID_RxAxis      = MakeGuid( 8 );
const GUID GUID_RyAxis      = MakeGuid( 9 );
const GUID GUID_RzAxis      = MakeGuid( 10 );
const GUID GUID_Slider      = MakeGuid( 11 );
const GUID GUID_POV         = MakeGuid( 12 );

DIDATAFORMAT c_dfDIMouse = {
    sizeof( DIDATAFORMAT ), sizeof( DIOBJECTDATAFORMAT ), DIDF_RELAXIS,
    sizeof( DIMOUSESTATE ), 7, g_mouseObjects
};

DIDATAFORMAT c_dfDIMouse2 = {
    sizeof( DIDATAFORMAT ), sizeof( DIOBJECTDATAFORMAT ), DIDF_RELAXIS,
    sizeof( DIMOUSESTATE2 ), 7, g_mouseObjects
};

DIDATAFORMAT c_dfDIKeyboard = {
    sizeof( DIDATAFORMAT ), sizeof( DIOBJECTDATAFORMAT ), DIDF_RELAXIS,
    256, 256, g_keyboardObjects
};

extern "C" {

HRESULT DirectInput8Create( HINSTANCE, DWORD, const GUID &, void **ppOut, IUnknown * )
{
    if ( ppOut == 0 )
        return E_INVALIDARG;
    *ppOut = new SDirectInput();
    return DI_OK;
}

HRESULT DirectInputCreate( HINSTANCE, DWORD, IDirectInput8 **ppOut, IUnknown * )
{
    if ( ppOut == 0 )
        return E_INVALIDARG;
    *ppOut = new SDirectInput();
    return DI_OK;
}

void Bk1PushInputEvent( int nDevice, DWORD nOffset, DWORD nValue )
{
    std::lock_guard<std::mutex> lock( g_mutex );
    SDeviceState &state = StateFor( nDevice );

    // keep the immediate state in step with the stream
    if ( nDevice == BK1_INPUT_KEYBOARD )
    {
        if ( nOffset < 256 )
            state.keys[nOffset] = ( nValue & 0x80 ) ? 0x80 : 0;
    }
    else
    {
        switch ( nOffset )
        {
        case DIMOFS_X: state.lX += (LONG)(int)nValue; break;
        case DIMOFS_Y: state.lY += (LONG)(int)nValue; break;
        case DIMOFS_Z: state.lZ += (LONG)(int)nValue; break;
        default:
            if ( nOffset >= DIMOFS_BUTTON0 && nOffset <= DIMOFS_BUTTON7 )
                state.buttons[nOffset - DIMOFS_BUTTON0] = ( nValue & 0x80 ) ? 0x80 : 0;
            break;
        }
    }

    SEvent e;
    e.nOffset = nOffset;
    e.nValue = nValue;
    e.nTime = GetTickCount();
    e.nSequence = ++g_nSequence;
    state.events.push_back( e );

    // The oldest events go first when the buffer is full, which is what
    // DirectInput does when it overflows.
    while ( state.events.size() > state.nBufferSize )
        state.events.pop_front();
}

void Bk1ClearInputEvents( void )
{
    std::lock_guard<std::mutex> lock( g_mutex );
    g_mouse.events.clear();
    g_keyboard.events.clear();
    g_mouse.lX = g_mouse.lY = g_mouse.lZ = 0;
    memset( g_mouse.buttons, 0, sizeof( g_mouse.buttons ) );
    memset( g_keyboard.keys, 0, sizeof( g_keyboard.keys ) );
}

}   // extern "C"
