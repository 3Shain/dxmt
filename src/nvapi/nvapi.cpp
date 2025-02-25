#include "../d3d11/d3d11_interfaces.hpp"
#include "com/com_pointer.hpp"
#include "log/log.hpp"
#include <string>
#define __NVAPI_EMPTY_SAL
#include "nvapi.h"
#include "nvapi_interface.h"
#undef __NVAPI_EMPTY_SAL

namespace dxmt {
Logger Logger::s_instance("nvapi.log");

NVAPI_INTERFACE NvAPI_Initialize() { return NVAPI_OK; };

NVAPI_INTERFACE NvAPI_Unload() { return NVAPI_OK; };

NVAPI_INTERFACE
NvAPI_SYS_GetDriverAndBranchVersion(NvU32 *pDriverVersion,
                                    NvAPI_ShortString szBuildBranchString) {
  std::string build_str = std::format("r{}_000", NVAPI_SDK_VERSION);

  if (!pDriverVersion || !szBuildBranchString)
    return NVAPI_INVALID_POINTER;

  memcpy(szBuildBranchString, build_str.c_str(), build_str.size());
  *pDriverVersion = 99999;

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

extern "C" __cdecl void *nvapi_QueryInterface(NvU32 id) {
  switch (id) {
  case 0x0150e828:
    return (void *)&NvAPI_Initialize;
  case 0x33c7358c:
  case 0x593e8644:
  case 0xad298d3f:
    // unknown functions
    return nullptr;
  case 0x2926aaad:
    return (void *)&NvAPI_SYS_GetDriverAndBranchVersion;
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