#if !defined(SUNBIRD_MEMORY_UNITS_H)
#define SUNBIRD_MEMORY_UNITS_H

#include "Core/Types.h"

constexpr usize KiB(usize count)
{
    return count * 1024;
}

constexpr usize MiB(usize count)
{
    return count * 1024 * 1024;
}

constexpr usize GiB(usize count)
{
    return count * 1024 * 1024 * 1024;
}

#endif
