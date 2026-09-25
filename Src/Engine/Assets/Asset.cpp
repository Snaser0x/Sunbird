#include "Engine/Assets/Asset.h"
#include "Engine/Assets/AssetFormat.h"
#include "Engine/Platform/File.h"

// NOTE(saeb): Reads the file into Upper heap scratch (the caller owns the frame) and checks the common header. On success, *payload points just past the header, 16-byte aligned, and *payloadSize is exact.
static AssetLoadResult AssetReadPayload(StackAllocator* allocator, StringView8 path, AssetType type, const uint8** payload, uint64* payloadSize)
{
    FileContents file;
    if(FileRead(allocator, Heap::Upper, path, &file) != FileReadResult::Ok)
    {
        return(AssetLoadResult::FileError);
    }

    if(file.Size < sizeof(AssetHeader))
    {
        return(AssetLoadResult::NotAnAsset);
    }

    const AssetHeader* header = (const AssetHeader*)file.Data;
    if(header->Magic != SB_ASSET_MAGIC)
    {
        return(AssetLoadResult::NotAnAsset);
    }

    if(header->Version != SB_ASSET_VERSION)
    {
        return(AssetLoadResult::WrongVersion);
    }

    if(header->Type != type)
    {
        return(AssetLoadResult::WrongType);
    }

    // NOTE(saeb): Exact, not "at least"; a truncated file and one with junk appended are both damaged.
    if(header->PayloadSize != file.Size - sizeof(AssetHeader))
    {
        return(AssetLoadResult::Corrupt);
    }

    *payload = (const uint8*)(header + 1);
    *payloadSize = header->PayloadSize;

    return(AssetLoadResult::Ok);
}

static AssetLoadResult AssetCreateTexture(const uint8* payload, uint64 payloadSize, RendererTexture* texture)
{
    if(payloadSize < sizeof(AssetTextureHeader))
    {
        return(AssetLoadResult::Corrupt);
    }

    const AssetTextureHeader* textureHeader = (const AssetTextureHeader*)payload;
    uint32 width = textureHeader->Width;
    uint32 height = textureHeader->Height;

    if(width == 0 || height == 0 || width > SB_ASSET_MAX_TEXTURE_DIMENSION || height > SB_ASSET_MAX_TEXTURE_DIMENSION)
    {
        return(AssetLoadResult::Corrupt);
    }

    // NOTE(saeb): At most 16384 * 16384 * 4 = 1 GiB, so this can't overflow 64 bits.
    uint64 pixelBytes = (uint64)width * height * 4;
    if(payloadSize != sizeof(AssetTextureHeader) + pixelBytes)
    {
        return(AssetLoadResult::Corrupt);
    }

    RendererTexture handle = RendererCreateTexture(width, height, (const uint8*)(textureHeader + 1));
    if(handle == 0)
    {
        return(AssetLoadResult::RendererFailed);
    }

    *texture = handle;

    return(AssetLoadResult::Ok);
}

// NOTE(saeb): A present blob must start after the shader header, on SB_ASSET_ALIGNMENT, and end inside the payload; 64-bit math, so offset + size can't wrap.
static bool AssetShaderBlobValid(uint32 offset, uint32 size, uint64 payloadSize)
{
    return(offset >= sizeof(AssetShaderHeader) && (offset % SB_ASSET_ALIGNMENT) == 0 && (uint64)offset + size <= payloadSize);
}

// NOTE(saeb): Checks every present blob; which stages are required is up to the caller.
static AssetLoadResult AssetValidateShader(const uint8* payload, uint64 payloadSize, const AssetShaderHeader** shaderHeader)
{
    if(payloadSize < sizeof(AssetShaderHeader))
    {
        return(AssetLoadResult::Corrupt);
    }

    const AssetShaderHeader* header = (const AssetShaderHeader*)payload;

    if(header->VertexSize > 0 && !AssetShaderBlobValid(header->VertexOffset, header->VertexSize, payloadSize))
    {
        return(AssetLoadResult::Corrupt);
    }

    if(header->PixelSize > 0 && !AssetShaderBlobValid(header->PixelOffset, header->PixelSize, payloadSize))
    {
        return(AssetLoadResult::Corrupt);
    }

    *shaderHeader = header;

    return(AssetLoadResult::Ok);
}

static AssetLoadResult AssetCreateDefaultPipeline(const uint8* payload, uint64 payloadSize)
{
    const AssetShaderHeader* shaderHeader = nullptr;
    AssetLoadResult result = AssetValidateShader(payload, payloadSize, &shaderHeader);
    if(result != AssetLoadResult::Ok)
    {
        return(result);
    }

    if(shaderHeader->VertexSize == 0 || shaderHeader->PixelSize == 0)
    {
        return(AssetLoadResult::MissingStage);
    }

    if(!RendererSetDefaultPipeline(payload + shaderHeader->VertexOffset, shaderHeader->VertexSize, payload + shaderHeader->PixelOffset, shaderHeader->PixelSize))
    {
        return(AssetLoadResult::RendererFailed);
    }

    return(AssetLoadResult::Ok);
}

static AssetLoadResult AssetCreatePipeline(const uint8* payload, uint64 payloadSize, RendererPipeline* pipeline)
{
    const AssetShaderHeader* shaderHeader = nullptr;
    AssetLoadResult result = AssetValidateShader(payload, payloadSize, &shaderHeader);
    if(result != AssetLoadResult::Ok)
    {
        return(result);
    }

    if(shaderHeader->PixelSize == 0)
    {
        return(AssetLoadResult::MissingStage);
    }

    RendererPipeline handle = RendererCreatePipeline(payload + shaderHeader->PixelOffset, shaderHeader->PixelSize);
    if(handle == 0)
    {
        return(AssetLoadResult::RendererFailed);
    }

    *pipeline = handle;

    return(AssetLoadResult::Ok);
}

AssetLoadResult AssetLoadTexture(StackAllocator* allocator, StringView8 path, RendererTexture* texture)
{
    *texture = 0;

    // NOTE(saeb): The GPU copies the pixels at creation, so the whole file is scratch.
    Frame scratch = GetFrame(allocator, Heap::Upper);

    const uint8* payload = nullptr;
    uint64 payloadSize = 0;
    AssetLoadResult result = AssetReadPayload(allocator, path, AssetType::Texture, &payload, &payloadSize);
    if(result == AssetLoadResult::Ok)
    {
        result = AssetCreateTexture(payload, payloadSize, texture);
    }

    ReleaseFrame(allocator, scratch);

    return(result);
}

AssetLoadResult AssetLoadDefaultPipeline(StackAllocator* allocator, StringView8 path)
{
    // NOTE(saeb): The driver keeps its own copy of the bytecode, so the whole file is scratch.
    Frame scratch = GetFrame(allocator, Heap::Upper);

    const uint8* payload = nullptr;
    uint64 payloadSize = 0;
    AssetLoadResult result = AssetReadPayload(allocator, path, AssetType::Shader, &payload, &payloadSize);
    if(result == AssetLoadResult::Ok)
    {
        result = AssetCreateDefaultPipeline(payload, payloadSize);
    }

    ReleaseFrame(allocator, scratch);

    return(result);
}

AssetLoadResult AssetLoadPipeline(StackAllocator* allocator, StringView8 path, RendererPipeline* pipeline)
{
    *pipeline = 0;

    // NOTE(saeb): The driver keeps its own copy of the bytecode, so the whole file is scratch.
    Frame scratch = GetFrame(allocator, Heap::Upper);

    const uint8* payload = nullptr;
    uint64 payloadSize = 0;
    AssetLoadResult result = AssetReadPayload(allocator, path, AssetType::Shader, &payload, &payloadSize);
    if(result == AssetLoadResult::Ok)
    {
        result = AssetCreatePipeline(payload, payloadSize, pipeline);
    }

    ReleaseFrame(allocator, scratch);

    return(result);
}
