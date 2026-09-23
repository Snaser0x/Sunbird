#if !defined(SUNBIRD_D3D11Render_H)
#define SUNBIRD_D3D11Render_H

#include "Core/Types.h"
#include "Core/StackAllocator.h"

#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

bool D3D11RenderInit();
void D3D11RenderClear();
void D3D11RenderPresent();
void D3D11RenderShutdown();

#endif
