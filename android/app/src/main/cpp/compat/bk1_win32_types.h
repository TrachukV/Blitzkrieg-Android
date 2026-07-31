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

// BugSlayer's entry points take the structured-exception record by pointer and
// never look inside it off Windows.
struct EXCEPTION_RECORD;
struct CONTEXT;
typedef struct _EXCEPTION_POINTERS {
    struct EXCEPTION_RECORD* ExceptionRecord;
    struct CONTEXT*          ContextRecord;
} EXCEPTION_POINTERS, *PEXCEPTION_POINTERS, *LPEXCEPTION_POINTERS;
