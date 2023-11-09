#include "objc-wrapper/dispatch-queue.hpp"
#include "Foundation/NSLock.hpp"
#include <minwindef.h>
#include <processthreadsapi.h>
#include <winnt.h>

#define MAX_QUEUE_SIZE 128
class SimpleQueue {
  pcontext array[MAX_QUEUE_SIZE];
  int front = 0;
  int rear = 0;
  int count = 0;

public:
  void push(const pcontext &task) noexcept {
    if (count == MAX_QUEUE_SIZE) {
      // std::cerr << "Queue overflow" << std::endl;
      return;
    }

    array[rear] = task;
    rear = (rear + 1) % MAX_QUEUE_SIZE;
    count++;
  }

  pcontext frontThenPop() noexcept {
    // if (count == 0) {
    //   // Return a no-op lambda for safety.

    // }

    pcontext task = (array[front]);
    front = (front + 1) % MAX_QUEUE_SIZE;
    count--;

    return task;
  }

  bool empty() noexcept { return count == 0; }
};

SimpleQueue jobQueue;
bool shouldExit = false;

NS::Condition *queueCondition;
DWORD Worker(LPVOID lpParameter) {
  while (true) {
    pcontext job;

    queueCondition->lock();

    while (jobQueue.empty()) {
      queueCondition->wait();
    }

    job = jobQueue.frontThenPop();
    queueCondition->unlock();

    if (unix_setjmp(job->cont)) {
      auto cond = (NS::Condition *)job->lock;
      cond->retain();
      cond->lock();
      job->done = 1;
      cond->signal();
      cond->unlock();
      cond->release();
    } else {
      unix_longjmp(job->ret, 1);
    }
  }
  return 0;
}

extern "C" {

void enqueue_real(pcontext ctx) noexcept {

  auto cond = NS::Condition::alloc()->init();

  ctx->lock = cond;
  queueCondition->lock();
  jobQueue.push(ctx);
  queueCondition->signal();
  queueCondition->unlock();

  cond->lock();
  while (ctx->done == 0) {
    cond->wait();
  }
  cond->unlock();
  cond->release();
}

void enqueue(pcontext ctx, void *stack) noexcept {
  asm volatile("movq %%rsp, %%r12;" // Store current rsp to r12
               "movq %%rdx, %%rsp;" // Set rsp to the second parameter (rdx)
               "call enqueue_real;" // Call enqueue with param1 (rcx is already
                                    // the first parameter according to ABI)
               "movq %%r12, %%rsp;" // Restore rsp from r12
               :
               :
               : "rcx", "rdx", "r12", "memory");
}

void resume(pcontext ctx) noexcept { unix_longjmp(ctx->cont, 1); }

BOOL InitializeDispatchQueue(UINT threadPoolSize) {
  HANDLE workerThread = NULL;
  workerThread = CreateThread(NULL,   // default security attributes
                              0,      // default stack size
                              Worker, // thread function name
                              NULL,   // argument to thread function
                              0,      // use default creation flags
                              NULL);  // returns the thread identifier

  if (workerThread == NULL) {
    return FALSE;
    // Handle the error as needed
  }
  queueCondition = NS::Condition::alloc()->init();
  return TRUE;
};
}