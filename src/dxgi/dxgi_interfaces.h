#pragma once

#include <dxgi1_6.h>
#include "Metal/MTLDevice.hpp"
#include "QuartzCore/CAMetalLayer.hpp"
#include "com/com_guid.hpp"

DEFINE_COM_INTERFACE("acdf3ef1-b33a-4cb6-97bd-1c1974827e6d", IMTLDXGIAdapter)
    : public IDXGIAdapter4 {
  virtual MTL::Device *STDMETHODCALLTYPE GetMTLDevice() = 0;
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

struct MTLDXGI_GAMMA_DATA;

DEFINE_COM_INTERFACE("8f804acf-f020-4914-98d4-a951da684b7f", IMTLDXGIMonitor)
    : public IUnknown {
  virtual HRESULT STDMETHODCALLTYPE GetDisplayId(HWND hWnd, UINT *pDisplayId) = 0;
  virtual HRESULT STDMETHODCALLTYPE AttachGammaTable(MTLDXGI_GAMMA_DATA *pGammaData) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetAttachedGammaTable(MTLDXGI_GAMMA_DATA **pGammaData) = 0;
  virtual HRESULT STDMETHODCALLTYPE SetGammaCurve(UINT DisplayId, DXGI_RGB *pGammaCurve, UINT NumPoints) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetGammaCurve(UINT DisplayId, DXGI_RGB *pGammaCurve, UINT *pNumPoints) = 0;
  virtual VOID STDMETHODCALLTYPE GetNumGammaCurvePoints(UINT DisplayId, UINT *pNumPoints) = 0;
};
