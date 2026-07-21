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
#include "rc/util_rc_ptr.hpp"
#include "util_d3dkmt.h"
#include <atomic>
#include <cstdint>
#include <functional>

namespace dxmt {

using KeyedMutexAcquire = std::function<void(uint64_t)>;
using KeyedMutexRelease = std::function<void(uint64_t)>;

class KeyedMutex {
public:
  void incRef();
  void decRef();

  ~KeyedMutex();

  static Rc<KeyedMutex> create(WMT::Device device);
  static Rc<KeyedMutex> import(WMT::Device device, D3DKMT_HANDLE global_kmt);

  HRESULT acquire(KeyedMutexAcquire&& func_acquire, uint64_t key, uint32_t milliseconds);
  HRESULT release(KeyedMutexRelease&& func_release, uint64_t key);

  D3DKMT_HANDLE
  globalHandle() const {
    return global_kmt_;
  }

  WMT::SharedEvent
  sharedEvent() const {
    return event_;
  }

private:
  D3DKMT_HANDLE local_kmt_;
  D3DKMT_HANDLE global_kmt_;
  std::atomic<uint32_t> refcount_ = {0u};
  std::atomic_bool owned_ = false;
  uint64_t fence_value_ = 0;
  WMT::Reference<WMT::SharedEvent> event_;
};

} // namespace dxmt