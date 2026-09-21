#if !defined(SUNBIRD_GAME_H)
#define SUNBIRD_GAME_H

#include "Engine/Platform/StackAllocator.h"
#include "Engine/Platform/Window.h"
#include "Engine/Platform/Input.h"

inline bool GameInit(StackAllocator* allocator)
{
    WindowSetFlags(WindowFlags_None);
    InputSetFlags(InputFlags_Mouse | InputFlags_Keyboard);

    return(true);
}

inline void GameUpdate(StackAllocator* allocator, real64 deltaTime)
{
}

inline void GameShutdown(StackAllocator* allocator)
{
}

#endif
