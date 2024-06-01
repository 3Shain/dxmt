
#include "DXBCParser/winerror.h"
#include "Metal/MTLPixelFormat.hpp"
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

#pragma region DynamicBuffer

class DynamicBuffer
    : public TResourceBase<tag_buffer, IMTLDynamicBuffer, IMTLDynamicBindable> {
private:
  Obj<MTL::Buffer> buffer;

  // using SRVBase = TResourceViewBase<tag_shader_resource_view<DynamicBuffer>,
  // IMTLDynamicBindable> ; class SRVView :public SRVBase {};

  class DynamicBinding : public ComObject<IMTLBindable> {
  private:
    Com<DynamicBuffer> parent;
    std::function<void()> observer;

  public:
    DynamicBinding(DynamicBuffer *parent, std::function<void()> &&observer)
        : ComObject(), parent(parent), observer(std::move(observer)) {}

    ~DynamicBinding() { parent->RemoveObserver(this); }

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
      resource.IsTexture = false;
      resource.Buffer = parent->buffer.ptr();
    };

    void GetLogicalResourceOrView(REFIID riid, void **ppLogicalResource) {
      parent->QueryInterface(riid, ppLogicalResource);
    };

    void NotifyObserver() { observer(); };
  };

  std::set<DynamicBinding *> observers;

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

  HRESULT PrivateQueryInterface(const IID &riid, void **ppvObject) override {
    if (riid == __uuidof(IMTLDynamicBuffer)) {
      *ppvObject = ref_and_cast<IMTLDynamicBuffer>(this);
      return S_OK;
    }
    if (riid == __uuidof(IMTLDynamicBindable)) {
      *ppvObject = ref_and_cast<IMTLDynamicBindable>(this);
      return S_OK;
    }

    return E_FAIL;
  }

  HRESULT CreateShaderResourceView(const D3D11_SHADER_RESOURCE_VIEW_DESC *pDesc,
                                   ID3D11ShaderResourceView **ppView) override {
    D3D11_SHADER_RESOURCE_VIEW_DESC finalDesc;
    if (FAILED(ExtractEntireResourceViewDescription(&this->desc, pDesc,
                                                    &finalDesc))) {
      return E_INVALIDARG;
    }
    ERR("TODO: Dynamic buffer SRV");
    return E_NOTIMPL;
  }
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
  class SRVView : public SRVBase {};

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
      resource.IsTexture = false;
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

  MTL::Buffer *GetCurrentBuffer(UINT *pBytesPerRow) {
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

  void RotateBuffer(IMTLDynamicBufferPool *pool) {
    pool->ExchangeFromPool(&buffer);
    for (auto &observer : observers) {
      observer->NotifyObserver();
    }
  };

  void RemoveObserver(DynamicTextureBindable *pBindable) {
    assert(observers.erase(pBindable) == 1 &&
           "it must be 1 unless the destructor called twice");
  }

  HRESULT PrivateQueryInterface(const IID &riid, void **ppvObject) {
    if (riid == __uuidof(IMTLDynamicBuffer)) {
      *ppvObject = ref_and_cast<IMTLDynamicBuffer>(this);
      return S_OK;
    }
    return E_FAIL;
  }

  HRESULT CreateShaderResourceView(const D3D11_SHADER_RESOURCE_VIEW_DESC *pDesc,
                                   ID3D11ShaderResourceView **ppView) {
    D3D11_SHADER_RESOURCE_VIEW_DESC finalDesc;
    if (FAILED(ExtractEntireResourceViewDescription(&this->desc, pDesc,
                                                    &finalDesc))) {
      return E_INVALIDARG;
    }
    return E_INVALIDARG;
  };
};

#pragma endregion

HRESULT
CreateDynamicTexture2D(IMTLD3D11Device *pDevice,
                       const D3D11_TEXTURE2D_DESC *pDesc,
                       const D3D11_SUBRESOURCE_DATA *pInitialData,
                       ID3D11Texture2D **ppTexture) {
  Com<IMTLDXGIAdatper> adapter;
  auto metal = pDevice->GetMTLDevice();
  pDevice->GetAdapter(&adapter);
  MTL_FORMAT_DESC format;
  adapter->QueryFormatDesc(pDesc->Format, &format);
  if (format.IsCompressed) {
    throw 0;
  }
  if (format.PixelFormat == MTL::PixelFormatInvalid) {
    throw 0;
  }
  assert(format.Stride > 0);
  auto minimumAlignment =
      metal->minimumLinearTextureAlignmentForPixelFormat(format.PixelFormat);
  *ppTexture =
      ref(new DynamicTexture2D(pDesc, pInitialData, pDevice, format.Stride));
  return S_OK;
}

} // namespace dxmt
