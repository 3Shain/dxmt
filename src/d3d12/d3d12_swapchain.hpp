#pragma once

#include "com/com_pointer.hpp"
#include "dxgi_interfaces.h"
#include "d3d12.h"
#include "dxgi_object.hpp"
#include "Metal.hpp"
#include <atomic>

namespace dxmt {

class MTLD3D12Device;
class MTLD3D12Resource;

class MTLD3D12SwapChain : public IDXGISwapChain1 {
public:
  MTLD3D12SwapChain(IDXGIFactory1 *factory, MTLD3D12Device *device,
                    IMTLDXGIDevice *dxgi_device, HWND hWnd,
                    const DXGI_SWAP_CHAIN_DESC1 *desc,
                    const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *fullscreen_desc);
  ~MTLD3D12SwapChain();

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) override;
  ULONG STDMETHODCALLTYPE AddRef() override;
  ULONG STDMETHODCALLTYPE Release() override;

  HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID Name, UINT *pDataSize, void *pData) override;
  HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID Name, UINT DataSize, const void *pData) override;
  HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID Name, const IUnknown *pUnknown) override;
  HRESULT STDMETHODCALLTYPE GetParent(REFIID riid, void **ppParent) override;
  HRESULT STDMETHODCALLTYPE GetDevice(REFIID riid, void **device) override;

  HRESULT STDMETHODCALLTYPE Present(UINT sync_interval, UINT flags) override;
  HRESULT STDMETHODCALLTYPE GetBuffer(UINT buffer_idx, REFIID riid, void **surface) override;
  HRESULT STDMETHODCALLTYPE SetFullscreenState(BOOL fullscreen, IDXGIOutput *target) override;
  HRESULT STDMETHODCALLTYPE GetFullscreenState(BOOL *fullscreen, IDXGIOutput **target) override;
  HRESULT STDMETHODCALLTYPE GetDesc(DXGI_SWAP_CHAIN_DESC *desc) override;
  HRESULT STDMETHODCALLTYPE ResizeBuffers(UINT buffer_count, UINT width, UINT height,
                                          DXGI_FORMAT format, UINT flags) override;
  HRESULT STDMETHODCALLTYPE ResizeTarget(const DXGI_MODE_DESC *new_target_params) override;
  HRESULT STDMETHODCALLTYPE GetContainingOutput(IDXGIOutput **output) override;
  HRESULT STDMETHODCALLTYPE GetFrameStatistics(DXGI_FRAME_STATISTICS *stats) override;
  HRESULT STDMETHODCALLTYPE GetLastPresentCount(UINT *last_present_count) override;

  HRESULT STDMETHODCALLTYPE GetDesc1(DXGI_SWAP_CHAIN_DESC1 *desc) override;
  HRESULT STDMETHODCALLTYPE GetFullscreenDesc(DXGI_SWAP_CHAIN_FULLSCREEN_DESC *desc) override;
  HRESULT STDMETHODCALLTYPE GetHwnd(HWND *hWnd) override;
  HRESULT STDMETHODCALLTYPE GetCoreWindow(REFIID riid, void **core_window) override;
  HRESULT STDMETHODCALLTYPE Present1(UINT sync_interval, UINT flags,
                                     const DXGI_PRESENT_PARAMETERS *params) override;
  WINBOOL STDMETHODCALLTYPE IsTemporaryMonoSupported() override;
  HRESULT STDMETHODCALLTYPE GetRestrictToOutput(IDXGIOutput **output) override;
  HRESULT STDMETHODCALLTYPE SetBackgroundColor(const DXGI_RGBA *color) override;
  HRESULT STDMETHODCALLTYPE GetBackgroundColor(DXGI_RGBA *color) override;
  HRESULT STDMETHODCALLTYPE SetRotation(DXGI_MODE_ROTATION rotation) override;
  HRESULT STDMETHODCALLTYPE GetRotation(DXGI_MODE_ROTATION *rotation) override;

private:
  Com<IDXGIFactory1> m_factory;
  Com<IMTLDXGIDevice> m_dxgi_device;
  MTLD3D12Device *m_device;
  HWND m_hwnd;
  DXGI_SWAP_CHAIN_DESC1 m_desc;
  DXGI_SWAP_CHAIN_FULLSCREEN_DESC m_fs_desc;
  Com<MTLD3D12Resource> m_backbuffer;
  obj_handle_t m_native_view = NULL_OBJECT_HANDLE;
  WMT::MetalLayer m_layer{};
  uint64_t m_present_count = 0;
  std::atomic<uint32_t> m_refCount = {1ul};
};

HRESULT CreateD3D12SwapChain(IDXGIFactory1 *factory, MTLD3D12Device *device,
                             IMTLDXGIDevice *dxgi_device, HWND hWnd,
                             const DXGI_SWAP_CHAIN_DESC1 *desc,
                             const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *fs_desc,
                             IDXGISwapChain1 **pp_swap_chain);

} // namespace dxmt
