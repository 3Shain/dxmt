#include "DXBCParser/winerror.h"
#include "Metal/MTLPixelFormat.hpp"
#include "Metal/MTLResource.hpp"
#include "Metal/MTLTexture.hpp"
#include "com/com_pointer.hpp"
#include "d3d11_device.hpp"
#include "d3d11_texture.hpp"
#include "mtld11_resource.hpp"
#include "objc_pointer.hpp"

namespace dxmt {

#pragma region DeviceBuffer

class DeviceBuffer : public TResourceBase<tag_buffer, IMTLBindable> {
private:
  Obj<MTL::Buffer> buffer;
  bool structured;
  bool allow_raw_view;

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
    structured = desc->MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    allow_raw_view =
        desc->MiscFlags & D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
  }

  void GetBoundResource(MTL_BIND_RESOURCE *ppResource) override {
    (*ppResource).IsTexture = 0;
    (*ppResource).Buffer = buffer.ptr();
  };
  void GetLogicalResourceOrView(REFIID riid,
                                void **ppLogicalResource) override {
    QueryInterface(riid, ppLogicalResource);
  };

  using SRVBase = TResourceViewBase<tag_shader_resource_view<DeviceBuffer>,
                        IMTLBindable>;

  HRESULT CreateShaderResourceView(const D3D11_SHADER_RESOURCE_VIEW_DESC *pDesc,
                                   ID3D11ShaderResourceView **ppView) override {
    D3D11_SHADER_RESOURCE_VIEW_DESC finalDesc;
    if (FAILED(ExtractEntireResourceViewDescription(&this->desc, pDesc,
                                                    &finalDesc))) {
      return E_INVALIDARG;
    }
    if (finalDesc.ViewDimension != D3D11_SRV_DIMENSION_BUFFER &&
        finalDesc.ViewDimension != D3D11_SRV_DIMENSION_BUFFEREX) {
      return E_INVALIDARG;
    }
    if (structured) {
      // StructuredBuffer
    } else {
      if (finalDesc.ViewDimension == D3D11_SRV_DIMENSION_BUFFEREX &&
          finalDesc.BufferEx.Flags & D3D11_BUFFEREX_SRV_FLAG_RAW) {
        if (!allow_raw_view)
          return E_INVALIDARG;
        // ByteAddressBuffer
      } else {
        Obj<MTL::Texture> view;
        if (FAILED(CreateMTLTextureView(this->m_parent, this->buffer,
                                        &finalDesc, &view))) {
          return E_FAIL;
        }
      }
    }
    ERR("DeviceBuffer: SRV not supported");
    return E_FAIL;
  };

  using UAVBase = TResourceViewBase<tag_unordered_access_view<DeviceBuffer>,
                        IMTLBindable>;

  HRESULT
  CreateUnorderedAccessView(const D3D11_UNORDERED_ACCESS_VIEW_DESC *pDesc,
                            ID3D11UnorderedAccessView **ppView) override {
    D3D11_UNORDERED_ACCESS_VIEW_DESC finalDesc;
    if (FAILED(ExtractEntireResourceViewDescription(&this->desc, pDesc,
                                                    &finalDesc))) {
      return E_INVALIDARG;
    }
    if (finalDesc.ViewDimension != D3D11_UAV_DIMENSION_BUFFER) {
      return E_INVALIDARG;
    }
    if (structured) {
      // StructuredBuffer
    } else {
      if (finalDesc.Buffer.Flags & D3D11_BUFFER_UAV_FLAG_RAW) {
        if (!allow_raw_view)
          return E_INVALIDARG;
        // ByteAddressBuffer
      } else {
        Obj<MTL::Texture> view;
        if (FAILED(CreateMTLTextureView(this->m_parent, this->buffer,
                                        &finalDesc, &view))) {
          return E_FAIL;
        }
      }
    }
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

HRESULT
CreateDeviceBuffer(IMTLD3D11Device *pDevice, const D3D11_BUFFER_DESC *pDesc,
                   const D3D11_SUBRESOURCE_DATA *pInitialData,
                   ID3D11Buffer **ppBuffer) {
  *ppBuffer = ref(new DeviceBuffer(pDesc, pInitialData, pDevice));
  return S_OK;
}

#pragma endregion

#pragma region DeviceTexture

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

    void GetBoundResource(MTL_BIND_RESOURCE *ppResource) override {
      (*ppResource).IsTexture = 1;
      (*ppResource).Texture = view.ptr();
    };

    void GetLogicalResourceOrView(REFIID riid, void **ppLogicalResource) override {
      this->resource->QueryInterface(riid, ppLogicalResource);
    };
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

    void GetBoundResource(MTL_BIND_RESOURCE *ppResource) override {
      (*ppResource).IsTexture = 1;
      (*ppResource).Texture = view.ptr();
    };

    void GetLogicalResourceOrView(REFIID riid, void **ppLogicalResource) override {
      this->resource->QueryInterface(riid, ppLogicalResource);
    };
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
  DeviceTexture(const tag_texture::DESC_S *pDesc, MTL::Texture *texture,
                IMTLD3D11Device *pDevice)
      : TResourceBase<tag_texture, IMTLBindable>(pDesc, pDevice),
        texture(texture) {}

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
    if (FAILED(ExtractEntireResourceViewDescription(&this->desc, pDesc,
                                                    &finalDesc))) {
      return E_INVALIDARG;
    }
    Obj<MTL::Texture> view;
    if (FAILED(CreateMTLTextureView(this->m_parent, this->texture, &finalDesc,
                                    &view))) {
      return E_FAIL;
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
    if (FAILED(ExtractEntireResourceViewDescription(&this->desc, pDesc,
                                                    &finalDesc))) {
      return E_INVALIDARG;
    }
    Obj<MTL::Texture> view;
    if (FAILED(CreateMTLTextureView(this->m_parent, this->texture, &finalDesc,
                                    &view))) {
      return E_FAIL;
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
    if (FAILED(ExtractEntireResourceViewDescription(&this->desc, pDesc,
                                                    &finalDesc))) {
      return E_INVALIDARG;
    }
    Obj<MTL::Texture> view;
    if (FAILED(CreateMTLTextureView(this->m_parent, this->texture, &finalDesc,
                                    &view))) {
      return E_FAIL;
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
    if (FAILED(ExtractEntireResourceViewDescription(&this->desc, pDesc,
                                                    &finalDesc))) {
      return E_INVALIDARG;
    }
    Obj<MTL::Texture> view;
    if (FAILED(CreateMTLTextureView(this->m_parent, this->texture, &finalDesc,
                                    &view))) {
      return E_FAIL;
    }
    if (ppView) {
      *ppView = ref(new TextureUAV(view, &finalDesc, this, this->m_parent));
    } else {
      return S_FALSE;
    }
    return S_OK;
  };
};

template <typename tag>
HRESULT CreateDeviceTextureInternal(IMTLD3D11Device *pDevice,
                                    const typename tag::DESC_S *pDesc,
                                    const D3D11_SUBRESOURCE_DATA *pInitialData,
                                    typename tag::COM **ppTexture) {
  auto metal = pDevice->GetMTLDevice();
  Obj<MTL::TextureDescriptor> textureDescriptor;
  if (FAILED(CreateMTLTextureDescriptor(pDevice, pDesc, &textureDescriptor))) {
    return E_INVALIDARG;
  }
  auto texture = transfer(metal->newTexture(textureDescriptor));
  if (pInitialData) {
    initWithSubresourceData(texture, pDesc, pInitialData);
  }
  *ppTexture = ref(new DeviceTexture<tag>(pDesc, texture, pDevice));
  return S_OK;
}

HRESULT
CreateDeviceTexture1D(IMTLD3D11Device *pDevice,
                      const D3D11_TEXTURE1D_DESC *pDesc,
                      const D3D11_SUBRESOURCE_DATA *pInitialData,
                      ID3D11Texture1D **ppTexture) {
  return CreateDeviceTextureInternal<tag_texture_1d>(pDevice, pDesc,
                                                     pInitialData, ppTexture);
}

HRESULT
CreateDeviceTexture2D(IMTLD3D11Device *pDevice,
                      const D3D11_TEXTURE2D_DESC *pDesc,
                      const D3D11_SUBRESOURCE_DATA *pInitialData,
                      ID3D11Texture2D **ppTexture) {
  return CreateDeviceTextureInternal<tag_texture_2d>(pDevice, pDesc,
                                                     pInitialData, ppTexture);
}

HRESULT
CreateDeviceTexture3D(IMTLD3D11Device *pDevice,
                      const D3D11_TEXTURE3D_DESC *pDesc,
                      const D3D11_SUBRESOURCE_DATA *pInitialData,
                      ID3D11Texture3D **ppTexture) {
  return CreateDeviceTextureInternal<tag_texture_3d>(pDevice, pDesc,
                                                     pInitialData, ppTexture);
}

#pragma endregion

} // namespace dxmt