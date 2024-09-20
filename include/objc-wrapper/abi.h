
#pragma once

#include <stddef.h>
#define SYSV_ABI __attribute__((sysv_abi))

#ifdef __cplusplus
extern "C" {
#endif

int SYSV_ABI unix_printf(const char *__format, ...);
typedef unsigned long __pthread_key_t;
void *SYSV_ABI __pthread_getspecific(__pthread_key_t);
int SYSV_ABI __pthread_setspecific(__pthread_key_t, const void *);
void *SYSV_ABI unix_malloc(size_t __size);
void SYSV_ABI unix_free(void *);
void *SYSV_ABI unix_dlsym(void* handle, const char* symbol);
int SYSV_ABI __pthread_set_qos_class_self_np(int __qos_class, int __relative_priority);

#define __QOS_CLASS_USER_INTERACTIVE 0x21
#define __QOS_CLASS_USER_INITIATED 0x19

#ifdef __cplusplus
}
#endif