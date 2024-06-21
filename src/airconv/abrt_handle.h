#pragma once

#ifdef __MACH__
#include <setjmp.h>
#include <signal.h>

template <int N = SIGABRT> struct signal_guard {
private:
  struct sigaction new_sa;
  struct sigaction old_sa;

public:
  signal_guard(void (*f)(int)) {
    sigemptyset(&new_sa.sa_mask);
    new_sa.sa_handler = f;
    sigaction(N, &new_sa, &old_sa);
  }

  ~signal_guard() { sigaction(N, &old_sa, 0); }
};

#define ABRT_HANDLE_INIT                                                       \
  thread_local jmp_buf abort_buffer;                                           \
  auto abort_handler = [](int) { longjmp(abort_buffer, 1); };

#define ABRT_HANDLE_RETURN(x)                                                  \
  signal_guard<> g(abort_handler);                                             \
  if (setjmp(abort_buffer)) {                                                  \
    return x;                                                                  \
  }

#else

#define ABRT_HANDLE_INIT ;

#define ABRT_HANDLE_RETURN(x) ;
#endif