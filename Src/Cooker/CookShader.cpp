#include "Cooker.h"

#include "Engine/Assets/AssetFormat.h"

#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <d3dcompiler.h>

#include <stdio.h>
#include <string.h>

// NOTE(saeb): Compiles one entry point. Returns false on a real error (already printed). A missing entry point isn't an error: it returns true with *bytecode left null.
static bool CookShaderStage(CookerContext* context, StringView8 sourcePath, const FileContents* source, const char* entryPoint, const char* target, ID3DBlob** bytecode)
{
    *bytecode = nullptr;

    UINT compileFlags = D3DCOMPILE_ENABLE_STRICTNESS;
    if(context->Debug)
    {
        compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION; // Embeds the HLSL, so RenderDoc and PIX show source
    }
    else
    {
        compileFlags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
    }

    // NOTE(saeb): The source path doubles as the name in messages, so they come out as "D:\...\Quad.hlsl(12,5): error X3000: ...", clickable like any compiler error. No include handler yet: #include fails, since the cooker's timestamp check couldn't see changes to included files anyway.
    ID3DBlob* messages = nullptr;
    HRESULT result = D3DCompile(source->Data, source->Size, (const char*)sourcePath.Data, nullptr, nullptr, entryPoint, target, compileFlags, 0, bytecode, &messages);

    const char* text = messages ? (const char*)messages->GetBufferPointer() : nullptr;

    // NOTE(saeb): X3501 is "entrypoint not found"; this file just doesn't have that stage.
    bool missing = FAILED(result) && text && strstr(text, "X3501");

    // NOTE(saeb): Errors on failure, warnings on success; either way they're already formatted.
    if(text && !missing)
    {
        printf("%s", text);
    }

    if(messages)
    {
        messages->Release();
    }

    if(missing)
    {
        return(true);
    }

    if(FAILED(result))
    {
        if(!text)
        {
            CookerError(sourcePath, "%s failed to compile (HRESULT 0x%08X)", entryPoint, (uint32)result);
        }

        return(false);
    }

    return(true);
}

static bool CookShaderWrite(CookerContext* context, StringView8 sourcePath, StringView8 outputPath, ID3DBlob* vertex, ID3DBlob* pixel)
{
    usize vertexSize = vertex ? vertex->GetBufferSize() : 0;
    usize pixelSize = pixel ? pixel->GetBufferSize() : 0;

    // NOTE(saeb): Blobs follow the shader header, each starting on SB_ASSET_ALIGNMENT; offsets are from the start of the payload, and an absent stage keeps offset and size 0.
    usize cursor = sizeof(AssetShaderHeader);
    usize vertexOffset = 0;
    usize pixelOffset = 0;

    if(vertexSize > 0)
    {
        vertexOffset = cursor;
        cursor = SB_ALIGNUP(cursor + vertexSize, SB_ASSET_ALIGNMENT);
    }

    if(pixelSize > 0)
    {
        pixelOffset = cursor;
        cursor = SB_ALIGNUP(cursor + pixelSize, SB_ASSET_ALIGNMENT);
    }

    usize payloadSize = cursor;
    usize outputSize = sizeof(AssetHeader) + payloadSize;

    uint8* output = (uint8*)Allocate(&context->Memory, Heap::Lower, outputSize, SB_ASSET_ALIGNMENT);
    if(!output)
    {
        CookerError(sourcePath, "out of memory");
        return(false);
    }

    // NOTE(saeb): Zero everything first, so padding bytes are deterministic and the same source always cooks to the same file (debug shaders excepted: the compiler stamps their embedded PDB with a new ID every time).
    memset(output, 0, outputSize);

    AssetHeader* header = (AssetHeader*)output;
    header->Magic = SB_ASSET_MAGIC;
    header->Version = SB_ASSET_VERSION;
    header->Type = AssetType::Shader;
    header->PayloadSize = payloadSize;

    uint8* payload = (uint8*)(header + 1);

    AssetShaderHeader* shaderHeader = (AssetShaderHeader*)payload;
    shaderHeader->VertexOffset = (uint32)vertexOffset;
    shaderHeader->VertexSize = (uint32)vertexSize;
    shaderHeader->PixelOffset = (uint32)pixelOffset;
    shaderHeader->PixelSize = (uint32)pixelSize;

    if(vertexSize > 0)
    {
        memcpy(payload + vertexOffset, vertex->GetBufferPointer(), vertexSize);
    }

    if(pixelSize > 0)
    {
        memcpy(payload + pixelOffset, pixel->GetBufferPointer(), pixelSize);
    }

    FileWriteResult writeResult = FileWrite(&context->Memory, outputPath, output, outputSize);
    if(writeResult != FileWriteResult::Ok)
    {
        CookerError(outputPath, "couldn't write file (%s)", CookerDescribeWrite(writeResult));
        return(false);
    }

    return(true);
}

bool CookShader(CookerContext* context, StringView8 sourcePath, StringView8 outputPath)
{
    FileContents source;
    FileReadResult readResult = FileRead(&context->Memory, Heap::Upper, sourcePath, &source);
    if(readResult != FileReadResult::Ok)
    {
        CookerError(sourcePath, "couldn't read file (%s)", CookerDescribeRead(readResult));
        return(false);
    }

    // NOTE(saeb): Pixel only compiles if the vertex stage didn't fail; a syntax error would otherwise print the same messages twice.
    ID3DBlob* vertex = nullptr;
    ID3DBlob* pixel = nullptr;
    bool compiled = CookShaderStage(context, sourcePath, &source, "VSMain", "vs_5_0", &vertex) && CookShaderStage(context, sourcePath, &source, "PSMain", "ps_5_0", &pixel);

    bool cooked = false;
    if(compiled)
    {
        if(!vertex && !pixel)
        {
            CookerError(sourcePath, "no VSMain or PSMain entry point");
        }
        else
        {
            cooked = CookShaderWrite(context, sourcePath, outputPath, vertex, pixel);
        }
    }

    if(vertex)
    {
        vertex->Release();
    }

    if(pixel)
    {
        pixel->Release();
    }

    return(cooked);
}
