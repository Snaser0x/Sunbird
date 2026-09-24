#if !defined(SUNBIRD_RENDERER_H)
#define SUNBIRD_RENDERER_H

#include "Core/Types.h"

enum RendererFlags : uint32
{
    RendererFlags_None = 0,
    RendererFlags_VSync = 1 << 0
};

void RendererSetFlags(uint32 rendererFlags);

#endif
