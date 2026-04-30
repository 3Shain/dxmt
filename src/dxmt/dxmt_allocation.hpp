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

#pragma once

#include <atomic>
#include <cstdint>

namespace dxmt {

class Allocation {
public:
  void incRef();
  void decRef();

  bool
  checkRetained(uint64_t seq_id) {
    // FIXME: is a compare-and-swap necessary?
    if (seq_id == last_retained_seq_id)
      return true;
    last_retained_seq_id = seq_id;
    return false;
  }

private:
  virtual void free() = 0;

  std::atomic<uint32_t> refcount_ = {0u};
  uint64_t last_retained_seq_id = 0;
};

class AllocationRefTracking {
public:
  AllocationRefTracking();

  bool track(Allocation *allocation);

  void addStorage(void *ptr, size_t length);

  void clear();

private:
  template <size_t Size = 1> struct RefAddChunk {
    RefAddChunk *next_chunk;
    size_t capacity;
    size_t size;
    Allocation *allocations[Size];
  };
  RefAddChunk<29> chunk_placed;
  RefAddChunk<> *chunk_last = nullptr;
};

} // namespace dxmt
