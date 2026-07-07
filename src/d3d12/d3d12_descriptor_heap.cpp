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
#include "d3d12_descriptor_heap.hpp"
#include "d3d12_pageable.hpp"
#include "com/com_pointer.hpp"
#include "log/log.hpp"

namespace dxmt {

struct SRVTextureGPUStorage {
  uint64_t resource_id;
  uint64_t metadata;
  uint64_t padding[2];
};

struct ShaderVisibleDescriptorGPUStorage {
  union {
    SRVTextureGPUStorage SRVTexture;
    CBVCommonStorage ConstantBuffer;
  };

  ShaderVisibleDescriptorGPUStorage();
};

static_assert(sizeof(ShaderVisibleDescriptorGPUStorage) == 32);

inline uint64_t
TextureMetadata(uint32_t array_length, float min_lod) {
  return ((uint64_t)array_length << 32) | (uint64_t)std::bit_cast<uint32_t>(min_lod);
}

class MTLD3D12DescriptorHeapImpl : public MTLD3D12Pageable<MTLD3D12DescriptorHeap> {

  D3D12_DESCRIPTOR_HEAP_DESC desc_;

  std::vector<ShaderVisibleDescriptorCPUStorage> descriptors_;
  Rc<Buffer> buffer_;
  ShaderVisibleDescriptorGPUStorage *mapped_argument_buffer_ = nullptr;
  uint64_t argument_buffer_gpu_address_ = 0;

public:
  MTLD3D12DescriptorHeapImpl(MTLD3D12Device *pDevice) : MTLD3D12Pageable<MTLD3D12DescriptorHeap>(pDevice) {}

  HRESULT
  Initialize(const D3D12_DESCRIPTOR_HEAP_DESC *pDesc) {
    if (!pDesc)
      return E_INVALIDARG;
    desc_ = *pDesc;
    switch (pDesc->Type) {
    case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV: {
      break;
    }
    default:
      return E_INVALIDARG;
    }
    descriptors_.resize(pDesc->NumDescriptors);

    if (pDesc->Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE) {
      buffer_ = new Buffer(descriptors_.size() * sizeof(ShaderVisibleDescriptorGPUStorage), device_->GetMTLDevice());

      Flags<BufferAllocationFlag> flags;
#ifdef __i386__
      IMPLEMENT_ME
#endif
      buffer_->rename(buffer_->allocate(flags));
      mapped_argument_buffer_ =
          reinterpret_cast<ShaderVisibleDescriptorGPUStorage *>(buffer_->current()->mappedMemory(0));
      argument_buffer_gpu_address_ = buffer_->current()->gpuAddress();
      device_->RegisterResidencyAndVA(buffer_->current());
    } else {
      mapped_argument_buffer_ = reinterpret_cast<ShaderVisibleDescriptorGPUStorage *>(
          malloc(descriptors_.size() * sizeof(ShaderVisibleDescriptorGPUStorage))
      );
    }

    return S_OK;
  };

  ~MTLD3D12DescriptorHeapImpl() {
    if (buffer_) {
      device_->UnregisterResidencyAndVA(buffer_->current());
    } else {
      free(mapped_argument_buffer_);
    }
  }

  HRESULT
  STDMETHODCALLTYPE
  QueryInterface(REFIID riid, void **ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D12Object) || riid == __uuidof(ID3D12DeviceChild) ||
        riid == __uuidof(ID3D12Pageable) || riid == __uuidof(ID3D12DescriptorHeap)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(ID3D12DescriptorHeap), riid)) {
      WARN("D3D12DescriptorHeap: Unknown interface query ", str::format(riid));
    }

    return E_NOINTERFACE;
  }

  virtual D3D12_DESCRIPTOR_HEAP_DESC *STDMETHODCALLTYPE
  GetDesc(D3D12_DESCRIPTOR_HEAP_DESC *__ret) {
    *__ret = desc_;
    return __ret;
  }

  virtual D3D12_CPU_DESCRIPTOR_HANDLE *STDMETHODCALLTYPE
  GetCPUDescriptorHandleForHeapStart(D3D12_CPU_DESCRIPTOR_HANDLE *__ret) {
    *__ret = GetShaderVisibleDescriptor(this, 0);
    return __ret;
  }

  virtual D3D12_GPU_DESCRIPTOR_HANDLE *STDMETHODCALLTYPE
  GetGPUDescriptorHandleForHeapStart(D3D12_GPU_DESCRIPTOR_HANDLE *__ret) {
    __ret->ptr = argument_buffer_gpu_address_;
    return __ret;
  }

  virtual HRESULT
  AddShaderResourceView(UINT Index, Texture *Texture, TextureViewKey View, FLOAT ResourceMinLODClamp) {
    if (Index >= descriptors_.size())
      return E_INVALIDARG;
    auto &cpu_storage = descriptors_[Index];
    cpu_storage.type = ShaderVisibleDescriptorType::SRVTexture;
    cpu_storage.SRVTexture.texture = Texture;
    cpu_storage.SRVTexture.view = View;
    if (mapped_argument_buffer_) {
      auto &texture_view = Texture->view(View);
      auto &gpu_storage = mapped_argument_buffer_[Index];
      gpu_storage.SRVTexture.resource_id = texture_view.gpuResourceID;
      gpu_storage.SRVTexture.metadata = TextureMetadata(Texture->arrayLength(View), ResourceMinLODClamp);
    }
    return S_OK;
  }
    virtual HRESULT
  AddConstantBufferView(UINT Index, UINT64 VA, UINT32 SizeInBytes) {
    if (Index >= descriptors_.size())
      return E_INVALIDARG;
    auto &cpu_storage = descriptors_[Index];
    cpu_storage.type = ShaderVisibleDescriptorType::ConstantBuffer;
    cpu_storage.ConstantBuffer.address = VA;
    cpu_storage.ConstantBuffer.size = SizeInBytes;
    if (mapped_argument_buffer_) {
      auto &gpu_storage = mapped_argument_buffer_[Index];
      gpu_storage.ConstantBuffer.address = VA;
      gpu_storage.ConstantBuffer.size = SizeInBytes;
    }
    return S_OK;
  }

};

class MTLD3D12RenderTargetDescriptorHeapImpl : public MTLD3D12Pageable<MTLD3D12RenderTargetDescriptorHeap> {

  D3D12_DESCRIPTOR_HEAP_DESC desc_;

  std::vector<MTL_RENDER_TARGET_DESC> render_targets_;

public:
  MTLD3D12RenderTargetDescriptorHeapImpl(MTLD3D12Device *pDevice) :
      MTLD3D12Pageable<MTLD3D12RenderTargetDescriptorHeap>(pDevice) {}

  HRESULT
  Initialize(const D3D12_DESCRIPTOR_HEAP_DESC *pDesc) {
    if (!pDesc)
      return E_INVALIDARG;
    desc_ = *pDesc;
    switch (pDesc->Type) {
    case D3D12_DESCRIPTOR_HEAP_TYPE_DSV:
    case D3D12_DESCRIPTOR_HEAP_TYPE_RTV: {
      if (pDesc->Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
        return E_INVALIDARG;
      render_targets_.resize(pDesc->NumDescriptors);
      break;
    }
    default:
      return E_INVALIDARG;
    }
    return S_OK;
  };

  HRESULT
  STDMETHODCALLTYPE
  QueryInterface(REFIID riid, void **ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D12Object) || riid == __uuidof(ID3D12DeviceChild) ||
        riid == __uuidof(ID3D12Pageable) || riid == __uuidof(ID3D12DescriptorHeap)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(ID3D12DescriptorHeap), riid)) {
      WARN("D3D12DescriptorHeap: Unknown interface query ", str::format(riid));
    }

    return E_NOINTERFACE;
  }

  virtual D3D12_DESCRIPTOR_HEAP_DESC *STDMETHODCALLTYPE
  GetDesc(D3D12_DESCRIPTOR_HEAP_DESC *__ret) {
    *__ret = desc_;
    return __ret;
  }

  virtual D3D12_CPU_DESCRIPTOR_HANDLE *STDMETHODCALLTYPE
  GetCPUDescriptorHandleForHeapStart(D3D12_CPU_DESCRIPTOR_HANDLE *__ret) {
    *__ret = GetRenderTargetDescriptor(this, 0);
    return __ret;
  }

  virtual D3D12_GPU_DESCRIPTOR_HANDLE *STDMETHODCALLTYPE
  GetGPUDescriptorHandleForHeapStart(D3D12_GPU_DESCRIPTOR_HANDLE *__ret) {
    __ret->ptr = 0;
    return __ret;
  }

  virtual HRESULT
  AddRenderTarget(UINT Index, MTL_RENDER_TARGET_DESC const *pDesc) {
    if (Index >= render_targets_.size())
      return E_INVALIDARG;
    if (pDesc)
      render_targets_[Index] = *pDesc;
    else
      render_targets_[Index] = {};
    return S_OK;
  }

  virtual MTL_RENDER_TARGET_DESC
  GetRenderTarget(UINT Index) {
    return render_targets_[Index];
  }
};

class MTLD3D12SamplerDescriptorHeapImpl : public MTLD3D12Pageable<MTLD3D12SamplerDescriptorHeap> {

  D3D12_DESCRIPTOR_HEAP_DESC desc_;

public:
  MTLD3D12SamplerDescriptorHeapImpl(MTLD3D12Device *pDevice) :
      MTLD3D12Pageable<MTLD3D12SamplerDescriptorHeap>(pDevice) {}

  HRESULT
  Initialize(const D3D12_DESCRIPTOR_HEAP_DESC *pDesc) {
    if (!pDesc)
      return E_INVALIDARG;
    desc_ = *pDesc;
    switch (pDesc->Type) {
    case D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER: {
      break;
    }
    default:
      return E_INVALIDARG;
    }
    return S_OK;
  };

  ~MTLD3D12SamplerDescriptorHeapImpl() {}

  HRESULT
  STDMETHODCALLTYPE
  QueryInterface(REFIID riid, void **ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D12Object) || riid == __uuidof(ID3D12DeviceChild) ||
        riid == __uuidof(ID3D12Pageable) || riid == __uuidof(ID3D12DescriptorHeap)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(ID3D12DescriptorHeap), riid)) {
      WARN("D3D12DescriptorHeap: Unknown interface query ", str::format(riid));
    }

    return E_NOINTERFACE;
  }

  virtual D3D12_DESCRIPTOR_HEAP_DESC *STDMETHODCALLTYPE
  GetDesc(D3D12_DESCRIPTOR_HEAP_DESC *__ret) {
    *__ret = desc_;
    return __ret;
  }

  virtual D3D12_CPU_DESCRIPTOR_HANDLE *STDMETHODCALLTYPE
  GetCPUDescriptorHandleForHeapStart(D3D12_CPU_DESCRIPTOR_HANDLE *__ret) {
    *__ret = {};
    return __ret;
  }

  virtual D3D12_GPU_DESCRIPTOR_HANDLE *STDMETHODCALLTYPE
  GetGPUDescriptorHandleForHeapStart(D3D12_GPU_DESCRIPTOR_HANDLE *__ret) {
    __ret->ptr = 0;
    return __ret;
  }
};

HRESULT
CreateDescriptorHeap(
    MTLD3D12Device *pDevice, const D3D12_DESCRIPTOR_HEAP_DESC *pDesc, REFIID riid, void **ppDescriptorHeap
) {
  InitReturnPtr(ppDescriptorHeap);
  if (!pDesc)
    return E_INVALIDARG;
  if (pDesc->NumDescriptors > 0xFFFFF) {
    ERR("CreateDescriptorHeap: NumDescriptors is too large");
    return E_INVALIDARG;
  }
  switch (pDesc->Type) {
  case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV: {
    auto descriptor_heap = Com(new MTLD3D12DescriptorHeapImpl(pDevice));
    HRESULT hr = descriptor_heap->Initialize(pDesc);
    if (FAILED(hr))
      return hr;
    if (!ppDescriptorHeap)
      return S_FALSE;
    return descriptor_heap->QueryInterface(riid, ppDescriptorHeap);
  }
  case D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER: {
    auto sampler_heap = Com(new MTLD3D12SamplerDescriptorHeapImpl(pDevice));
    HRESULT hr = sampler_heap->Initialize(pDesc);
    if (FAILED(hr))
      return hr;
    if (!ppDescriptorHeap)
      return S_FALSE;
    return sampler_heap->QueryInterface(riid, ppDescriptorHeap);
  }
  case D3D12_DESCRIPTOR_HEAP_TYPE_RTV:
  case D3D12_DESCRIPTOR_HEAP_TYPE_DSV: {
    auto descriptor_heap = Com(new MTLD3D12RenderTargetDescriptorHeapImpl(pDevice));
    HRESULT hr = descriptor_heap->Initialize(pDesc);
    if (FAILED(hr))
      return hr;
    if (!ppDescriptorHeap)
      return S_FALSE;
    return descriptor_heap->QueryInterface(riid, ppDescriptorHeap);
  }
  default:
    break;
  }
  return E_INVALIDARG;
}

} // namespace dxmt