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

#include "dxmt_fence.hpp"
#include <thread>

namespace dxmt {

void
Fence::incRef() {
  refcount_.fetch_add(1u, std::memory_order_acquire);
};

void
Fence::decRef() {
  if (refcount_.fetch_sub(1u, std::memory_order_release) == 1u)
    delete this;
};

Fence::Fence(WMT::Device device) : device_(device) {
  reset();
}

void
Fence::reset() {
  this->event_ = device_.newSharedEvent();
}

EventListener::EventListener() :
    shared_event_listener_(SharedEventListener_create()),
    event_listener_thread_([this]() { SharedEventListener_start(this->shared_event_listener_); }) {};

EventListener::~EventListener() {
  // FIXME: potential deadlock if a device is created and immediately destroyed
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  SharedEventListener_destroy(shared_event_listener_);
  event_listener_thread_.join();
}

void
EventListener::setEventOnValue(Fence const *fence, HANDLE event, uint64_t value) {
  MTLSharedEvent_setWin32EventAtValue(fence->sharedEvent().handle, shared_event_listener_, event, value);
}

}; // namespace dxmt