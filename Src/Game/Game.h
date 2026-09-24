#if !defined(SUNBIRD_GAME_H)
#define SUNBIRD_GAME_H

#include "Core/StackAllocator.h"
#include "Engine/Platform/Window.h"
#include "Engine/Platform/Input.h"
#include "Engine/Renderer/Renderer.h"

// NOTE(saeb): Only set flags here, never acquire resources (nothing tears this down).
inline void GameConfigure()
{
    WindowSetFlags(WindowFlags_None);
    RendererSetFlags(RendererFlags_VSync);
    InputSetFlags(InputFlags_Mouse | InputFlags_Keyboard);
}

inline bool GameInit(StackAllocator* allocator)
{
    return(true);
}

inline void GameUpdate(StackAllocator* allocator, real64 deltaTime)
{
}

inline void GameShutdown(StackAllocator* allocator)
{
}

#endif
