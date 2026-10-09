#include "dxil_converter.hpp"
#include "dxbc_converter.hpp"

namespace dxmt::dxil {
int
initialize(
    const void *pDxilBlob, size_t DxilBlobSize, sm50_shader_t *ppShader, MTL_SHADER_REFLECTION *pRefl,
    sm50_error_t *ppError
) {
  auto errorObj = std::make_unique<dxmt::dxbc::SM50ErrorInternal>();
  llvm::raw_svector_ostream errorOut(errorObj->buf);

  errorOut << "dxil shader is not supported\0";
  SM50_RETURN_ERROR(errorObj, ppError, 1);
}

} // namespace dxmt::dxil
