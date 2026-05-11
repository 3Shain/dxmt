#include "d3d12_dxgi_device.hpp"
#include "d3d12_device.hpp"
#include "d3d12_swapchain.hpp"
#include "log/log.hpp"
#include "util_string.hpp"
#include <windows.h>

#define DDTRACE(fmt, ...) do { FILE *_tf = fopen("Z:\\tmp\\dxmt_dxgi_trace.log", "a"); if (_tf) { fprintf(_tf, "DXGIDev::" fmt "\n", ##__VA_ARGS__); fclose(_tf); } } while(0)

namespace dxmt {

MTLD3D12DXGIDevice::MTLD3D12DXGIDevice(std::unique_ptr<Device> &&device,
                                       IMTLDXGIAdapter *adapter)
    : m_adapter(adapter) {
  if (m_adapter)
    m_adapter->AddRef();
  void *dev_mem = VirtualAlloc((void*)0x500000000ULL, sizeof(MTLD3D12Device),
    MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
  if (!dev_mem) dev_mem = VirtualAlloc(nullptr, sizeof(MTLD3D12Device),
    MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
  m_d3d12_device = ::new(dev_mem) MTLD3D12Device(std::move(device), m_adapter.ptr());
  DDTRACE("D3D12Device at %p (VirtualAlloc)", (void*)m_d3d12_device);
  m_d3d12_device->SetDXGIDevice(this);
  Logger::info("D3D12DXGIDevice created");
}

MTLD3D12DXGIDevice::~MTLD3D12DXGIDevice() {
  if (m_d3d12_device)
    m_d3d12_device->Release();
}

ULONG STDMETHODCALLTYPE MTLD3D12DXGIDevice::AddRef() { return ++m_refCount; }

ULONG STDMETHODCALLTYPE MTLD3D12DXGIDevice::Release() {
  uint32_t rc = --m_refCount;
  if (!rc)
    delete this;
  return rc;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12DXGIDevice::GetPrivateData(REFGUID Name, UINT *pDataSize, void *pData) {
  DDTRACE("GetPrivateData E_NOTIMPL");
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12DXGIDevice::SetPrivateData(REFGUID Name, UINT DataSize, const void *pData) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12DXGIDevice::SetPrivateDataInterface(REFGUID Name, const IUnknown *pUnknown) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12DXGIDevice::QueryInterface(REFIID riid, void **ppvObject) {
  {
    FILE *f = fopen("Z:\\tmp\\dxmt_dxgi_trace.log", "a");
    if (f) { fprintf(f, "DXGIDevice::QI(%s)\n", str::format(riid).c_str()); fclose(f); }
  }
  if (!ppvObject)
    return E_POINTER;
  *ppvObject = nullptr;

  if (riid == __uuidof(IUnknown) || riid == __uuidof(IDXGIObject) ||
      riid == __uuidof(IDXGIDevice) || riid == __uuidof(IDXGIDevice1) ||
      riid == __uuidof(IDXGIDevice2) || riid == __uuidof(IDXGIDevice3) ||
      riid == __uuidof(IMTLDXGIDevice)) {
    *ppvObject = ref(this);
    return S_OK;
  }

  if (riid == __uuidof(ID3D12Device) || riid == __uuidof(ID3D12Object) ||
      riid == __uuidof(ID3D12DeviceChild)) {
    return m_d3d12_device->QueryInterface(riid, ppvObject);
  }

  Logger::warn(str::format("D3D12DXGIDevice::QueryInterface: unknown ", riid));
  return E_NOINTERFACE;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12DXGIDevice::GetParent(REFIID riid, void **ppParent) {
  return m_adapter->QueryInterface(riid, ppParent);
}

HRESULT STDMETHODCALLTYPE
MTLD3D12DXGIDevice::GetAdapter(IDXGIAdapter **pAdapter) {
  if (!pAdapter)
    return DXGI_ERROR_INVALID_CALL;
  *pAdapter = m_adapter.ref();
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12DXGIDevice::CreateSurface(const DXGI_SURFACE_DESC *desc,
                                   UINT surface_count, DXGI_USAGE usage,
                                   const DXGI_SHARED_RESOURCE *shared_resource,
                                   IDXGISurface **surface) {
  DDTRACE("CreateSurface E_NOTIMPL");
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12DXGIDevice::QueryResourceResidency(IUnknown *const *ppResources,
                                           DXGI_RESIDENCY *pResidency,
                                           UINT ResourceCount) {
  if (!ppResources || !pResidency)
    return E_INVALIDARG;
  for (UINT i = 0; i < ResourceCount; i++)
    pResidency[i] = DXGI_RESIDENCY_FULLY_RESIDENT;
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12DXGIDevice::SetGPUThreadPriority(INT Priority) { return S_OK; }

HRESULT STDMETHODCALLTYPE
MTLD3D12DXGIDevice::GetGPUThreadPriority(INT *pPriority) {
  if (pPriority)
    *pPriority = 0;
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12DXGIDevice::SetMaximumFrameLatency(UINT MaxLatency) { return S_OK; }

HRESULT STDMETHODCALLTYPE
MTLD3D12DXGIDevice::GetMaximumFrameLatency(UINT *pMaxLatency) {
  if (pMaxLatency)
    *pMaxLatency = 2;
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12DXGIDevice::OfferResources(UINT NumResources,
                                   IDXGIResource *const *ppResources,
                                   DXGI_OFFER_RESOURCE_PRIORITY Priority) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12DXGIDevice::ReclaimResources(UINT NumResources,
                                     IDXGIResource *const *ppResources,
                                     WINBOOL *pDiscarded) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12DXGIDevice::EnqueueSetEvent(HANDLE hEvent) { return E_FAIL; }

void STDMETHODCALLTYPE MTLD3D12DXGIDevice::Trim() {}

WMT::Device STDMETHODCALLTYPE MTLD3D12DXGIDevice::GetMTLDevice() {
  return m_adapter->GetMTLDevice();
}

D3DKMT_HANDLE STDMETHODCALLTYPE MTLD3D12DXGIDevice::GetLocalD3DKMT() {
  return m_kmt;
}

HRESULT STDMETHODCALLTYPE MTLD3D12DXGIDevice::CreateSwapChain(
    IDXGIFactory1 *pFactory, HWND hWnd, const DXGI_SWAP_CHAIN_DESC1 *pDesc,
    const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc,
    IDXGISwapChain1 **ppSwapChain) {
  DDTRACE("CreateSwapChain called");
  return dxmt::CreateD3D12SwapChain(pFactory, m_d3d12_device, this, hWnd,
                                    pDesc, pFullscreenDesc, ppSwapChain);
}

} // namespace dxmt
