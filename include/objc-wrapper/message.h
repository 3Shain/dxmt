#pragma once

#include "abi.h"
#ifdef __cplusplus
extern "C" {
#endif

extern void SYSV_ABI objc_msgSend_fpret(void /* id self, SEL op, ... */);

extern void SYSV_ABI objc_msgSend_stret(void /* id self, SEL op, ... */);

extern void SYSV_ABI objc_msgSend(void /* id self, SEL op, ... */);

#ifdef __cplusplus
}
#endif