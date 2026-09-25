#if !defined(SUNBIRD_FILE_H)
#define SUNBIRD_FILE_H

#include "Core/Types.h"
#include "Core/String.h"
#include "Core/StackAllocator.h"

struct FileContents
{
    uint8* Data; // Followed by one zero byte (not counted in Size), so text can be read as a C string
    usize Size;
};

enum class FileReadResult : uint8
{
    Ok,
    InvalidPath, // Empty, drive-relative, root-relative, illegal characters, too long, or the exe path couldn't be resolved
    NotFound, // File or directory doesn't exist
    AccessDenied, // No permission, a sharing violation, or the path is a directory
    OutOfMemory, // Not enough room in the allocator for the path or the data
    ReadFailed // Open, size query or read failed for any other reason, including the file shrinking mid-read
};

// NOTE(saeb): Reads the whole file into heap, 16-byte aligned. The caller owns the data and frees it by releasing a frame taken on heap before the call. Relative paths resolve against the exe's folder. Always borrows temporary space on the Upper heap for the path (freed before returning), even when heap is Lower. On failure, zeroes contents and leaves the allocator exactly as it was.
FileReadResult FileRead(StackAllocator* allocator, Heap heap, StringView8 path, FileContents* contents);

#endif
