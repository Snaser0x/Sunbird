#include "Cooker.h"

#include "Engine/Assets/AssetFormat.h"

// NOTE(saeb): PNG only, and no stdio: files come in through FileRead (Unicode paths, proper error results), so stb never opens anything itself.
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "stb/stb_image.h"

bool CookTexture(CookerContext* context, StringView8 sourcePath, StringView8 outputPath)
{
    StackAllocator* memory = &context->Memory;

    FileContents file;
    FileReadResult readResult = FileRead(memory, Heap::Upper, sourcePath, &file);
    if(readResult != FileReadResult::Ok)
    {
        CookerError(sourcePath, "couldn't read file (%s)", CookerDescribeRead(readResult));
        return(false);
    }

    // NOTE(saeb): stb takes an int length.
    if(file.Size > 0x7FFFFFFF)
    {
        CookerError(sourcePath, "file is larger than 2 GiB");
        return(false);
    }

    // NOTE(saeb): Forced to 4 channels, so grayscale, RGB and palette PNGs all come out as RGBA8, rows top to bottom.
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* pixels = stbi_load_from_memory(file.Data, (int)file.Size, &width, &height, &channels, 4);
    if(!pixels)
    {
        CookerError(sourcePath, "couldn't decode PNG (%s)", stbi_failure_reason());
        return(false);
    }

    if(width <= 0 || height <= 0 || width > SB_ASSET_MAX_TEXTURE_DIMENSION || height > SB_ASSET_MAX_TEXTURE_DIMENSION)
    {
        CookerError(sourcePath, "image is %dx%d; textures are limited to %d on a side", width, height, SB_ASSET_MAX_TEXTURE_DIMENSION);
        stbi_image_free(pixels);
        return(false);
    }

    usize pixelBytes = (usize)width * (usize)height * 4;
    usize payloadSize = sizeof(AssetTextureHeader) + pixelBytes;
    usize outputSize = sizeof(AssetHeader) + payloadSize;

    uint8* output = (uint8*)Allocate(memory, Heap::Lower, outputSize, SB_ASSET_ALIGNMENT);
    if(!output)
    {
        CookerError(sourcePath, "out of memory; a %dx%d image needs a larger cooker memory block", width, height);
        stbi_image_free(pixels);
        return(false);
    }

    AssetHeader* header = (AssetHeader*)output;
    *header = {};
    header->Magic = SB_ASSET_MAGIC;
    header->Version = SB_ASSET_VERSION;
    header->Type = AssetType::Texture;
    header->PayloadSize = payloadSize;

    AssetTextureHeader* textureHeader = (AssetTextureHeader*)(header + 1);
    *textureHeader = {};
    textureHeader->Width = (uint32)width;
    textureHeader->Height = (uint32)height;

    // NOTE(saeb): Premultiply, so fully transparent texels are black and blend to nothing with ONE / INV_SRC_ALPHA. Rounded, not truncated: (255 * 128 + 127) / 255 = 128.
    uint8* destination = (uint8*)(textureHeader + 1);
    for(usize index = 0; index < pixelBytes; index += 4)
    {
        uint32 alpha = pixels[index + 3];

        destination[index + 0] = (uint8)((pixels[index + 0] * alpha + 127) / 255);
        destination[index + 1] = (uint8)((pixels[index + 1] * alpha + 127) / 255);
        destination[index + 2] = (uint8)((pixels[index + 2] * alpha + 127) / 255);
        destination[index + 3] = (uint8)alpha;
    }

    stbi_image_free(pixels);

    FileWriteResult writeResult = FileWrite(memory, outputPath, output, outputSize);
    if(writeResult != FileWriteResult::Ok)
    {
        CookerError(outputPath, "couldn't write file (%s)", CookerDescribeWrite(writeResult));
        return(false);
    }

    return(true);
}
