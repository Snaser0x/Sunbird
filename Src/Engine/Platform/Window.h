#if !defined(SUNBIRD_WINDOW_H)
#define SUNBIRD_WINDOW_H

#include "Core/Types.h"

enum WindowFlags : uint32
{
    WindowFlags_None = 0,
    WindowFlags_Fullscreen = 1 << 0
};

void WindowSetFlags(uint32 windowFlags);
void WindowGetClientAreaDimensions(uint32* width, uint32* height);
bool WindowGetMinimized();

#endif
