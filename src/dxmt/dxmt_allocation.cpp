/*
 * Copyright 2026 Feifan He for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "dxmt_allocation.hpp"
#include "util_likely.hpp"
#include <cassert>
#include <cstdio>
#include <iterator>
#include <new>
#include <windows.h>

namespace dxmt {

void *g_d3d12_device_addr = nullptr;
size_t g_d3d12_device_size = 0;

} // namespace dxmt

void operator delete(void *ptr) noexcept {
  if (dxmt::g_d3d12_device_addr && ptr) {
    uintptr_t p = (uintptr_t)ptr;
    uintptr_t d = (uintptr_t)dxmt::g_d3d12_device_addr;
    if (p >= d && p < d + dxmt::g_d3d12_device_size) {
      DWORD tid = GetCurrentThreadId();
      FILE *f = fopen("Z:\\tmp\\dxmt_dxgi_trace.log", "a");
      if (f) {
        fprintf(f, "!!! GLOBAL operator delete ON DEVICE! ptr=%p device=%p size=%zu tid=%lu\n",
          ptr, dxmt::g_d3d12_device_addr, dxmt::g_d3d12_device_size, (unsigned long)tid);
        void *buf[16];
        ULONG n = RtlCaptureStackBackTrace(1, 16, buf, nullptr);
        fprintf(f, "  stack[%lu]=", (unsigned long)n);
        for (ULONG i = 0; i < n; i++) fprintf(f, "%p ", buf[i]);
        fprintf(f, "\n");
        fclose(f);
      }
      return;
    }
  }
  free(ptr);
}

void operator delete(void *ptr, std::align_val_t) noexcept {
  free(ptr);
}

namespace dxmt {

void
Allocation::incRef() {
  refcount_.fetch_add(1u, std::memory_order_acquire);
};

void
Allocation::decRef() {
  if (g_d3d12_device_addr != nullptr &&
      (uintptr_t)this >= (uintptr_t)g_d3d12_device_addr &&
      (uintptr_t)this < (uintptr_t)g_d3d12_device_addr + g_d3d12_device_size) {
    FILE *f = fopen("Z:\\tmp\\dxmt_dxgi_trace.log", "a");
    if (f) {
      fprintf(f, "!!! decRef BLOCKED on DEVICE this=%p vtable=%p refcount=%u tid=%lu\n",
        (void*)this, *(void**)this, refcount_.load(), (unsigned long)GetCurrentThreadId());
      void *buf[16];
      ULONG n = RtlCaptureStackBackTrace(1, 16, buf, nullptr);
      fprintf(f, "  stack[%lu]=", (unsigned long)n);
      for (ULONG i = 0; i < n; i++) fprintf(f, "%p ", buf[i]);
      fprintf(f, "\n");
      fclose(f);
    }
    return;
  }
  uint32_t prev = refcount_.fetch_sub(1u, std::memory_order_release);
  if (prev == 1u) {
    this->destroy();
  }
};

AllocationRefTracking::AllocationRefTracking() {
  chunk_placed.next_chunk = nullptr;
  chunk_placed.size = 0;
  chunk_placed.capacity = std::size(chunk_placed.allocations);
  chunk_last = reinterpret_cast<RefAddChunk<> *>(&chunk_placed);
}

bool
AllocationRefTracking::track(Allocation *allocation) {
  if (unlikely(chunk_last->capacity <= chunk_last->size))
    return false;
  dxmt::Allocation **list = std::launder(chunk_last->allocations);
  allocation->incRef();
  list[chunk_last->size++] = allocation;
  return true;
};

void
AllocationRefTracking::addStorage(void *ptr, size_t length) {
  RefAddChunk<> *new_chunk = std::launder(reinterpret_cast<RefAddChunk<> *>(ptr));
  new_chunk->next_chunk = nullptr;
  new_chunk->size = 0;
  assert(length > sizeof(RefAddChunk<>) && "No enough space for allocation reference tracking");
  new_chunk->capacity = (length - offsetof(RefAddChunk<>, allocations)) / sizeof(Allocation *);
  chunk_last->next_chunk = new_chunk;
  chunk_last = new_chunk;
}

void
AllocationRefTracking::clear() {
  FILE *f = fopen("Z:\\tmp\\dxmt_dxgi_trace.log", "a");
  RefAddChunk<> *chunk = reinterpret_cast<RefAddChunk<> *>(&chunk_placed);
  while (chunk) {
    dxmt::Allocation **list = std::launder(chunk->allocations);
    for (unsigned i = 0; i < chunk->size; i++) {
      if (f) fprintf(f, "RefTracking::clear decRef on alloc[%u]=%p vtable=%p\n",
        i, (void*)list[i], list[i] ? *(void**)list[i] : nullptr);
      list[i]->decRef();
    }
    chunk->size = 0;
    chunk = chunk->next_chunk;
  }
  if (f) { fprintf(f, "RefTracking::clear done\n"); fclose(f); }
  chunk_last = reinterpret_cast<RefAddChunk<> *>(&chunk_placed);
  chunk_placed.next_chunk = nullptr;
};

} // namespace dxmt