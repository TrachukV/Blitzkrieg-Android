#pragma once
// The touch gestures, as a machine with no Android in it.
//
// Blitzkrieg is a mouse-and-keyboard real-time strategy: left-drag draws a
// selection box, right-click gives an order, the wheel zooms, the arrow keys
// scroll. A touchscreen has none of those, so these gestures stand in -- and
// they stand in by producing the events the engine's own bindings already
// read, never by reaching into the engine. Whatever the player has bound in
// the options still applies.
//
//   one finger, tap            left click
//   one finger, drag           cursor follows with the button held: box select
//   one finger, held still     right click, the order gesture
//   two fingers, drag          scroll the camera
//   two fingers, pinch         zoom
//
// A second finger cancels whatever the first was doing. Putting two fingers
// down is never a click, and treating it as one drops a stray order on the
// battlefield every time the player moves the camera.
//
// It is a separate unit from the Android event loop for one reason: two-finger
// gestures cannot be injected with adb on a stock emulator image -- the input
// devices refuse a non-root writer -- so the only way to test them is to drive
// the machine directly. android/tests/touch_gesture_test.cpp does exactly that,
// against this unit, which is the one the APK ships.
#include <stddef.h>

namespace NBk1Touch {

// What the machine decides to do. The caller performs these; the machine never
// calls into anything itself, which is what makes it testable.
enum EActionKind
{
    ACTION_CURSOR_TO,       // move the cursor to (nX, nY)
    ACTION_MOUSE_MOVE,      // relative motion, (nX, nY) as a delta
    ACTION_LEFT_DOWN,
    ACTION_LEFT_UP,
    ACTION_TAP,             // a finger down and up in one place, at (nX, nY).
                            // What it means depends on what is under it, and
                            // only the port can ask, so the recogniser reports
                            // the gesture and leaves the meaning alone.
    ACTION_RIGHT_CLICK,     // down and up together: an order
    ACTION_WHEEL,           // nX is notches, positive is zoom in
    ACTION_SCROLL_LEFT,     // nX is 1 to press the key, 0 to let go
    ACTION_SCROLL_RIGHT,
    ACTION_SCROLL_UP,
    ACTION_SCROLL_DOWN,
};

struct SAction
{
    EActionKind kind;
    int nX;
    int nY;
};

// Where a finger is and what it is doing. The caller translates Android's
// motion events into these; nothing else about Android crosses this line.
enum EPhase
{
    PHASE_DOWN,             // the first finger touched
    PHASE_SECOND_DOWN,      // another finger joined
    PHASE_MOVE,
    PHASE_SECOND_UP,        // one of two fingers left
    PHASE_UP,               // the last finger left
    PHASE_CANCEL,
};

struct SFinger
{
    int   nId;
    float fX;
    float fY;
};

const int MAX_ACTIONS = 16;

class CRecogniser
{
public:
    CRecogniser();

    // Density decides how far a finger may drift and still count as held, so
    // the same gesture feels the same on a phone and on a tablet. 160 is
    // Android's reference density.
    void SetDisplayDensity( int nDpi );

    // Feeds one event. Fingers are all the contacts currently down, in the
    // order the platform reports them; nTimeMs is monotonic. Returns how many
    // actions were produced and writes them to pActions.
    int Handle( EPhase phase, const SFinger *pFingers, size_t nFingers,
                long long nTimeMs, SAction *pActions );

    // Called from the frame loop, because a hold becomes an order after enough
    // time has passed with nothing happening -- and if the finger is perfectly
    // still, no move event ever arrives to notice it.
    int Tick( long long nTimeMs, SAction *pActions );

    // For the test, and for anyone reasoning about a log.
    bool IsHolding() const { return bLeftDown; }
    bool IsTwoFinger() const { return bTwoFinger; }

private:
    float MillimetresToPixels( float fMillimetres ) const;
    int   ReleaseLeft( SAction *pActions, int nCount );
    int   ReleaseScroll( SAction *pActions, int nCount );
    int   SetScroll( SAction *pActions, int nCount, int nIndex,
                     EActionKind kind, bool bDown );

    int   nDensityDpi;

    int   nActiveId;
    float fLastX, fLastY;
    float fPressX, fPressY;
    long long nPressTime;
    bool  bMovedPastSlop;
    bool  bLongPressFired;
    bool  bLeftDown;

    // One finger drags the map: where the drag was last accounted for.
    bool  bPanning;
    float fPanAnchorX, fPanAnchorY;

    bool  bTwoFinger;
    float fPinchCentreX, fPinchCentreY;
    float fPinchSpread;
    bool  bScrollHeld[4];

    // Two fingers draw the selection box: where it was anchored, and whether
    // it has started.
    bool  bBoxSelect;
    float fBoxAnchorX, fBoxAnchorY;
};

}   // namespace NBk1Touch
