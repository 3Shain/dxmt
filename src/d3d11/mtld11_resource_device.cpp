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
  DeviceBuffer(const tag_buffer::DESC1 *pDesc,
               const D3D11_SUBRESOURCE_DATA *pInitialData,
               IMTLD3D11Device *device)
      : TResourceBase<tag_buffer, IMTLBindable>(*pDesc, device) {
    auto metal = device->GetMTLDevice();
    buffer = transfer(
        metal->newBuffer((pDesc->BindFlags & D3D11_BIND_UNORDERED_ACCESS)
                             ? (pDesc->ByteWidth + 16)
                             : pDesc->ByteWidth,
                         MTL::ResourceStorageModeManaged));
    if (pInitialData) {
      memcpy(buffer->contents(), pInitialData->pSysMem, pDesc->ByteWidth);
      buffer->didModifyRange({0, pDesc->ByteWidth});
    }
    buffer_handle = buffer->gpuAddress();
    structured = pDesc->MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    allow_raw_view =
        pDesc->MiscFlags & D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
  }

  BindingRef UseBindable(uint64_t seq_id) override {
    occupancy.MarkAsOccupied(seq_id);
    return BindingRef(static_cast<ID3D11DeviceChild *>(this), buffer.ptr(),
                      desc.ByteWidth, 0);
  };

  ArgumentData GetArgumentData(SIMPLE_RESIDENCY_TRACKER **ppTracker) override {
    *ppTracker = &residency;
    return ArgumentData(buffer_handle, desc.ByteWidth);
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
    SRV(const tag_shader_resource_view<>::DESC1 *pDesc, DeviceBuffer *pResource,
        IMTLD3D11Device *pDevice, ArgumentData argument_data, F &&fn)
        : SRVBase(pDesc, pResource, pDevice), argument_data(argument_data),
          f(std::forward<F>(fn)) {}

    BindingRef UseBindable(uint64_t seq_id) override {
      this->resource->occupancy.MarkAsOccupied(seq_id);
      return std::invoke(f, static_cast<SRV *>(this));
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
  CreateShaderResourceView(const D3D11_SHADER_RESOURCE_VIEW_DESC1 *pDesc,
                           ID3D11ShaderResourceView1 **ppView) override {
    D3D11_SHADER_RESOURCE_VIEW_DESC1 finalDesc;
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
      uint32_t offset, size;
      CalculateBufferViewOffsetAndSize(
          desc, desc.StructureByteStride, finalDesc.Buffer.FirstElement,
          finalDesc.Buffer.NumElements, offset, size);
      *ppView = ref(new SRV(
          &finalDesc, this, m_parent,
          ArgumentData(buffer_handle + offset, size),
          [ctx = Com(this), offset, size](auto _this) {
            return BindingRef(static_cast<ID3D11ShaderResourceView *>(_this),
                              ctx->buffer.ptr(), size, offset);
          }));
      return S_OK;
    }
    if (finalDesc.ViewDimension == D3D11_SRV_DIMENSION_BUFFEREX &&
        finalDesc.BufferEx.Flags & D3D11_BUFFEREX_SRV_FLAG_RAW) {
      if (!allow_raw_view)
        return E_INVALIDARG;
      if (finalDesc.Format != DXGI_FORMAT_R32_TYPELESS)
        return E_INVALIDARG;
      uint32_t offset, size;
      CalculateBufferViewOffsetAndSize(
          desc, sizeof(uint32_t), finalDesc.Buffer.FirstElement,
          finalDesc.Buffer.NumElements, offset, size);
      *ppView = ref(new SRV(
          &finalDesc, this, m_parent,
          ArgumentData(buffer_handle + offset, size),
          [ctx = Com(this), offset, size](auto _this) {
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
    uint32_t offset = view->bufferOffset();
    uint32_t size = view->bufferBytesPerRow();
    *ppView = ref(new SRV(&finalDesc, this, m_parent,
                          ArgumentData(view->gpuResourceID(), view.ptr(),
                                       buffer_handle + offset, size),
                          [view = std::move(view), buffer = this->buffer.ptr(),
                           size, offset](auto _this) {
                            return BindingRef(
                                static_cast<ID3D11ShaderResourceView *>(_this),
                                view.ptr(), buffer, size, offset);
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
    UAV(const tag_unordered_access_view<>::DESC1 *pDesc,
        DeviceBuffer *pResource, IMTLD3D11Device *pDevice,
        ArgumentData argument_data, F &&fn)
        : UAVBase(pDesc, pResource, pDevice), argument_data(argument_data),
          f(std::forward<F>(fn)) {}

    BindingRef UseBindable(uint64_t seq_id) override {
      this->resource->occupancy.MarkAsOccupied(seq_id);
      return std::invoke(f, this);
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

    virtual uint64_t SwapCounter(uint64_t handle) override { return handle; };
  };

  template <typename A, typename F> class UAVWithCounter : public UAVBase {
  private:
    A a;
    F f;

  public:
    uint64_t counter_handle;

    UAVWithCounter(const tag_unordered_access_view<>::DESC1 *pDesc,
                   DeviceBuffer *pResource, IMTLD3D11Device *pDevice, A &&a,
                   F &&fn)
        : UAVBase(pDesc, pResource, pDevice), a(std::forward<A>(a)),
          f(std::forward<F>(fn)) {
      counter_handle = pDevice->AloocateCounter(0);
    }

    ~UAVWithCounter() { m_parent->DiscardCounter(counter_handle); }

    BindingRef UseBindable(uint64_t seq_id) override {
      this->resource->occupancy.MarkAsOccupied(seq_id);
      return std::invoke(f, this);
    };

    ArgumentData
    GetArgumentData(SIMPLE_RESIDENCY_TRACKER **ppTracker) override {
      *ppTracker = &resource->residency;
      return std::invoke(a, this);
    };

    void GetLogicalResourceOrView(REFIID riid,
                                  void **ppLogicalResource) override {
      QueryInterface(riid, ppLogicalResource);
    };

    virtual uint64_t SwapCounter(uint64_t handle) override {
      uint64_t current = counter_handle;
      if (~handle != 0) {
        counter_handle = handle;
      }
      return current;
    };
  };

  HRESULT
  CreateUnorderedAccessView(const D3D11_UNORDERED_ACCESS_VIEW_DESC1 *pDesc1,
                            ID3D11UnorderedAccessView1 **ppView) override {
    D3D11_UNORDERED_ACCESS_VIEW_DESC1 finalDesc;
    if (FAILED(ExtractEntireResourceViewDescription(&this->desc, pDesc1,
                                                    &finalDesc))) {
      ERR("DeviceBuffer: Failed to extract uav description");
      return E_INVALIDARG;
    }
    if (finalDesc.ViewDimension != D3D11_UAV_DIMENSION_BUFFER) {
      return E_INVALIDARG;
    }
    if (structured) {
      if (finalDesc.Format != DXGI_FORMAT_UNKNOWN) {
        return E_INVALIDARG;
      }
      bool with_counter =
          finalDesc.Buffer.Flags &
          (D3D11_BUFFER_UAV_FLAG_APPEND | D3D11_BUFFER_UAV_FLAG_COUNTER);
      // StructuredBuffer
      uint32_t offset, size;
      CalculateBufferViewOffsetAndSize(
          desc, desc.StructureByteStride, finalDesc.Buffer.FirstElement,
          finalDesc.Buffer.NumElements, offset, size);
      if (with_counter) {
        *ppView = ref(new UAVWithCounter(
            &finalDesc, this, m_parent,
            [buffer_handle = this->buffer_handle, offset, size](auto _this) {
              return ArgumentData(buffer_handle + offset, size,
                                  _this->counter_handle);
            },
            [ctx = Com(this), offset, size](auto _this) {
              return BindingRef(static_cast<ID3D11UnorderedAccessView *>(_this),
                                ctx->buffer.ptr(), size, offset,
                                _this->counter_handle);
            }));
      } else {
        *ppView = ref(new UAV(
            &finalDesc, this, m_parent,
            ArgumentData(buffer_handle + offset, size),
            [ctx = Com(this), offset, size](auto _this) {
              return BindingRef(static_cast<ID3D11UnorderedAccessView *>(_this),
                                ctx->buffer.ptr(), size, offset);
            }));
      }
      return S_OK;
    }
    if (finalDesc.Buffer.Flags & D3D11_BUFFER_UAV_FLAG_RAW) {
      if (!allow_raw_view)
        return E_INVALIDARG;
      if (finalDesc.Format != DXGI_FORMAT_R32_TYPELESS)
        return E_INVALIDARG;
      uint32_t offset, size;
      CalculateBufferViewOffsetAndSize(
          desc, sizeof(uint32_t), finalDesc.Buffer.FirstElement,
          finalDesc.Buffer.NumElements, offset, size);
      *ppView = ref(new UAV(
          &finalDesc, this, m_parent,
          ArgumentData(buffer_handle + offset, size),
          [ctx = Com(this), offset, size](auto _this) {
            return BindingRef(static_cast<ID3D11UnorderedAccessView *>(_this),
                              ctx->buffer.ptr(), size, offset);
          }));
      return S_OK;
    }
    Obj<MTL::Texture> view;
    if (FAILED(CreateMTLTextureView(this->m_parent, this->buffer, &finalDesc,
                                    &view))) {
      return E_FAIL;
    }
    uint32_t offset = view->bufferOffset();
    uint32_t size = view->bufferBytesPerRow();
    *ppView = ref(new UAV(&finalDesc, this, m_parent,
                          ArgumentData(view->gpuResourceID(), view.ptr(),
                                       buffer_handle + offset, size),
                          [view = std::move(view), buffer = this->buffer.ptr(),
                           size, offset](auto _this) {
                            return BindingRef(
                                static_cast<ID3D11UnorderedAccessView *>(_this),
                                view.ptr(), buffer, size, offset);
                          }));
    return S_OK;
  };

  HRESULT CreateRenderTargetView(const D3D11_RENDER_TARGET_VIEW_DESC1 *desc,
                                 ID3D11RenderTargetView1 **ppView) override {
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
               const tag_shader_resource_view<>::DESC1 *pDesc,
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
               const tag_unordered_access_view<>::DESC1 *pDesc,
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

    virtual uint64_t SwapCounter(uint64_t handle) override { return handle; };
  };

  using RTVBase =
      TResourceViewBase<tag_render_target_view<DeviceTexture<tag_texture>>>;
  class TextureRTV : public RTVBase {
  private:
    Obj<MTL::Texture> view;
    MTL::PixelFormat view_pixel_format;
    MTL_RENDER_TARGET_VIEW_DESC mtl_rtv_desc;

  public:
    TextureRTV(MTL::Texture *view, const tag_render_target_view<>::DESC1 *pDesc,
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
    TextureDSV(MTL::Texture *view, const tag_depth_stencil_view<>::DESC1 *pDesc,
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
  DeviceTexture(const tag_texture::DESC1 *pDesc, MTL::Texture *texture,
                IMTLD3D11Device *pDevice)
      : TResourceBase<tag_texture, IMTLBindable>(*pDesc, pDevice),
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

  HRESULT CreateRenderTargetView(const D3D11_RENDER_TARGET_VIEW_DESC1 *pDesc,
                                 ID3D11RenderTargetView1 **ppView) override {
    D3D11_RENDER_TARGET_VIEW_DESC1 finalDesc;
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

  HRESULT
  CreateShaderResourceView(const D3D11_SHADER_RESOURCE_VIEW_DESC1 *pDesc,
                           ID3D11ShaderResourceView1 **ppView) override {
    D3D11_SHADER_RESOURCE_VIEW_DESC1 finalDesc;
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
  CreateUnorderedAccessView(const D3D11_UNORDERED_ACCESS_VIEW_DESC1 *pDesc,
                            ID3D11UnorderedAccessView1 **ppView) override {
    D3D11_UNORDERED_ACCESS_VIEW_DESC1 finalDesc;
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
                                    const typename tag::DESC1 *pDesc,
                                    const D3D11_SUBRESOURCE_DATA *pInitialData,
                                    typename tag::COM_IMPL **ppTexture) {
  auto metal = pDevice->GetMTLDevice();
  Obj<MTL::TextureDescriptor> textureDescriptor;
  typename tag::DESC1 finalDesc;
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
                      const D3D11_TEXTURE2D_DESC1 *pDesc,
                      const D3D11_SUBRESOURCE_DATA *pInitialData,
                      ID3D11Texture2D1 **ppTexture) {
  return CreateDeviceTextureInternal<tag_texture_2d>(pDevice, pDesc,
                                                     pInitialData, ppTexture);
}

HRESULT
CreateDeviceTexture3D(IMTLD3D11Device *pDevice,
                      const D3D11_TEXTURE3D_DESC1 *pDesc,
                      const D3D11_SUBRESOURCE_DATA *pInitialData,
                      ID3D11Texture3D1 **ppTexture) {
  return CreateDeviceTextureInternal<tag_texture_3d>(pDevice, pDesc,
                                                     pInitialData, ppTexture);
}

#pragma endregion

} // namespace dxmt