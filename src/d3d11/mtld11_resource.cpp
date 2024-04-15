
#include "mtld11_resource.hpp"
#include "Metal/MTLBuffer.hpp"
#include "Metal/MTLTexture.hpp"
#include "d3d11_device_child.hpp"
#include "d3d11_texture.hpp"
#include "com/com_pointer.hpp"
#include "com/com_aggregatable.hpp"
#include "com/com_guid.hpp"
#include "log/log.hpp"

namespace dxmt {

template <typename tag>
class ResourceBase : public MTLD3D11DeviceChild<typename tag::COM> {
public:
  ResourceBase(const tag::DESC_S *desc, IMTLD3D11Device *device)
      : MTLD3D11DeviceChild<ID3D11Buffer>(device), desc(*desc) {}

  HRESULT QueryInterface(REFIID riid, void **ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D11DeviceChild) ||
        riid == __uuidof(ID3D11Resource) ||
        riid == __uuidof(typename tag::COM)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    // if (riid == __uuidof(IDXGIObject) ||
    //     riid == __uuidof(IDXGIDeviceSubObject) ||
    //     riid == __uuidof(IDXGIResource) || riid == __uuidof(IDXGIResource1))
    //     {
    //   *ppvObject = ref(&m_resource);
    //   return S_OK;
    // }

    if (SUCCEEDED(this->PrivateQueryInterface(riid, ppvObject))) {
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(typename tag::COM), riid)) {
      Logger::warn("QueryInterface: Unknown interface query");
      Logger::warn(str::format(riid));
    }

    return E_NOINTERFACE;
  }

  void GetDesc(tag::DESC *pDesc) final { *pDesc = desc; }

  void GetType(D3D11_RESOURCE_DIMENSION *pResourceDimension) final {
    *pResourceDimension = tag::dimension;
  }

  void SetEvictionPriority(UINT EvictionPriority) final {}

  UINT GetEvictionPriority() final { return DXGI_RESOURCE_PRIORITY_NORMAL; }

  virtual HRESULT PrivateQueryInterface(REFIID riid, void **ppvObject) {
    return E_FAIL;
  };

protected:
  tag::DESC_S desc;
};

#pragma region StagingBuffer

class StagingBuffer : public ResourceBase<tag_buffer> {
public:
  StagingBuffer(const tag_buffer::DESC_S *desc, IMTLD3D11Device *device)
      : ResourceBase<tag_buffer>(desc, device) {
    auto metal = device->GetMTLDevice();
    buffer = transfer(metal->newBuffer(desc->ByteWidth, 0)); // FIXME
  }

  Obj<MTL::Buffer> buffer;
};

#pragma endregion

#pragma region StagingBuffer

template <typename tag_texture>
class StagingTexture : public ResourceBase<tag_texture> {
  using CONTEXT = StagingTexture<tag_texture>;

  struct SomeWeirdInnerClass : public ComAggregatedObject<CONTEXT, IUnknown> {
    SomeWeirdInnerClass(CONTEXT *ctx)
        : ComAggregatedObject<CONTEXT, IUnknown>(ctx) {
    }
  };

public:
  StagingTexture(const tag_texture::DESC_S *desc, IMTLD3D11Device *device)
      : ResourceBase<tag_texture>(desc, device), s(this) {
    auto descriptor = getTextureDescriptor(desc);
    auto metal = device->GetMTLDevice();
    texture = transfer(metal->newTexture(descriptor));
  }

  virtual HRESULT PrivateQueryInterface(REFIID riid, void **ppvObject) {

    return E_FAIL;
  }

  Obj<MTL::Texture> texture;
  SomeWeirdInnerClass s;
};

using StagingTexture2D = StagingTexture<tag_texture_2d>;

static_assert(std::is_abstract_v<StagingTexture2D> == false);

#pragma endregion

} // namespace dxmt