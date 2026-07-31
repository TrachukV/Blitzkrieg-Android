#pragma once
// Types and keywords the engine takes from MSVC, force-included ahead of every
// translation unit so the original sources keep their spelling.
// __int64 and the calling-convention keywords come from -fms-extensions
// itself, so nothing is redefined here.
#include <cstdint>
#include <cstddef>
#include <typeinfo>
#include "bk1_win32_types.h"
#include "bk1_msvc_fpu.h"

// MSVC 6's <typeinfo> left 'type_info' in the global namespace, and the
// engine's object factory names it unqualified.
using std::type_info;

#define STDMETHODCALLTYPE
