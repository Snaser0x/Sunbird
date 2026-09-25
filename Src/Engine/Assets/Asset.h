#if !defined(SUNBIRD_ASSET_H)
#define SUNBIRD_ASSET_H

#include "Core/Types.h"
#include "Core/String.h"
#include "Core/StackAllocator.h"
#include "Engine/Renderer/Renderer.h"

enum class AssetLoadResult : uint8
{
    Ok,
    FileError, // FileRead failed: not found, access denied, out of memory, ...
    NotAnAsset, // Too small for a header, or the magic doesn't match
    WrongVersion, // Cooked with a different SB_ASSET_VERSION; recook
    WrongType, // For example, a shader passed to AssetLoadTexture
    MissingStage, // A shader without a stage this loader needs (PSMain, plus VSMain for the default pipeline)
    Corrupt, // Sizes, offsets or dimensions don't add up: truncated or damaged
    RendererFailed // The data was valid, but the renderer couldn't create the resource (or its table is full)
};

// NOTE(saeb): Loads a cooked .sba texture and creates it on the GPU. Relative paths resolve against the exe's folder. On any failure, *texture is 0 (the white texture), so callers can ignore the result and still draw. The file is read into Upper heap scratch and released before returning; the allocator is left exactly as it was.
AssetLoadResult AssetLoadTexture(StackAllocator* allocator, StringView8 path, RendererTexture* texture);

// NOTE(saeb): Engine startup only: loads the cooked default quad shader (both VSMain and PSMain) into the renderer's pipeline 0. Nothing draws until this succeeds. Same memory rules as AssetLoadTexture.
AssetLoadResult AssetLoadDefaultPipeline(StackAllocator* allocator, StringView8 path);

// NOTE(saeb): Loads a cooked .sba shader and creates a quad pipeline from its PSMain, which (for now) must take the default quad vertex shader's outputs (SV_Position, TEXCOORD, COLOR). A VSMain in the file is validated but not used yet; all quads share the renderer's vertex shader. On any failure, *pipeline is 0 (the default pipeline). Same memory rules as AssetLoadTexture.
AssetLoadResult AssetLoadPipeline(StackAllocator* allocator, StringView8 path, RendererPipeline* pipeline);

#endif
