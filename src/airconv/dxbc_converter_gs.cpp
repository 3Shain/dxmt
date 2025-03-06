#include "airconv_error.hpp"
#include "dxbc_converter.hpp"

namespace dxmt::dxbc {

llvm::Error
convert_dxbc_geometry_shader(
    SM50ShaderInternal *pShaderInternal, const char *name, SM50ShaderInternal *pVertexStage, llvm::LLVMContext &context,
    llvm::Module &module, SM50_SHADER_COMPILATION_ARGUMENT_DATA *pArgs
) {
  return llvm::make_error<UnsupportedFeature>("geometry shader not implemented yet");
}

llvm::Error
convert_dxbc_vertex_for_geometry_shader(
    const SM50ShaderInternal *pShaderInternal, const char *name, const SM50ShaderInternal *pGeometryStage,
    llvm::LLVMContext &context, llvm::Module &module, SM50_SHADER_COMPILATION_ARGUMENT_DATA *pArgs
) {
  return llvm::make_error<UnsupportedFeature>("geometry shader not implemented yet");
};

} // namespace dxmt::dxbc