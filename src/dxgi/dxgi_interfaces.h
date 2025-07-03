#pragma once

#include <dxgi1_6.h>
#include "Metal.hpp"
#include "com/com_guid.hpp"

DEFINE_COM_INTERFACE("acdf3ef1-b33a-4cb6-97bd-1c1974827e6d", IMTLDXGIAdapter)
    : public IDXGIAdapter4 {
  virtual WMT::Device STDMETHODCALLTYPE GetMTLDevice() = 0;
};

#ifdef DXMT_NATIVE
DEFINE_GUID(IID_IMTLDXGIAdapter, 0xacdf3ef1, 0xb33a, 0x4cb6, 0x97, 0xbd, 0x1c, 0x19, 0x74, 0x82, 0x7e, 0x6d);
__CRT_UUID_DECL(IMTLDXGIAdapter, 0xacdf3ef1, 0xb33a, 0x4cb6, 0x97, 0xbd, 0x1c, 0x19, 0x74, 0x82, 0x7e, 0x6d);
#endif

DEFINE_COM_INTERFACE("6bfa1657-9cb1-471a-a4fb-7cacf8a81207", IMTLDXGIDevice)
    : public IDXGIDevice3 {
  virtual WMT::Device STDMETHODCALLTYPE GetMTLDevice() = 0;
  virtual HRESULT STDMETHODCALLTYPE CreateSwapChain(
      IDXGIFactory1 * pFactory, HWND hWnd, const DXGI_SWAP_CHAIN_DESC1 *pDesc,
      const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc,
      IDXGISwapChain1 **ppSwapChain) = 0;
};

#ifdef DXMT_NATIVE
DEFINE_GUID(IID_IMTLDXGIDevice, 0x6bfa1657, 0x9cb1, 0x471a, 0xa4, 0xfb, 0x7c, 0xac, 0xf8, 0xa8, 0x12, 0x07);
__CRT_UUID_DECL(IMTLDXGIDevice, 0x6bfa1657, 0x9cb1, 0x471a, 0xa4, 0xfb, 0x7c, 0xac, 0xf8, 0xa8, 0x12, 0x07);
#endif

static constexpr IID DXMT_NVEXT_GUID = dxmt::guid::make_guid("ba0af616-4a43-4259-815c-db3b89829905");

namespace dxmt {
enum class VendorExtension {
  None,
  Nvidia,
};

extern VendorExtension g_extension_enabled;
} // namespace dxmt
