# The native build.
#
# Two things are built here: the compatibility layer, which answers everything
# the engine believes about its environment, and the Android layer, which owns
# the surface and the frame loop behind those answers. Engine modules join this
# list as they are finished -- the ones that compile are checked by
# android/compile_check.sh, which reads each module's own .dsp for its sources
# rather than globbing, because the .dsp is the authoritative list.
cmake_minimum_required( VERSION 3.18.1 )
project( blitzkrieg CXX C )

set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )

set( SOURCES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../Sources/src )

# The flags the engine needs, all of them measured rather than guessed:
#
#  -fms-extensions and -fdelayed-template-parsing, because this is 2003 MSVC
#   source and its templates are parsed the way that compiler parsed them.
#  -fshort-wchar, because wchar_t is 16 bits here and the data files hold
#   UTF-16. This one is an ABI hazard -- libc++ is built with 32-bit wchar_t
#   and the mangled names collide -- which is why bk1_wchar_abi.cpp exists and
#   why tests/check_wchar_abi.sh proves no such symbol is imported.
#  -fvisibility=hidden with a static C++ library, so that check stays true.
# Guarded by the language, because the NDK's glue is C and neither the C++
# template flags nor the force-include below mean anything to it.
add_compile_options(
    "$<$<COMPILE_LANGUAGE:CXX>:-fms-extensions>"
    "$<$<COMPILE_LANGUAGE:CXX>:-fdelayed-template-parsing>"
    "$<$<COMPILE_LANGUAGE:CXX>:-Wno-reserved-user-defined-literal>"
    "$<$<COMPILE_LANGUAGE:CXX>:-Wno-invalid-offsetof>"
    "$<$<COMPILE_LANGUAGE:CXX>:-D_LIBCPP_ENABLE_CXX17_REMOVED_FEATURES>"
    # bk1_msvc_types.h is force-included into every C++ unit: it is what makes
    # the original sources compile unchanged.
    "$<$<COMPILE_LANGUAGE:CXX>:-include>"
    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/compat/bk1_msvc_types.h>"
    # These two are not language-guarded on purpose: wchar_t's width and symbol
    # visibility have to hold across every unit that ends up in the library, or
    # the ABI check stops meaning anything.
    -fshort-wchar
    -fvisibility=hidden
    -fvisibility-inlines-hidden
)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/compat
    ${CMAKE_CURRENT_SOURCE_DIR}/compat/stl
    ${SOURCES_DIR}
)

# The NDK's own glue, which turns the activity's callbacks into a queue the
# frame loop can poll.
add_library( native_app_glue STATIC
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c )
target_include_directories( native_app_glue PUBLIC
    ${ANDROID_NDK}/sources/android/native_app_glue )

set( COMPAT_SOURCES
    compat/bk1_win32_files.cpp
    compat/bk1_win32_fileio.cpp
    compat/bk1_win32_strings.cpp
    compat/bk1_win32_registry.cpp
    compat/bk1_win32_process.cpp
    compat/bk1_win32_window.cpp
    compat/bk1_win32_keys.cpp
    compat/bk1_msvc_fpu.cpp
    compat/bk1_wchar_abi.cpp
    compat/bk1_msxml.cpp
    compat/bk1_s3tc.cpp
    compat/bk1_ddraw.cpp
    compat/bk1_bink.cpp
    compat/bk1_dinput.cpp
    compat/bk1_com_apartment.cpp
    compat/bk1_d3d8_resources.cpp
    compat/bk1_d3d8_device.cpp
)

add_library( blitzkrieg SHARED
    bk1_android_main.cpp
    ${COMPAT_SOURCES}
)

target_link_libraries( blitzkrieg
    native_app_glue
    android
    EGL
    GLESv3
    log
)

# The glue's entry point is reached through the linker, not through a call, so
# it must survive the collector.
set_target_properties( blitzkrieg PROPERTIES
    LINK_FLAGS "-u ANativeActivity_onCreate" )
