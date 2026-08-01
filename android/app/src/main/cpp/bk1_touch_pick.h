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

#ifdef __cplusplus
}
#endif
