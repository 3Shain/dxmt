#pragma once

#include <dxgi1_6.h>
#include "Metal/MTLDevice.hpp"
#include "Metal/MTLPixelFormat.hpp"
#include "Metal/MTLStageInputOutputDescriptor.hpp"
#include "Metal/MTLVertexDescriptor.hpp"
#include "QuartzCore/CAMetalLayer.hpp"
#include "com/com_guid.hpp"
#include "dxmt_format.hpp"

struct MTL_FORMAT_DESC {
  MTL::PixelFormat PixelFormat;
  MTL::AttributeFormat AttributeFormat;
  MTL::VertexFormat VertexFormat;
  dxmt::FormatCapability Capability;
  union {
    UINT BytesPerTexel;
    UINT BlockSize;
  };
  BOOL IsCompressed;
  BOOL SupportBackBuffer;
  UINT DepthStencilFlag; // 0 - none, 1 - depth, 2 - stencil
  BOOL Typeless;
};

DEFINE_COM_INTERFACE("acdf3ef1-b33a-4cb6-97bd-1c1974827e6d", IMTLDXGIAdatper)
    : public IDXGIAdapter3 {
  virtual MTL::Device *STDMETHODCALLTYPE GetMTLDevice() = 0;
  virtual HRESULT STDMETHODCALLTYPE QueryFormatDesc(
      DXGI_FORMAT Format, MTL_FORMAT_DESC * pMtlFormatDesc) = 0;
  virtual int STDMETHODCALLTYPE GetConfigInt(const char* name, int defaultValue) = 0;
  virtual float STDMETHODCALLTYPE GetConfigFloat(const char* name, float defaultValue) = 0;
};

DEFINE_COM_INTERFACE("6bfa1657-9cb1-471a-a4fb-7cacf8a81207", IMTLDXGIDevice)
    : public IDXGIDevice3 {
  virtual MTL::Device *STDMETHODCALLTYPE GetMTLDevice() = 0;
  virtual HRESULT STDMETHODCALLTYPE CreateSwapChain(
      IDXGIFactory1 * pFactory, HWND hWnd, const DXGI_SWAP_CHAIN_DESC1 *pDesc,
      const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc,
      IDXGISwapChain1 **ppSwapChain) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetMetalLayerFromHwnd(
      HWND hWnd, CA::MetalLayer * *ppMetalLayer, void **ppNativeView) = 0;
  virtual HRESULT ReleaseMetalLayer(HWND hWnd, void *ppNativeView) = 0;
};
