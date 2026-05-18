#define INITGUID
#include "d3d12_command_queue.hpp"
#include "d3d12_command_allocator.hpp"
#include "d3d12_command_list.hpp"
#include "d3d12_descriptor_heap.hpp"
#include "d3d12_device.hpp"
#include "d3d12_fence.hpp"
#include "d3d12_heap.hpp"
#include "d3d12_pipeline_state.hpp"
#include "d3d12_query_heap.hpp"
#include "d3d12_resource.hpp"

#define TRACE(fmt, ...) do { FILE *_tf = fopen("Z:\\tmp\\dxmt_dxgi_trace.log", "a"); if (_tf) { fprintf(_tf, fmt "\n", ##__VA_ARGS__); fclose(_tf); } } while(0)
#include "d3d12_root_signature.hpp"
#include "com/com_object.hpp"
#include "log/log.hpp"
#include "util_string.hpp"
#include "d3d12_resource.hpp"
#include <thread>
#include <vector>
#include <d3d12.h>
#include <windows.h>

static LONG WINAPI crash_handler(EXCEPTION_POINTERS *ep) {
  if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION ||
      ep->ExceptionRecord->ExceptionCode == EXCEPTION_STACK_OVERFLOW) {
    FILE *f = fopen("Z:\\tmp\\dxmt_dxgi_trace.log", "a");
    if (f) {
      fprintf(f, "!!! EXCEPTION code=0x%lx addr=%p flags=0x%lx\n",
        ep->ExceptionRecord->ExceptionCode,
        ep->ExceptionRecord->ExceptionAddress,
        ep->ExceptionRecord->ExceptionFlags);
      void *buf[32];
      ULONG n = RtlCaptureStackBackTrace(0, 32, buf, nullptr);
      for (ULONG i = 0; i < n; i++) {
        fprintf(f, "  [%lu] %p\n", (unsigned long)i, buf[i]);
      }
      fclose(f);
    }
  }
  return EXCEPTION_CONTINUE_SEARCH;
}

void install_crash_handler() {
  AddVectoredExceptionHandler(1, crash_handler);
}

namespace dxmt {

static const GUID IID_ID3D12Device11_ = {0x5405c344, 0xd457, 0x444e, {0xb4, 0xdd, 0x23, 0x66, 0xe4, 0x5a, 0xee, 0x39}};
static const GUID IID_ID3D12Device12_ = {0x5af5c532, 0x4c91, 0x4cd0, {0xb5, 0x41, 0x15, 0xa4, 0x05, 0x39, 0x5f, 0xc5}};

Logger Logger::s_instance("d3d12.log");

static bool has_format_capability(FormatCapability capabilities,
                                  FormatCapability capability) {
  return (static_cast<int>(capabilities) & static_cast<int>(capability)) != 0;
}

static FormatCapability query_format_capability(
    const FormatCapabilityInspector &inspector, WMTPixelFormat format) {
  format = ORIGINAL_FORMAT(format);
  auto iter = inspector.textureCapabilities.find(format);
  if (iter == inspector.textureCapabilities.end())
    return FormatCapability::None;
  return iter->second;
}

static bool format_is_stream_output_compatible(DXGI_FORMAT format) {
  switch (format) {
  case DXGI_FORMAT_R32_FLOAT:
  case DXGI_FORMAT_R32_UINT:
  case DXGI_FORMAT_R32_SINT:
  case DXGI_FORMAT_R32G32_FLOAT:
  case DXGI_FORMAT_R32G32_UINT:
  case DXGI_FORMAT_R32G32_SINT:
  case DXGI_FORMAT_R32G32B32_FLOAT:
  case DXGI_FORMAT_R32G32B32_UINT:
  case DXGI_FORMAT_R32G32B32_SINT:
  case DXGI_FORMAT_R32G32B32A32_FLOAT:
  case DXGI_FORMAT_R32G32B32A32_UINT:
  case DXGI_FORMAT_R32G32B32A32_SINT:
    return true;
  default:
    return false;
  }
}

template <typename T>
static size_t pipeline_stream_payload_offset() {
  size_t offset = sizeof(UINT);
  size_t alignment = alignof(T);
  return (offset + alignment - 1) & ~(alignment - 1);
}

template <typename T>
static size_t pipeline_stream_subobject_size() {
  size_t size = pipeline_stream_payload_offset<T>() + sizeof(T);
  size_t alignment = alignof(void *);
  return (size + alignment - 1) & ~(alignment - 1);
}

template <typename T>
static bool read_pipeline_stream_subobject(uint8_t *stream, uint8_t *end,
                                           T *value) {
  size_t offset = pipeline_stream_payload_offset<T>();
  if (stream + offset + sizeof(T) > end)
    return false;
  *value = *reinterpret_cast<T *>(stream + offset);
  return true;
}

template <typename T>
static bool advance_pipeline_stream(uint8_t **stream, uint8_t *end) {
  size_t size = pipeline_stream_subobject_size<T>();
  if (*stream + size > end)
    return false;
  *stream += size;
  return true;
}

struct D3D12RTFormatArray {
  UINT NumRenderTargets;
  DXGI_FORMAT RTFormats[8];
};

struct D3D12DepthStencilDesc1 {
  BOOL DepthEnable;
  D3D12_DEPTH_WRITE_MASK DepthWriteMask;
  D3D12_COMPARISON_FUNC DepthFunc;
  BOOL StencilEnable;
  UINT8 StencilReadMask;
  UINT8 StencilWriteMask;
  D3D12_DEPTH_STENCILOP_DESC FrontFace;
  D3D12_DEPTH_STENCILOP_DESC BackFace;
  BOOL DepthBoundsTestEnable;
};

struct D3D12ViewInstanceLocation {
  UINT ViewportArrayIndex;
  UINT RenderTargetArrayIndex;
};

struct D3D12ViewInstancingDesc {
  UINT ViewInstanceCount;
  const D3D12ViewInstanceLocation *pViewInstanceLocations;
  UINT Flags;
};

static D3D12_DEPTH_STENCIL_DESC
convert_depth_stencil_desc1(const D3D12DepthStencilDesc1 &desc1) {
  D3D12_DEPTH_STENCIL_DESC desc = {};
  desc.DepthEnable = desc1.DepthEnable;
  desc.DepthWriteMask = desc1.DepthWriteMask;
  desc.DepthFunc = desc1.DepthFunc;
  desc.StencilEnable = desc1.StencilEnable;
  desc.StencilReadMask = desc1.StencilReadMask;
  desc.StencilWriteMask = desc1.StencilWriteMask;
  desc.FrontFace = desc1.FrontFace;
  desc.BackFace = desc1.BackFace;
  return desc;
}

class MTLD3D12CommandSignature : public ComObject<ID3D12CommandSignature> {
public:
  MTLD3D12CommandSignature(MTLD3D12Device *device, const D3D12_COMMAND_SIGNATURE_DESC &desc)
      : m_device(device), m_desc(desc) {
    if (desc.pArgumentDescs && desc.NumArgumentDescs) {
      m_argument_descs.assign(desc.pArgumentDescs,
                             desc.pArgumentDescs + desc.NumArgumentDescs);
      m_desc.pArgumentDescs = m_argument_descs.data();
    }
    m_device->AddRef();
  }
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
  const D3D12_COMMAND_SIGNATURE_DESC &GetDesc() const { return m_desc; }
private:
  MTLD3D12Device *m_device;
  D3D12_COMMAND_SIGNATURE_DESC m_desc;
  std::vector<D3D12_INDIRECT_ARGUMENT_DESC> m_argument_descs;
};

const D3D12_COMMAND_SIGNATURE_DESC *
GetD3D12CommandSignatureDesc(ID3D12CommandSignature *signature) {
  if (!signature)
    return nullptr;
  return &static_cast<MTLD3D12CommandSignature *>(signature)->GetDesc();
}

} // namespace dxmt

namespace dxmt {

static void *g_device_this = nullptr;
static void *g_device_expected_vtable = nullptr;
static uint64_t g_device_expected_m_device = 0;
static std::atomic<bool> g_device_watcher_running{false};
static int g_watcher_restore_count = 0;

static void device_vtable_watcher() {
  int check_count = 0;
  int snapshot_count = 0;
  while (g_device_watcher_running.load()) {
    if (g_device_this) {
      void *current = *(void**)g_device_this;
      uint64_t current_m_device = *((uint64_t*)((char*)g_device_this + 8));
      bool vtable_bad = (current != g_device_expected_vtable);
      bool m_device_bad = (g_device_expected_m_device != 0 && current_m_device != g_device_expected_m_device);
      if (vtable_bad || m_device_bad) {
        g_watcher_restore_count++;
        FILE *f = fopen("Z:\\tmp\\dxmt_dxgi_trace.log", "a");
        if (f) {
          fprintf(f, "!!! CORRUPTION #%d after %d checks: this=%p vtable_expected=%p vtable_now=%p m_device_expected=0x%llx m_device_now=0x%llx watcher_tid=%lu\n",
            g_watcher_restore_count, check_count,
            g_device_this, g_device_expected_vtable, current,
            (unsigned long long)g_device_expected_m_device, (unsigned long long)current_m_device,
            (unsigned long)GetCurrentThreadId());
          unsigned char *raw = (unsigned char *)g_device_this;
          fprintf(f, "!!! DEVICE DUMP [0x00-0x3F]:");
          for (int i = 0; i < 64; i += 8) {
            fprintf(f, " %02x%02x%02x%02x%02x%02x%02x%02x",
              raw[i], raw[i+1], raw[i+2], raw[i+3], raw[i+4], raw[i+5], raw[i+6], raw[i+7]);
          }
          fprintf(f, "\n!!! DEVICE DUMP [0x40-0x7F]:");
          for (int i = 64; i < 128; i += 8) {
            fprintf(f, " %02x%02x%02x%02x%02x%02x%02x%02x",
              raw[i], raw[i+1], raw[i+2], raw[i+3], raw[i+4], raw[i+5], raw[i+6], raw[i+7]);
          }
          fprintf(f, "\n");
          fclose(f);
        }
        *(void**)g_device_this = g_device_expected_vtable;
        if (g_device_expected_m_device != 0) {
          *((uint64_t*)((char*)g_device_this + 8)) = g_device_expected_m_device;
        }
        check_count = 0;
        continue;
      }
      check_count++;
      snapshot_count++;
      if (snapshot_count % 10000 == 0) {
        FILE *f = fopen("Z:\\tmp\\dxmt_dxgi_trace.log", "a");
        if (f) {
          fprintf(f, "watcher snapshot #%d: vtable=%p m_device=0x%llx OK\n",
            snapshot_count, current, (unsigned long long)current_m_device);
          fclose(f);
        }
      }
    }
    std::this_thread::sleep_for(std::chrono::microseconds(100));
  }
}

MTLD3D12Device::MTLD3D12Device(std::unique_ptr<Device> &&device,
                               IMTLDXGIAdapter *pAdapter)
    : m_device(std::move(device)), m_adapter(pAdapter) {
  m_format_inspector.Inspect(GetMTLDevice());
  if (m_adapter)
    m_adapter->AddRef();
  m_expected_vtable = *(void**)this;
	  g_device_this = (void*)this;
	  g_device_expected_vtable = m_expected_vtable;
	  g_device_expected_m_device = (uint64_t)m_device.get();
	  TRACE("M12 feature contract build=full_caps_current_pipeline_20260517");
	  TRACE("Device ctor: this=%p vtable=%p m_device=%p sizeof=%zu",
	    (void*)this, m_expected_vtable, (void*)m_device.get(), sizeof(MTLD3D12Device));
  extern void *g_d3d12_device_addr;
  extern size_t g_d3d12_device_size;
  g_d3d12_device_addr = (void*)this;
  g_d3d12_device_size = sizeof(MTLD3D12Device);
  TRACE("Device ctor: registered device guard at %p size=%zu", g_d3d12_device_addr, g_d3d12_device_size);
  g_device_watcher_running.store(true);
  std::thread watcher(device_vtable_watcher);
  watcher.detach();
  Logger::info("D3D12 device created via DXMT Metal backend");
}

MTLD3D12Device::~MTLD3D12Device() {
  void *current_vt = *(void**)this;
  FILE *f = fopen("Z:\\tmp\\dxmt_dxgi_trace.log", "a");
  if (f) {
    fprintf(f, "Device REAL DTOR this=%p vtable=%p expected=%p m_refCount=%u\n",
      (void*)this, current_vt, m_expected_vtable, m_refCount.load());
    fclose(f);
  }
  Logger::info("D3D12 device destroyed");
}

void MTLD3D12Device::CheckVtable(const char *where) {
  void *current = *(void**)this;
  if (current != m_expected_vtable) {
    TRACE("VTABLE CORRUPTION at %s: expected=%p got=%p this=%p — AUTO-RESTORING", where, m_expected_vtable, current, (void*)this);
    *(void**)this = m_expected_vtable;
  }
}

WMT::Device MTLD3D12Device::GetMTLDevice() {
  return m_device->device();
}

Device &MTLD3D12Device::GetDXMTDevice() { return *m_device; }

HRESULT STDMETHODCALLTYPE
MTLD3D12Device::QueryInterface(REFIID riid, void **ppvObject) {
  if (!ppvObject)
    return E_POINTER;
  *ppvObject = nullptr;

  TRACE("D3D12Device::QI(%s)", str::format(riid).c_str());

  if (riid == IID_IUnknown || riid == IID_ID3D12Object ||
      riid == IID_ID3D12DeviceChild || riid == IID_ID3D12Pageable ||
      riid == IID_ID3D12Device || riid == IID_ID3D12Device1 ||
      riid == IID_ID3D12Device2 || riid == IID_ID3D12Device3 ||
      riid == IID_ID3D12Device4 || riid == IID_ID3D12Device5 ||
      riid == IID_ID3D12Device6 || riid == IID_ID3D12Device7 ||
      riid == IID_ID3D12Device8 || riid == IID_ID3D12Device9 ||
      riid == IID_ID3D12Device10 ||
      riid == IID_ID3D12Device11_ || riid == IID_ID3D12Device12_) {
    *ppvObject = ref(this);
    TRACE("D3D12Device::QI(%s) -> S_OK (device)", str::format(riid).c_str());
    return S_OK;
  }

  if (riid == __uuidof(IMTLDXGIDevice) && m_dxgi_device) {
    TRACE("D3D12Device::QI(%s) -> delegating to dxgi_device", str::format(riid).c_str());
    return m_dxgi_device->QueryInterface(riid, ppvObject);
  }

  if (m_dxgi_device) {
    if (riid == IID_IDXGIDevice) {
      TRACE("D3D12Device::QI(%s) -> delegating DXGI to dxgi_device", str::format(riid).c_str());
      return m_dxgi_device->QueryInterface(riid, ppvObject);
    }
  }

  Logger::warn(str::format("D3D12Device::QueryInterface: unknown IID ", riid));
  TRACE("D3D12Device::QI(%s) -> E_NOINTERFACE", str::format(riid).c_str());
  return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE MTLD3D12Device::AddRef() {
  CheckVtable("AddRef");
  uint32_t rc = m_refCount++;
  if (!rc)
    ++m_refPrivate;
  return rc + 1;
}

ULONG STDMETHODCALLTYPE MTLD3D12Device::Release() {
  CheckVtable("Release");
  uint32_t rc = --m_refCount;
  if (rc <= 1) TRACE("Device::Release rc=%u this=%p", rc, (void*)this);
  if (!rc) {
    uint32_t rp = --m_refPrivate;
    if (!rp) {
      TRACE("Device::Release DELETING this=%p", (void*)this);
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

  TRACE("CreateGraphicsPSO ENTER: VS=%p(%zu) PS=%p(%zu) NumRT=%u DSV=%u Topo=%u",
        desc->VS.pShaderBytecode, desc->VS.BytecodeLength,
        desc->PS.pShaderBytecode, desc->PS.BytecodeLength,
        desc->NumRenderTargets, (unsigned)desc->DSVFormat,
        (unsigned)desc->PrimitiveTopologyType);

  auto pso = new MTLD3D12PipelineState(this, false);
  pso->SetGraphicsDesc(*desc);
  bool compiled = pso->Compile();
  TRACE("CreateGraphicsPSO: compile=%d VS=%p PS=%p stage=%s detail=%s",
        compiled, desc->VS.pShaderBytecode, desc->PS.pShaderBytecode,
        pso->GetCompileFailureStage(), pso->GetCompileFailureDetail());
  if (!compiled) {
    Logger::warn(str::format("CreateGraphicsPipelineState: shader compilation deferred/failed at ",
                             pso->GetCompileFailureStage(), ": ",
                             pso->GetCompileFailureDetail()));
  }
  HRESULT hr = pso->QueryInterface(riid, pipeline_state);
  if (FAILED(hr))
    pso->Release();
  TRACE("CreateGraphicsPSO EXIT hr=0x%lx pso=%p", hr, *pipeline_state);
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
  TRACE("CreateComputePSO: compile=%d CS=%p stage=%s detail=%s",
        compiled, desc->CS.pShaderBytecode,
        pso->GetCompileFailureStage(), pso->GetCompileFailureDetail());
  if (!compiled) {
    Logger::warn(str::format("CreateComputePipelineState: shader compilation deferred/failed at ",
                             pso->GetCompileFailureStage(), ": ",
                             pso->GetCompileFailureDetail()));
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
  TRACE("CheckFeatureSupport this=%p feature=%u data=%p size=%u", (void*)this, (unsigned)feature, feature_data, feature_data_size);
  if ((UINT_PTR)feature_data > 0 && (UINT_PTR)feature_data < 0x10000) {
    TRACE("!!! SUSPICIOUS CheckFeatureSupport: feature_data=%p looks like row_pitch (small int), this=%p — probable vtable slot 13 collision (ReadFromSubresource->CheckFeatureSupport)", feature_data, (void*)this);
  }
  switch (feature) {
  case D3D12_FEATURE_D3D12_OPTIONS: {
    auto *opts = (D3D12_FEATURE_DATA_D3D12_OPTIONS *)feature_data;
    if (feature_data_size < sizeof(*opts))
      return E_INVALIDARG;
    opts->DoublePrecisionFloatShaderOps = FALSE;
    opts->OutputMergerLogicOp = TRUE;
    opts->MinPrecisionSupport = D3D12_SHADER_MIN_PRECISION_SUPPORT_NONE;
    opts->TiledResourcesTier = D3D12_TILED_RESOURCES_TIER_NOT_SUPPORTED;
    opts->ResourceBindingTier = D3D12_RESOURCE_BINDING_TIER_2;
    opts->PSSpecifiedStencilRefSupported = TRUE;
    opts->TypedUAVLoadAdditionalFormats = FALSE;
    opts->ROVsSupported = FALSE;
    opts->ConservativeRasterizationTier =
        D3D12_CONSERVATIVE_RASTERIZATION_TIER_NOT_SUPPORTED;
    opts->MaxGPUVirtualAddressBitsPerResource = 40;
    opts->StandardSwizzle64KBSupported = FALSE;
    opts->CrossNodeSharingTier = D3D12_CROSS_NODE_SHARING_TIER_NOT_SUPPORTED;
    opts->CrossAdapterRowMajorTextureSupported = FALSE;
    opts->VPAndRTArrayIndexFromAnyShaderFeedingRasterizerSupportedWithoutGSEmulation = TRUE;
    opts->ResourceHeapTier = D3D12_RESOURCE_HEAP_TIER_2;
    TRACE("  OPTIONS: DoubleFP=%d LogicOp=%d TiledTier=%u BindingTier=%u PSStencilRef=%d TypedUAV=%d ROV=%d ConsRaster=%u VAbit=%u HeapTier=%u",
          opts->DoublePrecisionFloatShaderOps, opts->OutputMergerLogicOp,
          opts->TiledResourcesTier, opts->ResourceBindingTier,
          opts->PSSpecifiedStencilRefSupported, opts->TypedUAVLoadAdditionalFormats,
          opts->ROVsSupported, opts->ConservativeRasterizationTier,
          opts->MaxGPUVirtualAddressBitsPerResource, opts->ResourceHeapTier);
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
    TRACE("  FEATURE_LEVELS: MaxSupported=%u (from %u requested)", (unsigned)fl->MaxSupportedFeatureLevel, fl->NumFeatureLevels);
    return S_OK;
  }
  case D3D12_FEATURE_FORMAT_SUPPORT: {
    auto *fmt = (D3D12_FEATURE_DATA_FORMAT_SUPPORT *)feature_data;
    if (feature_data_size < sizeof(*fmt))
      return E_INVALIDARG;
    TRACE("  FORMAT_SUPPORT: format=%u", (unsigned)fmt->Format);
    fmt->Support1 = D3D12_FORMAT_SUPPORT1_NONE;
    fmt->Support2 = D3D12_FORMAT_SUPPORT2_NONE;

    if (fmt->Format == DXGI_FORMAT_UNKNOWN) {
      fmt->Support1 = D3D12_FORMAT_SUPPORT1_BUFFER;
      fmt->Support2 = (D3D12_FORMAT_SUPPORT2)(
          D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_ADD |
          D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_BITWISE_OPS |
          D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_COMPARE_STORE_OR_COMPARE_EXCHANGE |
          D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_EXCHANGE |
          D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_SIGNED_MIN_OR_MAX |
          D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_UNSIGNED_MIN_OR_MAX |
          D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD |
          D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE);
      TRACE("  FORMAT_SUPPORT: format=%u Support1=0x%x Support2=0x%x",
            (unsigned)fmt->Format, (unsigned)fmt->Support1,
            (unsigned)fmt->Support2);
      return S_OK;
    }

    MTL_DXGI_FORMAT_DESC metal_format;
    if (FAILED(MTLQueryDXGIFormat(GetMTLDevice(), fmt->Format, metal_format))) {
      TRACE("  FORMAT_SUPPORT: format=%u unsupported by MTLQueryDXGIFormat",
            (unsigned)fmt->Format);
      return E_INVALIDARG;
    }

    D3D12_FORMAT_SUPPORT1 support1 = D3D12_FORMAT_SUPPORT1_NONE;
    D3D12_FORMAT_SUPPORT2 support2 = D3D12_FORMAT_SUPPORT2_NONE;

    if (metal_format.PixelFormat) {
      support1 = (D3D12_FORMAT_SUPPORT1)(
          support1 | D3D12_FORMAT_SUPPORT1_SHADER_LOAD |
          D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE |
          D3D12_FORMAT_SUPPORT1_SHADER_GATHER |
          D3D12_FORMAT_SUPPORT1_MULTISAMPLE_LOAD |
          D3D12_FORMAT_SUPPORT1_TEXTURE1D |
          D3D12_FORMAT_SUPPORT1_TEXTURE2D |
          D3D12_FORMAT_SUPPORT1_TEXTURE3D |
          D3D12_FORMAT_SUPPORT1_TEXTURECUBE |
          D3D12_FORMAT_SUPPORT1_MIP |
          D3D12_FORMAT_SUPPORT1_CAST_WITHIN_BIT_LAYOUT);

      if (!(metal_format.Flag & (MTL_DXGI_FORMAT_BC |
                                 MTL_DXGI_FORMAT_DEPTH_PLANER |
                                 MTL_DXGI_FORMAT_STENCIL_PLANER))) {
        support1 = (D3D12_FORMAT_SUPPORT1)(support1 |
                                           D3D12_FORMAT_SUPPORT1_BUFFER);
      }

      if (metal_format.Flag & MTL_DXGI_FORMAT_BACKBUFFER) {
        support1 = (D3D12_FORMAT_SUPPORT1)(support1 |
                                           D3D12_FORMAT_SUPPORT1_DISPLAY);
      }
    }

    if (metal_format.AttributeFormat) {
      support1 = (D3D12_FORMAT_SUPPORT1)(support1 |
                                         D3D12_FORMAT_SUPPORT1_IA_VERTEX_BUFFER);
    }

    if (metal_format.PixelFormat == WMTPixelFormatR32Uint ||
        metal_format.PixelFormat == WMTPixelFormatR16Uint) {
      support1 = (D3D12_FORMAT_SUPPORT1)(support1 |
                                         D3D12_FORMAT_SUPPORT1_IA_INDEX_BUFFER);
    }

    auto capability =
        query_format_capability(m_format_inspector, metal_format.PixelFormat);

    if (has_format_capability(capability, FormatCapability::Color)) {
      support1 = (D3D12_FORMAT_SUPPORT1)(support1 |
                                         D3D12_FORMAT_SUPPORT1_RENDER_TARGET);
    }

    if (has_format_capability(capability, FormatCapability::Blend)) {
      support1 = (D3D12_FORMAT_SUPPORT1)(support1 |
                                         D3D12_FORMAT_SUPPORT1_BLENDABLE);
    }

    if (has_format_capability(capability, FormatCapability::DepthStencil)) {
      support1 = (D3D12_FORMAT_SUPPORT1)(
          support1 | D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL |
          D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE_COMPARISON |
          D3D12_FORMAT_SUPPORT1_SHADER_GATHER_COMPARISON);
    }

    if (has_format_capability(capability, FormatCapability::Resolve)) {
      support1 = (D3D12_FORMAT_SUPPORT1)(
          support1 | D3D12_FORMAT_SUPPORT1_MULTISAMPLE_RESOLVE);
    }

    if (has_format_capability(capability, FormatCapability::MSAA)) {
      support1 = (D3D12_FORMAT_SUPPORT1)(
          support1 | D3D12_FORMAT_SUPPORT1_MULTISAMPLE_RENDERTARGET);
    }

    if (has_format_capability(capability, FormatCapability::Write)) {
      support1 = (D3D12_FORMAT_SUPPORT1)(
          support1 | D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW);
    }

    if (format_is_stream_output_compatible(fmt->Format)) {
      support1 = (D3D12_FORMAT_SUPPORT1)(support1 |
                                         D3D12_FORMAT_SUPPORT1_SO_BUFFER);
    }

    if (has_format_capability(capability, FormatCapability::TextureBufferRead)) {
      support2 = (D3D12_FORMAT_SUPPORT2)(support2 |
                                         D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD);
    }

    if (has_format_capability(capability, FormatCapability::TextureBufferWrite)) {
      support2 = (D3D12_FORMAT_SUPPORT2)(support2 |
                                         D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE);
    }

    if (has_format_capability(capability,
                              FormatCapability::TextureBufferReadWrite)) {
      support2 = (D3D12_FORMAT_SUPPORT2)(
          support2 | D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD |
          D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE);
    }

    if (has_format_capability(capability, FormatCapability::Atomic)) {
      support2 = (D3D12_FORMAT_SUPPORT2)(
          support2 | D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_ADD |
          D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_BITWISE_OPS |
          D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_COMPARE_STORE_OR_COMPARE_EXCHANGE |
          D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_EXCHANGE |
          D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_SIGNED_MIN_OR_MAX |
          D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_UNSIGNED_MIN_OR_MAX);
    }

    if (has_format_capability(capability, FormatCapability::Blend)) {
      support2 = (D3D12_FORMAT_SUPPORT2)(
          support2 | D3D12_FORMAT_SUPPORT2_OUTPUT_MERGER_LOGIC_OP);
    }

    if (has_format_capability(capability, FormatCapability::Sparse)) {
      support2 =
          (D3D12_FORMAT_SUPPORT2)(support2 | D3D12_FORMAT_SUPPORT2_TILED);
    }

    fmt->Support1 = support1;
    fmt->Support2 = support2;
    TRACE("  FORMAT_SUPPORT: format=%u Support1=0x%x Support2=0x%x",
          (unsigned)fmt->Format, (unsigned)fmt->Support1, (unsigned)fmt->Support2);
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
    if (sm->HighestShaderModel > D3D_SHADER_MODEL_6_0)
      sm->HighestShaderModel = D3D_SHADER_MODEL_6_0;
    TRACE("  SHADER_MODEL: HighestSM=%u", (unsigned)sm->HighestShaderModel);
    return S_OK;
  }
  case D3D12_FEATURE_D3D12_OPTIONS1: {
    auto *o = (D3D12_FEATURE_DATA_D3D12_OPTIONS1 *)feature_data;
    if (feature_data_size < sizeof(*o))
      return E_INVALIDARG;
    o->WaveOps = FALSE;
    o->WaveLaneCountMin = 0;
    o->WaveLaneCountMax = 0;
    o->TotalLaneCount = 0;
    o->ExpandedComputeResourceStates = FALSE;
    o->Int64ShaderOps = FALSE;
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
    o->DepthBoundsTestSupported = FALSE;
    o->ProgrammableSamplePositionsTier =
        D3D12_PROGRAMMABLE_SAMPLE_POSITIONS_TIER_NOT_SUPPORTED;
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
    o->CastingFullyTypedFormatSupported = FALSE;
    o->WriteBufferImmediateSupportFlags = D3D12_COMMAND_LIST_SUPPORT_FLAG_NONE;
    o->ViewInstancingTier = D3D12_VIEW_INSTANCING_TIER_NOT_SUPPORTED;
    o->BarycentricsSupported = FALSE;
    TRACE("  OPTIONS3: CopyQueueTS=%d CastFullyTyped=%d WriteBufImm=0x%x ViewInstTier=%u Bary=%d",
          o->CopyQueueTimestampQueriesSupported, o->CastingFullyTypedFormatSupported,
          o->WriteBufferImmediateSupportFlags, o->ViewInstancingTier, o->BarycentricsSupported);
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
    o->RenderPassesTier = D3D12_RENDER_PASS_TIER_1;
    o->RaytracingTier = D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
    TRACE("  OPTIONS5: SRVTiled3=%d RenderPassesTier=%u RayTier=%u",
          o->SRVOnlyTiledResourceTier3, o->RenderPassesTier, o->RaytracingTier);
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
  case D3D12_FEATURE_PROTECTED_RESOURCE_SESSION_SUPPORT: {
    auto *p = (D3D12_FEATURE_DATA_PROTECTED_RESOURCE_SESSION_SUPPORT *)feature_data;
    if (feature_data_size < sizeof(*p))
      return E_INVALIDARG;
    p->Support = D3D12_PROTECTED_RESOURCE_SESSION_SUPPORT_FLAG_NONE;
    return S_OK;
  }
  default:
    TRACE("CheckFeatureSupport UNHANDLED feature=%u size=%u -> zeroing and returning S_OK", feature, feature_data_size);
    if (feature_data && feature_data_size > 0)
      memset(feature_data, 0, feature_data_size);
    return S_OK;
  }
}

HRESULT STDMETHODCALLTYPE
MTLD3D12Device::CreateDescriptorHeap(const D3D12_DESCRIPTOR_HEAP_DESC *desc,
                                      REFIID riid, void **descriptor_heap) {
  if (!desc || !descriptor_heap)
    return E_POINTER;
  CheckVtable("CreateDescriptorHeap");
  TRACE("CreateDescriptorHeap type=%u num=%u starting", desc->Type, desc->NumDescriptors);
  InitReturnPtr(descriptor_heap);

  TRACE("CreateDescriptorHeap: about to allocate %u bytes for object", (unsigned)sizeof(MTLD3D12DescriptorHeap));
  void *raw = HeapAlloc(GetProcessHeap(), 0, sizeof(MTLD3D12DescriptorHeap));
  TRACE("CreateDescriptorHeap: HeapAlloc returned %p", raw);
  if (!raw) {
    TRACE("CreateDescriptorHeap: HeapAlloc for object FAILED");
    return E_OUTOFMEMORY;
  }
  TRACE("CreateDescriptorHeap: about to placement-new, sizeof=%u", (unsigned)sizeof(MTLD3D12DescriptorHeap));
  MTLD3D12DescriptorHeap *heap = new (raw) MTLD3D12DescriptorHeap(this, *desc);
  TRACE("CreateDescriptorHeap: heap=%p data=%p", (void *)heap, heap->GetDescriptors());
  HRESULT hr = heap->QueryInterface(riid, descriptor_heap);
  if (FAILED(hr)) {
    heap->~MTLD3D12DescriptorHeap();
    HeapFree(GetProcessHeap(), 0, raw);
  }
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
  TRACE("CreateRootSignature DONE hr=0x%lx rs=%p out=%p", hr, (void*)rs, root_signature ? *root_signature : nullptr);
  return hr;
}

void STDMETHODCALLTYPE MTLD3D12Device::CreateConstantBufferView(
    const D3D12_CONSTANT_BUFFER_VIEW_DESC *desc,
    D3D12_CPU_DESCRIPTOR_HANDLE descriptor) {
  TRACE("CreateConstantBufferView");
  CheckVtable("CreateConstantBufferView");
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
  TRACE("CreateShaderResourceView res=%p handle=0x%llx device=%p", (void*)resource, (unsigned long long)descriptor.ptr, (void*)this);
  if ((void*)resource == (void*)this) {
    TRACE("!!! LEAK DETECTED: CreateShaderResourceView called with device pointer as resource!");
  }
  CheckVtable("CreateShaderResourceView");
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
  TRACE("CreateUnorderedAccessView res=%p counter=%p handle=0x%llx device=%p", (void*)resource, (void*)counter_resource, (unsigned long long)descriptor.ptr, (void*)this);
  if ((void*)resource == (void*)this || (void*)counter_resource == (void*)this) {
    TRACE("!!! LEAK DETECTED: CreateUnorderedAccessView called with device pointer as resource!");
  }
  CheckVtable("CreateUnorderedAccessView");
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
  TRACE("CreateRenderTargetView res=%p device=%p", (void*)resource, (void*)this);
  if ((void*)resource == (void*)this) {
    TRACE("!!! LEAK DETECTED: CreateRenderTargetView called with device pointer as resource!");
  }
  auto *d = reinterpret_cast<D3D12Descriptor *>(descriptor.ptr);
  if (d) {
    d->resource = resource;
    if (desc)
      d->rtv = *desc;
    d->type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    auto *dxmt_res = static_cast<MTLD3D12Resource *>(resource);
    TRACE("CreateRenderTargetView desc=%p res=%p tex=%llu fmt=%u dim=%u",
          (void*)d, (void*)resource,
          dxmt_res ? (unsigned long long)dxmt_res->GetMTLTexture().handle : 0ull,
          desc ? (unsigned)desc->Format : 0u,
          desc ? (unsigned)desc->ViewDimension : 0u);
  }
}

void STDMETHODCALLTYPE MTLD3D12Device::CreateDepthStencilView(
    ID3D12Resource *resource, const D3D12_DEPTH_STENCIL_VIEW_DESC *desc,
    D3D12_CPU_DESCRIPTOR_HANDLE descriptor) {
  TRACE("CreateDepthStencilView res=%p device=%p", (void*)resource, (void*)this);
  if ((void*)resource == (void*)this) {
    TRACE("!!! LEAK DETECTED: CreateDepthStencilView called with device pointer as resource!");
  }
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

    WMTSamplerInfo info = {};
    switch (desc->Filter) {
      case D3D12_FILTER_MIN_MAG_MIP_POINT:
        info.min_filter = WMTSamplerMinMagFilterNearest;
        info.mag_filter = WMTSamplerMinMagFilterNearest;
        info.mip_filter = WMTSamplerMipFilterNearest;
        break;
      case D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR:
        info.min_filter = WMTSamplerMinMagFilterNearest;
        info.mag_filter = WMTSamplerMinMagFilterNearest;
        info.mip_filter = WMTSamplerMipFilterLinear;
        break;
      case D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT:
        info.min_filter = WMTSamplerMinMagFilterNearest;
        info.mag_filter = WMTSamplerMinMagFilterLinear;
        info.mip_filter = WMTSamplerMipFilterNearest;
        break;
      case D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR:
        info.min_filter = WMTSamplerMinMagFilterNearest;
        info.mag_filter = WMTSamplerMinMagFilterLinear;
        info.mip_filter = WMTSamplerMipFilterLinear;
        break;
      case D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT:
        info.min_filter = WMTSamplerMinMagFilterLinear;
        info.mag_filter = WMTSamplerMinMagFilterNearest;
        info.mip_filter = WMTSamplerMipFilterNearest;
        break;
      case D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR:
        info.min_filter = WMTSamplerMinMagFilterLinear;
        info.mag_filter = WMTSamplerMinMagFilterNearest;
        info.mip_filter = WMTSamplerMipFilterLinear;
        break;
      case D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT:
        info.min_filter = WMTSamplerMinMagFilterLinear;
        info.mag_filter = WMTSamplerMinMagFilterLinear;
        info.mip_filter = WMTSamplerMipFilterNearest;
        break;
      case D3D12_FILTER_MIN_MAG_MIP_LINEAR:
        info.min_filter = WMTSamplerMinMagFilterLinear;
        info.mag_filter = WMTSamplerMinMagFilterLinear;
        info.mip_filter = WMTSamplerMipFilterLinear;
        break;
      case D3D12_FILTER_ANISOTROPIC:
        info.min_filter = WMTSamplerMinMagFilterLinear;
        info.mag_filter = WMTSamplerMinMagFilterLinear;
        info.mip_filter = WMTSamplerMipFilterLinear;
        info.max_anisotroy = desc->MaxAnisotropy;
        break;
      default:
        info.min_filter = WMTSamplerMinMagFilterLinear;
        info.mag_filter = WMTSamplerMinMagFilterLinear;
        info.mip_filter = WMTSamplerMipFilterLinear;
        break;
    }

    auto map_addr = [](D3D12_TEXTURE_ADDRESS_MODE mode) -> WMTSamplerAddressMode {
      switch (mode) {
        case D3D12_TEXTURE_ADDRESS_MODE_WRAP: return WMTSamplerAddressModeRepeat;
        case D3D12_TEXTURE_ADDRESS_MODE_MIRROR: return WMTSamplerAddressModeMirrorRepeat;
        case D3D12_TEXTURE_ADDRESS_MODE_CLAMP: return WMTSamplerAddressModeClampToEdge;
        case D3D12_TEXTURE_ADDRESS_MODE_BORDER: return WMTSamplerAddressModeClampToBorderColor;
        case D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE: return WMTSamplerAddressModeMirrorClampToEdge;
        default: return WMTSamplerAddressModeClampToEdge;
      }
    };
    info.s_address_mode = map_addr(desc->AddressU);
    info.t_address_mode = map_addr(desc->AddressV);
    info.r_address_mode = map_addr(desc->AddressW);
    info.lod_min_clamp = desc->MinLOD;
    info.lod_max_clamp = desc->MaxLOD;
    info.normalized_coords = true;
    info.support_argument_buffers = true;
    if (desc->Filter == D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR ||
        desc->Filter == D3D12_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR ||
        desc->Filter == D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT) {
      if (desc->ComparisonFunc >= D3D12_COMPARISON_FUNC_LESS &&
          desc->ComparisonFunc <= D3D12_COMPARISON_FUNC_ALWAYS) {
        info.compare_function = (WMTCompareFunction)(desc->ComparisonFunc - 1);
      }
    }

    d->metal_sampler = GetMTLDevice().newSamplerState(info);
    d->metal_sampler_gpu_id = info.gpu_resource_id;

    WMTSamplerInfo cube_info = info;
    if (cube_info.min_filter == WMTSamplerMinMagFilterLinear &&
        cube_info.mag_filter == WMTSamplerMinMagFilterLinear) {
      cube_info.s_address_mode = WMTSamplerAddressModeClampToBorderColor;
      cube_info.t_address_mode = WMTSamplerAddressModeClampToBorderColor;
      cube_info.r_address_mode = WMTSamplerAddressModeClampToBorderColor;
    } else {
      cube_info.s_address_mode = WMTSamplerAddressModeClampToEdge;
      cube_info.t_address_mode = WMTSamplerAddressModeClampToEdge;
      cube_info.r_address_mode = WMTSamplerAddressModeClampToEdge;
    }
    d->metal_sampler_cube = GetMTLDevice().newSamplerState(cube_info);
    d->metal_sampler_cube_gpu_id = cube_info.gpu_resource_id;
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
        if (src->resource && (void*)src->resource == (void*)this) {
          TRACE("!!! CopyDescriptors: src descriptor at %p has device pointer as resource! copying to dst %p", (void*)src, (void*)dst);
        }
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
  CheckVtable("CreateCommittedResource");
  if (!desc || !resource)
    return E_POINTER;
  InitReturnPtr(resource);

  auto res = new MTLD3D12Resource(this, *desc, initial_state,
                                  heap_properties ? *heap_properties
                                                  : D3D12_HEAP_PROPERTIES{});
  HRESULT hr = res->QueryInterface(riid, resource);
  TRACE("CreateCommittedResource res_obj=%p out=%p hr=0x%lx", (void*)res, resource ? *resource : nullptr, hr);
  if (resource && *resource == (void*)this) {
    TRACE("!!! LEAK DETECTED: CreateCommittedResource returned device pointer %p as resource!", (void*)this);
  }
  if (FAILED(hr))
    res->Release();
  return hr;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12Device::CreateHeap(const D3D12_HEAP_DESC *desc, REFIID riid,
                           void **heap) {
  TRACE("CreateHeap size=%llu type=%u flags=0x%x",
    desc ? (unsigned long long)desc->SizeInBytes : 0,
    desc ? desc->Properties.Type : 0xFF,
    desc ? desc->Flags : 0);
  if (!desc || !heap)
    return E_POINTER;
  InitReturnPtr(heap);

  auto h = new MTLD3D12Heap(this, *desc);
  HRESULT hr = h->QueryInterface(riid, heap);
  if (FAILED(hr))
    h->Release();
  return hr;
}

HRESULT STDMETHODCALLTYPE MTLD3D12Device::CreatePlacedResource(
    ID3D12Heap *heap, UINT64 heap_offset, const D3D12_RESOURCE_DESC *desc,
    D3D12_RESOURCE_STATES initial_state,
    const D3D12_CLEAR_VALUE *optimized_clear_value, REFIID riid,
    void **resource) {
  TRACE("CreatePlacedResource heap=%p offset=%llu dim=%u fmt=%u w=%llu",
    (void*)heap, (unsigned long long)heap_offset,
    desc ? desc->Dimension : 0, desc ? desc->Format : 0,
    desc ? desc->Width : 0);
  if (!desc || !resource || !heap)
    return E_POINTER;
  InitReturnPtr(resource);

  D3D12_HEAP_PROPERTIES heap_props = {};
  heap_props.Type = D3D12_HEAP_TYPE_DEFAULT;
  auto mt_heap = static_cast<MTLD3D12Heap *>(heap);
  if (mt_heap) {
    heap_props = mt_heap->GetHeapDesc().Properties;
  }

  auto res = new MTLD3D12Resource(this, *desc, initial_state, heap_props);
  HRESULT hr = res->QueryInterface(riid, resource);
  if (FAILED(hr))
    res->Release();
  return hr;
}

HRESULT STDMETHODCALLTYPE MTLD3D12Device::CreateReservedResource(
    const D3D12_RESOURCE_DESC *desc, D3D12_RESOURCE_STATES initial_state,
    const D3D12_CLEAR_VALUE *optimized_clear_value, REFIID riid,
    void **resource) {
  TRACE("CreateReservedResource dim=%u fmt=%u w=%llu (fallback to committed)", desc ? desc->Dimension : 0, desc ? desc->Format : 0, desc ? desc->Width : 0);
  CheckVtable("CreateReservedResource");
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
    D3D12_RESOURCE_DESC desc = {};
    res->GetDesc(&desc);
    if (desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
      if (addr >= gpu_addr && addr < gpu_addr + desc.Width)
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
  TRACE("SetEventOnMultipleFenceCompletion count=%u flags=0x%x event=%p",
    fence_count, flags, (void*)(uintptr_t)event);
  if (!fences || !values || !event)
    return E_POINTER;

  bool all_signaled = true;
  for (UINT i = 0; i < fence_count; i++) {
    if (fences[i]->GetCompletedValue() < values[i]) {
      all_signaled = false;
      break;
    }
  }

  if (all_signaled) {
    SetEvent(event);
    return S_OK;
  }

  if (flags == D3D12_MULTIPLE_FENCE_WAIT_FLAG_ALL) {
    for (UINT i = 0; i < fence_count; i++) {
      fences[i]->SetEventOnCompletion(values[i], nullptr);
    }
    SetEvent(event);
  } else {
    for (UINT i = 0; i < fence_count; i++) {
      if (fences[i]->GetCompletedValue() < values[i]) {
        fences[i]->SetEventOnCompletion(values[i], nullptr);
        SetEvent(event);
        break;
      }
    }
  }
  return S_OK;
}

HRESULT STDMETHODCALLTYPE MTLD3D12Device::SetResidencyPriority(
    UINT object_count, ID3D12Pageable *const *objects,
    const D3D12_RESIDENCY_PRIORITY *priorities) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE MTLD3D12Device::CreatePipelineState(
    const D3D12_PIPELINE_STATE_STREAM_DESC *desc, REFIID riid,
    void **ppPipelineState) {
  TRACE("ID3D12Device2::CreatePipelineState ENTER: size=%zu", desc ? desc->SizeInBytes : 0);

  if (!desc || !desc->pPipelineStateSubobjectStream || !ppPipelineState)
    return E_INVALIDARG;

  *ppPipelineState = nullptr;

  auto *stream = (uint8_t *)desc->pPipelineStateSubobjectStream;
  auto *end = stream + desc->SizeInBytes;

  D3D12_GRAPHICS_PIPELINE_STATE_DESC graphics_desc = {};
  D3D12_COMPUTE_PIPELINE_STATE_DESC compute_desc = {};
  bool has_cs = false;
  bool is_compute = true;

  graphics_desc.SampleMask = UINT_MAX;
  graphics_desc.SampleDesc.Count = 1;

  while (stream + sizeof(UINT) <= end) {
    uint8_t *subobject = stream;
    UINT type = *reinterpret_cast<UINT *>(subobject);
    bool advanced = false;

    switch (type) {
    case 0: { // D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE
      ID3D12RootSignature *root_signature = nullptr;
      if (!read_pipeline_stream_subobject(subobject, end, &root_signature))
        return E_INVALIDARG;
      graphics_desc.pRootSignature = root_signature;
      compute_desc.pRootSignature = root_signature;
      advanced = advance_pipeline_stream<ID3D12RootSignature *>(&stream, end);
      break;
    }
    case 1: { // VS
      if (!read_pipeline_stream_subobject(subobject, end, &graphics_desc.VS))
        return E_INVALIDARG;
      is_compute = false;
      advanced = advance_pipeline_stream<D3D12_SHADER_BYTECODE>(&stream, end);
      break;
    }
    case 2: { // PS
      if (!read_pipeline_stream_subobject(subobject, end, &graphics_desc.PS))
        return E_INVALIDARG;
      is_compute = false;
      advanced = advance_pipeline_stream<D3D12_SHADER_BYTECODE>(&stream, end);
      break;
    }
    case 3: { // DS
      if (!read_pipeline_stream_subobject(subobject, end, &graphics_desc.DS))
        return E_INVALIDARG;
      is_compute = false;
      advanced = advance_pipeline_stream<D3D12_SHADER_BYTECODE>(&stream, end);
      break;
    }
    case 4: { // HS
      if (!read_pipeline_stream_subobject(subobject, end, &graphics_desc.HS))
        return E_INVALIDARG;
      is_compute = false;
      advanced = advance_pipeline_stream<D3D12_SHADER_BYTECODE>(&stream, end);
      break;
    }
    case 5: { // GS
      if (!read_pipeline_stream_subobject(subobject, end, &graphics_desc.GS))
        return E_INVALIDARG;
      is_compute = false;
      advanced = advance_pipeline_stream<D3D12_SHADER_BYTECODE>(&stream, end);
      break;
    }
    case 6: { // CS
      if (!read_pipeline_stream_subobject(subobject, end, &compute_desc.CS))
        return E_INVALIDARG;
      has_cs = true;
      advanced = advance_pipeline_stream<D3D12_SHADER_BYTECODE>(&stream, end);
      break;
    }
    case 7: { // STREAM_OUTPUT
      if (!read_pipeline_stream_subobject(subobject, end,
                                          &graphics_desc.StreamOutput))
        return E_INVALIDARG;
      is_compute = false;
      advanced = advance_pipeline_stream<D3D12_STREAM_OUTPUT_DESC>(&stream, end);
      break;
    }
    case 8: { // BLEND
      if (!read_pipeline_stream_subobject(subobject, end,
                                          &graphics_desc.BlendState))
        return E_INVALIDARG;
      is_compute = false;
      advanced = advance_pipeline_stream<D3D12_BLEND_DESC>(&stream, end);
      break;
    }
    case 9: { // SAMPLE_MASK
      if (!read_pipeline_stream_subobject(subobject, end,
                                          &graphics_desc.SampleMask))
        return E_INVALIDARG;
      is_compute = false;
      advanced = advance_pipeline_stream<UINT>(&stream, end);
      break;
    }
    case 10: { // RASTERIZER
      if (!read_pipeline_stream_subobject(subobject, end,
                                          &graphics_desc.RasterizerState))
        return E_INVALIDARG;
      is_compute = false;
      advanced = advance_pipeline_stream<D3D12_RASTERIZER_DESC>(&stream, end);
      break;
    }
    case 11: { // DEPTH_STENCIL
      if (!read_pipeline_stream_subobject(subobject, end,
                                          &graphics_desc.DepthStencilState))
        return E_INVALIDARG;
      is_compute = false;
      advanced = advance_pipeline_stream<D3D12_DEPTH_STENCIL_DESC>(&stream, end);
      break;
    }
    case 12: { // INPUT_LAYOUT
      if (!read_pipeline_stream_subobject(subobject, end,
                                          &graphics_desc.InputLayout))
        return E_INVALIDARG;
      is_compute = false;
      advanced = advance_pipeline_stream<D3D12_INPUT_LAYOUT_DESC>(&stream, end);
      break;
    }
    case 13: { // IB_STRIP_CUT_VALUE
      if (!read_pipeline_stream_subobject(subobject, end,
                                          &graphics_desc.IBStripCutValue))
        return E_INVALIDARG;
      is_compute = false;
      advanced =
          advance_pipeline_stream<D3D12_INDEX_BUFFER_STRIP_CUT_VALUE>(&stream, end);
      break;
    }
    case 14: { // PRIMITIVE_TOPOLOGY
      if (!read_pipeline_stream_subobject(subobject, end,
                                          &graphics_desc.PrimitiveTopologyType))
        return E_INVALIDARG;
      is_compute = false;
      advanced =
          advance_pipeline_stream<D3D12_PRIMITIVE_TOPOLOGY_TYPE>(&stream, end);
      break;
    }
    case 15: { // RENDER_TARGET_FORMATS
      D3D12RTFormatArray fmt = {};
      if (!read_pipeline_stream_subobject(subobject, end, &fmt))
        return E_INVALIDARG;
      graphics_desc.NumRenderTargets = fmt.NumRenderTargets;
      for (UINT i = 0; i < 8 && i < fmt.NumRenderTargets; i++)
        graphics_desc.RTVFormats[i] = fmt.RTFormats[i];
      is_compute = false;
      advanced = advance_pipeline_stream<D3D12RTFormatArray>(&stream, end);
      break;
    }
    case 16: { // DEPTH_STENCIL_FORMAT
      if (!read_pipeline_stream_subobject(subobject, end,
                                          &graphics_desc.DSVFormat))
        return E_INVALIDARG;
      is_compute = false;
      advanced = advance_pipeline_stream<DXGI_FORMAT>(&stream, end);
      break;
    }
    case 17: { // SAMPLE_DESC
      if (!read_pipeline_stream_subobject(subobject, end,
                                          &graphics_desc.SampleDesc))
        return E_INVALIDARG;
      is_compute = false;
      advanced = advance_pipeline_stream<DXGI_SAMPLE_DESC>(&stream, end);
      break;
    }
    case 18: { // NODE_MASK
      UINT node_mask = 0;
      if (!read_pipeline_stream_subobject(subobject, end, &node_mask))
        return E_INVALIDARG;
      graphics_desc.NodeMask = node_mask;
      compute_desc.NodeMask = node_mask;
      advanced = advance_pipeline_stream<UINT>(&stream, end);
      break;
    }
    case 19: { // CACHED_PSO
      D3D12_CACHED_PIPELINE_STATE cached_pso = {};
      if (!read_pipeline_stream_subobject(subobject, end, &cached_pso))
        return E_INVALIDARG;
      graphics_desc.CachedPSO = cached_pso;
      compute_desc.CachedPSO = cached_pso;
      advanced =
          advance_pipeline_stream<D3D12_CACHED_PIPELINE_STATE>(&stream, end);
      break;
    }
    case 20: { // FLAGS
      D3D12_PIPELINE_STATE_FLAGS flags = D3D12_PIPELINE_STATE_FLAG_NONE;
      if (!read_pipeline_stream_subobject(subobject, end, &flags))
        return E_INVALIDARG;
      graphics_desc.Flags = flags;
      compute_desc.Flags = flags;
      advanced = advance_pipeline_stream<D3D12_PIPELINE_STATE_FLAGS>(&stream, end);
      break;
    }
    case 21: { // DEPTH_STENCIL1
      D3D12DepthStencilDesc1 depth_stencil = {};
      if (!read_pipeline_stream_subobject(subobject, end, &depth_stencil))
        return E_INVALIDARG;
      graphics_desc.DepthStencilState =
          convert_depth_stencil_desc1(depth_stencil);
      is_compute = false;
      advanced = advance_pipeline_stream<D3D12DepthStencilDesc1>(&stream, end);
      break;
    }
    case 22: { // VIEW_INSTANCING
      D3D12ViewInstancingDesc view_instancing = {};
      if (!read_pipeline_stream_subobject(subobject, end, &view_instancing))
        return E_INVALIDARG;
      TRACE("CreatePipelineState: view instancing count=%u flags=0x%x ignored",
            view_instancing.ViewInstanceCount, view_instancing.Flags);
      is_compute = false;
      advanced = advance_pipeline_stream<D3D12ViewInstancingDesc>(&stream, end);
      break;
    }
    case 24: // AS
    case 25: { // MS
      D3D12_SHADER_BYTECODE shader = {};
      if (!read_pipeline_stream_subobject(subobject, end, &shader))
        return E_INVALIDARG;
      TRACE("CreatePipelineState: mesh/amplification shader subobject type=%u ignored",
            type);
      is_compute = false;
      advanced = advance_pipeline_stream<D3D12_SHADER_BYTECODE>(&stream, end);
      break;
    }
    default:
      TRACE("CreatePipelineState: unknown subobject type %u at offset=%zu",
            type,
            static_cast<size_t>(subobject -
                                (uint8_t *)desc->pPipelineStateSubobjectStream));
      return E_INVALIDARG;
    }

    if (!advanced)
      return E_INVALIDARG;
  }

  if (has_cs && is_compute) {
    compute_desc.pRootSignature = graphics_desc.pRootSignature;
    TRACE("ID3D12Device2::CreatePipelineState -> delegating to CreateComputePSO CS=%p", compute_desc.CS.pShaderBytecode);
    return CreateComputePipelineState(&compute_desc, riid, ppPipelineState);
  }

  TRACE("ID3D12Device2::CreatePipelineState -> delegating to CreateGraphicsPSO VS=%p PS=%p NumRT=%u",
        graphics_desc.VS.pShaderBytecode, graphics_desc.PS.pShaderBytecode, graphics_desc.NumRenderTargets);
  return CreateGraphicsPipelineState(&graphics_desc, riid, ppPipelineState);
}

/*** ID3D12Device3 ***/
HRESULT STDMETHODCALLTYPE MTLD3D12Device::OpenExistingHeapFromAddress(
    const void *address, REFIID riid, void **heap) {
  TRACE("ID3D12Device3::OpenExistingHeapFromAddress -> E_NOTIMPL");
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE MTLD3D12Device::OpenExistingHeapFromFileMapping(
    HANDLE file_mapping, REFIID riid, void **heap) {
  TRACE("ID3D12Device3::OpenExistingHeapFromFileMapping -> E_NOTIMPL");
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE MTLD3D12Device::EnqueueMakeResident(
    D3D12_RESIDENCY_FLAGS flags, UINT num_objects,
    ID3D12Pageable *const *objects, ID3D12Fence *fence,
    UINT64 fence_value) {
  TRACE("ID3D12Device3::EnqueueMakeResident -> S_OK (delegating to MakeResident)");
  HRESULT hr = MakeResident(num_objects, objects);
  if (SUCCEEDED(hr) && fence) {
    fence->Signal(fence_value);
  }
  return hr;
}

/*** ID3D12Device4 ***/
HRESULT STDMETHODCALLTYPE MTLD3D12Device::CreateCommandList1(
    UINT node_mask, D3D12_COMMAND_LIST_TYPE type,
    D3D12_COMMAND_LIST_FLAGS flags, REFIID riid,
    void **command_list) {
  TRACE("ID3D12Device4::CreateCommandList1 -> delegating to CreateCommandList");
  return CreateCommandList(node_mask, type, nullptr, nullptr, riid, command_list);
}

HRESULT STDMETHODCALLTYPE MTLD3D12Device::CreateProtectedResourceSession(
    const D3D12_PROTECTED_RESOURCE_SESSION_DESC *desc, REFIID riid,
    void **session) {
  TRACE("ID3D12Device4::CreateProtectedResourceSession -> E_NOTIMPL");
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE MTLD3D12Device::CreateCommittedResource1(
    const D3D12_HEAP_PROPERTIES *heap_properties,
    D3D12_HEAP_FLAGS heap_flags, const D3D12_RESOURCE_DESC *desc,
    D3D12_RESOURCE_STATES initial_resource_state,
    const D3D12_CLEAR_VALUE *optimized_clear_value,
    ID3D12ProtectedResourceSession *protected_session,
    REFIID riid_resource, void **resource) {
  if (protected_session) {
    TRACE("ID3D12Device4::CreateCommittedResource1 -> E_NOTIMPL (protected session)");
    return E_NOTIMPL;
  }
  return CreateCommittedResource(heap_properties, heap_flags, desc,
      initial_resource_state, optimized_clear_value, riid_resource, resource);
}

HRESULT STDMETHODCALLTYPE MTLD3D12Device::CreateHeap1(
    const D3D12_HEAP_DESC *desc,
    ID3D12ProtectedResourceSession *protected_session,
    REFIID riid, void **heap) {
  if (protected_session) {
    TRACE("ID3D12Device4::CreateHeap1 -> E_NOTIMPL (protected session)");
    return E_NOTIMPL;
  }
  return CreateHeap(desc, riid, heap);
}

HRESULT STDMETHODCALLTYPE MTLD3D12Device::CreateReservedResource1(
    const D3D12_RESOURCE_DESC *desc, D3D12_RESOURCE_STATES initial_state,
    const D3D12_CLEAR_VALUE *optimized_clear_value,
    ID3D12ProtectedResourceSession *protected_session,
    REFIID riid, void **resource) {
  if (protected_session) {
    TRACE("ID3D12Device4::CreateReservedResource1 -> E_NOTIMPL (protected session)");
    return E_NOTIMPL;
  }
  return CreateReservedResource(desc, initial_state, optimized_clear_value, riid, resource);
}

D3D12_RESOURCE_ALLOCATION_INFO* STDMETHODCALLTYPE MTLD3D12Device::GetResourceAllocationInfo1(
    D3D12_RESOURCE_ALLOCATION_INFO *__ret, UINT visible_mask,
    UINT resource_descs_count, const D3D12_RESOURCE_DESC *resource_descs,
    D3D12_RESOURCE_ALLOCATION_INFO1 *resource_allocation_info1) {
  TRACE("ID3D12Device4::GetResourceAllocationInfo1 -> delegating");
  return GetResourceAllocationInfo(__ret, visible_mask, resource_descs_count, resource_descs);
}

/*** ID3D12Device5 ***/
HRESULT STDMETHODCALLTYPE MTLD3D12Device::CreateLifetimeTracker(
    ID3D12LifetimeOwner *owner, REFIID riid, void **tracker) {
  TRACE("ID3D12Device5::CreateLifetimeTracker -> E_NOTIMPL");
  return E_NOTIMPL;
}

void STDMETHODCALLTYPE MTLD3D12Device::RemoveDevice() {
  TRACE("ID3D12Device5::RemoveDevice");
}

HRESULT STDMETHODCALLTYPE MTLD3D12Device::EnumerateMetaCommands(
    UINT *meta_commands_count, D3D12_META_COMMAND_DESC *descs) {
  TRACE("ID3D12Device5::EnumerateMetaCommands -> E_NOTIMPL");
  if (meta_commands_count) *meta_commands_count = 0;
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE MTLD3D12Device::EnumerateMetaCommandParameters(
    REFGUID command_id, D3D12_META_COMMAND_PARAMETER_STAGE stage,
    UINT *total_structure_size_in_bytes, UINT *parameter_count,
    D3D12_META_COMMAND_PARAMETER_DESC *parameter_descs) {
  TRACE("ID3D12Device5::EnumerateMetaCommandParameters -> E_NOTIMPL");
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE MTLD3D12Device::CreateMetaCommand(
    REFGUID command_id, UINT node_mask,
    const void *creation_parameters_data,
    SIZE_T creation_parameters_data_size_in_bytes,
    REFIID riid, void **meta_command) {
  TRACE("ID3D12Device5::CreateMetaCommand -> E_NOTIMPL");
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE MTLD3D12Device::CreateStateObject(
    const D3D12_STATE_OBJECT_DESC *desc, REFIID riid,
    void **state_object) {
  TRACE("ID3D12Device5::CreateStateObject -> E_NOTIMPL");
  return E_NOTIMPL;
}

void STDMETHODCALLTYPE MTLD3D12Device::GetRaytracingAccelerationStructurePrebuildInfo(
    const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS *desc,
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO *info) {
  TRACE("ID3D12Device5::GetRaytracingAccelerationStructurePrebuildInfo");
  if (info) {
    memset(info, 0, sizeof(*info));
  }
}

D3D12_DRIVER_MATCHING_IDENTIFIER_STATUS STDMETHODCALLTYPE MTLD3D12Device::CheckDriverMatchingIdentifier(
    D3D12_SERIALIZED_DATA_TYPE serialized_data_type,
    const D3D12_SERIALIZED_DATA_DRIVER_MATCHING_IDENTIFIER *identifier_to_check) {
  TRACE("ID3D12Device5::CheckDriverMatchingIdentifier -> UNRECOGNIZED");
  return D3D12_DRIVER_MATCHING_IDENTIFIER_UNRECOGNIZED;
}

/*** ID3D12Device6 ***/
HRESULT STDMETHODCALLTYPE MTLD3D12Device::SetBackgroundProcessingMode(
    D3D12_BACKGROUND_PROCESSING_MODE mode,
    D3D12_MEASUREMENTS_ACTION action, HANDLE event,
    WINBOOL *further_measurements_desired) {
  TRACE("ID3D12Device6::SetBackgroundProcessingMode -> E_NOTIMPL");
  return E_NOTIMPL;
}

/*** ID3D12Device7 ***/
HRESULT STDMETHODCALLTYPE MTLD3D12Device::AddToStateObject(
    const D3D12_STATE_OBJECT_DESC *addition,
    ID3D12StateObject *state_object_to_grow_from,
    REFIID riid, void **new_state_object) {
  TRACE("ID3D12Device7::AddToStateObject -> E_NOTIMPL");
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE MTLD3D12Device::CreateProtectedResourceSession1(
    const D3D12_PROTECTED_RESOURCE_SESSION_DESC1 *desc,
    REFIID riid, void **session) {
  TRACE("ID3D12Device7::CreateProtectedResourceSession1 -> E_NOTIMPL");
  return E_NOTIMPL;
}

/*** ID3D12Device8 ***/
static const int MAX_DESCS = 256;

D3D12_RESOURCE_ALLOCATION_INFO* STDMETHODCALLTYPE MTLD3D12Device::GetResourceAllocationInfo2(
    D3D12_RESOURCE_ALLOCATION_INFO *__ret, UINT visible_mask,
    UINT resource_descs_count, const D3D12_RESOURCE_DESC1 *resource_descs,
    D3D12_RESOURCE_ALLOCATION_INFO1 *resource_allocation_info1) {
  TRACE("ID3D12Device8::GetResourceAllocationInfo2 -> delegating");
  D3D12_RESOURCE_DESC descs_compat[MAX_DESCS];
  for (UINT i = 0; i < resource_descs_count && i < MAX_DESCS; i++) {
    memcpy(&descs_compat[i], &resource_descs[i], sizeof(D3D12_RESOURCE_DESC));
  }
  return GetResourceAllocationInfo(__ret, visible_mask, resource_descs_count, descs_compat);
}

HRESULT STDMETHODCALLTYPE MTLD3D12Device::CreateCommittedResource2(
    const D3D12_HEAP_PROPERTIES *heap_properties,
    D3D12_HEAP_FLAGS heap_flags, const D3D12_RESOURCE_DESC1 *desc,
    D3D12_RESOURCE_STATES initial_resource_state,
    const D3D12_CLEAR_VALUE *optimized_clear_value,
    ID3D12ProtectedResourceSession *protected_session,
    REFIID riid_resource, void **resource) {
  if (protected_session) {
    TRACE("ID3D12Device8::CreateCommittedResource2 -> E_NOTIMPL (protected session)");
    return E_NOTIMPL;
  }
  D3D12_RESOURCE_DESC desc_compat;
  memcpy(&desc_compat, desc, sizeof(D3D12_RESOURCE_DESC));
  return CreateCommittedResource(heap_properties, heap_flags, &desc_compat,
      initial_resource_state, optimized_clear_value, riid_resource, resource);
}

HRESULT STDMETHODCALLTYPE MTLD3D12Device::CreatePlacedResource1(
    ID3D12Heap *heap, UINT64 heap_offset,
    const D3D12_RESOURCE_DESC1 *desc,
    D3D12_RESOURCE_STATES initial_state,
    const D3D12_CLEAR_VALUE *optimized_clear_value,
    REFIID riid, void **resource) {
  D3D12_RESOURCE_DESC desc_compat;
  memcpy(&desc_compat, desc, sizeof(D3D12_RESOURCE_DESC));
  return CreatePlacedResource(heap, heap_offset, &desc_compat,
      initial_state, optimized_clear_value, riid, resource);
}

void STDMETHODCALLTYPE MTLD3D12Device::CreateSamplerFeedbackUnorderedAccessView(
    ID3D12Resource *targeted_resource, ID3D12Resource *feedback_resource,
    D3D12_CPU_DESCRIPTOR_HANDLE dst_descriptor) {
  TRACE("ID3D12Device8::CreateSamplerFeedbackUnorderedAccessView -> noop");
}

void STDMETHODCALLTYPE MTLD3D12Device::GetCopyableFootprints1(
    const D3D12_RESOURCE_DESC1 *resource_desc, UINT first_subresource,
    UINT subresources_count, UINT64 base_offset,
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT *layouts, UINT *rows_count,
    UINT64 *row_size_in_bytes, UINT64 *total_bytes) {
  D3D12_RESOURCE_DESC desc_compat;
  memcpy(&desc_compat, resource_desc, sizeof(D3D12_RESOURCE_DESC));
  GetCopyableFootprints(&desc_compat, first_subresource, subresources_count,
      base_offset, layouts, rows_count, row_size_in_bytes, total_bytes);
}

/*** ID3D12Device9 ***/
HRESULT STDMETHODCALLTYPE MTLD3D12Device::CreateShaderCacheSession(
    const D3D12_SHADER_CACHE_SESSION_DESC *desc, REFIID riid,
    void **session) {
  TRACE("ID3D12Device9::CreateShaderCacheSession -> E_NOTIMPL");
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE MTLD3D12Device::ShaderCacheControl(
    D3D12_SHADER_CACHE_KIND_FLAGS kinds,
    D3D12_SHADER_CACHE_CONTROL_FLAGS control) {
  TRACE("ID3D12Device9::ShaderCacheControl -> E_NOTIMPL");
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE MTLD3D12Device::CreateCommandQueue1(
    const D3D12_COMMAND_QUEUE_DESC *desc, REFIID creator_id,
    REFIID riid, void **command_queue) {
  TRACE("ID3D12Device9::CreateCommandQueue1 -> delegating to CreateCommandQueue");
  return CreateCommandQueue(desc, riid, command_queue);
}

/*** ID3D12Device10 ***/
HRESULT STDMETHODCALLTYPE MTLD3D12Device::CreateCommittedResource3(
    const D3D12_HEAP_PROPERTIES *heap_properties,
    D3D12_HEAP_FLAGS heap_flags, const D3D12_RESOURCE_DESC1 *desc,
    D3D12_BARRIER_LAYOUT initial_layout,
    const D3D12_CLEAR_VALUE *optimized_clear_value,
    ID3D12ProtectedResourceSession *protected_session,
    UINT32 castable_formats_count,
    DXGI_FORMAT *castable_formats,
    REFIID riid_resource, void **resource) {
  if (protected_session) {
    TRACE("ID3D12Device10::CreateCommittedResource3 -> E_NOTIMPL (protected session)");
    return E_NOTIMPL;
  }
  TRACE("ID3D12Device10::CreateCommittedResource3 -> delegating to CreateCommittedResource2");
  return CreateCommittedResource2(heap_properties, heap_flags, desc,
      (D3D12_RESOURCE_STATES)initial_layout, optimized_clear_value,
      protected_session, riid_resource, resource);
}

HRESULT STDMETHODCALLTYPE MTLD3D12Device::CreatePlacedResource2(
    ID3D12Heap *heap, UINT64 heap_offset,
    const D3D12_RESOURCE_DESC1 *desc,
    D3D12_BARRIER_LAYOUT initial_layout,
    const D3D12_CLEAR_VALUE *optimized_clear_value,
    UINT32 castable_formats_count,
    DXGI_FORMAT *castable_formats,
    REFIID riid, void **resource) {
  TRACE("ID3D12Device10::CreatePlacedResource2 -> delegating to CreatePlacedResource1");
  return CreatePlacedResource1(heap, heap_offset, desc,
      (D3D12_RESOURCE_STATES)initial_layout, optimized_clear_value,
      riid, resource);
}

HRESULT STDMETHODCALLTYPE MTLD3D12Device::CreateReservedResource2(
    const D3D12_RESOURCE_DESC *desc,
    D3D12_BARRIER_LAYOUT initial_layout,
    const D3D12_CLEAR_VALUE *optimized_clear_value,
    ID3D12ProtectedResourceSession *protected_session,
    UINT32 castable_formats_count,
    DXGI_FORMAT *castable_formats,
    REFIID riid, void **resource) {
  if (protected_session) {
    TRACE("ID3D12Device10::CreateReservedResource2 -> E_NOTIMPL (protected session)");
    return E_NOTIMPL;
  }
  TRACE("ID3D12Device10::CreateReservedResource2 -> delegating to CreateReservedResource");
  return CreateReservedResource(desc,
      (D3D12_RESOURCE_STATES)initial_layout, optimized_clear_value,
      riid, resource);
}

} // namespace dxmt
