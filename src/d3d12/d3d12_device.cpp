#define INITGUID
#include "d3d12_command_queue.hpp"
#include "d3d12_command_allocator.hpp"
#include "d3d12_command_list.hpp"
#include "d3d12_device.hpp"
#include "com/com_object.hpp"
#include "log/log.hpp"
#include "util_string.hpp"
#include <d3d12.h>

namespace dxmt {
Logger Logger::s_instance("d3d12.log");
} // namespace dxmt

namespace dxmt {

MTLD3D12Device::MTLD3D12Device(std::unique_ptr<Device> &&device,
                               IMTLDXGIAdapter *pAdapter)
    : m_device(std::move(device)), m_adapter(pAdapter) {
  if (m_adapter)
    m_adapter->AddRef();
  Logger::info("D3D12 device created via DXMT Metal backend");
}

MTLD3D12Device::~MTLD3D12Device() { Logger::info("D3D12 device destroyed"); }

WMT::Device MTLD3D12Device::GetMTLDevice() {
  return m_device->device();
}

Device &MTLD3D12Device::GetDXMTDevice() { return *m_device; }

HRESULT STDMETHODCALLTYPE
MTLD3D12Device::QueryInterface(REFIID riid, void **ppvObject) {
  if (!ppvObject)
    return E_POINTER;
  *ppvObject = nullptr;

  if (riid == IID_IUnknown || riid == IID_ID3D12Object ||
      riid == IID_ID3D12DeviceChild || riid == IID_ID3D12Pageable ||
      riid == IID_ID3D12Device) {
    *ppvObject = ref(this);
    return S_OK;
  }

  Logger::warn(str::format("D3D12Device::QueryInterface: unknown IID ", riid));
  return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE MTLD3D12Device::AddRef() {
  uint32_t rc = m_refCount++;
  if (!rc)
    ++m_refPrivate;
  return rc + 1;
}

ULONG STDMETHODCALLTYPE MTLD3D12Device::Release() {
  uint32_t rc = --m_refCount;
  if (!rc) {
    uint32_t rp = --m_refPrivate;
    if (!rp) {
      m_refPrivate += 0x80000000;
      delete this;
    }
  }
  return rc;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12Device::GetPrivateData(REFGUID guid, UINT *data_size, void *data) {
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12Device::SetPrivateData(REFGUID guid, UINT data_size,
                               const void *data) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12Device::SetPrivateDataInterface(REFGUID guid, const IUnknown *data) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE MTLD3D12Device::SetName(LPCWSTR name) {
  return S_OK;
}

UINT STDMETHODCALLTYPE MTLD3D12Device::GetNodeCount() { return 1; }

HRESULT STDMETHODCALLTYPE
MTLD3D12Device::CreateCommandQueue(const D3D12_COMMAND_QUEUE_DESC *desc,
                                   REFIID riid, void **command_queue) {
  if (!desc || !command_queue)
    return E_POINTER;
  InitReturnPtr(command_queue);

  auto queue = new MTLD3D12CommandQueue(this, m_device->queue(), *desc);
  HRESULT hr = queue->QueryInterface(riid, command_queue);
  if (FAILED(hr))
    queue->Release();
  return hr;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12Device::CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE type,
                                       REFIID riid,
                                       void **command_allocator) {
  if (!command_allocator)
    return E_POINTER;
  InitReturnPtr(command_allocator);

  auto allocator = new MTLD3D12CommandAllocator(this, type);
  HRESULT hr = allocator->QueryInterface(riid, command_allocator);
  if (FAILED(hr))
    allocator->Release();
  return hr;
}

HRESULT STDMETHODCALLTYPE MTLD3D12Device::CreateGraphicsPipelineState(
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC *desc, REFIID riid,
    void **pipeline_state) {
  Logger::warn("D3D12Device::CreateGraphicsPipelineState: stub");
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE MTLD3D12Device::CreateComputePipelineState(
    const D3D12_COMPUTE_PIPELINE_STATE_DESC *desc, REFIID riid,
    void **pipeline_state) {
  Logger::warn("D3D12Device::CreateComputePipelineState: stub");
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE MTLD3D12Device::CreateCommandList(
    UINT node_mask, D3D12_COMMAND_LIST_TYPE type,
    ID3D12CommandAllocator *command_allocator,
    ID3D12PipelineState *initial_pipeline_state, REFIID riid,
    void **command_list) {
  if (!command_list)
    return E_POINTER;
  InitReturnPtr(command_list);

  auto allocator = static_cast<MTLD3D12CommandAllocator *>(command_allocator);
  auto list = new MTLD3D12GraphicsCommandList(this, allocator, type,
                                              initial_pipeline_state);
  HRESULT hr = list->QueryInterface(riid, command_list);
  if (FAILED(hr))
    list->Release();
  return hr;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12Device::CheckFeatureSupport(D3D12_FEATURE feature,
                                    void *feature_data,
                                    UINT feature_data_size) {
  switch (feature) {
  case D3D12_FEATURE_D3D12_OPTIONS: {
    auto *opts = (D3D12_FEATURE_DATA_D3D12_OPTIONS *)feature_data;
    if (feature_data_size < sizeof(*opts))
      return E_INVALIDARG;
    opts->DoublePrecisionFloatShaderOps = FALSE;
    opts->OutputMergerLogicOp = FALSE;
    opts->MinPrecisionSupport = D3D12_SHADER_MIN_PRECISION_SUPPORT_10_BIT;
    opts->TiledResourcesTier = D3D12_TILED_RESOURCES_TIER_NOT_SUPPORTED;
    opts->ResourceBindingTier = D3D12_RESOURCE_BINDING_TIER_2;
    opts->PSSpecifiedStencilRefSupported = FALSE;
    opts->TypedUAVLoadAdditionalFormats = FALSE;
    opts->ROVsSupported = FALSE;
    opts->ConservativeRasterizationTier = D3D12_CONSERVATIVE_RASTERIZATION_TIER_1;
    opts->MaxGPUVirtualAddressBitsPerResource = 40;
    opts->StandardSwizzle64KBSupported = FALSE;
    opts->CrossNodeSharingTier = D3D12_CROSS_NODE_SHARING_TIER_NOT_SUPPORTED;
    opts->CrossAdapterRowMajorTextureSupported = FALSE;
    opts->VPAndRTArrayIndexFromAnyShaderFeedingRasterizerSupportedWithoutGSEmulation = FALSE;
    opts->ResourceHeapTier = D3D12_RESOURCE_HEAP_TIER_1;
    return S_OK;
  }
  case D3D12_FEATURE_ARCHITECTURE: {
    auto *arch = (D3D12_FEATURE_DATA_ARCHITECTURE *)feature_data;
    if (feature_data_size < sizeof(*arch))
      return E_INVALIDARG;
    arch->NodeIndex = 0;
    arch->TileBasedRenderer = FALSE;
    arch->UMA = TRUE;
    arch->CacheCoherentUMA = TRUE;
    return S_OK;
  }
  case D3D12_FEATURE_FEATURE_LEVELS: {
    auto *fl = (D3D12_FEATURE_DATA_FEATURE_LEVELS *)feature_data;
    if (feature_data_size < sizeof(*fl))
      return E_INVALIDARG;
    fl->MaxSupportedFeatureLevel = D3D_FEATURE_LEVEL_11_0;
    for (UINT i = 0; i < fl->NumFeatureLevels; i++) {
      if (fl->pFeatureLevelsRequested[i] <= D3D_FEATURE_LEVEL_11_0) {
        fl->MaxSupportedFeatureLevel = fl->pFeatureLevelsRequested[i];
        break;
      }
    }
    return S_OK;
  }
  case D3D12_FEATURE_FORMAT_SUPPORT: {
    auto *fmt = (D3D12_FEATURE_DATA_FORMAT_SUPPORT *)feature_data;
    if (feature_data_size < sizeof(*fmt))
      return E_INVALIDARG;
    fmt->Support1 = (D3D12_FORMAT_SUPPORT1)(
        D3D12_FORMAT_SUPPORT1_TEXTURE2D | D3D12_FORMAT_SUPPORT1_RENDER_TARGET |
        D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL |
        D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE);
    return S_OK;
  }
  case D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS: {
    auto *ms =
        (D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS *)feature_data;
    if (feature_data_size < sizeof(*ms))
      return E_INVALIDARG;
    ms->NumQualityLevels = 1;
    return S_OK;
  }
  default:
    Logger::warn(
        str::format("CheckFeatureSupport: unhandled feature ", feature));
    return E_NOTIMPL;
  }
}

HRESULT STDMETHODCALLTYPE
MTLD3D12Device::CreateDescriptorHeap(const D3D12_DESCRIPTOR_HEAP_DESC *desc,
                                     REFIID riid, void **descriptor_heap) {
  Logger::warn("D3D12Device::CreateDescriptorHeap: stub");
  return E_NOTIMPL;
}

UINT STDMETHODCALLTYPE MTLD3D12Device::GetDescriptorHandleIncrementSize(
    D3D12_DESCRIPTOR_HEAP_TYPE descriptor_heap_type) {
  return 64;
}

HRESULT STDMETHODCALLTYPE MTLD3D12Device::CreateRootSignature(
    UINT node_mask, const void *bytecode, SIZE_T bytecode_length,
    REFIID riid, void **root_signature) {
  Logger::warn("D3D12Device::CreateRootSignature: stub");
  return E_NOTIMPL;
}

void STDMETHODCALLTYPE MTLD3D12Device::CreateConstantBufferView(
    const D3D12_CONSTANT_BUFFER_VIEW_DESC *desc,
    D3D12_CPU_DESCRIPTOR_HANDLE descriptor) {}

void STDMETHODCALLTYPE MTLD3D12Device::CreateShaderResourceView(
    ID3D12Resource *resource, const D3D12_SHADER_RESOURCE_VIEW_DESC *desc,
    D3D12_CPU_DESCRIPTOR_HANDLE descriptor) {}

void STDMETHODCALLTYPE MTLD3D12Device::CreateUnorderedAccessView(
    ID3D12Resource *resource, ID3D12Resource *counter_resource,
    const D3D12_UNORDERED_ACCESS_VIEW_DESC *desc,
    D3D12_CPU_DESCRIPTOR_HANDLE descriptor) {}

void STDMETHODCALLTYPE MTLD3D12Device::CreateRenderTargetView(
    ID3D12Resource *resource, const D3D12_RENDER_TARGET_VIEW_DESC *desc,
    D3D12_CPU_DESCRIPTOR_HANDLE descriptor) {}

void STDMETHODCALLTYPE MTLD3D12Device::CreateDepthStencilView(
    ID3D12Resource *resource, const D3D12_DEPTH_STENCIL_VIEW_DESC *desc,
    D3D12_CPU_DESCRIPTOR_HANDLE descriptor) {}

void STDMETHODCALLTYPE
MTLD3D12Device::CreateSampler(const D3D12_SAMPLER_DESC *desc,
                              D3D12_CPU_DESCRIPTOR_HANDLE descriptor) {}

void STDMETHODCALLTYPE MTLD3D12Device::CopyDescriptors(
    UINT dst_descriptor_range_count,
    const D3D12_CPU_DESCRIPTOR_HANDLE *dst_descriptor_range_offsets,
    const UINT *dst_descriptor_range_sizes,
    UINT src_descriptor_range_count,
    const D3D12_CPU_DESCRIPTOR_HANDLE *src_descriptor_range_offsets,
    const UINT *src_descriptor_range_sizes,
    D3D12_DESCRIPTOR_HEAP_TYPE descriptor_heap_type) {}

void STDMETHODCALLTYPE MTLD3D12Device::CopyDescriptorsSimple(
    UINT descriptor_count,
    const D3D12_CPU_DESCRIPTOR_HANDLE dst_descriptor_range_offset,
    const D3D12_CPU_DESCRIPTOR_HANDLE src_descriptor_range_offset,
    D3D12_DESCRIPTOR_HEAP_TYPE descriptor_heap_type) {}

D3D12_RESOURCE_ALLOCATION_INFO* STDMETHODCALLTYPE
MTLD3D12Device::GetResourceAllocationInfo(
    D3D12_RESOURCE_ALLOCATION_INFO *__ret, UINT visible_mask,
    UINT resource_desc_count,
    const D3D12_RESOURCE_DESC *resource_descs) {
  __ret->Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
  __ret->SizeInBytes = 0;
  for (UINT i = 0; i < resource_desc_count; i++) {
    if (resource_descs[i].Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
      __ret->SizeInBytes += resource_descs[i].Width;
    } else {
      __ret->SizeInBytes +=
          resource_descs[i].Width * resource_descs[i].Height *
          resource_descs[i].DepthOrArraySize;
    }
  }
  return __ret;
}

D3D12_HEAP_PROPERTIES* STDMETHODCALLTYPE
MTLD3D12Device::GetCustomHeapProperties(D3D12_HEAP_PROPERTIES *__ret,
                                        UINT node_mask,
                                        D3D12_HEAP_TYPE heap_type) {
  __ret->Type = heap_type;
  __ret->CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  __ret->MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  __ret->CreationNodeMask = 1;
  __ret->VisibleNodeMask = 1;
  return __ret;
}

HRESULT STDMETHODCALLTYPE MTLD3D12Device::CreateCommittedResource(
    const D3D12_HEAP_PROPERTIES *heap_properties, D3D12_HEAP_FLAGS heap_flags,
    const D3D12_RESOURCE_DESC *desc, D3D12_RESOURCE_STATES initial_state,
    const D3D12_CLEAR_VALUE *optimized_clear_value, REFIID riid,
    void **resource) {
  Logger::warn("D3D12Device::CreateCommittedResource: stub");
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12Device::CreateHeap(const D3D12_HEAP_DESC *desc, REFIID riid,
                           void **heap) {
  Logger::warn("D3D12Device::CreateHeap: stub");
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE MTLD3D12Device::CreatePlacedResource(
    ID3D12Heap *heap, UINT64 heap_offset, const D3D12_RESOURCE_DESC *desc,
    D3D12_RESOURCE_STATES initial_state,
    const D3D12_CLEAR_VALUE *optimized_clear_value, REFIID riid,
    void **resource) {
  Logger::warn("D3D12Device::CreatePlacedResource: stub");
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE MTLD3D12Device::CreateReservedResource(
    const D3D12_RESOURCE_DESC *desc, D3D12_RESOURCE_STATES initial_state,
    const D3D12_CLEAR_VALUE *optimized_clear_value, REFIID riid,
    void **resource) {
  Logger::warn("D3D12Device::CreateReservedResource: stub");
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE MTLD3D12Device::CreateSharedHandle(
    ID3D12DeviceChild *object, const SECURITY_ATTRIBUTES *attributes,
    DWORD access, const WCHAR *name, HANDLE *handle) {
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12Device::OpenSharedHandle(HANDLE handle, REFIID riid, void **object) {
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12Device::OpenSharedHandleByName(const WCHAR *name, DWORD access,
                                       HANDLE *handle) {
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12Device::MakeResident(UINT object_count,
                             ID3D12Pageable *const *objects) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12Device::Evict(UINT object_count, ID3D12Pageable *const *objects) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12Device::CreateFence(UINT64 initial_value, D3D12_FENCE_FLAGS flags,
                            REFIID riid, void **fence) {
  Logger::warn("D3D12Device::CreateFence: stub");
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE MTLD3D12Device::GetDeviceRemovedReason() {
  return S_OK;
}

void STDMETHODCALLTYPE MTLD3D12Device::GetCopyableFootprints(
    const D3D12_RESOURCE_DESC *desc, UINT first_sub_resource,
    UINT sub_resource_count, UINT64 base_offset,
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT *layouts, UINT *row_count,
    UINT64 *row_size, UINT64 *total_bytes) {
  if (total_bytes)
    *total_bytes = 0;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12Device::CreateQueryHeap(const D3D12_QUERY_HEAP_DESC *desc,
                                REFIID riid, void **heap) {
  Logger::warn("D3D12Device::CreateQueryHeap: stub");
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12Device::SetStablePowerState(WINBOOL enable) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE MTLD3D12Device::CreateCommandSignature(
    const D3D12_COMMAND_SIGNATURE_DESC *desc,
    ID3D12RootSignature *root_signature, REFIID riid,
    void **command_signature) {
  Logger::warn("D3D12Device::CreateCommandSignature: stub");
  return E_NOTIMPL;
}

void STDMETHODCALLTYPE MTLD3D12Device::GetResourceTiling(
    ID3D12Resource *resource, UINT *total_tile_count,
    D3D12_PACKED_MIP_INFO *packed_mip_info,
    D3D12_TILE_SHAPE *standard_tile_shape,
    UINT *sub_resource_tiling_count, UINT first_sub_resource_tiling,
    D3D12_SUBRESOURCE_TILING *sub_resource_tilings) {}

LUID* STDMETHODCALLTYPE MTLD3D12Device::GetAdapterLuid(LUID *__ret) {
  *__ret = {};
  return __ret;
}

} // namespace dxmt
