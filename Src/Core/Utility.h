#if !defined(SUNBIRD_UTILITY_H)
#define SUNBIRD_UTILITY_H

#define SB_ARRAYCOUNT(array) (sizeof(array) / sizeof((array)[0]))

#define SB_KIB(count) (((count) * 1024ULL))
#define SB_MIB(count) (((count) * 1024ULL * 1024ULL))
#define SB_GIB(count) (((count) * 1024ULL * 1024ULL * 1024ULL))

#endif
