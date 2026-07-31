#pragma once
// The Windows registry calls the engine uses to keep its settings.
//
// Blitzkrieg stores its installation path and a handful of options under
// HKEY_LOCAL_MACHINE and HKEY_CURRENT_USER. That is real persistent state, not
// something to answer with a constant, so this is a small registry of its own:
// an in-memory tree of keys and values, loaded from and written back to one
// file. Call Bk1SetRegistryFile before anything reads a key -- on Android that
// is a path inside the application's own storage.
#include "bk1_win32_types.h"

typedef struct _BK1_REGKEY *HKEY;
typedef HKEY *PHKEY;
typedef DWORD REGSAM;
typedef BYTE *LPBYTE;

// The predefined roots. They are ordinary keys here, named by these handles.
#define HKEY_CLASSES_ROOT     ( (HKEY)(size_t)0x80000000 )
#define HKEY_CURRENT_USER     ( (HKEY)(size_t)0x80000001 )
#define HKEY_LOCAL_MACHINE    ( (HKEY)(size_t)0x80000002 )
#define HKEY_USERS            ( (HKEY)(size_t)0x80000003 )
#define HKEY_CURRENT_CONFIG   ( (HKEY)(size_t)0x80000005 )

// --- access rights. Nothing is enforced; they are recorded and ignored. ---
#define KEY_QUERY_VALUE       0x0001
#define KEY_SET_VALUE         0x0002
#define KEY_CREATE_SUB_KEY    0x0004
#define KEY_ENUMERATE_SUB_KEYS 0x0008
#define KEY_NOTIFY            0x0010
#define KEY_READ              0x20019
#define KEY_WRITE             0x20006
#define KEY_ALL_ACCESS        0xF003F

// --- value types ---
#define REG_NONE              0
#define REG_SZ                1
#define REG_EXPAND_SZ         2
#define REG_BINARY            3
#define REG_DWORD             4
#define REG_MULTI_SZ          7

// --- options and dispositions ---
#define REG_OPTION_NON_VOLATILE 0x00000000
#define REG_OPTION_VOLATILE     0x00000001
#define REG_CREATED_NEW_KEY     0x00000001
#define REG_OPENED_EXISTING_KEY 0x00000002

#ifndef ERROR_MORE_DATA
#define ERROR_MORE_DATA       234
#endif
#ifndef ERROR_NO_MORE_ITEMS
#define ERROR_NO_MORE_ITEMS   259
#endif
#ifndef ERROR_INVALID_DATA
#define ERROR_INVALID_DATA    13
#endif
#ifndef ERROR_INVALID_HANDLE
#define ERROR_INVALID_HANDLE  6
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Where the registry is kept. Until this is called the registry lives only in
// memory, which is what a test wants and what a first run gets.
void Bk1SetRegistryFile( const char *pszPath );
// Writes any pending change out. Called on every set as well, so a crash does
// not lose the last option the player changed.
void Bk1FlushRegistry( void );

LONG RegCreateKeyExA( HKEY hKey, const char *pszSubKey, DWORD dwReserved,
                      char *pszClass, DWORD dwOptions, REGSAM samDesired,
                      void *pSecurityAttributes, PHKEY phkResult,
                      DWORD *pdwDisposition );
LONG RegOpenKeyExA( HKEY hKey, const char *pszSubKey, DWORD dwOptions,
                    REGSAM samDesired, PHKEY phkResult );
LONG RegCloseKey( HKEY hKey );
LONG RegQueryValueExA( HKEY hKey, const char *pszValueName, DWORD *pdwReserved,
                       DWORD *pdwType, LPBYTE pData, DWORD *pcbData );
LONG RegSetValueExA( HKEY hKey, const char *pszValueName, DWORD dwReserved,
                     DWORD dwType, const BYTE *pData, DWORD cbData );
LONG RegDeleteKeyA( HKEY hKey, const char *pszSubKey );
LONG RegDeleteValueA( HKEY hKey, const char *pszValueName );
LONG RegEnumKeyExA( HKEY hKey, DWORD dwIndex, char *pszName, DWORD *pcchName,
                    DWORD *pdwReserved, char *pszClass, DWORD *pcchClass,
                    void *pftLastWriteTime );
LONG RegEnumValueA( HKEY hKey, DWORD dwIndex, char *pszValueName,
                    DWORD *pcchValueName, DWORD *pdwReserved, DWORD *pdwType,
                    LPBYTE pData, DWORD *pcbData );

#ifdef __cplusplus
}
#endif

#define RegCreateKeyEx  RegCreateKeyExA
#define RegOpenKeyEx    RegOpenKeyExA
#define RegQueryValueEx RegQueryValueExA
#define RegSetValueEx   RegSetValueExA
#define RegDeleteKey    RegDeleteKeyA
#define RegDeleteValue  RegDeleteValueA
#define RegEnumKeyEx    RegEnumKeyExA
#define RegEnumValue    RegEnumValueA
