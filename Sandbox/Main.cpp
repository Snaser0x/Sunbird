#include "Core/Config.h"
#include "Core/Types.h"
#include "Core/String.h"
#include "Memory/StackAllocator.h"

#if SB_PLATFORM_WINDOWS
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

static StackAllocator allocator;

int APIENTRY WinMain(HINSTANCE instance, HINSTANCE prevInstance, LPSTR commandLine, int showCommand)
{
    if(InitStackAllocator(&allocator, MiB(64)))
    {
        String8 name = String8FromLiteral(&allocator, u8"Saeb");

        String8 greeting = String8FromView(&allocator, SV8(u8"Hello"));

        String8 path = String8Reserve(&allocator, 256);
        String8Append(&path, SV8(u8"Assets/"));
        String8Append(&path, SV8(u8"Models/"));
        String8Append(&path, SV8(u8"Character.mesh/"));

        usize usedMemory = GetUsedMemory(&allocator);
        usize availableMemory = GetAvailableMemory(&allocator);

        Frame frameStrings = GetFrame(&allocator, Heap::Lower);
        {
            String8 temp1 = String8FromLiteral(&allocator, u8"Temporary");
            String8 temp2 = String8FromLiteral(&allocator, u8"Strings");

            usedMemory = GetUsedMemory(&allocator);
            availableMemory = GetAvailableMemory(&allocator);
        }
        ReleaseFrame(&allocator, frameStrings);

        usedMemory = GetUsedMemory(&allocator);
        availableMemory = GetAvailableMemory(&allocator);
    }

    ShutdownStackAllocator(&allocator);
    return(0);
}
#else
int main(void)
{
	return(0);
}
#endif
