#pragma once

#include <dxgi1_2.h>
#include "Metal/MTLDevice.hpp"
#include "Metal/MTLPixelFormat.hpp"
#include "Metal/MTLStageInputOutputDescriptor.hpp"
#include "Metal/MTLVertexDescriptor.hpp"
#include "QuartzCore/CAMetalLayer.hpp"

struct METAL_FORMAT_DESC {
  MTL::PixelFormat PixelFormat;
  MTL::AttributeFormat AttributeFormat;
  MTL::VertexFormat VertexFormat;
  UINT Stride;
  UINT Alignment;
  BOOL IsCompressed;
};

MIDL_INTERFACE("acdf3ef1-b33a-4cb6-97bd-1c1974827e6d")
IMTLDXGIAdatper : public IDXGIAdapter2 {
  virtual MTL::Device *STDMETHODCALLTYPE GetMTLDevice() = 0;
  virtual HRESULT STDMETHODCALLTYPE QueryFormatDesc(DXGI_FORMAT Format, METAL_FORMAT_DESC* pMtlFormatDesc) = 0;
};
__CRT_UUID_DECL(IMTLDXGIAdatper, 0xacdf3ef1, 0xb33a, 0x4cb6, 0x97, 0xbd, 0x1c,
                0x19, 0x74, 0x82, 0x7e, 0x6d);

// MIDL_INTERFACE("5cc49399-ead6-44b3-af53-0c0ec1a8a35e")
// IMTLDXGISwapChainBuffer : public IUnknown {
//   virtual void SetColorAttachment(MTL::RenderPassColorAttachmentDescriptor *
//                                   colorAttachment) = 0;
// };
// __CRT_UUID_DECL(IMTLDXGISwapChainBuffer, 0x5cc49399, 0xead6, 0x44b3, 0xaf, 0x53,
//                 0x0c, 0x0e, 0xc1, 0xa8, 0xa3, 0x5e);

MIDL_INTERFACE("f2be9c71-9c4b-42d3-b46b-bbfd459db688")
IDXGIMetalLayerFactory : public IUnknown {
  virtual HRESULT STDMETHODCALLTYPE GetMetalLayerFromHwnd(
      HWND hWnd, CA::MetalLayer * *ppMetalLayer, void **ppNativeView) = 0;
  virtual HRESULT ReleaseMetalLayer(HWND hWnd, void *ppNativeView) = 0;
};
__CRT_UUID_DECL(IDXGIMetalLayerFactory, 0xf2be9c71, 0x9c4b, 0x42d3, 0xb4, 0x6b,
                0xbb, 0xfd, 0x45, 0x9d, 0xb6, 0x88);

MIDL_INTERFACE("6bfa1657-9cb1-471a-a4fb-7cacf8a81207")
IMTLDXGIDevice : public IDXGIDevice2 {
  virtual MTL::Device *STDMETHODCALLTYPE GetMTLDevice() = 0;
  virtual HRESULT STDMETHODCALLTYPE CreateSwapChain(
      IDXGIFactory1 * pFactory, HWND hWnd, const DXGI_SWAP_CHAIN_DESC1 *pDesc,
      const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc,
      IDXGISwapChain1 **ppSwapChain) = 0;
};
__CRT_UUID_DECL(IMTLDXGIDevice, 0x6bfa1657, 0x9cb1, 0x471a, 0xa4, 0xfb, 0x7c,
                0xac, 0xf8, 0xa8, 0x12, 0x07);