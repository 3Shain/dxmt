#include "d3d11.h"
#include "nvngx.hpp"
#include "log/log.hpp"

namespace dxmt {
Logger Logger::s_instance("nvngx.log");

#define IMPLEMENT_ME                                                                                                   \
  Logger::err(str::format(__FILE__, ":", __FUNCTION__, " is not implemented."));                                       \
  abort();                                                                                                             \
  __builtin_unreachable();

NVNGX_API NVNGX_RESULT
NVSDK_NGX_D3D11_Init_Ext(
    unsigned long long id, const wchar_t *path, ID3D11Device *device, unsigned int sdk_version, const void *feature_info
) {
  return NVNGX_RESULT_OK;
}

NVNGX_API NVNGX_RESULT
NVSDK_NGX_D3D11_Init(
    unsigned long long id, const wchar_t *path, ID3D11Device *device, const void *feature_info, unsigned int sdk_version
) {
  return NVNGX_RESULT_OK;
}

NVNGX_API NVNGX_RESULT
NVSDK_NGX_D3D11_Init_ProjectID(
    const char *project, unsigned int engine_type, const char *engine_version, const wchar_t *path,
    ID3D11Device *device, unsigned int sdk_version, const void *feature_info
) {
  return NVNGX_RESULT_OK;
}

NVNGX_API NVNGX_RESULT
NVSDK_NGX_D3D11_Init_with_ProjectID(
    const char *project, unsigned int engine_type, const char *engine_version, const wchar_t *path,
    ID3D11Device *device, const void *feature_info, unsigned int sdk_version
) {
  return NVNGX_RESULT_OK;
}

NVNGX_API NVNGX_RESULT
NVSDK_NGX_D3D11_CreateFeature(
    ID3D11DeviceContext *context, unsigned int feature, NVNGXParameter *params, unsigned int **out_handle
) {
  IMPLEMENT_ME
  ERR("NVSDK_NGX_D3D11_CreateFeature: feature ", feature, " not handled");
  return NVNGX_RESULT_FAIL;
}

NVNGX_API NVNGX_RESULT
NVSDK_NGX_D3D11_EvaluateFeature(
    ID3D11DeviceContext *context, const unsigned int *handle, const NVNGXParameter *params, void *callback
) {
  IMPLEMENT_ME
  return NVNGX_RESULT_FAIL;
}

NVNGX_API NVNGX_RESULT
NVSDK_NGX_D3D11_GetParameters(NVNGXParameter **out_params) {
  IMPLEMENT_ME
  return NVNGX_RESULT_FAIL;
}

NVNGX_API NVNGX_RESULT
NVSDK_NGX_D3D11_Shutdown() {
  return NVNGX_RESULT_OK;
}

NVNGX_API NVNGX_RESULT
NVSDK_NGX_D3D11_Shutdown1(ID3D11Device *device) {
  return NVNGX_RESULT_OK;
}

NVNGX_API NVNGX_RESULT
NVSDK_NGX_D3D11_GetScratchBufferSize(unsigned int feature, const NVNGXParameter *params, size_t *out_size) {
  IMPLEMENT_ME
  return NVNGX_RESULT_FAIL;
}

NVNGX_API NVNGX_RESULT
NVSDK_NGX_D3D11_ReleaseFeature(unsigned int *handle) {
  IMPLEMENT_ME
  return NVNGX_RESULT_OK;
}

NVNGX_API NVNGX_RESULT
NVSDK_NGX_D3D11_DestroyParameters(NVNGXParameter *params) {
  IMPLEMENT_ME
  return NVNGX_RESULT_FAIL;
}

NVNGX_API NVNGX_RESULT
NVSDK_NGX_D3D11_AllocateParameters(NVNGXParameter **out_params) {
  IMPLEMENT_ME
  return NVNGX_RESULT_FAIL;
}

NVNGX_API NVNGX_RESULT
NVSDK_NGX_UpdateFeature(const void *app_id, const unsigned int feature) {
  IMPLEMENT_ME
  return NVNGX_RESULT_FAIL;
}

NVNGX_API NVNGX_RESULT
NVSDK_NGX_D3D11_GetCapabilityParameters(NVNGXParameter **out_params) {
  IMPLEMENT_ME
  return NVNGX_RESULT_FAIL;
}

}; // namespace dxmt