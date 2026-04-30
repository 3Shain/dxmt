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

#include "dxmt_allocation.hpp"
#include "dxmt_deptrack.hpp"
#include "dxmt_buffer.hpp"
#include "rc/util_rc_ptr.hpp"
#include "util_svector.hpp"
#include "thread.hpp"
#include <cstdint>

namespace dxmt {

constexpr auto kCounterSetSize = 64;
constexpr auto kCounterAlignAs = 128; // Apple GPU cache line size

class CounterSet;

class CounterAllocation final : public Allocation {
public:
  BufferAllocation *buffer();

  constexpr uint32_t
  offset() {
    return index_ * kCounterAlignAs;
  }

private:
  void free();

  friend class CounterSet;
  CounterSet *set_;
  uint32_t index_;

  void
  init(CounterSet *set, uint32_t index) {
    set_ = set;
    index_ = index;
  }
};

class CounterSet {
public:
  CounterSet(WMT::Device device, Rc<BufferAllocation> &&allocation);

private:
  friend class CounterAllocation;
  friend class CounterPool;
  std::array<CounterAllocation, kCounterSetSize> counters_;
  std::atomic<uint64_t> used_counter_ = {};
  Rc<BufferAllocation> allocation_;

  void recycle(uint32_t index);

  Rc<CounterAllocation> tryAllocate(uint32_t initial_value);
};

class Counter {
public:
  void incRef();
  void decRef();

  operator bool() const {
    return bool(current_);
  }

private:
  friend class CounterPool;

  Rc<CounterAllocation> current_;
  std::atomic<uint32_t> refcount_ = {0u};
};

class CounterPool {
public:
  CounterPool(WMT::Device device);

  CounterAllocation *getCurrentAllocation(const Rc<Counter> &counter) {
    if (unlikely(!*counter))
      updateCounter(counter, 0);
    return counter->current_.ptr();
  }

  void updateCounter(const Rc<Counter> &counter, uint32_t new_value);

private:
  Rc<CounterAllocation> allocate(uint32_t initial_value);

  std::unique_ptr<CounterSet> allocateCounterSet();

  small_vector<std::unique_ptr<CounterSet>, 4> counter_sets_;
  dxmt::mutex mutex_;
  size_t next_set_index_ = 0;
  WMT::Device device_;
  Rc<Buffer> buffer_;
};

} // namespace dxmt