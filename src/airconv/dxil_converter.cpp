#include "dxil_converter.hpp"
#include "dxbc_converter.hpp"
#include "dxil_metadata.hpp"

#include "llvm/IR/Instructions.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/raw_ostream.h"

#include "airconv_error.hpp"
#include "nt/dxil_converter_base.hpp"

#include <memory>
#include <utility>

using namespace llvm;

namespace dxmt::dxil {

llvm::Error
initialize_shader(dxmt::dxbc::SM50ShaderInternal *shader, const Info &info) {
  using namespace microsoft;

  if (info.entry_points.size() != 1)
    return llvm::make_error<UnsupportedFeature>("dxil: only a single entry point is supported");
  const EntryPoint &entry = info.entry_points.front();

  const char *stage = info.shader_model.stage.c_str();
  if (info.shader_model.stage == "cs") {
    shader->shader_type = microsoft::D3D11_SB_COMPUTE_SHADER;
  } else {
    return llvm::make_error<UnsupportedFeature>("dxil: unsupported shader stage " + std::string(stage));
  }
  shader->dxil_entry = const_cast<llvm::Function *>(entry.function);

  auto &func_signature = shader->func_signature;
  if (shader->shader_type == microsoft::D3D11_SB_COMPUTE_SHADER) {
    for (auto &property : entry.properties) {
      if (property.tag != EntryPropertyTag::NumThreads)
        continue;
      auto n = property.num_threads;
      shader->threadgroup_size[0] = n.x;
      shader->threadgroup_size[1] = n.y;
      shader->threadgroup_size[2] = n.z;
      func_signature.UseMaxWorkgroupSize(n.x * n.y * n.z);
    }
  }

  return llvm::Error::success();
}

int
initialize(
    const void *pDxilBlob, size_t DxilBlobSize, sm50_shader_t *ppShader, MTL_SHADER_REFLECTION *pRefl,
    sm50_error_t *ppError
) {
  auto errorObj = std::make_unique<dxmt::dxbc::SM50ErrorInternal>();
  llvm::raw_svector_ostream errorOut(errorObj->buf);

  auto parsed = parse_dxil_blob(llvm::StringRef((const char *)pDxilBlob, DxilBlobSize));
  if (!parsed) {
    errorOut << llvm::toString(parsed.takeError()) << '\0';
    SM50_RETURN_ERROR(errorObj, ppError, 1);
  }
  auto shader = std::make_unique<dxmt::dxbc::SM50ShaderInternal>();
  shader->dxil_context = std::move(parsed->context);
  shader->dxil = std::move(parsed->module);

  if (auto err = initialize_shader(shader.get(), parsed->info)) {
    errorOut << llvm::toString(std::move(err)) << '\0';
    SM50_RETURN_ERROR(errorObj, ppError, 1);
  }

  if (pRefl) {
    *pRefl = {};
    pRefl->ConstanttBufferTableBindIndex = ~0u;
    pRefl->ArgumentBufferBindIndex = ~0u;
    pRefl->NumOutputElement = shader->max_output_register;
    switch (shader->shader_type) {
    case microsoft::D3D11_SB_COMPUTE_SHADER:
      pRefl->ThreadgroupSize[0] = shader->threadgroup_size[0];
      pRefl->ThreadgroupSize[1] = shader->threadgroup_size[1];
      pRefl->ThreadgroupSize[2] = shader->threadgroup_size[2];
      break;
    default:
      break;
    }
  }

  *ppShader = (sm50_shader_t)shader.release();
  return 0;
}

llvm::Expected<llvm::BasicBlock *>
convert_basicblocks(llvm::Function *entry, dxmt::dxbc::context &ctx, llvm::BasicBlock *return_bb) {
  auto saved_block = ctx.builder.GetInsertBlock();
  Converter converter(entry, ctx);
  auto result = converter.Run(return_bb);
  ctx.builder.SetInsertPoint(saved_block);
  return result;
}

} // namespace dxmt::dxil
