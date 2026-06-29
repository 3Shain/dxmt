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

#include "com/com_guid.hpp"
#include "com/com_pointer.hpp"
#include "d3d12_device.hpp"
#include "d3d12_pageable.hpp"
#include "dxgi_interfaces.h"
#include "log/log.hpp"

namespace dxmt {

class MTLD3D12CommandQueueImpl : public MTLD3D12Pageable<MTLD3D12CommandQueue, IMTLSwapChainFactory> {

  D3D12_COMMAND_QUEUE_DESC desc_;

public:
  MTLD3D12CommandQueueImpl(MTLD3D12Device *pDevice) :
      MTLD3D12Pageable<MTLD3D12CommandQueue, IMTLSwapChainFactory>(pDevice) {}

  HRESULT
  Initialize(const D3D12_COMMAND_QUEUE_DESC *pDesc) {
    // TODO: validate and normalize
    desc_ = *pDesc;
    desc_.NodeMask = 1; // typically 1 GPU only
    return S_OK;
  }

  HRESULT
  STDMETHODCALLTYPE
  QueryInterface(REFIID riid, void **ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D12Object) || riid == __uuidof(ID3D12DeviceChild) ||
        riid == __uuidof(ID3D12Pageable) || riid == __uuidof(ID3D12CommandQueue)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (riid == __uuidof(IMTLSwapChainFactory)) {
      *ppvObject = ref_and_cast<IMTLSwapChainFactory>(this);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(ID3D12CommandQueue), riid)) {
      WARN("D3D12CommandQueue: Unknown interface query ", str::format(riid));
    }

    return E_NOINTERFACE;
  }

  void STDMETHODCALLTYPE UpdateTileMappings(
      ID3D12Resource *resource, UINT region_count, const D3D12_TILED_RESOURCE_COORDINATE *region_start_coordinates,
      const D3D12_TILE_REGION_SIZE *region_sizes, ID3D12Heap *heap, UINT range_count,
      const D3D12_TILE_RANGE_FLAGS *range_flags, const UINT *heap_range_offsets, const UINT *range_tile_counts,
      D3D12_TILE_MAPPING_FLAGS flags
  ) {
    IMPLEMENT_ME
  };

  void STDMETHODCALLTYPE CopyTileMappings(
      ID3D12Resource *dst_resource, const D3D12_TILED_RESOURCE_COORDINATE *dst_region_start_coordinate,
      ID3D12Resource *src_resource, const D3D12_TILED_RESOURCE_COORDINATE *src_region_start_coordinate,
      const D3D12_TILE_REGION_SIZE *region_size, D3D12_TILE_MAPPING_FLAGS flags
  ) {
    IMPLEMENT_ME
  };

  void STDMETHODCALLTYPE ExecuteCommandLists(UINT Count, ID3D12CommandList *const *ppCommandLists) { IMPLEMENT_ME };

  void STDMETHODCALLTYPE SetMarker(UINT metadata, const void *data, UINT size) {};

  void STDMETHODCALLTYPE BeginEvent(UINT metadata, const void *data, UINT size) {};

  void STDMETHODCALLTYPE EndEvent() {};

  HRESULT STDMETHODCALLTYPE
  Signal(ID3D12Fence *pFence, UINT64 Value) {
    return E_NOTIMPL;
  };

  HRESULT STDMETHODCALLTYPE
  Wait(ID3D12Fence *pFence, UINT64 Value) {
    return E_NOTIMPL;
  };

  HRESULT STDMETHODCALLTYPE
  GetTimestampFrequency(UINT64 *pFrequency) {
    // FIXME: stub
    if (pFrequency)
      *pFrequency = 1;
    return S_OK;
  };

  HRESULT STDMETHODCALLTYPE
  GetClockCalibration(UINT64 *gpu_timestamp, UINT64 *cpu_timestamp) {
    return E_NOTIMPL;
  };

  D3D12_COMMAND_QUEUE_DESC *STDMETHODCALLTYPE
  GetDesc(D3D12_COMMAND_QUEUE_DESC *__ret) {
    *__ret = desc_;
    return __ret;
  };

  HRESULT STDMETHODCALLTYPE
  CreateSwapChain(
      IDXGIFactory1 *pFactory, HWND hWnd, const DXGI_SWAP_CHAIN_DESC1 *pDesc,
      const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc, IDXGISwapChain1 **ppSwapChain
  ) {
    return dxmt::CreateSwapChain(pFactory, device_, this, hWnd, pDesc, pFullscreenDesc, ppSwapChain);
  }
};

HRESULT
CreateCommandQueue(MTLD3D12Device *pDevice, const D3D12_COMMAND_QUEUE_DESC *pDesc, REFIID riid, void **ppCommandQueue) {
  auto command_queue = Com(new MTLD3D12CommandQueueImpl(pDevice));
  HRESULT hr = command_queue->Initialize(pDesc);
  if (FAILED(hr))
    return hr;
  return command_queue->QueryInterface(riid, ppCommandQueue);
};

} // namespace dxmt