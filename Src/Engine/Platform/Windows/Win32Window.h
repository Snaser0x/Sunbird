#if !defined(SUNBIRD_WIN32WINDOW_H)
#define SUNBIRD_WIN32WINDOW_H

#include "Core/Types.h"
#include "Core/String.h"

#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

bool Win32WindowCreate(StackAllocator* allocator, StringView8 title, uint32 width, uint32 height);
bool Win32WindowPumpEvents();
void Win32WindowShutdown();
HWND Win32WindowGetHandle();

#endif
