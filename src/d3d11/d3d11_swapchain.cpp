#include "d3d11_swapchain.hpp"
#include "com/com_guid.hpp"
#include "d3d11_private.h"
#include "dxgi_interfaces.h"
#include "dxgi_object.hpp"
#include "d3d11_context.hpp"
#include "log/log.hpp"
#include "mtld11_resource.hpp"
#include "objc_pointer.hpp"
#include "util_error.hpp"
#include "d3d11_device.hpp"

#include "objc-wrapper/dispatch.h"
#include "util_string.hpp"
#include "wsi_window.hpp"

/**
Metal support at most 3 swapchain
*/
constexpr size_t kSwapchainLatency = 3;

template <typename F> class DestructorWrapper {
  F f;

public:
  DestructorWrapper(F &&f) : f(std::forward<F>(f)) {}
  ~DestructorWrapper() { std::invoke(f); };
};

namespace dxmt {

Com<IMTLD3D11BackBuffer>
CreateEmulatedBackBuffer(IMTLD3D11Device *pDevice,
                         const DXGI_SWAP_CHAIN_DESC1 *pDesc, HWND hWnd);

class MTLD3D11SwapChain final : public MTLDXGISubObject<IDXGISwapChain2> {
public:
  MTLD3D11SwapChain(IDXGIFactory1 *pFactory, IMTLDXGIDevice *pDevice, HWND hWnd,
                    const DXGI_SWAP_CHAIN_DESC1 *pDesc,
                    const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc)
      : MTLDXGISubObject(pDevice), factory_(pFactory), presentation_count_(0),
        desc_(*pDesc), hWnd(hWnd) {

    device_ = Com<IMTLD3D11Device>::queryFrom(pDevice);

    Com<ID3D11DeviceContext1> context;
    device_->GetImmediateContext1(&context);
    context->QueryInterface(IID_PPV_ARGS(&device_context_));

    present_semaphore_ =
        CreateSemaphore(nullptr, kSwapchainLatency, kSwapchainLatency, nullptr);

    if (desc_.Width == 0 || desc_.Height == 0) {
      wsi::getWindowSize(hWnd, &desc_.Width, &desc_.Height);
    }

    if (pFullscreenDesc) {
      fullscreen_desc_ = *pFullscreenDesc;
    } else {
      fullscreen_desc_.Windowed = true;
    }
  };

  ~MTLD3D11SwapChain() { CloseHandle(present_semaphore_); };

  HRESULT
  STDMETHODCALLTYPE
  QueryInterface(REFIID riid, void **ppvObject) final {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(IDXGIObject) ||
        riid == __uuidof(IDXGIDeviceSubObject) ||
        riid == __uuidof(IDXGISwapChain) || riid == __uuidof(IDXGISwapChain1) ||
        riid == __uuidof(IDXGISwapChain2)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(IDXGISwapChain1), riid)) {
      WARN("DXGISwapChain: Unknown interface query ", str::format(riid));
    }

    return E_NOINTERFACE;
  };

  HRESULT
  STDMETHODCALLTYPE
  GetParent(REFIID riid, void **parent) final {
    return factory_->QueryInterface(riid, parent);
  };

  HRESULT
  STDMETHODCALLTYPE
  Present(UINT sync_interval, UINT flags) final {
    return Present1(sync_interval, flags, nullptr);
  };

  HRESULT
  STDMETHODCALLTYPE
  GetBuffer(UINT buffer_idx, REFIID riid, void **surface) final {
    if (buffer_idx == 0) {
      if (!backbuffer_) {
        backbuffer_ = CreateEmulatedBackBuffer(device_.ptr(), &desc_, hWnd);
      }
      return backbuffer_->QueryInterface(riid, surface);
    } else {
      ERR("Non zero-index buffer is not supported");
      return E_FAIL;
    }
  };

  HRESULT
  STDMETHODCALLTYPE
  SetFullscreenState(BOOL fullscreen, IDXGIOutput *target) final {
    WARN("SetFullscreenState: stub");
    return S_OK;
  };

  HRESULT
  STDMETHODCALLTYPE
  GetFullscreenState(BOOL *fullscreen, IDXGIOutput **target) final {
    if (fullscreen) {
      *fullscreen = !fullscreen_desc_.Windowed;
    }
    if (target) {
      *target = NULL;
      // TODO
      WARN("GetFullscreenState return null");
    }
    return S_OK;
  };

  HRESULT
  STDMETHODCALLTYPE
  GetDesc(DXGI_SWAP_CHAIN_DESC *pDesc) final {
    if (!pDesc)
      return E_INVALIDARG;

    pDesc->BufferDesc.Width = desc_.Width;
    pDesc->BufferDesc.Height = desc_.Height;
    pDesc->BufferDesc.RefreshRate = fullscreen_desc_.RefreshRate;
    pDesc->BufferDesc.Format = desc_.Format;
    pDesc->BufferDesc.ScanlineOrdering = fullscreen_desc_.ScanlineOrdering;
    pDesc->BufferDesc.Scaling = fullscreen_desc_.Scaling;
    pDesc->SampleDesc = desc_.SampleDesc;
    pDesc->BufferUsage = desc_.BufferUsage;
    pDesc->BufferCount = desc_.BufferCount;
    pDesc->OutputWindow = hWnd;
    pDesc->Windowed = fullscreen_desc_.Windowed;
    pDesc->SwapEffect = desc_.SwapEffect;
    pDesc->Flags = desc_.Flags;
    return S_OK;
  };

  HRESULT
  STDMETHODCALLTYPE
  ResizeBuffers(UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT Format,
                UINT flags) final {
    // FIXME: ... weird
    // if (backbuffer_) {
    //   // required behavior...
    //   if (backbuffer_->AddRef() != 2) {
    //     ERR("ResizeBuffers: unreleased outstanding references to back "
    //         "buffers");
    //     backbuffer_->Release();
    //     return DXGI_ERROR_INVALID_CALL;
    //   }
    //   backbuffer_->Release();
    // }
    /* BufferCount ignored */
    if (Width == 0 || Height == 0) {
      wsi::getWindowSize(hWnd, &desc_.Width, &desc_.Height);
    } else {
      desc_.Width = Width;
      desc_.Height = Height;
    }
    if (Format != DXGI_FORMAT_UNKNOWN) {
      desc_.Format = Format;
    }
    device_context_->WaitUntilGPUIdle();
    backbuffer_ = nullptr;
    return S_OK;
  };

  HRESULT
  STDMETHODCALLTYPE
  ResizeTarget(const DXGI_MODE_DESC *pDesc) final {
    /* FIXME: check this out!*/
    return ResizeBuffers(0, pDesc->Width, pDesc->Height, pDesc->Format, 0);
  };

  HRESULT
  STDMETHODCALLTYPE
  GetContainingOutput(IDXGIOutput **output) final { IMPLEMENT_ME; };

  HRESULT
  STDMETHODCALLTYPE
  GetFrameStatistics(DXGI_FRAME_STATISTICS *stats) final { IMPLEMENT_ME; };

  HRESULT
  STDMETHODCALLTYPE
  GetLastPresentCount(UINT *last_present_count) final {
    if (last_present_count == NULL) {
      return E_POINTER;
    }
    *last_present_count = presentation_count_;
    return S_OK;
  };

  HRESULT
  STDMETHODCALLTYPE
  GetDesc1(DXGI_SWAP_CHAIN_DESC1 *pDesc) final {
    if (pDesc == NULL) {
      return E_POINTER;
    }
    *pDesc = desc_;
    return S_OK;
  };

  HRESULT
  STDMETHODCALLTYPE
  GetFullscreenDesc(DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pDesc) final {
    if (pDesc == NULL) {
      return E_POINTER;
    }
    *pDesc = fullscreen_desc_;
    return S_OK;
  };

  HRESULT
  STDMETHODCALLTYPE
  GetHwnd(HWND *pHwnd) final {
    if (pHwnd == NULL) {
      return E_POINTER;
    }
    *pHwnd = hWnd;
    return S_OK;
  };

  HRESULT
  STDMETHODCALLTYPE
  GetCoreWindow(REFIID refiid, void **ppUnk) final {
    ERR("Not implemented");
    return E_NOTIMPL;
  };

  HRESULT
  STDMETHODCALLTYPE
  Present1(UINT SyncInterval, UINT PresentFlags,
           const DXGI_PRESENT_PARAMETERS *pPresentParameters) final {

    device_context_->Flush2(
        // why transfer?
        // it works because the texture is in use, so drawable is valid during
        // encoding...
        [drawable = transfer(backbuffer_->CurrentDrawable()),
         _ = DestructorWrapper([sem = present_semaphore_]() {
           // called when cmdbuf complete
           ReleaseSemaphore(sem, 1, nullptr);
         })](MTL::CommandBuffer *cmdbuf) {
          if (drawable) {
            cmdbuf->presentDrawable(drawable.ptr());
          }
        });
    backbuffer_->Swap();

    presentation_count_ += 1;

    return S_OK;
  };

  BOOL STDMETHODCALLTYPE IsTemporaryMonoSupported() final { return FALSE; };

  HRESULT
  STDMETHODCALLTYPE
  GetRestrictToOutput(IDXGIOutput **ppRestrictToOutput) final { IMPLEMENT_ME; };

  HRESULT
  STDMETHODCALLTYPE
  SetBackgroundColor(const DXGI_RGBA *pColor) final { IMPLEMENT_ME; };

  HRESULT
  STDMETHODCALLTYPE
  GetBackgroundColor(DXGI_RGBA *pColor) final { IMPLEMENT_ME; };

  HRESULT
  STDMETHODCALLTYPE
  SetRotation(DXGI_MODE_ROTATION Rotation) final { IMPLEMENT_ME; };

  HRESULT
  STDMETHODCALLTYPE
  GetRotation(DXGI_MODE_ROTATION *pRotation) final { IMPLEMENT_ME; };

  HRESULT STDMETHODCALLTYPE SetSourceSize(UINT width, UINT height) {
    IMPLEMENT_ME
    return S_OK;
  };

  HRESULT STDMETHODCALLTYPE GetSourceSize(UINT *width, UINT *height) {
    IMPLEMENT_ME
    return S_OK;
  };

  HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency(UINT max_latency) {
    // no-op?
    return S_OK;
  };

  HRESULT STDMETHODCALLTYPE GetMaximumFrameLatency(UINT *max_latency) {
    if (max_latency) {
      *max_latency = kSwapchainLatency;
    }
    return S_OK;
  };

  HANDLE STDMETHODCALLTYPE GetFrameLatencyWaitableObject() {
    HANDLE result = nullptr;
    HANDLE processHandle = GetCurrentProcess();

    if (!DuplicateHandle(processHandle, present_semaphore_, processHandle,
                         &result, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
      return nullptr;
    }

    return result;
  };

  HRESULT STDMETHODCALLTYPE
  SetMatrixTransform(const DXGI_MATRIX_3X2_F *matrix) {
    return DXGI_ERROR_INVALID_CALL;
  };

  HRESULT STDMETHODCALLTYPE GetMatrixTransform(DXGI_MATRIX_3X2_F *matrix) {
    return DXGI_ERROR_INVALID_CALL;
  };

private:
  Com<IDXGIFactory1> factory_;
  ULONG presentation_count_;
  DXGI_SWAP_CHAIN_DESC1 desc_;
  DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreen_desc_;
  Com<IMTLD3D11Device> device_;
  Com<IMTLD3D11DeviceContext> device_context_;
  Com<IMTLD3D11BackBuffer> backbuffer_;
  HANDLE present_semaphore_;
  HWND hWnd;
};

HRESULT CreateSwapChain(IDXGIFactory1 *pFactory, IMTLDXGIDevice *pDevice,
                        HWND hWnd, const DXGI_SWAP_CHAIN_DESC1 *pDesc,
                        const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc,
                        IDXGISwapChain1 **ppSwapChain) {
  if (pDesc == NULL) {
    return DXGI_ERROR_INVALID_CALL;
  }
  if (ppSwapChain == NULL) {
    return DXGI_ERROR_INVALID_CALL;
  }
  InitReturnPtr(ppSwapChain);
  try {
    *ppSwapChain = ref(
        new MTLD3D11SwapChain(pFactory, pDevice, hWnd, pDesc, pFullscreenDesc));
    return S_OK;
  } catch (MTLD3DError &err) {
    ERR(err.message());
    return E_FAIL;
  }
};

} // namespace dxmt