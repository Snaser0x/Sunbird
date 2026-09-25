#if !defined(SUNBIRD_COOKER_H)
#define SUNBIRD_COOKER_H

#include "Core/Types.h"
#include "Core/String.h"
#include "Core/StackAllocator.h"
#include "Engine/Platform/File.h"

struct CookerContext
{
    StackAllocator Memory;
    bool Debug; // Shaders keep debug info and skip optimization, so RenderDoc / PIX shows the HLSL
};

// NOTE(saeb): Prints "path: error: message", MSVC's format, so the path is clickable in Visual Studio and most terminals. A null path prints just the message. The message is a printf format.
void CookerError(StringView8 path, const char* format, ...);

// NOTE(saeb): Short reasons for error messages: "not found", "access denied", ...
const char* CookerDescribeRead(FileReadResult result);
const char* CookerDescribeWrite(FileWriteResult result);

// NOTE(saeb): Both paths are absolute UTF-8; the output's parent folders may not exist yet (FileWrite creates them). Each prints its own error and returns false on failure. The caller wraps every call in Lower and Upper frames, so a cook can allocate freely and never needs to clean up.
bool CookTexture(CookerContext* context, StringView8 sourcePath, StringView8 outputPath);
bool CookShader(CookerContext* context, StringView8 sourcePath, StringView8 outputPath);

#endif
