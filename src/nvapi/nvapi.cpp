#include "../d3d11/d3d11_interfaces.hpp"
#include "../dxgi/dxgi_interfaces.h"
#include "com/com_pointer.hpp"
#include "log/log.hpp"
#include "nvapi_lite_common.h"
#include "util_string.hpp"
#include "winemetal.h"
#include "wsi_monitor.hpp"
#include <string>
#define __NVAPI_EMPTY_SAL
#include "nvapi.h"
#include "nvapi_interface.h"
#include "nvShaderExtnEnums.h"
#undef __NVAPI_EMPTY_SAL

namespace dxmt {
Logger Logger::s_instance("nvapi.log");

static std::atomic_uint32_t s_initialization_count = 0;

NVAPI_INTERFACE
NvAPI_Initialize() {
  if (FAILED(DXGIGetDebugInterface1(0, DXMT_NVEXT_GUID, nullptr))) {
    return NVAPI_NVIDIA_DEVICE_NOT_FOUND;
  }
  s_initialization_count++;
  return NVAPI_OK;
};

NVAPI_INTERFACE
NvAPI_Unload() {
  if (!s_initialization_count)
    return NVAPI_API_NOT_INITIALIZED;
  s_initialization_count--;
  return NVAPI_OK;
};

NVAPI_INTERFACE
NvAPI_SYS_GetDriverAndBranchVersion(NvU32 *pDriverVersion,
                                    NvAPI_ShortString szBuildBranchString) {
  std::string build_str = std::format("r{}_000", NVAPI_SDK_VERSION);

  if (!pDriverVersion || !szBuildBranchString)
    return NVAPI_INVALID_ARGUMENT;

  memcpy(szBuildBranchString, build_str.c_str(), build_str.size());
  *pDriverVersion = 99999;

  return NVAPI_OK;
}

NVAPI_INTERFACE
NvAPI_GetDisplayDriverVersion(NvDisplayHandle hNvDisplay, NV_DISPLAY_DRIVER_VERSION *pVersion) {
  std::string build_str = std::format("r{}_000", NVAPI_SDK_VERSION);
  std::string adapter_str = "NVIDIA GeForce RTX";

  if (!pVersion)
    return NVAPI_INVALID_ARGUMENT;

  if (pVersion->version != NV_DISPLAY_DRIVER_VERSION_VER)
    return NVAPI_INCOMPATIBLE_STRUCT_VERSION;

  pVersion->drvVersion = 99999;
  pVersion->bldChangeListNum = 0;
  memcpy(pVersion->szBuildBranchString, build_str.c_str(), build_str.size());
  memcpy(pVersion->szAdapterString, adapter_str.c_str(), adapter_str.size());

  return NVAPI_OK;
}

NVAPI_INTERFACE
NvAPI_GetInterfaceVersionString(NvAPI_ShortString szDesc) {
  std::string version_str = "NVAPI Open Source Interface (DXMT-NVAPI)";

  if (!szDesc)
    return NVAPI_INVALID_ARGUMENT;

  memcpy(szDesc, version_str.c_str(), version_str.size());

  return NVAPI_OK;
}

NVAPI_INTERFACE
NvAPI_D3D_GetCurrentSLIState(IUnknown *pDevice,
                             NV_GET_CURRENT_SLI_STATE *pSliState) {
  if (!pDevice || !pSliState)
    return NVAPI_INVALID_ARGUMENT;

  switch (pSliState->version) {
  case NV_GET_CURRENT_SLI_STATE_VER1:
    pSliState->maxNumAFRGroups = 1;
    pSliState->numAFRGroups = 1;
    pSliState->currentAFRIndex = 0;
    pSliState->nextFrameAFRIndex = 0;
    pSliState->previousFrameAFRIndex = 0;
    pSliState->bIsCurAFRGroupNew = FALSE;
    break;
  case NV_GET_CURRENT_SLI_STATE_VER2:
    pSliState->maxNumAFRGroups = 1;
    pSliState->numAFRGroups = 1;
    pSliState->currentAFRIndex = 0;
    pSliState->nextFrameAFRIndex = 0;
    pSliState->previousFrameAFRIndex = 0;
    pSliState->bIsCurAFRGroupNew = FALSE;
    pSliState->numVRSLIGpus = 0;
    break;
  default:
    return NVAPI_INCOMPATIBLE_STRUCT_VERSION;
  }

  return NVAPI_OK;
}

NVAPI_INTERFACE NvAPI_GetErrorMessage(NvAPI_Status nr,
                                      NvAPI_ShortString szDesc) {
  szDesc[0] = '\0';
  return NVAPI_OK;
}

Com<IMTLD3D11ContextExt> GetD3D11ContextExt(IUnknown *pDeviceOrContext) {
  Com<IMTLD3D11ContextExt> context_ext;
  if (FAILED(pDeviceOrContext->QueryInterface(IID_PPV_ARGS(&context_ext)))) {
    Com<ID3D11Device> device;
    if (FAILED(pDeviceOrContext->QueryInterface(IID_PPV_ARGS(&device)))) {
      return nullptr;
    }
    Com<ID3D11DeviceContext> context;
    device->GetImmediateContext(&context);
    if (FAILED(context->QueryInterface(IID_PPV_ARGS(&context_ext)))) {
      return nullptr;
    }
  }
  return context_ext;
}

NVAPI_INTERFACE NvAPI_D3D11_BeginUAVOverlap(__in IUnknown *pDeviceOrContext) {
  if (!pDeviceOrContext)
    return NVAPI_INVALID_ARGUMENT;
  auto context = GetD3D11ContextExt(pDeviceOrContext);
  if (!context)
    return NVAPI_INVALID_ARGUMENT;
  context->BeginUAVOverlap();
  return NVAPI_OK;
}

NVAPI_INTERFACE NvAPI_D3D11_EndUAVOverlap(__in IUnknown *pDeviceOrContext) {
  if (!pDeviceOrContext)
    return NVAPI_INVALID_ARGUMENT;
  auto context = GetD3D11ContextExt(pDeviceOrContext);
  if (!context)
    return NVAPI_INVALID_ARGUMENT;
  context->EndUAVOverlap();
  return NVAPI_OK;
}

NVAPI_INTERFACE NvAPI_D3D11_SetDepthBoundsTest(IUnknown *pDeviceOrContext,
                                               NvU32 bEnable, float fMinDepth,
                                               float fMaxDepth) {
  return NVAPI_NOT_SUPPORTED;
}

NVAPI_INTERFACE NvAPI_D3D11_IsNvShaderExtnOpCodeSupported(
    __in IUnknown *pDev, __in NvU32 opCode, __out bool *pSupported) {
  switch (opCode) {
  /*
   * UnrealEngine queries it for checking if Nanite capability
   */
  case NV_EXTN_OP_UINT64_ATOMIC:
  /*
   * UnrealEngine queries it for checking if GPU support time query (NVIDIA-specific workaround)
   */
  case NV_EXTN_OP_SHFL:
  /*
   * SOTTR and ROTTR queries it.
   */
  case NV_EXTN_OP_FP16_ATOMIC:
    *pSupported = false;
    break;
  default:
    WARN("nvapi: unsupported shader extension opcode ", opCode);
    *pSupported = false;
    break;
  }
  return NVAPI_OK;
}

Com<IMTLD3D11DeviceExt> GetD3D11DeviceExt(IUnknown *pDevice) {
  Com<IMTLD3D11DeviceExt> device_ext;
  if (FAILED(pDevice->QueryInterface(IID_PPV_ARGS(&device_ext)))) {
    return nullptr;
  }
  return device_ext;
}

NVAPI_INTERFACE NvAPI_D3D11_SetNvShaderExtnSlot(__in IUnknown *pDev,
                                                __in NvU32 uavSlot) {
  auto device_ext = GetD3D11DeviceExt(pDev);
  if (!device_ext)
    return NVAPI_INVALID_ARGUMENT;
  device_ext->SetShaderExtensionSlot(uavSlot);
  return NVAPI_OK;
}

NVAPI_INTERFACE
NvAPI_DISP_GetDisplayIdByDisplayName(const char *displayName, NvU32 *displayId) {
  struct MonitorEnumInfo {
    std::string name;
    HMONITOR handle;
    bool is_primary;
  };

  MonitorEnumInfo info;
  info.name = displayName;
  info.handle = nullptr;
  ::EnumDisplayMonitors(
      nullptr, nullptr,
      [](HMONITOR hmon, HDC hdc, LPRECT rect, LPARAM lp) -> BOOL {
        auto data = reinterpret_cast<MonitorEnumInfo *>(lp);

        ::MONITORINFOEXW monitor_info;
        monitor_info.cbSize = sizeof(monitor_info);

        if (!::GetMonitorInfoW(hmon, reinterpret_cast<MONITORINFO *>(&monitor_info)))
          return TRUE;

        if (data->name != str::fromws(monitor_info.szDevice))
          return TRUE;

        data->handle = hmon;
        data->is_primary = monitor_info.dwFlags & MONITORINFOF_PRIMARY;
        return FALSE;
      },
      reinterpret_cast<LPARAM>(&info)
  );

  if (!info.handle)
    return NVAPI_NVIDIA_DEVICE_NOT_FOUND;

  *displayId = info.is_primary ? WMTGetPrimaryDisplayId() : WMTGetSecondaryDisplayId();

  return NVAPI_OK;
}

NVAPI_INTERFACE
NvAPI_D3D_GetObjectHandleForResource(IUnknown *pDevice, IUnknown *pResource, NVDX_ObjectHandle *pHandle) {
  /*
   * SLI related API, return resource pointer as fake handle, since we won't use the handle anyway
   */
  *pHandle = (NVDX_ObjectHandle)pResource;
  return NVAPI_OK;
}

NVAPI_INTERFACE
NvAPI_D3D_SetResourceHint(
    IUnknown *pDev, NVDX_ObjectHandle obj, NVAPI_D3D_SETRESOURCEHINT_CATEGORY dwHintCategory, NvU32 dwHintName,
    NvU32 *pdwHintValue
) {
  /*
   * SLI related API, just do nothing
   */
  return NVAPI_OK;
}

NVAPI_INTERFACE
NvAPI_EnumPhysicalGPUs(NvPhysicalGpuHandle nvGPUHandle[NVAPI_MAX_PHYSICAL_GPUS], NvU32 *pGpuCount) {
  if (!nvGPUHandle || !pGpuCount)
    return NVAPI_INVALID_ARGUMENT;
  /*
   * reasonable to report one fake gpu handle
   */
  *pGpuCount = 1;
  nvGPUHandle[0] = (NvPhysicalGpuHandle)0xdeadbeef;
  return NVAPI_OK;
}

NVAPI_INTERFACE
NvAPI_EnumLogicalGPUs(NvLogicalGpuHandle nvGPUHandle[NVAPI_MAX_LOGICAL_GPUS], NvU32 *pGpuCount) {
  if (!nvGPUHandle || !pGpuCount)
    return NVAPI_INVALID_ARGUMENT;
  /*
   * reasonable to report one fake gpu handle
   */
  *pGpuCount = 1;
  nvGPUHandle[0] = (NvLogicalGpuHandle)0xdeadbeef;
  return NVAPI_OK;
}

NVAPI_INTERFACE
NvAPI_GetPhysicalGPUsFromDisplay(NvDisplayHandle hNvDisp, NvPhysicalGpuHandle nvGPUHandle[NVAPI_MAX_PHYSICAL_GPUS], NvU32 *pGpuCount) {
  if (!nvGPUHandle || !pGpuCount)
    return NVAPI_INVALID_ARGUMENT;
  /*
   * reasonable to report one fake gpu handle
   */
  *pGpuCount = 1;
  nvGPUHandle[0] = (NvPhysicalGpuHandle)0xdeadbeef;
  return NVAPI_OK;
}

NVAPI_INTERFACE
NvAPI_Disp_GetHdrCapabilities(NvU32 displayId, NV_HDR_CAPABILITIES *pHdrCapabilities) {
  WMTDisplayDescription desc;
  WMTGetDisplayDescription(displayId, &desc);

  switch (pHdrCapabilities->version) {
  case NV_HDR_CAPABILITIES_VER1: {
    *reinterpret_cast<NV_HDR_CAPABILITIES_V1 *>(pHdrCapabilities) = {};
    pHdrCapabilities->version = NV_HDR_CAPABILITIES_VER1;
    break;
  }
  case NV_HDR_CAPABILITIES_VER2: {
    *reinterpret_cast<NV_HDR_CAPABILITIES_V2 *>(pHdrCapabilities) = {};
    pHdrCapabilities->version = NV_HDR_CAPABILITIES_VER2;
    break;
  }
  case NV_HDR_CAPABILITIES_VER3: {
    *pHdrCapabilities = {};
    pHdrCapabilities->version = NV_HDR_CAPABILITIES_VER3;
    break;
  }
  default:
    return NVAPI_INCOMPATIBLE_STRUCT_VERSION;
  }

  auto cap_out = reinterpret_cast<NV_HDR_CAPABILITIES_V1 *>(pHdrCapabilities);

  cap_out->isST2084EotfSupported = desc.maximum_potential_edr_color_component_value > 1.0;

  cap_out->display_data.displayPrimary_x0 = desc.red_primaries[0] * 50000;
  cap_out->display_data.displayPrimary_y0 = desc.red_primaries[1] * 50000;
  cap_out->display_data.displayPrimary_x1 = desc.green_primaries[0] * 50000;
  cap_out->display_data.displayPrimary_y1 = desc.green_primaries[1] * 50000;
  cap_out->display_data.displayPrimary_x2 = desc.blue_primaries[0] * 50000;
  cap_out->display_data.displayPrimary_y2 = desc.blue_primaries[1] * 50000;
  cap_out->display_data.displayWhitePoint_x = desc.white_points[0] * 50000;
  cap_out->display_data.displayWhitePoint_y = desc.white_points[1] * 50000;
  cap_out->display_data.desired_content_max_luminance = desc.maximum_potential_edr_color_component_value * 100;
  cap_out->display_data.desired_content_min_luminance = 1;
  cap_out->display_data.desired_content_max_frame_average_luminance =
      cap_out->display_data.desired_content_max_luminance;

  return NVAPI_OK;
}

NVAPI_INTERFACE
NvAPI_Disp_HdrColorControl(NvU32 displayId, NV_HDR_COLOR_DATA *pHdrColorData) {
  switch (pHdrColorData->version) {
  case NV_HDR_COLOR_DATA_VER1:
  case NV_HDR_COLOR_DATA_VER2:
    break;
  default:
    return NVAPI_INCOMPATIBLE_STRUCT_VERSION;
  }

  WMTHDRMetadata metadata;
  WMTColorSpace colorspace = WMTColorSpaceSRGB;

  if (pHdrColorData->cmd == NV_HDR_CMD_GET) {
    if (WMTQueryDisplaySetting(displayId, &colorspace, &metadata)) {
      pHdrColorData->hdrMode = colorspace == WMTColorSpaceHDR_scRGB ? NV_HDR_MODE_UHDA
                               : colorspace == WMTColorSpaceHDR_PQ  ? NV_HDR_MODE_UHDA_PASSTHROUGH
                                                                    : NV_HDR_MODE_OFF;
      pHdrColorData->mastering_display_data.displayPrimary_x0 = metadata.red_primary[0];
      pHdrColorData->mastering_display_data.displayPrimary_y0 = metadata.red_primary[1];
      pHdrColorData->mastering_display_data.displayPrimary_x1 = metadata.green_primary[0];
      pHdrColorData->mastering_display_data.displayPrimary_y1 = metadata.green_primary[1];
      pHdrColorData->mastering_display_data.displayPrimary_x2 = metadata.blue_primary[0];
      pHdrColorData->mastering_display_data.displayPrimary_y2 = metadata.blue_primary[1];
      pHdrColorData->mastering_display_data.displayWhitePoint_x = metadata.white_point[0];
      pHdrColorData->mastering_display_data.displayWhitePoint_y = metadata.white_point[1];
      pHdrColorData->mastering_display_data.max_display_mastering_luminance = metadata.max_mastering_luminance;
      pHdrColorData->mastering_display_data.min_display_mastering_luminance = metadata.min_mastering_luminance;
      pHdrColorData->mastering_display_data.max_content_light_level = metadata.max_content_light_level;
      pHdrColorData->mastering_display_data.max_frame_average_light_level = metadata.max_frame_average_light_level;
    } else {
      WMTDisplayDescription desc;
      WMTGetDisplayDescription(displayId, &desc);
      pHdrColorData->hdrMode = NV_HDR_MODE_OFF;
      pHdrColorData->mastering_display_data.displayPrimary_x0 = desc.red_primaries[0] * 50000;
      pHdrColorData->mastering_display_data.displayPrimary_y0 = desc.red_primaries[1] * 50000;
      pHdrColorData->mastering_display_data.displayPrimary_x1 = desc.green_primaries[0] * 50000;
      pHdrColorData->mastering_display_data.displayPrimary_y1 = desc.green_primaries[1] * 50000;
      pHdrColorData->mastering_display_data.displayPrimary_x2 = desc.blue_primaries[0] * 50000;
      pHdrColorData->mastering_display_data.displayPrimary_y2 = desc.blue_primaries[1] * 50000;
      pHdrColorData->mastering_display_data.displayWhitePoint_x = desc.white_points[0] * 50000;
      pHdrColorData->mastering_display_data.displayWhitePoint_y = desc.white_points[1] * 50000;
      pHdrColorData->mastering_display_data.max_display_mastering_luminance =
          desc.maximum_potential_edr_color_component_value * 100;
      pHdrColorData->mastering_display_data.min_display_mastering_luminance = 1;
      pHdrColorData->mastering_display_data.max_content_light_level =
          desc.maximum_potential_edr_color_component_value * 100;
      pHdrColorData->mastering_display_data.max_frame_average_light_level =
          desc.maximum_potential_edr_color_component_value * 100;
    }
  } else {
    if (pHdrColorData->hdrMode != NV_HDR_MODE_OFF) {
      metadata.red_primary[0] = pHdrColorData->mastering_display_data.displayPrimary_x0;
      metadata.red_primary[1] = pHdrColorData->mastering_display_data.displayPrimary_y0;
      metadata.green_primary[0] = pHdrColorData->mastering_display_data.displayPrimary_x1;
      metadata.green_primary[1] = pHdrColorData->mastering_display_data.displayPrimary_y1;
      metadata.blue_primary[0] = pHdrColorData->mastering_display_data.displayPrimary_x2;
      metadata.blue_primary[1] = pHdrColorData->mastering_display_data.displayPrimary_y2;
      metadata.white_point[0] = pHdrColorData->mastering_display_data.displayWhitePoint_x;
      metadata.white_point[1] = pHdrColorData->mastering_display_data.displayWhitePoint_y;
      metadata.max_mastering_luminance = pHdrColorData->mastering_display_data.max_display_mastering_luminance;
      metadata.min_mastering_luminance = pHdrColorData->mastering_display_data.min_display_mastering_luminance;
      metadata.max_content_light_level = pHdrColorData->mastering_display_data.max_content_light_level;
      metadata.max_frame_average_light_level = pHdrColorData->mastering_display_data.max_frame_average_light_level;
      colorspace = pHdrColorData->hdrMode == NV_HDR_MODE_UHDA ? WMTColorSpaceHDR_scRGB : WMTColorSpaceHDR_PQ;
      WMTUpdateDisplaySetting(displayId, colorspace, &metadata);
    } else {
      WMTUpdateDisplaySetting(displayId, WMTColorSpaceSRGB, nullptr);
    }
  }

  if (pHdrColorData->version == NV_HDR_COLOR_DATA_VER2) {
    auto pHDRColorDataV2 = reinterpret_cast<NV_HDR_COLOR_DATA_V2 *>(pHdrColorData);
    if (pHDRColorDataV2->cmd == NV_HDR_CMD_GET) {
      pHDRColorDataV2->hdrColorFormat = NV_COLOR_FORMAT_RGB;
      pHDRColorDataV2->hdrDynamicRange = NV_DYNAMIC_RANGE_VESA;
      pHDRColorDataV2->hdrBpc = NV_BPC_10;
    }
  }

  return NVAPI_OK;
}

NVAPI_INTERFACE
NvAPI_EnumNvidiaDisplayHandle(NvU32 thisEnum, NvDisplayHandle *pNvDispHandle) {

  if (!pNvDispHandle)
    return NVAPI_INVALID_ARGUMENT;

  HMONITOR monitor = wsi::enumMonitors(thisEnum);

  if (!monitor)
    return NVAPI_END_ENUMERATION;
  *pNvDispHandle = (NvDisplayHandle)monitor;
  return NVAPI_OK;
}

NVAPI_INTERFACE
NvAPI_GPU_GetConnectedDisplayIds(
    NvPhysicalGpuHandle hPhysicalGpu, NV_GPU_DISPLAYIDS *pDisplayIds, NvU32 *pDisplayIdCount, NvU32 flags
) {
  if (pDisplayIdCount == nullptr)
    return NVAPI_INVALID_ARGUMENT;

  auto primary_display_id = WMTGetPrimaryDisplayId();
  auto nonprimary_display_id = WMTGetSecondaryDisplayId();

  unsigned count = primary_display_id != 0 ? nonprimary_display_id != 0 ? 2 : 1 : 0;
  if (pDisplayIds == nullptr) {
    *pDisplayIdCount = count;
    return NVAPI_OK;
  }

  if (*pDisplayIdCount < count) {
    *pDisplayIdCount = count;
    return NVAPI_INSUFFICIENT_BUFFER;
  }

  for (unsigned i = 0; i < count; i++) {
    auto version = pDisplayIds[i].version;
    switch (version) {
    case NV_GPU_DISPLAYIDS_VER1:
    case NV_GPU_DISPLAYIDS_VER2:
      break;
    default:
      return NVAPI_INCOMPATIBLE_STRUCT_VERSION;
    }
    pDisplayIds[i] = {};
    pDisplayIds[i].version = version;
    pDisplayIds[i].displayId = i ? nonprimary_display_id : primary_display_id;
    pDisplayIds[i].connectorType = NV_MONITOR_CONN_TYPE_UNKNOWN;
    pDisplayIds[i].isActive = true;
    pDisplayIds[i].isConnected = true;
    pDisplayIds[i].isPhysicallyConnected = true;
  }
  return NVAPI_OK;
}

NVAPI_INTERFACE
NvAPI_DISP_GetMonitorCapabilities(__in NvU32 displayId, __inout NV_MONITOR_CAPABILITIES *pMonitorCapabilities) {
  if (!pMonitorCapabilities)
    return NVAPI_INVALID_ARGUMENT;

  return NVAPI_NOT_SUPPORTED;
}

NVAPI_INTERFACE
NvAPI_GPU_GetAllClockFrequencies(__in NvPhysicalGpuHandle hPhysicalGPU,
                                 __inout NV_GPU_CLOCK_FREQUENCIES *pClkFreqs) {
  if (!pClkFreqs)
    return NVAPI_INVALID_ARGUMENT;

  return NVAPI_NOT_SUPPORTED;
}

NVAPI_INTERFACE
NvAPI_GPU_GetArchInfo(NvPhysicalGpuHandle hPhysicalGpu, NV_GPU_ARCH_INFO *pGpuArchInfo) {
  if (!hPhysicalGpu || !pGpuArchInfo)
    return NVAPI_INVALID_ARGUMENT;

  // NVIDIA GeForce RTX 4090
  switch (pGpuArchInfo->version) {
    case NV_GPU_ARCH_INFO_VER_1: {
      auto pGpuArchInfoV1 = (NV_GPU_ARCH_INFO_V1 *)(pGpuArchInfo);
      pGpuArchInfoV1->architecture = NV_GPU_ARCHITECTURE_AD100;
      pGpuArchInfoV1->implementation = NV_GPU_ARCH_IMPLEMENTATION_AD102;
      pGpuArchInfoV1->revision = NV_GPU_CHIP_REV_UNKNOWN;
      break;
    }
    case NV_GPU_ARCH_INFO_VER_2:
      pGpuArchInfo->architecture_id = NV_GPU_ARCHITECTURE_AD100;
      pGpuArchInfo->implementation_id = NV_GPU_ARCH_IMPLEMENTATION_AD102;
      pGpuArchInfo->revision_id = NV_GPU_CHIP_REV_UNKNOWN;
      break;
    default:
      return NVAPI_INCOMPATIBLE_STRUCT_VERSION;
  }

  return NVAPI_OK;
}

NVAPI_INTERFACE
NvAPI_GPU_GetGpuCoreCount(NvPhysicalGpuHandle hPhysicalGpu, NvU32 *pCount) {
  if (!hPhysicalGpu || !pCount)
    return NVAPI_INVALID_ARGUMENT;

  return NVAPI_NOT_SUPPORTED;
}

NVAPI_INTERFACE
NvAPI_GPU_GetBusId(NvPhysicalGpuHandle hPhysicalGpu, NvU32 *pBusId) {
  if (!hPhysicalGpu || !pBusId)
    return NVAPI_INVALID_ARGUMENT;

  return NVAPI_NOT_SUPPORTED;
}

NVAPI_INTERFACE
NvAPI_GPU_GetBusType(NvPhysicalGpuHandle hPhysicalGpu, NV_GPU_BUS_TYPE *pBusType) {
  if (!hPhysicalGpu || !pBusType)
    return NVAPI_INVALID_ARGUMENT;

  return NVAPI_NOT_SUPPORTED;
}

NVAPI_INTERFACE
NvAPI_GPU_GetMemoryInfo(NvPhysicalGpuHandle hPhysicalGpu, NV_DISPLAY_DRIVER_MEMORY_INFO *pMemoryInfo) {
  if (!hPhysicalGpu || !pMemoryInfo)
    return NVAPI_INVALID_ARGUMENT;

  return NVAPI_NOT_SUPPORTED;
}

NVAPI_INTERFACE
NvAPI_GPU_GetMemoryInfoEx(NvPhysicalGpuHandle hPhysicalGpu, NV_GPU_MEMORY_INFO_EX *pMemoryInfo) {
  if (!hPhysicalGpu || !pMemoryInfo)
    return NVAPI_INVALID_ARGUMENT;

  return NVAPI_NOT_SUPPORTED;
}

NVAPI_INTERFACE
NvAPI_GPU_GetFullName(NvPhysicalGpuHandle hPhysicalGpu, NvAPI_ShortString szName) {
  std::string adapter_str = "NVIDIA GeForce RTX 4090";

  if (!szName)
    return NVAPI_INVALID_ARGUMENT;

  memcpy(szName, adapter_str.c_str(), adapter_str.size());

  return NVAPI_OK;
}

NVAPI_INTERFACE
NvAPI_DISP_GetGDIPrimaryDisplayId(NvU32 *displayId) {
  if (!displayId)
    return NVAPI_INVALID_ARGUMENT;
  *displayId = WMTGetPrimaryDisplayId();
  return NVAPI_OK;
}

NVAPI_INTERFACE
NvAPI_D3D_GetSleepStatus(IUnknown *pDev, NV_GET_SLEEP_STATUS_PARAMS *pGetSleepStatusParams) {
  if (!pDev || !pGetSleepStatusParams)
    return NVAPI_INVALID_ARGUMENT;

  switch (pGetSleepStatusParams->version) {
  case NV_GET_SLEEP_STATUS_PARAMS_VER1:
    pGetSleepStatusParams->bLowLatencyMode = false;
    pGetSleepStatusParams->bFsVrr = false;
    pGetSleepStatusParams->bCplVsyncOn = false;
    pGetSleepStatusParams->sleepIntervalUs = 0;
    pGetSleepStatusParams->bUseGameSleep = false;
    break;
  default:
    return NVAPI_INCOMPATIBLE_STRUCT_VERSION;
  }

  return NVAPI_OK;
}

NVAPI_INTERFACE
NvAPI_D3D_SetSleepMode(IUnknown *pDev, NV_SET_SLEEP_MODE_PARAMS *pSetSleepModeParams) {
  if (!pDev || !pSetSleepModeParams)
    return NVAPI_INVALID_ARGUMENT;

  switch (pSetSleepModeParams->version) {
  case NV_SET_SLEEP_MODE_PARAMS_VER1:
    break;
  default:
    return NVAPI_INCOMPATIBLE_STRUCT_VERSION;
  }

  return NVAPI_OK;
}

NVAPI_INTERFACE
NvAPI_D3D_SetLatencyMarker(IUnknown *pDev, NV_LATENCY_MARKER_PARAMS *pSetLatencyMarkerParams) {
  if (!pDev || !pSetLatencyMarkerParams)
    return NVAPI_INVALID_ARGUMENT;

  switch (pSetLatencyMarkerParams->version) {
  case NV_LATENCY_MARKER_PARAMS_VER1:
    break;
  default:
    return NVAPI_INCOMPATIBLE_STRUCT_VERSION;
  }

  return NVAPI_OK;
}

NVAPI_INTERFACE
NvAPI_D3D_Sleep(IUnknown *pDev) {
  if (!pDev)
    return NVAPI_INVALID_ARGUMENT;

  return NVAPI_OK;
}

NVAPI_INTERFACE
NvAPI_D3D_GetLatency(IUnknown *pDev, NV_LATENCY_RESULT_PARAMS *pGetLatencyParams) {
  if (!pDev || !pGetLatencyParams)
    return NVAPI_INVALID_ARGUMENT;

  switch (pGetLatencyParams->version) {
  case NV_LATENCY_RESULT_PARAMS_VER1:
    memset(pGetLatencyParams->frameReport, 0, sizeof(pGetLatencyParams->frameReport));
    break;
  default:
    return NVAPI_INCOMPATIBLE_STRUCT_VERSION;
  }

  return NVAPI_OK;
}

NVAPI_INTERFACE
NvAPI_GPU_GetPstates20(NvPhysicalGpuHandle hPhysicalGpu, NV_GPU_PERF_PSTATES20_INFO *pPstatesInfo) {
  if (!hPhysicalGpu || !pPstatesInfo)
    return NVAPI_INVALID_ARGUMENT;

  return NVAPI_NO_IMPLEMENTATION;
}

NVAPI_INTERFACE
NvAPI_Mosaic_GetDisplayViewportsByResolution(NvU32 displayId, NvU32 srcWidth, NvU32 srcHeight, NV_RECT viewports[NV_MOSAIC_MAX_DISPLAYS], NvU8 *bezelCorrected) {
  return NVAPI_MOSAIC_NOT_ACTIVE;
}

NVAPI_INTERFACE
NvAPI_Stereo_IsEnabled(NvU8 *pIsStereoEnabled) {
  if (!pIsStereoEnabled)
    return NVAPI_INVALID_ARGUMENT;

  return NVAPI_STEREO_NOT_INITIALIZED;
}

NVAPI_INTERFACE
NvAPI_Stereo_SetDriverMode(NV_STEREO_DRIVER_MODE mode) {
  return NVAPI_STEREO_NOT_INITIALIZED;
}

NVAPI_INTERFACE
NvAPI_DRS_CreateSession(NvDRSSessionHandle *phSession) {
  if (!phSession)
    return NVAPI_INVALID_ARGUMENT;

  return NVAPI_NOT_SUPPORTED;
}

NVAPI_INTERFACE
NvAPI_DRS_LoadSettings(NvDRSSessionHandle hSession) {
  return NVAPI_NOT_SUPPORTED;
}

extern "C" __cdecl void *nvapi_QueryInterface(NvU32 id) {
  switch (id) {
  case 0x0150e828:
    return (void *)&NvAPI_Initialize;
  case 0x33c7358c:
  case 0x593e8644:
  case 0xad298d3f:
  case 0xd7c61344:
  case 0xd44d3c4e:
  case 0x6d67533c:
  case 0xb7fbdbfa:
  case 0xc99f4a67:
    // unknown functions
    return nullptr;
  case 0x2926aaad:
    return (void *)&NvAPI_SYS_GetDriverAndBranchVersion;
  case 0xf951a4d1:
    return (void *)&NvAPI_GetDisplayDriverVersion;
  case 0x01053fa5:
    return (void *)&NvAPI_GetInterfaceVersionString;
  case 0x4b708b54:
    return (void *)&NvAPI_D3D_GetCurrentSLIState;
  case 0xd22bdd7e:
    return (void *)&NvAPI_Unload;
  case 0x6c2d048c:
    return (void *)&NvAPI_GetErrorMessage;
  case 0x65b93ca8:
    return (void *)&NvAPI_D3D11_BeginUAVOverlap;
  case 0x2216a357:
    return (void *)&NvAPI_D3D11_EndUAVOverlap;
  case 0x7aaf7a04:
    return (void *)&NvAPI_D3D11_SetDepthBoundsTest;
  case 0x5f68da40:
    return (void *)&NvAPI_D3D11_IsNvShaderExtnOpCodeSupported;
  case 0x8e90bb9f:
    return (void *)&NvAPI_D3D11_SetNvShaderExtnSlot;
  case 0xae457190:
    return (void *)&NvAPI_DISP_GetDisplayIdByDisplayName;
  case 0xfceac864:
    return (void *)&NvAPI_D3D_GetObjectHandleForResource;
  case 0x6c0ed98c:
    return (void *)&NvAPI_D3D_SetResourceHint;
  case 0xe5ac921f:
    return (void *)&NvAPI_EnumPhysicalGPUs;
  case 0x48b3ea59:
    return (void *)&NvAPI_EnumLogicalGPUs;
  case 0x34ef9506:
    return (void *)&NvAPI_GetPhysicalGPUsFromDisplay;
  case 0x351da224:
    return (void *)&NvAPI_Disp_HdrColorControl;
  case 0x84f2a8df:
    return (void *)&NvAPI_Disp_GetHdrCapabilities;
  case 0x3b05c7e1:
    return (void *)&NvAPI_DISP_GetMonitorCapabilities;
  case 0x9abdd40d:
    return (void *)&NvAPI_EnumNvidiaDisplayHandle;
  case 0x0078dba2:
    return (void *)&NvAPI_GPU_GetConnectedDisplayIds;
  case 0x1e9d8a31:
    return (void *)&NvAPI_DISP_GetGDIPrimaryDisplayId;
  case 0xd8265d24:
    return (void *)&NvAPI_GPU_GetArchInfo;
  case 0xdcb616c3:
    return (void *)&NvAPI_GPU_GetAllClockFrequencies;
  case 0xc7026a87:
    return (void *)&NvAPI_GPU_GetGpuCoreCount;
  case 0x1be0b8e5:
    return (void *)&NvAPI_GPU_GetBusId;
  case 0x1bb18724:
    return (void *)&NvAPI_GPU_GetBusType;
  case 0x07f9b368:
    return (void *)&NvAPI_GPU_GetMemoryInfo;
  case 0xc0599498:
    return (void *)&NvAPI_GPU_GetMemoryInfoEx;
  case 0xceee8e9f:
    return (void *)&NvAPI_GPU_GetFullName;
  case 0x6ff81213:
    return (void *)&NvAPI_GPU_GetPstates20;
  case 0xaef96ca1:
    return (void *)&NvAPI_D3D_GetSleepStatus;
  case 0xac1ca9e0:
    return (void *)&NvAPI_D3D_SetSleepMode;
  case 0xd9984c05:
    return (void *)&NvAPI_D3D_SetLatencyMarker;
  case 0x852cd1d2:
    return (void *)&NvAPI_D3D_Sleep;
  case 0x1a587f9c:
    return (void *)&NvAPI_D3D_GetLatency;
  case 0xdc6dc8d3:
    return (void *)&NvAPI_Mosaic_GetDisplayViewportsByResolution;
  case 0x348ff8e1:
    return (void *)&NvAPI_Stereo_IsEnabled;
  case 0x5e8f0bec:
    return (void *)&NvAPI_Stereo_SetDriverMode;
  case 0x0694d52e:
    return (void *)&NvAPI_DRS_CreateSession;
  case 0x375dbd6b:
    return (void *)&NvAPI_DRS_LoadSettings;
  default:
    break;
  }

  for (auto &iface : nvapi_interface_table) {
    if (iface.id == id) {
      ERR("nvapi: function ", iface.func, " not implemented");
      return nullptr;
    }
  }
  ERR("nvapi: function id ", id, " not implemented");
  return nullptr;
}

}; // namespace dxmt