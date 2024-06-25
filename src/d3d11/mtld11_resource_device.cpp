#include "DXBCParser/winerror.h"
#include "Metal/MTLPixelFormat.hpp"
#include "Metal/MTLResource.hpp"
#include "Metal/MTLTexture.hpp"
#include "com/com_pointer.hpp"
#include "d3d11_device.hpp"
#include "d3d11_texture.hpp"
#include "dxmt_binding.hpp"
#include "log/log.hpp"
#include "mtld11_resource.hpp"
#include "objc_pointer.hpp"

namespace dxmt {

#pragma region DeviceBuffer

class DeviceBuffer : public TResourceBase<tag_buffer, IMTLBindable> {
private:
  Obj<MTL::Buffer> buffer;
  uint64_t buffer_handle;
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
    buffer_handle = buffer->gpuAddress();
    structured = desc->MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    allow_raw_view =
        desc->MiscFlags & D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
  }

  BindingRef UseBindable(uint64_t) override {
    return BindingRef(buffer.ptr());
  };

  ArgumentData GetArgumentData() override {
    return ArgumentData(buffer_handle);
  }

  bool GetContentionState(uint64_t) override { return true; };

  void GetLogicalResourceOrView(REFIID riid,
                                void **ppLogicalResource) override {
    QueryInterface(riid, ppLogicalResource);
  };

  using SRVBase =
      TResourceViewBase<tag_shader_resource_view<DeviceBuffer>, IMTLBindable>;
  template <typename F> class SRV : public SRVBase {
  private:
    ArgumentData argument_data;
    F f;

  public:
    SRV(const tag_shader_resource_view<>::DESC_S *pDesc,
        DeviceBuffer *pResource, IMTLD3D11Device *pDevice,
        ArgumentData argument_data, F &&fn)
        : SRVBase(pDesc, pResource, pDevice), argument_data(argument_data),
          f(std::forward<F>(fn)) {}

    BindingRef UseBindable(uint64_t t) override { return std::invoke(f, t); };

    ArgumentData GetArgumentData() override { return argument_data; };

    void GetLogicalResourceOrView(REFIID riid,
                                  void **ppLogicalResource) override {
      QueryInterface(riid, ppLogicalResource);
    };
  };

  HRESULT CreateShaderResourceView(const D3D11_SHADER_RESOURCE_VIEW_DESC *pDesc,
                                   ID3D11ShaderResourceView **ppView) override {
    D3D11_SHADER_RESOURCE_VIEW_DESC finalDesc;
    if (FAILED(ExtractEntireResourceViewDescription(&this->desc, pDesc,
                                                    &finalDesc))) {
      ERR("DeviceBuffer: Failed to extract srv description");
      return E_INVALIDARG;
    }
    if (finalDesc.ViewDimension != D3D11_SRV_DIMENSION_BUFFER &&
        finalDesc.ViewDimension != D3D11_SRV_DIMENSION_BUFFEREX) {
      return E_INVALIDARG;
    }
    if (structured) {
      // StructuredBuffer
      auto offset = pDesc->Buffer.FirstElement * this->desc.StructureByteStride;
      auto size = pDesc->Buffer.NumElements;
      *ppView = ref(new SRV(
          pDesc, this, m_parent, ArgumentData(buffer_handle + offset, size),
          [ctx = Com(this), offset, size](uint64_t) {
            return BindingRef(ctx->buffer.ptr(), size, offset);
          }));
      return S_OK;
    } else {
      if (finalDesc.ViewDimension == D3D11_SRV_DIMENSION_BUFFEREX &&
          finalDesc.BufferEx.Flags & D3D11_BUFFEREX_SRV_FLAG_RAW) {
        if (!allow_raw_view)
          return E_INVALIDARG;
        auto offset = pDesc->Buffer.FirstElement;
        auto size = pDesc->Buffer.NumElements;
        *ppView = ref(new SRV(
            pDesc, this, m_parent, ArgumentData(buffer_handle + offset, size),
            [ctx = Com(this), offset, size](uint64_t) {
              return BindingRef(ctx->buffer.ptr(), size, offset);
            }));
        return S_OK;
      } else {
        Obj<MTL::Texture> view;
        if (FAILED(CreateMTLTextureView(this->m_parent, this->buffer,
                                        &finalDesc, &view))) {
          return E_FAIL;
        }
        *ppView = ref(new SRV(pDesc, this, m_parent,
                              ArgumentData(view->gpuResourceID(), view.ptr()),
                              [view = std::move(view)](uint64_t) {
                                return BindingRef(view.ptr());
                              }));
        return S_OK;
      }
    }
    ERR("DeviceBuffer: SRV not supported");
    return E_FAIL;
  };

  using UAVBase =
      TResourceViewBase<tag_unordered_access_view<DeviceBuffer>, IMTLBindable>;

  template <typename F> class UAV : public UAVBase {
  private:
    ArgumentData argument_data;
    F f;

  public:
    UAV(const tag_unordered_access_view<>::DESC_S *pDesc,
        DeviceBuffer *pResource, IMTLD3D11Device *pDevice,
        ArgumentData argument_data, F &&fn)
        : UAVBase(pDesc, pResource, pDevice), argument_data(argument_data),
          f(std::forward<F>(fn)) {}

    BindingRef UseBindable(uint64_t t) override { return std::invoke(f, t); };

    ArgumentData GetArgumentData() override { return argument_data; };

    void GetLogicalResourceOrView(REFIID riid,
                                  void **ppLogicalResource) override {
      QueryInterface(riid, ppLogicalResource);
    };
  };

  HRESULT
  CreateUnorderedAccessView(const D3D11_UNORDERED_ACCESS_VIEW_DESC *pDesc,
                            ID3D11UnorderedAccessView **ppView) override {
    D3D11_UNORDERED_ACCESS_VIEW_DESC finalDesc;
    if (FAILED(ExtractEntireResourceViewDescription(&this->desc, pDesc,
                                                    &finalDesc))) {
      ERR("DeviceBuffer: Failed to extract uav description");
      return E_INVALIDARG;
    }
    if (finalDesc.ViewDimension != D3D11_UAV_DIMENSION_BUFFER) {
      return E_INVALIDARG;
    }
    // assert(finalDesc.Buffer.Flags < 2 && "TODO: uav counter");
    if (structured) {
      // StructuredBuffer
      auto offset = pDesc->Buffer.FirstElement * this->desc.StructureByteStride,
           size = pDesc->Buffer.NumElements;
      *ppView = ref(new UAV(
          pDesc, this, m_parent, ArgumentData(buffer_handle + offset, size),
          [ctx = Com(this), offset, size](uint64_t) {
            return BindingRef(ctx->buffer.ptr(), size, offset);
          }));
      return S_OK;
    } else {
      if (finalDesc.Buffer.Flags & D3D11_BUFFER_UAV_FLAG_RAW) {
        if (!allow_raw_view)
          return E_INVALIDARG;
        auto offset = pDesc->Buffer.FirstElement,
             size = pDesc->Buffer.NumElements;
        *ppView = ref(new UAV(
            pDesc, this, m_parent, ArgumentData(buffer_handle + offset, size),
            [ctx = Com(this), offset, size](uint64_t) {
              return BindingRef(ctx->buffer.ptr(), size, offset);
            }));
        return S_OK;
      } else {
        Obj<MTL::Texture> view;
        if (FAILED(CreateMTLTextureView(this->m_parent, this->buffer,
                                        &finalDesc, &view))) {
          return E_FAIL;
        }
        *ppView = ref(new UAV(pDesc, this, m_parent,
                              ArgumentData(view->gpuResourceID(), view.ptr()),
                              [view = std::move(view)](uint64_t) {
                                return BindingRef(view.ptr());
                              }));
        return S_OK;
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
  MTL::ResourceID texture_handle;

  using SRVBase =
      TResourceViewBase<tag_shader_resource_view<DeviceTexture<tag_texture>>,
                        IMTLBindable>;
  class TextureSRV : public SRVBase {
  private:
    Obj<MTL::Texture> view;
    MTL::ResourceID view_handle;

  public:
    TextureSRV(MTL::Texture *view,
               const tag_shader_resource_view<>::DESC_S *pDesc,
               DeviceTexture *pResource, IMTLD3D11Device *pDevice)
        : SRVBase(pDesc, pResource, pDevice), view(view),
          view_handle(view->gpuResourceID()) {}

    BindingRef UseBindable(uint64_t) override {
      return BindingRef(view.ptr());
    };

    ArgumentData GetArgumentData() override {
      return ArgumentData(view_handle, view.ptr());
    };

    void GetLogicalResourceOrView(REFIID riid,
                                  void **ppLogicalResource) override {
      this->QueryInterface(riid, ppLogicalResource);
    };
  };

  using UAVBase =
      TResourceViewBase<tag_unordered_access_view<DeviceTexture<tag_texture>>,
                        IMTLBindable>;
  class TextureUAV : public UAVBase {
  private:
    Obj<MTL::Texture> view;
    MTL::ResourceID view_handle;

  public:
    TextureUAV(MTL::Texture *view,
               const tag_unordered_access_view<>::DESC_S *pDesc,
               DeviceTexture *pResource, IMTLD3D11Device *pDevice)
        : UAVBase(pDesc, pResource, pDevice), view(view),
          view_handle(view->gpuResourceID()) {}

    BindingRef UseBindable(uint64_t) override {
      return BindingRef(view.ptr());
    };

    ArgumentData GetArgumentData() override {
      return ArgumentData(view_handle, view.ptr());
    };

    void GetLogicalResourceOrView(REFIID riid,
                                  void **ppLogicalResource) override {
      this->QueryInterface(riid, ppLogicalResource);
    };
  };

  using RTVBase =
      TResourceViewBase<tag_render_target_view<DeviceTexture<tag_texture>>>;
  class TextureRTV : public RTVBase {
  private:
    Obj<MTL::Texture> view;
    MTL::PixelFormat view_pixel_format;

  public:
    TextureRTV(MTL::Texture *view,
               const tag_render_target_view<>::DESC_S *pDesc,
               DeviceTexture *pResource, IMTLD3D11Device *pDevice)
        : RTVBase(pDesc, pResource, pDevice), view(view),
          view_pixel_format(view->pixelFormat()) {}

    MTL::PixelFormat GetPixelFormat() final { return view_pixel_format; }

    BindingRef GetBinding(uint64_t) final { return BindingRef(view.ptr()); }
  };

  using DSVBase =
      TResourceViewBase<tag_depth_stencil_view<DeviceTexture<tag_texture>>>;
  class TextureDSV : public DSVBase {
  private:
    Obj<MTL::Texture> view;
    MTL::PixelFormat view_pixel_format;

  public:
    TextureDSV(MTL::Texture *view,
               const tag_depth_stencil_view<>::DESC_S *pDesc,
               DeviceTexture *pResource, IMTLD3D11Device *pDevice)
        : DSVBase(pDesc, pResource, pDevice), view(view),
          view_pixel_format(view->pixelFormat()) {}

    MTL::PixelFormat GetPixelFormat() final { return view_pixel_format; }

    BindingRef GetBinding(uint64_t) final { return BindingRef(view.ptr()); }
  };

public:
  DeviceTexture(const tag_texture::DESC_S *pDesc, MTL::Texture *texture,
                IMTLD3D11Device *pDevice)
      : TResourceBase<tag_texture, IMTLBindable>(pDesc, pDevice),
        texture(texture), texture_handle(texture->gpuResourceID()) {}

  BindingRef UseBindable(uint64_t) override {
    return BindingRef(texture.ptr());
  };

  ArgumentData GetArgumentData() override {
    // rarely used, since texture is not directly accessed by pipeline
    return ArgumentData(texture_handle, texture.ptr());
  }

  bool GetContentionState(uint64_t) override { return true; };

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
      ERR("DeviceTexture: Failed to create SRV descriptor");
      return E_INVALIDARG;
    }
    // !DIRTY HACK
    if (this->desc.BindFlags & D3D11_BIND_DEPTH_STENCIL) {
      if (finalDesc.Format == DXGI_FORMAT_R32_FLOAT) {
        WARN("hack fired: interpret R32Float as Depth32Float");
        finalDesc.Format = DXGI_FORMAT_D32_FLOAT; // sad!
      }
      if (finalDesc.Format == DXGI_FORMAT_R16_UINT) {
        WARN("hack fired: interpret R16Uint as Depth16Unorm");
        finalDesc.Format = DXGI_FORMAT_D16_UNORM;
      }
      if (finalDesc.Format == DXGI_FORMAT_R16_UNORM) {
        WARN("hack fired: interpret R16Unorm as Depth16Unorm");
        finalDesc.Format = DXGI_FORMAT_D16_UNORM;
      }
    }
    Obj<MTL::Texture> view;
    if (FAILED(CreateMTLTextureView(this->m_parent, this->texture, &finalDesc,
                                    &view))) {
      ERR("DeviceTexture: Failed to create SRV texture view");
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
  typename tag::DESC_S finalDesc;
  if (FAILED(CreateMTLTextureDescriptor(pDevice, pDesc, &finalDesc,
                                        &textureDescriptor))) {
    return E_INVALIDARG;
  }
  auto texture = transfer(metal->newTexture(textureDescriptor));
  if (pInitialData) {
    initWithSubresourceData(texture, &finalDesc, pInitialData);
  }
  *ppTexture = ref(new DeviceTexture<tag>(&finalDesc, texture, pDevice));
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