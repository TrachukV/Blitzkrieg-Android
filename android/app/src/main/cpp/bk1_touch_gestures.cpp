// The touch gesture machine. See bk1_touch_gestures.h for what each gesture
// stands in for and why this is a unit of its own.
#include "bk1_touch_gestures.h"

#include <math.h>

namespace NBk1Touch {

namespace {

// Distances are in millimetres and converted with the display's real density,
// because a threshold in pixels is a different gesture on a phone and on a
// tablet.
const float TOUCH_SLOP_MM = 2.5f;      // drift that still counts as "held still"
const float PINCH_SLOP_MM = 4.0f;      // spread change that makes one zoom notch
const long long LONG_PRESS_MS = 450;   // hold before it becomes an order

// One notch is what a mouse reports per click of its wheel, which is what the
// engine's zoom slider is scaled against.
const int WHEEL_NOTCH = 120;

const int SCROLL_LEFT = 0, SCROLL_RIGHT = 1, SCROLL_UP = 2, SCROLL_DOWN = 3;

bool FindFinger( const SFinger *pFingers, size_t nFingers, int nId,
                 float *pfX, float *pfY )
{
    for ( size_t i = 0; i < nFingers; ++i )
    {
        if ( pFingers[i].nId == nId )
        {
            *pfX = pFingers[i].fX;
            *pfY = pFingers[i].fY;
            return true;
        }
    }
    return false;
}

// Where the two fingers are together, and how far apart.
bool TwoFingerState( const SFinger *pFingers, size_t nFingers,
                     float *pfCentreX, float *pfCentreY, float *pfSpread )
{
    if ( nFingers < 2 )
        return false;
    const float dx = pFingers[1].fX - pFingers[0].fX;
    const float dy = pFingers[1].fY - pFingers[0].fY;
    *pfCentreX = ( pFingers[0].fX + pFingers[1].fX ) * 0.5f;
    *pfCentreY = ( pFingers[0].fY + pFingers[1].fY ) * 0.5f;
    *pfSpread = sqrtf( dx * dx + dy * dy );
    return true;
}

SAction Make( EActionKind kind, int nX, int nY )
{
    SAction action;
    action.kind = kind;
    action.nX = nX;
    action.nY = nY;
    return action;
}

}   // anonymous namespace

CRecogniser::CRecogniser()
    : nDensityDpi( 160 ), nActiveId( -1 ), fLastX( 0.0f ), fLastY( 0.0f ),
      fPressX( 0.0f ), fPressY( 0.0f ), nPressTime( 0 ), bMovedPastSlop( false ),
      bLongPressFired( false ), bLeftDown( false ), bTwoFinger( false ),
      fPinchCentreX( 0.0f ), fPinchCentreY( 0.0f ), fPinchSpread( 0.0f )
{
    for ( int i = 0; i < 4; ++i )
        bScrollHeld[i] = false;
}

void CRecogniser::SetDisplayDensity( int nDpi )
{
    if ( nDpi > 0 )
        nDensityDpi = nDpi;
}

float CRecogniser::MillimetresToPixels( float fMillimetres ) const
{
    return fMillimetres * (float)nDensityDpi / 25.4f;
}

int CRecogniser::ReleaseLeft( SAction *pActions, int nCount )
{
    if ( !bLeftDown )
        return nCount;
    bLeftDown = false;
    pActions[nCount++] = Make( ACTION_LEFT_UP, 0, 0 );
    return nCount;
}

int CRecogniser::SetScroll( SAction *pActions, int nCount, int nIndex,
                            EActionKind kind, bool bDown )
{
    if ( bScrollHeld[nIndex] == bDown )
        return nCount;
    bScrollHeld[nIndex] = bDown;
    pActions[nCount++] = Make( kind, bDown ? 1 : 0, 0 );
    return nCount;
}

int CRecogniser::ReleaseScroll( SAction *pActions, int nCount )
{
    nCount = SetScroll( pActions, nCount, SCROLL_LEFT,  ACTION_SCROLL_LEFT,  false );
    nCount = SetScroll( pActions, nCount, SCROLL_RIGHT, ACTION_SCROLL_RIGHT, false );
    nCount = SetScroll( pActions, nCount, SCROLL_UP,    ACTION_SCROLL_UP,    false );
    nCount = SetScroll( pActions, nCount, SCROLL_DOWN,  ACTION_SCROLL_DOWN,  false );
    return nCount;
}

int CRecogniser::Tick( long long nTimeMs, SAction *pActions )
{
    int nCount = 0;
    // A finger held perfectly still sends no move events, so the hold has to
    // be noticed from outside the event stream.
    if ( !bTwoFinger && !bLongPressFired && !bMovedPastSlop && bLeftDown &&
         nTimeMs - nPressTime >= LONG_PRESS_MS )
    {
        bLongPressFired = true;
        nCount = ReleaseLeft( pActions, nCount );
        pActions[nCount++] = Make( ACTION_RIGHT_CLICK, (int)fPressX, (int)fPressY );
    }
    return nCount;
}

int CRecogniser::Handle( EPhase phase, const SFinger *pFingers, size_t nFingers,
                         long long nTimeMs, SAction *pActions )
{
    int nCount = 0;

    switch ( phase )
    {
    case PHASE_DOWN:
        if ( nFingers < 1 )
            return 0;
        nActiveId = pFingers[0].nId;
        fLastX = fPressX = pFingers[0].fX;
        fLastY = fPressY = pFingers[0].fY;
        nPressTime = nTimeMs;
        bMovedPastSlop = false;
        bLongPressFired = false;
        bTwoFinger = false;
        // The cursor goes to the finger before the press, so the engine sees
        // the button go down where it was touched.
        pActions[nCount++] = Make( ACTION_CURSOR_TO, (int)fPressX, (int)fPressY );
        pActions[nCount++] = Make( ACTION_LEFT_DOWN, 0, 0 );
        bLeftDown = true;
        return nCount;

    case PHASE_SECOND_DOWN:
        // A second finger means this was a camera gesture all along. Undo the
        // press the first one started, so no click and no order is delivered.
        nCount = ReleaseLeft( pActions, nCount );
        bTwoFinger = true;
        bLongPressFired = true;          // no order can come out of this now
        TwoFingerState( pFingers, nFingers, &fPinchCentreX, &fPinchCentreY,
                        &fPinchSpread );
        return nCount;

    case PHASE_MOVE:
        if ( bTwoFinger && nFingers >= 2 )
        {
            float fCentreX = 0.0f, fCentreY = 0.0f, fSpread = 0.0f;
            if ( !TwoFingerState( pFingers, nFingers, &fCentreX, &fCentreY, &fSpread ) )
                return nCount;

            // --- zoom ---
            const float fPinchSlop = MillimetresToPixels( PINCH_SLOP_MM );
            const float fSpreadChange = fSpread - fPinchSpread;
            if ( fabsf( fSpreadChange ) >= fPinchSlop )
            {
                // Fingers apart is zoom in, which is a wheel notch forward.
                const int nNotches = (int)( fSpreadChange / fPinchSlop );
                pActions[nCount++] = Make( ACTION_WHEEL, nNotches * WHEEL_NOTCH, 0 );
                fPinchSpread += nNotches * fPinchSlop;
            }

            // --- scroll ---
            // The map moves with the fingers, so dragging left brings the land
            // on the right into view. That is how a map behaves, and it is the
            // opposite of how a cursor would.
            const float fSlop = MillimetresToPixels( TOUCH_SLOP_MM );
            const float fDeltaX = fCentreX - fPinchCentreX;
            const float fDeltaY = fCentreY - fPinchCentreY;
            nCount = SetScroll( pActions, nCount, SCROLL_LEFT,  ACTION_SCROLL_LEFT,
                                fDeltaX > fSlop );
            nCount = SetScroll( pActions, nCount, SCROLL_RIGHT, ACTION_SCROLL_RIGHT,
                                fDeltaX < -fSlop );
            nCount = SetScroll( pActions, nCount, SCROLL_UP,    ACTION_SCROLL_UP,
                                fDeltaY > fSlop );
            nCount = SetScroll( pActions, nCount, SCROLL_DOWN,  ACTION_SCROLL_DOWN,
                                fDeltaY < -fSlop );
            if ( fabsf( fDeltaX ) > fSlop )
                fPinchCentreX = fCentreX;
            if ( fabsf( fDeltaY ) > fSlop )
                fPinchCentreY = fCentreY;
            return nCount;
        }

        {
            float fX = 0.0f, fY = 0.0f;
            if ( !FindFinger( pFingers, nFingers, nActiveId, &fX, &fY ) )
                return nCount;

            const float fFromPressX = fX - fPressX;
            const float fFromPressY = fY - fPressY;
            const float fSlop = MillimetresToPixels( TOUCH_SLOP_MM );
            if ( fFromPressX * fFromPressX + fFromPressY * fFromPressY > fSlop * fSlop )
                bMovedPastSlop = true;

            const int nDeltaX = (int)( fX - fLastX );
            const int nDeltaY = (int)( fY - fLastY );
            if ( nDeltaX != 0 || nDeltaY != 0 )
            {
                // Both spellings, because the engine reads the axes as a
                // relative stream and the cursor as an absolute position.
                pActions[nCount++] = Make( ACTION_MOUSE_MOVE, nDeltaX, nDeltaY );
                pActions[nCount++] = Make( ACTION_CURSOR_TO, (int)fX, (int)fY );
                fLastX = fX;
                fLastY = fY;
            }
        }

        // A finger held still long enough is an order rather than a selection.
        {
            const int nHold = Tick( nTimeMs, pActions + nCount );
            nCount += nHold;
        }
        return nCount;

    case PHASE_SECOND_UP:
        // Back to one finger. The camera gesture is over, and the finger still
        // down must not turn into a click -- it was never a press.
        nCount = ReleaseScroll( pActions, nCount );
        bTwoFinger = false;
        nActiveId = -1;
        return nCount;

    case PHASE_UP:
    case PHASE_CANCEL:
        nCount = ReleaseScroll( pActions, nCount );
        nCount = ReleaseLeft( pActions, nCount );
        nActiveId = -1;
        bTwoFinger = false;
        return nCount;
    }
    return nCount;
}

}   // namespace NBk1Touch
