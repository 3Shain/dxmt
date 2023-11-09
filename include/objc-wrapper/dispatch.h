#include "abi.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dispatch_semaphore *dispatch_semaphore_t;
typedef struct dispatch_object *dispatch_object_t;
typedef uint64_t dispatch_time_t;

#define DISPATCH_TIME_NOW (0ull)
#define DISPATCH_TIME_FOREVER (~0ull)

extern dispatch_semaphore_t SYSV_ABI dispatch_semaphore_create(intptr_t value);

extern intptr_t SYSV_ABI dispatch_semaphore_wait(dispatch_semaphore_t dsema,
                                                 dispatch_time_t timeout);

extern intptr_t SYSV_ABI dispatch_semaphore_signal(dispatch_semaphore_t dsema);

extern void dispatch_release(dispatch_object_t object);
#ifdef __cplusplus
}
#endif