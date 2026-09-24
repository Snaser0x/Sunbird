#if !defined(SUNBIRD_STRING_H)
#define SUNBIRD_STRING_H

#include "Types.h"
#include "StackAllocator.h"

using char8 = char8_t;
using char16 = char16_t;

struct StringView8
{
    const char8* Data;
    usize Length;
};

struct String8
{
    char8* Data;
    usize Length;
    usize Capacity;
};

struct StringView16
{
    const char16* Data;
    usize Length;
};

struct String16
{
    char16* Data;
    usize Length;
    usize Capacity;
};

inline StringView8 SV8(const char8* data)
{
    usize length = 0;
    while(data[length] != '\0')
    {
        length++;
    }

    return(StringView8{ data, length });
}

inline StringView16 SV16(const char16* data)
{
    usize length = 0;
    while(data[length] != '\0')
    {
        length++;
    }

    return(StringView16{ data, length });
}

struct String8Decoded
{
    uint32 Codepoint;
    usize Length;
};

inline String8Decoded String8DecodeNext(StringView8 view, usize index)
{
    // NOTE(saeb): Invalid input decodes to U+FFFD and consumes one byte, so a bad byte never swallows the valid bytes after it.
    const uint32 Replacement = 0xFFFD;
    char8 lead = view.Data[index];

    // 1-byte (ASCII).
    if((lead & 0x80) == 0)
    {
        return(String8Decoded{ lead, 1 });
    }

    usize length;
    uint32 codepoint;
    uint32 minimum;

    if((lead & 0xE0) == 0xC0)
    {
        length = 2;
        codepoint = lead & 0x1F;
        minimum = 0x80;
    }
    else if((lead & 0xF0) == 0xE0)
    {
        length = 3;
        codepoint = lead & 0x0F;
        minimum = 0x800;
    }
    else if((lead & 0xF8) == 0xF0)
    {
        length = 4;
        codepoint = lead & 0x07;
        minimum = 0x10000;
    }
    else
    {
        // Continuation byte without a lead, or 0xF8..0xFF.
        return(String8Decoded{ Replacement, 1 });
    }

    // Truncated at the end of the view.
    if(length > view.Length - index)
    {
        return(String8Decoded{ Replacement, 1 });
    }

    for(usize i = 1; i < length; ++i)
    {
        char8 next = view.Data[index + i];
        if((next & 0xC0) != 0x80)
        {
            return(String8Decoded{ Replacement, 1 });
        }

        codepoint = (codepoint << 6) | (next & 0x3F);
    }

    // Overlong, UTF-16 surrogate, or beyond Unicode.
    if(codepoint < minimum || (codepoint >= 0xD800 && codepoint <= 0xDFFF) || codepoint > 0x10FFFF)
    {
        return(String8Decoded{ Replacement, 1 });
    }

    return(String8Decoded{ codepoint, length });
}

inline usize SV8ToSV16Length(StringView8 view)
{
    usize length = 0;

    for(usize i = 0; i < view.Length;)
    {
        String8Decoded decoded = String8DecodeNext(view, i);

        // Above the BMP needs a surrogate pair.
        length += (decoded.Codepoint >= 0x10000) ? 2 : 1;
        i += decoded.Length;
    }

    return(length);
}

inline StringView16 SV8ToSV16(StackAllocator* allocator, StringView8 view)
{
    if(!allocator)
    {
        return(StringView16{ nullptr, 0 });
    }

    usize bufferCapacity = SV8ToSV16Length(view) + 1;
    char16* buffer = (char16*)Allocate(allocator, Heap::Upper, bufferCapacity * sizeof(char16), alignof(char16));
    if(!buffer)
    {
        return(StringView16{ nullptr, 0 });
    }

    usize bufferIndex = 0;

    for(usize i = 0; i < view.Length;)
    {
        String8Decoded decoded = String8DecodeNext(view, i);

        if(decoded.Codepoint >= 0x10000)
        {
            // Surrogate pair.
            uint32 offset = decoded.Codepoint - 0x10000;
            buffer[bufferIndex++] = (char16)(0xD800 + (offset >> 10));
            buffer[bufferIndex++] = (char16)(0xDC00 + (offset & 0x3FF));
        }
        else
        {
            buffer[bufferIndex++] = (char16)decoded.Codepoint;
        }

        i += decoded.Length;
    }

    buffer[bufferIndex] = u'\0';
    
    return(StringView16{ buffer, bufferIndex });
}

inline String8 String8FromView(StackAllocator* allocator, StringView8 view)
{
    if(!allocator || view.Length == 0)
    {
        return(String8{ nullptr, 0, 0 });
    }

    // Allocate with null terminator.
    char8* data = (char8*)Allocate(allocator, Heap::Lower, view.Length + 1, alignof(char8));
    if(!data)
    {
        return(String8{ nullptr, 0, 0 });
    }

    // Copy data.
    for(usize i = 0; i < view.Length; i++)
    {
        data[i] = view.Data[i];
    }
    data[view.Length] = '\0';

    return(String8{ data, view.Length, view.Length + 1 });
}

inline String8 String8FromLiteral(StackAllocator* allocator, const char8* literal)
{
    return(String8FromView(allocator, SV8(literal)));
}

inline String8 String8Reserve(StackAllocator* allocator, usize capacity)
{
    if(!allocator || capacity == 0)
    {
        return(String8{ nullptr, 0, 0 });
    }

    char8* data = (char8*)Allocate(allocator, Heap::Lower, capacity, alignof(char8));
    if(!data)
    {
        return(String8{ nullptr, 0, 0 });
    }

    return(String8{ data, 0, capacity });
}

inline bool String8Append(String8* string, StringView8 view)
{
    if(!string || view.Length == 0)
    {
        return(true);
    }

    if(string->Length + view.Length >= string->Capacity)
    {
        return(false); // Not enough capacity
    }

    for(usize i = 0; i < view.Length; i++)
    {
        string->Data[string->Length + i] = view.Data[i];
    }

    string->Length += view.Length;
    string->Data[string->Length] = '\0';

    return(true);
}

#endif
