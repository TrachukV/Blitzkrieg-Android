#include "bk1_android_keys.h"

#include "compat/dinput.h"

#include <android/keycodes.h>

// One table, in Android's order, because that is the side that indexes it.
// Only the keys a keyboard actually has: media keys, the camera button and the
// rest of Android's vocabulary have no scan code and no meaning to the game.
int Bk1AndroidKeyToScanCode( int nAndroidKeyCode )
{
    switch ( nAndroidKeyCode )
    {
    // letters
    case AKEYCODE_A: return DIK_A;
    case AKEYCODE_B: return DIK_B;
    case AKEYCODE_C: return DIK_C;
    case AKEYCODE_D: return DIK_D;
    case AKEYCODE_E: return DIK_E;
    case AKEYCODE_F: return DIK_F;
    case AKEYCODE_G: return DIK_G;
    case AKEYCODE_H: return DIK_H;
    case AKEYCODE_I: return DIK_I;
    case AKEYCODE_J: return DIK_J;
    case AKEYCODE_K: return DIK_K;
    case AKEYCODE_L: return DIK_L;
    case AKEYCODE_M: return DIK_M;
    case AKEYCODE_N: return DIK_N;
    case AKEYCODE_O: return DIK_O;
    case AKEYCODE_P: return DIK_P;
    case AKEYCODE_Q: return DIK_Q;
    case AKEYCODE_R: return DIK_R;
    case AKEYCODE_S: return DIK_S;
    case AKEYCODE_T: return DIK_T;
    case AKEYCODE_U: return DIK_U;
    case AKEYCODE_V: return DIK_V;
    case AKEYCODE_W: return DIK_W;
    case AKEYCODE_X: return DIK_X;
    case AKEYCODE_Y: return DIK_Y;
    case AKEYCODE_Z: return DIK_Z;

    // digits along the top
    case AKEYCODE_0: return DIK_0;
    case AKEYCODE_1: return DIK_1;
    case AKEYCODE_2: return DIK_2;
    case AKEYCODE_3: return DIK_3;
    case AKEYCODE_4: return DIK_4;
    case AKEYCODE_5: return DIK_5;
    case AKEYCODE_6: return DIK_6;
    case AKEYCODE_7: return DIK_7;
    case AKEYCODE_8: return DIK_8;
    case AKEYCODE_9: return DIK_9;

    // the ones the game's instructions name by name
    case AKEYCODE_ESCAPE:      return DIK_ESCAPE;
    case AKEYCODE_TAB:         return DIK_TAB;
    case AKEYCODE_SPACE:       return DIK_SPACE;
    case AKEYCODE_ENTER:       return DIK_RETURN;
    case AKEYCODE_DEL:         return DIK_BACK;      // Android's DEL is backspace
    case AKEYCODE_FORWARD_DEL: return DIK_DELETE;
    case AKEYCODE_SHIFT_LEFT:  return DIK_LSHIFT;
    case AKEYCODE_SHIFT_RIGHT: return DIK_RSHIFT;
    case AKEYCODE_CTRL_LEFT:   return DIK_LCONTROL;
    case AKEYCODE_CTRL_RIGHT:  return DIK_RCONTROL;
    case AKEYCODE_ALT_LEFT:    return DIK_LMENU;
    case AKEYCODE_ALT_RIGHT:   return DIK_RMENU;

    case AKEYCODE_F1:  return DIK_F1;
    case AKEYCODE_F2:  return DIK_F2;
    case AKEYCODE_F3:  return DIK_F3;
    case AKEYCODE_F4:  return DIK_F4;
    case AKEYCODE_F5:  return DIK_F5;
    case AKEYCODE_F6:  return DIK_F6;
    case AKEYCODE_F7:  return DIK_F7;
    case AKEYCODE_F8:  return DIK_F8;
    case AKEYCODE_F9:  return DIK_F9;
    case AKEYCODE_F10: return DIK_F10;
    case AKEYCODE_F11: return DIK_F11;
    case AKEYCODE_F12: return DIK_F12;

    // punctuation, so that a real keyboard behaves like one
    case AKEYCODE_MINUS:         return DIK_MINUS;
    case AKEYCODE_EQUALS:        return DIK_EQUALS;
    case AKEYCODE_LEFT_BRACKET:  return DIK_LBRACKET;
    case AKEYCODE_RIGHT_BRACKET: return DIK_RBRACKET;
    case AKEYCODE_BACKSLASH:     return DIK_BACKSLASH;
    case AKEYCODE_SEMICOLON:     return DIK_SEMICOLON;
    case AKEYCODE_APOSTROPHE:    return DIK_APOSTROPHE;
    case AKEYCODE_GRAVE:         return DIK_GRAVE;
    case AKEYCODE_COMMA:         return DIK_COMMA;
    case AKEYCODE_PERIOD:        return DIK_PERIOD;
    case AKEYCODE_SLASH:         return DIK_SLASH;

    // arrows and the block above them: the camera keys
    case AKEYCODE_DPAD_UP:    return DIK_UP;
    case AKEYCODE_DPAD_DOWN:  return DIK_DOWN;
    case AKEYCODE_DPAD_LEFT:  return DIK_LEFT;
    case AKEYCODE_DPAD_RIGHT: return DIK_RIGHT;
    case AKEYCODE_MOVE_HOME:  return DIK_HOME;
    case AKEYCODE_MOVE_END:   return DIK_END;
    case AKEYCODE_PAGE_UP:    return DIK_PRIOR;
    case AKEYCODE_PAGE_DOWN:  return DIK_NEXT;
    case AKEYCODE_INSERT:     return DIK_INSERT;

    // the numeric pad, which the game uses for formations
    case AKEYCODE_NUMPAD_0:        return DIK_NUMPAD0;
    case AKEYCODE_NUMPAD_1:        return DIK_NUMPAD1;
    case AKEYCODE_NUMPAD_2:        return DIK_NUMPAD2;
    case AKEYCODE_NUMPAD_3:        return DIK_NUMPAD3;
    case AKEYCODE_NUMPAD_4:        return DIK_NUMPAD4;
    case AKEYCODE_NUMPAD_5:        return DIK_NUMPAD5;
    case AKEYCODE_NUMPAD_6:        return DIK_NUMPAD6;
    case AKEYCODE_NUMPAD_7:        return DIK_NUMPAD7;
    case AKEYCODE_NUMPAD_8:        return DIK_NUMPAD8;
    case AKEYCODE_NUMPAD_9:        return DIK_NUMPAD9;
    case AKEYCODE_NUMPAD_ADD:      return DIK_ADD;
    case AKEYCODE_NUMPAD_SUBTRACT: return DIK_SUBTRACT;
    case AKEYCODE_NUMPAD_MULTIPLY: return DIK_MULTIPLY;
    case AKEYCODE_NUMPAD_DIVIDE:   return DIK_DIVIDE;
    case AKEYCODE_NUMPAD_ENTER:    return DIK_NUMPADENTER;
    case AKEYCODE_NUMPAD_DOT:      return DIK_DECIMAL;

    default: return 0;
    }
}
