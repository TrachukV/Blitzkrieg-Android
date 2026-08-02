#pragma once
// The on-screen buttons for the things that were keys on a keyboard.
//
// Two groups, because they answer two different problems.
//
// Right edge, under the right thumb: time. Blitzkrieg runs faster or slower on
// a keypress and can be paused, and a mission spends long stretches with nothing
// to do but wait for a column to arrive. Without those keys a touch player sits
// through it in real time.
//
// Left edge, under the left thumb: the orders that are given on a PC by holding
// a key while clicking -- Ctrl to attack a point rather than what is under it,
// Alt to move ready to fight, Shift to add to the queue instead of replacing it.
// A finger cannot hold a key and tap at the same time, so these latch: press one
// and it stays down for the next order, then lets go by itself. That is the way
// a modifier is offered on a touchscreen everywhere it is offered at all.
//
// Neither group implements anything. Time posts the same command ids the
// keyboard posts. The modifiers press the actual keys through the engine's own
// input, so a player who rebinds Ctrl in the options screen rebinds these too --
// they carry no opinion about which key means what.
//
// Positions are in the engine's 1024x768 space, the same space the gesture layer
// reports fingers in, so the buttons land in the same place on every device and
// hit-testing needs no conversion of its own.

#ifdef __cplusplus
extern "C" {
#endif

// Drawn over the finished frame, before the buffers are swapped. Does nothing
// when no mission is running -- there is no speed to change behind a menu.
void Bk1TouchPanelDraw( int nSurfaceWidth, int nSurfaceHeight, long long nNowMs );

#define BK1_PANEL_NONE          (-1)
// Time, right edge.
#define BK1_PANEL_SPEED_UP      0
#define BK1_PANEL_PAUSE         1
#define BK1_PANEL_SPEED_DOWN    2
// Order modifiers, left edge. These latch.
#define BK1_PANEL_FORCE_ATTACK  3
#define BK1_PANEL_AGGRESSIVE    4
#define BK1_PANEL_QUEUE         5
// One-shot, left edge: bring the camera back to what is selected.
#define BK1_PANEL_CENTRE_CAMERA 6
#define BK1_PANEL_BUTTON_COUNT  7

// Which button, if any, is under a point given in engine coordinates.
//
// A tap that lands on a button must not also reach the game beneath it, or
// speeding time up would order the selected platoon into the corner of the map.
int Bk1TouchPanelHitTest( int nX, int nY );

// Presses a button by its id.
void Bk1TouchPanelPress( int nButton, long long nNowMs );

// Lets go of whatever modifier was latched, once the order it was latched for
// has been given. Called from the tap handler after the order, not before: the
// key has to still be down while the engine reads the click.
void Bk1TouchPanelReleaseModifiers( void );

// Whether any modifier is currently latched. The gesture layer asks so that a
// latched Ctrl turns a tap into an order rather than a selection -- holding Ctrl
// and clicking is an order on a PC, and the finger means the same thing.
int Bk1TouchPanelModifierLatched( void );

// Frees the GL objects. Called when the surface goes away, because the context
// goes with it and the names left behind would be dangling.
void Bk1TouchPanelRelease( void );

#ifdef __cplusplus
}
#endif
