// The COM apartment and activation calls the machine probe makes.
//
// There is no component object model here. Initialising an apartment succeeds
// because nothing depends on it, and asking for a class fails because there is
// no class factory to ask -- which is exactly what the probe expects from a
// machine that does not have the component installed, and it already handles
// that answer.
#include "dmusici.h"

extern "C" {

HRESULT CoInitialize( void * ) { return S_OK; }
HRESULT CoInitializeEx( void *, DWORD ) { return S_OK; }
void CoUninitialize( void ) {}

HRESULT CoCreateInstance( const CLSID &, IUnknown *, DWORD, const IID &, void **ppv )
{
    if ( ppv != 0 )
        *ppv = 0;
    return REGDB_E_CLASSNOTREG;
}

}   // extern "C"
