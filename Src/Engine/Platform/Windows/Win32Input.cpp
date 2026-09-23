#include "Win32Input.h"
#include "Engine/Platform/Input.h"

struct InputButtonState
{
    bool IsDown; // State at the end of the frame; persists across frames
    uint8 TransitionCount; // Up/down changes this frame; reset at frame start
};

static bool InputIsDown(InputButtonState button)
{
    return(button.IsDown);
}

static bool InputPressed(InputButtonState button)
{
    // NOTE(saeb): Went up->down at least once this frame; a tap within one frame ends up with 2 transitions.
    return((button.IsDown && button.TransitionCount >= 1) || (!button.IsDown && button.TransitionCount >= 2));
}

static bool InputReleased(InputButtonState button)
{
    return((!button.IsDown && button.TransitionCount >= 1) || (button.IsDown && button.TransitionCount >= 2));
}

struct InputKeyboard
{
    InputButtonState Keys[static_cast<usize>(InputKey::Count)];
};

struct InputMouse
{
    int32 X;
    int32 Y;
    InputButtonState Buttons[static_cast<usize>(InputMouseButton::Count)];
};

struct Input
{
    InputKeyboard Keyboard;
    InputMouse Mouse;

    uint32 Flags;
};
static Input InputData = {};

static void InputSetButtonState(InputButtonState* button, bool isDown)
{
    // NOTE(saeb): Only count real changes; autorepeat sends WM_KEYDOWN again while a key is already down.
    if(button->IsDown != isDown)
    {
        button->IsDown = isDown;

        if(button->TransitionCount < 255)
        {
            ++button->TransitionCount;
        }
    }
}

static InputButtonState InputGetButtonState(InputKey key)
{
    return(InputData.Keyboard.Keys[static_cast<usize>(key)]);
}

static InputButtonState InputGetButtonState(InputMouseButton mouseButton)
{
    return(InputData.Mouse.Buttons[static_cast<usize>(mouseButton)]);
}

static bool InputAnyMouseButtonDown()
{
    for(usize mouseButtonIndex = 0; mouseButtonIndex < static_cast<usize>(InputMouseButton::Count); ++mouseButtonIndex)
    {
        if(InputData.Mouse.Buttons[mouseButtonIndex].IsDown)
        {
            return(true);
        }
    }

    return(false);
}

static void InputReleaseAllKeys()
{
    for(usize keyIndex = 0; keyIndex < static_cast<usize>(InputKey::Count); ++keyIndex)
    {
        InputSetButtonState(&InputData.Keyboard.Keys[keyIndex], false);
    }
}

static void InputReleaseAllMouseButtons()
{
    for(usize mouseButtonIndex = 0; mouseButtonIndex < static_cast<usize>(InputMouseButton::Count); ++mouseButtonIndex)
    {
        InputSetButtonState(&InputData.Mouse.Buttons[mouseButtonIndex], false);
    }
}

static InputKey Win32InputTranslateKey(WPARAM virtualKeyCode)
{
    switch(virtualKeyCode)
    {
        case VK_BACK:
        {
            return(InputKey::Backspace);
        }

        case VK_RETURN:
        {
            return(InputKey::Enter);
        }

        case VK_SPACE:
        {
            return(InputKey::Space);
        }

        case VK_ESCAPE:
        {
            return(InputKey::Escape);
        }

        case VK_OEM_3:
        {
            return(InputKey::Tilde); // US ANSI keyboard
        }

        case VK_TAB:
        {
            return(InputKey::Tab);
        }

        case VK_RIGHT:
        {
            return(InputKey::Right);
        }

        case VK_UP:
        {
            return(InputKey::Up);
        }

        case VK_DOWN:
        {
            return(InputKey::Down);
        }

        case VK_LEFT:
        {
            return(InputKey::Left);
        }

        default:
        {
            return(InputKey::Unknown);
        }
    }
}

static InputMouseButton Win32InputTranslateMouseButton(UINT message)
{
    switch(message)
    {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        {
            return(InputMouseButton::Left);
        }

        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        {
            return(InputMouseButton::Right);
        }

        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        {
            return(InputMouseButton::Middle);
        }

        default:
        {
            return(InputMouseButton::Unknown);
        }
    }
}

void Win32InputBegin()
{
    if(InputData.Flags & InputFlags_Keyboard)
    {
        for(usize keyIndex = 0; keyIndex < static_cast<usize>(InputKey::Count); ++keyIndex)
        {
            InputData.Keyboard.Keys[keyIndex].TransitionCount = 0;
        }
    }

    if(InputData.Flags & InputFlags_Mouse)
    {
        for(usize mouseButtonIndex = 0; mouseButtonIndex < static_cast<usize>(InputMouseButton::Count); ++mouseButtonIndex)
        {
            InputData.Mouse.Buttons[mouseButtonIndex].TransitionCount = 0;
        }
    }
}

void Win32InputProcess(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
        case WM_KEYUP:
        case WM_KEYDOWN:
        case WM_SYSKEYUP:
        case WM_SYSKEYDOWN:
        {
            if(!(InputData.Flags & InputFlags_Keyboard))
            {
                break;
            }

            InputKey key = Win32InputTranslateKey(wParam);

            if(key != InputKey::Unknown)
            {
                bool isDown = (message == WM_KEYDOWN || message == WM_SYSKEYDOWN);
                InputSetButtonState(&InputData.Keyboard.Keys[static_cast<usize>(key)], isDown);
            }
        } break;

        case WM_MOUSEMOVE:
        {
            if(!(InputData.Flags & InputFlags_Mouse))
            {
                break;
            }

            InputData.Mouse.X = (int32)(int16)LOWORD(lParam);
            InputData.Mouse.Y = (int32)(int16)HIWORD(lParam);
        } break;

        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        {
            if(!(InputData.Flags & InputFlags_Mouse))
            {
                break;
            }

            InputMouseButton mouseButton = Win32InputTranslateMouseButton(message);
            bool isDown = (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN);

            InputSetButtonState(&InputData.Mouse.Buttons[static_cast<usize>(mouseButton)], isDown);

            // NOTE(saeb): Capture while any button is held so the button-up still arrives outside the window. State is updated first because ReleaseCapture() sends WM_CAPTURECHANGED synchronously. SetCapture() sends WM_CAPTURECHANGED even
            // when this window already has capture, so only take it once.
            if(isDown)
            {
                if(GetCapture() != windowHandle)
                {
                    SetCapture(windowHandle);
                }
            }
            else if(!InputAnyMouseButtonDown())
            {
                ReleaseCapture();
            }
        } break;

        case WM_CAPTURECHANGED:
        {
            // NOTE(saeb): Capture was taken away (or released by us); no button-up is coming, so release every button.
            if(InputData.Flags & InputFlags_Mouse)
            {
                InputReleaseAllMouseButtons();
            }
        } break;

        case WM_ACTIVATEAPP:
        {
            // NOTE(saeb): App lost activation mid-press; the key/button-up goes to another app, so release everything.
            if(wParam == FALSE)
            {
                if(InputData.Flags & InputFlags_Keyboard)
                {
                    InputReleaseAllKeys();
                }

                if(InputData.Flags & InputFlags_Mouse)
                {
                    InputReleaseAllMouseButtons();
                }
            }
        } break;
    }
}

void InputSetFlags(uint32 inputFlags)
{
    InputData.Flags = inputFlags;
}

bool InputKeyDown(InputKey key)
{
    if(InputData.Flags & InputFlags_Keyboard)
    {
        return(InputIsDown(InputGetButtonState(key)));
    }
    else
    {
        return(false);
    }
}

bool InputKeyPressed(InputKey key)
{
    if(InputData.Flags & InputFlags_Keyboard)
    {
        return(InputPressed(InputGetButtonState(key)));
    }
    else
    {
        return(false);
    }
}

bool InputKeyReleased(InputKey key)
{
    if(InputData.Flags & InputFlags_Keyboard)
    {
        return(InputReleased(InputGetButtonState(key)));
    }
    else
    {
        return(false);
    }
}

bool InputMouseButtonDown(InputMouseButton mouseButton)
{
    if(InputData.Flags & InputFlags_Mouse)
    {
        return(InputIsDown(InputGetButtonState(mouseButton)));
    }
    else
    {
        return(false);
    }
}

bool InputMouseButtonPressed(InputMouseButton mouseButton)
{
    if(InputData.Flags & InputFlags_Mouse)
    {
        return(InputPressed(InputGetButtonState(mouseButton)));
    }
    else
    {
        return(false);
    }
}

bool InputMouseButtonReleased(InputMouseButton mouseButton)
{
    if(InputData.Flags & InputFlags_Mouse)
    {
        return(InputReleased(InputGetButtonState(mouseButton)));
    }
    else
    {
        return(false);
    }
}

void InputGetMouseXY(int32* x, int32* y)
{
    if(InputData.Flags & InputFlags_Mouse)
    {
        *x = InputData.Mouse.X;
        *y = InputData.Mouse.Y;
    }
    else
    {
        *x = 0;
        *y = 0;
    }
}
