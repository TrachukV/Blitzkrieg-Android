#pragma once
// The on-screen buttons for the things that were hotkeys on a keyboard.
//
// Blitzkrieg lets the player run the game faster or slower and pause it, and on
// a PC those are keys. A phone has no keys, and the two speed controls are not
// a convenience in this game -- a mission spends long stretches with nothing to
// do but wait for a column to arrive, and the original answer to that is to
// speed time up. Without them a touch player has to sit through it.
//
// The panel does not implement any of these. It posts the same command ids the
// keyboard posts (Main/iMainCommands.h) through Bk1SendGameCommand, so the
// clamping against maxspeed/minspeed, the multiplayer send and the on-screen
// notice naming the new speed are all the engine's own code on its own path.
// An on-screen button and the original hotkey therefore cannot drift apart.
//
// Positions are in the engine's 1024x768 space, the same space the gesture
// layer reports fingers in, so the buttons land in the same place on every
// device and hit-testing needs no conversion of its own.

#ifdef __cplusplus
extern "C" {
#endif

// Draws the panel over the finished frame. Call after the game has drawn and
// before the buffers are swapped. Does nothing when no mission is running --
// there is no speed to change behind a menu, and the engine's own menus are
// already reachable by finger.
void Bk1TouchPanelDraw( int nSurfaceWidth, int nSurfaceHeight, long long nNowMs );

// Which button, if any, is under a point given in engine coordinates.
// Returns BK1_PANEL_NONE when the point is not on the panel.
//
// A tap that lands on a button must not also reach the game beneath it, or
// speeding time up would order the selected platoon into the corner of the map.
#define BK1_PANEL_NONE        (-1)
#define BK1_PANEL_SPEED_DOWN  0
#define BK1_PANEL_PAUSE       1
#define BK1_PANEL_SPEED_UP    2
int Bk1TouchPanelHitTest( int nX, int nY );

// Presses a button by its id: posts the command and lights the button for a
// moment so a finger gets the same acknowledgement a mouse gets from a UI
// element that depresses.
void Bk1TouchPanelPress( int nButton, long long nNowMs );

// Frees the GL objects. Called when the surface goes away, because the context
// goes with it and the names left behind would be dangling.
void Bk1TouchPanelRelease( void );

#ifdef __cplusplus
}
#endif
