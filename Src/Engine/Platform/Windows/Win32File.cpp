#include "Engine/Platform/File.h"

#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

static bool Win32FileIsSeparator(char16 c)
{
    return(c == u'\\' || c == u'/');
}

// NOTE(saeb): Relative paths resolve against the exe's folder, not the working directory; shortcuts, launchers and debuggers can all start the game from anywhere. Writes a null-terminated UTF-16 path on the Upper heap to *fullPath.
static FileReadResult Win32FileBuildPath(StackAllocator* allocator, StringView8 path, const char16** fullPath)
{
    *fullPath = nullptr;

    // NOTE(saeb): An empty path would resolve to the exe's folder itself, which CreateFileW rejects as access denied.
    if(path.Length == 0)
    {
        return(FileReadResult::InvalidPath);
    }

    StringView16 widePath = SV8ToSV16(allocator, path);
    if(!widePath.Data)
    {
        return(FileReadResult::OutOfMemory);
    }

    const char16* wide = widePath.Data;
    usize length = widePath.Length;

    // NOTE(saeb): Only "C:\..." and "\\..." (UNC, \\?\) are truly absolute and pass through unchanged, so tools can reuse this. "C:something" and "\something" still depend on the working directory's drive or folder, so reject them rather than guess.
    bool hasDrive = (length >= 2 && wide[1] == u':');
    if(hasDrive)
    {
        if(length >= 3 && Win32FileIsSeparator(wide[2]))
        {
            *fullPath = wide;
            return(FileReadResult::Ok);
        }

        return(FileReadResult::InvalidPath); // Drive-relative
    }

    if(length >= 1 && Win32FileIsSeparator(wide[0]))
    {
        if(length >= 2 && Win32FileIsSeparator(wide[1]))
        {
            *fullPath = wide;
            return(FileReadResult::Ok);
        }

        return(FileReadResult::InvalidPath); // Root-relative
    }

    // NOTE(saeb): MAX_PATH is also CreateFileW's own limit without a long-path manifest, so a longer exe path couldn't open files anyway.
    char16* exePath = (char16*)Allocate(allocator, Heap::Upper, MAX_PATH * sizeof(char16), alignof(char16));
    if(!exePath)
    {
        return(FileReadResult::OutOfMemory);
    }

    DWORD exePathLength = GetModuleFileNameW(nullptr, (LPWSTR)exePath, MAX_PATH);
    if(exePathLength == 0 || exePathLength >= MAX_PATH)
    {
        return(FileReadResult::InvalidPath); // Failed, or truncated
    }

    // NOTE(saeb): Keep everything up to and including the last separator: "C:\Dev\Sunbird\Sunbird.exe" -> "C:\Dev\Sunbird\".
    usize directoryLength = exePathLength;
    while(directoryLength > 0 && exePath[directoryLength - 1] != u'\\')
    {
        --directoryLength;
    }

    char16* result = (char16*)Allocate(allocator, Heap::Upper, (directoryLength + length + 1) * sizeof(char16), alignof(char16));
    if(!result)
    {
        return(FileReadResult::OutOfMemory);
    }

    for(usize index = 0; index < directoryLength; ++index)
    {
        result[index] = exePath[index];
    }

    for(usize index = 0; index < length; ++index)
    {
        result[directoryLength + index] = wide[index];
    }

    result[directoryLength + length] = u'\0';

    *fullPath = result;
    return(FileReadResult::Ok);
}

FileReadResult FileRead(StackAllocator* allocator, Heap heap, StringView8 path, FileContents* contents)
{
    contents->Data = nullptr;
    contents->Size = 0;

    // NOTE(saeb): The path is only needed to open the file; release it before allocating the data so the two never interleave on the Upper heap.
    Frame pathScratch = GetFrame(allocator, Heap::Upper);

    const char16* fullPath = nullptr;
    FileReadResult pathResult = Win32FileBuildPath(allocator, path, &fullPath);

    HANDLE fileHandle = INVALID_HANDLE_VALUE;
    DWORD openError = 0;
    if(pathResult == FileReadResult::Ok)
    {
        // NOTE(saeb): Share everything so the open doesn't fail while a tool or editor still has the file open. The file can then change mid-read; a shrink is caught by the short-read check below, but an in-place rewrite of the same size isn't.
        fileHandle = CreateFileW((LPCWSTR)fullPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        openError = GetLastError(); // Before ReleaseFrame, so nothing in between can overwrite it
    }

    ReleaseFrame(allocator, pathScratch);

    if(pathResult != FileReadResult::Ok)
    {
        return(pathResult);
    }

    if(fileHandle == INVALID_HANDLE_VALUE)
    {
        switch(openError)
        {
            case ERROR_FILE_NOT_FOUND:
            case ERROR_PATH_NOT_FOUND:
            {
                return(FileReadResult::NotFound);
            }

            case ERROR_INVALID_NAME:
            case ERROR_FILENAME_EXCED_RANGE:
            {
                return(FileReadResult::InvalidPath);
            }

            case ERROR_ACCESS_DENIED:
            case ERROR_SHARING_VIOLATION:
            {
                return(FileReadResult::AccessDenied);
            }

            default:
            {
                return(FileReadResult::ReadFailed);
            }
        }
    }

    LARGE_INTEGER fileSize;
    if(!GetFileSizeEx(fileHandle, &fileSize))
    {
        CloseHandle(fileHandle);
        return(FileReadResult::ReadFailed);
    }

    usize size = (usize)fileSize.QuadPart;

    // NOTE(saeb): +1 for the zero terminator, which also means an empty file still gets a valid (non-null) buffer. 16-byte alignment so cooked data can be read as structs in place.
    Frame dataFrame = GetFrame(allocator, heap);
    uint8* data = (uint8*)Allocate(allocator, heap, size + 1, 16);
    if(!data)
    {
        CloseHandle(fileHandle);
        return(FileReadResult::OutOfMemory);
    }

    // NOTE(saeb): ReadFile takes a 32-bit count, so read in chunks; a short read (the file shrank while reading) is a failure, not a partial success.
    usize totalRead = 0;
    while(totalRead < size)
    {
        usize remaining = size - totalRead;
        DWORD chunkSize = (remaining > 0x40000000) ? 0x40000000 : (DWORD)remaining;
        DWORD bytesRead = 0;

        if(!ReadFile(fileHandle, data + totalRead, chunkSize, &bytesRead, nullptr) || bytesRead == 0)
        {
            // NOTE(saeb): Give the allocation back; a failed read leaves the allocator exactly as it was.
            ReleaseFrame(allocator, dataFrame);
            CloseHandle(fileHandle);
            return(FileReadResult::ReadFailed);
        }

        totalRead += bytesRead;
    }

    CloseHandle(fileHandle);

    data[size] = 0;

    contents->Data = data;
    contents->Size = size;

    return(FileReadResult::Ok);
}
