#include "Metal/MTLPixelFormat.hpp"
#include "com/com_pointer.hpp"
#include "d3d11_device.hpp"
#include "dxmt_buffer.hpp"
#include "dxmt_buffer_pool.hpp"
#include "dxmt_format.hpp"
#include "mtld11_resource.hpp"

namespace dxmt {

struct BufferViewInfo {
  BufferViewKey viewKey;
  uint32_t viewElementOffset;
  uint32_t viewElementWidth;
  uint32_t byteOffset;
  uint32_t byteWidth;
};

class D3D11Buffer : public TResourceBase<tag_buffer, IMTLDynamicBuffer> {
private:
  Rc<Buffer> buffer_;
  Rc<BufferAllocation> allocation;
#ifdef DXMT_DEBUG
  std::string debug_name;
#endif
  bool structured;
  bool allow_raw_view;

  std::unique_ptr<BufferPool2> pool;

  using SRVBase = TResourceViewBase<tag_shader_resource_view<D3D11Buffer>>;

  class TBufferSRV : public SRVBase {
    BufferViewInfo info;

  public:
    TBufferSRV(
        const tag_shader_resource_view<>::DESC1 *pDesc, D3D11Buffer *pResource, MTLD3D11Device *pDevice,
        BufferViewInfo const &info
    ) :
        SRVBase(pDesc, pResource, pDevice),
        info(info) {}

    ~TBufferSRV() {}

    Rc<Buffer> buffer() final { return resource->buffer_; };
    Rc<Texture> texture() final { return {}; };
    unsigned viewId() final { return info.viewKey;};
    BufferSlice bufferSlice() final { return {info.byteOffset, info.byteWidth, info.viewElementOffset, info.viewElementWidth };}
  };

  using UAVBase = TResourceViewBase<tag_unordered_access_view<D3D11Buffer>>;

  class UAVWithCounter : public UAVBase {
  private:
    BufferViewInfo info;
    Rc<Buffer> counter_;

  public:
    UAVWithCounter(
        const tag_unordered_access_view<>::DESC1 *pDesc, D3D11Buffer *pResource, MTLD3D11Device *pDevice,
        BufferViewInfo const &info, Rc<Buffer>&& counter
    ) :
        UAVBase(pDesc, pResource, pDevice),
        info(info),
        counter_(std::move(counter)) {}

    Rc<Buffer> buffer() final { return resource->buffer_; };
    Rc<Texture> texture() final { return {}; };
    unsigned viewId() final { return info.viewKey;};
    BufferSlice bufferSlice() final { return {info.byteOffset, info.byteWidth, info.viewElementOffset, info.viewElementWidth };}
    Rc<Buffer> counter() final { return counter_; };
  };

public:
  D3D11Buffer(const tag_buffer::DESC1 *pDesc, const D3D11_SUBRESOURCE_DATA *pInitialData, MTLD3D11Device *device) :
      TResourceBase<tag_buffer, IMTLDynamicBuffer>(*pDesc, device) {
    buffer_ = new Buffer(
        pDesc->BindFlags & D3D11_BIND_UNORDERED_ACCESS ? pDesc->ByteWidth + 16 : pDesc->ByteWidth,
        device->GetMTLDevice()
    );
    Flags<BufferAllocationFlag> flags;
    if (!m_parent->IsTraced() && pDesc->Usage == D3D11_USAGE_DYNAMIC)
      flags.set(BufferAllocationFlag::CpuWriteCombined);
    // if (pDesc->Usage != D3D11_USAGE_DEFAULT)
    if (pDesc->Usage == D3D11_USAGE_IMMUTABLE)
      flags.set(BufferAllocationFlag::GpuReadonly);
    if (pDesc->Usage != D3D11_USAGE_DYNAMIC)
      flags.set(BufferAllocationFlag::GpuManaged);
    pool = std::make_unique<BufferPool2>(buffer_.ptr(), flags);
    RotateBuffer(m_parent);
    if (pInitialData) {
      memcpy(allocation->mappedMemory, pInitialData->pSysMem, pDesc->ByteWidth);
    }
    auto _ = buffer_->rename(Rc(allocation));
    D3D11_ASSERT(_.ptr() == nullptr);
    structured = pDesc->MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    allow_raw_view = pDesc->MiscFlags & D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
  }

  void *
  GetMappedMemory(UINT *pBytesPerRow, UINT *pBytesPerImage) override {
    *pBytesPerRow = desc.ByteWidth;
    *pBytesPerImage = desc.ByteWidth;
    assert(allocation->mappedMemory);
    return allocation->mappedMemory;
  };

  UINT
  GetSize(UINT *pBytesPerRow, UINT *pBytesPerImage) override {
    *pBytesPerRow = desc.ByteWidth;
    *pBytesPerImage = desc.ByteWidth;
    return desc.ByteWidth;
  };

  D3D11_BIND_FLAG
  GetBindFlag() override {
    return (D3D11_BIND_FLAG)desc.BindFlags;
  }

  HRESULT
  QueryInterface(REFIID riid, void **ppvObject) override {
    if (ppvObject == nullptr)
      return E_POINTER;

    if ((desc.BindFlags & (D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_STREAM_OUTPUT)) &&
        __uuidof(IMTLDynamicBuffer) == riid)
      return E_NOINTERFACE;

    return TResourceBase<tag_buffer, IMTLDynamicBuffer>::QueryInterface(riid, ppvObject);
  }

  dxmt::Rc<dxmt::Buffer> __buffer_dyn() final { return buffer_; };
  dxmt::Rc<dxmt::Texture> __texture_dyn()final { return {}; };
  bool __isBuffer_dyn() final { return true; };
  dxmt::Rc<dxmt::BufferAllocation> __bufferAllocated() final { return allocation; };
  dxmt::Rc<dxmt::TextureAllocation> __textureAllocated() final { return {}; };

  Rc<Buffer>
  buffer() final {
    return buffer_;
  };
  Rc<Texture>
  texture() final {
    return {};
  };
  BufferSlice
  bufferSlice() final {
    return {0, desc.ByteWidth, 0, 0};
  }
  Com<IMTLDynamicBuffer>
  dynamic() final {
    return (desc.BindFlags & (D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_STREAM_OUTPUT)) ? nullptr : this;
  }
  Com<IMTLD3D11Staging>
  staging() final {
    return nullptr;
  }

  void
  RotateBuffer(MTLD3D11Device *exch) override {
    auto &queue = exch->GetDXMTDevice().queue();
    if(allocation.ptr()) {
      pool->discard(std::move(allocation), queue.CurrentSeqId());
    }
    allocation = pool->allocate(queue.CoherentSeqId());
#ifdef DXMT_DEBUG
    {
      auto pool = transfer(NS::AutoreleasePool::alloc()->init());
      buffer_dynamic->setLabel(NS::String::string(debug_name.c_str(), NS::ASCIIStringEncoding));
    }
#endif
  }

  HRESULT
  CreateShaderResourceView(const D3D11_SHADER_RESOURCE_VIEW_DESC1 *pDesc, ID3D11ShaderResourceView1 **ppView) override {
    D3D11_SHADER_RESOURCE_VIEW_DESC1 finalDesc;
    if (FAILED(ExtractEntireResourceViewDescription(&this->desc, pDesc, &finalDesc))) {
      return E_INVALIDARG;
    }
    if (finalDesc.ViewDimension != D3D11_SRV_DIMENSION_BUFFER &&
        finalDesc.ViewDimension != D3D11_SRV_DIMENSION_BUFFEREX) {
      return E_FAIL;
    }
    MTL::PixelFormat view_format = MTL::PixelFormatInvalid;
    uint32_t offset, size, viewElementOffset, viewElementWidth;
    if (structured) {
      if (finalDesc.Format != DXGI_FORMAT_UNKNOWN) {
        return E_INVALIDARG;
      }
      // StructuredBuffer
      CalculateBufferViewOffsetAndSize(
          this->desc, desc.StructureByteStride, finalDesc.Buffer.FirstElement, finalDesc.Buffer.NumElements, offset,
          size
      );
      view_format = MTL::PixelFormatR32Uint;
      viewElementOffset = finalDesc.Buffer.FirstElement * (desc.StructureByteStride >> 2);
      viewElementWidth = finalDesc.Buffer.NumElements * (desc.StructureByteStride >> 2);
    } else if (finalDesc.ViewDimension == D3D11_SRV_DIMENSION_BUFFEREX && finalDesc.BufferEx.Flags & D3D11_BUFFEREX_SRV_FLAG_RAW) {
      if (!allow_raw_view)
        return E_INVALIDARG;
      if (finalDesc.Format != DXGI_FORMAT_R32_TYPELESS)
        return E_INVALIDARG;
      CalculateBufferViewOffsetAndSize(
          this->desc, sizeof(uint32_t), finalDesc.Buffer.FirstElement, finalDesc.Buffer.NumElements, offset, size
      );
      view_format = MTL::PixelFormatR32Uint;
      viewElementOffset = finalDesc.Buffer.FirstElement;
      viewElementWidth = finalDesc.Buffer.NumElements;
    } else {
      MTL_DXGI_FORMAT_DESC format;
      if (FAILED(MTLQueryDXGIFormat(m_parent->GetMTLDevice(), finalDesc.Format, format))) {
        return E_FAIL;
      }

      view_format = format.PixelFormat;
      offset = finalDesc.Buffer.FirstElement * format.BytesPerTexel;
      size = finalDesc.Buffer.NumElements * format.BytesPerTexel;
      viewElementOffset = finalDesc.Buffer.FirstElement;
      viewElementWidth = finalDesc.Buffer.NumElements;
    }

    if (!ppView) {
      return S_FALSE;
    }

    auto viewId = buffer_->createView({.format = view_format});

    auto srv = ref(new TBufferSRV(
        &finalDesc, this, m_parent,
        {
            .viewKey = viewId,
            .viewElementOffset = viewElementOffset,
            .viewElementWidth = viewElementWidth,
            .byteOffset = offset,
            .byteWidth = size,
        }
    ));
    *ppView = srv;
    return S_OK;
  };

  HRESULT
  CreateUnorderedAccessView(const D3D11_UNORDERED_ACCESS_VIEW_DESC1 *pDesc, ID3D11UnorderedAccessView1 **ppView)
      override {
    D3D11_UNORDERED_ACCESS_VIEW_DESC1 finalDesc;
    if (FAILED(ExtractEntireResourceViewDescription(&this->desc, pDesc, &finalDesc))) {
      return E_INVALIDARG;
    }
    if (finalDesc.ViewDimension != D3D11_UAV_DIMENSION_BUFFER) {
      return E_FAIL;
    }
    MTL::PixelFormat view_format = MTL::PixelFormatInvalid;
    uint32_t offset, size, viewElementOffset, viewElementWidth;
    Rc<Buffer> counter = {};
    if (structured) {
      if (finalDesc.Format != DXGI_FORMAT_UNKNOWN) {
        return E_INVALIDARG;
      }
      // StructuredBuffer
      CalculateBufferViewOffsetAndSize(
          this->desc, desc.StructureByteStride, finalDesc.Buffer.FirstElement, finalDesc.Buffer.NumElements, offset,
          size
      );
      view_format = MTL::PixelFormatR32Uint;
      // when structured buffer is interpreted as typed buffer for any reason
      viewElementOffset = finalDesc.Buffer.FirstElement * (desc.StructureByteStride >> 2);
      viewElementWidth = finalDesc.Buffer.NumElements * (desc.StructureByteStride >> 2);
      if (finalDesc.Buffer.Flags & (D3D11_BUFFER_UAV_FLAG_APPEND | D3D11_BUFFER_UAV_FLAG_COUNTER)) {
        counter = new dxmt::Buffer(sizeof(uint32_t), m_parent->GetMTLDevice());
        counter->rename(counter->allocate(BufferAllocationFlag::GpuManaged));
      }
    } else if (finalDesc.Buffer.Flags & D3D11_BUFFER_UAV_FLAG_RAW) {
      if (!allow_raw_view)
        return E_INVALIDARG;
      if (finalDesc.Format != DXGI_FORMAT_R32_TYPELESS)
        return E_INVALIDARG;
      CalculateBufferViewOffsetAndSize(
          this->desc, sizeof(uint32_t), finalDesc.Buffer.FirstElement, finalDesc.Buffer.NumElements, offset, size
      );
      view_format = MTL::PixelFormatR32Uint;
      viewElementOffset = finalDesc.Buffer.FirstElement;
      viewElementWidth = finalDesc.Buffer.NumElements;
    } else {
      MTL_DXGI_FORMAT_DESC format;
      if (FAILED(MTLQueryDXGIFormat(m_parent->GetMTLDevice(), finalDesc.Format, format))) {
        return E_FAIL;
      }

      view_format = format.PixelFormat;
      offset = finalDesc.Buffer.FirstElement * format.BytesPerTexel;
      size = finalDesc.Buffer.NumElements * format.BytesPerTexel;
      viewElementOffset = finalDesc.Buffer.FirstElement;
      viewElementWidth = finalDesc.Buffer.NumElements;
    }

    if (!ppView) {
      return S_FALSE;
    }

    auto viewId = buffer_->createView({.format = view_format});

    auto srv = ref(new UAVWithCounter(
        &finalDesc, this, m_parent,
        {
            .viewKey = viewId,
            .viewElementOffset = viewElementOffset,
            .viewElementWidth = viewElementWidth,
            .byteOffset = offset,
            .byteWidth = size,
        },
        std::move(counter)
    ));
    *ppView = srv;
    return S_OK;
  };

  void
  OnSetDebugObjectName(LPCSTR Name) override {
    if (!Name) {
      return;
    }
#ifdef DXMT_DEBUG
    debug_name = std::string(Name);
#endif
  }
};

HRESULT
CreateBuffer(
    MTLD3D11Device *pDevice, const D3D11_BUFFER_DESC *pDesc, const D3D11_SUBRESOURCE_DATA *pInitialData,
    ID3D11Buffer **ppBuffer
) {
  *ppBuffer = reinterpret_cast<ID3D11Buffer *>(ref(new D3D11Buffer(pDesc, pInitialData, pDevice)));
  return S_OK;
}

} // namespace dxmt
