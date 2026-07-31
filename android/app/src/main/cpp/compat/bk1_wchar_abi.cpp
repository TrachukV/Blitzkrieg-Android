// Makes -fshort-wchar safe to use with a prebuilt libc++.
//
// The port compiles with -fshort-wchar so that wchar_t is 16 bits, as it was
// under MSVC 6: that is what makes the engine's 231 L"..." literals and its
// std::wstring instances hold UTF-16, which is what its data and its own
// WORD* interfaces are.
//
// The hazard is that libc++ ships prebuilt with a 32-bit wchar_t, and the
// mangled names of basic_string<wchar_t>'s members do not encode the width. A
// reference from here would link against it and then read 32-bit string
// internals written by 16-bit code -- silently, at run time, far from the
// cause.
//
// The explicit instantiation below defines every one of those members in this
// object instead, so nothing is imported. Two build settings keep it that way,
// and android/tests/run_tests.sh checks the result rather than trusting it:
//
//   * libc++ is linked statically (ANDROID_STL=c++_static), so there is no
//     shared library to interpose at load time;
//   * -fvisibility=hidden, so these definitions are not exported either.
//
// If any of that is ever undone the check fails and says so, which is the
// whole point of choosing this over a failure that shows up as corrupted text
// in the middle of a mission.
#include <string>

template class std::basic_string<wchar_t>;
