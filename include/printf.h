#ifndef INC_PRINTF_H
#define INC_PRINTF_H

#include "defs.h"
#include "config.h"

#define assert(x)                                                   \
({                                                                  \
    if (!(x)) {                                                     \
        printf("%s:%d: assertion failed.\n", __FILE__, __LINE__);   \
        panic("");                                                  \
    }                                                               \
})

#define asserts(x, ...)                                             \
({                                                                  \
    if (!(x)) {                                                     \
        printf("%s:%d: assertion failed.\n", __FILE__, __LINE__);   \
        panic("");                                                  \
    }                                                               \
})

#define LOG1(level, ...)                        \
({                                              \
    printf("[%s] %s: ", level, __func__);    \
    printf(__VA_ARGS__);                      \
    printf("\n");                             \
})

#ifdef LOG_ERROR
#define error(...)  LOG1("ERROR", __VA_ARGS__);
#define warn(...)
#define info(...)
#define debug(...)
#define trace(...)

#elif defined(LOG_WARN)
#define error(...)  LOG1("ERROR", __VA_ARGS__);
#define warn(...)   LOG1("WARN ", __VA_ARGS__);
#define info(...)
#define debug(...)
#define trace(...)

#elif defined(LOG_INFO)
#define error(...)  LOG1("ERROR", __VA_ARGS__);
#define warn(...)   LOG1("WARN ", __VA_ARGS__);
#define info(...)   LOG1("INFO ", __VA_ARGS__);
#define debug(...)
#define trace(...)

#elif defined(LOG_DEBUG)
#define error(...)  LOG1("ERROR", __VA_ARGS__);
#define warn(...)   LOG1("WARN ", __VA_ARGS__);
#define info(...)   LOG1("INFO ", __VA_ARGS__);
#define debug(...)  LOG1("DEBUG", __VA_ARGS__);
#define trace(...)

#elif defined(LOG_TRACE)
#define error(...)  LOG1("ERROR", __VA_ARGS__);
#define warn(...)   LOG1("WARN ", __VA_ARGS__);
#define info(...)   LOG1("INFO ", __VA_ARGS__);
#define debug(...)  LOG1("DEBUG", __VA_ARGS__);
#define trace(...)  LOG1("TRACE", __VA_ARGS__);

#else
/* Default to none. */
#define error(...)
#define warn(...)
#define info(...)
#define debug(...)
#define trace(...)

#endif

#endif
