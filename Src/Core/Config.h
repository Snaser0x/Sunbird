#if !defined(SUNBIRD_CONFIG_H)
#define SUNBIRD_CONFIG_H

#define SB_PLATFORM_WINDOWS 0

#if defined(_WIN32) || defined(_WIN64)
    #undef SB_PLATFORM_WINDOWS
    #define SB_PLATFORM_WINDOWS 1
#endif

#endif
