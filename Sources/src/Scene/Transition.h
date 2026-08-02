#ifndef __TRANSITION_H__
#define __TRANSITION_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CTransition : public CTRefCount<ITransition>
{
	OBJECT_SERVICE_METHODS( CTransition );
	DECLARE_SERIALIZE;
	//
	NTimer::STime timeStart;							// start time
	float fAlphaStart;										// start alpha value
	float fAlphaEnd;											// end alpha value
	float fAlpha;													// current alpha value
	bool bInfinite;												// infinite updates
public:
	// None of the above were initialised, and Start is what sets them -- so a
	// transition that reaches the scene without being started carries whatever
	// was in the memory it got. On Windows that happened to read as harmless.
	// Here it read as alpha 255 of 255, infinite, and fifty-one seconds into a
	// five-hundred millisecond fade: an opaque quad over every mission after
	// the first, with everything drawing correctly underneath it.
	//
	// Started transitions are unaffected: Start assigns all five.
	CTransition()
		: timeStart( 0 ), fAlphaStart( 0.0f ), fAlphaEnd( 0.0f ), fAlpha( 0.0f ),
		  bInfinite( false ) {  }
	// update object
	bool STDCALL Update( const NTimer::STime &time, bool bForced = false );
	// drawing
	bool STDCALL Draw( interface IGFX *pGFX );
	// visiting
	void STDCALL Visit( interface ISceneVisitor *pVisitor, int nType = -1 );
	//
	int STDCALL Start( const char *pszVideoName, const DWORD dwAddFlags, const NTimer::STime &currTime, const bool bFadeIn );
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __TRANSITION_H__
