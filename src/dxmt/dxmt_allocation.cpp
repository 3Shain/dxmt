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
#include <iterator>
#include <new>

namespace dxmt {

void
Allocation::incRef() {
  refcount_.fetch_add(1u, std::memory_order_acquire);
};

void
Allocation::decRef() {
  if (refcount_.fetch_sub(1u, std::memory_order_release) == 1u)
    this->free();
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
  RefAddChunk<> *chunk = reinterpret_cast<RefAddChunk<> *>(&chunk_placed);
  while (chunk) {
    dxmt::Allocation **list = std::launder(chunk->allocations);
    for (unsigned i = 0; i < chunk->size; i++)
      list[i]->decRef();
    chunk->size = 0;
    chunk = chunk->next_chunk;
  }
  // reset state
  chunk_last = reinterpret_cast<RefAddChunk<> *>(&chunk_placed);
  chunk_placed.next_chunk = nullptr;
};

} // namespace dxmt