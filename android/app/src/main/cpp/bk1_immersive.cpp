// Hide the system bars, the way a game does.
//
// The manifest already asks for a fullscreen theme, and that removes the status
// bar, but the navigation bar stays: on a modern device it is drawn over the
// game and the strip it occupies is lost. Every game hides it, and a player who
// wants it back swipes from the edge.
//
// There is no manifest attribute for this -- android:immersive is an older,
// unrelated flag about activity transitions. It has to be asked for on the
// window at run time, and with no Java in this port that means JNI.
//
// It also has to be asked for again after every focus change: the swipe that
// brings the bars back is temporary, and the system does not restore the hidden
// state by itself. That is what the STICKY flag makes bearable and what the
// call from APP_CMD_GAINED_FOCUS finishes.

#include "bk1_immersive.h"

#include <android/native_activity.h>
#include <android/log.h>

#include <jni.h>

#define LOGI( ... ) __android_log_print( ANDROID_LOG_INFO, "blitzkrieg", __VA_ARGS__ )
#define LOGE( ... ) __android_log_print( ANDROID_LOG_ERROR, "blitzkrieg", __VA_ARGS__ )

namespace
{
// View.SYSTEM_UI_FLAG_*, as documented. Named here rather than written as one
// number so the intent survives reading.
const int SYSTEM_UI_FLAG_FULLSCREEN            = 0x00000004;
const int SYSTEM_UI_FLAG_HIDE_NAVIGATION       = 0x00000002;
const int SYSTEM_UI_FLAG_LAYOUT_STABLE         = 0x00000100;
const int SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION = 0x00000200;
const int SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN     = 0x00000400;
const int SYSTEM_UI_FLAG_IMMERSIVE_STICKY      = 0x00001000;
}

void Bk1GoImmersive( ANativeActivity *pActivity )
{
    if ( pActivity == 0 || pActivity->vm == 0 )
        return;

    JNIEnv *pEnv = 0;
    // The caller is the app's own thread, not the one the VM knows about, so it
    // has to attach before it may touch anything Java.
    if ( pActivity->vm->AttachCurrentThread( &pEnv, 0 ) != JNI_OK || pEnv == 0 )
    {
        LOGE( "immersive: cannot attach to the VM" );
        return;
    }

    jclass clsActivity = pEnv->GetObjectClass( pActivity->clazz );
    jmethodID midGetWindow = pEnv->GetMethodID( clsActivity, "getWindow", "()Landroid/view/Window;" );
    if ( midGetWindow == 0 )
    {
        LOGE( "immersive: no getWindow" );
        pActivity->vm->DetachCurrentThread();
        return;
    }
    jobject objWindow = pEnv->CallObjectMethod( pActivity->clazz, midGetWindow );

    jclass clsWindow = pEnv->GetObjectClass( objWindow );
    jmethodID midGetDecorView = pEnv->GetMethodID( clsWindow, "getDecorView", "()Landroid/view/View;" );
    jobject objDecor = pEnv->CallObjectMethod( objWindow, midGetDecorView );

    jclass clsView = pEnv->GetObjectClass( objDecor );
    jmethodID midSetFlags = pEnv->GetMethodID( clsView, "setSystemUiVisibility", "(I)V" );
    if ( midSetFlags == 0 )
    {
        LOGE( "immersive: no setSystemUiVisibility" );
        pActivity->vm->DetachCurrentThread();
        return;
    }

    // The three LAYOUT flags keep the surface the same size whether the bars are
    // showing or not, so a swipe that reveals them does not resize the game
    // underneath and move every button the player is aiming at.
    const int nFlags = SYSTEM_UI_FLAG_LAYOUT_STABLE
                     | SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                     | SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                     | SYSTEM_UI_FLAG_HIDE_NAVIGATION
                     | SYSTEM_UI_FLAG_FULLSCREEN
                     | SYSTEM_UI_FLAG_IMMERSIVE_STICKY;
    pEnv->CallVoidMethod( objDecor, midSetFlags, nFlags );

    if ( pEnv->ExceptionCheck() )
    {
        pEnv->ExceptionClear();
        LOGE( "immersive: the call threw" );
    }

    pActivity->vm->DetachCurrentThread();
}
