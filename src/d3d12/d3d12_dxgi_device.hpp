#pragma once

#include "com/com_pointer.hpp"
#include "dxgi_interfaces.h"
#include "dxmt_device.hpp"
#include "d3d12.h"
#include "Metal.hpp"
#include <atomic>
#include <memory>

namespace dxmt {

class MTLD3D12Device;

class MTLD3D12DXGIDevice : public IMTLDXGIDevice {
public:
  MTLD3D12DXGIDevice(std::unique_ptr<Device> &&device,
                     IMTLDXGIAdapter *adapter);
  ~MTLD3D12DXGIDevice();

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) override;
  ULONG STDMETHODCALLTYPE AddRef() override;
  ULONG STDMETHODCALLTYPE Release() override;

  HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID Name, UINT *pDataSize, void *pData) override;
  HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID Name, UINT DataSize, const void *pData) override;
  HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID Name, const IUnknown *pUnknown) override;
  HRESULT STDMETHODCALLTYPE GetParent(REFIID riid, void **ppParent) override;

  HRESULT STDMETHODCALLTYPE GetAdapter(IDXGIAdapter **pAdapter) override;
  HRESULT STDMETHODCALLTYPE CreateSurface(const DXGI_SURFACE_DESC *desc,
                                          UINT surface_count, DXGI_USAGE usage,
                                          const DXGI_SHARED_RESOURCE *shared_resource,
                                          IDXGISurface **surface) override;
  HRESULT STDMETHODCALLTYPE QueryResourceResidency(IUnknown *const *ppResources,
                                                   DXGI_RESIDENCY *pResidency,
                                                   UINT ResourceCount) override;
  HRESULT STDMETHODCALLTYPE SetGPUThreadPriority(INT Priority) override;
  HRESULT STDMETHODCALLTYPE GetGPUThreadPriority(INT *pPriority) override;
  HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency(UINT MaxLatency) override;
  HRESULT STDMETHODCALLTYPE GetMaximumFrameLatency(UINT *pMaxLatency) override;
  HRESULT STDMETHODCALLTYPE OfferResources(UINT NumResources,
                                           IDXGIResource *const *ppResources,
                                           DXGI_OFFER_RESOURCE_PRIORITY Priority) override;
  HRESULT STDMETHODCALLTYPE ReclaimResources(UINT NumResources,
                                             IDXGIResource *const *ppResources,
                                             WINBOOL *pDiscarded) override;
  HRESULT STDMETHODCALLTYPE EnqueueSetEvent(HANDLE hEvent) override;
  void STDMETHODCALLTYPE Trim() override;

  WMT::Device STDMETHODCALLTYPE GetMTLDevice() override;
  D3DKMT_HANDLE STDMETHODCALLTYPE GetLocalD3DKMT() override;
  HRESULT STDMETHODCALLTYPE CreateSwapChain(
      IDXGIFactory1 *pFactory, HWND hWnd, const DXGI_SWAP_CHAIN_DESC1 *pDesc,
      const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc,
      IDXGISwapChain1 **ppSwapChain) override;

  MTLD3D12Device *GetD3D12Device() { return m_d3d12_device; }

private:
  Com<IMTLDXGIAdapter> m_adapter;
  MTLD3D12Device *m_d3d12_device;
  D3DKMT_HANDLE m_kmt = 0;
  std::atomic<uint32_t> m_refCount = {1ul};
};

} // namespace dxmt
