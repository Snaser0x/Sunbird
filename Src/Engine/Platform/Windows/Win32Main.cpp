#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "Core/Config.h"
#include "Core/Types.h"
#include "Core/Utility.h"
#include "Core/String.h"
#include "Engine/Platform/StackAllocator.h"

static StackAllocator EngineMemory;

int APIENTRY WinMain(HINSTANCE instance, HINSTANCE prevInstance, LPSTR commandLine, int showCommand)
{
    if(InitStackAllocator(&EngineMemory, MIB(64)))
    {   
        ShutdownStackAllocator(&EngineMemory);
    }

    return(0);
}
