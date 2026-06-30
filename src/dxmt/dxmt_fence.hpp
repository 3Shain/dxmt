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

#include "Metal.hpp"
#include "thread.hpp"
#include <atomic>
#include <cstdint>

namespace dxmt {

class Fence {
public:
  void incRef();
  void decRef();

  WMT::SharedEvent
  sharedEvent() const {
    return event_;
  }

  uint64_t
  completedValue() {
    return event_.signaledValue();
  }

  void
  signal(uint64_t value) {
    if (value < last_signaled_)
      reset();
    else if (value == last_signaled_)
      return;
    event_.signalValue(value);
    last_signaled_ = value;
  };

  void
  signal(WMT::CommandBuffer cmdbuf, uint64_t value) {
    if (value < last_signaled_)
      reset();
    else if (value == last_signaled_)
      return;
    cmdbuf.encodeSignalEvent(event_, value);
    last_signaled_ = value;
  }

  void
  wait(uint64_t value, uint64_t timeout = ~0ULL) {
    event_.waitUntilSignaledValue(value, timeout);
  }

  void
  wait(WMT::CommandBuffer cmdbuf, uint64_t value) {
    cmdbuf.encodeWaitForEvent(event_, value);
  }

  void reset();

  Fence(WMT::Device device);

private:
  WMT::Device device_;
  WMT::Reference<WMT::SharedEvent> event_;
  uint64_t last_signaled_ = 0;
  std::atomic<uint32_t> refcount_ = {0u};
};

class EventListener {
public:
  EventListener();
  ~EventListener();

  void setEventOnValue(Fence const *fence, HANDLE event, uint64_t value);

private:
  obj_handle_t shared_event_listener_;
  dxmt::thread event_listener_thread_;
};

}; // namespace dxmt
