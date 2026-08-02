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
    # Not language-guarded: the engine writes its includes with backslashes,
    # "..\zlib\zlib.h", and this is what makes a compiler on this side of the
    # world read them as paths. libpng's png.h does it from C. Only the
    # force-include below is C++-only -- that is what the C glue choked on.
    -fms-extensions
    "$<$<COMPILE_LANGUAGE:CXX>:-fdelayed-template-parsing>"
    "$<$<COMPILE_LANGUAGE:CXX>:-Wno-reserved-user-defined-literal>"
    "$<$<COMPILE_LANGUAGE:CXX>:-Wno-invalid-offsetof>"
    # The NDK makes -Wformat-security fatal by default. The engine's console
    # takes a computed string -- NStr::Format has already done the formatting,
    # and what reaches printf is the finished text. That is exactly the shape
    # the warning describes and exactly the case where it is not a defect, so
    # it stays a warning here rather than stopping the build.
    "$<$<COMPILE_LANGUAGE:CXX>:-Wno-error=format-security>"
    "$<$<COMPILE_LANGUAGE:CXX>:-D_LIBCPP_ENABLE_CXX17_REMOVED_FEATURES>"
    # NI_ASSERT_TF's third argument is the engine's error handling, not a
    # diagnostic -- "return 0" when a file is missing, and so on -- and without
    # this the whole macro expands to {} and takes that statement with it.
    # BugSlayer.h says these are "meant to be enabled in release builds", and
    # the shipped game was built that way. See compat/bk1_bugslayer.cpp.
    "$<$<COMPILE_LANGUAGE:CXX>:-D_DO_ASSERT>"
    # The engine ships its own run-time check that a downcast is legal, and it
    # has been off for this whole port for a silly reason: its failure path is
    # x86 inline assembly, so the arm64 build could never compile it. Basic.h now
    # reports instead of trapping, which lets the net be switched on. A wrong
    # checked_cast is otherwise an unchecked static_cast that yields a pointer of
    # the wrong type in silence -- and a virtual call through one is exactly what
    # the load-crash stack shows.
    "$<$<COMPILE_LANGUAGE:CXX>:-D_DO_CHECKED_CAST>"
    # bk1_msvc_types.h is force-included into every C++ unit: it is what makes
    # the original sources compile unchanged.
    "$<$<COMPILE_LANGUAGE:CXX>:-include>"
    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/compat/bk1_msvc_types.h>"
    # These two are not language-guarded on purpose: wchar_t's width and symbol
    # visibility have to hold across every unit that ends up in the library, or
    # the ABI check stops meaning anything.
    -fshort-wchar
    # ...and with it, the wide-string functions have to be ours. Clang folds
    # wcslen into an inline scan sized for the 32-bit wchar_t it believes the
    # target has, so std::wstring built from a pointer -- which takes its length
    # through char_traits<wchar_t>::length -- came out short. That reached the
    # screen: a mission instruction of 198 characters drew as 99, cut mid-word.
    # Measured at the point the string is built, the same text counted 320 by
    # hand and 162 through std::wstring.
    #
    # Being inline is also why checking the shipped library for imported wcs*
    # symbols found none and wrongly cleared the C library of any part in it.
    # These flags send the calls to compat/bk1_wide_strings.cpp instead, which
    # counts in 16-bit units.
    -fno-builtin-wcslen
    -fno-builtin-wcscpy
    -fno-builtin-wcscmp
    -fno-builtin-wmemcpy
    -fno-builtin-wmemmove
    -fno-builtin-wmemset
    -fno-builtin-wmemcmp
    -fno-builtin-wmemchr
    # char is signed under MSVC and unsigned on ARM by default, and the engine
    # was written against the first. It packs a surface normal into a DWORD one
    # byte a component and reads it back with
    #     float( char( cNumber ) ) / 127.0f
    # which on ARM turns every negative component into a large positive one:
    # -1.0 packs to 129, and 129 read unsigned is +1.016 rather than -1.0.
    # Measured, in a mission: the terrain normal under a vehicle came back as
    # 0.573 0.580 0.580 -- normalize(1,1,1) -- instead of near-vertical, which
    # collapsed the shadow projection's denominator to -0.0028 and threw the
    # shadow across the map as a second solid copy of the vehicle.
    # This is a whole class of bug, not one line, so it is fixed where the
    # assumption lives rather than at each place that relies on it.
    -fsigned-char
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
    compat/bk1_wide_strings.cpp
    compat/bk1_win32_fileio.cpp
    compat/bk1_win32_strings.cpp
    compat/bk1_win32_registry.cpp
    compat/bk1_win32_process.cpp
    compat/bk1_win32_window.cpp
    compat/bk1_win32_keys.cpp
    compat/bk1_win32_version.cpp
    compat/bk1_bugslayer.cpp
    compat/bk1_wave.cpp
    compat/bk1_fmod_audio.cpp
    compat/bk1_msvc_fpu.cpp
    compat/bk1_wchar_abi.cpp
    compat/bk1_msxml.cpp
    compat/bk1_s3tc.cpp
    compat/bk1_ddraw.cpp
    compat/bk1_bink.cpp
    compat/bk1_dinput.cpp
    compat/bk1_black_rect_probe.cpp
    compat/bk1_com_apartment.cpp
    compat/bk1_d3d8_resources.cpp
    compat/bk1_d3d8_device.cpp
)

# --- the engine ---------------------------------------------------------------
# Each module's source list comes out of its own .dsp, which is the build file
# the game was actually built with. Globbing *.cpp would sweep in files that
# were never in the build -- dead experiments, editor-only helpers -- and
# hand-copying the lists would let them drift. android/compile_check.sh reads
# the same .dsp files, so what is checked and what is built cannot disagree.
find_package( Python3 COMPONENTS Interpreter REQUIRED )

function( bk1_module_sources MODULE OUT_VAR )
    execute_process(
        COMMAND ${Python3_EXECUTABLE}
                "${CMAKE_CURRENT_SOURCE_DIR}/../../../../list_module_sources.py"
                "${SOURCES_DIR}" "${MODULE}"
        OUTPUT_VARIABLE LISTING
        RESULT_VARIABLE STATUS
        OUTPUT_STRIP_TRAILING_WHITESPACE )
    if( NOT STATUS EQUAL 0 )
        message( FATAL_ERROR "cannot list ${MODULE}: ${STATUS}" )
    endif()
    string( REPLACE "\n" ";" RESULT "${LISTING}" )
    set( ${OUT_VAR} "${RESULT}" PARENT_SCOPE )
endfunction()

# Every module in the game's dependency closure. ELK, MapEditor and editor are
# absent because nothing the game runs reaches them.
set( ENGINE_MODULES
    Misc StreamIO Formats Image Anim Common zlib libpng GameSpy LuaLib
    RandomMapGen Net Scene UI GFX Input Main GameTT AILogic SFX
)

set( ENGINE_SOURCES "" )
foreach( MODULE ${ENGINE_MODULES} )
    bk1_module_sources( ${MODULE} MODULE_SOURCES )
    list( LENGTH MODULE_SOURCES COUNT )
    message( STATUS "  ${MODULE}: ${COUNT} sources" )
    list( APPEND ENGINE_SOURCES ${MODULE_SOURCES} )
endforeach()

# --- twenty DLLs becoming one library -----------------------------------------
# Every module carried a byte-identical GlobalsLoader.cpp. That was not
# duplication by accident: each module was a DLL, and a DLL needs its own
# pointers to the process-wide singletons, filled in at load time by pulling
# GetSLS_Hook, GetSingletonGlobal_Hook and GetTempRawBuffer_Hook out of
# streamio.dll. Twenty copies of the same four globals, one per module, each
# private to its own DLL.
#
# Here there is one binary and one address space, so exactly one copy is
# correct and the other nineteen are duplicate symbols at link time.
#
# The file already knows what to do about it. Its _STREAMIO_DLL branch is the
# case where the code is in the same binary as StreamIO: it calls those three
# hooks directly instead of looking them up in a library. That branch was
# written for StreamIO itself, and in this build it is true of everything. So
# StreamIO's copy is the one that is built, with the branch selected, and the
# nineteen others are dropped.
set( GLOBALS_LOADER_KEPT "${SOURCES_DIR}/StreamIO/GlobalsLoader.cpp" )

# Two more files exist twice for the same reason. Net/Streams.cpp is a
# byte-identical copy of StreamIO/Streams.cpp, and Scene/TerrainBuilder.cpp of
# RandomMapGen/TerrainBuilder.cpp -- a DLL could not share a translation unit
# with another DLL, so it carried its own. One address space, one copy.
# (GameTT/AckManager.cpp and AILogic/AckManager.cpp share a name but not their
# contents, so both are built.)
set( DUPLICATED_SOURCES
    "${SOURCES_DIR}/Net/Streams.cpp"
    "${SOURCES_DIR}/Scene/TerrainBuilder.cpp"
)

# And Main/LoadDLLs.cpp is the DLL loader itself -- a platform file, like
# WinMain was. bk1_module_registry.cpp replaces it, keeping the same NMain
# interface but taking the descriptors from the modules linked in here.
list( APPEND DUPLICATED_SOURCES "${SOURCES_DIR}/Main/LoadDLLs.cpp" )

# libpng's own test harness. Its .dsp lists it because the project built both
# the library and the test; it carries a main() and nothing in the game calls
# it.
list( APPEND DUPLICATED_SOURCES "${SOURCES_DIR}/libpng/pngtest.c" )

set( FILTERED_SOURCES "" )
set( DROPPED_LOADERS 0 )
foreach( SOURCE ${ENGINE_SOURCES} )
    get_filename_component( SOURCE_NAME "${SOURCE}" NAME )
    set( DROP FALSE )
    if( SOURCE_NAME STREQUAL "GlobalsLoader.cpp" AND
        NOT "${SOURCE}" STREQUAL "${GLOBALS_LOADER_KEPT}" )
        set( DROP TRUE )
    endif()
    if( "${SOURCE}" IN_LIST DUPLICATED_SOURCES )
        set( DROP TRUE )
    endif()
    if( DROP )
        math( EXPR DROPPED_LOADERS "${DROPPED_LOADERS} + 1" )
    else()
        list( APPEND FILTERED_SOURCES "${SOURCE}" )
    endif()
endforeach()
set( ENGINE_SOURCES ${FILTERED_SOURCES} )
message( STATUS "dropped ${DROPPED_LOADERS} sources that exist once per DLL" )

# Each module exports GetModuleDescriptor under that one name, which is fine
# for eleven separate DLLs and a collision in one library. Renaming it per
# module at the compiler rather than in the source keeps the engine files
# untouched -- and bk1_module_registry.cpp calls them by these names.
set( DESCRIPTOR_SOURCES
    "StreamIO/StreamIOObjectFactory.cpp"
    "Image/ImageObjectFactory.cpp"
    "Anim/AnimObjectFactory.cpp"
    "Net/NetObjectFactory.cpp"
    "Scene/SceneObjectFactory.cpp"
    "UI/UIObjectFactory.cpp"
    "GFX/GFXObjectFactory.cpp"
    "Input/InputObjectFactory.cpp"
    "GameTT/MissionObjectFactory.cpp"
    "AILogic/AILogicObjectFactory.cpp"
    "SFX/SoundObjectFactory.cpp"
)
foreach( RELATIVE ${DESCRIPTOR_SOURCES} )
    string( REGEX REPLACE "/.*" "" MODULE "${RELATIVE}" )
    set( FULL "${SOURCES_DIR}/${RELATIVE}" )
    if( NOT EXISTS "${FULL}" )
        message( FATAL_ERROR "no ${FULL} -- the descriptor list is stale" )
    endif()
    set_property( SOURCE "${FULL}" APPEND PROPERTY COMPILE_DEFINITIONS
                  "GetModuleDescriptor=Bk1GetModuleDescriptor_${MODULE}" )
endforeach()

set_property( SOURCE "${GLOBALS_LOADER_KEPT}" APPEND PROPERTY
              COMPILE_DEFINITIONS "_STREAMIO_DLL" "_DONT_LOAD_STREAMIO" )

list( LENGTH ENGINE_SOURCES ENGINE_COUNT )
message( STATUS "engine: ${ENGINE_COUNT} translation units" )

add_library( blitzkrieg SHARED
    bk1_android_main.cpp
    bk1_android_keys.cpp
    bk1_game_startup.cpp
    bk1_touch_gestures.cpp
    bk1_touch_panel.cpp
    bk1_module_registry.cpp
    ${COMPAT_SOURCES}
    ${ENGINE_SOURCES}
)

# The engine's headers are reached as "Foo.h" from within a module and as
# "..\Module\Foo.h" from outside it. The second needs Sources/src on the path.
# The first needs nothing: a quoted include looks in the including file's own
# directory first, so Misc/Foo.cpp finds Misc/StdAfx.h by itself.
target_include_directories( blitzkrieg PRIVATE ${SOURCES_DIR} )

# Except for a source that sits in a subdirectory of its module. There is one --
# AILogic/Scripts/Scripts.cpp -- and its "stdafx.h" means AILogic's, which is
# not in its own directory. The original project compiled each module with the
# module's directory current, which is what made that resolve.
#
# This is done per source and not by putting every module directory on the
# global path, and the difference matters: thirty-five modules have a StdAfx.h
# and they are not interchangeable. A global list would resolve every such
# fall-through to whichever module happened to be listed first.
foreach( SOURCE ${ENGINE_SOURCES} )
    get_filename_component( SOURCE_DIR "${SOURCE}" DIRECTORY )
    get_filename_component( PARENT_DIR "${SOURCE_DIR}" DIRECTORY )
    if( NOT "${PARENT_DIR}" STREQUAL "${SOURCES_DIR}" )
        set_property( SOURCE "${SOURCE}" APPEND PROPERTY
                      INCLUDE_DIRECTORIES "${PARENT_DIR}" )
    endif()
endforeach()

target_link_libraries( blitzkrieg
    native_app_glue
    android
    EGL
    GLESv3
    # The audio device and the music decoder, both from the NDK itself.
    aaudio
    mediandk
    log
)

# The glue's entry point is reached through the linker, not through a call, so
# it must survive the collector.
set_target_properties( blitzkrieg PROPERTIES
    LINK_FLAGS "-u ANativeActivity_onCreate" )
