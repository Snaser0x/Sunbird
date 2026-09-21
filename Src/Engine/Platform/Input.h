#if !defined(SUNBIRD_INPUT_H)
#define SUNBIRD_INPUT_H

#include "Core/Types.h"

enum InputFlags : uint32
{
    InputFlags_None = 0,
    InputFlags_Mouse = 1 << 0,
    InputFlags_Keyboard = 1 << 1
};

enum class InputKey : uint8
{
    Unknown,

    Backspace, Enter, Space, Escape, Tilde, Tab,
    Right, Up, Down, Left,

    Count
};

enum class InputMouseButton : uint8
{
    Unknown,

    Right,
    Middle,
    Left,

    Count
};

void InputSetFlags(uint32 inputFlags);

bool InputKeyDown(InputKey key);
bool InputKeyPressed(InputKey key);
bool InputKeyReleased(InputKey key);

bool InputMouseButtonDown(InputMouseButton mouseButton);
bool InputMouseButtonPressed(InputMouseButton mouseButton);
bool InputMouseButtonReleased(InputMouseButton mouseButton);
void InputGetMouseXY(int32* x, int32* y);

#endif
