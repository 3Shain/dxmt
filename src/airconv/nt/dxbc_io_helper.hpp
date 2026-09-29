#pragma once
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/Error.h"
#include "air_builder.hpp"
#include "../airconv_public.h"
#include "../air_signature.hpp"
#include "../air_type.hpp"
#include "../shader_common.hpp"

#include <cstdint>
#include <functional>
#include <vector>

namespace dxmt::dxbc {

struct io_binding_map;

class IOHelper {
public:
  IOHelper(llvm::air::AIRBuilder &air, llvm::Function *function, air::AirType &types, io_binding_map &io_legacy);

  llvm::Error InitializeInput(
      uint32_t FromFunctionArgumentIndex, uint32_t ToRegister, uint32_t Mask, bool InvertWComponent = false
  );

  llvm::Error InitializeInterpolatedInput(
      uint32_t FromFunctionArgumentIndex, uint32_t ToRegister, uint32_t Mask,
      air::Interpolation Interpolation, uint32_t AtSampleIndex
  );

  llvm::Error PullVertexInput(
      uint32_t VertexBufferTableArgumentIndex, uint32_t ToRegister, uint32_t Mask,
      SM50_IA_INPUT_ELEMENT ElementInfo, uint32_t SlotMask
  );

  llvm::Error InitializeVertexIDInput(uint32_t ToRegister, uint32_t Mask);

  llvm::Error InitializeInstanceIDInput(uint32_t ToRegister, uint32_t Mask);

  llvm::Error InitializeInputAttribute(shader::common::InputAttribute Value, uint32_t FromFunctionArgumentIndex);

  llvm::Error CreateDepthOutputRegister();

  llvm::Error CreateStencilRefOutputRegister();

  llvm::Error CreateCoverageMaskOutputRegister();

  llvm::Value *
  PopulateOutput(uint32_t FromRegister, uint32_t Mask, llvm::Value *StructRet, uint32_t AtElement);

  llvm::Value *
  PopulateOutputFixUnorm(uint32_t FromRegister, uint32_t Mask, llvm::Value *StructRet, uint32_t AtElement);

  llvm::Value *
  PopulateOutputSanitizePosition(uint32_t FromRegister, uint32_t Mask, llvm::Value *StructRet, uint32_t AtElement);

  llvm::Value *
  PopulateOutputDepth(llvm::Value *StructRet, uint32_t AtElement);

  llvm::Value *
  PopulateOutputStencilRef(llvm::Value *StructRet, uint32_t AtElement);

  llvm::Value *
  PopulateOutputCoverageMask(llvm::Value *StructRet, uint32_t AtElement, uint32_t SampleMask);

  llvm::Value *
  PopulateDefaultCoverageMask(llvm::Value *StructRet, uint32_t AtElement, uint32_t SampleMask);

  llvm::Value *PopulateOutputStreamOutput(
      llvm::Value *StructRet, const SM50_SHADER_EMULATE_VERTEX_STREAM_OUTPUT_DATA *VertexSO,
      uint32_t BaseVertexArgumentIndex, uint32_t VertexIDArgumentIndex, uint32_t StreamOutputTableArgumentIndex
  );

  void PopulateMeshOutputRenderTargetArrayIndex(uint32_t FromRegister, uint32_t Mask, llvm::Value *PrimitiveId);

  void PopulateMeshOutputViewportArrayIndex(uint32_t FromRegister, uint32_t Mask, llvm::Value *PrimitiveId);

  void PopulateMeshOutputPosition(uint32_t FromRegister, uint32_t Mask, llvm::Value *VertexId);

  void PopulateMeshOutputVertexData(
      uint32_t FromRegister, uint32_t Mask, uint32_t Index, llvm::Value *VertexId,
      air::MSLScalerOrVectorType DesiredType
  );

private:
  llvm::Value *LoadFromArrayAt(llvm::Value *Array, llvm::Value *Index);
  llvm::Value *LoadFromVec4ArrayMasked(llvm::Value *Array, llvm::Value *Index, uint32_t Mask);
  llvm::Error StoreAtVec4ArrayMasked(llvm::Value *Array, llvm::Value *Index, llvm::Value *MaybeVec4, uint32_t Mask);
  llvm::Value *ExtendToVec4(llvm::Value *Value);
  llvm::Value *ToDesiredTypeFromIntVec4(llvm::Value *Vec4, llvm::Type *Desired, uint32_t Mask);
  llvm::Value *GetFloat4Splat(float Value);

  llvm::air::AIRBuilder &air;
  llvm::IRBuilderBase &ir;
  llvm::Function *function;
  air::AirType &types;

  /* Legacy */
  io_binding_map &io;
};

using PrologueFn = std::function<llvm::Error(IOHelper &)>;

class Prologues {
public:
  void
  Add(PrologueFn &&Fn) {
    functions.push_back(std::move(Fn));
  }

  llvm::Error Run(IOHelper &IO);

private:
  std::vector<PrologueFn> functions;
};

using EpilogueFn = std::function<llvm::Expected<llvm::Value *>(llvm::Value *, IOHelper &)>;

class Epilogues {
public:
  void
  Add(EpilogueFn &&Fn) {
    functions.push_back(std::move(Fn));
  }

  llvm::Expected<llvm::Value *> Run(llvm::Type *TyRet, IOHelper &IO);

private:
  std::vector<EpilogueFn> functions;
};

} // namespace dxmt::dxbc
