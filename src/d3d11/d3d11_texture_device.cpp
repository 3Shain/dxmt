#include "com/com_pointer.hpp"
#include "d3d11_device.hpp"
#include "d3d11_enumerable.hpp"
#include "d3d11_view.hpp"
#include "dxmt_dynamic.hpp"
#include "dxmt_staging.hpp"
#include "dxmt_texture.hpp"
#include "d3d11_resource.hpp"
#include "util_win32_compat.h"

namespace dxmt {

#pragma region DeviceTexture

template <typename tag_texture>
class DeviceTexture : public TResourceBase<tag_texture, IMTLMinLODClampable> {
private:
  Rc<Texture> underlying_texture_;
  Rc<RenamableTexturePool> renamable_;
  float min_lod = 0.0;
  D3DKMT_HANDLE local_kmt_ = 0;
  D3DKMT_HANDLE global_kmt_ = 0;

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
      const tag_texture::DESC1 *pDesc, Rc<Texture> &&u_texture, D3DKMT_HANDLE localHandle, D3DKMT_HANDLE globalHandle,
      MTLD3D11Device *pDevice
  ) :
      TResourceBase<tag_texture, IMTLMinLODClampable>(*pDesc, pDevice),
      underlying_texture_(std::move(u_texture)), local_kmt_(localHandle), global_kmt_(globalHandle) {}

  ~DeviceTexture() {
    if (local_kmt_) {
      D3DKMT_DESTROYALLOCATION destroy = {};
      destroy.hDevice = this->m_parent->GetLocalD3DKMT();
      destroy.hResource = local_kmt_;
      D3DKMTDestroyAllocation(&destroy);
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
    if (pSharedHandle == nullptr || (this->desc.MiscFlags & D3D11_RESOURCE_MISC_SHARED_NTHANDLE)) {
      return E_INVALIDARG;
    }

    if (!(this->desc.MiscFlags & (D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX))) {
      *pSharedHandle = NULL;
      return S_OK;
    }

    if (!global_kmt_) {
      return E_INVALIDARG;
    }

    *pSharedHandle = reinterpret_cast<HANDLE>(global_kmt_);
    return S_OK;
  }

  virtual HRESULT
  CreateSharedHandle(const SECURITY_ATTRIBUTES *Attributes, DWORD Access, const WCHAR *pName, HANDLE *pNTHandle)
      override {
    InitReturnPtr(pNTHandle);
    if (!local_kmt_)
      return E_INVALIDARG;

    OBJECT_ATTRIBUTES attr = {};
    attr.Length = sizeof(attr);
    attr.SecurityDescriptor = const_cast<SECURITY_ATTRIBUTES*>(Attributes);

    WCHAR buffer[MAX_PATH];
    UNICODE_STRING name_str;
    if (pName) {
      DWORD session, len, name_len = wcslen(pName);

      ProcessIdToSessionId(GetCurrentProcessId(), &session);
      len = swprintf(buffer, ARRAYSIZE(buffer), L"\\Sessions\\%u\\BaseNamedObjects\\", session);
      memcpy(buffer + len, pName, (name_len + 1) * sizeof(WCHAR));
      name_str.MaximumLength = name_str.Length = (len + name_len) * sizeof(WCHAR);
      name_str.MaximumLength += sizeof(WCHAR);
      name_str.Buffer = buffer;

      attr.ObjectName = &name_str;
      attr.Attributes = OBJ_CASE_INSENSITIVE;
    }

    if (D3DKMTShareObjects(1, &local_kmt_, &attr, Access, pNTHandle)) {
      ERR("DeviceTexture: Failed to create shared handle");
      return E_FAIL;
    }

    return S_OK;
  }

  void SetMinLOD(float MinLod) override { min_lod = MinLod; }

  float GetMinLOD() override { return min_lod; }
};

struct SharedResourceData {
  char mach_port_name[54];
  D3D11_RESOURCE_DIMENSION dimension;
  union {
    D3D11_TEXTURE1D_DESC desc1d;
    D3D11_TEXTURE2D_DESC1 desc2d;
    D3D11_TEXTURE3D_DESC1 desc3d;
  } desc;
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

  auto &initializer = pDevice->GetDXMTDevice().queue().initializer;

  auto initialize = [&](Rc<TextureAllocation> &&allocation) {
    texture->rename(std::move(allocation));
    if (!pInitialData) {
      for (auto sub : EnumerateSubresources(finalDesc)) {
        initializer.initWithZero(texture.ptr(), texture->current(), sub.ArraySlice, sub.MipLevel);
      }
    } else {
      for (auto sub : EnumerateSubresources(finalDesc)) {
        auto &data = pInitialData[sub.SubresourceId];
        initializer.initWithData(
            texture.ptr(), texture->current(), sub.ArraySlice, sub.MipLevel, data.pSysMem, data.SysMemPitch,
            data.SysMemSlicePitch
        );
      }
    }
  };

  auto shared_flag =
      D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
  if (finalDesc.MiscFlags & shared_flag) {
    if (!(pDevice->GetLocalD3DKMT() & 0xc0000000)) {
      ERR("DeviceTexture: Invalid device handle");
      return E_FAIL;
    }
    // use a dedicated path for now, because there are other works for private storage mode

    Flags<TextureAllocationFlag> flags;
    flags.set(TextureAllocationFlag::GpuPrivate);
    if (finalDesc.Usage == D3D11_USAGE_IMMUTABLE)
      flags.set(TextureAllocationFlag::GpuReadonly);
    flags.set(TextureAllocationFlag::Shared);
    auto allocation = texture->allocate(flags);

    mach_port_t mach_port = allocation->machPort;
    if (!mach_port) {
      ERR("DeviceTexture: Failed to get mach port for shared texture");
      return E_FAIL;
    }
    SharedResourceData runtimeData;
    MakeUniqueSharedName(runtimeData.mach_port_name);
    if (!WMTBootstrapRegister(runtimeData.mach_port_name, mach_port)) {
      ERR("DeviceTexture: Failed to register mach port for shared texture");
      return E_FAIL;
    }
    runtimeData.dimension = tag::dimension;
    memcpy(&runtimeData.desc, pDesc, sizeof(typename tag::DESC1));

    D3DKMT_CREATEALLOCATION create = {};
    create.hDevice = pDevice->GetLocalD3DKMT();
    create.pPrivateRuntimeData = &runtimeData;
    create.PrivateRuntimeDataSize = sizeof(runtimeData);
    create.Flags.StandardAllocation = 1;
    create.NumAllocations = 1;
    D3DDDI_ALLOCATIONINFO2 allocationInfo = {};
    create.pAllocationInfo2 = &allocationInfo;
    D3DKMT_CREATESTANDARDALLOCATION standardAllocation = {};
    create.pStandardAllocation = &standardAllocation;
    standardAllocation.Type = D3DKMT_STANDARDALLOCATIONTYPE_EXISTINGHEAP;
    create.Flags.ExistingSysMem = 1;
    D3DDDI_ALLOCATIONINFO systemMem;
    allocationInfo.pSystemMem = &systemMem;
    create.Flags.CreateResource = 1;
    create.Flags.CreateShared = 1;
    create.Flags.NtSecuritySharing = !!(finalDesc.MiscFlags & D3D11_RESOURCE_MISC_SHARED_NTHANDLE);
    if (D3DKMTCreateAllocation2(&create)) {
      ERR("DeviceTexture: Failed to create D3DKMT for shared texture");
      return E_FAIL;
    }

    // TODO: handle keyed mutex

    initialize(std::move(allocation));
    *ppTexture = reinterpret_cast<typename tag::COM_IMPL *>(
        ref(new DeviceTexture<tag>(&finalDesc, std::move(texture), create.hResource,
                                   create.hGlobalShare, pDevice))
    );
    return S_OK;
  }

  Flags<TextureAllocationFlag> flags;
  flags.set(finalDesc.CPUAccessFlags ? TextureAllocationFlag::GpuManaged : TextureAllocationFlag::GpuPrivate);
  if (finalDesc.Usage == D3D11_USAGE_IMMUTABLE)
    flags.set(TextureAllocationFlag::GpuReadonly);
  if (single_subresource && (finalDesc.BindFlags & D3D11_BIND_DEPTH_STENCIL)) {
    Rc<RenamableTexturePool> renamable = new RenamableTexturePool(texture.ptr(), 32, flags);
    initialize(renamable->getNext(0));
    *ppTexture = reinterpret_cast<typename tag::COM_IMPL *>(
        ref(new DeviceTexture<tag>(&finalDesc, std::move(texture), std::move(renamable), pDevice)));
  } else {
    initialize(texture->allocate(flags));
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
  InitReturnPtr(ppTexture);

  if (!(reinterpret_cast<uintptr_t>(hResource) & 0xc0000000)) {
    WARN("ImportSharedTexture: Invalid shared handle type");
    return E_INVALIDARG;
  }

  if (ppTexture == nullptr)
    return S_FALSE;

  struct SharedResourceData runtimeData;

  D3DKMT_QUERYRESOURCEINFO query = {};
  query.hDevice = pDevice->GetLocalD3DKMT();
  query.hGlobalShare = reinterpret_cast<uintptr_t>(hResource);
  query.pPrivateRuntimeData = &runtimeData;
  query.PrivateRuntimeDataSize = sizeof(runtimeData);

  if (D3DKMTQueryResourceInfo(&query)) {
    WARN("ImportSharedTexture: Failed to query resource: ", hResource);
    return E_INVALIDARG;
  }

  if (query.PrivateRuntimeDataSize != sizeof(runtimeData)) {
    WARN("ImportSharedTexture: Unexpected size: ", query.PrivateRuntimeDataSize);
    return E_INVALIDARG;
  } 

  D3DDDI_OPENALLOCATIONINFO2 alloc = {};
  D3DKMT_OPENRESOURCE open = {};
  open.hDevice = pDevice->GetLocalD3DKMT();
  open.hGlobalShare = reinterpret_cast<uintptr_t>(hResource);
  open.NumAllocations = 1;
  open.pOpenAllocationInfo2 = &alloc;
  open.pPrivateRuntimeData = &runtimeData;
  open.PrivateRuntimeDataSize = query.PrivateRuntimeDataSize;

  if (D3DKMTOpenResource2(&open)) {
    WARN("ImportSharedTexture: Failed to open resource: ", hResource);
    return E_INVALIDARG;
  }

  D3DKMT_DESTROYALLOCATION destroy = {};
  destroy.hDevice = pDevice->GetLocalD3DKMT();
  destroy.hResource = open.hResource;
  D3DKMTDestroyAllocation(&destroy);

  mach_port_t mach_port;
  if (!WMTBootstrapLookUp(runtimeData.mach_port_name, &mach_port)) {
    ERR("ImportSharedTexture: Failed to look up mach port");
    return E_INVALIDARG;
  }

  switch (runtimeData.dimension)
  {
  case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
    return ImportSharedTextureInternal<tag_texture_1d>(
        pDevice, &runtimeData.desc.desc1d, mach_port, riid, ppTexture
    );
  case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
    return ImportSharedTextureInternal<tag_texture_2d>(
        pDevice, &runtimeData.desc.desc2d, mach_port, riid, ppTexture
    );
  case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
    return ImportSharedTextureInternal<tag_texture_3d>(
        pDevice, &runtimeData.desc.desc3d, mach_port, riid, ppTexture
    );
  default:
    ERR("ImportSharedTexture: Unsupported resource dimension");
    return E_INVALIDARG;
  }
}

HRESULT
ImportSharedTextureFromNtHandle(MTLD3D11Device *pDevice, HANDLE hResource, REFIID riid, void **ppTexture) {
  InitReturnPtr(ppTexture);

  if (reinterpret_cast<uintptr_t>(hResource) & 0xc0000000) {
    WARN("ImportSharedTextureFromNtHandle: Invalid shared handle type");
    return E_INVALIDARG;
  }

  if (ppTexture == nullptr)
    return S_FALSE;

  struct SharedResourceData runtimeData;

  D3DKMT_QUERYRESOURCEINFOFROMNTHANDLE query = {};
  query.hDevice = pDevice->GetLocalD3DKMT();
  query.hNtHandle = hResource;
  query.pPrivateRuntimeData = &runtimeData;
  query.PrivateRuntimeDataSize = sizeof(runtimeData);

  if (D3DKMTQueryResourceInfoFromNtHandle(&query)) {
    WARN(str::format("ImportSharedTextureFromNtHandle: Failed to query resource: ", hResource));
    return E_INVALIDARG;
  }
  
  if (query.PrivateRuntimeDataSize != sizeof(runtimeData)) {
    WARN(str::format("ImportSharedTextureFromNtHandle: Unexpected size: ", query.PrivateRuntimeDataSize));
    return E_INVALIDARG;
  }

  D3DDDI_OPENALLOCATIONINFO2 alloc = {};
  D3DKMT_OPENRESOURCEFROMNTHANDLE open = {};
  char dummy;

  open.hDevice = pDevice->GetLocalD3DKMT();
  open.hNtHandle = hResource;
  open.NumAllocations = 1;
  open.pOpenAllocationInfo2 = &alloc;
  open.pPrivateRuntimeData = &runtimeData;
  open.PrivateRuntimeDataSize = query.PrivateRuntimeDataSize;
  open.pTotalPrivateDriverDataBuffer = &dummy;
  open.TotalPrivateDriverDataBufferSize = 0;

  if (D3DKMTOpenResourceFromNtHandle(&open)) {
    WARN(str::format("ImportSharedTextureFromNtHandle: Failed to open resource: ", hResource));
    return E_INVALIDARG;
  }

  D3DKMT_DESTROYALLOCATION destroy = {};
  destroy.hDevice = pDevice->GetLocalD3DKMT();
  destroy.hResource = open.hResource;
  D3DKMTDestroyAllocation(&destroy);

  if (open.hSyncObject) {
    WARN(str::format("ImportSharedTextureFromNtHandle: Ignoring bundled sync object"));
    D3DKMT_DESTROYSYNCHRONIZATIONOBJECT destroySync = {};
    destroySync.hSyncObject = open.hSyncObject;
    D3DKMTDestroySynchronizationObject(&destroySync);
  }
  if (open.hKeyedMutex) {
    WARN(str::format("ImportSharedTextureFromNtHandle: Ignoring bundled keyed mutex"));
    D3DKMT_DESTROYKEYEDMUTEX destroyMutex = {};
    destroyMutex.hKeyedMutex = open.hKeyedMutex;
    D3DKMTDestroyKeyedMutex(&destroyMutex);
  }

  mach_port_t mach_port;
  if (!WMTBootstrapLookUp(runtimeData.mach_port_name, &mach_port)) {
    ERR("ImportSharedTexture: Failed to look up mach port");
    return E_INVALIDARG;
  }

  switch (runtimeData.dimension)
  {
  case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
    return ImportSharedTextureInternal<tag_texture_1d>(
        pDevice, &runtimeData.desc.desc1d, mach_port, riid, ppTexture
    );
  case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
    return ImportSharedTextureInternal<tag_texture_2d>(
        pDevice, &runtimeData.desc.desc2d, mach_port, riid, ppTexture
    );
  case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
    return ImportSharedTextureInternal<tag_texture_3d>(
        pDevice, &runtimeData.desc.desc3d, mach_port, riid, ppTexture
    );
  default:
    ERR("ImportSharedTexture: Unsupported resource dimension");
    return E_INVALIDARG;
  }
}

HRESULT
ImportSharedTextureByName(
    MTLD3D11Device *pDevice, LPCWSTR lpName, DWORD dwDesiredAccess, REFIID riid, void **ppTexture
) {
  D3DKMT_OPENNTHANDLEFROMNAME openFromName = {};
  openFromName.dwDesiredAccess = dwDesiredAccess;

  OBJECT_ATTRIBUTES attr = {};
  attr.Length = sizeof(attr);

  WCHAR buffer[MAX_PATH];
  UNICODE_STRING name_str;
  DWORD session, len, name_len = wcslen(lpName);

  ProcessIdToSessionId(GetCurrentProcessId(), &session);
  len = swprintf(buffer, ARRAYSIZE(buffer), L"\\Sessions\\%u\\BaseNamedObjects\\", session);
  memcpy(buffer + len, lpName, (name_len + 1) * sizeof(WCHAR));
  name_str.MaximumLength = name_str.Length = (len + name_len) * sizeof(WCHAR);
  name_str.MaximumLength += sizeof(WCHAR);
  name_str.Buffer = buffer;

  attr.ObjectName = &name_str;
  attr.Attributes = OBJ_CASE_INSENSITIVE;
  openFromName.pObjAttrib = &attr;

  if (D3DKMTOpenNtHandleFromName(&openFromName)) {
    WARN(str::format("ImportSharedTextureByName: Failed to open NT handle from name: ", lpName));
    return E_INVALIDARG;
  }

  HRESULT res = ImportSharedTextureFromNtHandle(pDevice, openFromName.hNtHandle, riid, ppTexture);
  CloseHandle(openFromName.hNtHandle);
  return res;
}

#pragma endregion

} // namespace dxmt