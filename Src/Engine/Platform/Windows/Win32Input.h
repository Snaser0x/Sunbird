#if !defined(SUNBIRD_WIN32INPUT_H)
#define SUNBIRD_WIN32INPUT_H

#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

void Win32InputBegin();
void Win32InputProcess(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam);

#endif
