#if !defined(SUNBIRD_CONFIG_H)
#define SUNBIRD_CONFIG_H

#define SB_PLATFORM_WINDOWS 0
#define SB_PLATFORM_LINUX 0
#define SB_PLATFORM_APPLE 0

#if defined(_WIN32) || defined(_WIN64)
    #undef SB_PLATFORM_WINDOWS
    #define SB_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #undef SB_PLATFORM_LINUX
    #define SB_PLATFORM_LINUX 1
#elif defined(__APPLE__)
    #undef SB_PLATFORM_APPLE
    #define SB_PLATFORM_APPLE 1
#endif

#endif
