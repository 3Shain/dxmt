
#pragma once

#include "objc-wrapper/abi.h"

extern "C" {
#define _JBLEN ((14 + 8 + 2) * 4)

// typedef int jmp_buf[_JBLEN];
typedef int sigjmp_buf[_JBLEN + 1];

extern SYSV_ABI int unix_setjmp(sigjmp_buf);
extern SYSV_ABI void unix_longjmp(sigjmp_buf, int);

typedef struct {
  void *lock;
  void* rsp;
  int done;
  sigjmp_buf ret;
  sigjmp_buf cont;
} _context;

typedef _context *pcontext;

}

extern "C" __attribute__((dllexport)) void enqueue(pcontext ctx, void* stack) noexcept;

extern "C" __attribute__((dllexport)) void resume(pcontext ctx) noexcept;