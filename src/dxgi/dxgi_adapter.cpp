
#include "Metal/MTLPixelFormat.hpp"
#include "Metal/MTLStageInputOutputDescriptor.hpp"
#include "Metal/MTLVertexDescriptor.hpp"
#include "com/com_guid.hpp"
#include "util_string.hpp"
#include "log/log.hpp"
#include "wsi_monitor.hpp"
#include "dxgi_interfaces.h"
#include "dxgi_object.hpp"
#include "objc_pointer.hpp"

namespace dxmt {

Com<IDXGIOutput> CreateOutput(IMTLDXGIAdatper *pAadapter, HMONITOR monitor);

class MTLDXGIAdatper : public MTLDXGIObject<IMTLDXGIAdatper> {
public:
  MTLDXGIAdatper(MTL::Device *device, IDXGIFactory *factory)
      : m_deivce(device), m_factory(factory) {
    m_deivce->retain();
  };
  ~MTLDXGIAdatper() { m_deivce->release(); }
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                           void **ppvObject) final {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(IDXGIObject) ||
        riid == __uuidof(IDXGIAdapter) || riid == __uuidof(IDXGIAdapter1) ||
        riid == __uuidof(IDXGIAdapter2) || riid == __uuidof(IMTLDXGIAdatper)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(IDXGIAdapter), riid)) {
      Logger::warn("DxgiAdapter::QueryInterface: Unknown interface query");
      Logger::warn(str::format(riid));
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

    //   pDesc->VendorId = deviceProp.vendorID;
    //   pDesc->DeviceId = deviceProp.deviceID;
    //   pDesc->SubSysId = 0;
    //   pDesc->Revision = 0;
    //   pDesc->DedicatedVideoMemory = deviceMemory;
    //   pDesc->DedicatedSystemMemory = 0;
    //   pDesc->SharedSystemMemory = sharedMemory;
    //   pDesc->AdapterLuid = LUID{0, 0};
    //   pDesc->Flags = DXGI_ADAPTER_FLAG3_NONE;
    //   pDesc->GraphicsPreemptionGranularity =
    //       DXGI_GRAPHICS_PREEMPTION_DMA_BUFFER_BOUNDARY;
    //   pDesc->ComputePreemptionGranularity =
    //       DXGI_COMPUTE_PREEMPTION_DMA_BUFFER_BOUNDARY;

    // TODO

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

  MTL::Device *STDMETHODCALLTYPE GetMTLDevice() final { return m_deivce.ptr(); }

  HRESULT STDMETHODCALLTYPE QueryFormatDesc(DXGI_FORMAT Format,
                                            METAL_FORMAT_DESC *pMtlDesc) final {
    if (!pMtlDesc) {
      return E_INVALIDARG;
    }

    pMtlDesc->PixelFormat = MTL::PixelFormatInvalid;
    pMtlDesc->VertexFormat = MTL::VertexFormatInvalid;
    pMtlDesc->AttributeFormat = MTL::AttributeFormatInvalid;
    pMtlDesc->Alignment = 0;
    pMtlDesc->Stride = 0;
    pMtlDesc->IsCompressed = false;

    switch (Format) {
    case DXGI_FORMAT_R32G32B32A32_TYPELESS: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRGBA32Uint;
      pMtlDesc->Stride = 16;
      pMtlDesc->Alignment = 16;
      break;
    }
    case DXGI_FORMAT_R32G32B32A32_UINT: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRGBA32Uint;
      pMtlDesc->VertexFormat = MTL::VertexFormatUInt4;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatUInt4;
      pMtlDesc->Stride = 16;
      pMtlDesc->Alignment = 16;
      break;
    }
    case DXGI_FORMAT_R32G32B32A32_SINT: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRGBA32Sint;
      pMtlDesc->VertexFormat = MTL::VertexFormatInt4;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatInt4;
      pMtlDesc->Stride = 16;
      pMtlDesc->Alignment = 16;
      break;
    }
    case DXGI_FORMAT_R32G32B32A32_FLOAT: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRGBA32Float;
      pMtlDesc->VertexFormat = MTL::VertexFormatFloat4;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatFloat4;
      pMtlDesc->Stride = 16;
      pMtlDesc->Alignment = 16;
      break;
    }
    case DXGI_FORMAT_R32G32B32_TYPELESS: {
      pMtlDesc->Stride = 12;
      pMtlDesc->Alignment = 16;
      break;
    }
    case DXGI_FORMAT_R32G32B32_UINT: {
      pMtlDesc->VertexFormat = MTL::VertexFormatUInt3;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatUInt3;
      pMtlDesc->Stride = 12;
      pMtlDesc->Alignment = 16;
      break;
    }
    case DXGI_FORMAT_R32G32B32_SINT: {
      pMtlDesc->VertexFormat = MTL::VertexFormatInt3;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatInt3;
      pMtlDesc->Stride = 12;
      pMtlDesc->Alignment = 16;
      break;
    }
    case DXGI_FORMAT_R32G32B32_FLOAT: {
      pMtlDesc->VertexFormat = MTL::VertexFormatFloat3;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatFloat3;
      pMtlDesc->Stride = 12;
      pMtlDesc->Alignment = 16;
      break;
    }
    case DXGI_FORMAT_R16G16B16A16_TYPELESS: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRGBA16Uint;
      pMtlDesc->Stride = 8;
      pMtlDesc->Alignment = 8;
      break;
    }
    case DXGI_FORMAT_R16G16B16A16_FLOAT: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRGBA16Float;
      pMtlDesc->VertexFormat = MTL::VertexFormatHalf4;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatHalf4;
      pMtlDesc->Stride = 8;
      pMtlDesc->Alignment = 8;
      break;
    }
    case DXGI_FORMAT_R16G16B16A16_UNORM: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRGBA16Unorm;
      pMtlDesc->VertexFormat = MTL::VertexFormatUShort4Normalized;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatUShort4Normalized;
      pMtlDesc->Stride = 8;
      pMtlDesc->Alignment = 8;
      break;
    }
    case DXGI_FORMAT_R16G16B16A16_UINT: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRGBA16Uint;
      pMtlDesc->VertexFormat = MTL::VertexFormatUShort4;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatUShort4;
      pMtlDesc->Stride = 8;
      pMtlDesc->Alignment = 8;
      break;
    }
    case DXGI_FORMAT_R16G16B16A16_SNORM: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRGBA16Snorm;
      pMtlDesc->VertexFormat = MTL::VertexFormatShort4Normalized;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatShort4Normalized;
      pMtlDesc->Stride = 8;
      pMtlDesc->Alignment = 8;
      break;
    }
    case DXGI_FORMAT_R16G16B16A16_SINT: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRGBA16Sint;
      pMtlDesc->VertexFormat = MTL::VertexFormatShort4;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatShort4;
      pMtlDesc->Stride = 8;
      pMtlDesc->Alignment = 8;
      break;
    }
    case DXGI_FORMAT_R32G32_TYPELESS: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRG32Uint;
      pMtlDesc->Stride = 8;
      pMtlDesc->Alignment = 8;
      break;
    }
    case DXGI_FORMAT_R32G32_FLOAT: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRG32Float;
      pMtlDesc->VertexFormat = MTL::VertexFormatFloat2;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatFloat2;
      pMtlDesc->Stride = 8;
      pMtlDesc->Alignment = 8;
      break;
    }
    case DXGI_FORMAT_R32G32_UINT: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRG32Uint;
      pMtlDesc->VertexFormat = MTL::VertexFormatUInt2;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatUInt2;
      pMtlDesc->Stride = 8;
      pMtlDesc->Alignment = 8;
      break;
    }
    case DXGI_FORMAT_R32G32_SINT: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRG32Sint;
      pMtlDesc->VertexFormat = MTL::VertexFormatInt2;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatInt2;
      pMtlDesc->Stride = 8;
      pMtlDesc->Alignment = 8;
      break;
    }
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
      return E_FAIL;
    case DXGI_FORMAT_R10G10B10A2_TYPELESS: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRGB10A2Uint;
      pMtlDesc->Stride = 4;
      pMtlDesc->Alignment = 4;
      break;
    }
    case DXGI_FORMAT_R10G10B10A2_UNORM: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRGB10A2Unorm;
      pMtlDesc->VertexFormat = MTL::VertexFormatUInt1010102Normalized;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatUInt1010102Normalized;
      pMtlDesc->Stride = 4;
      pMtlDesc->Alignment = 4;
      break;
    }
    case DXGI_FORMAT_R10G10B10A2_UINT: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRGB10A2Uint;
      pMtlDesc->Stride = 4;
      pMtlDesc->Alignment = 4;
      break;
    }
    case DXGI_FORMAT_R11G11B10_FLOAT: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRG11B10Float;
      pMtlDesc->VertexFormat = MTL::VertexFormatFloatRG11B10;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatFloatRG11B10;
      pMtlDesc->Stride = 4;
      pMtlDesc->Alignment = 4;
      break;
    }
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_R8G8B8A8_UINT:
    case DXGI_FORMAT_R8G8B8A8_SNORM:
    case DXGI_FORMAT_R8G8B8A8_SINT:
    case DXGI_FORMAT_R16G16_TYPELESS:
    case DXGI_FORMAT_R16G16_FLOAT:
    case DXGI_FORMAT_R16G16_UNORM:
    case DXGI_FORMAT_R16G16_UINT:
    case DXGI_FORMAT_R16G16_SNORM:
    case DXGI_FORMAT_R16G16_SINT:
      break;
    case DXGI_FORMAT_R32_TYPELESS: {
      pMtlDesc->PixelFormat = MTL::PixelFormatR32Uint;
      pMtlDesc->Stride = 4;
      pMtlDesc->Alignment = 4;
      break;
    }
    case DXGI_FORMAT_D32_FLOAT: {
      pMtlDesc->PixelFormat = MTL::PixelFormatDepth32Float;
      pMtlDesc->Stride = 4;
      pMtlDesc->Alignment = 4;
      break;
    }
    case DXGI_FORMAT_R32_FLOAT: {
      pMtlDesc->PixelFormat = MTL::PixelFormatR32Float;
      pMtlDesc->VertexFormat = MTL::VertexFormatFloat;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatFloat;
      pMtlDesc->Stride = 4;
      pMtlDesc->Alignment = 4;
      break;
    }
    case DXGI_FORMAT_R32_UINT: {
      pMtlDesc->PixelFormat = MTL::PixelFormatR32Uint;
      pMtlDesc->VertexFormat = MTL::VertexFormatUInt;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatUInt;
      pMtlDesc->Stride = 4;
      pMtlDesc->Alignment = 4;
      break;
    }
    case DXGI_FORMAT_R32_SINT: {
      pMtlDesc->PixelFormat = MTL::PixelFormatR32Sint;
      pMtlDesc->VertexFormat = MTL::VertexFormatInt;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatInt;
      pMtlDesc->Stride = 4;
      pMtlDesc->Alignment = 4;
      break;
    }
    case DXGI_FORMAT_R24G8_TYPELESS: {
      // ?
      break;
    }
    case DXGI_FORMAT_D24_UNORM_S8_UINT: {
      if (m_deivce->depth24Stencil8PixelFormatSupported()) {
        pMtlDesc->PixelFormat = MTL::PixelFormatDepth24Unorm_Stencil8;
      } else {
        ERR_ONCE("depth24Stencil8 is not supported.");
        pMtlDesc->PixelFormat = MTL::PixelFormatDepth32Float_Stencil8;
      }
      break;
    }
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
    case DXGI_FORMAT_R8G8_TYPELESS:
    case DXGI_FORMAT_R8G8_UNORM:
    case DXGI_FORMAT_R8G8_UINT:
    case DXGI_FORMAT_R8G8_SNORM:
    case DXGI_FORMAT_R8G8_SINT:
    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_R16_FLOAT: {
      pMtlDesc->PixelFormat = MTL::PixelFormatR16Float;
      pMtlDesc->VertexFormat = MTL::VertexFormatHalf;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatHalf;
      pMtlDesc->Stride = 2;
      pMtlDesc->Alignment = 2;
      break;
    }
    case DXGI_FORMAT_D16_UNORM: {
      pMtlDesc->PixelFormat = MTL::PixelFormatDepth16Unorm;
      pMtlDesc->Stride = 2;
      pMtlDesc->Alignment = 2;
      break;
    }
    case DXGI_FORMAT_R16_UNORM: {
      pMtlDesc->PixelFormat = MTL::PixelFormatR16Unorm;
      pMtlDesc->VertexFormat = MTL::VertexFormatUShortNormalized;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatUShortNormalized;
      pMtlDesc->Stride = 2;
      pMtlDesc->Alignment = 2;
      break;
    }
    case DXGI_FORMAT_R16_UINT: {
      pMtlDesc->PixelFormat = MTL::PixelFormatR16Uint;
      pMtlDesc->VertexFormat = MTL::VertexFormatUShort;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatUShort;
      pMtlDesc->Stride = 2;
      pMtlDesc->Alignment = 2;
      break;
    }
    case DXGI_FORMAT_R16_SNORM: {
      pMtlDesc->PixelFormat = MTL::PixelFormatR16Snorm;
      pMtlDesc->VertexFormat = MTL::VertexFormatShortNormalized;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatShortNormalized;
      pMtlDesc->Stride = 2;
      pMtlDesc->Alignment = 2;
      break;
    }
    case DXGI_FORMAT_R16_SINT: {
      pMtlDesc->PixelFormat = MTL::PixelFormatR16Sint;
      pMtlDesc->VertexFormat = MTL::VertexFormatShort;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatShort;
      pMtlDesc->Stride = 2;
      pMtlDesc->Alignment = 2;
      break;
    }
    case DXGI_FORMAT_R8_TYPELESS:
    case DXGI_FORMAT_R8_UNORM:
    case DXGI_FORMAT_R8_UINT:
    case DXGI_FORMAT_R8_SNORM:
    case DXGI_FORMAT_R8_SINT:
    case DXGI_FORMAT_A8_UNORM:
    case DXGI_FORMAT_R1_UNORM:
    case DXGI_FORMAT_R9G9B9E5_SHAREDEXP: {
      pMtlDesc->PixelFormat = MTL::PixelFormatRGB9E5Float;
      pMtlDesc->VertexFormat = MTL::VertexFormatFloatRGB9E5;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatFloatRGB9E5;
      pMtlDesc->Stride = 4;
      pMtlDesc->Alignment = 4;
      break;
    }
    case DXGI_FORMAT_R8G8_B8G8_UNORM:
    case DXGI_FORMAT_G8R8_G8B8_UNORM:
    case DXGI_FORMAT_BC1_TYPELESS:
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT_BC2_TYPELESS:
    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_TYPELESS:
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC4_TYPELESS:
    case DXGI_FORMAT_BC4_UNORM:
    case DXGI_FORMAT_BC4_SNORM:
    case DXGI_FORMAT_BC5_TYPELESS:
    case DXGI_FORMAT_BC5_UNORM:
    case DXGI_FORMAT_BC5_SNORM:
      return E_FAIL;
    case DXGI_FORMAT_B5G6R5_UNORM: {
      pMtlDesc->PixelFormat = MTL::PixelFormatB5G6R5Unorm;
      pMtlDesc->Stride = 2;
      pMtlDesc->Alignment = 2;
      break;
    }
    case DXGI_FORMAT_B5G5R5A1_UNORM: {
      pMtlDesc->PixelFormat = MTL::PixelFormatBGR5A1Unorm;
      pMtlDesc->Stride = 2;
      pMtlDesc->Alignment = 2;
      break;
    }
    case DXGI_FORMAT_B8G8R8A8_UNORM: {
      pMtlDesc->PixelFormat = MTL::PixelFormatBGRA8Unorm;
      pMtlDesc->VertexFormat = MTL::VertexFormatUChar4Normalized_BGRA;
      pMtlDesc->AttributeFormat = MTL::AttributeFormatUChar4Normalized_BGRA;
      pMtlDesc->Stride = 4;
      pMtlDesc->Alignment = 4;
      break;
    }
    case DXGI_FORMAT_B8G8R8X8_UNORM:
    case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: {
      pMtlDesc->PixelFormat = MTL::PixelFormatBGRA8Unorm_sRGB;
      // pMtlDesc->VertexFormat =MTL::VertexFormatUChar4Normalized_BGRA;
      // pMtlDesc->AttributeFormat =MTL::AttributeFormatUChar4Normalized_BGRA;
      break;
    }
    case DXGI_FORMAT_B8G8R8X8_TYPELESS:
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
    case DXGI_FORMAT_BC6H_TYPELESS:
    case DXGI_FORMAT_BC6H_UF16:
    case DXGI_FORMAT_BC6H_SF16:
    case DXGI_FORMAT_BC7_TYPELESS:
    case DXGI_FORMAT_BC7_UNORM:
    case DXGI_FORMAT_BC7_UNORM_SRGB:
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
    return S_OK;
  };

private:
  Obj<MTL::Device> m_deivce;
  Com<IDXGIFactory> m_factory;
};

Com<IMTLDXGIAdatper> CreateAdapter(MTL::Device *pDevice,
                                   IDXGIFactory2 *pFactory) {
  return new MTLDXGIAdatper(pDevice, pFactory);
}

} // namespace dxmt