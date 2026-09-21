#if !defined(SUNBIRD_STRING_H)
#define SUNBIRD_STRING_H

#include "Types.h"
#include "Memory/StackAllocator.h"

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

constexpr StringView8 SV8(const char8* data)
{
    usize length = 0;
    while(data[length] != '\0')
    {
        length++;
    }

    return StringView8{ data, length };
}

constexpr StringView16 SV16(const char16* data)
{
    usize length = 0;
    while(data[length] != '\0')
    {
        length++;
    }

    return StringView16{ data, length };
}

inline usize SV8ToSV16Length(StringView8 view)
{
    usize length = 0;

    for (usize i = 0; i < view.Length;)
    {
        char8 c = view.Data[i];

        // 1-byte (ASCII).
        if ((c & 0x80) == 0)
        {
            length += 1;
            i += 1;
        }
        // 2-byte.
        else if ((c & 0xE0) == 0xC0)
        {
            if (i + 1 >= view.Length)
            {
                break;
            }

            length += 1;
            i += 2;
        }
        // 3-byte.
        else if ((c & 0xF0) == 0xE0)
        {
            if (i + 2 >= view.Length)
            {
                break;
            }

            length += 1;
            i += 3;
        }
        // 4-byte (surrogate pair).
        else if ((c & 0xF8) == 0xF0)
        {
            if (i + 3 >= view.Length)
            {
                break;
            }

            length += 2;
            i += 4;
        }
        // Invalid byte, skip.
        else
        {
            i += 1;
        }
    }

    return length;
}

inline StringView16 SV8ToSV16(StackAllocator* allocator, StringView8 view)
{
    if(!allocator)
    {
        return StringView16{ nullptr, 0 };
    }

    usize bufferCapacity = SV8ToSV16Length(view) + 1;
    char16* buffer = (char16*)Allocate(allocator, Heap::Upper, bufferCapacity * sizeof(char16), alignof(char16));
    if(!buffer)
    {
        return StringView16{ nullptr, 0 };
    }

    usize bufferIndex = 0;

    for (usize i = 0; i < view.Length;)
    {
        char8 c = view.Data[i];

        // 1-byte (ASCII).
        if ((c & 0x80) == 0)
        {
            buffer[bufferIndex++] = (char16_t)c;
            i += 1;
        }
        // 2-byte.
        else if ((c & 0xE0) == 0xC0)
        {
            if (i + 1 >= view.Length)
            {
                break;
            }

            char16_t codepoint = ((c & 0x1F) << 6) | (view.Data[i+1] & 0x3F);
            buffer[bufferIndex++] = codepoint;
            i += 2;
        }
        // 3-byte.
        else if ((c & 0xF0) == 0xE0)
        {
            if (i + 2 >= view.Length)
            {
                break;
            }

            char16_t codepoint = ((c & 0x0F) << 12) | ((view.Data[i+1] & 0x3F) << 6) | (view.Data[i+2] & 0x3F);
            buffer[bufferIndex++] = codepoint;
            i += 3;
        }
        // 4-byte (surrogate pair).
        else if ((c & 0xF8) == 0xF0)
        {
            if (i + 3 >= view.Length)
            {
                break;
            }

            uint32 codepoint = ((c & 0x07) << 18) | ((view.Data[i+1] & 0x3F) << 12) | ((view.Data[i+2] & 0x3F) << 6) | (view.Data[i+3] & 0x3F);

            codepoint -= 0x10000;
            buffer[bufferIndex++] = 0xD800 + (codepoint >> 10);
            buffer[bufferIndex++] = 0xDC00 + (codepoint & 0x3FF);
            i += 4;
        }
        // Invalid byte, skip.
        else
        {
            i += 1;
        }
    }

    buffer[bufferIndex] = u'\0';
    
    return StringView16{ buffer, bufferIndex };
}

inline String8 String8FromView(StackAllocator* allocator, StringView8 view)
{
    if(!allocator || view.Length == 0)
    {
        return String8{ nullptr, 0, 0 };
    }

    // Allocate with null terminator.
    char8* data = (char8*)Allocate(allocator, Heap::Lower, view.Length + 1, alignof(char8));
    if(!data)
    {
        return String8{ nullptr, 0, 0 };
    }

    // Copy data.
    for(usize i = 0; i < view.Length; i++)
    {
        data[i] = view.Data[i];
    }
    data[view.Length] = '\0';

    return String8{ data, view.Length, view.Length + 1 };
}

inline String8 String8FromLiteral(StackAllocator* allocator, const char8* literal)
{
    return String8FromView(allocator, SV8(literal));
}

inline String8 String8Reserve(StackAllocator* allocator, usize capacity)
{
    if(!allocator || capacity == 0)
    {
        return String8{ nullptr, 0, 0 };
    }

    char8* data = (char8*)Allocate(allocator, Heap::Lower, capacity, alignof(char8));
    if(!data)
    {
        return String8{ nullptr, 0, 0 };
    }

    return String8{ data, 0, capacity };
}

inline bool String8Append(String8* string, StringView8 view)
{
    if(!string || view.Length == 0)
    {
        return true;
    }

    if(string->Length + view.Length >= string->Capacity)
    {
        return false; // Not enough capacity
    }

    for(usize i = 0; i < view.Length; i++)
    {
        string->Data[string->Length + i] = view.Data[i];
    }

    string->Length += view.Length;
    string->Data[string->Length] = '\0';

    return true;
}

#endif
