
#include "DXBCParser/winerror.h"
#include "Metal/MTLPixelFormat.hpp"
#include "Metal/MTLResource.hpp"
#include "com/com_object.hpp"
#include "com/com_pointer.hpp"
#include "d3d11_device.hpp"
#include "dxgi_interfaces.h"
#include "mtld11_resource.hpp"
#include "Metal/MTLBuffer.hpp"
#include "Metal/MTLTexture.hpp"
#include "objc_pointer.hpp"
#include <set>

namespace dxmt {

#pragma region DynamicViewBindable
struct IMTLNotifiedBindable : public IMTLBindable {
  virtual void NotifyObserver() = 0;
};

template <typename BindableMap, typename ReleaseCallback>
class DynamicBinding : public ComObject<IMTLNotifiedBindable> {
private:
  Com<IUnknown> parent;
  std::function<void()> observer;
  BindableMap fn;
  ReleaseCallback cb;

public:
  DynamicBinding(IUnknown *parent, std::function<void()> &&observer,
                 BindableMap &&fn, ReleaseCallback &&cb)
      : ComObject(), parent(parent), observer(std::move(observer)),
        fn(std::forward<BindableMap>(fn)),
        cb(std::forward<ReleaseCallback>(cb)) {}

  ~DynamicBinding() { std::invoke(cb, this); }

  HRESULT QueryInterface(REFIID riid, void **ppvObject) override {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(IMTLBindable)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    return E_NOINTERFACE;
  }

  void GetBoundResource(MTL_BIND_RESOURCE *ppResource) override {
    std::invoke(fn, ppResource);
  };

  void GetLogicalResourceOrView(REFIID riid,
                                void **ppLogicalResource) override {
    parent->QueryInterface(riid, ppLogicalResource);
  };

  void NotifyObserver() override { observer(); };
};
#pragma endregion

#pragma region DynamicBuffer

class DynamicBuffer
    : public TResourceBase<tag_buffer, IMTLDynamicBuffer, IMTLDynamicBindable> {
private:
  Obj<MTL::Buffer> buffer_dynamic;

  std::set<IMTLNotifiedBindable *> observers;

  using SRVBase = TResourceViewBase<tag_shader_resource_view<DynamicBuffer>,
                                    IMTLDynamicBindable>;

  class SRV : public SRVBase {
  private:
  public:
    SRV(const tag_shader_resource_view<>::DESC_S *pDesc,
        DynamicBuffer *pResource, IMTLD3D11Device *pDevice)
        : SRVBase(pDesc, pResource, pDevice) {}

    ~SRV() { assert(resource->weak_srvs.erase(this) == 1); }

    void GetBindable(IMTLBindable **ppResource,
                     std::function<void()> &&onBufferSwap) override {
      IMTLNotifiedBindable *ret = ref(new DynamicBinding(
          static_cast<ID3D11ShaderResourceView *>(this),
          std::move(onBufferSwap),
          [u = this->resource.ptr()](MTL_BIND_RESOURCE *pResource) {
            assert(0 && "todo: prepare srv view");
          },
          [u = this->resource.ptr()](IMTLNotifiedBindable *_this) {
            u->RemoveObserver(_this);
          }));
      resource->AddObserver(ret);
      *ppResource = ret;
    };

    void RotateView() {

    };
  };

  std::set<SRV *> weak_srvs;

public:
  DynamicBuffer(const tag_buffer::DESC_S *desc,
                const D3D11_SUBRESOURCE_DATA *pInitialData,
                IMTLD3D11Device *device)
      : TResourceBase<tag_buffer, IMTLDynamicBuffer, IMTLDynamicBindable>(
            desc, device) {
    auto metal = device->GetMTLDevice();
    buffer_dynamic = transfer(metal->newBuffer(desc->ByteWidth, 0));
    if (pInitialData) {
      memcpy(buffer_dynamic->contents(), pInitialData->pSysMem,
             desc->ByteWidth);
      buffer_dynamic->didModifyRange({0, desc->ByteWidth});
    }
  }

  MTL::Buffer *GetCurrentBuffer(UINT *pBytesPerRow) override {
    return buffer_dynamic.ptr();
  };

  void GetBindable(IMTLBindable **ppResource,
                   std::function<void()> &&onBufferSwap) override {
    auto ret = ref(new DynamicBinding(
        static_cast<ID3D11Buffer *>(this), std::move(onBufferSwap),
        [this](MTL_BIND_RESOURCE *pResource) {
          MTL_BIND_RESOURCE &resource = *pResource;
          resource.Type = MTL_BIND_BUFFER_UNBOUNDED;
          resource.Buffer = this->buffer_dynamic.ptr();
        },
        [this](IMTLNotifiedBindable *_binding) {
          this->RemoveObserver(_binding);
        }));
    AddObserver(ret);
    *ppResource = ret;
  };

  void RotateBuffer(IMTLDynamicBufferPool *pool) override {
    pool->ExchangeFromPool(&buffer_dynamic);
    for (auto srv : weak_srvs) {
      srv->RotateView();
    }
    for (auto &observer : observers) {
      observer->NotifyObserver();
    }
  };

  void AddObserver(IMTLNotifiedBindable *pBindable) {
    assert(observers.insert(pBindable).second && "otherwise already added");
  }

  void RemoveObserver(IMTLNotifiedBindable *pBindable) {
    assert(observers.erase(pBindable) == 1 &&
           "it must be 1 unless the destructor called twice");
  }

  HRESULT CreateShaderResourceView(const D3D11_SHADER_RESOURCE_VIEW_DESC *pDesc,
                                   ID3D11ShaderResourceView **ppView) override {
    D3D11_SHADER_RESOURCE_VIEW_DESC finalDesc;
    if (FAILED(ExtractEntireResourceViewDescription(&this->desc, pDesc,
                                                    &finalDesc))) {
      return E_INVALIDARG;
    }
    if (finalDesc.ViewDimension != D3D11_SRV_DIMENSION_BUFFER &&
        finalDesc.ViewDimension != D3D11_SRV_DIMENSION_BUFFEREX) {
      return E_FAIL;
    }
    if (!ppView) {
      return S_FALSE;
    }
    // TODO: polymorphism: buffer/byteaddress/structured
    auto srv = ref(new SRV(&finalDesc, this, m_parent));
    assert(weak_srvs.insert(srv).second);
    *ppView = srv;
    srv->RotateView();
    return S_OK;
  };
};

#pragma endregion

HRESULT
CreateDynamicBuffer(IMTLD3D11Device *pDevice, const D3D11_BUFFER_DESC *pDesc,
                    const D3D11_SUBRESOURCE_DATA *pInitialData,
                    ID3D11Buffer **ppBuffer) {
  *ppBuffer = ref(new DynamicBuffer(pDesc, pInitialData, pDevice));
  return S_OK;
}

#pragma region DynamicTexture

class DynamicTexture2D
    : public TResourceBase<tag_texture_2d, IMTLDynamicBuffer> {
private:
  Obj<MTL::Buffer> buffer;
  size_t bytes_per_row;

  using SRVBase = TResourceViewBase<tag_shader_resource_view<DynamicTexture2D>,
                                    IMTLDynamicBindable>;

  class SRV : public SRVBase {
  private:
  public:
    SRV(const tag_shader_resource_view<>::DESC_S *pDesc,
        DynamicTexture2D *pResource, IMTLD3D11Device *pDevice)
        : SRVBase(pDesc, pResource, pDevice) {}

    ~SRV() { assert(resource->weak_srvs.erase(this) == 1); }

    void GetBindable(IMTLBindable **ppResource,
                     std::function<void()> &&onBufferSwap) override {
      *ppResource = ref(new DynamicBinding(
          static_cast<ID3D11ShaderResourceView *>(this),
          std::move(onBufferSwap),
          [u = this->resource.ptr()](MTL_BIND_RESOURCE *pResource) {
            assert(0 && "todo: prepare srv view");
          },
          [u = this->resource.ptr()](IMTLNotifiedBindable *_this) {
            u->RemoveObserver(_this);
          }));
    }

    void RotateView() {

    };
  };

  std::set<IMTLNotifiedBindable *> observers;
  std::set<SRV *> weak_srvs;

public:
  DynamicTexture2D(const tag_texture_2d::DESC_S *desc,
                   const D3D11_SUBRESOURCE_DATA *pInitialData,
                   IMTLD3D11Device *device, UINT bytesPerRow)
      : TResourceBase<tag_texture_2d, IMTLDynamicBuffer>(desc, device) {
    //
    // buffer = transfer(metal->newBuffer(desc->ByteWidth, 0));
    // if (pInitialData) {
    //   memcpy(buffer->contents(), pInitialData->pSysMem, desc->ByteWidth);
    //   buffer->didModifyRange({0, desc->ByteWidth});
    // }
    bytes_per_row = bytesPerRow;
  }

  MTL::Buffer *GetCurrentBuffer(UINT *pBytesPerRow) override {
    *pBytesPerRow = bytes_per_row;
    return buffer.ptr();
  };

  void RotateBuffer(IMTLDynamicBufferPool *pool) override {
    pool->ExchangeFromPool(&buffer);
    for (auto srv : weak_srvs) {
      srv->RotateView();
    }
    for (auto &observer : observers) {
      observer->NotifyObserver();
    }
  };

  void RemoveObserver(IMTLNotifiedBindable *pBindable) {
    assert(observers.erase(pBindable) == 1 &&
           "it must be 1 unless the destructor called twice");
  }

  HRESULT CreateShaderResourceView(const D3D11_SHADER_RESOURCE_VIEW_DESC *pDesc,
                                   ID3D11ShaderResourceView **ppView) override {
    D3D11_SHADER_RESOURCE_VIEW_DESC finalDesc;
    if (FAILED(ExtractEntireResourceViewDescription(&this->desc, pDesc,
                                                    &finalDesc))) {
      return E_INVALIDARG;
    }
    if (finalDesc.ViewDimension != D3D10_SRV_DIMENSION_TEXTURE2D) {
      ERR("Only 2d texture SRV can be created on dynamic texture");
      return E_FAIL;
    }
    if (finalDesc.Texture2D.MostDetailedMip != 0 ||
        finalDesc.Texture2D.MipLevels != 1) {
      ERR("2d texture SRV must be non-mipmapped on dynamic texture");
      return E_FAIL;
    }
    if (!ppView) {
      return S_FALSE;
    }
    auto srv = ref(new SRV(&finalDesc, this, m_parent));
    assert(weak_srvs.insert(srv).second);
    *ppView = srv;
    srv->RotateView();
    return S_OK;
  };
};

#pragma endregion

HRESULT
CreateDynamicTexture2D(IMTLD3D11Device *pDevice,
                       const D3D11_TEXTURE2D_DESC *pDesc,
                       const D3D11_SUBRESOURCE_DATA *pInitialData,
                       ID3D11Texture2D **ppTexture) {
  Com<IMTLDXGIAdatper> adapter;
  pDevice->GetAdapter(&adapter);
  MTL_FORMAT_DESC format;
  adapter->QueryFormatDesc(pDesc->Format, &format);
  if (format.IsCompressed) {
    return E_FAIL;
  }
  if (format.PixelFormat == MTL::PixelFormatInvalid) {
    return E_FAIL;
  }
  assert(format.Stride > 0);
  *ppTexture =
      ref(new DynamicTexture2D(pDesc, pInitialData, pDevice, format.Stride));
  return S_OK;
}

} // namespace dxmt
