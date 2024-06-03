
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

template <typename BindableMap>
class DynamicViewBindable : public ComObject<IMTLBindable> {
private:
  Com<IMTLBindable> source;
  MTL_BIND_RESOURCE cache;
  /**

  */
  Obj<MTL::Resource> view_ref;

public:
  DynamicViewBindable(Com<IMTLBindable> &&source, BindableMap &thunk)
      : source(std::move(source)) {
    MTL_BIND_RESOURCE bindable_original;
    source->GetBoundResource(&bindable_original);
    view_ref = std::move<Obj<MTL::Resource>>(
        std::invoke(thunk, &bindable_original, &cache));
  }

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
    *ppResource = cache;
  };

  void GetLogicalResourceOrView(REFIID riid,
                                void **ppLogicalResource) override {
    source->GetLogicalResourceOrView(riid, ppLogicalResource);
  };
};

#pragma endregion

#pragma region DynamicBuffer

class DynamicBuffer
    : public TResourceBase<tag_buffer, IMTLDynamicBuffer, IMTLDynamicBindable> {
private:
  Obj<MTL::Buffer> buffer;

  class DynamicBinding : public ComObject<IMTLBindable> {
  private:
    Com<DynamicBuffer> parent;
    std::function<void()> observer;

  public:
    DynamicBinding(DynamicBuffer *parent, std::function<void()> &&observer)
        : ComObject(), parent(parent), observer(std::move(observer)) {}

    ~DynamicBinding() { parent->RemoveObserver(this); }

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
      MTL_BIND_RESOURCE &resource = *ppResource;
      resource.Type = MTL_BIND_BUFFER_UNBOUNDED;
      resource.Buffer = parent->buffer.ptr();
    };

    void GetLogicalResourceOrView(REFIID riid,
                                  void **ppLogicalResource) override {
      parent->QueryInterface(riid, ppLogicalResource);
    };

    void NotifyObserver() { observer(); };
  };
  std::set<DynamicBinding *> observers;

  using SRVBase = TResourceViewBase<tag_shader_resource_view<DynamicBuffer>,
                                    IMTLDynamicBindable>;

  template <typename BindableMap> class SRV : public SRVBase {
  private:
    BindableMap thunk;

  public:
    SRV(const tag_shader_resource_view<>::DESC_S *pDesc,
        DynamicBuffer *pResource, IMTLD3D11Device *pDevice, BindableMap &&thunk)
        : SRVBase(pDesc, pResource, pDevice), thunk(std::move(thunk)) {}

    void GetBindable(IMTLBindable **ppResource,
                     std::function<void()> &&onBufferSwap) override {
      Com<IMTLBindable> underlying;
      resource->GetBindable(&underlying, std::move(onBufferSwap));
      *ppResource = ref(new DynamicViewBindable(std::move(underlying), thunk));
    };
  };

public:
  DynamicBuffer(const tag_buffer::DESC_S *desc,
                const D3D11_SUBRESOURCE_DATA *pInitialData,
                IMTLD3D11Device *device)
      : TResourceBase<tag_buffer, IMTLDynamicBuffer, IMTLDynamicBindable>(
            desc, device) {
    auto metal = device->GetMTLDevice();
    buffer = transfer(metal->newBuffer(desc->ByteWidth, 0));
    if (pInitialData) {
      memcpy(buffer->contents(), pInitialData->pSysMem, desc->ByteWidth);
      buffer->didModifyRange({0, desc->ByteWidth});
    }
  }

  MTL::Buffer *GetCurrentBuffer(UINT *pBytesPerRow) override {
    return buffer.ptr();
  };

  void GetBindable(IMTLBindable **ppResource,
                   std::function<void()> &&onBufferSwap) override {
    *ppResource = new DynamicBinding(this, std::move(onBufferSwap));
    assert(observers.insert((DynamicBinding *)*ppResource).second &&
           "are you kidding me?");
  };

  void RotateBuffer(IMTLDynamicBufferPool *pool) override {
    pool->ExchangeFromPool(&buffer);
    for (auto &observer : observers) {
      observer->NotifyObserver();
    }
  };

  void RemoveObserver(DynamicBinding *pBindable) {
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
    *ppView = new SRV(&finalDesc, this, m_parent,
                      [](const MTL_BIND_RESOURCE *, MTL_BIND_RESOURCE *) {
                        assert(0 && "TODO: dynamic texture2d srv bindable map");
                        return Obj<MTL::Resource>(nullptr);
                      });
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
  Obj<MTL::Texture> view;
  size_t bytes_per_row;

  using SRVBase = TResourceViewBase<tag_shader_resource_view<DynamicTexture2D>,
                                    IMTLDynamicBindable>;

  template <typename BindableMap> class SRV : public SRVBase {
  private:
    BindableMap thunk;

  public:
    SRV(const tag_shader_resource_view<>::DESC_S *pDesc,
        DynamicTexture2D *pResource, IMTLD3D11Device *pDevice,
        BindableMap &&thunk)
        : SRVBase(pDesc, pResource, pDevice), thunk(std::move(thunk)) {}

    void GetBindable(IMTLBindable **ppResource,
                     std::function<void()> &&onBufferSwap) override {
      Com<IMTLBindable> underlying;
      resource->GetBindablePrivate(&underlying, std::move(onBufferSwap));
      *ppResource = ref(new DynamicViewBindable(std::move(underlying), thunk));
    }
  };

  class DynamicTextureBindable : public ComObject<IMTLBindable> {
  private:
    Com<DynamicTexture2D> parent;
    std::function<void()> observer;

  public:
    DynamicTextureBindable(DynamicTexture2D *parent,
                           std::function<void()> &&observer)
        : ComObject(), parent(parent), observer(std::move(observer)) {}

    ~DynamicTextureBindable() { parent->RemoveObserver(this); }

    HRESULT QueryInterface(REFIID riid, void **ppvObject) {
      if (ppvObject == nullptr)
        return E_POINTER;

      *ppvObject = nullptr;

      if (riid == __uuidof(IUnknown) || riid == __uuidof(IMTLBindable)) {
        *ppvObject = ref(this);
        return S_OK;
      }

      return E_NOINTERFACE;
    }

    void GetBoundResource(MTL_BIND_RESOURCE *ppResource) {
      MTL_BIND_RESOURCE &resource = *ppResource;
      resource.Type = MTL_BIND_BUFFER_UNBOUNDED;
      resource.Buffer = parent->buffer.ptr();
    };

    void GetLogicalResourceOrView(REFIID riid, void **ppLogicalResource) {
      parent->QueryInterface(riid, ppLogicalResource);
    };

    void NotifyObserver() { observer(); };
  };

  std::set<DynamicTextureBindable *> observers;

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

  /**
  called by view
  */
  void GetBindablePrivate(IMTLBindable **ppResource,
                          std::function<void()> &&onBufferSwap) {
    *ppResource = new DynamicTextureBindable(this, std::move(onBufferSwap));
    assert(observers.insert((DynamicTextureBindable *)*ppResource).second &&
           "are you kidding me?");
  };

  void RotateBuffer(IMTLDynamicBufferPool *pool) override {
    pool->ExchangeFromPool(&buffer);
    for (auto &observer : observers) {
      observer->NotifyObserver();
    }
  };

  void RemoveObserver(DynamicTextureBindable *pBindable) {
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
    *ppView = new SRV(&finalDesc, this, m_parent,
                      [](MTL_BIND_RESOURCE *, MTL_BIND_RESOURCE *) {
                        assert(0 && "TODO: dynamic texture2d srv bindable map");
                        return Obj<MTL::Resource>(nullptr);
                      });
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
