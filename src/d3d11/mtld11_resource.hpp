#pragma once
#include "dxmt_buffer.hpp"
#include "dxmt_texture.hpp"
#include "Metal/MTLBuffer.hpp"
#include "Metal/MTLTexture.hpp"
#include "d3d11_device_child.hpp"
#include "com/com_pointer.hpp"
#include "com/com_guid.hpp"
#include "d3d11_view.hpp"
#include "dxgi_resource.hpp"
#include "dxmt_resource_binding.hpp"
#include "log/log.hpp"
#include <memory>
#include <type_traits>

typedef struct MappedResource {
  void *pData;
  uint32_t RowPitch;
  uint32_t DepthPitch;
} MappedResource;

struct MTL_STAGING_RESOURCE {
  MTL::Buffer *Buffer;
};

DEFINE_COM_INTERFACE("65feb8c5-01de-49df-bf58-d115007a117d", IMTLDynamicBuffer)
    : public IUnknown {
  virtual void *GetMappedMemory(UINT * pBytesPerRow, UINT * pBytesPerImage) = 0;
  virtual UINT GetSize(UINT * pBytesPerRow, UINT * pBytesPerImage) = 0;
  virtual void RotateBuffer(dxmt::MTLD3D11Device * pool) = 0;
  virtual D3D11_BIND_FLAG GetBindFlag() = 0;

  virtual dxmt::Rc<dxmt::Buffer> __buffer_dyn() = 0;
  virtual dxmt::Rc<dxmt::Texture> __texture_dyn() = 0;
  virtual bool __isBuffer_dyn() = 0;
  virtual dxmt::Rc<dxmt::BufferAllocation> __bufferAllocated() = 0;
  virtual dxmt::Rc<dxmt::TextureAllocation> __textureAllocated() = 0;
};

DEFINE_COM_INTERFACE("252c1a0e-1c61-42e7-9b57-23dfe3d73d49", IMTLD3D11Staging)
    : public IUnknown {

  virtual void UseCopyDestination(
      uint32_t Subresource, uint64_t seq_id, MTL_STAGING_RESOURCE * pBuffer,
      uint32_t * pBytesPerRow, uint32_t * pBytesPerImage) = 0;
  virtual void UseCopySource(
      uint32_t Subresource, uint64_t seq_id, MTL_STAGING_RESOURCE * pBuffer,
      uint32_t * pBytesPerRow, uint32_t * pBytesPerImage) = 0;
  /**
  cpu_coherent_seq_id: any operation at/before seq_id is coherent to cpu
  -
  return:
  = 0 - map success
  > 0 - resource in use
  < 0 - error (?)
  */
  virtual int64_t TryMap(uint32_t Subresource, uint64_t cpu_coherent_seq_id,
                         // uint64_t commited_seq_id,
                         D3D11_MAP flag,
                         D3D11_MAPPED_SUBRESOURCE * pMappedResource) = 0;
  virtual void Unmap(uint32_t Subresource) = 0;
};

DEFINE_COM_INTERFACE("9a6f6549-d4b1-45ea-8794-8503d190d3d1",
                     IMTLMinLODClampable)
    : public IUnknown {
  virtual void SetMinLOD(float MinLOD) = 0;
  virtual float GetMinLOD() = 0;
};

namespace dxmt {

template <typename RESOURCE_DESC>
void UpgradeResourceDescription(const RESOURCE_DESC *pSrc, RESOURCE_DESC &dst) {
  dst = *pSrc;
}
template <typename RESOURCE_DESC>
void DowngradeResourceDescription(const RESOURCE_DESC &src,
                                  RESOURCE_DESC *pDst) {
  *pDst = src;
}

template <typename RESOURCE_DESC_SRC, typename RESOURCE_DESC_DST>
void UpgradeResourceDescription(const RESOURCE_DESC_SRC *pSrc,
                                RESOURCE_DESC_DST &dst);
template <typename RESOURCE_DESC_SRC, typename RESOURCE_DESC_DST>
void DowngradeResourceDescription(const RESOURCE_DESC_SRC &src,
                                  RESOURCE_DESC_DST *pDst);

template <typename VIEW_DESC>
void UpgradeViewDescription(const VIEW_DESC *pSrc, VIEW_DESC &dst) {
  dst = *pSrc;
}
template <typename VIEW_DESC>
void DowngradeViewDescription(const VIEW_DESC &src, VIEW_DESC *pDst) {
  *pDst = src;
}

template <typename VIEW_DESC_SRC, typename VIEW_DESC_DST>
void UpgradeViewDescription(const VIEW_DESC_SRC *pSrc, VIEW_DESC_DST &dst);
template <typename VIEW_DESC_SRC, typename VIEW_DESC_DST>
void DowngradeViewDescription(const VIEW_DESC_SRC &src, VIEW_DESC_DST *pDst);

struct tag_buffer {
  static const D3D11_RESOURCE_DIMENSION dimension =
      D3D11_RESOURCE_DIMENSION_BUFFER;
  using COM = ID3D11Buffer;
  using COM_IMPL = ID3D11Buffer;
  using DESC = D3D11_BUFFER_DESC;
  using DESC1 = D3D11_BUFFER_DESC;
  static constexpr std::string_view debug_name = "buffer";
};

struct tag_texture_1d {
  static const D3D11_RESOURCE_DIMENSION dimension =
      D3D11_RESOURCE_DIMENSION_TEXTURE1D;
  using COM = ID3D11Texture1D;
  using COM_IMPL = ID3D11Texture1D;
  using DESC = D3D11_TEXTURE1D_DESC;
  using DESC1 = D3D11_TEXTURE1D_DESC;
  static constexpr std::string_view debug_name = "tex1d";
};

struct tag_texture_2d {
  static const D3D11_RESOURCE_DIMENSION dimension =
      D3D11_RESOURCE_DIMENSION_TEXTURE2D;
  using COM = ID3D11Texture2D;
  using COM_IMPL = ID3D11Texture2D1;
  using DESC = D3D11_TEXTURE2D_DESC;
  using DESC1 = D3D11_TEXTURE2D_DESC1;
  static constexpr std::string_view debug_name = "tex2d";
};

struct tag_texture_3d {
  static const D3D11_RESOURCE_DIMENSION dimension =
      D3D11_RESOURCE_DIMENSION_TEXTURE3D;
  using COM = ID3D11Texture3D;
  using COM_IMPL = ID3D11Texture3D1;
  using DESC = D3D11_TEXTURE3D_DESC;
  using DESC1 = D3D11_TEXTURE3D_DESC1;
  static constexpr std::string_view debug_name = "tex3d";
};

struct D3D11ResourceCommon : ID3D11Resource {
    /* ID3D11Buffer::GetDesc */
    /* ID3D11Texture1D::GetDesc */
    /* ID3D11Texture2D::GetDesc */
    /* ID3D11Texture3D::GetDesc */
  virtual void STDMETHODCALLTYPE GetDesc(void *pDesc) = 0;
    /* ID3D11Texture2D1::GetDesc */
    /* ID3D11Texture3D1::GetDesc */
  virtual void STDMETHODCALLTYPE GetDesc1(void *pDesc) = 0;

  virtual HRESULT STDMETHODCALLTYPE
  CreateShaderResourceView(const D3D11_SHADER_RESOURCE_VIEW_DESC1 *pDesc, ID3D11ShaderResourceView1 **ppView) = 0;
  virtual HRESULT STDMETHODCALLTYPE
  CreateUnorderedAccessView(const D3D11_UNORDERED_ACCESS_VIEW_DESC1 *pDesc, ID3D11UnorderedAccessView1 **ppView) = 0;
  virtual HRESULT STDMETHODCALLTYPE
  CreateRenderTargetView(const D3D11_RENDER_TARGET_VIEW_DESC1 *pDesc, ID3D11RenderTargetView1 **ppView) = 0;
  virtual HRESULT STDMETHODCALLTYPE
  CreateDepthStencilView(const D3D11_DEPTH_STENCIL_VIEW_DESC *pDesc, ID3D11DepthStencilView **ppView) = 0;

  virtual Rc<Buffer> buffer() = 0;
  virtual BufferSlice bufferSlice() = 0;
  virtual Rc<Texture> texture() = 0;
  virtual Com<IMTLD3D11Staging> staging() = 0;
  virtual Com<IMTLDynamicBuffer> dynamic() = 0;
};

inline Com<IMTLDynamicBuffer>
GetDynamic(ID3D11Resource *pResource) {
  return static_cast<D3D11ResourceCommon *>(pResource)->dynamic();
}
inline Com<IMTLD3D11Staging>
GetStaging(ID3D11Resource *pResource) {
  return static_cast<D3D11ResourceCommon *>(pResource)->staging();
}
inline Rc<Texture>
GetTexture(ID3D11Resource *pResource) {
  return static_cast<D3D11ResourceCommon *>(pResource)->texture();
}

template <typename tag, typename... Base>
class TResourceBase : public MTLD3D11DeviceChild<D3D11ResourceCommon, Base...> {
public:
  TResourceBase(const tag::DESC1 &desc, MTLD3D11Device *device)
      : MTLD3D11DeviceChild<D3D11ResourceCommon, Base...>(
            device),
        desc(desc),
        dxgi_resource(new MTLDXGIResource<TResourceBase<tag, Base...>>(this)) {}

  template <std::size_t n> HRESULT ResolveBase(REFIID riid, void **ppvObject) {
    return E_NOINTERFACE;
  };

  template <std::size_t n, typename V, typename... Args>
  HRESULT ResolveBase(REFIID riid, void **ppvObject) {
    if (riid == __uuidof(V)) {
      *ppvObject = ref_and_cast<V>(this);
      return S_OK;
    }
    return ResolveBase<n + 1, Args...>(riid, ppvObject);
  };

  HRESULT QueryInterface(REFIID riid, void **ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    auto hr = ResolveBase<0, Base...>(riid, ppvObject);
    if (SUCCEEDED(hr)) {
      return S_OK;
    }

    if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D11DeviceChild) ||
        riid == __uuidof(ID3D11Resource) ||
        riid == __uuidof(typename tag::COM) ||
        riid == __uuidof(typename tag::COM_IMPL)) {
      *ppvObject = ref_and_cast<D3D11ResourceCommon>(this);
      return S_OK;
    }

    if (riid == __uuidof(IDXGIObject) ||
        riid == __uuidof(IDXGIDeviceSubObject) ||
        riid == __uuidof(IDXGIResource) || riid == __uuidof(IDXGIResource1)) {
      *ppvObject = ref(dxgi_resource.get());
      return S_OK;
    }

    if (riid == __uuidof(IMTLDynamicBuffer) || riid == __uuidof(IMTLD3D11Staging)) {
      // silent these interfaces
      return E_NOINTERFACE;
    }

    if (logQueryInterfaceError(__uuidof(typename tag::COM), riid)) {
      WARN("D3D11Resource(", tag::debug_name ,"): Unknown interface query ", str::format(riid));
    }

    return E_NOINTERFACE;
  }

  void GetDesc(void *pDesc) final {
    ::dxmt::DowngradeResourceDescription(desc, (typename tag::DESC *)pDesc);
  }

  void GetDesc1(void *pDesc) /* override / final */ { *(typename tag::DESC1 *)pDesc = desc; }

  void GetType(D3D11_RESOURCE_DIMENSION *pResourceDimension) final {
    *pResourceDimension = tag::dimension;
  }

  void SetEvictionPriority(UINT EvictionPriority) final {}

  UINT GetEvictionPriority() final { return DXGI_RESOURCE_PRIORITY_NORMAL; }

  virtual HRESULT GetDeviceInterface(REFIID riid, void **ppDevice) {
    Com<ID3D11Device> device;
    this->GetDevice(&device);
    return device->QueryInterface(riid, ppDevice);
  };

  virtual HRESULT GetDXGIUsage(DXGI_USAGE *pUsage) {
    if (!pUsage) {
      return E_INVALIDARG;
    }
    *pUsage = 0;
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE
  CreateShaderResourceView(const D3D11_SHADER_RESOURCE_VIEW_DESC1 *pDesc,
                           ID3D11ShaderResourceView1 **ppView) {
    return E_INVALIDARG;
  };
  virtual HRESULT STDMETHODCALLTYPE
  CreateUnorderedAccessView(const D3D11_UNORDERED_ACCESS_VIEW_DESC1 *pDesc,
                            ID3D11UnorderedAccessView1 **ppView) {
    return E_INVALIDARG;
  };
  virtual HRESULT STDMETHODCALLTYPE
  CreateRenderTargetView(const D3D11_RENDER_TARGET_VIEW_DESC1 *pDesc,
                         ID3D11RenderTargetView1 **ppView) {
    return E_INVALIDARG;
  };
  virtual HRESULT STDMETHODCALLTYPE
  CreateDepthStencilView(const D3D11_DEPTH_STENCIL_VIEW_DESC *pDesc,
                         ID3D11DepthStencilView **ppView) {
    return E_INVALIDARG;
  };

protected:
  tag::DESC1 desc;
  std::unique_ptr<IDXGIResource1> dxgi_resource;
};

template <typename RESOURCE_IMPL_ = ID3D11Resource,
          typename COM_IMPL_ = IMTLD3D11RenderTargetView>
struct tag_render_target_view {
  using COM = ID3D11RenderTargetView;
  using COM1 = ID3D11RenderTargetView1;
  using COM_IMPL = COM_IMPL_;
  using RESOURCE = ID3D11Resource;
  using RESOURCE_IMPL = RESOURCE_IMPL_;
  using DESC = D3D11_RENDER_TARGET_VIEW_DESC;
  using DESC1 = D3D11_RENDER_TARGET_VIEW_DESC1;
};

template <typename RESOURCE_IMPL_ = ID3D11Resource,
          typename COM_IMPL_ = IMTLD3D11DepthStencilView>
struct tag_depth_stencil_view {
  using COM = ID3D11DepthStencilView;
  using COM1 = ID3D11DepthStencilView;
  using COM_IMPL = COM_IMPL_;
  using RESOURCE = ID3D11Resource;
  using RESOURCE_IMPL = RESOURCE_IMPL_;
  using DESC = D3D11_DEPTH_STENCIL_VIEW_DESC;
  using DESC1 = D3D11_DEPTH_STENCIL_VIEW_DESC;
};

template <typename RESOURCE_IMPL_ = ID3D11Resource,
          typename COM_IMPL_ = D3D11ShaderResourceView>
struct tag_shader_resource_view {
  using COM = ID3D11ShaderResourceView;
  using COM1 = ID3D11ShaderResourceView1;
  using COM_IMPL = COM_IMPL_;
  using RESOURCE = ID3D11Resource;
  using RESOURCE_IMPL = RESOURCE_IMPL_;
  using DESC = D3D11_SHADER_RESOURCE_VIEW_DESC;
  using DESC1 = D3D11_SHADER_RESOURCE_VIEW_DESC1;
};

template <typename RESOURCE_IMPL_ = ID3D11Resource,
          typename COM_IMPL_ = D3D11UnorderedAccessView>
struct tag_unordered_access_view {
  using COM = ID3D11UnorderedAccessView;
  using COM1 = ID3D11UnorderedAccessView1;
  using COM_IMPL = COM_IMPL_;
  using RESOURCE = ID3D11Resource;
  using RESOURCE_IMPL = RESOURCE_IMPL_;
  using DESC = D3D11_UNORDERED_ACCESS_VIEW_DESC;
  using DESC1 = D3D11_UNORDERED_ACCESS_VIEW_DESC1;
};

template <typename tag, typename... Base>
class TResourceViewBase
    : public MTLD3D11DeviceChild<typename tag::COM_IMPL, Base...> {
public:
  TResourceViewBase(const tag::DESC1 *pDesc, tag::RESOURCE_IMPL *pResource,
                    MTLD3D11Device *device)
      : MTLD3D11DeviceChild<typename tag::COM_IMPL, Base...>(device),
        resource(pResource) {
    if (pDesc) {
      desc = *pDesc;
    }
  }

  template <std::size_t n> HRESULT ResolveBase(REFIID riid, void **ppvObject) {
    return E_NOINTERFACE;
  };

  template <std::size_t n, typename V, typename... Args>
  HRESULT ResolveBase(REFIID riid, void **ppvObject) {
    if (riid == __uuidof(V)) {
      *ppvObject = ref_and_cast<V>(this);
      return S_OK;
    }
    return ResolveBase<n + 1, Args...>(riid, ppvObject);
  };

  HRESULT QueryInterface(REFIID riid, void **ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    auto hr = ResolveBase<0, Base...>(riid, ppvObject);
    if (SUCCEEDED(hr)) {
      return S_OK;
    }

    if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D11DeviceChild) ||
        riid == __uuidof(ID3D11View) || riid == __uuidof(typename tag::COM) ||
        riid == __uuidof(typename tag::COM1)) {
      *ppvObject = ref_and_cast<typename tag::COM_IMPL>(this);
      return S_OK;
    }

    if (riid == __uuidof(IMTLDynamicBuffer)) {
      // silent these interfaces
      return E_NOINTERFACE;
    }

    if (logQueryInterfaceError(__uuidof(typename tag::COM), riid)) {
      WARN("D3D11View: Unknown interface query ", str::format(riid));
    }

    return E_NOINTERFACE;
  }

  void GetDesc(tag::DESC *pDesc) final {
    DowngradeViewDescription(desc, pDesc);
  }

  void GetDesc1(tag::DESC1 *pDesc) /* override / final */ { *pDesc = desc; }

  void GetResource(tag::RESOURCE **ppResource) final {
    resource->QueryInterface(IID_PPV_ARGS(ppResource));
  }

  virtual ULONG64 GetUnderlyingResourceId() { return (ULONG64)resource.ptr(); };

  virtual dxmt::ResourceSubset GetViewRange() { return ResourceSubset(desc); };

protected:
  tag::DESC1 desc;
  /**
  strong ref to resource
  */
  Com<typename tag::RESOURCE_IMPL> resource;
};

#pragma region Resource Factory

HRESULT
CreateStagingBuffer(MTLD3D11Device *pDevice, const D3D11_BUFFER_DESC *pDesc,
                    const D3D11_SUBRESOURCE_DATA *pInitialData,
                    ID3D11Buffer **ppBuffer);

HRESULT
CreateStagingTexture1D(MTLD3D11Device *pDevice,
                       const D3D11_TEXTURE1D_DESC *pDesc,
                       const D3D11_SUBRESOURCE_DATA *pInitialData,
                       ID3D11Texture1D **ppTexture);

HRESULT
CreateStagingTexture2D(MTLD3D11Device *pDevice,
                       const D3D11_TEXTURE2D_DESC1 *pDesc,
                       const D3D11_SUBRESOURCE_DATA *pInitialData,
                       ID3D11Texture2D1 **ppTexture);

HRESULT
CreateStagingTexture3D(MTLD3D11Device *pDevice,
                       const D3D11_TEXTURE3D_DESC1 *pDesc,
                       const D3D11_SUBRESOURCE_DATA *pInitialData,
                       ID3D11Texture3D1 **ppTexture);

HRESULT CreateDeviceTexture1D(MTLD3D11Device *pDevice,
                              const D3D11_TEXTURE1D_DESC *pDesc,
                              const D3D11_SUBRESOURCE_DATA *pInitialData,
                              ID3D11Texture1D **ppTexture);

HRESULT CreateDeviceTexture2D(MTLD3D11Device *pDevice,
                              const D3D11_TEXTURE2D_DESC1 *pDesc,
                              const D3D11_SUBRESOURCE_DATA *pInitialData,
                              ID3D11Texture2D1 **ppTexture);

HRESULT CreateDeviceTexture3D(MTLD3D11Device *pDevice,
                              const D3D11_TEXTURE3D_DESC1 *pDesc,
                              const D3D11_SUBRESOURCE_DATA *pInitialData,
                              ID3D11Texture3D1 **ppTexture);

HRESULT
CreateBuffer(MTLD3D11Device *pDevice, const D3D11_BUFFER_DESC *pDesc,
                    const D3D11_SUBRESOURCE_DATA *pInitialData,
                    ID3D11Buffer **ppBuffer);

HRESULT CreateDynamicTexture2D(MTLD3D11Device *pDevice,
                               const D3D11_TEXTURE2D_DESC1 *pDesc,
                               const D3D11_SUBRESOURCE_DATA *pInitialData,
                               ID3D11Texture2D1 **ppTexture);
#pragma endregion

#pragma region Helper
template <typename RESOURCE_DESC, typename VIEW_DESC>
HRESULT ExtractEntireResourceViewDescription(const RESOURCE_DESC *pResourceDesc,
                                             VIEW_DESC *pViewDescOut);

template <typename RESOURCE_DESC, typename VIEW_DESC>
HRESULT ExtractEntireResourceViewDescription(const RESOURCE_DESC *pResourceDesc,
                                             const VIEW_DESC *pViewDescIn,
                                             VIEW_DESC *pViewDescOut) {
  if (pViewDescIn) {
    *pViewDescOut = *pViewDescIn;
    if constexpr (!std::is_same_v<RESOURCE_DESC, D3D11_BUFFER_DESC>) {
      if (pViewDescOut->Format == DXGI_FORMAT_UNKNOWN) {
        pViewDescOut->Format = pResourceDesc->Format;
      }
    }
    return S_OK;
  } else {
    return ExtractEntireResourceViewDescription(pResourceDesc, pViewDescOut);
  }
}

template <typename TEXTURE_DESC>
HRESULT CreateMTLTextureDescriptor(MTLD3D11Device *pDevice,
                                   const TEXTURE_DESC *pDesc,
                                   TEXTURE_DESC *pOutDesc,
                                   MTL::TextureDescriptor **pMtlDescOut);

template <typename VIEW_DESC>
HRESULT InitializeAndNormalizeViewDescriptor(
    MTLD3D11Device *pDevice, unsigned MiplevelCount, unsigned ArraySize, Texture *pTexture, VIEW_DESC &ViewDesc,
    TextureViewDescriptor &Descriptor
);

template <typename VIEW_DESC>
HRESULT InitializeAndNormalizeViewDescriptor(
    MTLD3D11Device *pDevice, unsigned MiplevelCount, unsigned ArraySize, Texture* pTexture,
    VIEW_DESC &ViewDesc, MTL_RENDER_PASS_ATTACHMENT_DESC &AttachmentDesc, TextureViewDescriptor &Descriptor
);

template <typename TEXTURE_DESC>
void GetMipmapSize(const TEXTURE_DESC *pDesc, uint32_t level, uint32_t *pWidth,
                   uint32_t *pHeight, uint32_t *pDepth);

template <typename TEXTURE_DESC>
HRESULT GetLinearTextureLayout(MTLD3D11Device *pDevice,
                               const TEXTURE_DESC *pDesc, uint32_t level,
                               uint32_t *pBytesPerRow, uint32_t *pBytesPerImage,
                               uint32_t *pBytesPerSlice);

constexpr void
CalculateBufferViewOffsetAndSize(const D3D11_BUFFER_DESC &buffer_desc,
                                 uint32_t element_stride,
                                 uint32_t first_element, uint32_t num_elements,
                                 uint32_t &offset, uint32_t &size) {
  offset = first_element * element_stride;
  size = std::min(std::max(0u, buffer_desc.ByteWidth - offset),
                  element_stride * num_elements);
};
#pragma endregion

} // namespace dxmt