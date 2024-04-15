#include "dxgi_factory.h"
#include "Metal/MTLDevice.hpp"
#include "com/com_guid.hpp"
#include "dxgi_adapter.h"
#include "log/log.hpp"
#include "util_string.hpp"
#include "wsi_window.hpp"

namespace dxmt {
MTLDXGIFactory::MTLDXGIFactory(UINT Flags) : flags_(Flags) {}

MTLDXGIFactory::~MTLDXGIFactory() {}

HRESULT STDMETHODCALLTYPE MTLDXGIFactory::QueryInterface(REFIID riid,
                                                         void **ppvObject) {
  if (ppvObject == nullptr)
    return E_POINTER;

  *ppvObject = nullptr;

  if (riid == __uuidof(IUnknown) || riid == __uuidof(IDXGIObject) ||
      riid == __uuidof(IDXGIFactory) || riid == __uuidof(IDXGIFactory1) ||
      riid == __uuidof(IDXGIFactory2)) {
    *ppvObject = ref(this);
    return S_OK;
  }

  if (logQueryInterfaceError(__uuidof(IDXGIFactory2), riid)) {
    WARN("Unknown interface query", str::format(riid));
  }

  return E_NOINTERFACE;
}

HRESULT STDMETHODCALLTYPE MTLDXGIFactory::GetParent(REFIID riid,
                                                    void **ppParent) {
  InitReturnPtr(ppParent);

  WARN("Unknown interface query", str::format(riid));
  return E_NOINTERFACE;
}

BOOL STDMETHODCALLTYPE MTLDXGIFactory::IsWindowedStereoEnabled() {
  // We don't support Stereo 3D at the moment
  return FALSE;
}

HRESULT STDMETHODCALLTYPE MTLDXGIFactory::CreateSoftwareAdapter(
    HMODULE Module, IDXGIAdapter **ppAdapter) {
  InitReturnPtr(ppAdapter);

  if (ppAdapter == nullptr)
    return DXGI_ERROR_INVALID_CALL;

  ERR("Software adapters not supported");
  return DXGI_ERROR_UNSUPPORTED;
}

HRESULT STDMETHODCALLTYPE
MTLDXGIFactory::CreateSwapChain(IUnknown *pDevice, DXGI_SWAP_CHAIN_DESC *pDesc,
                                IDXGISwapChain **ppSwapChain) {
  if (ppSwapChain == nullptr || pDesc == nullptr || pDevice == nullptr)
    return DXGI_ERROR_INVALID_CALL;

  DXGI_SWAP_CHAIN_DESC1 desc;
  desc.Width = pDesc->BufferDesc.Width;
  desc.Height = pDesc->BufferDesc.Height;
  desc.Format = pDesc->BufferDesc.Format;
  desc.Stereo = FALSE;
  desc.SampleDesc = pDesc->SampleDesc;
  desc.BufferUsage = pDesc->BufferUsage;
  desc.BufferCount = pDesc->BufferCount;
  desc.Scaling = DXGI_SCALING_STRETCH;
  desc.SwapEffect = pDesc->SwapEffect;
  desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
  desc.Flags = pDesc->Flags;

  DXGI_SWAP_CHAIN_FULLSCREEN_DESC descFs;
  descFs.RefreshRate = pDesc->BufferDesc.RefreshRate;
  descFs.ScanlineOrdering = pDesc->BufferDesc.ScanlineOrdering;
  descFs.Scaling = pDesc->BufferDesc.Scaling;
  descFs.Windowed = pDesc->Windowed;

  IDXGISwapChain1 *swapChain = nullptr;
  HRESULT hr = CreateSwapChainForHwnd(pDevice, pDesc->OutputWindow, &desc,
                                      &descFs, nullptr, &swapChain);

  *ppSwapChain = swapChain;
  return hr;
}

HRESULT STDMETHODCALLTYPE MTLDXGIFactory::CreateSwapChainForHwnd(
    IUnknown *pDevice, HWND hWnd, const DXGI_SWAP_CHAIN_DESC1 *pDesc,
    const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc,
    IDXGIOutput *pRestrictToOutput, IDXGISwapChain1 **ppSwapChain) {
  InitReturnPtr(ppSwapChain);

  if (!ppSwapChain || !pDesc || !hWnd || !pDevice)
    return DXGI_ERROR_INVALID_CALL;

  Com<IMTLDXGIDevice> metal_dxgi_device;
  if (FAILED(pDevice->QueryInterface(IID_PPV_ARGS(&metal_dxgi_device)))) {
    ERR("Unsupported device type");
    return DXGI_ERROR_UNSUPPORTED;
  }

  // Make sure the back buffer size is not zero
  DXGI_SWAP_CHAIN_DESC1 desc = *pDesc;

  wsi::getWindowSize(hWnd, desc.Width ? nullptr : &desc.Width,
                     desc.Height ? nullptr : &desc.Height);

  // If necessary, set up a default set of
  // fullscreen parameters for the swap chain
  DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsDesc;

  if (pFullscreenDesc) {
    fsDesc = *pFullscreenDesc;
  } else {
    fsDesc.RefreshRate = {0, 0};
    fsDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    fsDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    fsDesc.Windowed = TRUE;
  }

  return metal_dxgi_device->CreateSwapChain(this, hWnd, &desc, &fsDesc,
                                            ppSwapChain);
}

HRESULT STDMETHODCALLTYPE MTLDXGIFactory::CreateSwapChainForCoreWindow(
    IUnknown *pDevice, IUnknown *pWindow, const DXGI_SWAP_CHAIN_DESC1 *pDesc,
    IDXGIOutput *pRestrictToOutput, IDXGISwapChain1 **ppSwapChain) {
  InitReturnPtr(ppSwapChain);

  ERR("Not implemented");
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE MTLDXGIFactory::CreateSwapChainForComposition(
    IUnknown *pDevice, const DXGI_SWAP_CHAIN_DESC1 *pDesc,
    IDXGIOutput *pRestrictToOutput, IDXGISwapChain1 **ppSwapChain) {
  InitReturnPtr(ppSwapChain);

  ERR("Not implemented");
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
MTLDXGIFactory::EnumAdapters1(UINT Adapter, IDXGIAdapter1 **ppAdapter) {

  auto devices = MTL::CopyAllDevices();

  if (Adapter >= devices->count()) {
    return DXGI_ERROR_INVALID_CALL;
  }

  auto device = devices->object<MTL::Device>(Adapter);

  *ppAdapter = ref(new MTLDXGIAdatper(device, this));
  devices->release();
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLDXGIFactory::EnumAdapters(UINT Adapter, IDXGIAdapter **ppAdapter) {
  InitReturnPtr(ppAdapter);

  if (ppAdapter == nullptr)
    return DXGI_ERROR_INVALID_CALL;

  IDXGIAdapter1 *handle = nullptr;
  HRESULT hr = this->EnumAdapters1(Adapter, &handle);
  *ppAdapter = handle;
  return hr;
}

HRESULT STDMETHODCALLTYPE
MTLDXGIFactory::GetWindowAssociation(HWND *pWindowHandle) {
  if (pWindowHandle == nullptr)
    return DXGI_ERROR_INVALID_CALL;

  *pWindowHandle = m_associatedWindow;
  return S_OK;
}
HRESULT STDMETHODCALLTYPE
MTLDXGIFactory::MakeWindowAssociation(HWND WindowHandle, UINT Flags) {
  WARN("Ignoring flags");
  m_associatedWindow = WindowHandle;
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLDXGIFactory::GetSharedResourceAdapterLuid(HANDLE hResource, LUID *pLuid) {
  ERR("Not implemented");
  return E_NOTIMPL;
}
BOOL STDMETHODCALLTYPE MTLDXGIFactory::IsCurrent() { return TRUE; }

HRESULT STDMETHODCALLTYPE MTLDXGIFactory::RegisterOcclusionStatusWindow(
    HWND WindowHandle, UINT wMsg, DWORD *pdwCookie) {
  ERR("Not implemented");
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
MTLDXGIFactory::RegisterStereoStatusEvent(HANDLE hEvent, DWORD *pdwCookie) {
  ERR("Not implemented");
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE MTLDXGIFactory::RegisterStereoStatusWindow(
    HWND WindowHandle, UINT wMsg, DWORD *pdwCookie) {
  ERR("Not implemented");
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
MTLDXGIFactory::RegisterOcclusionStatusEvent(HANDLE hEvent, DWORD *pdwCookie) {
  ERR("Not implemented");
  return E_NOTIMPL;
}

void STDMETHODCALLTYPE MTLDXGIFactory::UnregisterStereoStatus(DWORD dwCookie) {
  ERR("Not implemented");
}

void STDMETHODCALLTYPE
MTLDXGIFactory::UnregisterOcclusionStatus(DWORD dwCookie) {
  ERR("Not implemented");
}

} // namespace dxmt