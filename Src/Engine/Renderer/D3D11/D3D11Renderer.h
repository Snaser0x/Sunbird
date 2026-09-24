#if !defined(SUNBIRD_D3D11RENDERER_H)
#define SUNBIRD_D3D11RENDERER_H

#include "Core/Types.h"
#include "Core/StackAllocator.h"

#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

bool D3D11RendererInit(StackAllocator* allocator, HWND windowHandle);
void D3D11RendererBeginFrame(uint32 width, uint32 height);
void D3D11RendererEndFrame();
void D3D11RendererShutdown();

#endif
