#pragma once

#include "windows.h"
#include "unknwn.h"
#include <cstdint>

// ═══════════════════════════════════════════════════════════════
// Minimal D3D12 type definitions for M1 device creation
//
// These will be replaced by include/native/directx/d3d12.h
// from the mingw-directx-headers submodule once initialized.
// ═══════════════════════════════════════════════════════════════

// ── Forward declarations ──

#ifndef __ID3D12Device_FWD_DEFINED__
#define __ID3D12Device_FWD_DEFINED__
typedef interface ID3D12Device ID3D12Device;
#endif

#ifndef __ID3D12CommandQueue_FWD_DEFINED__
#define __ID3D12CommandQueue_FWD_DEFINED__
typedef interface ID3D12CommandQueue ID3D12CommandQueue;
#endif

#ifndef __ID3D12CommandAllocator_FWD_DEFINED__
#define __ID3D12CommandAllocator_FWD_DEFINED__
typedef interface ID3D12CommandAllocator ID3D12CommandAllocator;
#endif

#ifndef __ID3D12CommandList_FWD_DEFINED__
#define __ID3D12CommandList_FWD_DEFINED__
typedef interface ID3D12CommandList ID3D12CommandList;
#endif

#ifndef __ID3D12GraphicsCommandList_FWD_DEFINED__
#define __ID3D12GraphicsCommandList_FWD_DEFINED__
typedef interface ID3D12GraphicsCommandList ID3D12GraphicsCommandList;
#endif

#ifndef __ID3D12Resource_FWD_DEFINED__
#define __ID3D12Resource_FWD_DEFINED__
typedef interface ID3D12Resource ID3D12Resource;
#endif

#ifndef __ID3D12Heap_FWD_DEFINED__
#define __ID3D12Heap_FWD_DEFINED__
typedef interface ID3D12Heap ID3D12Heap;
#endif

#ifndef __ID3D12DescriptorHeap_FWD_DEFINED__
#define __ID3D12DescriptorHeap_FWD_DEFINED__
typedef interface ID3D12DescriptorHeap ID3D12DescriptorHeap;
#endif

#ifndef __ID3D12RootSignature_FWD_DEFINED__
#define __ID3D12RootSignature_FWD_DEFINED__
typedef interface ID3D12RootSignature ID3D12RootSignature;
#endif

#ifndef __ID3D12PipelineState_FWD_DEFINED__
#define __ID3D12PipelineState_FWD_DEFINED__
typedef interface ID3D12PipelineState ID3D12PipelineState;
#endif

#ifndef __ID3D12Fence_FWD_DEFINED__
#define __ID3D12Fence_FWD_DEFINED__
typedef interface ID3D12Fence ID3D12Fence;
#endif

// ── D3D12 enums ──

enum D3D12_FEATURE_LEVEL {
    D3D_FEATURE_LEVEL_12_0 = 0xb000,
    D3D_FEATURE_LEVEL_12_1 = 0xb100,
    D3D_FEATURE_LEVEL_12_2 = 0xb200,
};

enum D3D12_COMMAND_LIST_TYPE {
    D3D12_COMMAND_LIST_TYPE_DIRECT   = 0,
    D3D12_COMMAND_LIST_TYPE_BUNDLE   = 1,
    D3D12_COMMAND_LIST_TYPE_COMPUTE  = 2,
    D3D12_COMMAND_LIST_TYPE_COPY     = 3,
};

enum D3D12_COMMAND_QUEUE_FLAGS {
    D3D12_COMMAND_QUEUE_FLAG_NONE                = 0,
    D3D12_COMMAND_QUEUE_FLAG_DISABLE_GPU_TIMEOUT = 0x1,
};

enum D3D12_COMMAND_QUEUE_PRIORITY {
    D3D12_COMMAND_QUEUE_PRIORITY_NORMAL          = 0,
    D3D12_COMMAND_QUEUE_PRIORITY_HIGH            = 100,
    D3D12_COMMAND_QUEUE_PRIORITY_GLOBAL_REALTIME = 10000,
};

enum D3D12_DESCRIPTOR_HEAP_TYPE {
    D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV = 0,
    D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER     = 1,
    D3D12_DESCRIPTOR_HEAP_TYPE_RTV         = 2,
    D3D12_DESCRIPTOR_HEAP_TYPE_DSV         = 3,
    D3D12_NUM_DESCRIPTOR_HEAP_TYPES        = 4,
};

enum D3D12_DESCRIPTOR_HEAP_FLAGS {
    D3D12_DESCRIPTOR_HEAP_FLAG_NONE           = 0,
    D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE = 0x1,
};

enum D3D12_HEAP_FLAGS {
    D3D12_HEAP_FLAG_NONE                       = 0,
    D3D12_HEAP_FLAG_SHARED                     = 0x1,
    D3D12_HEAP_FLAG_DENY_BUFFERS               = 0x4,
    D3D12_HEAP_FLAG_ALLOW_DISPLAY              = 0x8,
    D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER       = 0x20,
    D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES        = 0x40,
    D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES    = 0x80,
    D3D12_HEAP_FLAG_HARDWARE_PROTECTED         = 0x100,
    D3D12_HEAP_FLAG_ALLOW_WRITE_WATCH          = 0x200,
    D3D12_HEAP_FLAG_ALLOW_SHADER_ATOMICS       = 0x400,
    D3D12_HEAP_FLAG_CREATE_NOT_RESIDENT        = 0x800,
    D3D12_HEAP_FLAG_CREATE_NOT_ZEROED          = 0x1000,
    D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES = 0,
    D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS         = 0xc0,
    D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES = 0x44,
    D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES  = 0x84,
};

enum D3D12_RESOURCE_STATES {
    D3D12_RESOURCE_STATE_COMMON                     = 0,
    D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER = 0x1,
    D3D12_RESOURCE_STATE_INDEX_BUFFER               = 0x2,
    D3D12_RESOURCE_STATE_RENDER_TARGET              = 0x4,
    D3D12_RESOURCE_STATE_UNORDERED_ACCESS           = 0x8,
    D3D12_RESOURCE_STATE_DEPTH_WRITE                = 0x10,
    D3D12_RESOURCE_STATE_DEPTH_READ                 = 0x20,
    D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE  = 0x40,
    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE      = 0x80,
    D3D12_RESOURCE_STATE_STREAM_OUT                 = 0x100,
    D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT          = 0x200,
    D3D12_RESOURCE_STATE_COPY_SOURCE                = 0x400,
    D3D12_RESOURCE_STATE_COPY_DEST                  = 0x800,
    D3D12_RESOURCE_STATE_RESOLVE_SOURCE             = 0x1000,
    D3D12_RESOURCE_STATE_RESOLVE_DEST               = 0x2000,
    D3D12_RESOURCE_STATE_GENERIC_READ               = 0xAC3,
    D3D12_RESOURCE_STATE_PRESENT                    = 0,
};

enum D3D12_FENCE_FLAGS {
    D3D12_FENCE_FLAG_NONE                 = 0,
    D3D12_FENCE_FLAG_SHARED               = 0x1,
    D3D12_FENCE_FLAG_SHARED_CROSS_ADAPTER  = 0x2,
    D3D12_FENCE_FLAG_NON_MONITORED         = 0x4,
};

enum D3D12_TILE_MAPPING_FLAGS {
    D3D12_TILE_MAPPING_FLAG_NONE      = 0,
    D3D12_TILE_MAPPING_FLAG_NO_HAZARD = 0x1,
};

// ── Structs ──

typedef struct D3D12_COMMAND_QUEUE_DESC {
    D3D12_COMMAND_LIST_TYPE Type;
    INT Priority;
    D3D12_COMMAND_QUEUE_FLAGS Flags;
    UINT NodeMask;
} D3D12_COMMAND_QUEUE_DESC;

typedef struct _LUID {
    DWORD LowPart;
    LONG  HighPart;
} LUID, *PLUID;

// ── IIDs ──

static const GUID IID_ID3D12Object = {
    0xc4fec28f, 0x7966, 0x4e95, {0x9f, 0x94, 0xf4, 0x31, 0xcb, 0x56, 0xc3, 0xb8}};

static const GUID IID_ID3D12Device = {
    0x189819f1, 0x1db6, 0x4b57, {0xbe, 0x54, 0x18, 0x21, 0x33, 0x9b, 0x85, 0xf7}};

static const GUID IID_ID3D12CommandQueue = {
    0x0ec870a6, 0x5d7e, 0x4c22, {0x8c, 0xfc, 0x5b, 0xaa, 0xe0, 0x76, 0x16, 0xed}};

// ── ID3D12Object (base interface, inherits IUnknown) ──

DECLARE_INTERFACE_(ID3D12Object, IUnknown) {
    STDMETHOD(GetPrivateData)(THIS_ REFGUID guid, UINT *pDataSize, void *pData) PURE;
    STDMETHOD(SetPrivateData)(THIS_ REFGUID guid, UINT DataSize, const void *pData) PURE;
    STDMETHOD(SetPrivateDataInterface)(THIS_ REFGUID guid, const IUnknown *pData) PURE;
    STDMETHOD(SetName)(THIS_ LPCWSTR Name) PURE;
};

// ── ID3D12Device (inherits ID3D12Object) ──

DECLARE_INTERFACE_(ID3D12Device, ID3D12Object) {
    STDMETHOD_(UINT, GetNodeCount)(THIS) PURE;
    STDMETHOD(CreateCommandQueue)(THIS_
        const D3D12_COMMAND_QUEUE_DESC *pDesc,
        REFIID riid,
        void **ppCommandQueue) PURE;
    STDMETHOD(CreateCommandAllocator)(THIS_
        D3D12_COMMAND_LIST_TYPE type,
        REFIID riid,
        void **ppCommandAllocator) PURE;
    STDMETHOD(CreateGraphicsPipelineState)(THIS_
        const void *pDesc,
        REFIID riid,
        void **ppPipelineState) PURE;
    STDMETHOD(CreateComputePipelineState)(THIS_
        const void *pDesc,
        REFIID riid,
        void **ppPipelineState) PURE;
    STDMETHOD(CreateCommandList)(THIS_
        UINT nodeMask,
        D3D12_COMMAND_LIST_TYPE type,
        ID3D12CommandAllocator *pCommandAllocator,
        ID3D12PipelineState *pInitialState,
        REFIID riid,
        void **ppCommandList) PURE;
    STDMETHOD(CheckFeatureSupport)(THIS_
        D3D12_FEATURE_LEVEL Feature,
        void *pFeatureSupportData,
        UINT FeatureSupportDataSize) PURE;
    STDMETHOD(CreateDescriptorHeap)(THIS_
        const void *pDescriptorHeapDesc,
        REFIID riid,
        void **ppDescriptorHeap) PURE;
    STDMETHOD_(UINT, GetDescriptorHandleIncrementSize)(THIS_
        D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapType) PURE;
    STDMETHOD(CreateRootSignature)(THIS_
        UINT nodeMask,
        const void *pBlobWithRootSignature,
        SIZE_T blobLengthInBytes,
        REFIID riid,
        void **ppRootSignature) PURE;
    STDMETHOD(CreateCommittedResource)(THIS_
        const void *pHeapProperties,
        D3D12_HEAP_FLAGS HeapFlags,
        const void *pDesc,
        D3D12_RESOURCE_STATES InitialResourceState,
        const void *pOptimizedClearValue,
        REFIID riid,
        void **ppResource) PURE;
    STDMETHOD(CreateHeap)(THIS_
        const void *pDesc,
        REFIID riid,
        void **ppHeap) PURE;
    STDMETHOD(CreatePlacedResource)(THIS_
        ID3D12Heap *pHeap,
        UINT64 HeapOffset,
        const void *pDesc,
        D3D12_RESOURCE_STATES InitialState,
        const void *pOptimizedClearValue,
        REFIID riid,
        void **ppResource) PURE;
    STDMETHOD(CreateReservedResource)(THIS_
        const void *pDesc,
        D3D12_RESOURCE_STATES InitialState,
        const void *pOptimizedClearValue,
        REFIID riid,
        void **ppResource) PURE;
    STDMETHOD(CreateFence)(THIS_
        UINT64 InitialValue,
        D3D12_FENCE_FLAGS Flags,
        REFIID riid,
        void **ppFence) PURE;
    STDMETHOD(GetDeviceRemovedReason)(THIS) PURE;
    STDMETHOD(GetCopyableFootprints)(THIS_
        const void *pResourceDesc,
        UINT FirstSubresource,
        UINT NumSubresources,
        UINT64 BaseOffset,
        void *pLayouts,
        UINT *pNumRows,
        UINT64 *pRowSizeInBytes,
        UINT64 *pTotalBytes) PURE;
    STDMETHOD(CreateQueryHeap)(THIS_
        const void *pDesc,
        REFIID riid,
        void **ppQueryHeap) PURE;
    STDMETHOD(SetStablePowerState)(THIS_ BOOL Enable) PURE;
    STDMETHOD(CreateCommandSignature)(THIS_
        const void *pDesc,
        ID3D12RootSignature *pRootSignature,
        REFIID riid,
        void **ppCommandSignature) PURE;
    STDMETHOD(GetResourceTiling)(THIS_
        ID3D12Resource *pTiledResource,
        UINT *pNumTilesForEntireResource,
        void *pPackedMipDesc,
        void *pStandardTileShapeForNonPackedMips,
        UINT *pNumSubresourceTilings,
        UINT FirstSubresourceTilingToGet,
        void *pSubresourceTilingsForNonPackedMips) PURE;
    STDMETHOD_(LUID, GetAdapterLuid)(THIS) PURE;
    STDMETHOD(CreateRenderTargetView)(THIS_
        ID3D12Resource *pResource,
        const void *pDesc,
        D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) PURE;
    STDMETHOD(CreateDepthStencilView)(THIS_
        ID3D12Resource *pResource,
        const void *pDesc,
        D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) PURE;
};

// ── ID3D12CommandQueue (inherits ID3D12Object) ──

DECLARE_INTERFACE_(ID3D12CommandQueue, ID3D12Object) {
    STDMETHOD_(void, UpdateTileMappings)(THIS_
        ID3D12Resource *pResource,
        UINT NumResourceRegions,
        const void *pResourceRegionStartCoordinates,
        const void *pResourceRegionSizes,
        ID3D12Heap *pHeap,
        UINT NumRanges,
        const void *pRangeFlags,
        const UINT *pHeapRangeStartOffsets,
        const UINT *pRangeTileCounts,
        D3D12_TILE_MAPPING_FLAGS Flags) PURE;
    STDMETHOD(CopyTileMappings)(THIS_
        ID3D12Resource *pDstResource,
        const void *pDstRegionStartCoordinate,
        ID3D12Resource *pSrcResource,
        const void *pSrcRegionStartCoordinate,
        const void *pRegionSize,
        D3D12_TILE_MAPPING_FLAGS Flags) PURE;
    STDMETHOD(ExecuteCommandLists)(THIS_
        UINT NumCommandLists,
        ID3D12CommandList *const *ppCommandLists) PURE;
    STDMETHOD(SetMarker)(THIS_ UINT Metadata, const void *pData, UINT Size) PURE;
    STDMETHOD(BeginEvent)(THIS_ UINT Metadata, const void *pData, UINT Size) PURE;
    STDMETHOD(EndEvent)(THIS) PURE;
    STDMETHOD(Signal)(THIS_ ID3D12Fence *pFence, UINT64 Value) PURE;
    STDMETHOD(Wait)(THIS_ ID3D12Fence *pFence, UINT64 Value) PURE;
    STDMETHOD(GetTimestampFrequency)(THIS_ UINT64 *pFrequency) PURE;
    STDMETHOD(GetClockCalibration)(THIS_ UINT64 *pGpuTimestamp, UINT64 *pCpuTimestamp) PURE;
    STDMETHOD_(void, GetDesc)(THIS_ D3D12_COMMAND_QUEUE_DESC *pDesc) PURE;
};

// ── DLL exports ──

extern "C" HRESULT WINAPI D3D12CreateDevice(
    IUnknown *pAdapter,
    D3D12_FEATURE_LEVEL MinimumFeatureLevel,
    REFIID riid,
    void **ppDevice);

extern "C" HRESULT WINAPI D3D12GetDebugInterface(
    REFIID riid,
    void **ppDebug);

// ═══════════════════════════════════════════════════════════════
// M2: Command allocator + command list types
// ═══════════════════════════════════════════════════════════════

typedef UINT64 D3D12_GPU_VIRTUAL_ADDRESS;

typedef struct D3D12_CPU_DESCRIPTOR_HANDLE {
    SIZE_T ptr;
} D3D12_CPU_DESCRIPTOR_HANDLE;

typedef struct D3D12_GPU_DESCRIPTOR_HANDLE {
    UINT64 ptr;
} D3D12_GPU_DESCRIPTOR_HANDLE;

typedef enum D3D12_PRIMITIVE_TOPOLOGY {
    D3D_PRIMITIVE_TOPOLOGY_UNDEFINED          = 0,
    D3D_PRIMITIVE_TOPOLOGY_POINTLIST          = 1,
    D3D_PRIMITIVE_TOPOLOGY_LINELIST           = 2,
    D3D_PRIMITIVE_TOPOLOGY_LINESTRIP          = 3,
    D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST       = 4,
    D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP      = 5,
    D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ       = 10,
    D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ      = 11,
    D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ   = 12,
    D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ  = 13,
    D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST  = 33,
    D3D_PRIMITIVE_TOPOLOGY_32_CONTROL_POINT_PATCHLIST = 64,
} D3D12_PRIMITIVE_TOPOLOGY;

typedef enum D3D12_PRIMITIVE_TOPOLOGY_TYPE {
    D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED = 0,
    D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT     = 1,
    D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE      = 2,
    D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE  = 3,
    D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH     = 4,
} D3D12_PRIMITIVE_TOPOLOGY_TYPE;

typedef struct D3D12_VERTEX_BUFFER_VIEW {
    D3D12_GPU_VIRTUAL_ADDRESS BufferLocation;
    UINT SizeInBytes;
    UINT StrideInBytes;
} D3D12_VERTEX_BUFFER_VIEW;

typedef struct D3D12_INDEX_BUFFER_VIEW {
    D3D12_GPU_VIRTUAL_ADDRESS BufferLocation;
    UINT SizeInBytes;
    DXGI_FORMAT Format;
} D3D12_INDEX_BUFFER_VIEW;

typedef struct D3D12_VIEWPORT {
    FLOAT TopLeftX;
    FLOAT TopLeftY;
    FLOAT Width;
    FLOAT Height;
    FLOAT MinDepth;
    FLOAT MaxDepth;
} D3D12_VIEWPORT;

typedef struct D3D12_RECT {
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
} D3D12_RECT;

typedef enum D3D12_RESOURCE_BARRIER_TYPE {
    D3D12_RESOURCE_BARRIER_TYPE_TRANSITION = 0,
    D3D12_RESOURCE_BARRIER_TYPE_ALIASING   = 1,
    D3D12_RESOURCE_BARRIER_TYPE_UAV        = 2,
} D3D12_RESOURCE_BARRIER_TYPE;

typedef enum D3D12_RESOURCE_BARRIER_FLAGS {
    D3D12_RESOURCE_BARRIER_FLAG_NONE     = 0,
    D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY = 0x1,
    D3D12_RESOURCE_BARRIER_FLAG_END_ONLY   = 0x2,
} D3D12_RESOURCE_BARRIER_FLAGS;

typedef struct D3D12_RESOURCE_TRANSITION_BARRIER {
    ID3D12Resource *pResource;
    UINT Subresource;
    D3D12_RESOURCE_STATES StateBefore;
    D3D12_RESOURCE_STATES StateAfter;
} D3D12_RESOURCE_TRANSITION_BARRIER;

typedef struct D3D12_RESOURCE_ALIASING_BARRIER {
    ID3D12Resource *pResourceBefore;
    ID3D12Resource *pResourceAfter;
} D3D12_RESOURCE_ALIASING_BARRIER;

typedef struct D3D12_RESOURCE_UAV_BARRIER {
    ID3D12Resource *pResource;
} D3D12_RESOURCE_UAV_BARRIER;

typedef struct D3D12_RESOURCE_BARRIER {
    D3D12_RESOURCE_BARRIER_TYPE Type;
    D3D12_RESOURCE_BARRIER_FLAGS Flags;
    union {
        D3D12_RESOURCE_TRANSITION_BARRIER Transition;
        D3D12_RESOURCE_ALIASING_BARRIER Aliasing;
        D3D12_RESOURCE_UAV_BARRIER UAV;
    };
} D3D12_RESOURCE_BARRIER;

typedef struct D3D12_TEXTURE_COPY_LOCATION {
    ID3D12Resource *pResource;
    // Union with type discriminator - simplified for M2
    UINT SubresourceIndex;
} D3D12_TEXTURE_COPY_LOCATION;

typedef enum D3D12_COMMAND_LIST_FLAGS {
    D3D12_COMMAND_LIST_FLAG_NONE = 0,
} D3D12_COMMAND_LIST_FLAGS;

// ── IIDs ──

static const GUID IID_ID3D12CommandAllocator = {
    0x6102dee4, 0xaf59, 0x4b09, {0xb9, 0x99, 0xb4, 0x4d, 0x73, 0xf0, 0x9b, 0x24}};

static const GUID IID_ID3D12GraphicsCommandList = {
    0x5b160d0f, 0xac1b, 0x4185, {0x8b, 0xa8, 0xb3, 0xae, 0x42, 0xa5, 0xa4, 0x55}};

// ── ID3D12CommandAllocator ──

DECLARE_INTERFACE_(ID3D12CommandAllocator, ID3D12Object) {
    STDMETHOD(Reset)(THIS) PURE;
};

// ── ID3D12GraphicsCommandList (M2: lifecycle + stubs) ──

DECLARE_INTERFACE_(ID3D12GraphicsCommandList, ID3D12Object) {
    // Lifecycle
    STDMETHOD(Close)(THIS) PURE;
    STDMETHOD(Reset)(THIS_
        ID3D12CommandAllocator *pAllocator,
        ID3D12PipelineState *pInitialState) PURE;

    // State
    STDMETHOD_(void, SetPipelineState)(THIS_ ID3D12PipelineState *pPipelineState) PURE;
    STDMETHOD_(void, SetGraphicsRootSignature)(THIS_ ID3D12RootSignature *pRootSignature) PURE;
    STDMETHOD_(void, SetComputeRootSignature)(THIS_ ID3D12RootSignature *pRootSignature) PURE;
    STDMETHOD_(void, IASetPrimitiveTopology)(THIS_ D3D12_PRIMITIVE_TOPOLOGY Topology) PURE;
    STDMETHOD_(void, IASetVertexBuffers)(THIS_
        UINT StartSlot, UINT NumViews,
        const D3D12_VERTEX_BUFFER_VIEW *pViews) PURE;
    STDMETHOD_(void, IASetIndexBuffer)(THIS_ const D3D12_INDEX_BUFFER_VIEW *pView) PURE;
    STDMETHOD_(void, OMSetRenderTargets)(THIS_
        UINT NumRenderTargetDescriptors,
        const D3D12_CPU_DESCRIPTOR_HANDLE *pRenderTargetDescriptors,
        BOOL RTsSingleHandleToDescriptorRange,
        const D3D12_CPU_DESCRIPTOR_HANDLE *pDepthStencilDescriptor) PURE;
    STDMETHOD_(void, RSSetViewports)(THIS_
        UINT NumViewports, const D3D12_VIEWPORT *pViewports) PURE;
    STDMETHOD_(void, RSSetScissorRects)(THIS_
        UINT NumRects, const D3D12_RECT *pRects) PURE;
    STDMETHOD_(void, SetDescriptorHeaps)(THIS_
        UINT NumHeaps, ID3D12DescriptorHeap *const *ppHeaps) PURE;

    // Draw / Dispatch
    STDMETHOD_(void, DrawInstanced)(THIS_
        UINT VertexCountPerInstance, UINT InstanceCount,
        UINT StartVertexLocation, UINT StartInstanceLocation) PURE;
    STDMETHOD_(void, DrawIndexedInstanced)(THIS_
        UINT IndexCountPerInstance, UINT InstanceCount,
        UINT StartIndexLocation, INT BaseVertexLocation,
        UINT StartInstanceLocation) PURE;
    STDMETHOD_(void, Dispatch)(THIS_
        UINT ThreadGroupCountX, UINT ThreadGroupCountY,
        UINT ThreadGroupCountZ) PURE;

    // Copy
    STDMETHOD_(void, CopyBufferRegion)(THIS_
        ID3D12Resource *pDstBuffer, UINT64 DstOffset,
        ID3D12Resource *pSrcBuffer, UINT64 SrcOffset,
        UINT64 NumBytes) PURE;
    STDMETHOD_(void, CopyTextureRegion)(THIS_
        const D3D12_TEXTURE_COPY_LOCATION *pDst,
        UINT DstX, UINT DstY, UINT DstZ,
        const D3D12_TEXTURE_COPY_LOCATION *pSrc,
        const void *pSrcBox) PURE;
    STDMETHOD_(void, CopyResource)(THIS_
        ID3D12Resource *pDstResource,
        ID3D12Resource *pSrcResource) PURE;

    // Barriers
    STDMETHOD_(void, ResourceBarrier)(THIS_
        UINT NumBarriers, const D3D12_RESOURCE_BARRIER *pBarriers) PURE;
};

// ═══════════════════════════════════════════════════════════════
// M3: Resource + Heap types
// ═══════════════════════════════════════════════════════════════

enum D3D12_RESOURCE_DIMENSION {
    D3D12_RESOURCE_DIMENSION_UNKNOWN   = 0,
    D3D12_RESOURCE_DIMENSION_BUFFER    = 1,
    D3D12_RESOURCE_DIMENSION_TEXTURE1D = 2,
    D3D12_RESOURCE_DIMENSION_TEXTURE2D = 3,
    D3D12_RESOURCE_DIMENSION_TEXTURE3D = 4,
};

enum D3D12_TEXTURE_LAYOUT {
    D3D12_TEXTURE_LAYOUT_UNKNOWN              = 0,
    D3D12_TEXTURE_LAYOUT_ROW_MAJOR            = 1,
    D3D12_TEXTURE_LAYOUT_64KB_UNDEFINED_SWIZZLE = 2,
    D3D12_TEXTURE_LAYOUT_64KB_STANDARD_SWIZZLE  = 3,
};

enum D3D12_RESOURCE_FLAGS {
    D3D12_RESOURCE_FLAG_NONE                   = 0,
    D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET    = 0x1,
    D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL    = 0x2,
    D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS = 0x4,
    D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE   = 0x8,
    D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER    = 0x10,
    D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS = 0x20,
    D3D12_RESOURCE_FLAG_VIDEO_DECODE_REFERENCE_ONLY = 0x40,
};

enum D3D12_HEAP_TYPE {
    D3D12_HEAP_TYPE_DEFAULT      = 1,
    D3D12_HEAP_TYPE_UPLOAD       = 2,
    D3D12_HEAP_TYPE_READBACK     = 3,
    D3D12_HEAP_TYPE_CUSTOM       = 4,
};

enum D3D12_CPU_PAGE_PROPERTY {
    D3D12_CPU_PAGE_PROPERTY_UNKNOWN       = 0,
    D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE = 1,
    D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE = 2,
    D3D12_CPU_PAGE_PROPERTY_WRITE_BACK    = 3,
};

enum D3D12_MEMORY_POOL {
    D3D12_MEMORY_POOL_UNKNOWN = 0,
    D3D12_MEMORY_POOL_L0      = 1,
    D3D12_MEMORY_POOL_L1      = 2,
};

typedef struct D3D12_HEAP_PROPERTIES {
    D3D12_HEAP_TYPE         Type;
    D3D12_CPU_PAGE_PROPERTY CPUPageProperty;
    D3D12_MEMORY_POOL       MemoryPoolPreference;
    UINT                    CreationNodeMask;
    UINT                    VisibleNodeMask;
} D3D12_HEAP_PROPERTIES;

typedef struct D3D12_HEAP_DESC {
    UINT64              SizeInBytes;
    D3D12_HEAP_PROPERTIES Properties;
    UINT64              Alignment;
    D3D12_HEAP_FLAGS    Flags;
} D3D12_HEAP_DESC;

typedef struct D3D12_RESOURCE_DESC {
    D3D12_RESOURCE_DIMENSION Dimension;
    UINT64                   Alignment;
    UINT64                   Width;
    UINT                     Height;
    UINT16                   DepthOrArraySize;
    UINT16                   MipLevels;
    DXGI_FORMAT              Format;
    DXGI_SAMPLE_DESC         SampleDesc;
    D3D12_TEXTURE_LAYOUT     Layout;
    D3D12_RESOURCE_FLAGS     Flags;
} D3D12_RESOURCE_DESC;

typedef struct D3D12_RESOURCE_ALLOCATION_INFO {
    UINT64 SizeInBytes;
    UINT64 Alignment;
} D3D12_RESOURCE_ALLOCATION_INFO;

typedef struct D3D12_RANGE {
    SIZE_T Begin;
    SIZE_T End;
} D3D12_RANGE;

typedef struct D3D12_BOX {
    UINT left;
    UINT top;
    UINT front;
    UINT right;
    UINT bottom;
    UINT back;
} D3D12_BOX;

typedef struct D3D12_SUBRESOURCE_DATA {
    const void *pData;
    LONG_PTR RowPitch;
    LONG_PTR SlicePitch;
} D3D12_SUBRESOURCE_DATA;

typedef struct D3D12_SUBRESOURCE_FOOTPRINT {
    DXGI_FORMAT Format;
    UINT Width;
    UINT Height;
    UINT Depth;
    UINT RowPitch;
} D3D12_SUBRESOURCE_FOOTPRINT;

typedef struct D3D12_PLACED_SUBRESOURCE_FOOTPRINT {
    UINT64 Offset;
    D3D12_SUBRESOURCE_FOOTPRINT Footprint;
} D3D12_PLACED_SUBRESOURCE_FOOTPRINT;

// ── IIDs ──

static const GUID IID_ID3D12Resource = {
    0x696442be, 0xa72e, 0x4059, {0xbc, 0x79, 0x5b, 0x5c, 0x98, 0x04, 0x0f, 0xad}};

static const GUID IID_ID3D12Heap = {
    0x6b3b2502, 0x6e51, 0x45b3, {0x90, 0xee, 0x98, 0x84, 0x26, 0x5e, 0x8d, 0xf6}};

// ── ID3D12Resource ──

DECLARE_INTERFACE_(ID3D12Resource, ID3D12Object) {
    STDMETHOD(Map)(THIS_ UINT Subresource, const D3D12_RANGE *pReadRange,
                   void **ppData) PURE;
    STDMETHOD_(void, Unmap)(THIS_ UINT Subresource,
                             const D3D12_RANGE *pWrittenRange) PURE;
    STDMETHOD_(D3D12_RESOURCE_DESC, GetDesc)(THIS) PURE;
    STDMETHOD_(D3D12_GPU_VIRTUAL_ADDRESS, GetGPUVirtualAddress)(THIS) PURE;
    STDMETHOD(WriteToSubresource)(THIS_
        UINT DstSubresource, const D3D12_BOX *pDstBox,
        const void *pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch) PURE;
    STDMETHOD(ReadFromSubresource)(THIS_
        void *pDstData, UINT DstRowPitch, UINT DstDepthPitch,
        UINT SrcSubresource, const D3D12_BOX *pSrcBox) PURE;
    STDMETHOD(GetHeapProperties)(THIS_
        D3D12_HEAP_PROPERTIES *pHeapProperties,
        D3D12_HEAP_FLAGS *pHeapFlags) PURE;
};

// ── ID3D12Heap ──

DECLARE_INTERFACE_(ID3D12Heap, ID3D12Object) {
    STDMETHOD_(D3D12_HEAP_DESC, GetDesc)(THIS) PURE;
};

// ── Additional M3 type ──

typedef struct D3D12_CLEAR_VALUE {
    DXGI_FORMAT Format;
    union {
        FLOAT Color[4];
        struct {
            FLOAT Depth;
            UINT8 Stencil;
        } DepthStencil;
    };
} D3D12_CLEAR_VALUE;

// ═══════════════════════════════════════════════════════════════
// M4: Descriptor Heap types
// ═══════════════════════════════════════════════════════════════

typedef struct D3D12_DESCRIPTOR_HEAP_DESC {
    D3D12_DESCRIPTOR_HEAP_TYPE  Type;
    UINT                        NumDescriptors;
    D3D12_DESCRIPTOR_HEAP_FLAGS Flags;
    UINT                        NodeMask;
} D3D12_DESCRIPTOR_HEAP_DESC;

// ── IIDs ──

static const GUID IID_ID3D12DescriptorHeap = {
    0x8efb471d, 0x616c, 0x4f49, {0x90, 0xf7, 0x12, 0x7b, 0xb7, 0x63, 0xfa, 0x51}};

// ── ID3D12DescriptorHeap ──

DECLARE_INTERFACE_(ID3D12DescriptorHeap, ID3D12Object) {
    STDMETHOD_(D3D12_DESCRIPTOR_HEAP_DESC, GetDesc)(THIS) PURE;
    STDMETHOD_(D3D12_CPU_DESCRIPTOR_HANDLE, GetCPUDescriptorHandleForHeapStart)(THIS) PURE;
    STDMETHOD_(D3D12_GPU_DESCRIPTOR_HANDLE, GetGPUDescriptorHandleForHeapStart)(THIS) PURE;
};

// ═══════════════════════════════════════════════════════════════
// M6: Root Signature types
// ═══════════════════════════════════════════════════════════════

enum D3D12_ROOT_PARAMETER_TYPE {
    D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE = 0,
    D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS  = 1,
    D3D12_ROOT_PARAMETER_TYPE_CBV              = 2,
    D3D12_ROOT_PARAMETER_TYPE_SRV              = 3,
    D3D12_ROOT_PARAMETER_TYPE_UAV              = 4,
};

enum D3D12_DESCRIPTOR_RANGE_TYPE {
    D3D12_DESCRIPTOR_RANGE_TYPE_SRV     = 0,
    D3D12_DESCRIPTOR_RANGE_TYPE_UAV     = 1,
    D3D12_DESCRIPTOR_RANGE_TYPE_CBV     = 2,
    D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER = 3,
};

enum D3D12_SHADER_VISIBILITY {
    D3D12_SHADER_VISIBILITY_ALL       = 0,
    D3D12_SHADER_VISIBILITY_VERTEX    = 1,
    D3D12_SHADER_VISIBILITY_HULL      = 2,
    D3D12_SHADER_VISIBILITY_DOMAIN    = 3,
    D3D12_SHADER_VISIBILITY_GEOMETRY  = 4,
    D3D12_SHADER_VISIBILITY_PIXEL     = 5,
    D3D12_SHADER_VISIBILITY_AMPLIFICATION = 6,
    D3D12_SHADER_VISIBILITY_MESH      = 7,
};

enum D3D12_ROOT_SIGNATURE_FLAGS {
    D3D12_ROOT_SIGNATURE_FLAG_NONE                                = 0,
    D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT  = 0x1,
    D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS      = 0x2,
    D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS        = 0x4,
    D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS      = 0x8,
    D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS    = 0x10,
    D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS       = 0x20,
    D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT                 = 0x40,
    D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE                = 0x80,
};

enum D3D12_STATIC_BORDER_COLOR {
    D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK = 0,
    D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK      = 1,
    D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE      = 2,
};

enum D3D12_FILTER {
    D3D12_FILTER_MIN_MAG_MIP_POINT                          = 0,
    D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR                   = 0x1,
    D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT             = 0x4,
    D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR                   = 0x5,
    D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT                   = 0x10,
    D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR            = 0x11,
    D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT                   = 0x14,
    D3D12_FILTER_MIN_MAG_MIP_LINEAR                         = 0x15,
    D3D12_FILTER_ANISOTROPIC                                = 0x55,
    D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT               = 0x80,
    D3D12_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR        = 0x81,
    D3D12_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT  = 0x84,
    D3D12_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR        = 0x85,
    D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT        = 0x90,
    D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR = 0x91,
    D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT        = 0x94,
    D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR              = 0x95,
    D3D12_FILTER_COMPARISON_ANISOTROPIC                     = 0xd5,
};

enum D3D12_TEXTURE_ADDRESS_MODE {
    D3D12_TEXTURE_ADDRESS_MODE_WRAP        = 1,
    D3D12_TEXTURE_ADDRESS_MODE_MIRROR      = 2,
    D3D12_TEXTURE_ADDRESS_MODE_CLAMP       = 3,
    D3D12_TEXTURE_ADDRESS_MODE_BORDER      = 4,
    D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE = 5,
};

enum D3D12_COMPARISON_FUNC {
    D3D12_COMPARISON_FUNC_NEVER         = 1,
    D3D12_COMPARISON_FUNC_LESS          = 2,
    D3D12_COMPARISON_FUNC_EQUAL         = 3,
    D3D12_COMPARISON_FUNC_LESS_EQUAL    = 4,
    D3D12_COMPARISON_FUNC_GREATER       = 5,
    D3D12_COMPARISON_FUNC_NOT_EQUAL     = 6,
    D3D12_COMPARISON_FUNC_GREATER_EQUAL = 7,
    D3D12_COMPARISON_FUNC_ALWAYS        = 8,
};

typedef struct D3D12_DESCRIPTOR_RANGE {
    D3D12_DESCRIPTOR_RANGE_TYPE RangeType;
    UINT                        NumDescriptors;
    UINT                        BaseShaderRegister;
    UINT                        RegisterSpace;
    UINT                        OffsetInDescriptorsFromTableStart;
} D3D12_DESCRIPTOR_RANGE;

typedef struct D3D12_ROOT_DESCRIPTOR_TABLE {
    UINT                         NumDescriptorRanges;
    const D3D12_DESCRIPTOR_RANGE *pDescriptorRanges;
} D3D12_ROOT_DESCRIPTOR_TABLE;

typedef struct D3D12_ROOT_CONSTANTS {
    UINT ShaderRegister;
    UINT RegisterSpace;
    UINT Num32BitValues;
} D3D12_ROOT_CONSTANTS;

typedef struct D3D12_ROOT_DESCRIPTOR {
    UINT ShaderRegister;
    UINT RegisterSpace;
} D3D12_ROOT_DESCRIPTOR;

typedef struct D3D12_ROOT_PARAMETER {
    D3D12_ROOT_PARAMETER_TYPE ParameterType;
    union {
        D3D12_ROOT_DESCRIPTOR_TABLE DescriptorTable;
        D3D12_ROOT_CONSTANTS        Constants;
        D3D12_ROOT_DESCRIPTOR       Descriptor;
    };
    D3D12_SHADER_VISIBILITY ShaderVisibility;
} D3D12_ROOT_PARAMETER;

typedef struct D3D12_STATIC_SAMPLER_DESC {
    D3D12_FILTER              Filter;
    D3D12_TEXTURE_ADDRESS_MODE AddressU;
    D3D12_TEXTURE_ADDRESS_MODE AddressV;
    D3D12_TEXTURE_ADDRESS_MODE AddressW;
    FLOAT                     MipLODBias;
    UINT                      MaxAnisotropy;
    D3D12_COMPARISON_FUNC     ComparisonFunc;
    D3D12_STATIC_BORDER_COLOR BorderColor;
    FLOAT                     MinLOD;
    FLOAT                     MaxLOD;
    UINT                      ShaderRegister;
    UINT                      RegisterSpace;
    D3D12_SHADER_VISIBILITY   ShaderVisibility;
} D3D12_STATIC_SAMPLER_DESC;

typedef struct D3D12_ROOT_SIGNATURE_DESC {
    UINT                            NumParameters;
    const D3D12_ROOT_PARAMETER     *pParameters;
    UINT                            NumStaticSamplers;
    const D3D12_STATIC_SAMPLER_DESC *pStaticSamplers;
    D3D12_ROOT_SIGNATURE_FLAGS      Flags;
} D3D12_ROOT_SIGNATURE_DESC;

// ── IID ──

static const GUID IID_ID3D12RootSignature = {
    0xc54a6b66, 0x72df, 0x4ee8, {0x8b, 0xe5, 0xa9, 0x46, 0xa1, 0x42, 0x92, 0x14}};

// ── ID3D12RootSignature ──

DECLARE_INTERFACE_(ID3D12RootSignature, ID3D12Object) {
};

// ═══════════════════════════════════════════════════════════════
// M7: Pipeline State types
// ═══════════════════════════════════════════════════════════════

enum D3D12_PIPELINE_STATE_SUBOBJECT_TYPE {
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE    = 0,
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS                = 1,
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS                = 2,
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DS                = 3,
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_HS                = 4,
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_GS                = 5,
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS                = 6,
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_STREAM_OUTPUT     = 7,
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND             = 8,
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_MASK       = 9,
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER        = 10,
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL     = 11,
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_INPUT_LAYOUT      = 12,
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_IB_STRIP_CUT_VALUE = 13,
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY = 14,
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS = 15,
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT = 16,
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_NODE_MASK         = 17,
};

enum D3D12_BLEND {
    D3D12_BLEND_ZERO            = 1,
    D3D12_BLEND_ONE             = 2,
    D3D12_BLEND_SRC_COLOR       = 3,
    D3D12_BLEND_INV_SRC_COLOR   = 4,
    D3D12_BLEND_SRC_ALPHA       = 5,
    D3D12_BLEND_INV_SRC_ALPHA   = 6,
    D3D12_BLEND_DEST_ALPHA      = 7,
    D3D12_BLEND_INV_DEST_ALPHA  = 8,
    D3D12_BLEND_DEST_COLOR      = 9,
    D3D12_BLEND_INV_DEST_COLOR  = 10,
    D3D12_BLEND_SRC_ALPHA_SAT   = 11,
    D3D12_BLEND_BLEND_FACTOR    = 14,
    D3D12_BLEND_INV_BLEND_FACTOR = 15,
};

enum D3D12_BLEND_OP {
    D3D12_BLEND_OP_ADD          = 1,
    D3D12_BLEND_OP_SUBTRACT     = 2,
    D3D12_BLEND_OP_REV_SUBTRACT = 3,
    D3D12_BLEND_OP_MIN          = 4,
    D3D12_BLEND_OP_MAX          = 5,
};

enum D3D12_LOGIC_OP {
    D3D12_LOGIC_OP_CLEAR         = 0,
    D3D12_LOGIC_OP_SET           = 1,
    D3D12_LOGIC_OP_COPY          = 2,
    D3D12_LOGIC_OP_COPY_INVERTED = 3,
    D3D12_LOGIC_OP_NOOP          = 4,
};

enum D3D12_COLOR_WRITE_ENABLE {
    D3D12_COLOR_WRITE_ENABLE_RED   = 1,
    D3D12_COLOR_WRITE_ENABLE_GREEN = 2,
    D3D12_COLOR_WRITE_ENABLE_BLUE  = 4,
    D3D12_COLOR_WRITE_ENABLE_ALPHA = 8,
    D3D12_COLOR_WRITE_ENABLE_ALL   = 0xf,
};

enum D3D12_FILL_MODE {
    D3D12_FILL_MODE_WIREFRAME = 2,
    D3D12_FILL_MODE_SOLID     = 3,
};

enum D3D12_CULL_MODE {
    D3D12_CULL_MODE_NONE  = 1,
    D3D12_CULL_MODE_FRONT = 2,
    D3D12_CULL_MODE_BACK  = 3,
};

enum D3D12_CONSERVATIVE_RASTERIZATION_MODE {
    D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF = 0,
    D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON  = 1,
};

enum D3D12_DEPTH_WRITE_MASK {
    D3D12_DEPTH_WRITE_MASK_ZERO = 0,
    D3D12_DEPTH_WRITE_MASK_ALL  = 1,
};

enum D3D12_STENCIL_OP {
    D3D12_STENCIL_OP_KEEP     = 1,
    D3D12_STENCIL_OP_ZERO     = 2,
    D3D12_STENCIL_OP_REPLACE  = 3,
    D3D12_STENCIL_OP_INCR_SAT = 4,
    D3D12_STENCIL_OP_DECR_SAT = 5,
    D3D12_STENCIL_OP_INVERT   = 6,
    D3D12_STENCIL_OP_INCR     = 7,
    D3D12_STENCIL_OP_DECR     = 8,
};

enum D3D12_INPUT_CLASSIFICATION {
    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA   = 0,
    D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA = 1,
};

enum D3D12_INDEX_BUFFER_STRIP_CUT_VALUE {
    D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED     = 0,
    D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFF       = 1,
    D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFFFFFF   = 2,
};

typedef struct D3D12_RENDER_TARGET_BLEND_DESC {
    BOOL           BlendEnable;
    BOOL           LogicOpEnable;
    D3D12_BLEND    SrcBlend;
    D3D12_BLEND    DestBlend;
    D3D12_BLEND_OP BlendOp;
    D3D12_BLEND    SrcBlendAlpha;
    D3D12_BLEND    DestBlendAlpha;
    D3D12_BLEND_OP BlendOpAlpha;
    D3D12_LOGIC_OP LogicOp;
    UINT8          RenderTargetWriteMask;
} D3D12_RENDER_TARGET_BLEND_DESC;

typedef struct D3D12_BLEND_DESC {
    BOOL                           AlphaToCoverageEnable;
    BOOL                           IndependentBlendEnable;
    D3D12_RENDER_TARGET_BLEND_DESC RenderTarget[8];
} D3D12_BLEND_DESC;

typedef struct D3D12_RASTERIZER_DESC {
    D3D12_FILL_MODE                       FillMode;
    D3D12_CULL_MODE                       CullMode;
    BOOL                                  FrontCounterClockwise;
    INT                                   DepthBias;
    FLOAT                                 DepthBiasClamp;
    FLOAT                                 SlopeScaledDepthBias;
    BOOL                                  DepthClipEnable;
    BOOL                                  MultisampleEnable;
    BOOL                                  AntialiasedLineEnable;
    UINT                                  ForcedSampleCount;
    D3D12_CONSERVATIVE_RASTERIZATION_MODE ConservativeRaster;
} D3D12_RASTERIZER_DESC;

typedef struct D3D12_DEPTH_STENCILOP_DESC {
    D3D12_STENCIL_OP      StencilFailOp;
    D3D12_STENCIL_OP      StencilDepthFailOp;
    D3D12_STENCIL_OP      StencilPassOp;
    D3D12_COMPARISON_FUNC StencilFunc;
} D3D12_DEPTH_STENCILOP_DESC;

typedef struct D3D12_DEPTH_STENCIL_DESC {
    BOOL                       DepthEnable;
    D3D12_DEPTH_WRITE_MASK     DepthWriteMask;
    D3D12_COMPARISON_FUNC      DepthFunc;
    BOOL                       StencilEnable;
    UINT8                      StencilReadMask;
    UINT8                      StencilWriteMask;
    D3D12_DEPTH_STENCILOP_DESC FrontFace;
    D3D12_DEPTH_STENCILOP_DESC BackFace;
} D3D12_DEPTH_STENCIL_DESC;

typedef struct D3D12_SHADER_BYTECODE {
    const void *pShaderBytecode;
    SIZE_T      BytecodeLength;
} D3D12_SHADER_BYTECODE;

typedef struct D3D12_STREAM_OUTPUT_DESC {
    const void *pSODeclaration;
    UINT        NumEntries;
    const UINT *pBufferStrides;
    UINT        NumStrides;
    UINT        RasterizedStream;
} D3D12_STREAM_OUTPUT_DESC;

typedef struct D3D12_INPUT_ELEMENT_DESC {
    LPCSTR                     SemanticName;
    UINT                       SemanticIndex;
    DXGI_FORMAT                Format;
    UINT                       InputSlot;
    UINT                       AlignedByteOffset;
    D3D12_INPUT_CLASSIFICATION InputSlotClass;
    UINT                       InstanceDataStepRate;
} D3D12_INPUT_ELEMENT_DESC;

typedef struct D3D12_INPUT_LAYOUT_DESC {
    const D3D12_INPUT_ELEMENT_DESC *pInputElementDescs;
    UINT                            NumElements;
} D3D12_INPUT_LAYOUT_DESC;

typedef struct D3D12_GRAPHICS_PIPELINE_STATE_DESC {
    ID3D12RootSignature       *pRootSignature;
    D3D12_SHADER_BYTECODE      VS;
    D3D12_SHADER_BYTECODE      PS;
    D3D12_SHADER_BYTECODE      DS;
    D3D12_SHADER_BYTECODE      HS;
    D3D12_SHADER_BYTECODE      GS;
    D3D12_STREAM_OUTPUT_DESC   StreamOutput;
    D3D12_BLEND_DESC           BlendState;
    UINT                       SampleMask;
    D3D12_RASTERIZER_DESC      RasterizerState;
    D3D12_DEPTH_STENCIL_DESC   DepthStencilState;
    D3D12_INPUT_LAYOUT_DESC    InputLayout;
    D3D12_INDEX_BUFFER_STRIP_CUT_VALUE IBStripCutValue;
    D3D12_PRIMITIVE_TOPOLOGY_TYPE PrimitiveTopologyType;
    UINT                       NumRenderTargets;
    DXGI_FORMAT                RTVFormats[8];
    DXGI_FORMAT                DSVFormat;
    DXGI_SAMPLE_DESC           SampleDesc;
    UINT                       NodeMask;
    void                      *CachedPSO;
    SIZE_T                     CachedPSOLength;
    D3D12_PIPELINE_STATE_FLAGS Flags;
} D3D12_GRAPHICS_PIPELINE_STATE_DESC;

typedef struct D3D12_COMPUTE_PIPELINE_STATE_DESC {
    ID3D12RootSignature  *pRootSignature;
    D3D12_SHADER_BYTECODE CS;
    UINT                  NodeMask;
    void                 *CachedPSO;
    SIZE_T                CachedPSOLength;
    D3D12_PIPELINE_STATE_FLAGS Flags;
} D3D12_COMPUTE_PIPELINE_STATE_DESC;

enum D3D12_PIPELINE_STATE_FLAGS {
    D3D12_PIPELINE_STATE_FLAG_NONE         = 0,
    D3D12_PIPELINE_STATE_FLAG_TOOL_DEBUG   = 0x1,
};

// ── IID ──

static const GUID IID_ID3D12PipelineState = {
    0x765a30f3, 0xf624, 0x4c6f, {0xa8, 0x28, 0xac, 0xe9, 0x48, 0x62, 0x24, 0x45}};

// ── ID3D12PipelineState ──

DECLARE_INTERFACE_(ID3D12PipelineState, ID3D12Object) {
    STDMETHOD(GetCachedBlob)(THIS_ void **ppBlob) PURE;
};

// ═══════════════════════════════════════════════════════════════
// M8: Fence types
// ═══════════════════════════════════════════════════════════════

static const GUID IID_ID3D12Fence = {
    0x0a753dcf, 0xc4d8, 0x4b91, {0xad, 0xf6, 0xbe, 0x5a, 0x60, 0xd9, 0x5a, 0x76}};

DECLARE_INTERFACE_(ID3D12Fence, ID3D12Object) {
    STDMETHOD_(UINT64, GetCompletedValue)(THIS) PURE;
    STDMETHOD(SetEventOnCompletion)(THIS_ UINT64 Value, HANDLE hEvent) PURE;
    STDMETHOD(Signal)(THIS_ UINT64 Value) PURE;
};

// ═══════════════════════════════════════════════════════════════
// M10: DXGI Swapchain types (DXGI subset needed for IDXGISwapChain3)
// ═══════════════════════════════════════════════════════════════

enum DXGI_SWAP_EFFECT {
    DXGI_SWAP_EFFECT_DISCARD           = 0,
    DXGI_SWAP_EFFECT_SEQUENTIAL        = 1,
    DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL   = 3,
    DXGI_SWAP_EFFECT_FLIP_DISCARD      = 4,
};

enum DXGI_SCALING {
    DXGI_SCALING_STRETCH  = 0,
    DXGI_SCALING_NONE     = 1,
    DXGI_SCALING_ASPECT_RATIO_STRETCH = 2,
};

enum DXGI_ALPHA_MODE {
    DXGI_ALPHA_MODE_UNSPECIFIED     = 0,
    DXGI_ALPHA_MODE_PREMULTIPLIED   = 1,
    DXGI_ALPHA_MODE_STRAIGHT        = 2,
    DXGI_ALPHA_MODE_IGNORE          = 3,
    DXGI_ALPHA_MODE_FORCE_DWORD     = 0xFFFFFFFF,
};

#define DXGI_SWAP_CHAIN_FLAG_NONPREROTATED            1
#define DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH        2
#define DXGI_SWAP_CHAIN_FLAG_GDI_COMPATIBLE           4
#define DXGI_SWAP_CHAIN_FLAG_RESTRICTED_CONTENT       8
#define DXGI_SWAP_CHAIN_FLAG_RESTRICT_SHARED_RESOURCE_DRIVER  16
#define DXGI_SWAP_CHAIN_FLAG_DISPLAY_ONLY             32
#define DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT 64
#define DXGI_SWAP_CHAIN_FLAG_FOREGROUND_LAYER         128
#define DXGI_SWAP_CHAIN_FLAG_FULLSCREEN_VIDEO         256
#define DXGI_SWAP_CHAIN_FLAG_YUV_VIDEO                512
#define DXGI_SWAP_CHAIN_FLAG_HW_PROTECTED            1024
#define DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING           2048
#define DXGI_SWAP_CHAIN_FLAG_RESTRICTED_TO_ALL_HOLOGRAPHIC_DISPLAYS 4096

#define DXGI_PRESENT_TEST                  0x00000001
#define DXGI_PRESENT_DO_NOT_SEQUENCE       0x00000002
#define DXGI_PRESENT_RESTART               0x00000004
#define DXGI_PRESENT_DO_NOT_WAIT           0x00000008
#define DXGI_PRESENT_STEREO_PREFER_RIGHT   0x00000010
#define DXGI_PRESENT_STEREO_TEMPORARY_MONO 0x00000020
#define DXGI_PRESENT_RESTRICT_TO_OUTPUT    0x00000040
#define DXGI_PRESENT_USE_DURATION          0x00000100
#define DXGI_PRESENT_ALLOW_TEARING         0x00000200

#define DXGI_MAX_SWAP_CHAIN_BUFFERS 16

#define DXGI_ENUM_MODES_INTERLACED    1
#define DXGI_ENUM_MODES_SCALING       2
#define DXGI_ENUM_MODES_STEREO        4
#define DXGI_ENUM_MODES_DISABLED_STEREO 8

typedef struct DXGI_RATIONAL {
    UINT Numerator;
    UINT Denominator;
} DXGI_RATIONAL;

typedef struct DXGI_MODE_DESC {
    UINT            Width;
    UINT            Height;
    DXGI_RATIONAL   RefreshRate;
    DXGI_FORMAT     Format;
    UINT            ScanlineOrdering;
    UINT            Scaling;
} DXGI_MODE_DESC;

typedef struct DXGI_SAMPLE_DESC {
    UINT Count;
    UINT Quality;
} DXGI_SAMPLE_DESC;

enum DXGI_MODE_SCANLINE_ORDER {
    DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED        = 0,
    DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE        = 1,
    DXGI_MODE_SCANLINE_ORDER_UPPER_FIELD_FIRST  = 2,
    DXGI_MODE_SCANLINE_ORDER_LOWER_FIELD_FIRST  = 3,
};

enum DXGI_MODE_SCALING {
    DXGI_MODE_SCALING_UNSPECIFIED   = 0,
    DXGI_MODE_SCALING_CENTERED      = 1,
    DXGI_MODE_SCALING_STRETCHED     = 2,
};

typedef struct DXGI_SWAP_CHAIN_DESC {
    DXGI_MODE_DESC   BufferDesc;
    DXGI_SAMPLE_DESC SampleDesc;
    DXGI_USAGE       BufferUsage;
    UINT             BufferCount;
    HWND             OutputWindow;
    BOOL             Windowed;
    DXGI_SWAP_EFFECT SwapEffect;
    UINT             Flags;
} DXGI_SWAP_CHAIN_DESC;

typedef struct DXGI_SWAP_CHAIN_DESC1 {
    UINT             Width;
    UINT             Height;
    DXGI_FORMAT      Format;
    BOOL             Stereo;
    DXGI_SAMPLE_DESC SampleDesc;
    DXGI_USAGE       BufferUsage;
    UINT             BufferCount;
    DXGI_SCALING     Scaling;
    DXGI_SWAP_EFFECT SwapEffect;
    DXGI_ALPHA_MODE  AlphaMode;
    UINT             Flags;
} DXGI_SWAP_CHAIN_DESC1;

typedef struct DXGI_SWAP_CHAIN_FULLSCREEN_DESC {
    DXGI_RATIONAL               RefreshRate;
    UINT                        ScanlineOrdering;
    UINT                        Scaling;
    BOOL                        Windowed;
} DXGI_SWAP_CHAIN_FULLSCREEN_DESC;

typedef struct DXGI_PRESENT_PARAMETERS {
    UINT  DirtyRectsCount;
    void *pDirtyRects;
    void *pScrollRect;
    void *pScrollOffset;
} DXGI_PRESENT_PARAMETERS;

typedef struct DXGI_FRAME_STATISTICS {
    UINT          PresentCount;
    UINT          PresentRefreshCount;
    UINT          SyncRefreshCount;
    LARGE_INTEGER SyncGPUTime;
    LARGE_INTEGER SyncQPCTime;
} DXGI_FRAME_STATISTICS;

// ── IIDs ──

static const GUID IID_IDXGISwapChain = {
    0x310d36a0, 0xd2e7, 0x4c0a, {0xaa, 0x04, 0x6a, 0x9d, 0x23, 0xb8, 0x88, 0x6a}};

static const GUID IID_IDXGISwapChain1 = {
    0x790a45f7, 0x0d42, 0x4876, {0x98, 0x3a, 0x0a, 0x55, 0xcf, 0xe6, 0xf4, 0xaa}};

static const GUID IID_IDXGISwapChain2 = {
    0xa8be2ac4, 0x199f, 0x4946, {0xb3, 0x31, 0x79, 0x59, 0x9f, 0xb9, 0x8d, 0xe7}};

static const GUID IID_IDXGISwapChain3 = {
    0x94d99bdb, 0xf1f8, 0x4ab0, {0xb2, 0x36, 0x7d, 0xa0, 0x17, 0x0e, 0xda, 0xb1}};

// ── IDXGISwapChain1 ──

DECLARE_INTERFACE_(IDXGISwapChain1, IUnknown) {
    // IDXGISwapChain
    STDMETHOD(Present)(THIS_ UINT SyncInterval, UINT Flags) PURE;
    STDMETHOD(GetBuffer)(THIS_ UINT Buffer, REFIID riid, void **ppSurface) PURE;
    STDMETHOD(SetFullscreenState)(THIS_ BOOL Fullscreen, void *pTarget) PURE;
    STDMETHOD(GetFullscreenState)(THIS_ BOOL *pFullscreen, void **ppTarget) PURE;
    STDMETHOD(GetDesc)(THIS_ DXGI_SWAP_CHAIN_DESC *pDesc) PURE;
    STDMETHOD(ResizeBuffers)(THIS_
        UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat,
        UINT SwapChainFlags) PURE;
    STDMETHOD(ResizeTarget)(THIS_ const DXGI_MODE_DESC *pNewTargetParameters) PURE;
    STDMETHOD(GetContainingOutput)(THIS_ void **ppOutput) PURE;
    STDMETHOD(GetFrameStatistics)(THIS_ DXGI_FRAME_STATISTICS *pStats) PURE;
    STDMETHOD(GetLastPresentCount)(THIS_ UINT *pLastPresentCount) PURE;

    // IDXGISwapChain1
    STDMETHOD(GetDesc1)(THIS_ DXGI_SWAP_CHAIN_DESC1 *pDesc) PURE;
    STDMETHOD(GetFullscreenDesc)(THIS_ DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pDesc) PURE;
    STDMETHOD(GetHwnd)(THIS_ HWND *pHwnd) PURE;
    STDMETHOD(GetCoreWindow)(THIS_ REFIID refiid, void **ppUnk) PURE;
    STDMETHOD(Present1)(THIS_
        UINT SyncInterval, UINT PresentFlags,
        const void *pPresentParameters) PURE;
    STDMETHOD(IsTemporaryMonoSupported)(THIS_ BOOL *pSupported) PURE;
    STDMETHOD(GetRestrictToOutput)(THIS_ void **ppRestrictToOutput) PURE;
};

// ── IDXGISwapChain3 ──

DECLARE_INTERFACE_(IDXGISwapChain3, IDXGISwapChain1) {
    STDMETHOD_(UINT, GetCurrentBackBufferIndex)(THIS) PURE;
    STDMETHOD(CheckColorSpaceSupport)(THIS_
        UINT ColorSpace, UINT *pColorSpaceSupport) PURE;
    STDMETHOD(SetColorSpace1)(THIS_ UINT ColorSpace) PURE;
    STDMETHOD(ResizeBuffers1)(THIS_
        UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT Format,
        UINT SwapChainFlags, const UINT *pCreationNodeMask,
        void *const *ppPresentQueue) PURE;
};

