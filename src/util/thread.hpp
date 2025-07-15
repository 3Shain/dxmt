/*
 * This file is part of DXMT, Copyright (c) 2023 Feifan He
 *
 * Derived from a part of DXVK (originally under zlib License),
 * Copyright (c) 2017 Philip Rebohle
 * Copyright (c) 2019 Joshua Ashton
 *
 * See <https://github.com/doitsujin/dxvk/blob/master/LICENSE>
 */

#pragma once

#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <unknwn.h>

#include "util_error.hpp"


#include "./rc/util_rc.hpp"
#include "./rc/util_rc_ptr.hpp"

namespace dxmt {

/**
 * \brief Thread priority
 */
enum class ThreadPriority : int32_t {
  Normal,
  Lowest,
};

#ifdef _WIN32
/**
 * \brief Thread helper class
 *
 * This is needed mostly  for winelib builds. Wine needs to setup each thread
 * that calls Windows APIs. It means that in winelib builds, we can't let
 * standard C++ library create threads and need to use Wine for that instead. We
 * use a thin wrapper around Windows thread functions so that the rest of code
 * just has to use dxmt::thread class instead of std::thread.
 */
class ThreadFn : public RcObject {
  using Proc = std::function<void()>;

public:
  ThreadFn(Proc &&proc) : m_proc(std::move(proc)) {
    // Reference for the thread function
    this->incRef();

    m_handle = ::CreateThread(nullptr, 0x100000, ThreadFn::threadProc, this,
                              STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);

    if (m_handle == nullptr)
      throw MTLD3DError("Failed to create thread");
  }

  ~ThreadFn() {
    if (this->joinable())
      std::terminate();
  }

  void detach() {
    ::CloseHandle(m_handle);
    m_handle = nullptr;
  }

  void join() {
    if (::WaitForSingleObjectEx(m_handle, INFINITE, FALSE) == WAIT_FAILED)
      throw MTLD3DError("Failed to join thread");
    this->detach();
  }

  bool joinable() const { return m_handle != nullptr; }

  void set_priority(ThreadPriority priority) {
    int32_t value;
    switch (priority) {
    default:
    case ThreadPriority::Normal:
      value = THREAD_PRIORITY_NORMAL;
      break;
    case ThreadPriority::Lowest:
      value = THREAD_PRIORITY_LOWEST;
      break;
    }
    ::SetThreadPriority(m_handle, int32_t(value));
  }

private:
  Proc m_proc;
  HANDLE m_handle;

  static DWORD WINAPI threadProc(void *arg) {
    auto thread = reinterpret_cast<ThreadFn *>(arg);
    thread->m_proc();
    thread->decRef();
    return 0;
  }
};

/**
 * \brief RAII thread wrapper
 *
 * Wrapper for \c ThreadFn that can be used
 * as a drop-in replacement for \c std::thread.
 */
class thread {

public:
  thread() {}

  explicit thread(std::function<void()> &&func)
      : m_thread(new ThreadFn(std::move(func))) {}

  thread(thread &&other) : m_thread(std::move(other.m_thread)) {}

  thread &operator=(thread &&other) {
    m_thread = std::move(other.m_thread);
    return *this;
  }

  void detach() { m_thread->detach(); }

  void join() { m_thread->join(); }

  bool joinable() const { return m_thread != nullptr && m_thread->joinable(); }

  void set_priority(ThreadPriority priority) {
    m_thread->set_priority(priority);
  }

  static uint32_t hardware_concurrency() {
    SYSTEM_INFO info = {};
    ::GetSystemInfo(&info);
    return info.dwNumberOfProcessors;
  }

private:
  Rc<ThreadFn> m_thread;
};

namespace this_thread {
inline void yield() { SwitchToThread(); }

inline uint32_t get_id() { return uint32_t(GetCurrentThreadId()); }

bool isInModuleDetachment();
} // namespace this_thread

/**
 * \brief SRW-based mutex implementation
 *
 * Drop-in replacement for \c std::mutex that uses Win32
 * SRW locks, which are implemented with \c futex in wine.
 */
class mutex {

public:
  using native_handle_type = PSRWLOCK;

  mutex() {}

  mutex(const mutex &) = delete;
  mutex &operator=(const mutex &) = delete;

  void lock() { AcquireSRWLockExclusive(&m_lock); }

  void unlock() { ReleaseSRWLockExclusive(&m_lock); }

  bool try_lock() { return TryAcquireSRWLockExclusive(&m_lock); }

  native_handle_type native_handle() { return &m_lock; }

private:
  SRWLOCK m_lock = SRWLOCK_INIT;
};

/**
 * \brief Recursive mutex implementation
 *
 * Drop-in replacement for \c std::recursive_mutex that
 * uses Win32 critical sections.
 */
class recursive_mutex {

public:
  using native_handle_type = PCRITICAL_SECTION;

  recursive_mutex() { InitializeCriticalSection(&m_lock); }

  ~recursive_mutex() { DeleteCriticalSection(&m_lock); }

  recursive_mutex(const recursive_mutex &) = delete;
  recursive_mutex &operator=(const recursive_mutex &) = delete;

  void lock() { EnterCriticalSection(&m_lock); }

  void unlock() { LeaveCriticalSection(&m_lock); }

  bool try_lock() { return TryEnterCriticalSection(&m_lock); }

  native_handle_type native_handle() { return &m_lock; }

private:
  CRITICAL_SECTION m_lock;
};

/**
 * \brief SRW-based condition variable implementation
 *
 * Drop-in replacement for \c std::condition_variable that
 * uses Win32 condition variables on SRW locks.
 */
class condition_variable {

public:
  using native_handle_type = PCONDITION_VARIABLE;

  condition_variable() { InitializeConditionVariable(&m_cond); }

  condition_variable(condition_variable &) = delete;

  condition_variable &operator=(condition_variable &) = delete;

  void notify_one() { WakeConditionVariable(&m_cond); }

  void notify_all() { WakeAllConditionVariable(&m_cond); }

  void wait(std::unique_lock<dxmt::mutex> &lock) {
    auto srw = lock.mutex()->native_handle();
    SleepConditionVariableSRW(&m_cond, srw, INFINITE, 0);
  }

  template <typename Predicate>
  void wait(std::unique_lock<dxmt::mutex> &lock, Predicate pred) {
    while (!pred())
      wait(lock);
  }

  template <typename Clock, typename Duration>
  std::cv_status
  wait_until(std::unique_lock<dxmt::mutex> &lock,
             const std::chrono::time_point<Clock, Duration> &time) {
    auto now = Clock::now();

    return (now < time) ? wait_for(lock, now - time) : std::cv_status::timeout;
  }

  template <typename Clock, typename Duration, typename Predicate>
  bool wait_until(std::unique_lock<dxmt::mutex> &lock,
                  const std::chrono::time_point<Clock, Duration> &time,
                  Predicate pred) {
    if (pred())
      return true;

    auto now = Clock::now();
    return now < time && wait_for(lock, now - time, pred);
  }

  template <typename Rep, typename Period>
  std::cv_status wait_for(std::unique_lock<dxmt::mutex> &lock,
                          const std::chrono::duration<Rep, Period> &timeout) {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(timeout);
    auto srw = lock.mutex()->native_handle();

    return SleepConditionVariableSRW(&m_cond, srw, ms.count(), 0)
               ? std::cv_status::no_timeout
               : std::cv_status::timeout;
  }

  template <typename Rep, typename Period, typename Predicate>
  bool wait_for(std::unique_lock<dxmt::mutex> &lock,
                const std::chrono::duration<Rep, Period> &timeout,
                Predicate pred) {
    bool result = pred();

    if (!result && wait_for(lock, timeout) == std::cv_status::no_timeout)
      result = pred();

    return result;
  }

  native_handle_type native_handle() { return &m_cond; }

private:
  CONDITION_VARIABLE m_cond;
};

#else
class thread : public std::thread {
public:
  using std::thread::thread;

  void set_priority(ThreadPriority priority) {
    ::sched_param param = {};
    int32_t policy;
    switch (priority) {
    default:
    case ThreadPriority::Normal:
      policy = SCHED_OTHER;
      break;
    case ThreadPriority::Lowest:
#ifdef DXMT_NATIVE
      policy = SCHED_FIFO; /* No SCHED_IDLE on macOS */
#else
      policy = SCHED_IDLE;
#endif
      break;
    }
    ::pthread_setschedparam(this->native_handle(), policy, &param);
  }
};

using mutex = std::mutex;
using recursive_mutex = std::recursive_mutex;
using condition_variable = std::condition_variable;

namespace this_thread {
inline void yield() { std::this_thread::yield(); }

uint32_t get_id();

inline bool isInModuleDetachment() { return false; }
} // namespace this_thread
#endif

struct null_mutex {
  void lock() {}
  void unlock() noexcept {}
  bool try_lock() { return true; }
};

} // namespace dxmt
