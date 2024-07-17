#include "Metal/MTLPixelFormat.hpp"
#include "Metal/MTLResource.hpp"
#include "Metal/MTLTexture.hpp"
#include "d3d11_private.h"
#include "com/com_pointer.hpp"
#include "d3d11_device.hpp"
#include "d3d11_texture.hpp"
#include "dxmt_binding.hpp"
#include "mtld11_resource.hpp"

namespace dxmt {

#pragma region DeviceBuffer

class DeviceBuffer : public TResourceBase<tag_buffer, IMTLBindable> {
private:
  Obj<MTL::Buffer> buffer;
  uint64_t buffer_handle;
  bool structured;
  bool allow_raw_view;
  SIMPLE_RESIDENCY_TRACKER residency{};
  SIMPLE_OCCUPANCY_TRACKER occupancy{};

public:
  DeviceBuffer(const tag_buffer::DESC_S *desc,
               const D3D11_SUBRESOURCE_DATA *pInitialData,
               IMTLD3D11Device *device)
      : TResourceBase<tag_buffer, IMTLBindable>(desc, device) {
    auto metal = device->GetMTLDevice();
    buffer = transfer(metal->newBuffer(
        (desc->BindFlags & D3D11_BIND_UNORDERED_ACCESS) ? (desc->ByteWidth + 16)
                                                        : desc->ByteWidth,
        MTL::ResourceStorageModeManaged));
    if (pInitialData) {
      memcpy(buffer->contents(), pInitialData->pSysMem, desc->ByteWidth);
      buffer->didModifyRange({0, desc->ByteWidth});
    }
    buffer_handle = buffer->gpuAddress();
    structured = desc->MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    allow_raw_view =
        desc->MiscFlags & D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
  }

  BindingRef UseBindable(uint64_t seq_id) override {
    occupancy.MarkAsOccupied(seq_id);
    return BindingRef(static_cast<ID3D11DeviceChild *>(this), buffer.ptr());
  };

  ArgumentData GetArgumentData(SIMPLE_RESIDENCY_TRACKER **ppTracker) override {
    *ppTracker = &residency;
    return ArgumentData(buffer_handle);
  }

  bool GetContentionState(uint64_t finished_seq_id) override {
    return occupancy.IsOccupied(finished_seq_id);
  };

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

    BindingRef UseBindable(uint64_t seq_id) override {
      this->resource->occupancy.MarkAsOccupied(seq_id);
      return std::invoke(f, seq_id, static_cast<SRV *>(this));
    };

    ArgumentData
    GetArgumentData(SIMPLE_RESIDENCY_TRACKER **ppTracker) override {
      *ppTracker = &resource->residency;
      return argument_data;
    };

    void GetLogicalResourceOrView(REFIID riid,
                                  void **ppLogicalResource) override {
      QueryInterface(riid, ppLogicalResource);
    };
  };

  HRESULT
  CreateShaderResourceView(const D3D11_SHADER_RESOURCE_VIEW_DESC *pDesc,
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
    if (!ppView) {
      return S_FALSE;
    }
    if (structured) {
      if (finalDesc.Format != DXGI_FORMAT_UNKNOWN) {
        return E_INVALIDARG;
      }
      // StructuredBuffer
      auto offset =
          finalDesc.Buffer.FirstElement * this->desc.StructureByteStride;
      auto size = finalDesc.Buffer.NumElements;
      *ppView = ref(new SRV(
          &finalDesc, this, m_parent,
          ArgumentData(buffer_handle + offset, size),
          [ctx = Com(this), offset, size](uint64_t, auto _this) {
            return BindingRef(static_cast<ID3D11ShaderResourceView *>(_this),
                              ctx->buffer.ptr(), size, offset);
          }));
      return S_OK;
    }
    if (finalDesc.ViewDimension == D3D11_SRV_DIMENSION_BUFFEREX &&
        finalDesc.BufferEx.Flags & D3D11_BUFFEREX_SRV_FLAG_RAW) {
      if (!allow_raw_view)
        return E_INVALIDARG;
      D3D11_ASSERT(finalDesc.Format != DXGI_FORMAT_UNKNOWN);
      MTL_FORMAT_DESC metal_format;
      Com<IMTLDXGIAdatper> adapter;
      m_parent->GetAdapter(&adapter);
      if (FAILED(adapter->QueryFormatDesc(finalDesc.Format, &metal_format))) {
        return E_INVALIDARG;
      }
      auto offset = finalDesc.Buffer.FirstElement * metal_format.BytesPerTexel;
      auto size = finalDesc.Buffer.NumElements * metal_format.BytesPerTexel;
      *ppView = ref(new SRV(
          &finalDesc, this, m_parent,
          ArgumentData(buffer_handle + offset, size),
          [ctx = Com(this), offset, size](uint64_t, auto _this) {
            return BindingRef(static_cast<ID3D11ShaderResourceView *>(_this),
                              ctx->buffer.ptr(), size, offset);
          }));
      return S_OK;
    }

    Obj<MTL::Texture> view;
    if (FAILED(CreateMTLTextureView(this->m_parent, this->buffer, &finalDesc,
                                    &view))) {
      return E_FAIL;
    }
    *ppView = ref(new SRV(&finalDesc, this, m_parent,
                          ArgumentData(view->gpuResourceID(), view.ptr()),
                          [view = std::move(view)](uint64_t, auto _this) {
                            return BindingRef(
                                static_cast<ID3D11ShaderResourceView *>(_this),
                                view.ptr());
                          }));
    return S_OK;
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

    BindingRef UseBindable(uint64_t seq_id) override {
      this->resource->occupancy.MarkAsOccupied(seq_id);
      return std::invoke(f, seq_id, this);
    };

    ArgumentData
    GetArgumentData(SIMPLE_RESIDENCY_TRACKER **ppTracker) override {
      *ppTracker = &resource->residency;
      return argument_data;
    };

    void GetLogicalResourceOrView(REFIID riid,
                                  void **ppLogicalResource) override {
      QueryInterface(riid, ppLogicalResource);
    };
  };

  HRESULT
  CreateUnorderedAccessView(const D3D11_UNORDERED_ACCESS_VIEW_DESC *pDesc1,
                            ID3D11UnorderedAccessView **ppView) override {
    D3D11_UNORDERED_ACCESS_VIEW_DESC finalDesc;
    if (FAILED(ExtractEntireResourceViewDescription(&this->desc, pDesc1,
                                                    &finalDesc))) {
      ERR("DeviceBuffer: Failed to extract uav description");
      return E_INVALIDARG;
    }
    if (finalDesc.ViewDimension != D3D11_UAV_DIMENSION_BUFFER) {
      return E_INVALIDARG;
    }
    D3D11_ASSERT(finalDesc.Buffer.Flags < 2 && "TODO: uav counter");
    if (structured) {
      if (finalDesc.Format != DXGI_FORMAT_UNKNOWN) {
        return E_INVALIDARG;
      }
      // StructuredBuffer
      auto offset =
          finalDesc.Buffer.FirstElement * this->desc.StructureByteStride;
      auto size = finalDesc.Buffer.NumElements;
      *ppView = ref(new UAV(
          &finalDesc, this, m_parent,
          ArgumentData(buffer_handle + offset, size),
          [ctx = Com(this), offset, size](uint64_t, auto _this) {
            return BindingRef(static_cast<ID3D11UnorderedAccessView *>(_this),
                              ctx->buffer.ptr(), size, offset);
          }));
      return S_OK;
    } else {
      if (finalDesc.Buffer.Flags & D3D11_BUFFER_UAV_FLAG_RAW) {
        if (!allow_raw_view)
          return E_INVALIDARG;
        D3D11_ASSERT(finalDesc.Format != DXGI_FORMAT_UNKNOWN);
        MTL_FORMAT_DESC metal_format;
        Com<IMTLDXGIAdatper> adapter;
        m_parent->GetAdapter(&adapter);
        if (FAILED(adapter->QueryFormatDesc(finalDesc.Format, &metal_format))) {
          return E_INVALIDARG;
        }
        auto offset =
            finalDesc.Buffer.FirstElement * metal_format.BytesPerTexel;
        auto size = finalDesc.Buffer.NumElements * metal_format.BytesPerTexel;
        *ppView = ref(new UAV(
            &finalDesc, this, m_parent,
            ArgumentData(buffer_handle + offset, size),
            [ctx = Com(this), offset, size](uint64_t, auto _this) {
              return BindingRef(static_cast<ID3D11UnorderedAccessView *>(_this),
                                ctx->buffer.ptr(), size, offset);
            }));
        return S_OK;
      } else {
        Obj<MTL::Texture> view;
        if (FAILED(CreateMTLTextureView(this->m_parent, this->buffer,
                                        &finalDesc, &view))) {
          return E_FAIL;
        }
        *ppView = ref(new UAV(
            &finalDesc, this, m_parent,
            ArgumentData(view->gpuResourceID(), view.ptr()),
            [view = std::move(view)](uint64_t, auto _this) {
              return BindingRef(static_cast<ID3D11UnorderedAccessView *>(_this),
                                view.ptr());
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

  void OnSetDebugObjectName(LPCSTR Name) override {
    if (!Name) {
      return;
    }
    buffer->setLabel(NS::String::string((char *)Name, NS::ASCIIStringEncoding));
  }
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
  SIMPLE_RESIDENCY_TRACKER residency{};
  SIMPLE_OCCUPANCY_TRACKER occupancy{};

  using SRVBase =
      TResourceViewBase<tag_shader_resource_view<DeviceTexture<tag_texture>>,
                        IMTLBindable>;
  class TextureSRV : public SRVBase {
  private:
    Obj<MTL::Texture> view;
    MTL::ResourceID view_handle;
    SIMPLE_RESIDENCY_TRACKER tracker{};

  public:
    TextureSRV(MTL::Texture *view,
               const tag_shader_resource_view<>::DESC_S *pDesc,
               DeviceTexture *pResource, IMTLD3D11Device *pDevice)
        : SRVBase(pDesc, pResource, pDevice), view(view),
          view_handle(view->gpuResourceID()) {}

    BindingRef UseBindable(uint64_t seq_id) override {
      this->resource->occupancy.MarkAsOccupied(seq_id);
      return BindingRef(static_cast<ID3D11View *>(this), view.ptr());
    };

    ArgumentData
    GetArgumentData(SIMPLE_RESIDENCY_TRACKER **ppTracker) override {
      *ppTracker = &tracker;
      return ArgumentData(view_handle, view.ptr());
    };

    void GetLogicalResourceOrView(REFIID riid,
                                  void **ppLogicalResource) override {
      this->QueryInterface(riid, ppLogicalResource);
    };

    void OnSetDebugObjectName(LPCSTR Name) override {
      if (!Name) {
        return;
      }
      view->setLabel(NS::String::string((char *)Name, NS::ASCIIStringEncoding));
    }
  };

  using UAVBase =
      TResourceViewBase<tag_unordered_access_view<DeviceTexture<tag_texture>>,
                        IMTLBindable>;
  class TextureUAV : public UAVBase {
  private:
    Obj<MTL::Texture> view;
    MTL::ResourceID view_handle;
    SIMPLE_RESIDENCY_TRACKER tracker{};

  public:
    TextureUAV(MTL::Texture *view,
               const tag_unordered_access_view<>::DESC_S *pDesc,
               DeviceTexture *pResource, IMTLD3D11Device *pDevice)
        : UAVBase(pDesc, pResource, pDevice), view(view),
          view_handle(view->gpuResourceID()) {}

    BindingRef UseBindable(uint64_t seq_id) override {
      this->resource->occupancy.MarkAsOccupied(seq_id);
      return BindingRef(static_cast<ID3D11View *>(this), view.ptr());
    };

    ArgumentData
    GetArgumentData(SIMPLE_RESIDENCY_TRACKER **ppTracker) override {
      *ppTracker = &tracker;
      return ArgumentData(view_handle, view.ptr());
    };

    void GetLogicalResourceOrView(REFIID riid,
                                  void **ppLogicalResource) override {
      this->QueryInterface(riid, ppLogicalResource);
    };

    void OnSetDebugObjectName(LPCSTR Name) override {
      if (!Name) {
        return;
      }
      view->setLabel(NS::String::string((char *)Name, NS::ASCIIStringEncoding));
    }
  };

  using RTVBase =
      TResourceViewBase<tag_render_target_view<DeviceTexture<tag_texture>>>;
  class TextureRTV : public RTVBase {
  private:
    Obj<MTL::Texture> view;
    MTL::PixelFormat view_pixel_format;
    MTL_RENDER_TARGET_VIEW_DESC mtl_rtv_desc;

  public:
    TextureRTV(MTL::Texture *view,
               const tag_render_target_view<>::DESC_S *pDesc,
               DeviceTexture *pResource, IMTLD3D11Device *pDevice,
               const MTL_RENDER_TARGET_VIEW_DESC &mtl_rtv_desc)
        : RTVBase(pDesc, pResource, pDevice), view(view),
          view_pixel_format(view->pixelFormat()), mtl_rtv_desc(mtl_rtv_desc) {}

    MTL::PixelFormat GetPixelFormat() final { return view_pixel_format; }

    MTL_RENDER_TARGET_VIEW_DESC *GetRenderTargetProps() final {
      return &mtl_rtv_desc;
    };

    BindingRef GetBinding(uint64_t seq_id) final {
      this->resource->occupancy.MarkAsOccupied(seq_id);
      return BindingRef(static_cast<ID3D11View *>(this), view.ptr());
    }

    void OnSetDebugObjectName(LPCSTR Name) override {
      if (!Name) {
        return;
      }
      view->setLabel(NS::String::string((char *)Name, NS::ASCIIStringEncoding));
    }
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

    BindingRef GetBinding(uint64_t seq_id) final {
      this->resource->occupancy.MarkAsOccupied(seq_id);
      return BindingRef(static_cast<ID3D11View *>(this), view.ptr());
    }

    void OnSetDebugObjectName(LPCSTR Name) override {
      if (!Name) {
        return;
      }
      view->setLabel(NS::String::string((char *)Name, NS::ASCIIStringEncoding));
    }
  };

public:
  DeviceTexture(const tag_texture::DESC_S *pDesc, MTL::Texture *texture,
                IMTLD3D11Device *pDevice)
      : TResourceBase<tag_texture, IMTLBindable>(pDesc, pDevice),
        texture(texture), texture_handle(texture->gpuResourceID()) {}

  BindingRef UseBindable(uint64_t seq_id) override {
    occupancy.MarkAsOccupied(seq_id);
    return BindingRef(static_cast<ID3D11Resource *>(this), texture.ptr());
  };

  ArgumentData GetArgumentData(SIMPLE_RESIDENCY_TRACKER **ppTracker) override {
    *ppTracker = &residency;
    // rarely used, since texture is not directly accessed by pipeline
    return ArgumentData(texture_handle, texture.ptr());
  }

  bool GetContentionState(uint64_t finished_seq_id) override {
    return occupancy.IsOccupied(finished_seq_id);
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
    MTL_RENDER_TARGET_VIEW_DESC mtl_rtv_desc;
    if (FAILED(CreateMTLRenderTargetView(this->m_parent, this->texture,
                                         &finalDesc, &view, &mtl_rtv_desc))) {
      return E_FAIL;
    }
    if (ppView) {
      *ppView = ref(
          new TextureRTV(view, &finalDesc, this, this->m_parent, mtl_rtv_desc));
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
        finalDesc.Format = DXGI_FORMAT_D32_FLOAT; // sad!
      }
      if (finalDesc.Format == DXGI_FORMAT_R16_UINT) {
        finalDesc.Format = DXGI_FORMAT_D16_UNORM;
      }
      if (finalDesc.Format == DXGI_FORMAT_R16_UNORM) {
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

  void OnSetDebugObjectName(LPCSTR Name) override {
    if (!Name) {
      return;
    }
    texture->setLabel(
        NS::String::string((char *)Name, NS::ASCIIStringEncoding));
  }
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