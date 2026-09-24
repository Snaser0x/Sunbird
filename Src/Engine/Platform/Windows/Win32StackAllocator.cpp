#include "Core/StackAllocator.h"

#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

bool InitStackAllocator(StackAllocator* allocator, usize size)
{
    if(!allocator || size == 0)
    {
        return(false);
    }

    // NOTE(saeb): VirtualAlloc() guarantees page-aligned base address.
    allocator->MemoryBlock = (uint8*)VirtualAlloc(nullptr, size, MEM_COMMIT, PAGE_READWRITE);
    if(!allocator->MemoryBlock)
    {
        return(false);
    }

    allocator->Base = allocator->MemoryBlock;
    allocator->Cap = allocator->MemoryBlock + size;
    allocator->LowerHeap = allocator->Base;
    allocator->UpperHeap = allocator->Cap;

    return(true);
}

void ShutdownStackAllocator(StackAllocator* allocator)
{
    if(!allocator || !allocator->MemoryBlock)
    {
        return;
    }

    VirtualFree(allocator->MemoryBlock, 0, MEM_RELEASE);

    allocator->MemoryBlock = nullptr;
    allocator->Base = nullptr;
    allocator->Cap = nullptr;
    allocator->LowerHeap = nullptr;
    allocator->UpperHeap = nullptr;
}
