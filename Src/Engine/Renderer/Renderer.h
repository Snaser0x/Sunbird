#if !defined(SUNBIRD_RENDERER_H)
#define SUNBIRD_RENDERER_H

#include "Core/Types.h"

enum RendererFlags : uint32
{
    RendererFlags_None = 0,
    RendererFlags_VSync = 1 << 0
};

using RendererTexture = uint32; // 0 = white texture
using RendererPipeline = uint32; // 0 = default quad pipeline

struct RendererQuad
{
    real32 X, Y, Width, Height; // Pixels, top-left origin
    real32 U0, V0, U1, V1; // Texture Coordinates, (U0, V0) = top-left
    real32 R, G, B, A; // Color
    RendererTexture Texture;
    RendererPipeline Pipeline;
};

void RendererSetFlags(uint32 rendererFlags);
void RendererPushQuad(const RendererQuad* quad);

#endif
