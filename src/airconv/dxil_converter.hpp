#pragma once

#include "airconv_public.h"

#include <cstddef>

namespace dxmt::dxil {

int initialize(
    const void *pDxilBlob, size_t DxilBlobSize, sm50_shader_t *ppShader, MTL_SHADER_REFLECTION *pRefl,
    sm50_error_t *ppError
);

} // namespace dxmt::dxil
