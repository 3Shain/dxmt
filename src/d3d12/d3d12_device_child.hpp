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
#include "d3d12_device.hpp"
#include "com/com_object.hpp"
#include "com/com_private_data.hpp"

#if defined(__MINGW32__)

const GUID kWKPDID_D3DDebugObjectNameW = {0x4cca5fd8, 0x921f, 0x42c8, 0x85, 0x66, 0x70, 0xca, 0xf2, 0xa9, 0xb7, 0x41};

#endif

namespace dxmt {
template <typename... Base> class MTLD3D12Object : public Base... {
public:
  MTLD3D12Object() {}

  virtual HRESULT STDMETHODCALLTYPE
  GetPrivateData(REFGUID guid, UINT *pDataSize, void *pData) final {
    return private_data_.getData(guid, pDataSize, pData);
  }

  virtual HRESULT STDMETHODCALLTYPE
  SetPrivateData(REFGUID guid, UINT DataSize, const void *pData) final {
    return private_data_.setData(guid, DataSize, pData);
  }

  virtual HRESULT STDMETHODCALLTYPE
  SetPrivateDataInterface(REFGUID guid, const IUnknown *pUnknown) override {
    return private_data_.setInterface(guid, pUnknown);
  }

  virtual HRESULT STDMETHODCALLTYPE
  SetName(const WCHAR *name) override {
    size_t size = 0;
    if (name)
      size = sizeof(WCHAR) * (wcslen(name) + 1);
    return SetPrivateData(kWKPDID_D3DDebugObjectNameW, size, name);
  }

private:
  ComPrivateData private_data_;
};

template <typename... Base> class MTLD3D12DeviceChild : public MTLD3D12Object<ComObject<Base...>> {
public:
  MTLD3D12DeviceChild(MTLD3D12Device *pDevice) : MTLD3D12Object<ComObject<Base...>>(), device_(pDevice) {
    device_->AddRef();
  }

  virtual ~MTLD3D12DeviceChild() {
    device_->Release();
    device_ = nullptr;
  };

  virtual HRESULT STDMETHODCALLTYPE
  GetDevice(REFIID riid, void **ppDevice) final {
    return device_->QueryInterface(riid, ppDevice);
  };

protected:
  MTLD3D12Device *device_;
};

} // namespace dxmt