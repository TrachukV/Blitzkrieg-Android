// The Android counterpart of Game/main.cpp's WinMain.
//
// That file is the Windows shell: it creates a window, pumps a message queue,
// installs a keyboard hook and puts up a splash bitmap. None of that has a
// meaning here -- android_main owns the window and the event loop -- but the
// startup order inside it does, and it is exact. The engine's subsystems
// register singletons that later ones look up, so a step taken out of turn
// finds a null and dies well away from the cause.
//
// So this mirrors WinMain step for step, and where a step is Windows-only it
// says so rather than quietly dropping it.
#include "compat/bk1_msvc_types.h"
#include "compat/bk1_win32_files.h"

#include <android/log.h>
#include <dirent.h>
#include <string.h>

#include <string>
#include <vector>

#include "../../../../../Sources/src/Main/StdAfx.h"

#include "../../../../../Sources/src/GFX/GFX.h"
#include "../../../../../Sources/src/SFX/SFX.h"
#include "../../../../../Sources/src/Input/Input.h"
#include "../../../../../Sources/src/Scene/Scene.h"
#include "../../../../../Sources/src/GameTT/iMission.h"
#include "../../../../../Sources/src/GameTT/CutScenesHelper.h"
#include "../../../../../Sources/src/Misc/FileUtils.h"
#include "../../../../../Sources/src/StreamIO/OptionSystem.h"
#include "../../../../../Sources/src/StreamIO/RandomGen.h"
#include "../../../../../Sources/src/Main/iMain.h"
#include "../../../../../Sources/src/Main/GameDB.h"
#include "../../../../../Sources/src/Net/NetDriver.h"
#include "../../../../../Sources/src/Main/ScenarioTracker.h"
#include "../../../../../Sources/src/Main/CommandsHistoryInterface.h"

#include "bk1_game_startup.h"

#define LOG_TAG "Blitzkrieg"
#define LOGI( ... ) __android_log_print( ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__ )
#define LOGE( ... ) __android_log_print( ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__ )

namespace {

IMainLoop *g_pMainLoop = 0;
bool       g_bStarted = false;
bool       g_bFinished = false;

}   // anonymous namespace

bool Bk1GameIsRunning() { return g_bStarted && !g_bFinished; }
bool Bk1GameHasFinished() { return g_bFinished; }

// ---------------------------------------------------------------------------
// Startup
// ---------------------------------------------------------------------------
bool Bk1GameStartup( const char *pszDataDirectory, int nSurfaceWidth, int nSurfaceHeight )
{
    if ( g_bStarted )
        return true;

    LOGI( "starting: data at %s, surface %dx%d",
          pszDataDirectory != 0 ? pszDataDirectory : "(none)",
          nSurfaceWidth, nSurfaceHeight );

    // First, because every subsystem below looks objects up through these
    // factories. In the original this happened when the DLLs loaded.
    Bk1RegisterModules();

    // --- console and log ---
    // WinMain deletes log.txt and error.txt beside the executable and points
    // the console buffer at them. Here they go next to the data, which is the
    // only directory the app can be sure it may write to.
    // The engine's paths are Windows paths, and it does its own arithmetic on
    // them -- CZipFileSystem and CCommonFileSystem both find the directory part
    // by searching for a backslash. Handed a path with forward slashes they
    // find none, decide the whole thing is a filename, and look in the current
    // directory instead; that is what left CZipFile::Init with a null stream.
    //
    // So the engine is given its root the way Windows would give it, and
    // Bk1HostPath translates back at the one boundary where a path becomes a
    // system call. Every path the engine builds on top of this stays in the
    // shape the engine expects.
    std::string szRoot = ( pszDataDirectory != 0 ) ? pszDataDirectory : ".";
    for ( size_t i = 0; i < szRoot.size(); ++i )
    {
        if ( szRoot[i] == '/' )
            szRoot[i] = '\\';
    }
    const std::string szLogFileName = szRoot + "\\log.txt";
    DeleteFile( szLogFileName.c_str() );

    if ( IConsoleBuffer *pConsole = GetSingleton<IConsoleBuffer>() )
    {
        pConsole->Configure( NStr::Format( "logfile;%s", szLogFileName.c_str() ) );
        pConsole->Configure( NStr::Format( "name;%d;World Commands", CONSOLE_STREAM_WORLD ) );
        pConsole->Configure( NStr::Format( "name;%d;Script Commands", CONSOLE_STREAM_SCRIPT ) );
        pConsole->Configure( NStr::Format( "name;%d;Console Feedbacks", CONSOLE_STREAM_CONSOLE ) );
        pConsole->Configure( NStr::Format( "name;%d;Console Commands", CONSOLE_STREAM_COMMAND ) );
        pConsole->Configure( NStr::Format( "name;%d;Chat", CONSOLE_STREAM_CHAT ) );
        pConsole->Configure( NStr::Format( "dublicate;%d;%d", CONSOLE_STREAM_CHAT, CONSOLE_STREAM_CONSOLE ) );
    }

    // --- random ---
    // There is no command line to process; the defaults WinMain would have
    // been left with are what apply.
    GetSingleton<IRandomGen>()->Init();

    // NWinFrame::InitApplication registered a window class and created the
    // window. android_main already has one, and the compatibility layer
    // answers for it.

    // --- the resource system ---
    // Everything the game reads comes out of these archives, and every later
    // step assumes the storage singleton is registered.
    {
        // Checked before opening, not after. OpenStorage hands back a storage
        // for a pattern that matches nothing, and every later step then fails
        // far away from the cause -- the first attempt at this crashed deep
        // inside CDataTableXML::Open on a null stream, which says nothing at
        // all about the real problem being an empty directory.
        const std::string szDataDirectory = szRoot + "\\data";
        const std::string szHostDataDirectory = Bk1HostPath( szDataDirectory.c_str() );
        // Either form counts. The shipped game packs its data into .pak
        // archives, but Nival's source release carries the same tree loose,
        // and CCommonFileSystem opens both -- a zip storage over the pattern
        // and a plain file storage over the directory beside it. Requiring an
        // archive would refuse data the engine reads perfectly well.
        int nArchives = 0;
        int nEntries = 0;
        if ( DIR *pDir = opendir( szHostDataDirectory.c_str() ) )
        {
            while ( dirent *pEntry = readdir( pDir ) )
            {
                if ( strcmp( pEntry->d_name, "." ) == 0 ||
                     strcmp( pEntry->d_name, ".." ) == 0 )
                    continue;
                ++nEntries;
                const char *pszDot = strrchr( pEntry->d_name, '.' );
                if ( pszDot != 0 && strcasecmp( pszDot, ".pak" ) == 0 )
                    ++nArchives;
            }
            closedir( pDir );
        }
        if ( nEntries == 0 )
        {
            LOGE( "no game data. Expected the game's Data tree here, either as" );
            LOGE( "*.pak archives or loose, both of which the engine reads:" );
            LOGE( "    %s", szHostDataDirectory.c_str() );
            LOGE( "This port ships no game content -- it belongs to whoever owns" );
            LOGE( "a copy of the game. Copy the Data directory across and start" );
            LOGE( "again. adb push <game>/Data/. %s/", szHostDataDirectory.c_str() );
            return false;
        }
        LOGI( "%d entries in %s, %d of them archives",
              nEntries, szHostDataDirectory.c_str(), nArchives );

        const std::string szPattern = szDataDirectory + "\\*.pak";
        CPtr<IDataStorage> pStorage =
            OpenStorage( szPattern.c_str(), STREAM_ACCESS_READ, STORAGE_TYPE_MOD );
        if ( pStorage == 0 )
        {
            LOGE( "the archives are there but could not be opened: %s",
                  szPattern.c_str() );
            return false;
        }
        RegisterSingleton( IDataStorage::tidTypeID, pStorage );
    }

    // --- the constants ---
    // consts.xml feeds the global variables that everything below reads. This
    // and the three steps after it were missing from the first version of this
    // file, which is why NMain::Initialize came back with no object database:
    // WinMain does them here, before it, and I had read around them.
    {
        CTableAccessor table = NDB::OpenDataTable( "consts.xml" );
        NMain::SetupGlobalVarConsts( table );
    }

    // The video mode the interface lays itself out against. There is one mode
    // here -- the surface Android gave us -- so mission and intermission are
    // the same, where Windows had a pair the player could switch between.
    //
    // Windowed, and that is not a compromise. The engine's fullscreen path
    // picks a display mode out of the adapter's list; Android has no modes to
    // pick from, and the surface size is already decided. Its windowed path
    // takes the size it is given, which is the truthful description of what
    // this is. Asking for fullscreen made it fall back to 1024x768 while the
    // interface laid itself out at 2700x1280, and the two disagreed on screen.
    SetGlobalVar( "GFX.Mode.Mission.SizeX", nSurfaceWidth );
    SetGlobalVar( "GFX.Mode.Mission.SizeY", nSurfaceHeight );
    SetGlobalVar( "GFX.Mode.Mission.BPP", 32 );
    SetGlobalVar( "GFX.Mode.Mission.Stencil", 8 );
    SetGlobalVar( "GFX.Mode.Mission.FullScreen", int( GFXFS_WINDOWED ) );
    SetGlobalVar( "GFX.Mode.Mission.Frequency", 0 );
    SetGlobalVar( "GFX.Mode.InterMission.SizeX", nSurfaceWidth );
    SetGlobalVar( "GFX.Mode.InterMission.SizeY", nSurfaceHeight );
    SetGlobalVar( "GFX.Mode.InterMission.BPP", 32 );
    SetGlobalVar( "GFX.Mode.InterMission.Stencil", 8 );
    SetGlobalVar( "GFX.Mode.InterMission.FullScreen", int( GFXFS_WINDOWED ) );
    SetGlobalVar( "GFX.Mode.InterMission.Frequency", 0 );
    SetGlobalVar( "GFX.Mode.Current.SizeX", nSurfaceWidth );
    SetGlobalVar( "GFX.Mode.Current.SizeY", nSurfaceHeight );
    SetGlobalVar( "GFX.Mode.Current.BPP", 32 );
    SetGlobalVar( "GFX.Mode.Current.Stencil", 8 );
    SetGlobalVar( "GFX.Mode.Current.FullScreen", int( GFXFS_WINDOWED ) );
    SetGlobalVar( "GFX.Mode.Current.Frequency", 0 );

    // --- the object database ---
    // Everything the game builds comes out of here, and the save/load system
    // needs to know about it before anything is created.
    {
        CPtr<IObjectsDB> pGDB = CreateObjectsDB();
        if ( pGDB == 0 )
        {
            LOGE( "cannot create the object database" );
            return false;
        }
        RegisterSingleton( IObjectsDB::tidTypeID, pGDB );
        GetSLS()->SetGDB( pGDB );
    }

    // --- the net driver ---
    // Registered even for a single-player launch, because the transceiver the
    // main loop builds looks it up either way.
    {
        SetGlobalVar( "GameSpyGameName", "blitzkrieg" );
        SetGlobalVar( "GameSpyEngineName", "blitzkrieg" );
        SetGlobalVar( "GameSpyChatName", "#GSP!blitzkrieg" );

        CTableAccessor constsTbl = NDB::OpenDataTable( "consts.xml" );
        SetGlobalVar( "NetGameVersion", constsTbl.GetInt( "Net", "GameVersion", 1 ) );

        INetDriver *pNetDriver = CreateObject<INetDriver>( INetDriver::tidTypeID );
        RegisterSingleton( INetDriver::tidTypeID, pNetDriver );
    }

    // --- the subsystems ---
    // Windows passed three window handles here -- the frame, the one graphics
    // draws into and the one input is captured against. They are one window in
    // this port, and the compatibility layer hands out its handle.
    if ( NMain::Initialize( GetActiveWindow(), GetActiveWindow(), GetActiveWindow(), true ) != true )
    {
        LOGE( "NMain::Initialize failed" );
        return false;
    }

    // --- the object database ---
    IObjectsDB *pObjectsDB = GetSingleton<IObjectsDB>();
    if ( pObjectsDB == 0 )
    {
        LOGE( "no object database: NMain::Initialize did not register one" );
        return false;
    }
    if ( pObjectsDB->LoadDB() == false )
    {
        LOGE( "cannot load objects.xml" );
        return false;
    }

    // --- what is in the archives ---
    // The menus read these lists to fill in the mission, chapter and campaign
    // pickers, so this runs before anything shows a menu.
    {
        IFilesInspector *pInspector = GetSingleton<IFilesInspector>();
        if ( pInspector == 0 )
        {
            LOGE( "no files inspector" );
            return false;
        }

        struct SEntry { const char *pszName; const char *pszPattern; };
        static const SEntry entries[] = {
            { "tutorial",            "scenarios\\tutorials\\;.xml" },
            { "custom_missions",     "scenarios\\custom\\missions\\;.xml" },
            { "custom_chapters",     "scenarios\\custom\\chapters\\;.xml" },
            { "custom_campaigns",    "scenarios\\custom\\campaigns\\;.xml" },
            { "maps_multiplayer_xml", "maps\\multiplayer\\;.xml" },
            { "maps_multiplayer_bzm", "maps\\multiplayer\\;.bzm" },
        };
        for ( size_t i = 0; i < sizeof( entries ) / sizeof( entries[0] ); ++i )
        {
            IFilesInspectorEntryCollector *pCollector =
                CreateObject<IFilesInspectorEntryCollector>( MAIN_FILES_INSPECTOR_ENTRY_COLLECTOR );
            if ( pCollector == 0 )
            {
                LOGE( "cannot create the collector for %s", entries[i].pszName );
                return false;
            }
            pCollector->Configure( entries[i].pszPattern );
            pInspector->AddEntry( entries[i].pszName, pCollector );
        }
        pInspector->InspectStorage( GetSingleton<IDataStorage>() );
    }

    // --- graphics ---
    // WinMain reads the resolution out of the profile and asks for a mode.
    // There is one mode here: the surface Android gave us. Passing its real
    // size is what makes the interface lay itself out to the screen.
    {
        IGFX *pGFX = GetSingleton<IGFX>();
        if ( pGFX == 0 )
        {
            LOGE( "no graphics singleton" );
            return false;
        }
        if ( pGFX->SetMode( nSurfaceWidth, nSurfaceHeight, 32, 8, GFXFS_WINDOWED, 0 ) == false )
        {
            LOGE( "IGFX::SetMode( %d, %d ) failed", nSurfaceWidth, nSurfaceHeight );
            return false;
        }
        pGFX->SetCullMode( GFXC_CW );          // right-handed, as the engine expects
        SHMatrix matrix;
        CreateOrthographicProjectionMatrixRH( &matrix, nSurfaceWidth, nSurfaceHeight,
                                              1, 1024 * 8 + nSurfaceHeight * 2 );
        pGFX->SetProjectionTransform( matrix );
        pGFX->EnableLighting( false );
        if ( ITextureManager *pTM = GetSingleton<ITextureManager>() )
            pTM->SetQuality( ITextureManager::TEXTURE_QUALITY_HIGH );
        else
            LOGE( "no texture manager" );
    }

    // --- saved settings ---
    SerializeConfig( true, SERIALIZE_CONFIG_BINDS | SERIALIZE_CONFIG_OPTIONS |
                           SERIALIZE_CONFIG_HELPCALLS );

    // --- the cursor ---
    // The engine keeps its own cursor and clamps it to these bounds. Touch
    // moves it; the bounds are the surface.
    {
        CPtr<ICursor> pCursor = GetSingleton<ICursor>();
        if ( pCursor == 0 )
        {
            LOGE( "no cursor singleton" );
            return false;
        }
        pCursor->SetBounds( 0, 0, nSurfaceWidth, nSurfaceHeight );
        pCursor->SetMode( 0 );
    }

    // --- the interface font ---
    {
        IFontManager *pFonts = GetSingleton<IFontManager>();
        if ( pFonts == 0 )
        {
            LOGE( "no font manager" );
            return false;
        }
        CPtr<IGFXFont> pFont = pFonts->GetFont( "fonts\\medium" );
        if ( pFont == 0 )
            LOGE( "fonts\\medium is missing; text will not draw" );
        GetSingleton<IGFX>()->SetFont( pFont );
    }

    // --- sound ---
    {
        ISFX *pSFX = GetSingleton<ISFX>();
        if ( pSFX == 0 )
        {
            LOGE( "no sound singleton" );
            return false;
        }
        pSFX->SetSFXMasterVolume( 1.0f );
        pSFX->SetStreamMasterVolume( GetGlobalVar( "Sound.StreamMasterVolume", 1.0f ) );
        pSFX->EnableSFX( GetGlobalVar( "Sound.EnableSFX", 1 ) );
        pSFX->EnableStreaming( GetGlobalVar( "Sound.EnableStream", 1 ) );
    }

    if ( IConsoleBuffer *pConsole = GetSingleton<IConsoleBuffer>() )
        pConsole->WriteASCII( CONSOLE_STREAM_COMMAND, "Exec( \"autoexec.cfg\" )", 0xff0000ff );

    if ( IOptionSystem *pOptions = GetSingleton<IOptionSystem>() )
        pOptions->Init();
    else
        LOGE( "no option system" );

    // NSysKeys::EnableSystemKeys is commented out in the original too.

    // --- the main loop ---
    g_pMainLoop = CreateMainLoop();
    if ( g_pMainLoop == 0 )
    {
        LOGE( "CreateMainLoop returned nothing" );
        return false;
    }
    RegisterSingleton( IMainLoop::tidTypeID, g_pMainLoop );
    if ( ICursor *pCursor = GetSingleton<ICursor>() )
        pCursor->Acquire( true );

    // The mod the profile remembers, if any.
    {
        IUserProfile *pProfile = GetSingleton<IUserProfile>();
        const std::string szMOD = ( pProfile != 0 ) ? pProfile->GetMOD() : std::string();
        if ( !szMOD.empty() )
            g_pMainLoop->Command( MAIN_COMMAND_CHANGE_MOD, szMOD.c_str() );
    }

    // The first command, and the one that decides what the player sees: the
    // intro movie, and the main menu when it ends. This is the no-arguments
    // branch of WinMain, which is the branch a launch without a command line
    // takes -- and on Android there is never a command line.
    g_pMainLoop->Command( MISSION_COMMAND_VIDEO,
                          NStr::Format( "%s;%d", "movies\\intro", MISSION_COMMAND_MAIN_MENU ) );
    NCutScenes::AddCutScene( "movies\\intro_only" );

    g_bStarted = true;
    LOGI( "started; main menu queued behind the intro" );
    return true;
}

// ---------------------------------------------------------------------------
// One frame
// ---------------------------------------------------------------------------
// WinMain's loop pumped the Windows queue, asked the window whether it was
// active, pumped input and called StepApp. The first is android_main's job and
// has already happened by the time this is called; the rest is here.
bool Bk1GameStep( bool bActive )
{
    if ( !g_bStarted || g_bFinished || g_pMainLoop == 0 )
        return false;

    GetSingleton<IInput>()->PumpMessages( bActive );

    if ( !g_pMainLoop->StepApp( bActive ) )
    {
        g_bFinished = true;
        return false;
    }
    return true;
}

void Bk1GameRequestExit()
{
    if ( g_pMainLoop != 0 && !g_bFinished )
        g_pMainLoop->Command( MAIN_COMMAND_EXIT_GAME, 0 );
}

// ---------------------------------------------------------------------------
// Shutdown
// ---------------------------------------------------------------------------
void Bk1GameShutdown()
{
    if ( g_pMainLoop != 0 )
    {
        g_pMainLoop->ResetStack();
        UnRegisterSingleton( IMainLoop::tidTypeID );
        g_pMainLoop = 0;
        // The player's settings, written back the way WinMain writes them.
        SerializeConfig( false, SERIALIZE_CONFIG_OPTIONS | SERIALIZE_CONFIG_BINDS |
                                SERIALIZE_CONFIG_HELPCALLS );
    }
    if ( g_bStarted )
    {
        GetSingleton<ICommandsHistory>()->Save();
        NMain::Finalize();
    }
    g_bStarted = false;
    g_bFinished = true;
}
