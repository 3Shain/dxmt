
#include "Metal/MTLPixelFormat.hpp"
#include "Metal/MTLStageInputOutputDescriptor.hpp"
#include "Metal/MTLVertexDescriptor.hpp"
#include "com/com_guid.hpp"
#include "com/com_pointer.hpp"
#include "dxgi_options.hpp"
#include "util_string.hpp"
#include "log/log.hpp"
#include "wsi_monitor.hpp"
#include "dxgi_interfaces.h"
#include "dxgi_object.hpp"
#include "objc_pointer.hpp"
#include "dxmt_format.hpp"

namespace dxmt {

Com<IDXGIOutput> CreateOutput(IMTLDXGIAdatper *pAadapter, HMONITOR monitor);

class MTLDXGIAdatper : public MTLDXGIObject<IMTLDXGIAdatper> {
public:
  MTLDXGIAdatper(MTL::Device *device, IDXGIFactory *factory, Config &config)
      : m_deivce(device), m_factory(factory), options(config), config(config) {
    format_inspector.Inspect(device);
  };
  ~MTLDXGIAdatper() { m_deivce->release(); }
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                           void **ppvObject) final {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(IDXGIObject) ||
        riid == __uuidof(IDXGIAdapter) || riid == __uuidof(IDXGIAdapter1) ||
        riid == __uuidof(IDXGIAdapter2) || riid == __uuidof(IDXGIAdapter3) ||
        riid == __uuidof(IMTLDXGIAdatper)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(IDXGIAdapter), riid)) {
      WARN("DXGIAdapter: Unknown interface query ", str::format(riid));
    }

    return E_NOINTERFACE;
  };

  HRESULT STDMETHODCALLTYPE GetParent(REFIID riid, void **ppParent) final {
    return m_factory->QueryInterface(riid, ppParent);
  }
  HRESULT STDMETHODCALLTYPE GetDesc(DXGI_ADAPTER_DESC *pDesc) final {
    if (pDesc == nullptr)
      return E_INVALIDARG;

    DXGI_ADAPTER_DESC2 desc;
    HRESULT hr = GetDesc2(&desc);

    if (SUCCEEDED(hr)) {
      std::memcpy(pDesc->Description, desc.Description,
                  sizeof(pDesc->Description));

      pDesc->VendorId = desc.VendorId;
      pDesc->DeviceId = desc.DeviceId;
      pDesc->SubSysId = desc.SubSysId;
      pDesc->Revision = desc.Revision;
      pDesc->DedicatedVideoMemory = desc.DedicatedVideoMemory;
      pDesc->DedicatedSystemMemory = desc.DedicatedSystemMemory;
      pDesc->SharedSystemMemory = desc.SharedSystemMemory;
      pDesc->AdapterLuid = desc.AdapterLuid;
    }

    return hr;
  }
  HRESULT STDMETHODCALLTYPE GetDesc1(DXGI_ADAPTER_DESC1 *pDesc) final {
    if (pDesc == nullptr)
      return E_INVALIDARG;

    DXGI_ADAPTER_DESC2 desc;
    HRESULT hr = GetDesc2(&desc);

    if (SUCCEEDED(hr)) {
      std::memcpy(pDesc->Description, desc.Description,
                  sizeof(pDesc->Description));

      pDesc->VendorId = desc.VendorId;
      pDesc->DeviceId = desc.DeviceId;
      pDesc->SubSysId = desc.SubSysId;
      pDesc->Revision = desc.Revision;
      pDesc->DedicatedVideoMemory = desc.DedicatedVideoMemory;
      pDesc->DedicatedSystemMemory = desc.DedicatedSystemMemory;
      pDesc->SharedSystemMemory = desc.SharedSystemMemory;
      pDesc->AdapterLuid = desc.AdapterLuid;
      pDesc->Flags = desc.Flags;
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE GetDesc2(DXGI_ADAPTER_DESC2 *pDesc) final {
    if (pDesc == nullptr)
      return E_INVALIDARG;

    std::memset(pDesc->Description, 0, sizeof(pDesc->Description));

    if (!options.customDeviceDesc.empty()) {
      str::transcodeString(
          pDesc->Description,
          sizeof(pDesc->Description) / sizeof(pDesc->Description[0]) - 1,
          options.customDeviceDesc.c_str(), options.customDeviceDesc.size());
    } else {
      wcscpy(pDesc->Description, (const wchar_t *)m_deivce->name()->cString(
                                     NS::UTF16StringEncoding));
    }

    if (options.customVendorId >= 0) {
      pDesc->VendorId = options.customVendorId;
    } else {
      pDesc->VendorId = 0x106B;
    }

    if (options.customDeviceId >= 0) {
      pDesc->DeviceId = options.customDeviceId;
    } else {
      pDesc->DeviceId = 0;
    }

    pDesc->SubSysId = 0;
    pDesc->Revision = 0;
    /**
    FIXME: divided by 2 is not necessary
     */
    pDesc->DedicatedVideoMemory = m_deivce->recommendedMaxWorkingSetSize() / 2;
    pDesc->DedicatedSystemMemory = 0;
    pDesc->SharedSystemMemory = 0;
    pDesc->AdapterLuid = LUID{1168, 1};
    pDesc->Flags = DXGI_ADAPTER_FLAG_NONE;
    pDesc->GraphicsPreemptionGranularity =
        DXGI_GRAPHICS_PREEMPTION_DMA_BUFFER_BOUNDARY;
    pDesc->ComputePreemptionGranularity =
        DXGI_COMPUTE_PREEMPTION_DMA_BUFFER_BOUNDARY;

    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE EnumOutputs(UINT Output,
                                        IDXGIOutput **ppOutput) final {
    InitReturnPtr(ppOutput);

    if (ppOutput == nullptr)
      return E_INVALIDARG;

    HMONITOR monitor = wsi::enumMonitors(Output);
    if (monitor == nullptr)
      return DXGI_ERROR_NOT_FOUND;

    *ppOutput = CreateOutput(this, monitor);
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE
  CheckInterfaceSupport(const GUID &guid, LARGE_INTEGER *umd_version) final {
    HRESULT hr = DXGI_ERROR_UNSUPPORTED;

    if (guid == __uuidof(IDXGIDevice))
      hr = S_OK;

    // We can't really reconstruct the version numbers
    // returned by Windows drivers from Metal
    if (SUCCEEDED(hr) && umd_version)
      umd_version->QuadPart = ~0ull;

    if (FAILED(hr)) {
      Logger::err("DXGI: CheckInterfaceSupport: Unsupported interface");
      Logger::err(str::format(guid));
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE
  RegisterHardwareContentProtectionTeardownStatusEvent(HANDLE event,
                                                       DWORD *cookie) override {
    assert(0 && "TODO");
  }

  void STDMETHODCALLTYPE
  UnregisterHardwareContentProtectionTeardownStatus(DWORD cookie) override {
    assert(0 && "TODO");
  }

  HRESULT STDMETHODCALLTYPE QueryVideoMemoryInfo(
      UINT NodeIndex, DXGI_MEMORY_SEGMENT_GROUP MemorySegmentGroup,
      DXGI_QUERY_VIDEO_MEMORY_INFO *pVideoMemoryInfo) override {
    if (NodeIndex > 0 || !pVideoMemoryInfo)
      return E_INVALIDARG;

    if (MemorySegmentGroup != DXGI_MEMORY_SEGMENT_GROUP_LOCAL &&
        MemorySegmentGroup != DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL)
      return E_INVALIDARG;

    // we don't actually care about MemorySegmentGroup
    pVideoMemoryInfo->Budget = m_deivce->recommendedMaxWorkingSetSize();
    pVideoMemoryInfo->CurrentUsage = m_deivce->currentAllocatedSize();
    pVideoMemoryInfo->AvailableForReservation = 0;
    pVideoMemoryInfo->CurrentReservation =
        mem_reserved_[uint32_t(MemorySegmentGroup)];
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE SetVideoMemoryReservation(
      UINT NodeIndex, DXGI_MEMORY_SEGMENT_GROUP MemorySegmentGroup,
      UINT64 Reservation) override {
    if (NodeIndex > 0)
      return E_INVALIDARG;

    if (MemorySegmentGroup != DXGI_MEMORY_SEGMENT_GROUP_LOCAL &&
        MemorySegmentGroup != DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL)
      return E_INVALIDARG;

    mem_reserved_[uint32_t(MemorySegmentGroup)] = Reservation;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE RegisterVideoMemoryBudgetChangeNotificationEvent(
      HANDLE event, DWORD *cookie) override {
    assert(0 && "TODO");
  }

  void STDMETHODCALLTYPE
  UnregisterVideoMemoryBudgetChangeNotification(DWORD cookie) override {
    assert(0 && "TODO");
  }

  MTL::Device *STDMETHODCALLTYPE GetMTLDevice() final { return m_deivce.ptr(); }

  HRESULT STDMETHODCALLTYPE QueryFormatDesc(DXGI_FORMAT Format,
                                            MTL_FORMAT_DESC *pMtlDesc) final {
    if (!pMtlDesc) {
      return E_INVALIDARG;
    }

    pMtlDesc->PixelFormat = MTL::PixelFormatInvalid;
    pMtlDesc->VertexFormat = MTL::VertexFormatInvalid;
    pMtlDesc->AttributeFormat = MTL::AttributeFormatInvalid;
    pMtlDesc->BytesPerTexel = 0;
    pMtlDesc->IsCompressed = false;
    pMtlDesc->SupportBackBuffer = false;
    pMtlDesc->Capability = FormatCapability::None;
    pMtlDesc->DepthStencilFlag = 0;
    pMtlDesc->Typeless = 0;

    switch (Format) {
    case DXGI_FORMAT_R32G32B32A32_TYPELESS: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRGBA32Uint;
      pMtlDesc->BytesPerTexel = 16;
      pMtlDesc->Typeless = 1;
      break;
    }
    case DXGI_FORMAT_R32G32B32A32_UINT: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRGBA32Uint;
      pMtlDesc->VertexFormat = MTL::VertexFormatUInt4;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatUInt4;
      pMtlDesc->BytesPerTexel = 16;
      break;
    }
    case DXGI_FORMAT_R32G32B32A32_SINT: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRGBA32Sint;
      pMtlDesc->VertexFormat = MTL::VertexFormatInt4;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatInt4;
      pMtlDesc->BytesPerTexel = 16;
      break;
    }
    case DXGI_FORMAT_R32G32B32A32_FLOAT: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRGBA32Float;
      pMtlDesc->VertexFormat = MTL::VertexFormatFloat4;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatFloat4;
      pMtlDesc->BytesPerTexel = 16;
      break;
    }
    case DXGI_FORMAT_R32G32B32_TYPELESS: {
      pMtlDesc->BytesPerTexel = 12;
      pMtlDesc->Typeless = 1;
      break;
    }
    case DXGI_FORMAT_R32G32B32_UINT: {
      pMtlDesc->VertexFormat = MTL::VertexFormatUInt3;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatUInt3;
      pMtlDesc->BytesPerTexel = 12;
      break;
    }
    case DXGI_FORMAT_R32G32B32_SINT: {
      pMtlDesc->VertexFormat = MTL::VertexFormatInt3;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatInt3;
      pMtlDesc->BytesPerTexel = 12;
      break;
    }
    case DXGI_FORMAT_R32G32B32_FLOAT: {
      pMtlDesc->VertexFormat = MTL::VertexFormatFloat3;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatFloat3;
      pMtlDesc->BytesPerTexel = 12;
      break;
    }
    case DXGI_FORMAT_R16G16B16A16_TYPELESS: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRGBA16Uint;
      pMtlDesc->BytesPerTexel = 8;
      pMtlDesc->Typeless = 1;
      break;
    }
    case DXGI_FORMAT_R16G16B16A16_FLOAT: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRGBA16Float;
      pMtlDesc->VertexFormat = MTL::VertexFormatHalf4;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatHalf4;
      pMtlDesc->BytesPerTexel = 8;
      pMtlDesc->SupportBackBuffer = true; // 11.1
      break;
    }
    case DXGI_FORMAT_R16G16B16A16_UNORM: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRGBA16Unorm;
      pMtlDesc->VertexFormat = MTL::VertexFormatUShort4Normalized;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatUShort4Normalized;
      pMtlDesc->BytesPerTexel = 8;
      break;
    }
    case DXGI_FORMAT_R16G16B16A16_UINT: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRGBA16Uint;
      pMtlDesc->VertexFormat = MTL::VertexFormatUShort4;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatUShort4;
      pMtlDesc->BytesPerTexel = 8;
      break;
    }
    case DXGI_FORMAT_R16G16B16A16_SNORM: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRGBA16Snorm;
      pMtlDesc->VertexFormat = MTL::VertexFormatShort4Normalized;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatShort4Normalized;
      pMtlDesc->BytesPerTexel = 8;
      break;
    }
    case DXGI_FORMAT_R16G16B16A16_SINT: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRGBA16Sint;
      pMtlDesc->VertexFormat = MTL::VertexFormatShort4;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatShort4;
      pMtlDesc->BytesPerTexel = 8;
      break;
    }
    case DXGI_FORMAT_R32G32_TYPELESS: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRG32Uint;
      pMtlDesc->BytesPerTexel = 8;
      pMtlDesc->Typeless = 1;
      break;
    }
    case DXGI_FORMAT_R32G32_FLOAT: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRG32Float;
      pMtlDesc->VertexFormat = MTL::VertexFormatFloat2;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatFloat2;
      pMtlDesc->BytesPerTexel = 8;
      break;
    }
    case DXGI_FORMAT_R32G32_UINT: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRG32Uint;
      pMtlDesc->VertexFormat = MTL::VertexFormatUInt2;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatUInt2;
      pMtlDesc->BytesPerTexel = 8;
      break;
    }
    case DXGI_FORMAT_R32G32_SINT: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRG32Sint;
      pMtlDesc->VertexFormat = MTL::VertexFormatInt2;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatInt2;
      pMtlDesc->BytesPerTexel = 8;
      break;
    }
    case DXGI_FORMAT_R32G8X24_TYPELESS: {
      pMtlDesc->PixelFormat = MTL::PixelFormatDepth32Float_Stencil8;
      pMtlDesc->BytesPerTexel = 8;
      pMtlDesc->DepthStencilFlag = 3; // ?
      pMtlDesc->Typeless = 1;
      break;
    }
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT: {
      pMtlDesc->PixelFormat = MTL::PixelFormatDepth32Float_Stencil8;
      pMtlDesc->BytesPerTexel = 8;
      pMtlDesc->DepthStencilFlag = 3; // ?
      break;
    }
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS: {
      pMtlDesc->PixelFormat = MTL::PixelFormatDepth32Float_Stencil8;
      pMtlDesc->BytesPerTexel = 8;
      pMtlDesc->DepthStencilFlag = 1; // ?
      pMtlDesc->Typeless = 1;
      break;
    }
      return E_FAIL;
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT: {
      pMtlDesc->PixelFormat = MTL::PixelFormatX32_Stencil8;
      pMtlDesc->BytesPerTexel = 8;
      pMtlDesc->Typeless = 1;
      break;
    }
    case DXGI_FORMAT_R10G10B10A2_TYPELESS: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRGB10A2Uint;
      pMtlDesc->BytesPerTexel = 4;
      pMtlDesc->Typeless = 1;
      break;
    }
    case DXGI_FORMAT_R10G10B10A2_UNORM: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRGB10A2Unorm;
      pMtlDesc->VertexFormat = MTL::VertexFormatUInt1010102Normalized;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatUInt1010102Normalized;
      pMtlDesc->BytesPerTexel = 4;
      pMtlDesc->SupportBackBuffer = true; // 11.1
      break;
    }
    case DXGI_FORMAT_R10G10B10A2_UINT: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRGB10A2Uint;
      pMtlDesc->BytesPerTexel = 4;
      break;
    }
    case DXGI_FORMAT_R11G11B10_FLOAT: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRG11B10Float;
      pMtlDesc->VertexFormat = MTL::VertexFormatFloatRG11B10;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatFloatRG11B10;
      pMtlDesc->BytesPerTexel = 4;
      break;
    }
    case DXGI_FORMAT_R8G8B8A8_TYPELESS: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRGBA8Unorm;
      pMtlDesc->BytesPerTexel = 4;
      pMtlDesc->Typeless = 1;
      break;
    }
    case DXGI_FORMAT_R8G8B8A8_UNORM: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRGBA8Unorm;
      pMtlDesc->VertexFormat = MTL::VertexFormatUChar4Normalized;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatUChar4Normalized;
      pMtlDesc->BytesPerTexel = 4;
      pMtlDesc->SupportBackBuffer = TRUE; // 11.1, swizzle from BGRA8Unorm
      // FIXME: backbuffer format not supported!
      // need some workaround for GetDisplayModeList
      break;
    }
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRGBA8Unorm_sRGB;
      pMtlDesc->BytesPerTexel = 4;
      pMtlDesc->SupportBackBuffer = TRUE; // 11.1, swizzle from BGRA8Unorm
      break;
    }
    case DXGI_FORMAT_R8G8B8A8_UINT: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRGBA8Uint;
      pMtlDesc->VertexFormat = MTL::VertexFormatUChar4;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatUChar4;
      pMtlDesc->BytesPerTexel = 4;
      break;
    }
    case DXGI_FORMAT_R8G8B8A8_SNORM: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRGBA8Snorm;
      pMtlDesc->VertexFormat = MTL::VertexFormatChar4Normalized;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatChar4Normalized;
      pMtlDesc->BytesPerTexel = 4;
      break;
    }
    case DXGI_FORMAT_R8G8B8A8_SINT: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRGBA8Sint;
      pMtlDesc->VertexFormat = MTL::VertexFormatChar4;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatChar4;
      pMtlDesc->BytesPerTexel = 4;
      break;
    }
    case DXGI_FORMAT_R16G16_TYPELESS: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRG16Uint;
      pMtlDesc->BytesPerTexel = 4;
      pMtlDesc->Typeless = 1;
      break;
    }
    case DXGI_FORMAT_R16G16_FLOAT: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRG16Float;
      pMtlDesc->VertexFormat = MTL::VertexFormatHalf2;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatHalf2;
      pMtlDesc->BytesPerTexel = 4;
      break;
    }
    case DXGI_FORMAT_R16G16_UNORM: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRG16Unorm;
      pMtlDesc->VertexFormat = MTL::VertexFormatUShort2Normalized;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatUShort2Normalized;
      pMtlDesc->BytesPerTexel = 4;
      break;
    }
    case DXGI_FORMAT_R16G16_UINT: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRG16Uint;
      pMtlDesc->VertexFormat = MTL::VertexFormatUShort2;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatUShort2;
      pMtlDesc->BytesPerTexel = 4;
      break;
    }
    case DXGI_FORMAT_R16G16_SNORM: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRG16Snorm;
      pMtlDesc->VertexFormat = MTL::VertexFormatShort2Normalized;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatShort2Normalized;
      pMtlDesc->BytesPerTexel = 4;
      break;
    }
    case DXGI_FORMAT_R16G16_SINT: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRG16Sint;
      pMtlDesc->VertexFormat = MTL::VertexFormatShort2;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatShort2;
      pMtlDesc->BytesPerTexel = 4;
      break;
    }
    case DXGI_FORMAT_R32_TYPELESS: {
      pMtlDesc->PixelFormat = MTL::PixelFormatR32Uint;
      pMtlDesc->BytesPerTexel = 4;
      pMtlDesc->Typeless = 1;
      break;
    }
    case DXGI_FORMAT_D32_FLOAT: {
      pMtlDesc->PixelFormat = MTL::PixelFormatDepth32Float;
      pMtlDesc->BytesPerTexel = 4;
      break;
    }
    case DXGI_FORMAT_R32_FLOAT: {
      pMtlDesc->PixelFormat = MTL::PixelFormatR32Float;
      pMtlDesc->VertexFormat = MTL::VertexFormatFloat;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatFloat;
      pMtlDesc->BytesPerTexel = 4;
      break;
    }
    case DXGI_FORMAT_R32_UINT: {
      pMtlDesc->PixelFormat = MTL::PixelFormatR32Uint;
      pMtlDesc->VertexFormat = MTL::VertexFormatUInt;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatUInt;
      pMtlDesc->BytesPerTexel = 4;
      break;
    }
    case DXGI_FORMAT_R32_SINT: {
      pMtlDesc->PixelFormat = MTL::PixelFormatR32Sint;
      pMtlDesc->VertexFormat = MTL::VertexFormatInt;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatInt;
      pMtlDesc->BytesPerTexel = 4;
      break;
    }
    case DXGI_FORMAT_R24G8_TYPELESS: {
      pMtlDesc->DepthStencilFlag = 3;
      if (m_deivce->depth24Stencil8PixelFormatSupported()) {
        pMtlDesc->PixelFormat = MTL::PixelFormatDepth24Unorm_Stencil8;
      } else {
        ERR_ONCE("depth24Stencil8 is not supported.");
        pMtlDesc->PixelFormat = MTL::PixelFormatDepth32Float_Stencil8;
      }
      pMtlDesc->Typeless = 1;
      break;
    }
    case DXGI_FORMAT_D24_UNORM_S8_UINT: {
      pMtlDesc->DepthStencilFlag = 3;
      if (m_deivce->depth24Stencil8PixelFormatSupported()) {
        pMtlDesc->PixelFormat = MTL::PixelFormatDepth24Unorm_Stencil8;
      } else {
        ERR_ONCE("depth24Stencil8 is not supported.");
        pMtlDesc->PixelFormat = MTL::PixelFormatDepth32Float_Stencil8;
      }
      break;
    }
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS: {
      pMtlDesc->DepthStencilFlag = 1;
      if (m_deivce->depth24Stencil8PixelFormatSupported()) {
        pMtlDesc->PixelFormat = MTL::PixelFormatDepth24Unorm_Stencil8;
      } else {
        ERR_ONCE("depth24Stencil8 is not supported.");
        pMtlDesc->PixelFormat = MTL::PixelFormatDepth32Float_Stencil8;
      }
      pMtlDesc->Typeless = 1;
      break;
    }
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT: {
      pMtlDesc->DepthStencilFlag = 2;
      if (m_deivce->depth24Stencil8PixelFormatSupported()) {
        pMtlDesc->PixelFormat = MTL::PixelFormatX24_Stencil8;
      } else {
        ERR_ONCE("depth24Stencil8 is not supported.");
        pMtlDesc->PixelFormat = MTL::PixelFormatX32_Stencil8;
      }
      pMtlDesc->Typeless = 1;
      break;
    }
    case DXGI_FORMAT_R8G8_TYPELESS: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRG8Uint;
      pMtlDesc->BytesPerTexel = 2;
      pMtlDesc->Typeless = 1;
      break;
    }
    case DXGI_FORMAT_R8G8_UNORM: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRG8Unorm;
      pMtlDesc->VertexFormat = MTL::VertexFormatUChar2Normalized;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatUChar2Normalized;
      pMtlDesc->BytesPerTexel = 2;
      break;
    }
    case DXGI_FORMAT_R8G8_UINT: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRG8Uint;
      pMtlDesc->VertexFormat = MTL::VertexFormatUChar2;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatUChar2;
      pMtlDesc->BytesPerTexel = 2;
      break;
    }
    case DXGI_FORMAT_R8G8_SNORM: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRG8Snorm;
      pMtlDesc->VertexFormat = MTL::VertexFormatChar2Normalized;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatChar2Normalized;
      pMtlDesc->BytesPerTexel = 2;
      break;
    }
    case DXGI_FORMAT_R8G8_SINT: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRG8Sint;
      pMtlDesc->VertexFormat = MTL::VertexFormatChar2;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatChar2;
      pMtlDesc->BytesPerTexel = 2;
      break;
    }
    case DXGI_FORMAT_R16_TYPELESS: {
      pMtlDesc->PixelFormat = MTL::PixelFormatR16Uint;
      pMtlDesc->BytesPerTexel = 2;
      pMtlDesc->Typeless = 1;
      break;
    }
    case DXGI_FORMAT_R16_FLOAT: {
      pMtlDesc->PixelFormat = MTL::PixelFormatR16Float;
      pMtlDesc->VertexFormat = MTL::VertexFormatHalf;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatHalf;
      pMtlDesc->BytesPerTexel = 2;
      break;
    }
    case DXGI_FORMAT_D16_UNORM: {
      pMtlDesc->PixelFormat = MTL::PixelFormatDepth16Unorm;
      pMtlDesc->BytesPerTexel = 2;
      break;
    }
    case DXGI_FORMAT_R16_UNORM: {
      pMtlDesc->PixelFormat = MTL::PixelFormatR16Unorm;
      pMtlDesc->VertexFormat = MTL::VertexFormatUShortNormalized;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatUShortNormalized;
      pMtlDesc->BytesPerTexel = 2;
      break;
    }
    case DXGI_FORMAT_R16_UINT: {
      pMtlDesc->PixelFormat = MTL::PixelFormatR16Uint;
      pMtlDesc->VertexFormat = MTL::VertexFormatUShort;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatUShort;
      pMtlDesc->BytesPerTexel = 2;
      break;
    }
    case DXGI_FORMAT_R16_SNORM: {
      pMtlDesc->PixelFormat = MTL::PixelFormatR16Snorm;
      pMtlDesc->VertexFormat = MTL::VertexFormatShortNormalized;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatShortNormalized;
      pMtlDesc->BytesPerTexel = 2;
      break;
    }
    case DXGI_FORMAT_R16_SINT: {
      pMtlDesc->PixelFormat = MTL::PixelFormatR16Sint;
      pMtlDesc->VertexFormat = MTL::VertexFormatShort;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatShort;
      pMtlDesc->BytesPerTexel = 2;
      break;
    }
    case DXGI_FORMAT_R8_TYPELESS: {
      pMtlDesc->PixelFormat = MTL::PixelFormatR8Uint;
      pMtlDesc->BytesPerTexel = 1;
      pMtlDesc->Typeless = 1;
      break;
    }
    case DXGI_FORMAT_R8_UNORM: {
      pMtlDesc->PixelFormat = MTL::PixelFormatR8Unorm;
      pMtlDesc->VertexFormat = MTL::VertexFormatUCharNormalized;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatUCharNormalized;
      pMtlDesc->BytesPerTexel = 1;
      break;
    }
    case DXGI_FORMAT_R8_UINT: {
      pMtlDesc->PixelFormat = MTL::PixelFormatR8Uint;
      pMtlDesc->VertexFormat = MTL::VertexFormatUChar;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatUChar;
      pMtlDesc->BytesPerTexel = 1;
      break;
    }
    case DXGI_FORMAT_R8_SNORM: {
      pMtlDesc->PixelFormat = MTL::PixelFormatR8Snorm;
      pMtlDesc->VertexFormat = MTL::VertexFormatCharNormalized;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatCharNormalized;
      pMtlDesc->BytesPerTexel = 1;
      break;
    }
    case DXGI_FORMAT_R8_SINT: {
      pMtlDesc->PixelFormat = MTL::PixelFormatR8Sint;
      pMtlDesc->VertexFormat = MTL::VertexFormatChar;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatChar;
      pMtlDesc->BytesPerTexel = 1;
      break;
    }
    case DXGI_FORMAT_A8_UNORM: {
      pMtlDesc->PixelFormat = MTL::PixelFormatA8Unorm;
      pMtlDesc->BytesPerTexel = 1;
      break;
    }
    case DXGI_FORMAT_R1_UNORM: {
      return E_FAIL;
    }
    case DXGI_FORMAT_R9G9B9E5_SHAREDEXP: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRGB9E5Float;
      pMtlDesc->VertexFormat = MTL::VertexFormatFloatRGB9E5;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatFloatRGB9E5;
      pMtlDesc->BytesPerTexel = 4;
      break;
    }
    case DXGI_FORMAT_R8G8_B8G8_UNORM: {
      pMtlDesc->PixelFormat = MTL::PixelFormatBGRG422;
      pMtlDesc->BytesPerTexel = 4;
      break;
    }
    case DXGI_FORMAT_G8R8_G8B8_UNORM: {
      pMtlDesc->PixelFormat = MTL::PixelFormatGBGR422;
      pMtlDesc->BytesPerTexel = 4;
      break;
    }
    case DXGI_FORMAT_BC1_TYPELESS: {
      pMtlDesc->PixelFormat = MTL::PixelFormatBC1_RGBA;
      pMtlDesc->IsCompressed = true;
      pMtlDesc->Typeless = 1;
      pMtlDesc->BlockSize = 8;
      break;
    }
    /**
    from doc:
    Reinterpretation of image data between pixel formats is supported:
    - sRGB and non-sRGB forms of the same compressed format
    */
    case DXGI_FORMAT_BC1_UNORM: {
      pMtlDesc->PixelFormat = MTL::PixelFormatBC1_RGBA;
      pMtlDesc->IsCompressed = true;
      pMtlDesc->BlockSize = 8;
      break;
    }
    case DXGI_FORMAT_BC1_UNORM_SRGB: {
      pMtlDesc->PixelFormat = MTL::PixelFormatBC1_RGBA_sRGB;
      pMtlDesc->IsCompressed = true;
      pMtlDesc->BlockSize = 8;
      break;
    }
    case DXGI_FORMAT_BC2_TYPELESS: {
      pMtlDesc->PixelFormat = MTL::PixelFormatBC2_RGBA;
      pMtlDesc->IsCompressed = true;
      pMtlDesc->Typeless = 1;
      pMtlDesc->BlockSize = 16;
      break;
    }
    case DXGI_FORMAT_BC2_UNORM: {
      pMtlDesc->PixelFormat = MTL::PixelFormatBC2_RGBA;
      pMtlDesc->IsCompressed = true;
      pMtlDesc->BlockSize = 16;
      break;
    }
    case DXGI_FORMAT_BC2_UNORM_SRGB: {
      pMtlDesc->PixelFormat = MTL::PixelFormatBC2_RGBA_sRGB;
      pMtlDesc->IsCompressed = true;
      pMtlDesc->BlockSize = 16;
      break;
    }
    case DXGI_FORMAT_BC3_TYPELESS: {
      pMtlDesc->PixelFormat = MTL::PixelFormatBC3_RGBA;
      pMtlDesc->IsCompressed = true;
      pMtlDesc->Typeless = 1;
      pMtlDesc->BlockSize = 16;
      break;
    }
    case DXGI_FORMAT_BC3_UNORM: {
      pMtlDesc->PixelFormat = MTL::PixelFormatBC3_RGBA;
      pMtlDesc->IsCompressed = true;
      pMtlDesc->BlockSize = 16;
      break;
    }
    case DXGI_FORMAT_BC3_UNORM_SRGB: {
      pMtlDesc->PixelFormat = MTL::PixelFormatBC3_RGBA_sRGB;
      pMtlDesc->IsCompressed = true;
      pMtlDesc->BlockSize = 16;
      break;
    }
    case DXGI_FORMAT_BC4_TYPELESS: {
      pMtlDesc->PixelFormat = MTL::PixelFormatBC4_RUnorm;
      pMtlDesc->IsCompressed = true;
      pMtlDesc->Typeless = 1;
      pMtlDesc->BlockSize = 8;
      break;
    }
    case DXGI_FORMAT_BC4_UNORM: {
      pMtlDesc->PixelFormat = MTL::PixelFormatBC4_RUnorm;
      pMtlDesc->IsCompressed = true;
      pMtlDesc->BlockSize = 8;
      break;
    }
    case DXGI_FORMAT_BC4_SNORM: {
      pMtlDesc->PixelFormat = MTL::PixelFormatBC4_RSnorm;
      pMtlDesc->IsCompressed = true;
      pMtlDesc->BlockSize = 8;
      break;
    }
    case DXGI_FORMAT_BC5_TYPELESS: {
      pMtlDesc->PixelFormat = MTL::PixelFormatBC5_RGUnorm;
      pMtlDesc->IsCompressed = true;
      pMtlDesc->Typeless = 1;
      pMtlDesc->BlockSize = 16;
      break;
    }
    case DXGI_FORMAT_BC5_UNORM: {
      pMtlDesc->PixelFormat = MTL::PixelFormatBC5_RGUnorm;
      pMtlDesc->IsCompressed = true;
      pMtlDesc->BlockSize = 16;
      break;
    }
    case DXGI_FORMAT_BC5_SNORM: {
      pMtlDesc->PixelFormat = MTL::PixelFormatBC5_RGSnorm;
      pMtlDesc->IsCompressed = true;
      pMtlDesc->BlockSize = 16;
      break;
    }
    case DXGI_FORMAT_B5G6R5_UNORM: {
      pMtlDesc->PixelFormat = MTL::PixelFormatB5G6R5Unorm;
      pMtlDesc->BytesPerTexel = 2;
      break;
    }
    case DXGI_FORMAT_B5G5R5A1_UNORM: {
      pMtlDesc->PixelFormat = MTL::PixelFormatBGR5A1Unorm;
      pMtlDesc->BytesPerTexel = 2;
      break;
    }
    case DXGI_FORMAT_B8G8R8A8_UNORM: {
      pMtlDesc->PixelFormat = MTL::PixelFormatBGRA8Unorm;
      pMtlDesc->VertexFormat = MTL::VertexFormatUChar4Normalized_BGRA;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatUChar4Normalized_BGRA;
      pMtlDesc->BytesPerTexel = 4;
      pMtlDesc->SupportBackBuffer = true; // 11.1
      break;
    }
    case DXGI_FORMAT_B8G8R8X8_UNORM: {
      pMtlDesc->PixelFormat = MTL::PixelFormatBGRA8Unorm;
      pMtlDesc->BytesPerTexel = 4;
      pMtlDesc->SupportBackBuffer = true;
      break;
    }
    case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
      return E_FAIL;
    case DXGI_FORMAT_B8G8R8A8_TYPELESS: {
      pMtlDesc->PixelFormat = MTL::PixelFormatBGRA8Unorm;
      pMtlDesc->BytesPerTexel = 4;
      pMtlDesc->Typeless = true;
      break;
    }
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: {
      pMtlDesc->PixelFormat = MTL::PixelFormatBGRA8Unorm_sRGB;
      pMtlDesc->BytesPerTexel = 4;
      pMtlDesc->SupportBackBuffer = true; // 11.1
      break;
    }
    case DXGI_FORMAT_B8G8R8X8_TYPELESS: {
      pMtlDesc->PixelFormat = MTL::PixelFormatBGRA8Unorm;
      pMtlDesc->BytesPerTexel = 4;
      pMtlDesc->Typeless = 1;
      break;
    }
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB: {
      pMtlDesc->PixelFormat = MTL::PixelFormatBGRA8Unorm_sRGB;
      pMtlDesc->BytesPerTexel = 4;
      pMtlDesc->SupportBackBuffer = true;
      break;
    }
    case DXGI_FORMAT_BC6H_TYPELESS: {
      pMtlDesc->PixelFormat = MTL::PixelFormatBC6H_RGBUfloat;
      pMtlDesc->IsCompressed = true;
      pMtlDesc->Typeless = 1;
      pMtlDesc->BlockSize = 16;
      break;
    }
    case DXGI_FORMAT_BC6H_UF16: {
      pMtlDesc->PixelFormat = MTL::PixelFormatBC6H_RGBUfloat;
      pMtlDesc->IsCompressed = true;
      pMtlDesc->BlockSize = 16;
      break;
    }
    case DXGI_FORMAT_BC6H_SF16: {
      pMtlDesc->PixelFormat = MTL::PixelFormatBC6H_RGBFloat;
      pMtlDesc->IsCompressed = true;
      pMtlDesc->BlockSize = 16;
      break;
    }
    case DXGI_FORMAT_BC7_TYPELESS: {
      pMtlDesc->PixelFormat = MTL::PixelFormatBC7_RGBAUnorm;
      pMtlDesc->IsCompressed = true;
      pMtlDesc->Typeless = 1;
      pMtlDesc->BlockSize = 16;
      break;
    }
    case DXGI_FORMAT_BC7_UNORM: {
      pMtlDesc->PixelFormat = MTL::PixelFormatBC7_RGBAUnorm;
      pMtlDesc->IsCompressed = true;
      pMtlDesc->BlockSize = 16;
      break;
    }
    case DXGI_FORMAT_BC7_UNORM_SRGB: {
      pMtlDesc->PixelFormat = MTL::PixelFormatBC7_RGBAUnorm_sRGB;
      pMtlDesc->IsCompressed = true;
      pMtlDesc->BlockSize = 16;
      break;
    }
    case DXGI_FORMAT_AYUV:
    case DXGI_FORMAT_Y410:
    case DXGI_FORMAT_Y416:
    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_P010:
    case DXGI_FORMAT_P016:
    case DXGI_FORMAT_420_OPAQUE:
    case DXGI_FORMAT_YUY2:
    case DXGI_FORMAT_Y210:
    case DXGI_FORMAT_Y216:
    case DXGI_FORMAT_NV11:
    case DXGI_FORMAT_AI44:
    case DXGI_FORMAT_IA44:
    case DXGI_FORMAT_P8:
    case DXGI_FORMAT_A8P8:
    case DXGI_FORMAT_B4G4R4A4_UNORM:
    case DXGI_FORMAT_P208:
    case DXGI_FORMAT_V208:
    case DXGI_FORMAT_V408:
    case DXGI_FORMAT_FORCE_UINT:
    case DXGI_FORMAT_UNKNOWN:
      return E_FAIL;
    }
    pMtlDesc->Capability =
        format_inspector.textureCapabilities[pMtlDesc->PixelFormat];

    return S_OK;
  };

  virtual int STDMETHODCALLTYPE GetConfigInt(const char *name,
                                             int defaultValue) override {
    return config.getOption<int>(name, defaultValue);
  }

  virtual float STDMETHODCALLTYPE GetConfigFloat(const char *name,
                                                 float defaultValue) override {
    return config.getOption<float>(name, defaultValue);
  }

private:
  Obj<MTL::Device> m_deivce;
  Com<IDXGIFactory> m_factory;
  FormatCapabilityInspector format_inspector;
  DxgiOptions options;
  Config &config;
  uint64_t mem_reserved_[2] = {0, 0};
};

Com<IMTLDXGIAdatper> CreateAdapter(MTL::Device *pDevice,
                                   IDXGIFactory2 *pFactory, Config &config) {
  return Com<IMTLDXGIAdatper>::transfer(new MTLDXGIAdatper(pDevice, pFactory, config));
}

} // namespace dxmt