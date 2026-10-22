#pragma once

#include "airconv_public.h"

#include "llvm/Support/Error.h"

#include <cstddef>

namespace llvm {
class BasicBlock;
class Function;
} // namespace llvm

namespace dxmt::dxbc {
struct context;
class IOHelper;
} // namespace dxmt::dxbc

namespace dxmt::dxil {

struct EntryPoint;

int initialize(
    const void *pDxilBlob, size_t DxilBlobSize, sm50_shader_t *ppShader, MTL_SHADER_REFLECTION *pRefl,
    sm50_error_t *ppError
);

llvm::Expected<llvm::BasicBlock *>
convert_basicblocks(const dxil::EntryPoint &entry, dxmt::dxbc::context &ctx, llvm::BasicBlock *return_bb);

} // namespace dxmt::dxil
