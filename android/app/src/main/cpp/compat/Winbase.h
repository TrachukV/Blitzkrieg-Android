#pragma once
// SFX/StreamFadeOff.cpp includes this by name for the timing and
// synchronisation calls, rather than reaching for <windows.h>. Everything it
// wants is already in the compatibility layer, so this points at it.
//
// The capitalisation matches the #include in the engine source. A
// case-insensitive filesystem would forgive either spelling; a Linux build
// would not, so it is spelt the way the engine spells it.
#include "windows.h"
