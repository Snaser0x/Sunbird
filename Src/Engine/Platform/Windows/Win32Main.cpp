#include "Win32Window.h"
#include "Win32Time.h"

#include "Game/Game.h"

#include "Core/Utility.h"

#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

static StackAllocator EngineMemory;

int APIENTRY WinMain(HINSTANCE instance, HINSTANCE prevInstance, LPSTR commandLine, int showCommand)
{
    if(InitStackAllocator(&EngineMemory, MIB(64)))
    {
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

        if(GameInit(&EngineMemory) && Win32WindowCreate(&EngineMemory, SV8(u8"Sunbird"), 1280, 720))
        {
            ShowWindow(Win32WindowGetHandle(), SW_SHOW);

            Win32TimeInit();

            while(Win32WindowPumpEvents())
            {
                Frame frameScratch = GetFrame(&EngineMemory, Heap::Upper);

                GameUpdate(&EngineMemory, Win32TimeTick());

                ReleaseFrame(&EngineMemory, frameScratch);
            }

            GameShutdown(&EngineMemory);
            Win32WindowShutdown();
        }

        ShutdownStackAllocator(&EngineMemory);
    }

    return(0);
}
