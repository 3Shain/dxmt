#include "Metal/MTLPixelFormat.hpp"
#include "Metal/MTLResource.hpp"
#include "Metal/MTLTexture.hpp"
#include "com/com_pointer.hpp"
#include "d3d11_texture.hpp"
#include "mtld11_resource.hpp"
#include "objc_pointer.hpp"

namespace dxmt {

class DeviceBuffer : public TResourceBase<tag_buffer, IMTLBindable> {
private:
  Obj<MTL::Buffer> buffer;

public:
  DeviceBuffer(const tag_buffer::DESC_S *desc,
               const D3D11_SUBRESOURCE_DATA *pInitialData,
               IMTLD3D11Device *device)
      : TResourceBase<tag_buffer, IMTLBindable>(desc, device) {
    auto metal = device->GetMTLDevice();
    buffer = transfer(
        metal->newBuffer(desc->ByteWidth, MTL::ResourceStorageModeManaged));
    if (pInitialData) {
      memcpy(buffer->contents(), pInitialData->pSysMem, desc->ByteWidth);
      buffer->didModifyRange({0, desc->ByteWidth});
    }
  }
  HRESULT PrivateQueryInterface(REFIID riid, void **ppvObject) override {
    if (riid == __uuidof(IMTLBindable)) {
      *ppvObject = ref_and_cast<IMTLBindable>(this);
      return S_OK;
    }
    return E_NOINTERFACE;
  };
  void GetBoundResource(MTL_BIND_RESOURCE *ppResource) override {
    (*ppResource).IsTexture = 0;
    (*ppResource).Buffer = buffer.ptr();
  };
  void GetLogicalResourceOrView(REFIID riid,
                                void **ppLogicalResource) override {
    QueryInterface(riid, ppLogicalResource);
  };

  HRESULT CreateShaderResourceView(const D3D11_SHADER_RESOURCE_VIEW_DESC *desc,
                                   ID3D11ShaderResourceView **ppView) override {
    ERR("DeviceBuffer: SRV not supported");
    return E_FAIL;
  };

  HRESULT
  CreateUnorderedAccessView(const D3D11_UNORDERED_ACCESS_VIEW_DESC *desc,
                            ID3D11UnorderedAccessView **ppView) override {
    ERR("DeviceBuffer: UAV not supported");
    return E_FAIL;
  };

  HRESULT CreateRenderTargetView(const D3D11_RENDER_TARGET_VIEW_DESC *desc,
                                 ID3D11RenderTargetView **ppView) override {
    ERR("DeviceBuffer: RTV not supported");
    return E_FAIL;
  };

  HRESULT CreateDepthStencilView(const D3D11_DEPTH_STENCIL_VIEW_DESC *desc,
                                 ID3D11DepthStencilView **ppView) override {
    ERR("DeviceBuffer: DSV not supported");
    return E_FAIL;
  };
};

Com<ID3D11Buffer>
CreateDeviceBuffer(IMTLD3D11Device *pDevice, const D3D11_BUFFER_DESC *pDesc,
                   const D3D11_SUBRESOURCE_DATA *pInitialData) {
  return new DeviceBuffer(pDesc, pInitialData, pDevice);
}

template <typename tag_texture>
class DeviceTexture : public TResourceBase<tag_texture, IMTLBindable> {
private:
  Obj<MTL::Texture> texture;

  using SRVBase =
      TResourceViewBase<tag_shader_resource_view<DeviceTexture<tag_texture>>,
                        IMTLBindable>;
  class TextureSRV : public SRVBase {
  private:
    Obj<MTL::Texture> view;

  public:
    TextureSRV(MTL::Texture *view,
               const tag_shader_resource_view<>::DESC_S *pDesc,
               DeviceTexture *pResource, IMTLD3D11Device *pDevice)
        : SRVBase(pDesc, pResource, pDevice), view(view) {}

    void GetBoundResource(MTL_BIND_RESOURCE *ppResource) {
      (*ppResource).IsTexture = 1;
      (*ppResource).Texture = view.ptr();
    };

    void GetLogicalResourceOrView(REFIID riid, void **ppLogicalResource) {
      this->QueryInterface(riid, ppLogicalResource);
    };

    HRESULT PrivateQueryInterface(REFIID riid, void **ppvObject) {
      if (riid == __uuidof(IMTLBindable)) {
        *ppvObject = ref_and_cast<IMTLBindable>(this);
        return S_OK;
      }
      return E_NOINTERFACE;
    }
  };

  using UAVBase =
      TResourceViewBase<tag_unordered_access_view<DeviceTexture<tag_texture>>,
                        IMTLBindable>;
  class TextureUAV : public UAVBase {
  private:
    Obj<MTL::Texture> view;

  public:
    TextureUAV(MTL::Texture *view,
               const tag_unordered_access_view<>::DESC_S *pDesc,
               DeviceTexture *pResource, IMTLD3D11Device *pDevice)
        : UAVBase(pDesc, pResource, pDevice), view(view) {}

    void GetBoundResource(MTL_BIND_RESOURCE *ppResource) {
      (*ppResource).IsTexture = 1;
      (*ppResource).Texture = view.ptr();
    };

    void GetLogicalResourceOrView(REFIID riid, void **ppLogicalResource) {
      this->QueryInterface(riid, ppLogicalResource);
    };

    HRESULT PrivateQueryInterface(REFIID riid, void **ppvObject) {
      if (riid == __uuidof(IMTLBindable)) {
        *ppvObject = ref_and_cast<IMTLBindable>(this);
        return S_OK;
      }
      return E_NOINTERFACE;
    }
  };

  using RTVBase =
      TResourceViewBase<tag_render_target_view<DeviceTexture<tag_texture>>>;
  class TextureRTV : public RTVBase {
  private:
    Obj<MTL::Texture> view;

  public:
    TextureRTV(MTL::Texture *view,
               const tag_render_target_view<>::DESC_S *pDesc,
               DeviceTexture *pResource, IMTLD3D11Device *pDevice)
        : RTVBase(pDesc, pResource, pDevice), view(view) {}

    MTL::PixelFormat GetPixelFormat() final { return view->pixelFormat(); }

    MTL::Texture *GetCurrentTexture() final { return view.ptr(); }
  };

  using DSVBase =
      TResourceViewBase<tag_depth_stencil_view<DeviceTexture<tag_texture>>>;
  class TextureDSV : public DSVBase {
  private:
    Obj<MTL::Texture> view;

  public:
    TextureDSV(MTL::Texture *view,
               const tag_depth_stencil_view<>::DESC_S *pDesc,
               DeviceTexture *pResource, IMTLD3D11Device *pDevice)
        : DSVBase(pDesc, pResource, pDevice), view(view) {}

    MTL::PixelFormat GetPixelFormat() final { return view->pixelFormat(); }

    MTL::Texture *GetCurrentTexture() final { return view.ptr(); }
  };

public:
  DeviceTexture(const tag_texture::DESC_S *pDesc,
                const D3D11_SUBRESOURCE_DATA *pInitialData,
                IMTLD3D11Device *pDevice)
      : TResourceBase<tag_texture, IMTLBindable>(pDesc, pDevice) {
    auto metal = pDevice->GetMTLDevice();
    auto textureDescriptor = getTextureDescriptor(pDevice, pDesc);
    assert(textureDescriptor);
    texture = transfer(metal->newTexture(textureDescriptor));
    if (pInitialData) {
      initWithSubresourceData(texture, pDesc, pInitialData);
    }
  }
  HRESULT PrivateQueryInterface(REFIID riid, void **ppvObject) override {
    if (riid == __uuidof(IMTLBindable)) {
      *ppvObject = ref_and_cast<IMTLBindable>(this);
      return S_OK;
    }
    return E_NOINTERFACE;
  };

  void GetBoundResource(MTL_BIND_RESOURCE *ppResource) override {
    (*ppResource).IsTexture = 1;
    (*ppResource).Texture = texture.ptr();
  };

  void GetLogicalResourceOrView(REFIID riid,
                                void **ppLogicalResource) override {
    this->QueryInterface(riid, ppLogicalResource);
  };

  HRESULT CreateRenderTargetView(const D3D11_RENDER_TARGET_VIEW_DESC *pDesc,
                                 ID3D11RenderTargetView **ppView) override {
    D3D11_RENDER_TARGET_VIEW_DESC finalDesc;
    getViewDescFromResourceDesc(&this->desc, pDesc, &finalDesc);
    auto view =
        transfer(newTextureView(this->m_parent, this->texture, &finalDesc));
    if (!view) {
      return E_FAIL; // ??
    }
    if (ppView) {
      *ppView = ref(new TextureRTV(view, &finalDesc, this, this->m_parent));
    } else {
      return S_FALSE;
    }
    return S_OK;
  };

  HRESULT CreateDepthStencilView(const D3D11_DEPTH_STENCIL_VIEW_DESC *pDesc,
                                 ID3D11DepthStencilView **ppView) override {
    D3D11_DEPTH_STENCIL_VIEW_DESC finalDesc;
    getViewDescFromResourceDesc(&this->desc, pDesc, &finalDesc);
    auto view =
        transfer(newTextureView(this->m_parent, this->texture, &finalDesc));
    if (!view) {
      return E_FAIL; // ??
    }
    if (ppView) {
      *ppView = ref(new TextureDSV(view, &finalDesc, this, this->m_parent));
    } else {
      return S_FALSE;
    }
    return S_OK;
  };

  HRESULT CreateShaderResourceView(const D3D11_SHADER_RESOURCE_VIEW_DESC *pDesc,
                                   ID3D11ShaderResourceView **ppView) override {
    D3D11_SHADER_RESOURCE_VIEW_DESC finalDesc;
    getViewDescFromResourceDesc(&this->desc, pDesc, &finalDesc);
    auto view =
        transfer(newTextureView(this->m_parent, this->texture, &finalDesc));
    if (!view) {
      return E_FAIL; // ??
    }
    if (ppView) {
      *ppView = ref(new TextureSRV(view, &finalDesc, this, this->m_parent));
    } else {
      return S_FALSE;
    }
    return S_OK;
  };

  HRESULT
  CreateUnorderedAccessView(const D3D11_UNORDERED_ACCESS_VIEW_DESC *pDesc,
                            ID3D11UnorderedAccessView **ppView) override {
    D3D11_UNORDERED_ACCESS_VIEW_DESC finalDesc;
    getViewDescFromResourceDesc(&this->desc, pDesc, &finalDesc);
    auto view =
        transfer(newTextureView(this->m_parent, this->texture, &finalDesc));
    if (!view) {
      return E_FAIL; // ??
    }
    if (ppView) {
      *ppView = ref(new TextureUAV(view, &finalDesc, this, this->m_parent));
    } else {
      return S_FALSE;
    }
    return S_OK;
  };
};

Com<ID3D11Texture1D>
CreateDeviceTexture1D(IMTLD3D11Device *pDevice,
                      const D3D11_TEXTURE1D_DESC *pDesc,
                      const D3D11_SUBRESOURCE_DATA *pInitialData) {
  return new DeviceTexture<tag_texture_1d>(pDesc, pInitialData, pDevice);
}

Com<ID3D11Texture2D>
CreateDeviceTexture2D(IMTLD3D11Device *pDevice,
                      const D3D11_TEXTURE2D_DESC *pDesc,
                      const D3D11_SUBRESOURCE_DATA *pInitialData) {
  return new DeviceTexture<tag_texture_2d>(pDesc, pInitialData, pDevice);
}

Com<ID3D11Texture3D>
CreateDeviceTexture3D(IMTLD3D11Device *pDevice,
                      const D3D11_TEXTURE3D_DESC *pDesc,
                      const D3D11_SUBRESOURCE_DATA *pInitialData) {
  return new DeviceTexture<tag_texture_3d>(pDesc, pInitialData, pDevice);
}

} // namespace dxmt