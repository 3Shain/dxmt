#pragma once

#include "com/com_pointer.hpp"
#include "d3d12.h"
#include <atomic>

namespace dxmt {

class MTLD3D12Device;

class MTLD3D12Heap : public ID3D12Heap {
public:
  MTLD3D12Heap(MTLD3D12Device *device, const D3D12_HEAP_DESC &desc);
  ~MTLD3D12Heap();

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

  D3D12_HEAP_DESC *STDMETHODCALLTYPE GetDesc(D3D12_HEAP_DESC *__ret) override;

  const D3D12_HEAP_DESC &GetHeapDesc() const { return m_desc; }

private:
  MTLD3D12Device *m_device;
  D3D12_HEAP_DESC m_desc;
  std::atomic<uint32_t> m_refCount = {1ul};
};

} // namespace dxmt
