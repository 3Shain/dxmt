#include "d3d12_swapchain.hpp"
#include "d3d12_device.hpp"
#include "d3d12_resource.hpp"
#include "log/log.hpp"
#include "util_string.hpp"
#include "Metal.hpp"

#define SCTRACE(fmt, ...) do { FILE *_tf = fopen("Z:\\tmp\\dxmt_dxgi_trace.log", "a"); if (_tf) { fprintf(_tf, "SwapChain::" fmt "\n", ##__VA_ARGS__); fclose(_tf); } } while(0)

namespace dxmt {

static WMTPixelFormat DXGIToMTL(DXGI_FORMAT fmt) {
  switch (fmt) {
  case DXGI_FORMAT_R8G8B8A8_UNORM: return WMTPixelFormatRGBA8Unorm;
  case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return WMTPixelFormatRGBA8Unorm_sRGB;
  case DXGI_FORMAT_B8G8R8A8_UNORM: return WMTPixelFormatBGRA8Unorm;
  case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return WMTPixelFormatBGRA8Unorm_sRGB;
  case DXGI_FORMAT_R16G16B16A16_FLOAT: return WMTPixelFormatRGBA16Float;
  case DXGI_FORMAT_R10G10B10A2_UNORM: return WMTPixelFormatRGB10A2Unorm;
  default: return WMTPixelFormatBGRA8Unorm;
  }
}

MTLD3D12SwapChain::MTLD3D12SwapChain(
    IDXGIFactory1 *factory, MTLD3D12Device *device,
    IMTLDXGIDevice *dxgi_device, HWND hWnd,
    const DXGI_SWAP_CHAIN_DESC1 *desc,
    const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *fs_desc)
    : m_factory(factory), m_dxgi_device(dxgi_device), m_device(device),
      m_hwnd(hWnd), m_desc(*desc) {
  if (m_factory)
    m_factory->AddRef();
  if (m_dxgi_device)
    m_dxgi_device->AddRef();
  if (m_device)
    m_device->AddRef();

  if (fs_desc) {
    m_fs_desc = *fs_desc;
  } else {
    m_fs_desc = {};
    m_fs_desc.Windowed = true;
  }

  m_native_view = WMT::CreateMetalViewFromHWND(
      (intptr_t)hWnd, dxgi_device->GetMTLDevice(), m_layer);

  auto wmt_dev = dxgi_device->GetMTLDevice();
  m_present_queue = wmt_dev.newCommandQueue(1);

  ResizeBuffers(0, m_desc.Width, m_desc.Height, m_desc.Format, m_desc.Flags);
  Logger::info(str::format("D3D12SwapChain: ", m_desc.Width, "x", m_desc.Height,
                            " fmt=", m_desc.Format, " hwnd=", (void*)hWnd));
}

MTLD3D12SwapChain::~MTLD3D12SwapChain() {
  for (uint32_t i = 0; i < 4; i++)
    m_backbuffers[i] = nullptr;
  if (m_native_view.handle)
    WMT::ReleaseMetalView(m_native_view);
  if (m_device)
    m_device->Release();
}

HRESULT STDMETHODCALLTYPE
MTLD3D12SwapChain::QueryInterface(REFIID riid, void **ppv) {
  if (!ppv)
    return E_POINTER;
  *ppv = nullptr;

  if (riid == __uuidof(IUnknown) || riid == __uuidof(IDXGIObject) ||
      riid == __uuidof(IDXGIDeviceSubObject) ||
      riid == __uuidof(IDXGISwapChain) || riid == __uuidof(IDXGISwapChain1) ||
      riid == __uuidof(IDXGISwapChain2) || riid == __uuidof(IDXGISwapChain3) ||
      riid == __uuidof(IDXGISwapChain4)) {
    *ppv = ref(this);
    return S_OK;
  }
  return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE MTLD3D12SwapChain::AddRef() { return ++m_refCount; }

ULONG STDMETHODCALLTYPE MTLD3D12SwapChain::Release() {
  uint32_t rc = --m_refCount;
  if (!rc)
    delete this;
  return rc;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12SwapChain::GetPrivateData(REFGUID Name, UINT *pDataSize, void *pData) {
  SCTRACE("GetPrivateData E_NOTIMPL");
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12SwapChain::SetPrivateData(REFGUID Name, UINT DataSize, const void *pData) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12SwapChain::SetPrivateDataInterface(REFGUID Name, const IUnknown *pUnknown) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12SwapChain::GetDevice(REFIID riid, void **device) {
  return m_device->QueryInterface(riid, device);
}

HRESULT STDMETHODCALLTYPE
MTLD3D12SwapChain::GetParent(REFIID riid, void **ppParent) {
  return m_factory->QueryInterface(riid, ppParent);
}

HRESULT STDMETHODCALLTYPE
MTLD3D12SwapChain::Present(UINT sync_interval, UINT flags) {
  SCTRACE("Present sync=%u flags=0x%x", sync_interval, flags);
  return Present1(sync_interval, flags, nullptr);
}

HRESULT STDMETHODCALLTYPE
MTLD3D12SwapChain::GetBuffer(UINT buffer_idx, REFIID riid, void **surface) {
  SCTRACE("GetBuffer idx=%u", buffer_idx);
  if (!surface)
    return E_POINTER;
  if (buffer_idx >= 4 || !m_backbuffers[buffer_idx]) {
    SCTRACE("GetBuffer idx=%u FAILED (no buffer)", buffer_idx);
    return DXGI_ERROR_INVALID_CALL;
  }
  return m_backbuffers[buffer_idx]->QueryInterface(riid, surface);
}

HRESULT STDMETHODCALLTYPE
MTLD3D12SwapChain::SetFullscreenState(BOOL fullscreen, IDXGIOutput *target) {
  m_fs_desc.Windowed = !fullscreen;
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12SwapChain::GetFullscreenState(BOOL *fullscreen, IDXGIOutput **target) {
  if (fullscreen)
    *fullscreen = !m_fs_desc.Windowed;
  if (target)
    *target = nullptr;
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12SwapChain::GetDesc(DXGI_SWAP_CHAIN_DESC *desc) {
  desc->BufferDesc.Width = m_desc.Width;
  desc->BufferDesc.Height = m_desc.Height;
  desc->BufferDesc.RefreshRate = m_fs_desc.RefreshRate;
  desc->BufferDesc.Format = m_desc.Format;
  desc->BufferDesc.ScanlineOrdering = m_fs_desc.ScanlineOrdering;
  desc->BufferDesc.Scaling = m_fs_desc.Scaling;
  desc->SampleDesc = m_desc.SampleDesc;
  desc->BufferUsage = m_desc.BufferUsage;
  desc->BufferCount = m_desc.BufferCount;
  desc->OutputWindow = m_hwnd;
  desc->Windowed = m_fs_desc.Windowed;
  desc->SwapEffect = m_desc.SwapEffect;
  desc->Flags = m_desc.Flags;
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12SwapChain::ResizeBuffers(UINT buffer_count, UINT width, UINT height,
                                 DXGI_FORMAT format, UINT flags) {
  for (uint32_t i = 0; i < 4; i++)
    m_backbuffers[i] = nullptr;

  if (buffer_count)
    m_desc.BufferCount = buffer_count;
  if (format != DXGI_FORMAT_UNKNOWN)
    m_desc.Format = format;

  if (width == 0 || height == 0) {
    RECT rect;
    if (GetClientRect(m_hwnd, &rect)) {
      width = rect.right - rect.left;
      height = rect.bottom - rect.top;
    }
    if (width == 0)
      width = 1;
    if (height == 0)
      height = 1;
  }
  m_desc.Width = width;
  m_desc.Height = height;

  D3D12_RESOURCE_DESC res_desc = {};
  res_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  res_desc.Width = m_desc.Width;
  res_desc.Height = m_desc.Height;
  res_desc.DepthOrArraySize = 1;
  res_desc.MipLevels = 1;
  res_desc.Format = m_desc.Format;
  res_desc.SampleDesc.Count = 1;
  res_desc.SampleDesc.Quality = 0;
  res_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
  res_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

  D3D12_HEAP_PROPERTIES heap_props = {};
  heap_props.Type = D3D12_HEAP_TYPE_DEFAULT;

  uint32_t count = m_desc.BufferCount ? m_desc.BufferCount : 2;
  if (count > 4) count = 4;
  for (uint32_t i = 0; i < count; i++) {
    m_backbuffers[i] = new MTLD3D12Resource(m_device, res_desc,
                                             D3D12_RESOURCE_STATE_RENDER_TARGET,
                                             heap_props);
  }
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12SwapChain::ResizeTarget(const DXGI_MODE_DESC *new_target_params) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12SwapChain::GetContainingOutput(IDXGIOutput **output) {
  SCTRACE("GetContainingOutput E_NOTIMPL");
  *output = nullptr;
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12SwapChain::GetFrameStatistics(DXGI_FRAME_STATISTICS *stats) {
  if (stats)
    memset(stats, 0, sizeof(*stats));
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12SwapChain::GetLastPresentCount(UINT *last_present_count) {
  if (last_present_count)
    *last_present_count = (UINT)m_present_count;
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12SwapChain::GetDesc1(DXGI_SWAP_CHAIN_DESC1 *desc) {
  *desc = m_desc;
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12SwapChain::GetFullscreenDesc(DXGI_SWAP_CHAIN_FULLSCREEN_DESC *desc) {
  *desc = m_fs_desc;
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12SwapChain::GetHwnd(HWND *hWnd) {
  if (hWnd)
    *hWnd = m_hwnd;
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12SwapChain::GetCoreWindow(REFIID riid, void **core_window) {
  SCTRACE("GetCoreWindow E_NOTIMPL");
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12SwapChain::Present1(UINT sync_interval, UINT flags,
                            const DXGI_PRESENT_PARAMETERS *params) {
  m_present_count++;

  if (!m_backbuffers[m_current_buffer])
    return S_OK;

  if (!m_present_queue.handle) {
    auto wmt_device = m_dxgi_device->GetMTLDevice();
    m_present_queue = wmt_device.newCommandQueue(1);
  }

  auto cmdbuf = m_present_queue.commandBuffer();

  auto drawable = m_layer.nextDrawable();
  if (!drawable.handle) {
    cmdbuf.commit();
    return S_OK;
  }

  auto dst_texture = drawable.texture();
  auto *res = static_cast<MTLD3D12Resource *>(m_backbuffers[m_current_buffer].ptr());
  auto src_texture = res->GetMTLTexture();

  if (src_texture.handle && dst_texture.handle) {
    auto blit = cmdbuf.blitCommandEncoder();
    struct wmtcmd_blit_copy_from_texture_to_texture copy = {};
    copy.type = WMTBlitCommandCopyFromTextureToTexture;
    copy.next.set(nullptr);
    copy.src = src_texture;
    copy.src_slice = 0;
    copy.src_level = 0;
    copy.src_origin = {0, 0, 0};
    copy.src_size = {m_desc.Width, m_desc.Height, 1};
    copy.dst = dst_texture;
    copy.dst_slice = 0;
    copy.dst_level = 0;
    copy.dst_origin = {0, 0, 0};
    blit.encodeCommands(reinterpret_cast<const wmtcmd_blit_nop *>(&copy));
    blit.endEncoding();
  }

  cmdbuf.presentDrawable(drawable);
  cmdbuf.commit();

  m_current_buffer = (m_current_buffer + 1) % (m_desc.BufferCount ? m_desc.BufferCount : 2);

  return S_OK;
}

WINBOOL STDMETHODCALLTYPE MTLD3D12SwapChain::IsTemporaryMonoSupported() {
  return false;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12SwapChain::GetRestrictToOutput(IDXGIOutput **output) {
  *output = nullptr;
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12SwapChain::SetBackgroundColor(const DXGI_RGBA *color) { return S_OK; }

HRESULT STDMETHODCALLTYPE
MTLD3D12SwapChain::GetBackgroundColor(DXGI_RGBA *color) { return S_OK; }

HRESULT STDMETHODCALLTYPE
MTLD3D12SwapChain::SetRotation(DXGI_MODE_ROTATION rotation) { return S_OK; }

HRESULT STDMETHODCALLTYPE
MTLD3D12SwapChain::GetRotation(DXGI_MODE_ROTATION *rotation) {
  if (rotation)
    *rotation = DXGI_MODE_ROTATION_IDENTITY;
  return S_OK;
}

HRESULT STDMETHODCALLTYPE MTLD3D12SwapChain::SetSourceSize(UINT Width, UINT Height) { return S_OK; }
HRESULT STDMETHODCALLTYPE MTLD3D12SwapChain::GetSourceSize(UINT *pWidth, UINT *pHeight) { return S_OK; }
HRESULT STDMETHODCALLTYPE MTLD3D12SwapChain::SetMaximumFrameLatency(UINT MaxLatency) { return S_OK; }
HRESULT STDMETHODCALLTYPE MTLD3D12SwapChain::GetMaximumFrameLatency(UINT *pMaxLatency) { if (pMaxLatency) *pMaxLatency = 1; return S_OK; }
HANDLE STDMETHODCALLTYPE MTLD3D12SwapChain::GetFrameLatencyWaitableObject() { return nullptr; }
HRESULT STDMETHODCALLTYPE MTLD3D12SwapChain::SetMatrixTransform(const DXGI_MATRIX_3X2_F *pMatrix) { return S_OK; }
HRESULT STDMETHODCALLTYPE MTLD3D12SwapChain::GetMatrixTransform(DXGI_MATRIX_3X2_F *pMatrix) { return S_OK; }
UINT STDMETHODCALLTYPE MTLD3D12SwapChain::GetCurrentBackBufferIndex() { return m_current_buffer; }
HRESULT STDMETHODCALLTYPE MTLD3D12SwapChain::CheckColorSpaceSupport(DXGI_COLOR_SPACE_TYPE ColorSpace, UINT *pSupport) { if (pSupport) *pSupport = 0; return S_OK; }
HRESULT STDMETHODCALLTYPE MTLD3D12SwapChain::SetColorSpace1(DXGI_COLOR_SPACE_TYPE ColorSpace) { return S_OK; }
HRESULT STDMETHODCALLTYPE MTLD3D12SwapChain::ResizeBuffers1(UINT, UINT, UINT, DXGI_FORMAT, UINT, const UINT *, IUnknown *const *) { return S_OK; }
HRESULT STDMETHODCALLTYPE MTLD3D12SwapChain::SetHDRMetaData(DXGI_HDR_METADATA_TYPE, UINT, void *) { return S_OK; }

HRESULT CreateD3D12SwapChain(IDXGIFactory1 *factory, MTLD3D12Device *device,
                             IMTLDXGIDevice *dxgi_device, HWND hWnd,
                             const DXGI_SWAP_CHAIN_DESC1 *desc,
                             const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *fs_desc,
                             IDXGISwapChain1 **pp_swap_chain) {
  if (!pp_swap_chain)
    return E_POINTER;
  *pp_swap_chain = nullptr;

  auto swapchain = new MTLD3D12SwapChain(factory, device, dxgi_device, hWnd,
                                          desc, fs_desc);
  HRESULT hr = swapchain->QueryInterface(IID_PPV_ARGS(pp_swap_chain));
  if (FAILED(hr))
    swapchain->Release();
  return hr;
}

} // namespace dxmt
