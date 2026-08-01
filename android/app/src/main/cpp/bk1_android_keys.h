#pragma once
// Android key codes, in the scan codes the engine's input was written against.
//
// The game's own text tells the player which key to press -- <A> for aggressive
// movement, <CTRL> with a click to force an order, function keys for the camera
// -- and 94 of those instructions sit in files the player reads. A touchscreen
// port has two ways to answer that: rewrite every one of those sentences, or
// give the player the keys they name.
//
// The second is the smaller change and the truer one. The instruction stays
// correct, the player's own key bindings keep meaning what they mean, and a
// Bluetooth keyboard -- which many people do attach to a tablet -- works without
// any of this knowing about it.
//
// Returns 0 for a key the engine has no scan code for.
int Bk1AndroidKeyToScanCode( int nAndroidKeyCode );
