#include "Metal/MTLPixelFormat.hpp"
#include "Metal/MTLBuffer.hpp"
#include "Metal/MTLTexture.hpp"
#include "d3d11_private.h"
#include "com/com_pointer.hpp"
#include "d3d11_device.hpp"
#include "dxmt_buffer_pool.hpp"
#include "dxmt_format.hpp"
#include "mtld11_resource.hpp"
#include "objc_pointer.hpp"
#include <vector>

namespace dxmt {

#pragma region DynamicTexture

class DynamicTexture2D
    : public TResourceBase<tag_texture_2d, IMTLDynamicBuffer> {
private:
  Obj<MTL::Buffer> buffer;
  uint64_t buffer_handle;
  void *buffer_mapped;
  size_t bytes_per_row;

  using SRVBase = TResourceViewBase<tag_shader_resource_view<DynamicTexture2D>,
                                    IMTLBindable>;

  class SRV : public SRVBase {
    Obj<MTL::Texture> view;
    MTL::ResourceID view_handle;
    Obj<MTL::TextureDescriptor> view_desc;
    SIMPLE_RESIDENCY_TRACKER tracker{};

  public:
    SRV(const tag_shader_resource_view<>::DESC1 *pDesc,
        DynamicTexture2D *pResource, MTLD3D11Device *pDevice,
        Obj<MTL::TextureDescriptor> &&view_desc)
        : SRVBase(pDesc, pResource, pDevice), view_desc(std::move(view_desc)) {}

    ~SRV() {
      auto& vec = resource->weak_srvs;
      vec.erase(std::remove(vec.begin(), vec.end(), this), vec.end());
    }

    BindingRef UseBindable(uint64_t seq_id) override {
      if (!seq_id)
        return BindingRef();
      return BindingRef(static_cast<ID3D11View *>(this), view.ptr());
    };

    ArgumentData
    GetArgumentData(SIMPLE_RESIDENCY_TRACKER **ppTracker) override {
      *ppTracker = &tracker;
      return ArgumentData(view_handle, view.ptr());
    }

    bool GetContentionState(uint64_t) override { return true; }

    void
    GetLogicalResourceOrView(REFIID riid, void **ppLogicalResource) override {
      if (riid == __uuidof(IMTLDynamicBuffer)) {
        resource->QueryInterface(riid, ppLogicalResource);
        return;
      }
      this->QueryInterface(riid, ppLogicalResource);
    }

    struct ViewCache {
      Obj<MTL::Texture> view;
      MTL::ResourceID view_handle;
    };

    // FIXME: use LRU cache instead!
    std::unordered_map<uint64_t, ViewCache> cache;

    void RotateView() {
      tracker = {};
      auto insert = cache.insert({resource->buffer_handle, {}});
      auto &item = insert.first->second;
      if (insert.second) {
        item.view = transfer(resource->buffer->newTexture(
            view_desc, 0, resource->bytes_per_row));
        item.view_handle = item.view->gpuResourceID();
      }
      view = item.view;
      view_handle = item.view_handle;
    };
  };

  std::vector<SRV *> weak_srvs;
  std::unique_ptr<BufferPool> pool_;

public:
  DynamicTexture2D(const tag_texture_2d::DESC1 *pDesc,
                   Obj<MTL::Buffer> &&buffer, MTLD3D11Device *device,
                   UINT bytesPerRow)
      : TResourceBase<tag_texture_2d, IMTLDynamicBuffer>(*pDesc, device),
        buffer(std::move(buffer)), buffer_handle(this->buffer->gpuAddress()),
        buffer_mapped(this->buffer->contents()), bytes_per_row(bytesPerRow) {

    pool_ = std::make_unique<BufferPool>(device->GetMTLDevice(),
                                         this->buffer->length(),
                                         this->buffer->resourceOptions());
  }

  void *GetMappedMemory(UINT *pBytesPerRow, UINT *pBytesPerImage) override {
    *pBytesPerRow = bytes_per_row;
    *pBytesPerImage = bytes_per_row * desc.Height;
    return buffer_mapped;
  };

  UINT GetSize(UINT *pBytesPerRow, UINT *pBytesPerImage) override {
    *pBytesPerRow = bytes_per_row;
    *pBytesPerImage = bytes_per_row * desc.Height;
    return buffer->length();
  };

  BindingRef GetCurrentBufferBinding() override {
    return BindingRef(static_cast<ID3D11Resource *>(this), buffer.ptr(),
                      buffer->length(), 0);
  };

  void RotateBuffer(MTLD3D11Device *exch) override {
    exch->ExchangeFromPool(&buffer, &buffer_handle, &buffer_mapped,
                           pool_.get());
    for (auto srv : weak_srvs) {
      srv->RotateView();
    }
  };

  D3D11_BIND_FLAG GetBindFlag() override {
    return (D3D11_BIND_FLAG)desc.BindFlags;
  }

  HRESULT CreateShaderResourceView(const D3D11_SHADER_RESOURCE_VIEW_DESC1 *pDesc,
                                   ID3D11ShaderResourceView1 **ppView) override {
    D3D11_SHADER_RESOURCE_VIEW_DESC1 finalDesc;
    if (FAILED(ExtractEntireResourceViewDescription(&this->desc, pDesc,
                                                    &finalDesc))) {
      return E_INVALIDARG;
    }
    if (finalDesc.ViewDimension != D3D10_SRV_DIMENSION_TEXTURE2D) {
      ERR("Only 2d texture SRV can be created on dynamic texture");
      return E_FAIL;
    }
    if (finalDesc.Texture2D.MostDetailedMip != 0 ||
        finalDesc.Texture2D.MipLevels != 1) {
      ERR("2d texture SRV must be non-mipmapped on dynamic texture");
      return E_FAIL;
    }
    if (!ppView) {
      return S_FALSE;
    }
    MTL_DXGI_FORMAT_DESC format;
    if (FAILED(MTLQueryDXGIFormat(m_parent->GetMTLDevice(), finalDesc.Format,
                                  format))) {
      return E_FAIL;
    }

    auto desc = transfer(MTL::TextureDescriptor::alloc()->init());
    desc->setTextureType(MTL::TextureType2D);
    desc->setWidth(this->desc.Width);
    desc->setHeight(this->desc.Height);
    desc->setDepth(1);
    desc->setArrayLength(1);
    desc->setMipmapLevelCount(1);
    desc->setSampleCount(1);
    desc->setUsage(MTL::TextureUsageShaderRead);
    desc->setStorageMode(MTL::StorageModeShared);

    desc->setResourceOptions(buffer->resourceOptions());
    desc->setPixelFormat(format.PixelFormat);
    auto srv = ref(new SRV(&finalDesc, this, m_parent, std::move(desc)));

    weak_srvs.push_back(srv);
    *ppView = srv;
    srv->RotateView();
    return S_OK;
  };
};

#pragma endregion

HRESULT
CreateDynamicTexture2D(MTLD3D11Device *pDevice,
                       const D3D11_TEXTURE2D_DESC1 *pDesc,
                       const D3D11_SUBRESOURCE_DATA *pInitialData,
                       ID3D11Texture2D1 **ppTexture) {
  MTL_DXGI_FORMAT_DESC format;
  MTLQueryDXGIFormat(pDevice->GetMTLDevice(), pDesc->Format, format);
  if (format.Flag  & MTL_DXGI_FORMAT_BC) {
    return E_FAIL;
  }
  if (format.PixelFormat == MTL::PixelFormatInvalid) {
    return E_FAIL;
  }
  Obj<MTL::TextureDescriptor> textureDescriptor;
  D3D11_TEXTURE2D_DESC1 finalDesc;
  if (FAILED(CreateMTLTextureDescriptor(pDevice, pDesc, &finalDesc,
                                        &textureDescriptor))) {
    return E_INVALIDARG;
  }
  uint32_t bytesPerRow, bytesPerImage, bufferLen;
  if (FAILED(GetLinearTextureLayout(pDevice, &finalDesc, 0, &bytesPerRow,
                                    &bytesPerImage, &bufferLen))) {
    return E_FAIL;
  }
  auto metal = pDevice->GetMTLDevice();
  auto buffer = transfer(metal->newBuffer(
      bufferLen, pDevice->IsTraced()
                     ? MTL::ResourceCPUCacheModeDefaultCache
                     : MTL::ResourceOptionCPUCacheModeWriteCombined));
  if (pInitialData) {
    D3D11_ASSERT(pInitialData->SysMemPitch == bytesPerRow);
    memcpy(buffer->contents(), pInitialData->pSysMem, bufferLen);
  }
  *ppTexture =
      ref(new DynamicTexture2D(pDesc, std::move(buffer), pDevice, bytesPerRow));
  return S_OK;
}

} // namespace dxmt
