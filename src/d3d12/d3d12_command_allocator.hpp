#pragma once

#include "com/com_pointer.hpp"
#include "d3d12.h"
#include <atomic>

namespace dxmt {

class MTLD3D12Device;

class MTLD3D12CommandAllocator : public ID3D12CommandAllocator {
public:
  MTLD3D12CommandAllocator(MTLD3D12Device *device,
                           D3D12_COMMAND_LIST_TYPE type);
  ~MTLD3D12CommandAllocator();

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
  HRESULT STDMETHODCALLTYPE Reset() override;

  D3D12_COMMAND_LIST_TYPE GetType() const { return m_type; }

private:
  MTLD3D12Device *m_device;
  D3D12_COMMAND_LIST_TYPE m_type;
  std::atomic<uint32_t> m_refCount = {1ul};
  std::atomic<uint32_t> m_refPrivate = {1ul};
};

} // namespace dxmt
