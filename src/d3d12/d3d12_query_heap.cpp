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

#include "com/com_pointer.hpp"
#include "d3d12_pageable.hpp"

namespace dxmt {

class MTLD3D12QueryHeapImpl : public MTLD3D12Pageable<MTLD3D12QueryHeap> {
public:
  MTLD3D12QueryHeapImpl(MTLD3D12Device *pDevice) : MTLD3D12Pageable<MTLD3D12QueryHeap>(pDevice) {}

  HRESULT
  Initialize(const D3D12_QUERY_HEAP_DESC *pDesc) {
    query_count = pDesc->Count;

    /**
    Two uint64_t are allocated for each occlusion query slot, one for binary occlusion.
    We need to store both numerical and binary value because we implement `ResolveQueryData` as a blit, where
    `D3D12_QUERY_TYPE` actually matters
    */
    auto query_size = pDesc->Type == D3D12_QUERY_HEAP_TYPE_OCCLUSION ? sizeof(uint64_t) * 2 : sizeof(uint64_t);

    WMTBufferInfo info;
    info.memory.set(nullptr);
    info.length = query_size * query_count;
    info.options = WMTResourceStorageModePrivate | WMTResourceHazardTrackingModeUntracked;

    query_heap = device_->GetMTLDevice().newBuffer(info);

    if (!query_heap) {
      ERR("MTLD3D12QueryHeapImpl: failed to allocate query heap");
      return E_OUTOFMEMORY;
    }
    query_heap_address = info.gpu_address;
    device_->RegisterResidency(query_heap);
    return S_OK;
  }

  ~MTLD3D12QueryHeapImpl() {
    if (query_heap) {
      device_->UnregisterResidency(query_heap);
    }
    query_heap = {};
    query_heap_address = 0;
  }

  HRESULT
  STDMETHODCALLTYPE
  QueryInterface(REFIID riid, void **ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D12Object) || riid == __uuidof(ID3D12DeviceChild) ||
        riid == __uuidof(ID3D12Pageable) || riid == __uuidof(ID3D12QueryHeap)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(ID3D12QueryHeap), riid)) {
      WARN("D3D12QueryHeap: Unknown interface query ", str::format(riid));
    }

    return E_NOINTERFACE;
  }
};

HRESULT
CreateQueryHeap(MTLD3D12Device *pDevice, const D3D12_QUERY_HEAP_DESC *pDesc, REFIID riid, void **ppQueryHeap) {
  auto heap = Com(new MTLD3D12QueryHeapImpl(pDevice));
  HRESULT hr = heap->Initialize(pDesc);
  if (FAILED(hr))
    return hr;
  if (!ppQueryHeap)
    return S_FALSE;
  return heap->QueryInterface(riid, ppQueryHeap);
}

} // namespace dxmt