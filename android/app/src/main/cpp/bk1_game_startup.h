#pragma once
// The engine's startup and frame, as android_main sees them.
//
// Kept behind these four calls so that the Android layer needs none of the
// engine's headers -- it owns a surface and an event loop, and this is the
// whole of what it has to know about the game running on top.

// Brings the engine up in the order Game/main.cpp brings it up, and queues the
// first command. False means the game cannot run -- the reason is in the log,
// and the usual one is that the data directory is not there.
// Registers the linked-in modules and their object factories. Called by
// Bk1GameStartup before anything else; see bk1_module_registry.cpp for why it
// is a call and not a static initialiser.
void Bk1RegisterModules();

bool Bk1GameStartup( const char *pszDataDirectory, int nSurfaceWidth, int nSurfaceHeight );

// One frame. False means the game has asked to end.
bool Bk1GameStep( bool bActive );

// Asks the game to exit as the player would, so it saves on the way out.
void Bk1GameRequestExit();

// A mouse button, delivered the way the engine expects for a device it
// believes is emulated.
//
// Marking the mouse emulated is what gives the cursor an absolute position --
// necessary, because a tap carries no motion for the relative path to
// integrate. But the engine then skips that device when it polls DirectInput
// ("if ( it->bEmulated ) continue"), so buttons pushed into the buffered queue
// are never read: the queue simply grows. EmulateInput is the other half of
// the same design, and this is the door to it.
void Bk1EmulateMouseButton( int nButtonOffset, bool bDown );

void Bk1GameShutdown();

bool Bk1GameIsRunning();
bool Bk1GameHasFinished();
