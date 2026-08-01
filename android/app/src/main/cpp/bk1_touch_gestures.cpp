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
    // nActiveId, not bLeftDown: the button is no longer pressed when a finger
    // lands -- it waits for the lift, so that dragging the map clicks nothing --
    // and using it as the "a finger is down" flag stopped the hold firing at all.
    if ( !bTwoFinger && !bLongPressFired && !bMovedPastSlop && nActiveId >= 0 &&
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
        bPanning = false;
        bBoxSelect = false;
        fPanAnchorX = fPressX;
        fPanAnchorY = fPressY;
        // The cursor goes to the finger, and nothing else happens yet.
        //
        // The button used to go down here, which was right when a drag meant a
        // selection box. Now a drag means the map moves, and pressing on touch
        // would mean every pan began by releasing a button at the point it
        // started -- a click on whatever was under the finger. So the press
        // waits until the finger lifts and the gesture is known to be a tap.
        pActions[nCount++] = Make( ACTION_CURSOR_TO, (int)fPressX, (int)fPressY );
        return nCount;

    case PHASE_SECOND_DOWN:
        // A second finger means this was a camera gesture all along. Undo the
        // press the first one started, so no click and no order is delivered.
        nCount = ReleaseLeft( pActions, nCount );
        bTwoFinger = true;
        bLongPressFired = true;          // no order can come out of this now
        TwoFingerState( pFingers, nFingers, &fPinchCentreX, &fPinchCentreY,
                        &fPinchSpread );
        fBoxAnchorX = fPinchCentreX;
        fBoxAnchorY = fPinchCentreY;
        bBoxSelect = false;
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

            // --- the selection box ---
            // Two fingers moving together draw it, because one finger is busy
            // dragging the map. That is the swap: the commonest gesture goes to
            // the commonest action, and the box -- which a player reaches for
            // far less often -- takes the second finger.
            //
            // The box is anchored where the two fingers first touched and
            // follows their midpoint, so it reads the same as dragging one out
            // with a mouse.
            const float fSlop = MillimetresToPixels( TOUCH_SLOP_MM );
            const float fFromStartX = fCentreX - fBoxAnchorX;
            const float fFromStartY = fCentreY - fBoxAnchorY;
            if ( !bBoxSelect &&
                 fFromStartX * fFromStartX + fFromStartY * fFromStartY > fSlop * fSlop )
            {
                // Put the cursor where the box begins before the button goes
                // down, or the engine anchors it wherever the cursor last was.
                pActions[nCount++] = Make( ACTION_CURSOR_TO, (int)fBoxAnchorX,
                                           (int)fBoxAnchorY );
                pActions[nCount++] = Make( ACTION_LEFT_DOWN, 0, 0 );
                bLeftDown = true;
                bBoxSelect = true;
            }
            if ( bBoxSelect )
            {
                pActions[nCount++] = Make( ACTION_MOUSE_MOVE,
                                           (int)( fCentreX - fPinchCentreX ),
                                           (int)( fCentreY - fPinchCentreY ) );
                pActions[nCount++] = Make( ACTION_CURSOR_TO, (int)fCentreX,
                                           (int)fCentreY );
            }
            fPinchCentreX = fCentreX;
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

            // One finger drags the map, the way every map on this device does
            // and every strategy game written for one. It is the commonest
            // thing a player does, so it gets the commonest gesture; the
            // selection box, which is rare, moved to two fingers.
            //
            // The land follows the finger: drag left and the ground on the
            // right comes into view. A cursor would do the opposite, which is
            // exactly why this is not left as a cursor drag.
            if ( bMovedPastSlop && !bLongPressFired )
            {
                const float fDeltaFromAnchorX = fX - fPanAnchorX;
                const float fDeltaFromAnchorY = fY - fPanAnchorY;
                nCount = SetScroll( pActions, nCount, SCROLL_LEFT,  ACTION_SCROLL_LEFT,
                                    fDeltaFromAnchorX > fSlop );
                nCount = SetScroll( pActions, nCount, SCROLL_RIGHT, ACTION_SCROLL_RIGHT,
                                    fDeltaFromAnchorX < -fSlop );
                nCount = SetScroll( pActions, nCount, SCROLL_UP,    ACTION_SCROLL_UP,
                                    fDeltaFromAnchorY > fSlop );
                nCount = SetScroll( pActions, nCount, SCROLL_DOWN,  ACTION_SCROLL_DOWN,
                                    fDeltaFromAnchorY < -fSlop );
                if ( fabsf( fDeltaFromAnchorX ) > fSlop )
                    fPanAnchorX = fX;
                if ( fabsf( fDeltaFromAnchorY ) > fSlop )
                    fPanAnchorY = fY;
                bPanning = true;
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
        nCount = ReleaseScroll( pActions, nCount );
        // A finger that went down, stayed put, and came up again is a tap --
        // and only now is that known. The press and the release go out
        // together, where the finger was.
        if ( !bMovedPastSlop && !bLongPressFired && !bTwoFinger && !bLeftDown )
        {
            pActions[nCount++] = Make( ACTION_CURSOR_TO, (int)fPressX, (int)fPressY );
            pActions[nCount++] = Make( ACTION_LEFT_DOWN, 0, 0 );
            bLeftDown = true;
        }
        nCount = ReleaseLeft( pActions, nCount );
        nActiveId = -1;
        bTwoFinger = false;
        bPanning = false;
        bBoxSelect = false;
        return nCount;

    case PHASE_CANCEL:
        nCount = ReleaseScroll( pActions, nCount );
        nCount = ReleaseLeft( pActions, nCount );
        nActiveId = -1;
        bTwoFinger = false;
        bPanning = false;
        bBoxSelect = false;
        return nCount;
    }
    return nCount;
}

}   // namespace NBk1Touch
