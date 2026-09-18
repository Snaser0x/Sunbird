#if !defined(SUNBIRD_STACKALLOCATOR_H)
#define SUNBIRD_STACKALLOCATOR_H

#include "Core/Types.h"
#include "Units.h"

#define ALIGNUP(address, alignmentBytes) ((((usize)(address)) + (alignmentBytes) - 1) & (~((alignmentBytes) - 1)))

struct StackAllocator
{
    uint8* MemoryBlock;
    uint8* Base;
    uint8* Cap;
    uint8* LowerHeap;
    uint8* UpperHeap;
};

enum class Heap : uint8
{
    Lower = 0,
        Upper
};

struct Frame
{
    uint8* Mark;
    Heap Heap;
};

bool InitStackAllocator(StackAllocator* allocator, usize size);
void ShutdownStackAllocator(StackAllocator* allocator);

inline void* Allocate(StackAllocator* allocator, Heap heap, usize size, uint8 alignment)
{
    if(!allocator || size == 0 || alignment == 0)
    {
        return nullptr;
    }

    // Validate alignment is power of two.
    if((alignment & (alignment - 1)) != 0)
    {
        return nullptr;
    }

    void* memory;
    if(heap == Heap::Upper)
    {
        // From upper heap (down).
        usize aligned = ALIGNUP((usize)(allocator->UpperHeap - size), alignment);

        if(aligned < (usize)allocator->LowerHeap)
        {
            return nullptr; // Out of memory or collision
        }

        allocator->UpperHeap = (uint8*)aligned;
        memory = (void*)aligned;
    }
    else
    {
        // From lower heap (up).
        usize aligned = ALIGNUP((usize)allocator->LowerHeap, alignment);

        if(aligned + size > (usize)allocator->UpperHeap)
        {
            return nullptr; // Out of memory or collision
        }

        allocator->LowerHeap = (uint8*)(aligned + size);
        memory = (void*)aligned;
    }

    return memory;
}

inline usize GetAvailableMemory(StackAllocator* allocator)
{
    if(!allocator)
        return 0;

    return allocator->UpperHeap - allocator->LowerHeap;
}

inline usize GetUsedMemory(StackAllocator* allocator)
{
    if(!allocator)
        return 0;

    return (allocator->LowerHeap - allocator->Base) + (allocator->Cap - allocator->UpperHeap);
}

inline Frame GetFrame(StackAllocator* allocator, Heap heap)
{
    Frame frame;
    frame.Mark = (heap == Heap::Upper) ? allocator->UpperHeap : allocator->LowerHeap;
    frame.Heap = heap;

    return frame;
}

inline void ReleaseFrame(StackAllocator* allocator, Frame frame)
{
    if(frame.Heap == Heap::Upper)
    {
        allocator->UpperHeap = frame.Mark;
    }
    else
    {
        allocator->LowerHeap = frame.Mark;
    }
}

#endif
