#include "d3d12_resource.hpp"
#include "d3d12_heap.hpp"
#include "d3d12_device.hpp"
#include "dxmt_device.hpp"
#include "log/log.hpp"
#include <cstring>

namespace dxmt::d3d12 {

// ── Helper: D3D12 heap type → Metal storage mode ──

static WMTResourceOptions MapHeapTypeToStorage(D3D12_HEAP_TYPE type) {
    switch (type) {
    case D3D12_HEAP_TYPE_DEFAULT:
        return WMTResourceStorageModePrivate;
    case D3D12_HEAP_TYPE_UPLOAD:
        return WMTResourceStorageModeShared;
    case D3D12_HEAP_TYPE_READBACK:
        return WMTResourceStorageModeShared;
    case D3D12_HEAP_TYPE_CUSTOM:
    default:
        return WMTResourceStorageModePrivate;
    }
}

static Flags<BufferAllocationFlag> MapStorageToBufferFlags(WMTResourceOptions storage) {
    Flags<BufferAllocationFlag> flags;
    if (storage == WMTResourceStorageModePrivate) {
        flags.set(BufferAllocationFlag::GpuPrivate);
    }
    return flags;
}

// ── Helper: resource desc → WMTTextureInfo ──

static WMTTextureInfo BuildTextureInfo(const D3D12_RESOURCE_DESC &desc) {
    WMTTextureInfo info = {};
    info.width = desc.Width;
    info.height = desc.Height;
    info.mipmap_level_count = desc.MipLevels;
    info.sample_count = desc.SampleDesc.Count;

    switch (desc.Dimension) {
    case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
        info.type = desc.DepthOrArraySize > 1
            ? WMTTextureType1DArray : WMTTextureType1D;
        info.depth = 1;
        info.array_length = desc.DepthOrArraySize;
        break;
    case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
        info.type = desc.DepthOrArraySize > 1
            ? WMTTextureType2DArray : WMTTextureType2D;
        info.depth = 1;
        info.array_length = desc.DepthOrArraySize;
        break;
    case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
        info.type = WMTTextureType3D;
        info.depth = desc.DepthOrArraySize;
        info.array_length = 1;
        break;
    default:
        break;
    }

    info.pixel_format = WMTPixelFormat(desc.Format);

    // Determine usage from flags
    WMTTextureUsage usage = WMTTextureUsageShaderRead;
    if (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
        usage = WMTTextureUsage(WMTTextureUsageRenderTarget | usage);
    if (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
        usage = WMTTextureUsageShaderRead; // depth/stencil handled separately
    if (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
        usage = WMTTextureUsage(WMTTextureUsageShaderWrite | usage);
    info.usage = usage;

    return info;
}

// ──────────────────────────────────────────────

D3D12Resource::D3D12Resource(D3D12Device *device,
                              const D3D12_RESOURCE_DESC &desc)
    : device_(device)
    , desc_(desc)
    , current_state_(D3D12_RESOURCE_STATE_COMMON)
    , is_buffer_(desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
    , refcount_(1) {
}

D3D12Resource::~D3D12Resource() {
    TRACE("D3D12Resource destroyed (",
          is_buffer_ ? "buffer" : "texture", ")");
}

// ── Factory: committed resource ──

HRESULT D3D12Resource::CreateCommitted(
    D3D12Device *device,
    const D3D12_HEAP_PROPERTIES *pHeapProperties,
    D3D12_HEAP_FLAGS HeapFlags,
    const D3D12_RESOURCE_DESC *pDesc,
    D3D12_RESOURCE_STATES InitialState,
    const D3D12_CLEAR_VALUE *pOptimizedClearValue,
    D3D12Resource **out) {

    if (!device || !pHeapProperties || !pDesc || !out)
        return E_INVALIDARG;

    *out = nullptr;

    auto *resource = new D3D12Resource(device, *pDesc);
    resource->heap_properties_ = *pHeapProperties;
    resource->heap_flags_ = HeapFlags;
    resource->current_state_ = InitialState;

    WMT::Device mtl_device = device->GetMTLDevice();
    WMTResourceOptions storage = MapHeapTypeToStorage(pHeapProperties->Type);

    try {
        if (pDesc->Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
            // ── Buffer ──
            resource->buffer_ = Rc<Buffer>(new Buffer(pDesc->Width, mtl_device));
            auto flags = MapStorageToBufferFlags(storage);
            resource->allocation_ = resource->buffer_->allocate(flags);
            TRACE("Committed buffer: ", pDesc->Width, " bytes, storage=", storage);
        } else {
            // ── Texture ──
            WMTTextureInfo info = BuildTextureInfo(*pDesc);
            info.storage_mode = storage;
            resource->texture_ = Rc<Texture>(new Texture(info, mtl_device));
            Flags<TextureAllocationFlag> tex_flags;
            if (storage == WMTResourceStorageModePrivate)
                tex_flags.set(TextureAllocationFlag::GpuPrivate);
            else
                tex_flags.set(TextureAllocationFlag::Shared);
            resource->allocation_ = resource->texture_->allocate(tex_flags);
            TRACE("Committed texture: ", pDesc->Width, "x", pDesc->Height,
                  " mips=", pDesc->MipLevels, " storage=", storage);
        }
    } catch (const std::exception &e) {
        ERR("CreateCommittedResource failed: ", e.what());
        delete resource;
        return E_FAIL;
    }

    *out = resource;
    return S_OK;
}

// ── Factory: placed resource ──

HRESULT D3D12Resource::CreatePlaced(
    D3D12Device *device,
    D3D12Heap *heap,
    UINT64 HeapOffset,
    const D3D12_RESOURCE_DESC *pDesc,
    D3D12_RESOURCE_STATES InitialState,
    D3D12Resource **out) {

    if (!device || !heap || !pDesc || !out)
        return E_INVALIDARG;
    if (pDesc->Dimension != D3D12_RESOURCE_DIMENSION_BUFFER) {
        WARN("CreatePlacedResource: only buffers supported for placed resources");
        return E_NOTIMPL;
    }

    *out = nullptr;

    auto *resource = new D3D12Resource(device, *pDesc);
    resource->heap_properties_ = heap->GetProperties();
    resource->current_state_ = InitialState;

    try {
        resource->buffer_ = Rc<Buffer>(new Buffer(pDesc->Width, device->GetMTLDevice()));
        // Sub-allocate from the heap's backing buffer
        Rc<BufferAllocation> sub_alloc;
        HRESULT hr = heap->Suballocate(pDesc->Width, pDesc->Alignment,
                                        &sub_alloc, nullptr);
        if (FAILED(hr)) {
            delete resource;
            return hr;
        }
        resource->allocation_ = std::move(sub_alloc);
        resource->buffer_->rename(Rc<BufferAllocation>(resource->allocation_));
        TRACE("Placed buffer: ", pDesc->Width, " bytes at heap offset ", HeapOffset);
    } catch (const std::exception &e) {
        ERR("CreatePlacedResource failed: ", e.what());
        delete resource;
        return E_FAIL;
    }

    *out = resource;
    return S_OK;
}

// ── IUnknown ──

HRESULT STDMETHODCALLTYPE D3D12Resource::QueryInterface(REFIID riid, void **ppvObject) {
    if (!ppvObject) return E_POINTER;
    *ppvObject = nullptr;
    if (riid == IID_IUnknown || riid == IID_ID3D12Object || riid == IID_ID3D12Resource) {
        *ppvObject = static_cast<ID3D12Resource *>(this);
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE D3D12Resource::AddRef() {
    return refcount_.fetch_add(1, std::memory_order_relaxed) + 1;
}

ULONG STDMETHODCALLTYPE D3D12Resource::Release() {
    ULONG c = refcount_.fetch_sub(1, std::memory_order_acq_rel) - 1;
    if (c == 0) delete this;
    return c;
}

HRESULT STDMETHODCALLTYPE D3D12Resource::SetName(LPCWSTR Name) {
    // Optional: set debug label on MTLResource
    return S_OK;
}

// ── ID3D12Resource ──

HRESULT STDMETHODCALLTYPE D3D12Resource::Map(
    UINT Subresource, const D3D12_RANGE *pReadRange, void **ppData) {
    if (!ppData) return E_INVALIDARG;
    *ppData = nullptr;

    if (mapped_) {
        WARN("Map: resource already mapped");
        return E_FAIL;
    }

    if (heap_properties_.Type != D3D12_HEAP_TYPE_UPLOAD &&
        heap_properties_.Type != D3D12_HEAP_TYPE_READBACK) {
        // For upload: CPU-writable, for readback: CPU-readable
        // For default: not mappable
        if (heap_properties_.Type == D3D12_HEAP_TYPE_DEFAULT) {
            WARN("Map: DEFAULT heap resources are not CPU-mappable");
            return E_INVALIDARG;
        }
    }

    if (is_buffer_ && allocation_) {
        *ppData = allocation_->mappedMemory(0);
        mapped_ = true;
        mapped_subresource_ = Subresource;
        mapped_data_ = *ppData;
        TRACE("Map: buffer mapped at ", *ppData);
        return S_OK;
    }

    WARN("Map: not supported for this resource type");
    return E_NOTIMPL;
}

void STDMETHODCALLTYPE D3D12Resource::Unmap(
    UINT Subresource, const D3D12_RANGE *pWrittenRange) {
    if (!mapped_) {
        WARN("Unmap: resource not mapped");
        return;
    }

    // For upload resources with WriteCombine, flush if needed
    if (heap_properties_.Type == D3D12_HEAP_TYPE_UPLOAD &&
        is_buffer_ && allocation_) {
        // Metal manages coherency for Shared storage
    }

    mapped_ = false;
    mapped_data_ = nullptr;
    TRACE("Unmap: buffer unmapped");
}

D3D12_RESOURCE_DESC STDMETHODCALLTYPE D3D12Resource::GetDesc() {
    return desc_;
}

D3D12_GPU_VIRTUAL_ADDRESS STDMETHODCALLTYPE D3D12Resource::GetGPUVirtualAddress() {
    if (is_buffer_ && allocation_) {
        return allocation_->gpuAddress();
    }
    return 0;
}

HRESULT STDMETHODCALLTYPE D3D12Resource::WriteToSubresource(
    UINT DstSubresource, const D3D12_BOX *pDstBox,
    const void *pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch) {
    if (!pSrcData) return E_INVALIDARG;

    if (is_buffer_ && allocation_) {
        UINT64 offset = pDstBox ? pDstBox->left : 0;
        UINT64 size = desc_.Width;
        if (pDstBox) size = pDstBox->right - pDstBox->left;
        void *dst = allocation_->mappedMemory(0);
        memcpy(static_cast<char *>(dst) + offset, pSrcData, size);
        TRACE("WriteToSubresource: ", size, " bytes to buffer");
        return S_OK;
    }

    // Texture upload via replaceRegion (shared-storage only)
    if (!is_buffer_ && texture_ && allocation_) {
        WMT::Texture mtl_tex = allocation_->texture();
        if (mtl_tex == nullptr) {
            WARN("WriteToSubresource: texture allocation has no MTLTexture");
            return E_FAIL;
        }

        // Compute subresource layout
        UINT mip = DstSubresource % desc_.MipLevels;
        UINT slice = DstSubresource / desc_.MipLevels;
        UINT row_pitch = SrcRowPitch ? SrcRowPitch
            : (UINT)(desc_.Width * 4); // assume 4 bytes/pixel
        UINT depth_pitch = SrcDepthPitch ? SrcDepthPitch
            : (UINT)(row_pitch * desc_.Height);

        WMTOrigin origin = {};
        WMTSize size = {};
        if (pDstBox) {
            origin.x = pDstBox->left;
            origin.y = pDstBox->top;
            origin.z = pDstBox->front;
            size.width = pDstBox->right - pDstBox->left;
            size.height = pDstBox->bottom - pDstBox->top;
            size.depth = pDstBox->back - pDstBox->front;
        } else {
            size.width = std::max(desc_.Width >> mip, 1ULL);
            size.height = std::max((UINT64)desc_.Height >> mip, 1ULL);
            size.depth = desc_.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D
                ? std::max((UINT64)desc_.DepthOrArraySize >> mip, 1ULL) : 1;
        }

        mtl_tex.replaceRegion(origin, size, mip, slice, pSrcData,
                               row_pitch, depth_pitch);
        TRACE("WriteToSubresource: texture mip=", mip, " slice=", slice,
              " size=", size.width, "x", size.height);
        return S_OK;
    }

    WARN("WriteToSubresource: no valid allocation");
    return E_FAIL;
}

HRESULT STDMETHODCALLTYPE D3D12Resource::ReadFromSubresource(
    void *pDstData, UINT DstRowPitch, UINT DstDepthPitch,
    UINT SrcSubresource, const D3D12_BOX *pSrcBox) {
    if (!pDstData) return E_INVALIDARG;

    if (is_buffer_ && allocation_) {
        UINT64 offset = pSrcBox ? pSrcBox->left : 0;
        UINT64 size = desc_.Width;
        if (pSrcBox) size = pSrcBox->right - pSrcBox->left;
        void *src = allocation_->mappedMemory(0);
        memcpy(pDstData, static_cast<char *>(src) + offset, size);
        TRACE("ReadFromSubresource: ", size, " bytes from buffer");
        return S_OK;
    }

    // Texture readback via MTLTexture::getBytes (shared-storage only)
    if (!is_buffer_ && texture_ && allocation_) {
        WMT::Texture mtl_tex = allocation_->texture();
        if (mtl_tex == nullptr) {
            WARN("ReadFromSubresource: texture allocation has no MTLTexture");
            return E_FAIL;
        }

        UINT mip = SrcSubresource % desc_.MipLevels;
        UINT slice = SrcSubresource / desc_.MipLevels;

        WMTOrigin origin = {};
        WMTSize size = {};
        if (pSrcBox) {
            origin.x = pSrcBox->left; origin.y = pSrcBox->top; origin.z = pSrcBox->front;
            size.width = pSrcBox->right - pSrcBox->left;
            size.height = pSrcBox->bottom - pSrcBox->top;
            size.depth = pSrcBox->back - pSrcBox->front;
        } else {
            size.width = std::max(desc_.Width >> mip, 1ULL);
            size.height = std::max((UINT64)desc_.Height >> mip, 1ULL);
            size.depth = desc_.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D
                ? std::max((UINT64)desc_.DepthOrArraySize >> mip, 1ULL) : 1;
        }

        // getBytes is available on shared-storage textures
        mtl_tex.getBytes(pDstData, DstRowPitch, DstDepthPitch,
                          origin, size, mip, slice);
        TRACE("ReadFromSubresource: texture mip=", mip, " slice=", slice);
        return S_OK;
    }

    WARN("ReadFromSubresource: no valid allocation");
    return E_FAIL;
}

HRESULT STDMETHODCALLTYPE D3D12Resource::GetHeapProperties(
    D3D12_HEAP_PROPERTIES *pHeapProperties, D3D12_HEAP_FLAGS *pHeapFlags) {
    if (pHeapProperties) *pHeapProperties = heap_properties_;
    if (pHeapFlags) *pHeapFlags = heap_flags_;
    return S_OK;
}

} // namespace dxmt::d3d12
