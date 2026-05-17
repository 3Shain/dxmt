#pragma once

#include "com/com_pointer.hpp"
#include "d3d12.h"
#include "Metal.hpp"
#include "winemetal.h"
#include <atomic>

namespace dxmt {

class MTLD3D12Device;

class MTLD3D12Resource : public ID3D12Resource {
public:
  MTLD3D12Resource(MTLD3D12Device *device, const D3D12_RESOURCE_DESC &desc,
                   D3D12_RESOURCE_STATES initial_state,
                   D3D12_HEAP_PROPERTIES heap_properties);
  ~MTLD3D12Resource();

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

  HRESULT STDMETHODCALLTYPE Map(UINT sub_resource,
                                const D3D12_RANGE *read_range,
                                void **data) override;
  void STDMETHODCALLTYPE Unmap(UINT sub_resource,
                               const D3D12_RANGE *written_range) override;
  D3D12_RESOURCE_DESC *STDMETHODCALLTYPE
  GetDesc(D3D12_RESOURCE_DESC *__ret) override;
  D3D12_GPU_VIRTUAL_ADDRESS STDMETHODCALLTYPE
  GetGPUVirtualAddress() override;
  HRESULT STDMETHODCALLTYPE WriteToSubresource(
      UINT dst_sub_resource, const D3D12_BOX *dst_box, const void *src_data,
      UINT src_row_pitch, UINT src_slice_pitch) override;
  HRESULT STDMETHODCALLTYPE ReadFromSubresource(
      void *dst_data, UINT dst_row_pitch, UINT dst_slice_pitch,
      UINT src_sub_resource, const D3D12_BOX *src_box) override;
  HRESULT STDMETHODCALLTYPE
  GetHeapProperties(D3D12_HEAP_PROPERTIES *heap_properties,
                    D3D12_HEAP_FLAGS *flags) override;

  bool IsBuffer() const {
    return m_desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER;
  }

  WMT::Reference<WMT::Buffer> GetMTLBuffer() { return m_mtl_buffer; }
  WMT::Reference<WMT::Texture> GetMTLTexture();
  uint64_t GetTextureGPUResourceID() const { return m_tex_gpu_resource_id; }
  uint32_t GetTextureArrayLength() const;
  uint64_t GetBufferByteLength() const;

private:
  MTLD3D12Device *m_device;
  D3D12_RESOURCE_DESC m_desc;
  D3D12_RESOURCE_STATES m_state;
  D3D12_HEAP_PROPERTIES m_heap_properties;
  WMTBufferInfo m_buf_info = {};
  WMT::Reference<WMT::Buffer> m_mtl_buffer;
  WMT::Reference<WMT::Texture> m_mtl_texture;
  WMT::Reference<WMT::Buffer> m_fake_buffer;
  uint64_t m_tex_gpu_resource_id = 0;

  void *m_cpu_addr = nullptr;
  uint64_t m_gpu_addr = 0;
  std::atomic<uint32_t> m_refCount = {1ul};
  std::atomic<uint32_t> m_refPrivate = {1ul};
};

} // namespace dxmt
