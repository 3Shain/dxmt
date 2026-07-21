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

#include "dxmt_keyed_mutex.hpp"
#include "log/log.hpp"
#include <random>

namespace dxmt {

void
KeyedMutex::incRef() {
  refcount_.fetch_add(1u, std::memory_order_acquire);
};

void
KeyedMutex::decRef() {
  if (refcount_.fetch_sub(1u, std::memory_order_release) == 1u)
    delete this;
};

#ifdef _WIN32

KeyedMutex::~KeyedMutex() {
  if (local_kmt_) {
    D3DKMT_DESTROYKEYEDMUTEX destroy = {};
    destroy.hKeyedMutex = local_kmt_;
    D3DKMTDestroyKeyedMutex(&destroy);
    local_kmt_ = {};
  }
  global_kmt_ = {};
};

void
makeUniqueName(char (&out)[54]) {
  std::random_device rd;
  std::mt19937_64 gen(rd());
  std::uniform_int_distribution<uint64_t> dis;

  uint64_t r1 = dis(gen);
  uint64_t r2 = dis(gen);

  std::snprintf(
      out, 54, "DXMT_shared_resource_%016llx%016llx", static_cast<unsigned long long>(r1),
      static_cast<unsigned long long>(r2)
  );
}

Rc<KeyedMutex>
KeyedMutex::create(WMT::Device device) {
  D3DKMT_CREATEKEYEDMUTEX2 create = {};
  create.Flags.NtSecuritySharing = false;
  create.InitialValue = 0;

  auto event = device.newSharedEvent();

  mach_port_t mach_port = event.createMachPort();
  if (!mach_port) {
    ERR("KeyedMutex: Failed to create mach port.");
    return {};
  }
  char mach_port_name[54];
  makeUniqueName(mach_port_name);
  if (!WMTBootstrapRegister(mach_port_name, mach_port)) {
    ERR("KeyedMutex: Failed to register mach port.");
    return {};
  }
  create.pPrivateRuntimeData = mach_port_name;
  create.PrivateRuntimeDataSize = sizeof(mach_port_name);

  if (D3DKMTCreateKeyedMutex2(&create)) {
    ERR("KeyedMutex: Failed to create keyed mutex.");
    return {};
  }

  auto ret = Rc(new KeyedMutex());
  ret->global_kmt_ = create.hSharedHandle;
  ret->local_kmt_ = create.hKeyedMutex;
  ret->event_ = std::move(event);

  return ret;
}

Rc<KeyedMutex>
KeyedMutex::import(WMT::Device device, D3DKMT_HANDLE global_kmt) {
  char mach_port_name[54];
  D3DKMT_OPENKEYEDMUTEX2 open = {};
  open.hSharedHandle = global_kmt;
  open.pPrivateRuntimeData = mach_port_name;
  open.PrivateRuntimeDataSize = sizeof(mach_port_name);

  if (D3DKMTOpenKeyedMutex2(&open)) {
    ERR("KeyedMutex: Failed to open keyed mutex.");
    return {};
  }

  mach_port_t mach_port;
  if (!WMTBootstrapLookUp(mach_port_name, &mach_port)) {
    ERR("KeyedMutex: Failed to lookup mach port.");
    return {};
  }

  auto ret = Rc(new KeyedMutex());
  ret->global_kmt_ = global_kmt;
  ret->local_kmt_ = open.hKeyedMutex;
  ret->event_ = device.newSharedEventWithMachPort(mach_port);
  return ret;
}

HRESULT
KeyedMutex::acquire(KeyedMutexAcquire &&func_acquire, uint64_t key, uint32_t milliseconds) {
  /**
   * doc: If the owning device attempted to create another keyed mutex on the same shared resource, AcquireSync returns
   * E_FAIL.
   */
  if (owned_.load(std::memory_order_acquire))
    return E_FAIL;

  LARGE_INTEGER timeout = {};
  D3DKMT_ACQUIREKEYEDMUTEX acquire = {};
  acquire.hKeyedMutex = local_kmt_;
  acquire.Key = key;
  acquire.pTimeout = &timeout;
  timeout.QuadPart = milliseconds * -10000;

  NTSTATUS status = D3DKMTAcquireKeyedMutex(&acquire);
  if (status == STATUS_TIMEOUT)
    return WAIT_TIMEOUT;
  if (status == STATUS_ABANDONED_WAIT_0)
    return WAIT_ABANDONED;
  if (status) {
    WARN("KeyedMutex: Failed to acquire keyed mutex.");
    return E_FAIL;
  }

  func_acquire(acquire.FenceValue);

  fence_value_ = acquire.FenceValue;
  owned_.store(true, std::memory_order_release);
  return S_OK;
}

HRESULT
KeyedMutex::release(KeyedMutexRelease &&func_release, uint64_t key) {
  /**
   * doc: If the device attempted to release a keyed mutex that is not valid or owned by the device, ReleaseSync returns
   * E_FAIL. */
  if (!owned_.load(std::memory_order_acquire))
    return E_FAIL;

  func_release(fence_value_ + 1);

  D3DKMT_RELEASEKEYEDMUTEX release = {};
  release.hKeyedMutex = local_kmt_;
  release.Key = key;
  release.FenceValue = fence_value_ + 1;

  if (D3DKMTReleaseKeyedMutex(&release)) {
    WARN("KeyedMutex: Failed to release keyed mutex.");
    return E_FAIL;
  }

  owned_.store(false, std::memory_order_release);
  return S_OK;
}

#else

KeyedMutex::~KeyedMutex() {};

Rc<KeyedMutex>
KeyedMutex::create(WMT::Device device) {
  return {};
}

Rc<KeyedMutex>
KeyedMutex::import(WMT::Device device, D3DKMT_HANDLE global_kmt) {
  return {};
}

HRESULT
KeyedMutex::acquire(KeyedMutexAcquire &&func_acquire, uint64_t key, uint32_t milliseconds) {
  return DXGI_ERROR_INVALID_CALL;
}

HRESULT
KeyedMutex::release(KeyedMutexRelease &&func_release, uint64_t key) {
  return DXGI_ERROR_INVALID_CALL;
}

#endif

}; // namespace dxmt