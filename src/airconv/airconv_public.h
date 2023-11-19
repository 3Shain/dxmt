#pragma once
#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

void ConvertDXBC(const void *pDXBC, uint32_t DXBCSize, void **ppMetalLib,
                 uint32_t* pMetalLibSize);

#ifdef __cplusplus
}
#endif