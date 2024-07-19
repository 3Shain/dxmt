#include "Metal/MTLPixelFormat.hpp"
#include "Metal/MTLBuffer.hpp"
#include "Metal/MTLTexture.hpp"
#include "d3d11_private.h"
#include "com/com_pointer.hpp"
#include "d3d11_device.hpp"
#include "dxgi_interfaces.h"
#include "dxmt_buffer_pool.hpp"
#include "mtld11_resource.hpp"
#include "objc_pointer.hpp"
#include <vector>

namespace dxmt {

#pragma region DynamicViewBindable
struct IMTLNotifiedBindable : public IMTLBindable {
  virtual void NotifyObserver(MTL::Buffer *resource) = 0;
};

template <typename BindableMap, typename GetArgumentData_,
          typename ReleaseCallback>
class DynamicBinding : public ComObject<IMTLNotifiedBindable> {
private:
  Com<IUnknown> parent;
  BufferSwapCallback observer;
  BindableMap fn;
  GetArgumentData_ get_argument_data;
  ReleaseCallback cb;

public:
  DynamicBinding(IUnknown *parent, BufferSwapCallback &&observer,
                 BindableMap &&fn, GetArgumentData_ &&get_argument_data,
                 ReleaseCallback &&cb)
      : ComObject(), parent(parent), observer(std::move(observer)),
        fn(std::forward<BindableMap>(fn)),
        get_argument_data(std::forward<GetArgumentData_>(get_argument_data)),
        cb(std::forward<ReleaseCallback>(cb)) {}

  ~DynamicBinding() { std::invoke(cb, this); }

  HRESULT QueryInterface(REFIID riid, void **ppvObject) override {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(IMTLBindable)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    return E_NOINTERFACE;
  }

  BindingRef UseBindable(uint64_t x) override { return std::invoke(fn, x); };

  ArgumentData GetArgumentData(SIMPLE_RESIDENCY_TRACKER **ppTracker) override {
    return std::invoke(get_argument_data, ppTracker);
  }

  bool GetContentionState(uint64_t finishedSeqId) override { return true; }

  void GetLogicalResourceOrView(REFIID riid,
                                void **ppLogicalResource) override {
    parent->QueryInterface(riid, ppLogicalResource);
  };

  void NotifyObserver(MTL::Buffer *resource) override { observer(resource); };
};
#pragma endregion

#pragma region DynamicBuffer

class DynamicBuffer
    : public TResourceBase<tag_buffer, IMTLDynamicBuffer, IMTLDynamicBindable> {
private:
  Obj<MTL::Buffer> buffer_dynamic;
  uint64_t buffer_handle;
  uint64_t buffer_len;
  void *buffer_mapped;
#ifdef DXMT_DEBUG
  std::string debug_name;
#endif
  bool structured;
  bool allow_raw_view;

  std::vector<IMTLNotifiedBindable *> observers;

  using SRVBase = TResourceViewBase<tag_shader_resource_view<DynamicBuffer>,
                                    IMTLDynamicBindable>;

  class SRV : public SRVBase {
  private:
    size_t offset;
    size_t width;
    SIMPLE_RESIDENCY_TRACKER tracker{};

  public:
    SRV(const tag_shader_resource_view<>::DESC_S *pDesc,
        DynamicBuffer *pResource, IMTLD3D11Device *pDevice, size_t offset,
        size_t width)
        : SRVBase(pDesc, pResource, pDevice), offset(offset), width(width) {}

    ~SRV() {
      auto &vec = resource->weak_srvs;
      vec.erase(std::remove(vec.begin(), vec.end(), this), vec.end());
    }

    void GetBindable(IMTLBindable **ppResource,
                     BufferSwapCallback &&onBufferSwap) override {
      IMTLNotifiedBindable *ret = ref(new DynamicBinding(
          static_cast<ID3D11ShaderResourceView *>(this),
          std::move(onBufferSwap),
          [this](uint64_t) {
            return BindingRef(static_cast<ID3D11View *>(this),
                              this->resource->buffer_dynamic, this->width,
                              this->offset);
          },
          [this](SIMPLE_RESIDENCY_TRACKER **ppTracker) {
            *ppTracker = &tracker;
            return ArgumentData(this->resource->buffer_handle + this->offset,
                                this->width);
          },
          [u = this->resource.ptr()](IMTLNotifiedBindable *_this) {
            u->RemoveObserver(_this);
          }));
      resource->AddObserver(ret);
      *ppResource = ret;
    };

    void RotateView() { tracker = {}; };
  };

  class TBufferSRV : public SRVBase {
    Obj<MTL::Texture> view;
    MTL::ResourceID view_handle;
    Obj<MTL::TextureDescriptor> view_desc;
    SIMPLE_RESIDENCY_TRACKER tracker{};
    uint64_t byte_offset;

  public:
    TBufferSRV(const tag_shader_resource_view<>::DESC_S *pDesc,
               DynamicBuffer *pResource, IMTLD3D11Device *pDevice,
               Obj<MTL::TextureDescriptor> &&view_desc, uint64_t byte_offset)
        : SRVBase(pDesc, pResource, pDevice), view_desc(std::move(view_desc)),
          byte_offset(byte_offset) {}

    ~TBufferSRV() {
      auto vec = resource->weak_srvs_tbuffer;
      vec.erase(std::remove(vec.begin(), vec.end(), this), vec.end());
    }

    void GetBindable(IMTLBindable **ppResource,
                     BufferSwapCallback &&onBufferSwap) override {
      auto ret = ref(new DynamicBinding(
          static_cast<ID3D11ShaderResourceView *>(this),
          std::move(onBufferSwap),
          [this](uint64_t) {
            return BindingRef(static_cast<ID3D11View *>(this),
                              this->view.ptr());
          },
          [this](SIMPLE_RESIDENCY_TRACKER **ppTracker) {
            *ppTracker = &tracker;
            return ArgumentData(this->view_handle, this->view.ptr());
          },
          [u = this->resource.ptr()](IMTLNotifiedBindable *_this) {
            u->RemoveObserver(_this);
          }));
      resource->AddObserver(ret);
      *ppResource = ret;
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
        item.view = transfer(resource->buffer_dynamic->newTexture(
            view_desc, byte_offset, resource->buffer_len));
        item.view_handle = item.view->gpuResourceID();
      }
      view = item.view;
      view_handle = item.view_handle;
    };
  };

  std::vector<SRV *> weak_srvs;
  std::vector<TBufferSRV *> weak_srvs_tbuffer;

  std::unique_ptr<BufferPool> pool;

  SIMPLE_RESIDENCY_TRACKER tracker{};

public:
  DynamicBuffer(const tag_buffer::DESC_S *desc,
                const D3D11_SUBRESOURCE_DATA *pInitialData,
                IMTLD3D11Device *device)
      : TResourceBase<tag_buffer, IMTLDynamicBuffer, IMTLDynamicBindable>(
            desc, device) {
    auto metal = device->GetMTLDevice();
    // sadly it needs to be tracked since it's a legal blit dst
    auto options = MTL::ResourceOptionCPUCacheModeWriteCombined;
    buffer_dynamic = transfer(metal->newBuffer(desc->ByteWidth, options));
    if (pInitialData) {
      memcpy(buffer_dynamic->contents(), pInitialData->pSysMem,
             desc->ByteWidth);
    }
    buffer_handle = buffer_dynamic->gpuAddress();
    buffer_len = buffer_dynamic->length();
    buffer_mapped = buffer_dynamic->contents();
    pool = std::make_unique<BufferPool>(metal, desc->ByteWidth, options);
    structured = desc->MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    allow_raw_view =
        desc->MiscFlags & D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
  }

  void *GetMappedMemory(UINT *pBytesPerRow, UINT *pBytesPerImage) override {
    *pBytesPerRow = buffer_len;
    *pBytesPerImage = buffer_len;
    return buffer_mapped;
  };

  void GetBindable(IMTLBindable **ppResource,
                   BufferSwapCallback &&onBufferSwap) override {
    auto ret = ref(new DynamicBinding(
        static_cast<ID3D11Buffer *>(this), std::move(onBufferSwap),
        [this](uint64_t) {
          return BindingRef(static_cast<ID3D11Resource *>(this),
                            buffer_dynamic.ptr());
        },
        [this](SIMPLE_RESIDENCY_TRACKER **ppTracker) {
          *ppTracker = &tracker;
          return ArgumentData(this->buffer_handle);
        },
        [this](IMTLNotifiedBindable *_binding) {
          this->RemoveObserver(_binding);
        }));
    AddObserver(ret);
    *ppResource = ret;
  };

  void RotateBuffer(IMTLDynamicBufferExchange *exch) override {
    tracker = {};
    exch->ExchangeFromPool(&buffer_dynamic, &buffer_handle, &buffer_mapped,
                           pool.get());
#ifdef DXMT_DEBUG
    {
      auto pool = transfer(NS::AutoreleasePool::alloc()->init());
      buffer_dynamic->setLabel(
          NS::String::string(debug_name.c_str(), NS::ASCIIStringEncoding));
    }
#endif
    for (auto srv : weak_srvs) {
      srv->RotateView();
    }
    for (auto srv : weak_srvs_tbuffer) {
      srv->RotateView();
    }
    for (auto &observer : observers) {
      observer->NotifyObserver(buffer_dynamic.ptr());
    }
  };

  void AddObserver(IMTLNotifiedBindable *pBindable) {
    observers.push_back(pBindable);
  }

  void RemoveObserver(IMTLNotifiedBindable *pBindable) {
    // D3D11_ASSERT(observers.erase(pBindable) == 1 &&
    //        "it must be 1 unless the destructor called twice");
    auto &vec = observers;
    vec.erase(std::remove(vec.begin(), vec.end(), pBindable), vec.end());
  }

  HRESULT
  CreateShaderResourceView(const D3D11_SHADER_RESOURCE_VIEW_DESC *pDesc,
                           ID3D11ShaderResourceView **ppView) override {
    D3D11_SHADER_RESOURCE_VIEW_DESC finalDesc;
    if (FAILED(ExtractEntireResourceViewDescription(&this->desc, pDesc,
                                                    &finalDesc))) {
      return E_INVALIDARG;
    }
    if (finalDesc.ViewDimension != D3D11_SRV_DIMENSION_BUFFER &&
        finalDesc.ViewDimension != D3D11_SRV_DIMENSION_BUFFEREX) {
      return E_FAIL;
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
          this->desc, desc.StructureByteStride, finalDesc.Buffer.FirstElement,
          finalDesc.Buffer.NumElements, offset, size);
      *ppView = ref(new SRV(&finalDesc, this, m_parent, offset, size));
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
          this->desc, sizeof(uint32_t), finalDesc.Buffer.FirstElement,
          finalDesc.Buffer.NumElements, offset, size);
      *ppView = ref(new SRV(&finalDesc, this, m_parent, offset, size));
      return S_OK;
    }

    MTL_FORMAT_DESC format;
    Com<IMTLDXGIAdatper> adapter;
    m_parent->GetAdapter(&adapter);
    if (FAILED(adapter->QueryFormatDesc(finalDesc.Format, &format))) {
      return E_FAIL;
    }

    auto desc = transfer(MTL::TextureDescriptor::alloc()->init());
    desc->setTextureType(MTL::TextureTypeTextureBuffer);
    desc->setWidth(finalDesc.Buffer.NumElements);
    desc->setHeight(1);
    desc->setDepth(1);
    desc->setArrayLength(1);
    desc->setMipmapLevelCount(1);
    desc->setSampleCount(1);
    desc->setUsage(MTL::TextureUsageShaderRead);
    desc->setStorageMode(MTL::StorageModeShared);
    desc->setCpuCacheMode(MTL::CPUCacheModeWriteCombined);
    desc->setPixelFormat(format.PixelFormat);
    auto srv = ref(
        new TBufferSRV(&finalDesc, this, m_parent, std::move(desc),
                       finalDesc.Buffer.FirstElement * format.BytesPerTexel));
    weak_srvs_tbuffer.push_back(srv);
    *ppView = srv;
    srv->RotateView();
    return S_OK;
  };

  void OnSetDebugObjectName(LPCSTR Name) override {
    if (!Name) {
      return;
    }
#ifdef DXMT_DEBUG
    debug_name = std::string(Name);
#endif
  }
};

#pragma endregion

HRESULT
CreateDynamicBuffer(IMTLD3D11Device *pDevice, const D3D11_BUFFER_DESC *pDesc,
                    const D3D11_SUBRESOURCE_DATA *pInitialData,
                    ID3D11Buffer **ppBuffer) {
  *ppBuffer = ref(new DynamicBuffer(pDesc, pInitialData, pDevice));
  return S_OK;
}

#pragma region DynamicTexture

class DynamicTexture2D
    : public TResourceBase<tag_texture_2d, IMTLDynamicBuffer> {
private:
  Obj<MTL::Buffer> buffer;
  uint64_t buffer_handle;
  void *buffer_mapped;
  size_t bytes_per_row;

  using SRVBase = TResourceViewBase<tag_shader_resource_view<DynamicTexture2D>,
                                    IMTLDynamicBindable>;

  class SRV : public SRVBase {
    Obj<MTL::Texture> view;
    MTL::ResourceID view_handle;
    Obj<MTL::TextureDescriptor> view_desc;
    SIMPLE_RESIDENCY_TRACKER tracker{};

  public:
    SRV(const tag_shader_resource_view<>::DESC_S *pDesc,
        DynamicTexture2D *pResource, IMTLD3D11Device *pDevice,
        Obj<MTL::TextureDescriptor> &&view_desc)
        : SRVBase(pDesc, pResource, pDevice), view_desc(std::move(view_desc)) {}

    ~SRV() {
      auto vec = resource->weak_srvs;
      vec.erase(std::remove(vec.begin(), vec.end(), this), vec.end());
    }

    void GetBindable(IMTLBindable **ppResource,
                     BufferSwapCallback &&onBufferSwap) override {
      auto ret = ref(new DynamicBinding(
          static_cast<ID3D11ShaderResourceView *>(this),
          std::move(onBufferSwap),
          [this](uint64_t) {
            return BindingRef(static_cast<ID3D11View *>(this),
                              this->view.ptr());
          },
          [this](SIMPLE_RESIDENCY_TRACKER **ppTracker) {
            *ppTracker = &tracker;
            return ArgumentData(this->view_handle, this->view.ptr());
          },
          [u = this->resource.ptr()](IMTLNotifiedBindable *_this) {
            u->RemoveObserver(_this);
          }));
      resource->AddObserver(ret);
      *ppResource = ret;
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

  std::vector<IMTLNotifiedBindable *> observers;
  std::vector<SRV *> weak_srvs;
  std::unique_ptr<BufferPool> pool_;

public:
  DynamicTexture2D(const tag_texture_2d::DESC_S *desc,
                   Obj<MTL::Buffer> &&buffer, IMTLD3D11Device *device,
                   UINT bytesPerRow)
      : TResourceBase<tag_texture_2d, IMTLDynamicBuffer>(desc, device),
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

  void RotateBuffer(IMTLDynamicBufferExchange *exch) override {
    exch->ExchangeFromPool(&buffer, &buffer_handle, &buffer_mapped,
                           pool_.get());
    for (auto srv : weak_srvs) {
      srv->RotateView();
    }
    for (auto &observer : observers) {
      observer->NotifyObserver(buffer.ptr());
    }
  };

  void AddObserver(IMTLNotifiedBindable *pBindable) {
    observers.push_back(pBindable);
  }

  void RemoveObserver(IMTLNotifiedBindable *pBindable) {
    // D3D11_ASSERT(observers.erase(pBindable) == 1 &&
    //        "it must be 1 unless the destructor called twice");
    auto &vec = observers;
    vec.erase(std::remove(vec.begin(), vec.end(), pBindable), vec.end());
  }

  HRESULT CreateShaderResourceView(const D3D11_SHADER_RESOURCE_VIEW_DESC *pDesc,
                                   ID3D11ShaderResourceView **ppView) override {
    D3D11_SHADER_RESOURCE_VIEW_DESC finalDesc;
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
    MTL_FORMAT_DESC format;
    Com<IMTLDXGIAdatper> adapter;
    m_parent->GetAdapter(&adapter);
    if (FAILED(adapter->QueryFormatDesc(finalDesc.Format, &format))) {
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
    desc->setCpuCacheMode(MTL::CPUCacheModeWriteCombined);
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
CreateDynamicTexture2D(IMTLD3D11Device *pDevice,
                       const D3D11_TEXTURE2D_DESC *pDesc,
                       const D3D11_SUBRESOURCE_DATA *pInitialData,
                       ID3D11Texture2D **ppTexture) {
  Com<IMTLDXGIAdatper> adapter;
  pDevice->GetAdapter(&adapter);
  MTL_FORMAT_DESC format;
  adapter->QueryFormatDesc(pDesc->Format, &format);
  if (format.IsCompressed) {
    return E_FAIL;
  }
  if (format.PixelFormat == MTL::PixelFormatInvalid) {
    return E_FAIL;
  }
  Obj<MTL::TextureDescriptor> textureDescriptor;
  D3D11_TEXTURE2D_DESC finalDesc;
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
      bufferLen, MTL::ResourceOptionCPUCacheModeWriteCombined));
  if (pInitialData) {
    D3D11_ASSERT(pInitialData->SysMemPitch == bytesPerRow);
    memcpy(buffer->contents(), pInitialData->pSysMem, bufferLen);
  }
  *ppTexture =
      ref(new DynamicTexture2D(pDesc, std::move(buffer), pDevice, bytesPerRow));
  return S_OK;
}

} // namespace dxmt
