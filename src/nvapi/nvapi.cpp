#include "log/log.hpp"
#include <string>
#define __NVAPI_EMPTY_SAL
#include "nvapi.h"
#include "nvapi_interface.h"
#undef __NVAPI_EMPTY_SAL

namespace dxmt {
Logger Logger::s_instance("nvapi.log");

NVAPI_INTERFACE NvAPI_Initialize() { return NVAPI_OK; };

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

extern "C" __cdecl void *nvapi_QueryInterface(NvU32 id) {
  switch (id) {
  case 0x0150e828:
    return (void *)&NvAPI_Initialize;
  case 0x33c7358c:
  case 0x593e8644:
    // unknown functions
    return nullptr;
  case 0x2926aaad:
    return (void *)&NvAPI_SYS_GetDriverAndBranchVersion;
  case 0x4b708b54:
    return (void *)&NvAPI_D3D_GetCurrentSLIState;
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