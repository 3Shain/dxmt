#pragma once

#include <dxgi1_6.h>
#include "util_d3dkmt.h"
#include "Metal.hpp"
#include "com/com_guid.hpp"
#include "dxgi_object.hpp"

DEFINE_COM_INTERFACE("acdf3ef1-b33a-4cb6-97bd-1c1974827e6d", IMTLDXGIAdapter)
    : public IDXGIAdapter4 {
  virtual WMT::Device STDMETHODCALLTYPE GetMTLDevice() = 0;
  virtual D3DKMT_HANDLE STDMETHODCALLTYPE GetLocalD3DKMT() = 0;
};

DEFINE_COM_INTERFACE("6bfa1657-9cb1-471a-a4fb-7cacf8a81207", IMTLDXGIDevice)
    : public IDXGIDevice3 {
  virtual WMT::Device STDMETHODCALLTYPE GetMTLDevice() = 0;
  virtual D3DKMT_HANDLE STDMETHODCALLTYPE GetLocalD3DKMT() = 0;
  virtual HRESULT STDMETHODCALLTYPE CreateSwapChain(
      IDXGIFactory1 * pFactory, HWND hWnd, const DXGI_SWAP_CHAIN_DESC1 *pDesc,
      const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc,
      IDXGISwapChain1 **ppSwapChain) = 0;
};

struct MTLDXGI_MONITOR_DATA;
namespace dxmt {
class MTLDXGIMonitor;
};

DEFINE_COM_INTERFACE("8f804acf-f020-4914-98d4-a951da684b7f", IMTLDXGIMonitor)
    : public IUnknown {
  virtual HRESULT STDMETHODCALLTYPE SetMonitorData(const MTLDXGI_MONITOR_DATA *pData) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetMonitorData(MTLDXGI_MONITOR_DATA **ppData) = 0;
};

DEFINE_COM_INTERFACE("413dae46-5707-46e1-b66d-6e58b9f15113", IMTLDXGIFactory)
    : public IDXGIFactory6 {
  virtual dxmt::MTLDXGIMonitor * STDMETHODCALLTYPE GetMonitor() = 0;
};

static constexpr IID DXMT_NVEXT_GUID = dxmt::guid::make_guid("ba0af616-4a43-4259-815c-db3b89829905");

namespace dxmt {
enum class VendorExtension {
  None,
  Nvidia,
};

extern VendorExtension g_extension_enabled;
} // namespace dxmt
