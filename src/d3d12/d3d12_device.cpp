#define INITGUID
#include "d3d12_command_queue.hpp"
#include "d3d12_command_allocator.hpp"
#include "d3d12_command_list.hpp"
#include "d3d12_descriptor_heap.hpp"
#include "d3d12_device.hpp"
#include "d3d12_fence.hpp"
#include "d3d12_pipeline_state.hpp"
#include "d3d12_query_heap.hpp"
#include "d3d12_resource.hpp"

#define TRACE(fmt, ...) do { FILE *_tf = fopen("Z:\\tmp\\dxmt_dxgi_trace.log", "a"); if (_tf) { fprintf(_tf, fmt "\n", ##__VA_ARGS__); fclose(_tf); } } while(0)
#include "d3d12_root_signature.hpp"
#include "com/com_object.hpp"
#include "log/log.hpp"
#include "util_string.hpp"
#include "d3d12_resource.hpp"
#include <d3d12.h>

namespace dxmt {

static const GUID IID_ID3D12Device2_ = {0x30baa41e, 0xb15b, 0x475c, {0xa0, 0xbb, 0x1a, 0xf5, 0xc5, 0xb6, 0x43, 0x28}};
static const GUID IID_ID3D12Device3_ = {0x81dadc15, 0x2bad, 0x4392, {0x93, 0xc5, 0x10, 0x13, 0x45, 0xc4, 0xaa, 0x98}};
static const GUID IID_ID3D12Device4_ = {0xe865df17, 0xa9ee, 0x46f9, {0xa4, 0x63, 0x30, 0x98, 0x31, 0x5a, 0xa2, 0xe5}};
static const GUID IID_ID3D12Device5_ = {0x8b4f173b, 0x2fea, 0x4b80, {0x8f, 0x58, 0x43, 0x07, 0x19, 0x1a, 0xb9, 0x5d}};
static const GUID IID_ID3D12Device6_ = {0xc70b221b, 0x40e4, 0x4a17, {0x89, 0xaf, 0x02, 0x5a, 0x07, 0x27, 0xa6, 0xdc}};
static const GUID IID_ID3D12Device7_ = {0x5c014b53, 0x68a1, 0x4b9b, {0x8b, 0xd1, 0xdd, 0x60, 0x46, 0xb9, 0x35, 0x8b}};
static const GUID IID_ID3D12Device8_ = {0x9218e6bb, 0xf944, 0x4f7e, {0xa7, 0x5c, 0xb1, 0xb2, 0xc7, 0xb7, 0x01, 0xf3}};
static const GUID IID_ID3D12Device9_ = {0x4c80e962, 0xf032, 0x4f60, {0xbc, 0x9e, 0xeb, 0xc2, 0xcf, 0xa1, 0xd8, 0x3c}};
static const GUID IID_ID3D12Device10_ = {0x517f8718, 0xaa66, 0x49f9, {0xb0, 0x2b, 0xa7, 0xab, 0x89, 0xc0, 0x60, 0x31}};
static const GUID IID_ID3D12Device11_ = {0x5405c344, 0xd457, 0x444e, {0xb4, 0xdd, 0x23, 0x66, 0xe4, 0x5a, 0xee, 0x39}};
static const GUID IID_ID3D12Device12_ = {0x5af5c532, 0x4c91, 0x4cd0, {0xb5, 0x41, 0x15, 0xa4, 0x05, 0x39, 0x5f, 0xc5}};

Logger Logger::s_instance("d3d12.log");

class MTLD3D12CommandSignature : public ComObject<ID3D12CommandSignature> {
public:
  MTLD3D12CommandSignature(MTLD3D12Device *device, const D3D12_COMMAND_SIGNATURE_DESC &desc)
      : m_device(device), m_desc(desc) { m_device->AddRef(); }
  ~MTLD3D12CommandSignature() { m_device->Release(); }
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) override {
    if (!ppv) return E_POINTER;
    if (riid == IID_IUnknown || riid == IID_ID3D12Object ||
        riid == IID_ID3D12DeviceChild || riid == IID_ID3D12CommandSignature) {
      *ppv = ref(this); return S_OK;
    }
    return E_NOINTERFACE;
  }
  HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID, UINT *, void *) override { return E_NOTIMPL; }
  HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID, UINT, const void *) override { return S_OK; }
  HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID, const IUnknown *) override { return S_OK; }
  HRESULT STDMETHODCALLTYPE SetName(LPCWSTR) override { return S_OK; }
  HRESULT STDMETHODCALLTYPE GetDevice(REFIID riid, void **device) override { return m_device->QueryInterface(riid, device); }
private:
  MTLD3D12Device *m_device;
  D3D12_COMMAND_SIGNATURE_DESC m_desc;
};

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
      riid == IID_ID3D12Device || riid == IID_ID3D12Device1 ||
      riid == IID_ID3D12Device2_ || riid == IID_ID3D12Device3_ ||
      riid == IID_ID3D12Device4_ || riid == IID_ID3D12Device5_ ||
      riid == IID_ID3D12Device6_ || riid == IID_ID3D12Device7_ ||
      riid == IID_ID3D12Device8_ || riid == IID_ID3D12Device9_ ||
      riid == IID_ID3D12Device10_ || riid == IID_ID3D12Device11_ ||
      riid == IID_ID3D12Device12_) {
    *ppvObject = ref(this);
    return S_OK;
  }

  if (riid == __uuidof(IMTLDXGIDevice) && m_dxgi_device) {
    return m_dxgi_device->QueryInterface(riid, ppvObject);
  }

  Logger::warn(str::format("D3D12Device::QueryInterface: unknown IID ", riid));
  TRACE("D3D12Device::QI unknown IID %s -> E_NOINTERFACE", str::format(riid).c_str());
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
  TRACE("GetPrivateData E_NOTIMPL");
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
  TRACE("CreateCommandQueue type=%u", desc ? desc->Type : 0xFF);
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
  TRACE("CreateCommandAllocator type=%u", type);
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
  if (!desc || !pipeline_state)
    return E_POINTER;
  InitReturnPtr(pipeline_state);

  auto pso = new MTLD3D12PipelineState(this, false);
  pso->SetGraphicsDesc(*desc);
  bool compiled = pso->Compile();
  TRACE("CreateGraphicsPSO: compile=%d VS=%p PS=%p", compiled, desc->VS.pShaderBytecode, desc->PS.pShaderBytecode);
  if (!compiled) {
    Logger::warn("CreateGraphicsPipelineState: shader compilation deferred/failed");
  }
  HRESULT hr = pso->QueryInterface(riid, pipeline_state);
  if (FAILED(hr))
    pso->Release();
  return hr;
}

HRESULT STDMETHODCALLTYPE MTLD3D12Device::CreateComputePipelineState(
    const D3D12_COMPUTE_PIPELINE_STATE_DESC *desc, REFIID riid,
    void **pipeline_state) {
  if (!desc || !pipeline_state)
    return E_POINTER;
  InitReturnPtr(pipeline_state);

  auto pso = new MTLD3D12PipelineState(this, true);
  pso->SetComputeDesc(*desc);
  bool compiled = pso->Compile();
  TRACE("CreateComputePSO: compile=%d CS=%p", compiled, desc->CS.pShaderBytecode);
  if (!compiled) {
    Logger::warn("CreateComputePipelineState: shader compilation deferred/failed");
  }
  HRESULT hr = pso->QueryInterface(riid, pipeline_state);
  if (FAILED(hr))
    pso->Release();
  return hr;
}

HRESULT STDMETHODCALLTYPE MTLD3D12Device::CreateCommandList(
    UINT node_mask, D3D12_COMMAND_LIST_TYPE type,
    ID3D12CommandAllocator *command_allocator,
    ID3D12PipelineState *initial_pipeline_state, REFIID riid,
    void **command_list) {
  TRACE("CreateCommandList type=%u", type);
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
  TRACE("CheckFeatureSupport feature=%u size=%u", feature, feature_data_size);
  switch (feature) {
  case D3D12_FEATURE_D3D12_OPTIONS: {
    auto *opts = (D3D12_FEATURE_DATA_D3D12_OPTIONS *)feature_data;
    if (feature_data_size < sizeof(*opts))
      return E_INVALIDARG;
    opts->DoublePrecisionFloatShaderOps = FALSE;
    opts->OutputMergerLogicOp = TRUE;
    opts->MinPrecisionSupport = D3D12_SHADER_MIN_PRECISION_SUPPORT_10_BIT;
    opts->TiledResourcesTier = D3D12_TILED_RESOURCES_TIER_2;
    opts->ResourceBindingTier = D3D12_RESOURCE_BINDING_TIER_3;
    opts->PSSpecifiedStencilRefSupported = TRUE;
    opts->TypedUAVLoadAdditionalFormats = TRUE;
    opts->ROVsSupported = TRUE;
    opts->ConservativeRasterizationTier = D3D12_CONSERVATIVE_RASTERIZATION_TIER_3;
    opts->MaxGPUVirtualAddressBitsPerResource = 40;
    opts->StandardSwizzle64KBSupported = FALSE;
    opts->CrossNodeSharingTier = D3D12_CROSS_NODE_SHARING_TIER_NOT_SUPPORTED;
    opts->CrossAdapterRowMajorTextureSupported = FALSE;
    opts->VPAndRTArrayIndexFromAnyShaderFeedingRasterizerSupportedWithoutGSEmulation = TRUE;
    opts->ResourceHeapTier = D3D12_RESOURCE_HEAP_TIER_2;
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
    fl->MaxSupportedFeatureLevel = D3D_FEATURE_LEVEL_9_1;
    for (UINT i = 0; i < fl->NumFeatureLevels; i++) {
      if (fl->pFeatureLevelsRequested[i] <= D3D_FEATURE_LEVEL_12_1 &&
          fl->pFeatureLevelsRequested[i] > fl->MaxSupportedFeatureLevel) {
        fl->MaxSupportedFeatureLevel = fl->pFeatureLevelsRequested[i];
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
        D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE |
        D3D12_FORMAT_SUPPORT1_SHADER_LOAD |
        D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE_COMPARISON |
        D3D12_FORMAT_SUPPORT1_BUFFER |
        D3D12_FORMAT_SUPPORT1_IA_INDEX_BUFFER |
        D3D12_FORMAT_SUPPORT1_IA_VERTEX_BUFFER);
    fmt->Support2 = D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD | D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE;
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
  case D3D12_FEATURE_FORMAT_INFO: {
    auto *fi = (D3D12_FEATURE_DATA_FORMAT_INFO *)feature_data;
    if (feature_data_size < sizeof(*fi))
      return E_INVALIDARG;
    fi->PlaneCount = 1;
    return S_OK;
  }
  case D3D12_FEATURE_GPU_VIRTUAL_ADDRESS_SUPPORT: {
    auto *va = (D3D12_FEATURE_DATA_GPU_VIRTUAL_ADDRESS_SUPPORT *)feature_data;
    if (feature_data_size < sizeof(*va))
      return E_INVALIDARG;
    va->MaxGPUVirtualAddressBitsPerResource = 40;
    va->MaxGPUVirtualAddressBitsPerProcess = 40;
    return S_OK;
  }
  case D3D12_FEATURE_SHADER_MODEL: {
    auto *sm = (D3D12_FEATURE_DATA_SHADER_MODEL *)feature_data;
    if (feature_data_size < sizeof(*sm))
      return E_INVALIDARG;
    sm->HighestShaderModel = D3D_SHADER_MODEL_6_5;
    return S_OK;
  }
  case D3D12_FEATURE_D3D12_OPTIONS1: {
    auto *o = (D3D12_FEATURE_DATA_D3D12_OPTIONS1 *)feature_data;
    if (feature_data_size < sizeof(*o))
      return E_INVALIDARG;
    o->WaveOps = TRUE;
    o->WaveLaneCountMin = 4;
    o->WaveLaneCountMax = 64;
    o->TotalLaneCount = 256;
    o->ExpandedComputeResourceStates = FALSE;
    o->Int64ShaderOps = TRUE;
    return S_OK;
  }
  case D3D12_FEATURE_ROOT_SIGNATURE: {
    auto *rs = (D3D12_FEATURE_DATA_ROOT_SIGNATURE *)feature_data;
    if (feature_data_size < sizeof(*rs))
      return E_INVALIDARG;
    rs->HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
    return S_OK;
  }
  case D3D12_FEATURE_ARCHITECTURE1: {
    auto *a = (D3D12_FEATURE_DATA_ARCHITECTURE1 *)feature_data;
    if (feature_data_size < sizeof(*a))
      return E_INVALIDARG;
    a->NodeIndex = 0;
    a->TileBasedRenderer = FALSE;
    a->UMA = TRUE;
    a->CacheCoherentUMA = TRUE;
    a->IsolatedMMU = FALSE;
    return S_OK;
  }
  case D3D12_FEATURE_D3D12_OPTIONS2: {
    auto *o = (D3D12_FEATURE_DATA_D3D12_OPTIONS2 *)feature_data;
    if (feature_data_size < sizeof(*o))
      return E_INVALIDARG;
    o->DepthBoundsTestSupported = TRUE;
    o->ProgrammableSamplePositionsTier = D3D12_PROGRAMMABLE_SAMPLE_POSITIONS_TIER_1;
    return S_OK;
  }
  case D3D12_FEATURE_SHADER_CACHE: {
    auto *sc = (D3D12_FEATURE_DATA_SHADER_CACHE *)feature_data;
    if (feature_data_size < sizeof(*sc))
      return E_INVALIDARG;
    sc->SupportFlags = D3D12_SHADER_CACHE_SUPPORT_NONE;
    return S_OK;
  }
  case D3D12_FEATURE_D3D12_OPTIONS3: {
    auto *o = (D3D12_FEATURE_DATA_D3D12_OPTIONS3 *)feature_data;
    if (feature_data_size < sizeof(*o))
      return E_INVALIDARG;
    o->CopyQueueTimestampQueriesSupported = FALSE;
    o->CastingFullyTypedFormatSupported = TRUE;
    o->WriteBufferImmediateSupportFlags = D3D12_COMMAND_LIST_SUPPORT_FLAG_DIRECT;
    o->ViewInstancingTier = D3D12_VIEW_INSTANCING_TIER_NOT_SUPPORTED;
    o->BarycentricsSupported = FALSE;
    return S_OK;
  }
  case D3D12_FEATURE_D3D12_OPTIONS4: {
    auto *o = (D3D12_FEATURE_DATA_D3D12_OPTIONS4 *)feature_data;
    if (feature_data_size < sizeof(*o))
      return E_INVALIDARG;
    o->MSAA64KBAlignedTextureSupported = FALSE;
    o->SharedResourceCompatibilityTier = D3D12_SHARED_RESOURCE_COMPATIBILITY_TIER_0;
    o->Native16BitShaderOpsSupported = FALSE;
    return S_OK;
  }
  case D3D12_FEATURE_SERIALIZATION: {
    auto *s = (D3D12_FEATURE_DATA_SERIALIZATION *)feature_data;
    if (feature_data_size < sizeof(*s))
      return E_INVALIDARG;
    s->HeapSerializationTier = D3D12_HEAP_SERIALIZATION_TIER_0;
    return S_OK;
  }
  case D3D12_FEATURE_D3D12_OPTIONS5: {
    auto *o = (D3D12_FEATURE_DATA_D3D12_OPTIONS5 *)feature_data;
    if (feature_data_size < sizeof(*o))
      return E_INVALIDARG;
    o->SRVOnlyTiledResourceTier3 = FALSE;
    o->RenderPassesTier = D3D12_RENDER_PASS_TIER_0;
    return S_OK;
  }
  case D3D12_FEATURE_D3D12_OPTIONS6: {
    auto *o = (D3D12_FEATURE_DATA_D3D12_OPTIONS6 *)feature_data;
    if (feature_data_size < sizeof(*o))
      return E_INVALIDARG;
    o->AdditionalShadingRatesSupported = FALSE;
    o->PerPrimitiveShadingRateSupportedWithViewportIndexing = FALSE;
    o->VariableShadingRateTier = D3D12_VARIABLE_SHADING_RATE_TIER_NOT_SUPPORTED;
    o->ShadingRateImageTileSize = 0;
    o->BackgroundProcessingSupported = FALSE;
    return S_OK;
  }
  case D3D12_FEATURE_D3D12_OPTIONS7: {
    auto *o = (D3D12_FEATURE_DATA_D3D12_OPTIONS7 *)feature_data;
    if (feature_data_size < sizeof(*o))
      return E_INVALIDARG;
    o->MeshShaderTier = D3D12_MESH_SHADER_TIER_NOT_SUPPORTED;
    o->SamplerFeedbackTier = D3D12_SAMPLER_FEEDBACK_TIER_NOT_SUPPORTED;
    return S_OK;
  }
  case D3D12_FEATURE_D3D12_OPTIONS8: {
    auto *o = (D3D12_FEATURE_DATA_D3D12_OPTIONS8 *)feature_data;
    if (feature_data_size < sizeof(*o))
      return E_INVALIDARG;
    o->UnalignedBlockTexturesSupported = FALSE;
    return S_OK;
  }
  case D3D12_FEATURE_D3D12_OPTIONS9: {
    auto *o = (D3D12_FEATURE_DATA_D3D12_OPTIONS9 *)feature_data;
    if (feature_data_size < sizeof(*o))
      return E_INVALIDARG;
    o->MeshShaderPipelineStatsSupported = FALSE;
    o->MeshShaderSupportsFullRangeRenderTargetArrayIndex = FALSE;
    o->AtomicInt64OnTypedResourceSupported = FALSE;
    o->AtomicInt64OnGroupSharedSupported = FALSE;
    o->DerivativesInMeshAndAmplificationShadersSupported = FALSE;
    return S_OK;
  }
  case D3D12_FEATURE_D3D12_OPTIONS10: {
    auto *o = (D3D12_FEATURE_DATA_D3D12_OPTIONS10 *)feature_data;
    if (feature_data_size < sizeof(*o))
      return E_INVALIDARG;
    o->VariableRateShadingSumCombinerSupported = FALSE;
    o->MeshShaderPerPrimitiveShadingRateSupported = FALSE;
    return S_OK;
  }
  case D3D12_FEATURE_D3D12_OPTIONS11: {
    auto *o = (D3D12_FEATURE_DATA_D3D12_OPTIONS11 *)feature_data;
    if (feature_data_size < sizeof(*o))
      return E_INVALIDARG;
    o->AtomicInt64OnDescriptorHeapResourceSupported = FALSE;
    return S_OK;
  }
  default:
    TRACE("CheckFeatureSupport UNHANDLED feature=%u size=%u -> E_NOTIMPL", feature, feature_data_size);
    Logger::warn(
        str::format("CheckFeatureSupport: unhandled feature ", feature));
    return E_NOTIMPL;
  }
}

HRESULT STDMETHODCALLTYPE
MTLD3D12Device::CreateDescriptorHeap(const D3D12_DESCRIPTOR_HEAP_DESC *desc,
                                      REFIID riid, void **descriptor_heap) {
  TRACE("CreateDescriptorHeap type=%u num=%u", desc ? desc->Type : 0xFF, desc ? desc->NumDescriptors : 0);
  if (!desc || !descriptor_heap)
    return E_POINTER;
  InitReturnPtr(descriptor_heap);

  MTLD3D12DescriptorHeap *heap;
  try {
    heap = new MTLD3D12DescriptorHeap(this, *desc);
  } catch (const std::bad_alloc &) {
    TRACE("CreateDescriptorHeap: E_OUTOFMEMORY (bad_alloc for %u descriptors)", desc->NumDescriptors);
    return E_OUTOFMEMORY;
  }
  HRESULT hr = heap->QueryInterface(riid, descriptor_heap);
  if (FAILED(hr))
    heap->Release();
  return hr;
}

UINT STDMETHODCALLTYPE MTLD3D12Device::GetDescriptorHandleIncrementSize(
    D3D12_DESCRIPTOR_HEAP_TYPE descriptor_heap_type) {
  return sizeof(D3D12Descriptor);
}

HRESULT STDMETHODCALLTYPE MTLD3D12Device::CreateRootSignature(
    UINT node_mask, const void *bytecode, SIZE_T bytecode_length,
    REFIID riid, void **root_signature) {
  TRACE("CreateRootSignature len=%llu", (unsigned long long)bytecode_length);
  if (!bytecode || !root_signature)
    return E_POINTER;
  InitReturnPtr(root_signature);

  auto rs =
      new MTLD3D12RootSignature(this, bytecode, bytecode_length);
  HRESULT hr = rs->QueryInterface(riid, root_signature);
  if (FAILED(hr))
    rs->Release();
  return hr;
}

void STDMETHODCALLTYPE MTLD3D12Device::CreateConstantBufferView(
    const D3D12_CONSTANT_BUFFER_VIEW_DESC *desc,
    D3D12_CPU_DESCRIPTOR_HANDLE descriptor) {
  TRACE("CreateConstantBufferView");
  if (!desc)
    return;
  auto *d = reinterpret_cast<D3D12Descriptor *>(descriptor.ptr);
  if (d) {
    d->cbv = *desc;
    d->type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  }
}

void STDMETHODCALLTYPE MTLD3D12Device::CreateShaderResourceView(
    ID3D12Resource *resource, const D3D12_SHADER_RESOURCE_VIEW_DESC *desc,
    D3D12_CPU_DESCRIPTOR_HANDLE descriptor) {
  TRACE("CreateShaderResourceView");
  auto *d = reinterpret_cast<D3D12Descriptor *>(descriptor.ptr);
  if (d) {
    d->resource = resource;
    if (desc)
      d->srv = *desc;
    d->type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  }
}

void STDMETHODCALLTYPE MTLD3D12Device::CreateUnorderedAccessView(
    ID3D12Resource *resource, ID3D12Resource *counter_resource,
    const D3D12_UNORDERED_ACCESS_VIEW_DESC *desc,
    D3D12_CPU_DESCRIPTOR_HANDLE descriptor) {
  TRACE("CreateUnorderedAccessView");
  auto *d = reinterpret_cast<D3D12Descriptor *>(descriptor.ptr);
  if (d) {
    d->resource = resource;
    d->resource_uav_counter = counter_resource;
    if (desc)
      d->uav = *desc;
    d->type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  }
}

void STDMETHODCALLTYPE MTLD3D12Device::CreateRenderTargetView(
    ID3D12Resource *resource, const D3D12_RENDER_TARGET_VIEW_DESC *desc,
    D3D12_CPU_DESCRIPTOR_HANDLE descriptor) {
  TRACE("CreateRenderTargetView");
  auto *d = reinterpret_cast<D3D12Descriptor *>(descriptor.ptr);
  if (d) {
    d->resource = resource;
    if (desc)
      d->rtv = *desc;
    d->type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  }
}

void STDMETHODCALLTYPE MTLD3D12Device::CreateDepthStencilView(
    ID3D12Resource *resource, const D3D12_DEPTH_STENCIL_VIEW_DESC *desc,
    D3D12_CPU_DESCRIPTOR_HANDLE descriptor) {
  TRACE("CreateDepthStencilView");
  auto *d = reinterpret_cast<D3D12Descriptor *>(descriptor.ptr);
  if (d) {
    d->resource = resource;
    if (desc)
      d->dsv = *desc;
    d->type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
  }
}

void STDMETHODCALLTYPE
MTLD3D12Device::CreateSampler(const D3D12_SAMPLER_DESC *desc,
                              D3D12_CPU_DESCRIPTOR_HANDLE descriptor) {
  auto *d = reinterpret_cast<D3D12Descriptor *>(descriptor.ptr);
  if (d && desc) {
    d->sampler = *desc;
    d->type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
  }
}

void STDMETHODCALLTYPE MTLD3D12Device::CopyDescriptors(
    UINT dst_descriptor_range_count,
    const D3D12_CPU_DESCRIPTOR_HANDLE *dst_descriptor_range_offsets,
    const UINT *dst_descriptor_range_sizes,
    UINT src_descriptor_range_count,
    const D3D12_CPU_DESCRIPTOR_HANDLE *src_descriptor_range_offsets,
    const UINT *src_descriptor_range_sizes,
    D3D12_DESCRIPTOR_HEAP_TYPE descriptor_heap_type) {
  UINT src_idx = 0;
  for (UINT dst_range = 0; dst_range < dst_descriptor_range_count; dst_range++) {
    for (UINT i = 0; i < dst_descriptor_range_sizes[dst_range]; i++) {
      auto *dst = reinterpret_cast<D3D12Descriptor *>(
          dst_descriptor_range_offsets[dst_range].ptr) +
                  i * (GetDescriptorHandleIncrementSize(descriptor_heap_type) /
                       sizeof(D3D12Descriptor));
      if (src_idx < src_descriptor_range_count) {
        auto *src = reinterpret_cast<D3D12Descriptor *>(
            src_descriptor_range_offsets[src_idx].ptr) +
                    i * (GetDescriptorHandleIncrementSize(descriptor_heap_type) /
                         sizeof(D3D12Descriptor));
        *dst = *src;
      }
    }
    src_idx++;
  }
}

void STDMETHODCALLTYPE MTLD3D12Device::CopyDescriptorsSimple(
    UINT descriptor_count,
    const D3D12_CPU_DESCRIPTOR_HANDLE dst_descriptor_range_offset,
    const D3D12_CPU_DESCRIPTOR_HANDLE src_descriptor_range_offset,
    D3D12_DESCRIPTOR_HEAP_TYPE descriptor_heap_type) {
  CopyDescriptors(1, &dst_descriptor_range_offset, &descriptor_count, 1,
                  &src_descriptor_range_offset, &descriptor_count,
                  descriptor_heap_type);
}

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
  TRACE("CreateCommittedResource dim=%u fmt=%u width=%llu state=0x%x heap_type=%u", desc ? desc->Dimension : 0xFF, desc ? desc->Format : 0, desc ? desc->Width : 0, initial_state, heap_properties ? heap_properties->Type : 0xFF);
  if (!desc || !resource)
    return E_POINTER;
  InitReturnPtr(resource);

  auto res = new MTLD3D12Resource(this, *desc, initial_state,
                                  heap_properties ? *heap_properties
                                                  : D3D12_HEAP_PROPERTIES{});
  HRESULT hr = res->QueryInterface(riid, resource);
  TRACE("CreateCommittedResource QI hr=0x%lx", hr);
  if (FAILED(hr))
    res->Release();
  return hr;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12Device::CreateHeap(const D3D12_HEAP_DESC *desc, REFIID riid,
                           void **heap) {
  Logger::warn("D3D12Device::CreateHeap: stub");
  TRACE("CreateHeap E_NOTIMPL");
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE MTLD3D12Device::CreatePlacedResource(
    ID3D12Heap *heap, UINT64 heap_offset, const D3D12_RESOURCE_DESC *desc,
    D3D12_RESOURCE_STATES initial_state,
    const D3D12_CLEAR_VALUE *optimized_clear_value, REFIID riid,
    void **resource) {
  Logger::warn("D3D12Device::CreatePlacedResource: stub");
  TRACE("CreatePlacedResource E_NOTIMPL");
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE MTLD3D12Device::CreateReservedResource(
    const D3D12_RESOURCE_DESC *desc, D3D12_RESOURCE_STATES initial_state,
    const D3D12_CLEAR_VALUE *optimized_clear_value, REFIID riid,
    void **resource) {
  TRACE("CreateReservedResource dim=%u fmt=%u w=%llu (fallback to committed)", desc ? desc->Dimension : 0, desc ? desc->Format : 0, desc ? desc->Width : 0);
  if (!desc || !resource)
    return E_POINTER;
  InitReturnPtr(resource);
  D3D12_HEAP_PROPERTIES heap_props = {};
  heap_props.Type = D3D12_HEAP_TYPE_DEFAULT;
  auto res = new MTLD3D12Resource(this, *desc, initial_state, heap_props);
  HRESULT hr = res->QueryInterface(riid, resource);
  if (FAILED(hr))
    res->Release();
  return hr;
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
  if (!fence)
    return E_POINTER;
  InitReturnPtr(fence);

  auto f = new MTLD3D12Fence(this, initial_value, flags);
  TRACE("CreateFence init=%llu fence=%p", (unsigned long long)initial_value, (void *)f);
  HRESULT hr = f->QueryInterface(riid, fence);
  if (FAILED(hr))
    f->Release();
  return hr;
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
  if (!desc || !heap)
    return E_POINTER;
  InitReturnPtr(heap);

  auto qh = new MTLD3D12QueryHeap(this, *desc);
  HRESULT hr = qh->QueryInterface(riid, heap);
  if (FAILED(hr))
    qh->Release();
  return hr;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12Device::SetStablePowerState(WINBOOL enable) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE MTLD3D12Device::CreateCommandSignature(
    const D3D12_COMMAND_SIGNATURE_DESC *desc,
    ID3D12RootSignature *root_signature, REFIID riid,
     void **command_signature) {
  if (!command_signature)
    return E_POINTER;
  InitReturnPtr(command_signature);
  TRACE("CreateCommandSignature stride=%u num_args=%u", desc ? desc->ByteStride : 0, desc ? desc->NumArgumentDescs : 0);
  if (!desc)
    return E_INVALIDARG;
  auto *obj = new MTLD3D12CommandSignature(this, *desc);
  HRESULT hr = obj->QueryInterface(riid, command_signature);
  if (FAILED(hr))
    obj->Release();
  return hr;
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

void MTLD3D12Device::RegisterResource(MTLD3D12Resource *res) {
  if (!res) return;
  D3D12_GPU_VIRTUAL_ADDRESS addr = res->GetGPUVirtualAddress();
  if (addr) {
    std::lock_guard<std::mutex> lock(m_resource_mutex);
    m_resources_by_gpu_addr[addr] = res;
  }
}

void MTLD3D12Device::UnregisterResource(MTLD3D12Resource *res) {
  if (!res) return;
  D3D12_GPU_VIRTUAL_ADDRESS addr = res->GetGPUVirtualAddress();
  if (addr) {
    std::lock_guard<std::mutex> lock(m_resource_mutex);
    m_resources_by_gpu_addr.erase(addr);
  }
}

MTLD3D12Resource *MTLD3D12Device::LookupResourceByGPUAddress(D3D12_GPU_VIRTUAL_ADDRESS addr) {
  if (!addr) return nullptr;
  std::lock_guard<std::mutex> lock(m_resource_mutex);
  auto it = m_resources_by_gpu_addr.find(addr);
  if (it != m_resources_by_gpu_addr.end())
    return it->second;
  for (auto &[gpu_addr, res] : m_resources_by_gpu_addr) {
    auto *desc = res->GetDesc({});
    if (desc && desc->Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
      if (addr >= gpu_addr && addr < gpu_addr + desc->Width)
        return res;
    }
  }
  return nullptr;
}

HRESULT STDMETHODCALLTYPE MTLD3D12Device::CreatePipelineLibrary(
    const void *blob, SIZE_T blob_size, REFIID riid, void **lib) {
  TRACE("CreatePipelineLibrary -> DXGI_ERROR_UNSUPPORTED");
  if (lib) *lib = nullptr;
  return DXGI_ERROR_UNSUPPORTED;
}

HRESULT STDMETHODCALLTYPE MTLD3D12Device::SetEventOnMultipleFenceCompletion(
    ID3D12Fence *const *fences, const UINT64 *values, UINT fence_count,
    D3D12_MULTIPLE_FENCE_WAIT_FLAGS flags, HANDLE event) {
  TRACE("SetEventOnMultipleFenceCompletion -> E_NOTIMPL");
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE MTLD3D12Device::SetResidencyPriority(
    UINT object_count, ID3D12Pageable *const *objects,
    const D3D12_RESIDENCY_PRIORITY *priorities) {
  return S_OK;
}

} // namespace dxmt
