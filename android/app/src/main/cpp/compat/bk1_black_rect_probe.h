#pragma once
// Names the caller that paints the screen black, by measurement.
//
// The mission that renders black ends its frame with a single solid rectangle
// covering the whole screen, opaque black, no texture. Seven separate attempts
// to identify its owner by reading code were each withdrawn: a measured fact,
// a few steps of reasoning, and a conclusion carried with the weight of the
// fact. This does not reason. CGraphicsEngine::DrawRects is the one funnel
// every rectangle passes through, so the return address recorded there is the
// caller itself.
//
// Fires only for the exact rectangle that was measured -- one solid rect
// covering the screen, alpha 255, colour components zero -- and only while the
// property debug.blitzkrieg.blackrect is set, so a normal run pays a float
// comparison and nothing else.
//
// Addresses are logged as offsets from the library's load base. Resolve them
// against the unstripped libblitzkrieg.so:
//   llvm-addr2line -Cfie <unstripped .so> <offset> ...
void Bk1ReportBlackScreenRect( float rminx, float rminy, float rmaxx, float rmaxy,
                               unsigned long dwColour,
                               float sminx, float sminy, float smaxx, float smaxy );

// The curtain that CTransition paints lives in the scene's always-visible list.
// A screen lifts it by clearing that list when it starts. Following the list --
// who adds to it, who clears it, and which scene it belongs to -- is what shows
// why the curtain outlives the screen that lowered it.
//
// Enabled by debug.blitzkrieg.always, read afresh on every call.
void Bk1TraceAlwaysObjects( const char *pszWhat, const void *pObject, int nSize, const void *pScene );

// Lifts the curtain lowered by FinishInterface. Declared here as well as in
// InterfaceScreenBase.h because CMainLoop::PopInterface needs it and Main does
// not include the Common screen header. Defined in InterfaceScreenBase.cpp.
void Bk1LiftCurtain();

// Save and load compose a file name and then act on it. Neither works: load
// leaves its dialog open, save closes and writes nothing. This prints what each
// one actually built, behind debug.blitzkrieg.saveload.
void Bk1TracePath( const char *pszTag, const char *pszPath, int nValue );

// Names the caller by return address, the way the black-screen probe did.
void Bk1TraceBacktrace( const char *pszTag );

// The engine's own checked_cast net, reported instead of trapped. Its original
// failure path is x86 inline assembly, so on arm64 the net was never compiled
// and every checked_cast has been an unchecked static_cast.
void Bk1ReportBadCast( const char *pszType );
