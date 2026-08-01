// The Android replacement for Main/LoadDLLs.cpp.
//
// Blitzkrieg was twenty DLLs. That file found them by scanning the game
// directory for *.dll, loading each one, pulling its exported
// GetModuleDescriptor out with GetProcAddress, and registering the descriptor
// and object factory it returned. Which modules existed was a question about
// the filesystem, answered at run time.
//
// An Android package is one shared library. Every module is already linked in,
// so the same question has a compile-time answer, and asking the filesystem
// would only be a way of getting it wrong.
//
// This keeps LoadDLLs.cpp's interface exactly -- NMain::LoadAllModules,
// GetModuleDesc, the iteration pair, UnloadAllModules -- and the static object
// at the bottom that runs before main, because the engine's own static
// initialisers depend on the factories being registered by the time they run.
// What changes is only where the descriptors come from.
//
// Each module still defines GetModuleDescriptor under that name; the build
// gives each one a distinct symbol through a per-source definition, which is
// why the engine sources need no edit at all.
#include "compat/bk1_msvc_types.h"

#include <android/log.h>

#include <string>
#include <vector>

#include "../../../../../Sources/src/Main/StdAfx.h"
#include "../../../../../Sources/src/Misc/Win32Helper.h"

#define LOG_TAG "Blitzkrieg"
#define LOGI( ... ) __android_log_print( ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__ )

// The eleven modules that were DLLs and carry a descriptor. The others --
// Misc, Formats, Common, zlib, GameSpy, LuaLib, RandomMapGen -- were static
// libraries linked into these, and have none.
#define BK1_MODULE_LIST( X )  \
    X( StreamIO )             \
    X( Image )                \
    X( Anim )                 \
    X( Net )                  \
    X( Scene )                \
    X( UI )                   \
    X( GFX )                  \
    X( Input )                \
    X( GameTT )               \
    X( AILogic )              \
    X( SFX )

// Not extern "C": the rename is a macro over the definition, so what each
// module emits keeps C++ linkage and this has to match it.
#define BK1_DECLARE( MODULE ) \
    const SModuleDescriptor* STDCALL Bk1GetModuleDescriptor_##MODULE();
BK1_MODULE_LIST( BK1_DECLARE )
#undef BK1_DECLARE

namespace NMain {

namespace {

struct SLinkedModule
{
    const SModuleDescriptor *pDesc;
    const char              *pszModule;
};

std::vector<SLinkedModule> g_modules;
int g_nModuleIndex = 0;

}   // anonymous namespace

// StreamIO first, and that is not cosmetic: it owns the save/load system and
// the singleton registry that every other module's factory registers into.
// LoadDLLs.cpp got this ordering by accident of directory enumeration; here it
// is stated.
int STDCALL LoadAllModules( const char * )
{
    if ( !g_modules.empty() )
        return (int)g_modules.size();

#define BK1_COLLECT( MODULE )                                              \
    {                                                                      \
        const SModuleDescriptor *pDesc = Bk1GetModuleDescriptor_##MODULE(); \
        if ( pDesc != 0 && pDesc->pFactory != 0 )                          \
        {                                                                  \
            SLinkedModule module;                                          \
            module.pDesc = pDesc;                                          \
            module.pszModule = #MODULE;                                    \
            g_modules.push_back( module );                                 \
        }                                                                  \
    }
    BK1_MODULE_LIST( BK1_COLLECT )
#undef BK1_COLLECT

    LOGI( "%d modules linked in", (int)g_modules.size() );
    return (int)g_modules.size();
}

const SModuleDescriptor* STDCALL GetModuleDesc( int nType )
{
    for ( size_t i = 0; i < g_modules.size(); ++i )
    {
        if ( g_modules[i].pDesc->nType == nType )
            return g_modules[i].pDesc;
    }
    NI_ASSERT_T( false, NStr::Format( "can't find module of type 0x%.8x", nType ) );
    return 0;
}

void STDCALL UnloadAllModules()
{
    // No libraries to let go of; what remains is the singleton teardown, which
    // LoadDLLs.cpp did here too.
    if ( ISingleton *pSingleton = GetSingletonGlobal() )
        pSingleton->Done();
    g_modules.clear();
}

namespace {

const SModuleDescriptor* GetModuleByIndex( int nIndex )
{
    if ( nIndex < 0 || nIndex >= (int)g_modules.size() )
        return 0;
    return g_modules[nIndex].pDesc;
}

}   // anonymous namespace

const SModuleDescriptor* GetFirstModuleDesc()
{
    g_nModuleIndex = 0;
    return GetModuleByIndex( g_nModuleIndex );
}

const SModuleDescriptor* GetNextModuleDesc()
{
    ++g_nModuleIndex;
    return GetModuleByIndex( g_nModuleIndex );
}

// The engine asks this so it can show which file a module came from. They all
// come from the same one now, and saying so is more truthful than inventing
// the DLL name it used to have.
const std::string GetModuleFileNameByDesc( const SModuleDescriptor *pModule )
{
    for ( size_t i = 0; i < g_modules.size(); ++i )
    {
        if ( g_modules[i].pDesc == pModule )
            return std::string( "libblitzkrieg.so:" ) + g_modules[i].pszModule;
    }
    return "";
}

// On Windows this read the install folder out of the registry and made it
// current. An Android package has no install folder to find and no working
// directory worth changing -- the data path is handed to Bk1GameStartup.
bool SetGameDirectory()
{
    return true;
}

}   // namespace NMain

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------
// LoadDLLs.cpp did this from a static object, and the first version here did
// too. It found nothing:
//
//     I Blitzkrieg: 0 modules linked in
//
// Each module's descriptor is itself a static object in that module's own
// translation unit, and inside one binary nothing orders those against this
// one. In twenty DLLs the loader did: a module's statics were all constructed
// before the module that looked them up was loaded at all.
//
// So it is not a static initialiser any more. Bk1GameStartup calls it, which
// is after every static initialiser in the library has run and before any
// engine code asks for a factory.
void Bk1RegisterModules()
{
    NMain::SetGameDirectory();
    NMain::LoadAllModules( 0 );

    for ( const SModuleDescriptor *pDesc = NMain::GetFirstModuleDesc();
          pDesc != 0; pDesc = NMain::GetNextModuleDesc() )
    {
        if ( pDesc->pFactory != 0 )
            GetSLS()->AddFactory( pDesc->pFactory );
        if ( pDesc->pChecker != 0 )
            pDesc->pChecker->SetModuleFunctionalityLimits();
    }
}
