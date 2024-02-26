
#pragma once
/*
 * This header contains an implementation of Objective-C Block ABI in C++
 * Reference:
 * https://clang.llvm.org/docs/Block-ABI-Apple.html#runtimehelperfunctions
 */

// BUG PRONE!

#include <functional>
#include "abi.h"
#include "dispatch-queue.hpp"

#include "intrin.h"

extern void *_NSConcreteGlobalBlock;
extern void *_NSConcreteStackBlock;

typedef struct _Block *Block; // opaque pointer. TODO: remove it

#ifdef NO_BLOCK_IMPL

template <typename Sig> struct __Block {
  __Block(const std::function<Sig> &fnCopy) {}
  __Block(std::function<Sig> &&fnMove) {}
};
#else

template <typename Sig> struct __Block;
template <typename R, typename... Arg> struct __Block<R(Arg...)> {
public:
  struct __block_descritor {
    unsigned long long reserved; // NULL
    unsigned long long size;     // sizeof(struct __block_literal_type)
                                 // optional helper functions
    void *copy_helper;           // IFF (1<<25)
    void *dispose_helper;        // IFF (1<<25)
    // required ABI.2010.3.16
    // const char *signature; // IFF (1<<30)
  };

  void *isa;
  int flags;
  int reserved;
  void *invoke;
  struct __block_descritor *descriptor;
  std::function<R(Arg...)> fn;
  void *a;
  void *b;
  void *c;

  static __attribute__((sysv_abi)) R
  invoke_impl_r(struct __Block<R(Arg...)> *dst, Arg... arg) {
    // if (__readgsqword(0x30) % 0x7ff00000) {
    //   new (&dst->fn) std::function<R(Arg...)>(src->fn);
    //   return;
    // }
    _context ctx = {0, 0, 0};
    std::array<void *, 0x200> stack;
    if (unix_setjmp(ctx.ret)) {
      {
        if constexpr (std::is_same<R, void>::value) {
          dst->fn(std::forward<Arg>(arg)...);
        } else {
          R ret = dst->fn(std::forward<Arg>(arg)...);
          return ret;
        }
      }
      resume(&ctx);
    } else {
      enqueue(&ctx, &stack[stack.size()]);
    }
  }

  static __attribute__((sysv_abi)) void
  copy_impl_r(struct __Block<R(Arg...)> *dst,
              struct __Block<R(Arg...)> *src) noexcept {
    if (__readgsqword(0x30) % 0x7ff00000) {
      new (&dst->fn) std::function<R(Arg...)>(src->fn);
      return;
    }
    _context ctx = {0, 0, 0};
    std::array<void *, 0x200> stack;
    if (unix_setjmp(ctx.ret)) {
      {
        new (&dst->fn) std::function<R(Arg...)>(src->fn);
      }
      resume(&ctx);
    } else {
      enqueue(&ctx, &stack[stack.size()]);
    }
  };

  static __attribute__((sysv_abi)) void
  release_impl_r(struct __Block<R(Arg...)> *dst) noexcept {
    _context ctx = {0, 0, 0};
    std::array<void *, 0x400> stack;
    if (unix_setjmp(ctx.ret)) {
      {
        dst->fn.~function<R(Arg...)>();
      }
      resume(&ctx);
    } else {
      enqueue(&ctx, &stack[0x400]);
    }
  }

  inline static __block_descritor static_descirptor = {
      .reserved = 0,
      .size = sizeof(struct __Block<R(Arg...)>),
      .copy_helper = (void *)copy_impl_r,
      .dispose_helper = (void *)release_impl_r};

  __Block(const std::function<R(Arg...)> &fnCopy)
      : isa(_NSConcreteStackBlock), flags((1 << 25) | (1 << 26)), reserved(0),
        invoke((void *)invoke_impl_r), descriptor(&static_descirptor),
        fn(fnCopy) {}

  __Block(std::function<R(Arg...)> &&fnMove)
      : isa(_NSConcreteStackBlock), flags((1 << 25) | (1 << 26)), reserved(0),
        invoke((void *)invoke_impl_r), descriptor(&static_descirptor),
        fn(std::move(fnMove)) {}
};

#endif