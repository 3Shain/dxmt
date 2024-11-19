#include "Metal/MTLPixelFormat.hpp"
#include "Metal/MTLTexture.hpp"
#include "com/com_pointer.hpp"
#include "d3d11_device.hpp"
#include "d3d11_texture.hpp"
#include "d3d11_view.hpp"
#include "dxmt_binding.hpp"
#include "dxmt_texture.hpp"
#include "mtld11_resource.hpp"

namespace dxmt {

#pragma region DeviceTexture

template <typename tag_texture>
class DeviceTexture : public TResourceBase<tag_texture, IMTLBindable, IMTLMinLODClampable> {
private:
  std::unique_ptr<Texture> underlying_texture_;
  Obj<MTL::Texture> texture;
  MTL::ResourceID texture_handle;
  SIMPLE_RESIDENCY_TRACKER residency{};
  SIMPLE_OCCUPANCY_TRACKER occupancy{};
  float min_lod = 0.0;

  using SRVBase =
      TResourceViewBase<tag_shader_resource_view<DeviceTexture<tag_texture>>,
                        IMTLBindable>;
  class TextureSRV : public SRVBase {
  private:
    TextureViewKey view_key_;
    SIMPLE_RESIDENCY_TRACKER tracker{};

  public:
    TextureSRV(TextureViewKey view_key,
               const tag_shader_resource_view<>::DESC1 *pDesc,
               DeviceTexture *pResource, MTLD3D11Device *pDevice)
        : SRVBase(pDesc, pResource, pDevice), view_key_(view_key) {}

    BindingRef UseBindable(uint64_t seq_id) override {
      this->resource->occupancy.MarkAsOccupied(seq_id);
      auto view = this->resource->underlying_texture_->view(view_key_);
      return BindingRef(static_cast<ID3D11View *>(this), view);
    };

    ArgumentData
    GetArgumentData(SIMPLE_RESIDENCY_TRACKER **ppTracker) override {
      *ppTracker = &tracker;
      auto view = this->resource->underlying_texture_->view(view_key_);
      return ArgumentData(view->gpuResourceID(), this->resource->min_lod);
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
    TextureViewKey view_key_;
    SIMPLE_RESIDENCY_TRACKER tracker{};

  public:
    TextureUAV(TextureViewKey view_key,
               const tag_unordered_access_view<>::DESC1 *pDesc,
               DeviceTexture *pResource, MTLD3D11Device *pDevice)
        : UAVBase(pDesc, pResource, pDevice), view_key_(view_key){}

    BindingRef UseBindable(uint64_t seq_id) override {
      this->resource->occupancy.MarkAsOccupied(seq_id);
      auto view = this->resource->underlying_texture_->view(view_key_);
      return BindingRef(static_cast<ID3D11View *>(this), view);
    };

    ArgumentData
    GetArgumentData(SIMPLE_RESIDENCY_TRACKER **ppTracker) override {
      *ppTracker = &tracker;
      auto view = this->resource->underlying_texture_->view(view_key_);
      return ArgumentData(view->gpuResourceID(), this->resource->min_lod);
    };

    void GetLogicalResourceOrView(REFIID riid,
                                  void **ppLogicalResource) override {
      this->QueryInterface(riid, ppLogicalResource);
    };

    virtual uint64_t SwapCounter(uint64_t handle) override { return handle; };
  };

  using RTVBase =
      TResourceViewBase<tag_render_target_view<DeviceTexture<tag_texture>>>;
  class TextureRTV : public RTVBase {
  private:
    Obj<MTL::Texture> view;
    MTL::PixelFormat view_pixel_format;
    MTL_RENDER_PASS_ATTACHMENT_DESC attachment_desc;

  public:
    TextureRTV(MTL::Texture *view, const tag_render_target_view<>::DESC1 *pDesc,
               DeviceTexture *pResource, MTLD3D11Device *pDevice,
               const MTL_RENDER_PASS_ATTACHMENT_DESC &mtl_rtv_desc)
        : RTVBase(pDesc, pResource, pDevice), view(view),
          view_pixel_format(view->pixelFormat()), attachment_desc(mtl_rtv_desc) {}

    MTL::PixelFormat GetPixelFormat() final { return view_pixel_format; }

    MTL_RENDER_PASS_ATTACHMENT_DESC &GetAttachmentDesc() final {
      return attachment_desc;
    };

    BindingRef UseBindable(uint64_t seq_id) final {
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
    MTL_RENDER_PASS_ATTACHMENT_DESC attachment_desc;

  public:
    TextureDSV(MTL::Texture *view, const tag_depth_stencil_view<>::DESC1 *pDesc,
               DeviceTexture *pResource, MTLD3D11Device *pDevice,
               const MTL_RENDER_PASS_ATTACHMENT_DESC &attachment_desc)
        : DSVBase(pDesc, pResource, pDevice), view(view),
          view_pixel_format(view->pixelFormat()),
          attachment_desc(attachment_desc) {}

    MTL::PixelFormat GetPixelFormat() final { return view_pixel_format; }

    BindingRef UseBindable(uint64_t seq_id) final {
      this->resource->occupancy.MarkAsOccupied(seq_id);
      return BindingRef(static_cast<ID3D11View *>(this), view.ptr());
    }

    MTL_RENDER_PASS_ATTACHMENT_DESC &GetAttachmentDesc() final {
      return attachment_desc;
    };

    void OnSetDebugObjectName(LPCSTR Name) override {
      if (!Name) {
        return;
      }
      view->setLabel(NS::String::string((char *)Name, NS::ASCIIStringEncoding));
    }
  };

public:
  DeviceTexture(const tag_texture::DESC1 *pDesc, std::unique_ptr<Texture> &&u_texture, MTLD3D11Device *pDevice) :
      TResourceBase<tag_texture, IMTLBindable, IMTLMinLODClampable>(*pDesc, pDevice),
      underlying_texture_(std::move(u_texture)),
      texture(underlying_texture_->current()->texture()),
      texture_handle(texture->gpuResourceID()) {}

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
    MTL_RENDER_PASS_ATTACHMENT_DESC attachment_desc;
    if (FAILED(CreateMTLRenderTargetView(this->m_parent, this->texture,
                                         &finalDesc, &view, attachment_desc))) {
      return E_FAIL;
    }
    if (ppView) {
      *ppView = ref(new TextureRTV(view, &finalDesc, this, this->m_parent,
                                   attachment_desc));
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
    MTL_RENDER_PASS_ATTACHMENT_DESC attachment_desc;
    if (FAILED(CreateMTLDepthStencilView(this->m_parent, this->texture,
                                         &finalDesc, &view, attachment_desc))) {
      return E_FAIL;
    }
    if (ppView) {
      *ppView = ref(new TextureDSV(view, &finalDesc, this, this->m_parent, attachment_desc));
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
    TextureViewDescriptor descriptor;
    uint32_t arraySize;
    if constexpr (std::is_same_v<typename tag_texture::DESC1, D3D11_TEXTURE3D_DESC1>) {
      arraySize = this->desc.Depth;
    } else {
      arraySize = this->desc.ArraySize;
    }
    if (FAILED(InitializeAndNormalizeViewDescriptor(
            this->m_parent, this->desc.MipLevels, arraySize, this->underlying_texture_.get(), finalDesc, descriptor
        ))) {
      ERR("DeviceTexture: Failed to create texture SRV");
      return E_FAIL;
    }
    if (!ppView) {
      return S_FALSE;
    }
    TextureViewKey key = underlying_texture_->createView(descriptor);
    *ppView = ref(new TextureSRV(key, &finalDesc, this, this->m_parent));
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
    TextureViewDescriptor descriptor;
    uint32_t arraySize;
    if constexpr (std::is_same_v<typename tag_texture::DESC1, D3D11_TEXTURE3D_DESC1>) {
      arraySize = this->desc.Depth;
    } else {
      arraySize = this->desc.ArraySize;
    }
    if (FAILED(InitializeAndNormalizeViewDescriptor(
            this->m_parent, this->desc.MipLevels, arraySize, this->underlying_texture_.get(), finalDesc, descriptor
        ))) {
      ERR("DeviceTexture: Failed to create texture UAV");
      return E_FAIL;
    }
    if (!ppView) {
      return S_FALSE;
    }
    TextureViewKey key = underlying_texture_->createView(descriptor);
    *ppView = ref(new TextureUAV(key, &finalDesc, this, this->m_parent));
    return S_OK;
  };

  void OnSetDebugObjectName(LPCSTR Name) override {
    if (!Name) {
      return;
    }
    texture->setLabel(
        NS::String::string((char *)Name, NS::ASCIIStringEncoding));
  }

  void SetMinLOD(float MinLod) override { min_lod = MinLod; }

  float GetMinLOD() override { return min_lod; }
};

template <typename tag>
HRESULT CreateDeviceTextureInternal(MTLD3D11Device *pDevice,
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
  auto texture = std::make_unique<Texture>(std::move(textureDescriptor), metal);
  Flags<TextureAllocationFlag> flags;
  flags.set(TextureAllocationFlag::GpuManaged);
  if (finalDesc.Usage == D3D11_USAGE_IMMUTABLE)
    flags.set(TextureAllocationFlag::GpuReadonly);
  auto default_allocation = texture->allocate(flags);
  if (pInitialData) {
    initWithSubresourceData(default_allocation->texture(), &finalDesc, pInitialData);
  }
  texture->rename(std::move(default_allocation));
  *ppTexture = ref(new DeviceTexture<tag>(&finalDesc, std::move(texture), pDevice));
  return S_OK;
}

HRESULT
CreateDeviceTexture1D(MTLD3D11Device *pDevice,
                      const D3D11_TEXTURE1D_DESC *pDesc,
                      const D3D11_SUBRESOURCE_DATA *pInitialData,
                      ID3D11Texture1D **ppTexture) {
  return CreateDeviceTextureInternal<tag_texture_1d>(pDevice, pDesc,
                                                     pInitialData, ppTexture);
}

HRESULT
CreateDeviceTexture2D(MTLD3D11Device *pDevice,
                      const D3D11_TEXTURE2D_DESC1 *pDesc,
                      const D3D11_SUBRESOURCE_DATA *pInitialData,
                      ID3D11Texture2D1 **ppTexture) {
  return CreateDeviceTextureInternal<tag_texture_2d>(pDevice, pDesc,
                                                     pInitialData, ppTexture);
}

HRESULT
CreateDeviceTexture3D(MTLD3D11Device *pDevice,
                      const D3D11_TEXTURE3D_DESC1 *pDesc,
                      const D3D11_SUBRESOURCE_DATA *pInitialData,
                      ID3D11Texture3D1 **ppTexture) {
  return CreateDeviceTextureInternal<tag_texture_3d>(pDevice, pDesc,
                                                     pInitialData, ppTexture);
}

#pragma endregion

} // namespace dxmt