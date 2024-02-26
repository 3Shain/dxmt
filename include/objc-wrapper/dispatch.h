#include "abi.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dispatch_object *dispatch_object_t;
typedef dispatch_object_t dispatch_semaphore_t;
typedef uint64_t dispatch_time_t;
typedef struct dispatch_queue *dispatch_queue_t;
typedef dispatch_object_t dispatch_data_t;

#define DISPATCH_TIME_NOW (0ull)
#define DISPATCH_TIME_FOREVER (~0ull)

extern dispatch_semaphore_t SYSV_ABI dispatch_semaphore_create(intptr_t value);

extern intptr_t SYSV_ABI dispatch_semaphore_wait(dispatch_semaphore_t dsema,
                                                 dispatch_time_t timeout);

extern intptr_t SYSV_ABI dispatch_semaphore_signal(dispatch_semaphore_t dsema);

extern void SYSV_ABI dispatch_release(dispatch_object_t object);

extern dispatch_queue_t SYSV_ABI dispatch_get_main_queue(void);

extern dispatch_data_t SYSV_ABI
dispatch_data_create(const void *buffer, size_t size, dispatch_queue_t queue,
                     nullptr_t destructor /* don't use block*/);

#ifdef __cplusplus
}
#endif