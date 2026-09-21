#if !defined(SUNBIRD_UTILITY_H)
#define SUNBIRD_UTILITY_H

#define ARRAYCOUNT(array) (sizeof(array) / sizeof((array)[0]))

#define KIB(count) (((count) * 1024ULL))
#define MIB(count) (((count) * 1024ULL * 1024ULL))
#define GIB(count) (((count) * 1024ULL * 1024ULL * 1024ULL))

#endif
