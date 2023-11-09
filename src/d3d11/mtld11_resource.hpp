
#pragma once
// #include "Metal/MTLDevice.hpp"
#include "d3d11_device.hpp"
#include "d3d11_private.h"
#include "d3d11_device_child.h"
#include "d3d11_resource.hpp"

#include "../dxmt/dxmt_command_stream.hpp"
#include <utility>

typedef struct MappedResource {
  void *pData;
  uint32_t RowPitch;
  uint32_t DepthPitch;
} MappedResource;

MIDL_INTERFACE("d8a49d20-9a1f-4bb8-9ee6-442e064dce23")
IDXMTResource : public IUnknown {
  virtual HRESULT STDMETHODCALLTYPE CreateShaderResourceView(
      const D3D11_SHADER_RESOURCE_VIEW_DESC *desc,
      ID3D11ShaderResourceView **ppView) = 0;
  virtual HRESULT STDMETHODCALLTYPE CreateUnorderedAccessView(
      const D3D11_UNORDERED_ACCESS_VIEW_DESC *desc,
      ID3D11UnorderedAccessView **ppView) = 0;
  virtual HRESULT STDMETHODCALLTYPE CreateRenderTargetView(
      const D3D11_RENDER_TARGET_VIEW_DESC *desc,
      ID3D11RenderTargetView **ppView) = 0;
  virtual HRESULT STDMETHODCALLTYPE CreateDepthStencilView(
      const D3D11_DEPTH_STENCIL_VIEW_DESC *desc,
      ID3D11DepthStencilView **ppView) = 0;
  virtual HRESULT STDMETHODCALLTYPE MapDiscard(
      uint32_t subresource, MappedResource * resource,
      dxmt::DXMTCommandStream * cs) = 0;
  virtual HRESULT STDMETHODCALLTYPE Unmap(uint32_t subresource) = 0;
};
__CRT_UUID_DECL(IDXMTResource, 0xd8a49d20, 0x9a1f, 0x4bb8, 0x9e, 0xe6, 0x44,
                0x2e, 0x06, 0x4d, 0xce, 0x23);

MIDL_INTERFACE("1c7e7c98-6dd4-42f0-867b-67960806886e")
IDXMTBufferResource : public IDXMTResource {
  virtual dxmt::VertexBufferBinding *BindAsVertexBuffer(
      dxmt::DXMTCommandStream * cs) = 0;
  virtual dxmt::IndexBufferBinding *BindAsIndexBuffer(dxmt::DXMTCommandStream *
                                                      cs) = 0;
  virtual dxmt::ConstantBufferBinding *BindAsConstantBuffer(
      dxmt::DXMTCommandStream * cs) = 0;
};
__CRT_UUID_DECL(IDXMTBufferResource, 0x1c7e7c98, 0x6dd4, 0x42f0, 0x86, 0x7b,
                0x67, 0x96, 0x08, 0x06, 0x88, 0x6e);

MIDL_INTERFACE("4fe0ec8e-8be0-4c41-a9a4-11726ceba59c")
IDXMTTextureResource : public IDXMTResource{};
__CRT_UUID_DECL(IDXMTTextureResource, 0x4fe0ec8e, 0x8be0, 0x4c41, 0xa9, 0xa4,
                0x11, 0x72, 0x6c, 0xeb, 0xa5, 0x9c);

#pragma region constraints

template <typename T, template <typename> typename Expression, typename = void>
struct is_valid_expression : std::false_type {};

template <typename T, template <typename> typename Expression>
struct is_valid_expression<T, Expression, std::void_t<Expression<T>>>
    : std::true_type {};

template <typename T>
using ImplHasDepDevice = decltype(std::declval<T>().__device__);

template <typename T>
using ImplHasDepRes = decltype(std::declval<T>().__resource__);

template <typename T>
using ImplHasDepData = decltype(std::declval<T>().__data__);

template <typename T>
using ImplHasCreateShaderResourceView =
    decltype(std::declval<T>().CreateShaderResourceView(
        std::declval<const D3D11_SHADER_RESOURCE_VIEW_DESC *>(),
        std::declval<ID3D11ShaderResourceView **>()));

template <typename T>
using ImplHasCreateUnorderedAccessView =
    decltype(std::declval<T>().CreateUnorderedAccessView(
        std::declval<const D3D11_UNORDERED_ACCESS_VIEW_DESC *>(),
        std::declval<ID3D11UnorderedAccessView **>()));

template <typename T>
using ImplHasCreateRenderTargetView =
    decltype(std::declval<T>().CreateRenderTargetView(
        std::declval<const D3D11_RENDER_TARGET_VIEW_DESC *>(),
        std::declval<ID3D11RenderTargetView **>()));

template <typename T>
using ImplHasCreateDepthStencilView =
    decltype(std::declval<T>().CreateDepthStencilView(
        std::declval<const D3D11_DEPTH_STENCIL_VIEW_DESC *>(),
        std::declval<ID3D11DepthStencilView **>()));

template <typename T>
using ImplHasBindAsVertexBuffer = decltype(std::declval<T>().BindAsVertexBuffer(
    std::declval<dxmt::DXMTCommandStream *>()));

template <typename T>
using ImplHasBindAsIndexBuffer = decltype(std::declval<T>().BindAsIndexBuffer(
    std::declval<dxmt::DXMTCommandStream *>()));

template <typename T>
using ImplHasBindAsConstantBuffer =
    decltype(std::declval<T>().BindAsConstantBuffer(
        std::declval<dxmt::DXMTCommandStream *>()));

template <typename T>
using ImplHasMapDiscard = decltype(std::declval<T>().MapDiscard(
    std::declval<uint32_t>(), std::declval<MappedResource *>(),
    std::declval<dxmt::DXMTCommandStream *>()));

template <typename T>
using ImplHasUnmap =
    decltype(std::declval<T>().Unmap(std::declval<uint32_t>()));
#pragma endregion

namespace dxmt {

template <typename Impl, typename Base = IDXMTResource>
class DXMTResource : public ComAggregatedObject<IUnknown, Base> {

public:
  DXMTResource(typename Impl::Interface *outer, IMTLD3D11Device *pDevice,
               typename Impl::Data data,
               const typename Impl::Description *pDesc,
               const D3D11_SUBRESOURCE_DATA *pData)
      : ComAggregatedObject<IUnknown, Base>(outer),
        impl(pDevice, data, pDesc, pData) {
    if constexpr (is_valid_expression<Impl, ImplHasDepDevice>::value) {
      impl.__device__ = pDevice;
    }
    if constexpr (is_valid_expression<Impl, ImplHasDepData>::value) {
      impl.__data__ = data;
    }
    if constexpr (is_valid_expression<Impl, ImplHasDepRes>::value) {
      impl.__resource__ = outer;
    }
  }
  ~DXMTResource() {}
  virtual HRESULT STDMETHODCALLTYPE
  CreateShaderResourceView(const D3D11_SHADER_RESOURCE_VIEW_DESC *desc,
                           ID3D11ShaderResourceView **ppView) final {

    if constexpr (is_valid_expression<Impl,
                                      ImplHasCreateShaderResourceView>::value) {
      return impl.CreateShaderResourceView(desc, ppView);
    }
    // TODO: WARN
    return E_FAIL;
  };
  virtual HRESULT STDMETHODCALLTYPE
  CreateUnorderedAccessView(const D3D11_UNORDERED_ACCESS_VIEW_DESC *desc,
                            ID3D11UnorderedAccessView **ppView) final {
    if constexpr (is_valid_expression<
                      Impl, ImplHasCreateUnorderedAccessView>::value) {
      return impl.CreateUnorderedAccessView(desc, ppView);
    }
    // TODO: WARN
    return E_FAIL;
  };
  virtual HRESULT STDMETHODCALLTYPE
  CreateRenderTargetView(const D3D11_RENDER_TARGET_VIEW_DESC *desc,
                         ID3D11RenderTargetView **ppView) final {
    if constexpr (is_valid_expression<Impl,
                                      ImplHasCreateRenderTargetView>::value) {
      return impl.CreateRenderTargetView(desc, ppView);
    }
    // TODO: WARN
    return E_FAIL;
  };
  virtual HRESULT STDMETHODCALLTYPE
  CreateDepthStencilView(const D3D11_DEPTH_STENCIL_VIEW_DESC *desc,
                         ID3D11DepthStencilView **ppView) final {
    if constexpr (is_valid_expression<Impl,
                                      ImplHasCreateDepthStencilView>::value) {
      return impl.CreateDepthStencilView(desc, ppView);
    }
    // TODO: WARN
    return E_FAIL;
  };

  virtual HRESULT STDMETHODCALLTYPE MapDiscard(uint32_t subresource,
                                               MappedResource *resource,
                                               DXMTCommandStream *cs) final {
    if constexpr (is_valid_expression<Impl, ImplHasMapDiscard>::value) {
      return this->impl.MapDiscard(subresource, resource, cs);
    }
    return E_NOTIMPL;
  };

  virtual HRESULT STDMETHODCALLTYPE Unmap(uint32_t subresource) final {
    if constexpr (is_valid_expression<Impl, ImplHasUnmap>::value) {
      return this->impl.Unmap(subresource);
    }
    return E_NOTIMPL;
  };

  Impl impl;
};

template <typename Impl>
class DXMTBufferResource : public DXMTResource<Impl, IDXMTBufferResource> {

public:
  DXMTBufferResource(typename Impl::Interface *outer, IMTLD3D11Device *pDevice,
                     typename Impl::Data data,
                     const typename Impl::Description *pDesc,
                     const D3D11_SUBRESOURCE_DATA *pData)
      : DXMTResource<Impl, IDXMTBufferResource>(outer, pDevice, data, pDesc,
                                                pData) {}

  virtual dxmt::VertexBufferBinding *
  BindAsVertexBuffer(DXMTCommandStream *cs) final {
    if constexpr (is_valid_expression<Impl, ImplHasBindAsVertexBuffer>::value) {
      return this->impl.BindAsVertexBuffer(cs);
    }
    // TODO: WARN
    return nullptr;
  };
  virtual dxmt::IndexBufferBinding *
  BindAsIndexBuffer(DXMTCommandStream *cs) final {
    if constexpr (is_valid_expression<Impl, ImplHasBindAsIndexBuffer>::value) {
      return this->impl.BindAsIndexBuffer(cs);
    }
    // TODO: WARN
    return nullptr;
  };
  virtual dxmt::ConstantBufferBinding *
  BindAsConstantBuffer(DXMTCommandStream *cs) final {
    if constexpr (is_valid_expression<Impl,
                                      ImplHasBindAsConstantBuffer>::value) {
      return this->impl.BindAsConstantBuffer(cs);
    }
    // TODO: WARN
    return nullptr;
  };
};

template <typename Impl>
class DXMTTextureResource : public DXMTResource<Impl, IDXMTTextureResource> {

public:
  DXMTTextureResource(typename Impl::Interface *outer, IMTLD3D11Device *pDevice,
                      typename Impl::Data data,
                      const typename Impl::Description *pDesc,
                      const D3D11_SUBRESOURCE_DATA *pData)
      : DXMTResource<Impl, IDXMTTextureResource>(outer, pDevice, data, pDesc,
                                                 pData) {}
};

template <typename A>
class D3D11Resource : public MTLD3D11DeviceChild<typename A::Interface> {
public:
  void STDMETHODCALLTYPE GetType(D3D11_RESOURCE_DIMENSION *pResourceDimension) {
    *pResourceDimension = A::Dimension;
  };

  void STDMETHODCALLTYPE SetEvictionPriority(UINT EvictionPriority) final {
    WARN("Stub");
  };

  UINT STDMETHODCALLTYPE GetEvictionPriority() final {
    return DXGI_RESOURCE_PRIORITY_NORMAL;
  };

  void STDMETHODCALLTYPE GetDesc(typename A::Description *pDesc) final {
    if (pDesc != NULL) {
      *pDesc = desc_;
    }
  };

  using Base__ = typename std::conditional<
      std::is_base_of<ID3D11Buffer, typename A::Interface>::value,
      IDXMTBufferResource, IDXMTTextureResource>::type;

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                           void **ppvObject) final {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D11DeviceChild) ||
        riid == __uuidof(ID3D11Resource) ||
        riid == __uuidof(typename A::Interface)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (riid == __uuidof(IDXGIObject) ||
        riid == __uuidof(IDXGIDeviceSubObject) ||
        riid == __uuidof(IDXGIResource) || riid == __uuidof(IDXGIResource1)) {
      *ppvObject = ref(&dxgi_resource_);
      return S_OK;
    }

    if (riid == __uuidof(IDXMTResource) || riid == __uuidof(Base__)) {
      *ppvObject = ref(&dxmt_resource_);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(typename A::Interface), riid)) {
      WARN("Unknown interface query", str::format(riid));
    }

    return E_NOINTERFACE;
  };

  D3D11Resource(IMTLD3D11Device *pDevice, typename A::Data impl_data,
                const typename A::Description *desc,
                const D3D11_SUBRESOURCE_DATA *res_data)
      : MTLD3D11DeviceChild<typename A::Interface>(pDevice), desc_(*desc),
        dxgi_resource_(this),
        dxmt_resource_(this, pDevice, impl_data, desc, res_data){};

protected:
  typename A::Description desc_;
  MTLDXGIResource dxgi_resource_;

  using Resource = typename std::conditional<
      std::is_base_of<ID3D11Buffer, typename A::Interface>::value,
      DXMTBufferResource<A>, DXMTTextureResource<A>>::type; // 人言否
  // 像是溜大了写出来的
public:
  Resource dxmt_resource_;
};

} // namespace dxmt