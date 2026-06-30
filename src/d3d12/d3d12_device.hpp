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
#include "d3d12.h"
#include "d3d12_command_encoder.hpp"
#include "d3d12_descriptor_heap.hpp"
#include "dxgi1_2.h"
#include "dxgi_interfaces.h"
#include "airconv_public.h"
#include "dxmt_buffer.hpp"
#include "dxmt_fence.hpp"
#include "dxmt_presenter.hpp"
#include "dxmt_texture.hpp"
#include "log/log.hpp"

#define IMPLEMENT_ME                                                                                                   \
  do {                                                                                                                 \
    Logger::err(str::format(__FILE__, ":", __FUNCTION__, "(", __LINE__, ") is not implemented."));                     \
    abort();                                                                                                           \
    __builtin_unreachable();                                                                                           \
  } while (0);

namespace dxmt {

class MTLD3D12GraphicsCommandList : public ID3D12GraphicsCommandList {
public:
  EncoderData *entry;
  size_t encoder_count;
};

class MTLD3D12CommandAllocator : public ID3D12CommandAllocator {
public:
  virtual HRESULT STDMETHODCALLTYPE CreateCommandList(
      UINT NodeMask, D3D12_COMMAND_LIST_TYPE Type, ID3D12PipelineState *pInitialPipelineState, REFIID riid,
      void **ppCommandList
  ) = 0;
};

class MTLD3D12CommandQueue : public ID3D12CommandQueue {
public:
  virtual HRESULT Present(Presenter *presenter, ID3D12Resource *backbuffer, HANDLE hLantecyWaitable) = 0;
};

class MTLD3D12Resource : public ID3D12Resource {
public:
  Rc<Texture> texture;
  Rc<Buffer> buffer;

  virtual HRESULT STDMETHODCALLTYPE
  CreateShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC *pDesc, D3D12_CPU_DESCRIPTOR_HANDLE Descriptor) = 0;

  virtual HRESULT STDMETHODCALLTYPE CreateUnorderedAccessView(
      ID3D12Resource *pCounter, const D3D12_UNORDERED_ACCESS_VIEW_DESC *pDesc, D3D12_CPU_DESCRIPTOR_HANDLE Descriptor
  ) = 0;

  virtual HRESULT STDMETHODCALLTYPE
  CreateRenderTargetView(const D3D12_RENDER_TARGET_VIEW_DESC *pDesc, D3D12_CPU_DESCRIPTOR_HANDLE Descriptor) = 0;

  virtual HRESULT STDMETHODCALLTYPE
  CreateDepthStencilView(const D3D12_DEPTH_STENCIL_VIEW_DESC *pDesc, D3D12_CPU_DESCRIPTOR_HANDLE Descriptor) = 0;

  virtual void STDMETHODCALLTYPE GetResourceTiling(
      UINT *TotalTileCount, D3D12_PACKED_MIP_INFO *PackedMipInfo, D3D12_TILE_SHAPE *StandardTileShape,
      UINT *SubresourceTilingCount, UINT FirstSubresourceTiling, D3D12_SUBRESOURCE_TILING *SubresourceTilings
  ) = 0;
};

class MTLD3D12Heap : public ID3D12Heap {
public:
};

class MTLD3D12Fence : public ID3D12Fence1 {
public:
  Rc<Fence> fence;
};

class MTLD3D12RootSignature : public ID3D12RootSignature {
public:
};

class MTLD3D12PipelineState : public ID3D12PipelineState {
public:
  UINT IsComputePipelineState;
};

class MTLD3D12GraphicsPipelineState : public MTLD3D12PipelineState {
public:
};

class MTLD3D12Device : public ID3D12Device1 {
public:
  virtual WMT::Device GetMTLDevice() = 0;

  virtual D3D_FEATURE_LEVEL GetFeatureLevel() = 0;

  EventListener event_listener;
};

HRESULT CreateD3D12Device(IMTLDXGIAdapter *adapter, REFIID riid, void **ppDevice);

HRESULT
CreateCommandQueue(MTLD3D12Device *pDevice, const D3D12_COMMAND_QUEUE_DESC *pDesc, REFIID riid, void **ppCommandQueue);

HRESULT
CreateCommandAllocator(MTLD3D12Device *pDevice, D3D12_COMMAND_LIST_TYPE Type, REFIID riid, void **ppCommandAllocator);

HRESULT
CreateDescriptorHeap(
    MTLD3D12Device *pDevice, const D3D12_DESCRIPTOR_HEAP_DESC *pDesc, REFIID riid, void **ppDescriptorHeap
);

HRESULT CreateCommittedTexture(
    MTLD3D12Device *pDevice, const D3D12_HEAP_PROPERTIES *pHeapProps, D3D12_HEAP_FLAGS HeapFlags,
    const D3D12_RESOURCE_DESC *pDesc, D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE *OptimizedClearValue,
    REFIID riid, void **ppResource
);

HRESULT
CreatePlacedTexture(
    MTLD3D12Device *pDevice, MTLD3D12Heap *pHeap, const D3D12_RESOURCE_DESC *pDesc, D3D12_RESOURCE_STATES InitialState,
    const D3D12_CLEAR_VALUE *OptimizedClearValue, REFIID riid, void **ppResource
);

HRESULT CreateCommittedBuffer(
    MTLD3D12Device *pDevice, const D3D12_HEAP_PROPERTIES *pHeapProps, D3D12_HEAP_FLAGS HeapFlags,
    const D3D12_RESOURCE_DESC *pDesc, D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE *OptimizedClearValue,
    REFIID riid, void **ppResource
);

HRESULT
CreatePlacedBuffer(
    MTLD3D12Device *pDevice, MTLD3D12Heap *pHeap, const D3D12_RESOURCE_DESC *pDesc, D3D12_RESOURCE_STATES InitialState,
    const D3D12_CLEAR_VALUE *OptimizedClearValue, REFIID riid, void **ppResource
);

HRESULT
CreateHeap(MTLD3D12Device *pDevice, const D3D12_HEAP_DESC *pDesc, REFIID riid, void **ppHeap);

HRESULT
CreateRootSignature(
    MTLD3D12Device *pDevice, UINT NodeMask, const void *pBytecode, SIZE_T BytecodeLength, REFIID riid,
    void **ppRootSignature
);

HRESULT
CreateGraphicsPipelineState(
    MTLD3D12Device *pDevice, const D3D12_GRAPHICS_PIPELINE_STATE_DESC *pDesc, REFIID riid, void **ppPipelineState
);

HRESULT
CreateSwapChain(
    IDXGIFactory1 *pFactory, MTLD3D12Device *pDevice, MTLD3D12CommandQueue *pQueue, HWND hWnd,
    const DXGI_SWAP_CHAIN_DESC1 *pDesc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc,
    IDXGISwapChain1 **ppSwapChain
);

HRESULT
CreateFence(MTLD3D12Device *pDevice, UINT64 InitialValue, D3D12_FENCE_FLAGS Flags, REFIID riid, void **ppFence);

inline std::tuple<MTLD3D12RenderTargetDescriptorHeap *, UINT>
GetRenderTargetHeap(MTLD3D12Device *pDevice, D3D12_CPU_DESCRIPTOR_HANDLE Handle) {
#ifdef DXMT_USE_EMBEDDED_HEAP_POINTER
  EMBEDDED_DESCRIPTOR_HANDLE impl(Handle);
  return {impl.extract<MTLD3D12RenderTargetDescriptorHeap>(), (UINT)impl.Descriptor};
#else
  IMPLEMENT_ME
  return {};
#endif
}

inline D3D12_CPU_DESCRIPTOR_HANDLE
GetRenderTargetDescriptor(MTLD3D12RenderTargetDescriptorHeap *pHeap, UINT Index) {
#ifdef DXMT_USE_EMBEDDED_HEAP_POINTER
  return EMBEDDED_DESCRIPTOR_HANDLE(pHeap, Index);
#else
  IMPLEMENT_ME
  return {};
#endif
}

template <typename VIEW_DESC>
HRESULT ExtractEntireResourceViewDescription(const D3D12_RESOURCE_DESC &ResourceDesc, VIEW_DESC *pViewDescOut);

} // namespace dxmt