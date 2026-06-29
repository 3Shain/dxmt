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

#include "d3d12_device.hpp"
#include "d3d12_pageable.hpp"
#include "com/com_pointer.hpp"

namespace dxmt {

class MTLD3D12HeapImpl : public MTLD3D12Pageable<MTLD3D12Heap> {

  D3D12_HEAP_DESC desc_;

public:
  MTLD3D12HeapImpl(MTLD3D12Device *pDevice) : MTLD3D12Pageable<MTLD3D12Heap>(pDevice) {}

  ~MTLD3D12HeapImpl() {}

  HRESULT
  STDMETHODCALLTYPE
  QueryInterface(REFIID riid, void **ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D12Object) || riid == __uuidof(ID3D12DeviceChild) ||
        riid == __uuidof(ID3D12Pageable) || riid == __uuidof(ID3D12Heap)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(ID3D12Resource), riid)) {
      WARN("D3D12Heap: Unknown interface query ", str::format(riid));
    }

    return E_NOINTERFACE;
  }

  HRESULT
  Initialize(const D3D12_HEAP_DESC *pDesc) {
    return S_OK;
  }

  virtual D3D12_HEAP_DESC *STDMETHODCALLTYPE
  GetDesc(D3D12_HEAP_DESC *__ret) {
    *__ret = desc_;
    return __ret;
  };
};

HRESULT
CreateHeap(MTLD3D12Device *pDevice, const D3D12_HEAP_DESC *pDesc, REFIID riid, void **ppHeap) {
  InitReturnPtr(ppHeap);
  auto heap = Com(new MTLD3D12HeapImpl(pDevice));
  HRESULT hr = heap->Initialize(pDesc);
  if (FAILED(hr))
    return hr;
  if (!ppHeap)
    return S_FALSE;
  return heap->QueryInterface(riid, ppHeap);
}

}; // namespace dxmt