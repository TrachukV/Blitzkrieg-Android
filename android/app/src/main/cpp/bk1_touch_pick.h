#pragma once
// What a finger is over.
//
// A tap on the ground should order the selected units there -- that is what a
// tap does in every strategy game written for a touchscreen -- while a tap on a
// unit should select it and a tap on the command panel should press it. Telling
// those apart needs the engine: the interface knows its own elements and the
// scene knows what is standing where.
//
// Implemented in GameTT/iMissionInternal.cpp, guarded on _MSC_VER, because
// there is no way to reach the current interface from outside the main loop --
// IMainLoop can be told to set one and cannot be asked to return one.
//
// Answers BK1_PICK_NOTHING when no mission is running, which is the menus.
#define BK1_PICK_NOTHING    0
#define BK1_PICK_INTERFACE  1
#define BK1_PICK_OBJECT     2
#define BK1_PICK_GROUND     3

#ifdef __cplusplus
extern "C" {
#endif

int Bk1PickAt( int nX, int nY );

// Ends the running mission as a win. Returns 0 when no mission is running.
//
// Diagnostic, not a cheat: it is how the road after a mission -- the statistics
// screen and the chapter that follows -- gets walked without playing a mission
// well enough to earn it. Reached only through a property that is unset in any
// normal run:
//
//   adb shell setprop debug.blitzkrieg.winmission 1
int Bk1FinishMissionAsWin( void );

// Posts an engine command exactly as a hotkey does -- the ids are the ones in
// Main/iMainCommands.h. The on-screen panel is a second way to reach commands
// the game already has, not a second implementation of them, so nothing here
// can behave differently from the keyboard.
//
// Returns 0 when no mission is running.
int Bk1SendGameCommand( int nCommand );

// The speed the game is running at right now, read from the timer rather than
// counted by the panel, so a save that restores a speed shows the truth.
int Bk1GetGameSpeed( void );

int Bk1IsMissionActive( void );

// Moves the camera by a drag, in the engine's 1024x768 screen units.
//
// The touch layer used to scroll by holding the arrow keys, which scrolls at the
// key's repeat rate -- the camera ratchets along instead of travelling with the
// finger, and that is what made panning feel jerky. This offsets the camera's
// own anchor instead, so the engine keeps doing the bounds and the smoothing.
void Bk1ScrollCameraBy( float fScreenDX, float fScreenDY );

// Command ids the touch panel issues. Kept as literals rather than including
// the engine header, which drags in the whole of Misc/Basic.h and its MSVC
// expectations; the guard below fails the build if the two ever disagree.
#define BK1_CMD_GAME_PAUSE      0x00100014
#define BK1_CMD_GAME_SPEED_INC  0x00100015
#define BK1_CMD_GAME_SPEED_DEC  0x00100016
#define BK1_CMD_RESTART_MISSION 0x00110008
#define BK1_CMD_SAVE            0x00100001
#define BK1_CMD_LOAD            0x00100002

#ifdef __cplusplus
}
#endif
