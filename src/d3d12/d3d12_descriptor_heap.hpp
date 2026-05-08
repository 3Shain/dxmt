#pragma once

#include "com/com_pointer.hpp"
#include "d3d12.h"
#include <atomic>
#include <vector>

namespace dxmt {

class MTLD3D12Device;

struct D3D12Descriptor {
  D3D12_DESCRIPTOR_HEAP_TYPE type;
  union {
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbv;
    D3D12_SHADER_RESOURCE_VIEW_DESC srv;
    D3D12_UNORDERED_ACCESS_VIEW_DESC uav;
    D3D12_RENDER_TARGET_VIEW_DESC rtv;
    D3D12_DEPTH_STENCIL_VIEW_DESC dsv;
    D3D12_SAMPLER_DESC sampler;
  };
  ID3D12Resource *resource = nullptr;
  ID3D12Resource *resource_uav_counter = nullptr;
};

class MTLD3D12DescriptorHeap : public ID3D12DescriptorHeap {
public:
  MTLD3D12DescriptorHeap(MTLD3D12Device *device,
                         const D3D12_DESCRIPTOR_HEAP_DESC &desc);
  ~MTLD3D12DescriptorHeap();

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                           void **ppvObject) override;
  ULONG STDMETHODCALLTYPE AddRef() override;
  ULONG STDMETHODCALLTYPE Release() override;

  HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID guid, UINT *data_size,
                                          void *data) override;
  HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID guid, UINT data_size,
                                          const void *data) override;
  HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
      REFGUID guid, const IUnknown *data) override;
  HRESULT STDMETHODCALLTYPE SetName(LPCWSTR name) override;

  HRESULT STDMETHODCALLTYPE GetDevice(REFIID riid, void **device) override;

  D3D12_DESCRIPTOR_HEAP_DESC *STDMETHODCALLTYPE
  GetDesc(D3D12_DESCRIPTOR_HEAP_DESC *__ret) override;

  D3D12_CPU_DESCRIPTOR_HANDLE *STDMETHODCALLTYPE
  GetCPUDescriptorHandleForHeapStart(D3D12_CPU_DESCRIPTOR_HANDLE *__ret) override;
  D3D12_GPU_DESCRIPTOR_HANDLE *STDMETHODCALLTYPE
  GetGPUDescriptorHandleForHeapStart(D3D12_GPU_DESCRIPTOR_HANDLE *__ret) override;

  D3D12Descriptor *GetDescriptors() { return m_descriptors.data(); }
  uint32_t GetDescriptorCount() { return m_desc.NumDescriptors; }

private:
  MTLD3D12Device *m_device;
  D3D12_DESCRIPTOR_HEAP_DESC m_desc;
  std::vector<D3D12Descriptor> m_descriptors;
  std::atomic<uint32_t> m_refCount = {1ul};
};

} // namespace dxmt
