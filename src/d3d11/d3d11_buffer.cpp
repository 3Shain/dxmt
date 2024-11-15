#include "Metal/MTLBuffer.hpp"
#include "Metal/MTLPixelFormat.hpp"
#include "Metal/MTLTexture.hpp"
#include "com/com_pointer.hpp"
#include "d3d11_device.hpp"
#include "dxmt_binding.hpp"
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

class D3D11Buffer : public TResourceBase<tag_buffer, IMTLDynamicBuffer, IMTLBindable> {
private:
  std::unique_ptr<Buffer> buffer;
#ifdef DXMT_DEBUG
  std::string debug_name;
#endif
  bool structured;
  bool allow_raw_view;

  std::unique_ptr<BufferPool2> pool;
  SIMPLE_RESIDENCY_TRACKER tracker{};
  SIMPLE_OCCUPANCY_TRACKER occupancy{};

  using SRVBase = TResourceViewBase<tag_shader_resource_view<D3D11Buffer>, IMTLBindable>;

  class TBufferSRV : public SRVBase {
    SIMPLE_RESIDENCY_TRACKER tracker{};
    BufferViewInfo info;

  public:
    TBufferSRV(
        const tag_shader_resource_view<>::DESC1 *pDesc, D3D11Buffer *pResource, MTLD3D11Device *pDevice,
        BufferViewInfo const &info
    ) :
        SRVBase(pDesc, pResource, pDevice),
        info(info) {}

    ~TBufferSRV() {}

    BindingRef
    UseBindable(uint64_t seq_id) override {
      if (!seq_id)
        return BindingRef();
      resource->occupancy.MarkAsOccupied(seq_id);
      return BindingRef(
          static_cast<ID3D11View *>(this), resource->buffer->view(info.viewKey), info.byteWidth, info.byteOffset
      );
    }

    BufferAllocation *previous_allocation = nullptr;

    ArgumentData
    GetArgumentData(SIMPLE_RESIDENCY_TRACKER **ppTracker) override {
      if (*ppTracker != nullptr) {
        *ppTracker = &resource->tracker;
        return ArgumentData(resource->buffer->current()->gpuAddress + info.byteOffset, info.byteWidth);
      }
      *ppTracker = &tracker;
      if (previous_allocation != resource->buffer->current()) {
        previous_allocation = resource->buffer->current();
        tracker = {};
      }
      auto view = resource->buffer->view(info.viewKey);
      return ArgumentData(view->gpuResourceID(), info.viewElementWidth, info.viewElementOffset);
    }

    void
    GetLogicalResourceOrView(REFIID riid, void **ppLogicalResource) override {
      if (riid == __uuidof(IMTLDynamicBuffer)) {
        resource->QueryInterface(riid, ppLogicalResource);
        return;
      }
      QueryInterface(riid, ppLogicalResource);
    }
  };

  using UAVBase = TResourceViewBase<tag_unordered_access_view<D3D11Buffer>, IMTLBindable>;

  class UAVWithCounter : public UAVBase {
  private:
    SIMPLE_RESIDENCY_TRACKER tracker{};
    BufferViewInfo info;
    uint64_t counter_handle;

  public:
    UAVWithCounter(
        const tag_unordered_access_view<>::DESC1 *pDesc, D3D11Buffer *pResource, MTLD3D11Device *pDevice,
        BufferViewInfo const &info, uint64_t counter_handle
    ) :
        UAVBase(pDesc, pResource, pDevice),
        info(info),
        counter_handle(counter_handle) {}

    ~UAVWithCounter() {
      m_parent->GetDXMTDevice().queue().DiscardCounter(counter_handle);
    }

    BindingRef
    UseBindable(uint64_t seq_id) override {
      resource->occupancy.MarkAsOccupied(seq_id);
      return BindingRef(
          static_cast<ID3D11View *>(this), resource->buffer->view(info.viewKey), info.byteWidth, info.byteOffset
      );
    };

    BufferAllocation *previous_allocation = nullptr;

    ArgumentData
    GetArgumentData(SIMPLE_RESIDENCY_TRACKER **ppTracker) override {
      if (*ppTracker != nullptr) {
        *ppTracker = &resource->tracker;
        return ArgumentData(resource->buffer->current()->gpuAddress + info.byteOffset, info.byteWidth, counter_handle);
      }
      *ppTracker = &tracker;
      if (previous_allocation != resource->buffer->current()) {
        previous_allocation = resource->buffer->current();
        tracker = {};
      }
      auto view = resource->buffer->view(info.viewKey);
      return ArgumentData(view->gpuResourceID(), info.viewElementWidth, info.viewElementOffset);
    };

    void
    GetLogicalResourceOrView(REFIID riid, void **ppLogicalResource) override {
      QueryInterface(riid, ppLogicalResource);
    };

    virtual uint64_t
    SwapCounter(uint64_t handle) override {
      uint64_t current = counter_handle;
      if (~handle != 0) {
        counter_handle = handle;
        return current;
      }
      return handle;
    };
  };

public:
  D3D11Buffer(const tag_buffer::DESC1 *pDesc, const D3D11_SUBRESOURCE_DATA *pInitialData, MTLD3D11Device *device) :
      TResourceBase<tag_buffer, IMTLDynamicBuffer, IMTLBindable>(*pDesc, device) {
    buffer = std::make_unique<Buffer>(
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
    pool = std::make_unique<BufferPool2>(buffer.get(), flags);
    RotateBuffer(m_parent);
    if (pInitialData) {
      memcpy(buffer->current()->mappedMemory, pInitialData->pSysMem, pDesc->ByteWidth);
    }
    structured = pDesc->MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    allow_raw_view = pDesc->MiscFlags & D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
  }

  void *
  GetMappedMemory(UINT *pBytesPerRow, UINT *pBytesPerImage) override {
    *pBytesPerRow = desc.ByteWidth;
    *pBytesPerImage = desc.ByteWidth;
    assert(buffer->current()->mappedMemory);
    return buffer->current()->mappedMemory;
  };

  UINT
  GetSize(UINT *pBytesPerRow, UINT *pBytesPerImage) override {
    *pBytesPerRow = desc.ByteWidth;
    *pBytesPerImage = desc.ByteWidth;
    return desc.ByteWidth;
  };

  BindingRef
  GetCurrentBufferBinding() override {
    return BindingRef(static_cast<ID3D11Resource *>(this), buffer->current()->buffer(), desc.ByteWidth, 0);
  };

  D3D11_BIND_FLAG
  GetBindFlag() override {
    return (D3D11_BIND_FLAG)desc.BindFlags;
  }

  BindingRef
  UseBindable(uint64_t seq_id) override {
    if (!seq_id)
      return BindingRef();
    occupancy.MarkAsOccupied(seq_id);
    return BindingRef(static_cast<ID3D11Resource *>(this), buffer->current()->buffer(), desc.ByteWidth, 0);
  }

  ArgumentData
  GetArgumentData(SIMPLE_RESIDENCY_TRACKER **ppTracker) override {
    *ppTracker = &tracker;
    return ArgumentData(buffer->current()->gpuAddress, desc.ByteWidth);
  }

  bool
  GetContentionState(uint64_t finished_seq_id) override {
    return occupancy.IsOccupied(finished_seq_id);
  };

  HRESULT
  QueryInterface(REFIID riid, void **ppvObject) override {
    if (ppvObject == nullptr)
      return E_POINTER;

    if ((desc.BindFlags & (D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_STREAM_OUTPUT)) &&
        __uuidof(IMTLDynamicBuffer) == riid)
      return E_NOINTERFACE;

    return TResourceBase<tag_buffer, IMTLDynamicBuffer, IMTLBindable>::QueryInterface(riid, ppvObject);
  }

  void
  GetLogicalResourceOrView(REFIID riid, void **ppLogicalResource) override {
    QueryInterface(riid, ppLogicalResource);
  }

  void
  RotateBuffer(MTLD3D11Device *exch) override {
    tracker = {};
    occupancy = {};
    auto &queue = exch->GetDXMTDevice().queue();
    pool->Rename(queue.CurrentSeqId(), queue.CoherentSeqId());
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

    auto viewId = buffer->createView({.format = view_format});

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
    uint64_t counter_handle = DXMT_NO_COUNTER;
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
        counter_handle = m_parent->GetDXMTDevice().queue().AllocateCounter(0);
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

    auto viewId = buffer->createView({.format = view_format});

    auto srv = ref(new UAVWithCounter(
        &finalDesc, this, m_parent,
        {
            .viewKey = viewId,
            .viewElementOffset = viewElementOffset,
            .viewElementWidth = viewElementWidth,
            .byteOffset = offset,
            .byteWidth = size,
        },
        counter_handle
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
  *ppBuffer = ref(new D3D11Buffer(pDesc, pInitialData, pDevice));
  return S_OK;
}

} // namespace dxmt
