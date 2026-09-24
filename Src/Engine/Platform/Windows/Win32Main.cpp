#include "Win32Window.h"
#include "Win32Time.h"
#include "Engine/Renderer/D3D11/D3D11Renderer.h"

#include "Game/Game.h"

#include "Core/Utility.h"

#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

static StackAllocator EngineMemory;

int APIENTRY WinMain(HINSTANCE instance, HINSTANCE prevInstance, LPSTR commandLine, int showCommand)
{
    if(InitStackAllocator(&EngineMemory, SB_MIB(64)))
    {
        GameConfigure();

        if(Win32WindowCreate(&EngineMemory, SV8(u8"Sunbird"), 1280, 720))
        {
            if(D3D11RendererInit())
            {
                if(GameInit(&EngineMemory))
                {
                    ShowWindow(Win32WindowGetHandle(), SW_SHOW);

                    Win32TimeInit();

                    while(Win32WindowPumpEvents())
                    {
                        // NOTE(saeb): Nothing to show while minimized; sleep until a message (restore, quit) arrives instead of spinning.
                        if(WindowGetMinimized())
                        {
                            WaitMessage();
                            continue;
                        }

                        Frame frameScratch = GetFrame(&EngineMemory, Heap::Upper);

                        D3D11RendererBeginFrame();
                        GameUpdate(&EngineMemory, Win32TimeTick());
                        D3D11RendererEndFrame();

                        ReleaseFrame(&EngineMemory, frameScratch);
                    }

                    GameShutdown(&EngineMemory);
                }

                D3D11RendererShutdown();
            }

            Win32WindowShutdown();
        }

        ShutdownStackAllocator(&EngineMemory);
    }

    return(0);
}
