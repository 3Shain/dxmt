#pragma once
#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

struct MetalShaderReflection {
    bool has_binding_map;
};

void ConvertDXBC(const void *pDXBC, uint32_t DXBCSize, void **ppMetalLib,
                 uint32_t* pMetalLibSize, MetalShaderReflection* reflection);

#ifdef __cplusplus
}
#endif