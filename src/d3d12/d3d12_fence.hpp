#pragma once

#include "com/com_pointer.hpp"
#include "d3d12.h"
#include "Metal.hpp"
#include <atomic>

namespace dxmt {

class MTLD3D12Device;

class MTLD3D12Fence : public ID3D12Fence {
public:
  MTLD3D12Fence(MTLD3D12Device *device, uint64_t initial_value,
                D3D12_FENCE_FLAGS flags);
  ~MTLD3D12Fence();

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

  uint64_t STDMETHODCALLTYPE GetCompletedValue() override;
  HRESULT STDMETHODCALLTYPE SetEventOnCompletion(uint64_t value,
                                                 HANDLE event) override;
  HRESULT STDMETHODCALLTYPE Signal(uint64_t value) override;

  WMT::Reference<WMT::SharedEvent> GetMTLSharedEvent() {
    return m_shared_event;
  }

private:
  MTLD3D12Device *m_device;
  D3D12_FENCE_FLAGS m_flags;
  std::atomic<uint64_t> m_value;
  WMT::Reference<WMT::SharedEvent> m_shared_event;
  std::atomic<uint32_t> m_refCount = {1ul};
};

} // namespace dxmt
