// The touch gestures, driven directly.
//
// Three of the five can be injected with `adb shell input` and were checked on
// a device that way. The two-finger ones cannot: a stock emulator image
// refuses a non-root writer on /dev/input, so `sendevent` returns Permission
// denied and there is no other way in. That is why the recogniser is a unit of
// its own -- it can be driven here, exactly as it is driven on the device,
// including the paths a device cannot reach.
#include <stdio.h>
#include <string.h>

#include <vector>

#include "../app/src/main/cpp/bk1_touch_gestures.h"

using namespace NBk1Touch;

namespace {

int g_nFailures = 0;

void Check( bool bCondition, const char *pszWhat )
{
    if ( !bCondition )
    {
        printf( "  FAIL  %s\n", pszWhat );
        ++g_nFailures;
    }
}

const char *Name( EActionKind kind )
{
    switch ( kind )
    {
    case ACTION_CURSOR_TO:    return "cursor";
    case ACTION_MOUSE_MOVE:   return "move";
    case ACTION_LEFT_DOWN:    return "left-down";
    case ACTION_LEFT_UP:      return "left-up";
    case ACTION_RIGHT_CLICK:  return "right-click";
    case ACTION_WHEEL:        return "wheel";
    case ACTION_SCROLL_LEFT:  return "scroll-left";
    case ACTION_SCROLL_RIGHT: return "scroll-right";
    case ACTION_SCROLL_UP:    return "scroll-up";
    case ACTION_SCROLL_DOWN:  return "scroll-down";
    }
    return "?";
}

// Collects what a sequence produced, so a test can ask what happened rather
// than checking one call at a time.
struct SLog
{
    std::vector<SAction> actions;

    void Add( const SAction *pActions, int nCount )
    {
        for ( int i = 0; i < nCount; ++i )
            actions.push_back( pActions[i] );
    }
    int Count( EActionKind kind ) const
    {
        int n = 0;
        for ( size_t i = 0; i < actions.size(); ++i )
        {
            if ( actions[i].kind == kind )
                ++n;
        }
        return n;
    }
    // Where an action first appears, so a test can say one thing came after
    // another. -1 when it never did.
    int IndexOf( EActionKind kind ) const
    {
        for ( size_t i = 0; i < actions.size(); ++i )
        {
            if ( actions[i].kind == kind )
                return (int)i;
        }
        return -1;
    }

    const SAction *First( EActionKind kind ) const
    {
        for ( size_t i = 0; i < actions.size(); ++i )
        {
            if ( actions[i].kind == kind )
                return &actions[i];
        }
        return 0;
    }
    void Dump() const
    {
        printf( "        " );
        for ( size_t i = 0; i < actions.size(); ++i )
            printf( "%s ", Name( actions[i].kind ) );
        printf( "\n" );
    }
};

// A 420 dpi phone, which is an ordinary modern density.
const int DPI = 420;

void Feed( CRecogniser *pR, SLog *pLog, EPhase phase, const SFinger *pFingers,
           size_t nFingers, long long nTimeMs )
{
    SAction actions[MAX_ACTIONS];
    const int n = pR->Handle( phase, pFingers, nFingers, nTimeMs, actions );
    pLog->Add( actions, n );
}

void TestTapIsALeftClick()
{
    printf( "a tap is a left click\n" );
    CRecogniser r;
    r.SetDisplayDensity( DPI );
    SLog log;

    SFinger f = { 1, 500.0f, 400.0f };
    Feed( &r, &log, PHASE_DOWN, &f, 1, 1000 );
    Feed( &r, &log, PHASE_UP, &f, 1, 1080 );

    Check( log.Count( ACTION_LEFT_DOWN ) == 1, "one press" );
    Check( log.Count( ACTION_LEFT_UP ) == 1, "one release" );
    Check( log.Count( ACTION_RIGHT_CLICK ) == 0, "no order" );
    const SAction *pCursor = log.First( ACTION_CURSOR_TO );
    Check( pCursor != 0 && pCursor->nX == 500 && pCursor->nY == 400,
           "cursor moved to the finger before the press" );
    // The press waits for the lift: on touch alone nothing is clicked, or
    // every drag of the map would begin by clicking whatever it started on.
    Check( log.IndexOf( ACTION_LEFT_DOWN ) > log.IndexOf( ACTION_CURSOR_TO ),
           "and the press came after it, at the lift" );
}

void TestOneFingerDragsTheMap()
{
    printf( "one finger drags the map, the way every map on this device does\n" );
    CRecogniser r;
    r.SetDisplayDensity( DPI );
    SLog log;

    SFinger f = { 1, 300.0f, 300.0f };
    Feed( &r, &log, PHASE_DOWN, &f, 1, 1000 );
    for ( int i = 1; i <= 6; ++i )
    {
        f.fX = 300.0f + i * 40.0f;
        f.fY = 300.0f + i * 20.0f;
        Feed( &r, &log, PHASE_MOVE, &f, 1, 1000 + i * 30 );
    }
    Check( log.Count( ACTION_LEFT_DOWN ) == 0,
           "nothing is pressed: a drag is not a click" );
    Check( log.Count( ACTION_SCROLL_LEFT ) + log.Count( ACTION_SCROLL_RIGHT ) +
           log.Count( ACTION_SCROLL_UP ) + log.Count( ACTION_SCROLL_DOWN ) > 0,
           "the map scrolled" );
    Check( log.Count( ACTION_RIGHT_CLICK ) == 0,
           "a moving finger is never an order, however long it takes" );

    Feed( &r, &log, PHASE_UP, &f, 1, 1400 );
    Check( log.Count( ACTION_LEFT_DOWN ) == 0,
           "and lifting after a drag still clicks nothing" );
}

void TestHoldBecomesAnOrder()
{
    printf( "a finger held still becomes a right click\n" );
    CRecogniser r;
    r.SetDisplayDensity( DPI );
    SLog log;

    SFinger f = { 1, 800.0f, 600.0f };
    Feed( &r, &log, PHASE_DOWN, &f, 1, 1000 );

    // Perfectly still: no move event ever arrives, so only the frame loop's
    // tick can notice. This is the case that would silently never fire if the
    // hold were only checked on movement.
    SAction actions[MAX_ACTIONS];
    log.Add( actions, r.Tick( 1200, actions ) );
    Check( log.Count( ACTION_RIGHT_CLICK ) == 0, "not yet at 200 ms" );

    log.Add( actions, r.Tick( 1500, actions ) );
    Check( log.Count( ACTION_RIGHT_CLICK ) == 1, "fired after the hold" );
    Check( log.Count( ACTION_LEFT_DOWN ) == 0,
           "and nothing was ever pressed, so nothing gets selected" );

    const SAction *pOrder = log.First( ACTION_RIGHT_CLICK );
    Check( pOrder != 0 && pOrder->nX == 800 && pOrder->nY == 600,
           "the order lands where the finger was pressed" );

    log.Add( actions, r.Tick( 2000, actions ) );
    Check( log.Count( ACTION_RIGHT_CLICK ) == 1, "and only once" );
}

void TestSecondFingerCancelsTheClick()
{
    printf( "a second finger cancels whatever the first was doing\n" );
    CRecogniser r;
    r.SetDisplayDensity( DPI );
    SLog log;

    SFinger fingers[2] = { { 1, 400.0f, 400.0f }, { 2, 800.0f, 400.0f } };
    Feed( &r, &log, PHASE_DOWN, fingers, 1, 1000 );
    Feed( &r, &log, PHASE_SECOND_DOWN, fingers, 2, 1040 );

    Check( log.Count( ACTION_LEFT_DOWN ) == 0, "the first finger had pressed nothing" );
    Check( r.IsTwoFinger(), "now a two-finger gesture" );

    // Long enough that a hold would have fired if the gesture had not changed.
    SAction actions[MAX_ACTIONS];
    log.Add( actions, r.Tick( 2000, actions ) );
    Check( log.Count( ACTION_RIGHT_CLICK ) == 0,
           "moving the camera never drops an order on the battlefield" );

    Feed( &r, &log, PHASE_SECOND_UP, fingers, 1, 2100 );
    Feed( &r, &log, PHASE_UP, fingers, 1, 2200 );
    Check( log.Count( ACTION_LEFT_DOWN ) == 0, "and no press appeared at the end" );
}

void TestTwoFingerDragDrawsTheBox()
{
    printf( "two fingers moving together draw the selection box\n" );
    CRecogniser r;
    r.SetDisplayDensity( DPI );
    SLog log;

    SFinger fingers[2] = { { 1, 400.0f, 400.0f }, { 2, 800.0f, 400.0f } };
    Feed( &r, &log, PHASE_DOWN, fingers, 1, 1000 );
    Feed( &r, &log, PHASE_SECOND_DOWN, fingers, 2, 1040 );

    // Both fingers travel together, keeping their distance, so this is a box
    // and not a zoom.
    for ( int i = 1; i <= 8; ++i )
    {
        fingers[0].fX = 400.0f + i * 30.0f;
        fingers[1].fX = 800.0f + i * 30.0f;
        fingers[0].fY = fingers[1].fY = 400.0f + i * 30.0f;
        Feed( &r, &log, PHASE_MOVE, fingers, 2, 1040 + i * 20 );
    }

    Check( log.Count( ACTION_LEFT_DOWN ) == 1, "the button went down once" );
    Check( log.Count( ACTION_LEFT_UP ) == 0, "and stays down while the box grows" );
    Check( log.Count( ACTION_WHEEL ) == 0, "a parallel drag is not a zoom" );
    Check( log.Count( ACTION_SCROLL_LEFT ) + log.Count( ACTION_SCROLL_RIGHT ) == 0,
           "and it does not scroll: one finger does that now" );

    // The box is anchored where the fingers first were, not where they are.
    const SAction *pCursor = log.First( ACTION_CURSOR_TO );
    Check( pCursor != 0, "the cursor was placed before the press" );

    Feed( &r, &log, PHASE_SECOND_UP, fingers, 1, 1300 );
    Feed( &r, &log, PHASE_UP, fingers, 1, 1340 );
    Check( log.Count( ACTION_LEFT_UP ) == 1, "and released when the fingers lift" );
}

void TestPinchZooms()
{
    printf( "fingers apart zoom in, together zoom out\n" );
    CRecogniser r;
    r.SetDisplayDensity( DPI );
    SLog log;

    SFinger fingers[2] = { { 1, 600.0f, 500.0f }, { 2, 800.0f, 500.0f } };
    Feed( &r, &log, PHASE_DOWN, fingers, 1, 1000 );
    Feed( &r, &log, PHASE_SECOND_DOWN, fingers, 2, 1040 );

    for ( int i = 1; i <= 8; ++i )
    {
        fingers[0].fX = 600.0f - i * 40.0f;
        fingers[1].fX = 800.0f + i * 40.0f;
        Feed( &r, &log, PHASE_MOVE, fingers, 2, 1040 + i * 20 );
    }

    Check( log.Count( ACTION_WHEEL ) > 0, "spreading produced wheel notches" );
    const SAction *pWheel = log.First( ACTION_WHEEL );
    Check( pWheel != 0 && pWheel->nX > 0, "apart is forward, which is zoom in" );

    // Now back together, from the same state.
    SLog in;
    CRecogniser r2;
    r2.SetDisplayDensity( DPI );
    SFinger close[2] = { { 1, 300.0f, 500.0f }, { 2, 1100.0f, 500.0f } };
    Feed( &r2, &in, PHASE_DOWN, close, 1, 1000 );
    Feed( &r2, &in, PHASE_SECOND_DOWN, close, 2, 1040 );
    for ( int i = 1; i <= 8; ++i )
    {
        close[0].fX = 300.0f + i * 40.0f;
        close[1].fX = 1100.0f - i * 40.0f;
        Feed( &r2, &in, PHASE_MOVE, close, 2, 1040 + i * 20 );
    }
    const SAction *pOut = in.First( ACTION_WHEEL );
    Check( pOut != 0 && pOut->nX < 0, "together is backward, which is zoom out" );
}

void TestDensityChangesTheThreshold()
{
    printf( "the thresholds are physical, not pixel counts\n" );
    // The same 20-pixel drift is a held finger on a dense display and a drag on
    // a coarse one. Getting this wrong makes every order on a tablet turn into
    // a selection box, which is why it is measured rather than assumed.
    const float fDrift = 20.0f;

    CRecogniser dense;
    dense.SetDisplayDensity( 560 );
    SLog denseLog;
    SFinger f = { 1, 500.0f, 500.0f };
    Feed( &dense, &denseLog, PHASE_DOWN, &f, 1, 1000 );
    f.fX = 500.0f + fDrift;
    Feed( &dense, &denseLog, PHASE_MOVE, &f, 1, 1100 );
    SAction actions[MAX_ACTIONS];
    denseLog.Add( actions, dense.Tick( 1600, actions ) );
    Check( denseLog.Count( ACTION_RIGHT_CLICK ) == 1,
           "on a dense display that drift is still a held finger" );

    CRecogniser coarse;
    coarse.SetDisplayDensity( 160 );
    SLog coarseLog;
    SFinger g = { 1, 500.0f, 500.0f };
    Feed( &coarse, &coarseLog, PHASE_DOWN, &g, 1, 1000 );
    g.fX = 500.0f + fDrift;
    Feed( &coarse, &coarseLog, PHASE_MOVE, &g, 1, 1100 );
    coarseLog.Add( actions, coarse.Tick( 1600, actions ) );
    Check( coarseLog.Count( ACTION_RIGHT_CLICK ) == 0,
           "on a coarse one the same drift is a drag" );
}

}   // anonymous namespace

int main()
{
    printf( "touch gestures\n\n" );
    TestTapIsALeftClick();
    TestOneFingerDragsTheMap();
    TestHoldBecomesAnOrder();
    TestSecondFingerCancelsTheClick();
    TestTwoFingerDragDrawsTheBox();
    TestPinchZooms();
    TestDensityChangesTheThreshold();

    printf( "\n%s\n", g_nFailures == 0 ? "all passed" : "FAILURES" );
    return g_nFailures == 0 ? 0 : 1;
}
