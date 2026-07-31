#pragma once
// The scalar types, handles and macros the engine takes from <windows.h>.
// Every module's StdAfx.h reaches for them, so they are defined once here and
// the original sources keep their spelling.
#include <cstdint>
#include <cstddef>

typedef unsigned char       BYTE;
typedef unsigned short      WORD;
typedef unsigned int        DWORD;
typedef int                 BOOL;
typedef int                 INT;
typedef unsigned int        UINT;
typedef long                LONG;
typedef unsigned long       ULONG;
typedef float               FLOAT;
typedef char                CHAR;
typedef wchar_t             WCHAR;
typedef const char*         LPCSTR;
typedef char*               LPSTR;
typedef const wchar_t*      LPCWSTR;
typedef wchar_t*            LPWSTR;
typedef void*               LPVOID;
typedef const void*         LPCVOID;
typedef UINT*               LPUINT;
typedef DWORD*              LPDWORD;
typedef char*               LPTSTR;
typedef const char*         LPCTSTR;
typedef char                TCHAR;

// Handles are opaque everywhere the engine touches them; keeping them
// pointer-sized is what matters on arm64.
typedef void*               HANDLE;
typedef void*               HWND;
typedef void*               HINSTANCE;
typedef void*               HMODULE;
typedef void*               HDC;
typedef void*               HBITMAP;
typedef void*               HCURSOR;
typedef void*               HICON;
typedef long                HRESULT;

#ifndef TRUE
#define TRUE  1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define WINAPI
#define CALLBACK
#define APIENTRY
#define FAR
#define NEAR

#define MAKEWORD(a, b) \
    ((WORD)(((BYTE)(a)) | ((WORD)((BYTE)(b))) << 8))
#define MAKELONG(a, b) \
    ((LONG)(((WORD)(a)) | ((DWORD)((WORD)(b))) << 16))
#define LOWORD(l) ((WORD)((DWORD)(l) & 0xffff))
#define HIWORD(l) ((WORD)(((DWORD)(l) >> 16) & 0xffff))
#define LOBYTE(w) ((BYTE)((DWORD)(w) & 0xff))
#define HIBYTE(w) ((BYTE)(((DWORD)(w) >> 8) & 0xff))

#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#define FAILED(hr)    (((HRESULT)(hr)) < 0)
#define S_OK          ((HRESULT)0)
#define E_FAIL        ((HRESULT)0x80004005L)

// IStream's seek origin, which StreamIO/StreamIO.h names in its own interface.
typedef enum tagSTREAM_SEEK {
    STREAM_SEEK_SET = 0,
    STREAM_SEEK_CUR = 1,
    STREAM_SEEK_END = 2
} STREAM_SEEK;

// GDI geometry structures. Misc/Geometry.h's GPoint and GRect convert to and
// from these, so the member names and their order have to match Windows.
typedef struct tagPOINT {
    LONG x;
    LONG y;
} POINT, *PPOINT, *LPPOINT;

typedef struct tagSIZE {
    LONG cx;
    LONG cy;
} SIZE, *PSIZE, *LPSIZE;

typedef struct tagRECT {
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
} RECT, *PRECT, *LPRECT;
typedef const RECT* LPCRECT;

// BugSlayer's entry points take the structured-exception record by pointer and
// never look inside it off Windows.
struct EXCEPTION_RECORD;
struct CONTEXT;
typedef struct _EXCEPTION_POINTERS {
    struct EXCEPTION_RECORD* ExceptionRecord;
    struct CONTEXT*          ContextRecord;
} EXCEPTION_POINTERS, *PEXCEPTION_POINTERS, *LPEXCEPTION_POINTERS;

// Declared last, so the types above are in scope. The engine reaches these
// through the force-included header rather than through <windows.h>, which
// the game modules' StdAfx.h never includes.
#include "bk1_win32_strings.h"
#include "bk1_win32_platform.h"
#include "bk1_win32_files.h"
#include "bk1_win32_fileio.h"
