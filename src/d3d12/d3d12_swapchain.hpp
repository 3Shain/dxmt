#pragma once

#include "com/com_pointer.hpp"
#include "d3d12.h"
#include "dxgi_interfaces.h"
#include "dxmt_presenter.hpp"
#include "Metal.hpp"
#include <atomic>
#include <array>
#include <memory>

namespace dxmt {

class MTLD3D12Device;
class MTLD3D12Resource;

class MTLD3D12SwapChain : public IDXGISwapChain4 {
public:
  MTLD3D12SwapChain(IDXGIFactory1 *factory, MTLD3D12Device *device,
                     IMTLDXGIDevice *dxgi_device, HWND hWnd,
                     const DXGI_SWAP_CHAIN_DESC1 *desc,
                     const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *fs_desc);
  ~MTLD3D12SwapChain();

  /*** IUnknown ***/
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) override;
  ULONG STDMETHODCALLTYPE AddRef() override;
  ULONG STDMETHODCALLTYPE Release() override;

  /*** IDXGIObject ***/
  HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID Name, UINT DataSize, const void *pData) override;
  HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID Name, const IUnknown *pUnknown) override;
  HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID Name, UINT *pDataSize, void *pData) override;
  HRESULT STDMETHODCALLTYPE GetParent(REFIID riid, void **ppParent) override;

  /*** IDXGIDeviceSubObject ***/
  HRESULT STDMETHODCALLTYPE GetDevice(REFIID riid, void **device) override;

  /*** IDXGISwapChain ***/
  HRESULT STDMETHODCALLTYPE Present(UINT SyncInterval, UINT Flags) override;
  HRESULT STDMETHODCALLTYPE GetBuffer(UINT Buffer, REFIID riid, void **ppSurface) override;
  HRESULT STDMETHODCALLTYPE SetFullscreenState(BOOL Fullscreen, IDXGIOutput *pTarget) override;
  HRESULT STDMETHODCALLTYPE GetFullscreenState(BOOL *pFullscreen, IDXGIOutput **ppTarget) override;
  HRESULT STDMETHODCALLTYPE GetDesc(DXGI_SWAP_CHAIN_DESC *pDesc) override;
  HRESULT STDMETHODCALLTYPE ResizeBuffers(UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags) override;
  HRESULT STDMETHODCALLTYPE ResizeTarget(const DXGI_MODE_DESC *pNewTargetParameters) override;
  HRESULT STDMETHODCALLTYPE GetContainingOutput(IDXGIOutput **ppOutput) override;
  HRESULT STDMETHODCALLTYPE GetFrameStatistics(DXGI_FRAME_STATISTICS *pStats) override;
  HRESULT STDMETHODCALLTYPE GetLastPresentCount(UINT *pLastPresentCount) override;

  /*** IDXGISwapChain1 ***/
  HRESULT STDMETHODCALLTYPE GetDesc1(DXGI_SWAP_CHAIN_DESC1 *pDesc) override;
  HRESULT STDMETHODCALLTYPE GetFullscreenDesc(DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pDesc) override;
  HRESULT STDMETHODCALLTYPE GetHwnd(HWND *pHwnd) override;
  HRESULT STDMETHODCALLTYPE GetCoreWindow(REFIID riid, void **ppUnk) override;
  HRESULT STDMETHODCALLTYPE Present1(UINT SyncInterval, UINT PresentFlags, const DXGI_PRESENT_PARAMETERS *pPresentParameters) override;

  WINBOOL STDMETHODCALLTYPE IsTemporaryMonoSupported() override;
  HRESULT STDMETHODCALLTYPE GetRestrictToOutput(IDXGIOutput **ppOutput) override;
  HRESULT STDMETHODCALLTYPE SetBackgroundColor(const DXGI_RGBA *pColor) override;
  HRESULT STDMETHODCALLTYPE GetBackgroundColor(DXGI_RGBA *pColor) override;
  HRESULT STDMETHODCALLTYPE SetRotation(DXGI_MODE_ROTATION Rotation) override;
  HRESULT STDMETHODCALLTYPE GetRotation(DXGI_MODE_ROTATION *pRotation) override;

  /*** IDXGISwapChain2 ***/
  HRESULT STDMETHODCALLTYPE SetSourceSize(UINT Width, UINT Height) override;
  HRESULT STDMETHODCALLTYPE GetSourceSize(UINT *pWidth, UINT *pHeight) override;
  HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency(UINT MaxLatency) override;
  HRESULT STDMETHODCALLTYPE GetMaximumFrameLatency(UINT *pMaxLatency) override;

  HANDLE STDMETHODCALLTYPE GetFrameLatencyWaitableObject() override;
  HRESULT STDMETHODCALLTYPE SetMatrixTransform(const DXGI_MATRIX_3X2_F *pMatrix) override;
  HRESULT STDMETHODCALLTYPE GetMatrixTransform(DXGI_MATRIX_3X2_F *pMatrix) override;

  /*** IDXGISwapChain3 ***/
  UINT STDMETHODCALLTYPE GetCurrentBackBufferIndex() override;
  HRESULT STDMETHODCALLTYPE CheckColorSpaceSupport(DXGI_COLOR_SPACE_TYPE ColorSpace, UINT *pColorSpaceSupport) override;
  HRESULT STDMETHODCALLTYPE SetColorSpace1(DXGI_COLOR_SPACE_TYPE ColorSpace) override;
  HRESULT STDMETHODCALLTYPE ResizeBuffers1(UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT Format, UINT SwapChainFlags, const UINT *pCreationNodeMask, IUnknown *const *ppPresentQueue) override;

  /*** IDXGISwapChain4 ***/
  HRESULT STDMETHODCALLTYPE SetHDRMetaData(DXGI_HDR_METADATA_TYPE Type, UINT Size, void *pMetaData) override;

private:
  void ConfigureLayer();

  std::atomic<uint32_t> m_refCount = {1ul};
  Com<IDXGIFactory1> m_factory;
  Com<IMTLDXGIDevice> m_dxgi_device;
  MTLD3D12Device *m_device = nullptr;
  HWND m_hwnd = nullptr;
  WMT::MetalLayer m_layer = {};
  WMT::Object m_native_view;
  DXGI_SWAP_CHAIN_DESC1 m_desc = {};
  DXGI_SWAP_CHAIN_FULLSCREEN_DESC m_fs_desc = {};
  uint64_t m_present_count = 0;

  std::array<Com<MTLD3D12Resource>, 4> m_backbuffers;
  uint32_t m_current_buffer = 0;
  WMT::Reference<WMT::CommandQueue> m_present_queue;
  std::unique_ptr<InternalCommandLibrary> m_present_library;
  Rc<Presenter> m_presenter;
};

HRESULT CreateD3D12SwapChain(IDXGIFactory1 *factory, MTLD3D12Device *device,
                              IMTLDXGIDevice *dxgi_device, HWND hWnd,
                              const DXGI_SWAP_CHAIN_DESC1 *desc,
                              const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *fs_desc,
                              IDXGISwapChain1 **pp_swap_chain);

} // namespace dxmt
