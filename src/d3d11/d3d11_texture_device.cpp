#include "com/com_pointer.hpp"
#include "d3d11_device.hpp"
#include "d3d11_texture.hpp"
#include "d3d11_view.hpp"
#include "dxmt_dynamic.hpp"
#include "dxmt_staging.hpp"
#include "dxmt_texture.hpp"
#include "d3d11_resource.hpp"

namespace dxmt {

#pragma region DeviceTexture

template <typename tag_texture>
class DeviceTexture : public TResourceBase<tag_texture, IMTLMinLODClampable> {
private:
  Rc<Texture> underlying_texture_;
  Rc<RenamableTexturePool> renamable_;
  float min_lod = 0.0;
  D3DKMT_HANDLE kmt_handle_ = 0;

  using SRVBase =
      TResourceViewBase<tag_shader_resource_view<DeviceTexture<tag_texture>>>;
  class TextureSRV : public SRVBase {
  private:
    TextureViewKey view_key_;

  public:
    TextureSRV(TextureViewKey view_key,
               const tag_shader_resource_view<>::DESC1 *pDesc,
               DeviceTexture *pResource, MTLD3D11Device *pDevice)
        : SRVBase(pDesc, pResource, pDevice), view_key_(view_key) {}

    Rc<Buffer> buffer() final { return {}; };
    Rc<Texture> texture() final { return this->resource->underlying_texture_; };
    unsigned viewId() final { return view_key_;};
    BufferSlice bufferSlice() final { return {};}
  };

  using UAVBase =
      TResourceViewBase<tag_unordered_access_view<DeviceTexture<tag_texture>>>;
  class TextureUAV : public UAVBase {
  private:
    TextureViewKey view_key_;

  public:
    TextureUAV(TextureViewKey view_key,
               const tag_unordered_access_view<>::DESC1 *pDesc,
               DeviceTexture *pResource, MTLD3D11Device *pDevice)
        : UAVBase(pDesc, pResource, pDevice), view_key_(view_key){}

    Rc<Buffer> buffer() final { return {}; };
    Rc<Texture> texture() final { return this->resource->underlying_texture_; };
    unsigned viewId() final { return view_key_;};
    BufferSlice bufferSlice() final { return {};}
    Rc<Buffer> counter() final { return {}; };
  };

  using RTVBase =
      TResourceViewBase<tag_render_target_view<DeviceTexture<tag_texture>>>;
  class TextureRTV : public RTVBase {
  private:
    TextureViewKey view_key_;
    WMTPixelFormat view_format_;
    MTL_RENDER_PASS_ATTACHMENT_DESC attachment_desc;

  public:
    TextureRTV(
        TextureViewKey view_key, WMTPixelFormat view_format, const tag_render_target_view<>::DESC1 *pDesc,
        DeviceTexture *pResource, MTLD3D11Device *pDevice, const MTL_RENDER_PASS_ATTACHMENT_DESC &mtl_rtv_desc
    ) :
        RTVBase(pDesc, pResource, pDevice),
        view_key_(view_key),
        view_format_(view_format),
        attachment_desc(mtl_rtv_desc) {}

    WMTPixelFormat
    pixelFormat() final {
      return view_format_;
    }

    MTL_RENDER_PASS_ATTACHMENT_DESC &description() final {
      return attachment_desc;
    };

    Rc<Texture> texture() final {
      return this->resource->underlying_texture_;
    }

    unsigned viewId() final {
      return view_key_;
    }
  };

  using DSVBase =
      TResourceViewBase<tag_depth_stencil_view<DeviceTexture<tag_texture>>>;
  class TextureDSV : public DSVBase {
  private:
    TextureViewKey view_key_;
    WMTPixelFormat view_format_;
    MTL_RENDER_PASS_ATTACHMENT_DESC attachment_desc;

  public:
    TextureDSV(
        TextureViewKey view_key, WMTPixelFormat view_format, const tag_depth_stencil_view<>::DESC1 *pDesc,
        DeviceTexture *pResource, MTLD3D11Device *pDevice, const MTL_RENDER_PASS_ATTACHMENT_DESC &attachment_desc
    ) :
        DSVBase(pDesc, pResource, pDevice),
        view_key_(view_key),
        view_format_(view_format),
        attachment_desc(attachment_desc) {}

    WMTPixelFormat
    pixelFormat() final {
      return view_format_;
    }

    MTL_RENDER_PASS_ATTACHMENT_DESC &description() final {
      return attachment_desc;
    };

    UINT readonlyFlags() final {
      return this->desc.Flags;
    };

    Rc<Texture> texture() final {
      return this->resource->underlying_texture_;
    }

    unsigned viewId() final {
      return view_key_;
    }

    dxmt::Rc<dxmt::RenamableTexturePool> renamable() final {
      return this->resource->renamable_;
    }
  };

public:
  DeviceTexture(const tag_texture::DESC1 *pDesc, Rc<Texture> &&u_texture, MTLD3D11Device *pDevice) :
      TResourceBase<tag_texture, IMTLMinLODClampable>(*pDesc, pDevice),
      underlying_texture_(std::move(u_texture)) {}

  DeviceTexture(
      const tag_texture::DESC1 *pDesc, Rc<Texture> &&u_texture, Rc<RenamableTexturePool> &&renamable,
      MTLD3D11Device *pDevice
  ) :
      TResourceBase<tag_texture, IMTLMinLODClampable>(*pDesc, pDevice),
      underlying_texture_(std::move(u_texture)),
      renamable_(std::move(renamable)) {}

  DeviceTexture(
      const tag_texture::DESC1 *pDesc, Rc<Texture> &&u_texture, UINT hKMT,
      MTLD3D11Device *pDevice
  ) :
      TResourceBase<tag_texture, IMTLMinLODClampable>(*pDesc, pDevice),
      underlying_texture_(std::move(u_texture)), kmt_handle_(hKMT) {}

  ~DeviceTexture() {
    if (kmt_handle_) {
      // TODO(shared-resource): D3DKMTDestroyAllocation2
    }
  }

  Rc<Buffer> buffer() final { return {}; };
  Rc<Texture> texture() final { return this->underlying_texture_; };
  BufferSlice bufferSlice() final { return {};}
  Rc<StagingResource> staging(UINT) final { return nullptr; }
  Rc<DynamicBuffer> dynamicBuffer(UINT*, UINT*) final { return {}; }
  Rc<DynamicLinearTexture> dynamicLinearTexture(UINT*, UINT*) final { return {}; };
  Rc<DynamicBuffer> dynamicTexture(UINT , UINT *, UINT *) final { return {}; };

  HRESULT STDMETHODCALLTYPE CreateRenderTargetView(const D3D11_RENDER_TARGET_VIEW_DESC1 *pDesc,
                                 ID3D11RenderTargetView1 **ppView) override {
    D3D11_RENDER_TARGET_VIEW_DESC1 finalDesc;
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
    MTL_RENDER_PASS_ATTACHMENT_DESC attachment_desc;
    if (FAILED(InitializeAndNormalizeViewDescriptor(
            this->m_parent, this->desc.MipLevels, arraySize, this->underlying_texture_.ptr(), finalDesc,
            attachment_desc, descriptor
        ))) {
      return E_FAIL;
    }
    if (!ppView) {
      return S_FALSE;
    }
    TextureViewKey key = underlying_texture_->createView(descriptor);
    *ppView = ref(new TextureRTV(key, descriptor.format, &finalDesc, this, this->m_parent, attachment_desc));
    return S_OK;
  };

  HRESULT STDMETHODCALLTYPE CreateDepthStencilView(const D3D11_DEPTH_STENCIL_VIEW_DESC *pDesc,
                                 ID3D11DepthStencilView **ppView) override {
    D3D11_DEPTH_STENCIL_VIEW_DESC finalDesc;
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
    MTL_RENDER_PASS_ATTACHMENT_DESC attachment_desc;
    if (FAILED(InitializeAndNormalizeViewDescriptor(
            this->m_parent, this->desc.MipLevels, arraySize, this->underlying_texture_.ptr(), finalDesc,
            attachment_desc, descriptor
        ))) {
      return E_FAIL;
    }
    if (!ppView) {
      return S_FALSE;
    }
    TextureViewKey key = underlying_texture_->createView(descriptor);
    *ppView = ref(new TextureDSV(key, descriptor.format, &finalDesc, this, this->m_parent, attachment_desc));
    return S_OK;
  };

  HRESULT
  STDMETHODCALLTYPE
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
            this->m_parent, this->desc.MipLevels, arraySize, this->underlying_texture_.ptr(), finalDesc, descriptor
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
  STDMETHODCALLTYPE
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
            this->m_parent, this->desc.MipLevels, arraySize, this->underlying_texture_.ptr(), finalDesc, descriptor
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

  virtual HRESULT
  GetSharedHandle(HANDLE *pSharedHandle) override {
    // TODO(shared-resource): note unlike CreateSharedHandle, this method returns a global handle
    // is it simply kmt_handle_?
    return E_INVALIDARG;
  }

  virtual HRESULT
  CreateSharedHandle(const SECURITY_ATTRIBUTES *Attributes, DWORD Access, const WCHAR *pName, HANDLE *pNTHandle)
      override {
    // TODO(shared-resource): D3DKMTShareObjects
    return E_INVALIDARG;
  }

  void SetMinLOD(float MinLod) override { min_lod = MinLod; }

  float GetMinLOD() override { return min_lod; }
};

template <typename tag>
HRESULT CreateDeviceTextureInternal(MTLD3D11Device *pDevice,
                                    const typename tag::DESC1 *pDesc,
                                    const D3D11_SUBRESOURCE_DATA *pInitialData,
                                    typename tag::COM_IMPL **ppTexture) {
  WMTTextureInfo info;
  typename tag::DESC1 finalDesc;
  if (FAILED(CreateMTLTextureDescriptor(pDevice, pDesc, &finalDesc, &info))) {
    return E_INVALIDARG;
  }
  bool single_subresource = info.mipmap_level_count == 1 && info.array_length == 1 &&
                            !(finalDesc.MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE);
  auto texture = Rc<Texture>(new Texture(info, pDevice->GetMTLDevice()));

  auto shared_flag =
      D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
  if (finalDesc.MiscFlags & shared_flag) {
    return E_NOTIMPL; // FIXME: remove after implemented
    // use a dedicated path for now, because there are other works for private storage mode

    Flags<TextureAllocationFlag> flags;
    // TODO(shared-resource): have to use private storage but pInitialData is not handled
    flags.set(TextureAllocationFlag::GpuPrivate);
    if (finalDesc.Usage == D3D11_USAGE_IMMUTABLE)
      flags.set(TextureAllocationFlag::GpuReadonly);
    flags.set(TextureAllocationFlag::Shared);
    auto allocation = texture->allocate(flags);

    // mach_port_t mach_port = allocation->machPort;
    D3DKMT_HANDLE kmt_handle = 0;
    Com<IMTLDXGIDevice> dxgi_device;
    Com<IMTLDXGIAdapter> dxgi_adapter;
    pDevice->QueryInterface(IID_PPV_ARGS(&dxgi_device));
    dxgi_device->GetParent(IID_PPV_ARGS(&dxgi_adapter));
    // TODO(shared-resource): D3DKMTCreateAllocation2/WMTBootstrapRegister
    // - need to create kmt adapter and device...
    // - D3D11_RESOURCE_MISC_SHARED_NTHANDLE -> Flags.NtSecuritySharing
    // - keyed mutex ?

    texture->rename(std::move(allocation));
    *ppTexture = reinterpret_cast<typename tag::COM_IMPL *>(
        ref(new DeviceTexture<tag>(&finalDesc, std::move(texture), kmt_handle, pDevice))
    );
    return S_OK;
  }

  Flags<TextureAllocationFlag> flags;
  flags.set(TextureAllocationFlag::GpuManaged);
  if (finalDesc.Usage == D3D11_USAGE_IMMUTABLE)
    flags.set(TextureAllocationFlag::GpuReadonly);
  if (pInitialData) {
    auto default_allocation = texture->allocate(flags);
    InitializeTextureData(pDevice, default_allocation->texture(), finalDesc, pInitialData);
    texture->rename(std::move(default_allocation));
    *ppTexture =
        reinterpret_cast<typename tag::COM_IMPL *>(
          ref(new DeviceTexture<tag>(&finalDesc, std::move(texture), pDevice)));
  } else if (single_subresource && (finalDesc.BindFlags & D3D11_BIND_DEPTH_STENCIL)) {
    Rc<RenamableTexturePool> renamable = new RenamableTexturePool(texture.ptr(), 32, flags);
    texture->rename(renamable->getNext(0));
    *ppTexture = reinterpret_cast<typename tag::COM_IMPL *>(
        ref(new DeviceTexture<tag>(&finalDesc, std::move(texture), std::move(renamable), pDevice)));
  } else {
    texture->rename(texture->allocate(flags));
    *ppTexture = reinterpret_cast<typename tag::COM_IMPL *>(
      ref(new DeviceTexture<tag>(&finalDesc, std::move(texture), pDevice)));
  }
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

template <typename tag>
HRESULT
ImportSharedTextureInternal(
    MTLD3D11Device *pDevice, const typename tag::DESC1 *pDescUnchecked, mach_port_t MachPort, REFIID riid,
    void **ppTexture
) {
  WMTTextureInfo info;
  typename tag::DESC1 finalDesc;
  if (FAILED(CreateMTLTextureDescriptor(pDevice, pDescUnchecked, &finalDesc, &info)))
    return E_INVALIDARG;

  auto texture = Rc<Texture>(new Texture(info, pDevice->GetMTLDevice()));
  auto allocation = texture->import(MachPort);
  if (!allocation)
    return E_FAIL;
  texture->rename(std::move(allocation));

  Com<DeviceTexture<tag>> device_texture = (ref(new DeviceTexture<tag>(&finalDesc, std::move(texture), pDevice)));
  return device_texture->QueryInterface(riid, ppTexture);
}

HRESULT
ImportSharedTexture(MTLD3D11Device *pDevice, HANDLE hResource, REFIID riid, void **ppTexture) {
  // TODO(shared-resource): D3DKMTOpenResource2/WMTBootstrapLookUp/ImportSharedTextureInternal
  return E_NOTIMPL;
}

HRESULT
ImportSharedTextureFromNtHandle(MTLD3D11Device *pDevice, HANDLE hResource, REFIID riid, void **ppTexture) {
  // TODO(shared-resource): D3DKMTOpenResourceFromNtHandle/WMTBootstrapLookUp/ImportSharedTextureInternal
  return E_NOTIMPL;
}

HRESULT
ImportSharedTextureByName(
    MTLD3D11Device *pDevice, LPCWSTR lpName, DWORD dwDesiredAccess, REFIID riid, void **ppTexture
) {
  // TODO(shared-resource): D3DKMTOpenNtHandleFromName
  return E_FAIL;
}

#pragma endregion

} // namespace dxmt