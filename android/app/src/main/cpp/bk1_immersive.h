#pragma once
// Hides the status and navigation bars for as long as the game has focus.
//
// Call once the activity exists, and again on every APP_CMD_GAINED_FOCUS: a
// swipe from the edge brings the bars back on purpose, and nothing puts them
// away again by itself.
struct ANativeActivity;
void Bk1GoImmersive( ANativeActivity *pActivity );
