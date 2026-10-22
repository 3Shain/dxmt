#pragma once

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/Error.h"

#include "../dxil_metadata.hpp"
#include "air_builder.hpp"
#include "dxbc_binding_map.hpp"

namespace dxmt::dxbc {
struct context;
} // namespace dxmt::dxbc

namespace dxmt::dxil {

struct HandleInfo {
  ResourceClass resource_class;
  uint32_t range_id = 0;
  llvm::Value *index = nullptr;
};

enum class ThreadBuiltin : uint32_t {
  kGlobalId = 0,
  kGroupId = 1,
  kLocalId = 2,
  kLocalLinearId = 3,
};

llvm::Error unsupported(llvm::StringRef what);

class Converter {
public:
  Converter(const EntryPoint &entry, dxmt::dxbc::context &ctx);
  llvm::Expected<llvm::BasicBlock *> Run(llvm::BasicBlock *ReturnBB);

private:
  dxmt::dxbc::context &ctx;
  llvm::IRBuilder<> &ir;
  llvm::air::AIRBuilder &air;
  const EntryPoint &entry;

  llvm::DenseMap<const llvm::Value *, llvm::Value *> value_map_;
  llvm::DenseMap<const llvm::BasicBlock *, llvm::BasicBlock *> block_map_;
  llvm::DenseMap<llvm::Type *, llvm::Type *> type_map_;
  llvm::DenseMap<llvm::Constant *, llvm::Constant *> constant_map_;
  llvm::DenseMap<llvm::GlobalVariable *, llvm::GlobalVariable *> global_map_;
  llvm::DenseMap<const llvm::Value *, HandleInfo> handles_;

  llvm::Type *MapType(llvm::Type *Type);
  llvm::Expected<llvm::Constant *> MapConstantExpr(llvm::ConstantExpr *Expr);
  llvm::Expected<llvm::Constant *> MapConstant(llvm::Constant *Constant);
  llvm::Expected<llvm::GlobalVariable *> MapGlobal(llvm::GlobalVariable *Constant);
  llvm::Expected<llvm::Value *> Get(llvm::Value *Value);
  llvm::Expected<HandleInfo> ResolveHandleOperand(llvm::Value *Operand);
  llvm::Optional<dxmt::dxbc::TextureDescirptor> GetTextureDescriptor(const HandleInfo &Handle);
  llvm::Optional<dxmt::dxbc::BufferDescriptor> GetBufferDescriptor(const HandleInfo &Handle);
  llvm::Expected<dxmt::dxbc::SamplerDescriptor> GetSamplerDescriptor(const HandleInfo &Handle);
  bool SupportsMemoryCoherency() const;
  bool SupportsNonExecutionBarrier() const;
  static llvm::air::Texture MakeTexture(const dxmt::dxbc::TextureDescirptor &Descriptor);
  llvm::Value *BitcastIfNeeded(llvm::Value *Value, llvm::Type *Type);
  llvm::Value *BuildResRet(const llvm::CallInst &Call, llvm::Value *VecOrScalar, llvm::Value *Residency);
  llvm::Value *MakeVec(llvm::ArrayRef<llvm::Value *> Components);
  llvm::Value *AddOffsetIfNeeded(llvm::Value *Coord, int32_t Offset);
  llvm::Expected<std::tuple<llvm::Value *, llvm::Value *, llvm::Value *>> BuildRwAddress(
      llvm::air::Texture::ResourceKind LogicalKind, llvm::Value *C0, llvm::Value *C1, llvm::Value *C2,
      llvm::Value *MipOrSampleIndex, int32_t Off0, int32_t Off1, int32_t Off2
  );
  llvm::Type *GetTextureSampleType(llvm::air::Texture::SampleType SampleType);
  llvm::Value *GetSamplePos(llvm::Value *SampleCount, llvm::Value *Index);

  llvm::Error TranslateTerminator(llvm::Instruction *Inst, llvm::BasicBlock *ReturnBB);
  llvm::Expected<llvm::Value *> TranslateInstruction(llvm::Instruction *Inst);
  llvm::Expected<llvm::Value *> TranslateCall(llvm::CallInst &Call);

  llvm::Expected<llvm::Value *> EmitDxilOp(llvm::CallInst &Call);
  llvm::Expected<llvm::Value *> EmitCreateHandle(llvm::CallInst &Call);
  llvm::Expected<llvm::Value *> EmitCbufferLoadLegacy(llvm::CallInst &Call);
  llvm::Expected<llvm::Value *> EmitThreadId(llvm::CallInst &Call, ThreadBuiltin Kind);
  llvm::Expected<llvm::Value *> EmitBufferStore(llvm::CallInst &Call);
  llvm::Expected<llvm::Value *> EmitBufferLoad(llvm::CallInst &Call);
  llvm::Expected<llvm::Value *> EmitTextureStore(llvm::CallInst &Call);
  llvm::Expected<llvm::Value *> EmitTextureLoad(llvm::CallInst &Call);
  llvm::Expected<llvm::Value *> EmitBufferUpdateCounter(llvm::CallInst &Call);
  llvm::Expected<llvm::Value *> EmitGetDimensions(llvm::CallInst &Call);
  llvm::Expected<llvm::Value *> EmitRawBufferStore(llvm::CallInst &Call);
  llvm::Expected<llvm::Value *> EmitRawBufferLoad(llvm::CallInst &Call);
  llvm::Expected<llvm::Value *> EmitLegacyF16ToF32(llvm::CallInst &Call);
  llvm::Expected<llvm::Value *> EmitLegacyF32ToF16(llvm::CallInst &Call);
  llvm::Expected<llvm::Value *> EmitUnaryBits(llvm::CallInst &Call);
  llvm::Expected<llvm::Value *> EmitBarrier(llvm::CallInst &Call);
  llvm::Expected<llvm::Value *> EmitLoadInput(const llvm::CallInst &Call);
  llvm::Expected<llvm::Value *> EmitStoreOutput(const llvm::CallInst &Call);
  llvm::Expected<llvm::Value *> EmitSample(const llvm::CallInst &Call, llvm::StringRef Op);
  llvm::Expected<llvm::Value *> EmitGather(const llvm::CallInst &Call, llvm::StringRef Op);
  llvm::Expected<llvm::Value *> EmitUnary(const llvm::CallInst &Call);
  llvm::Expected<llvm::Value *> EmitBinary(const llvm::CallInst &Call);
  llvm::Expected<llvm::Value *> EmitTertiary(const llvm::CallInst &Call);
  llvm::Expected<llvm::Value *> EmitDotProduct(const llvm::CallInst &Call, unsigned Dimension);
  llvm::Expected<llvm::Value *> EmitIsSpecialFloat(const llvm::CallInst &Call);
  llvm::Expected<llvm::Value *> EmitAtomicRMW(const llvm::CallInst &Call);
  llvm::Expected<llvm::Value *> EmitAtomicCmpXchg(const llvm::CallInst &Call);
  llvm::Expected<llvm::Value *> EmitDiscard(const llvm::CallInst &Call);
  llvm::Expected<llvm::Value *> EmitCalculateLOD(const llvm::CallInst &Call);
  llvm::Expected<llvm::Value *> EmitGetTextureSamplePos(const llvm::CallInst &Call);
  llvm::Expected<llvm::Value *> EmitReadLaneAt(const llvm::CallInst &Call, bool Quad);
  llvm::Expected<llvm::Value *> EmitQuadOp(const llvm::CallInst &Call);
};

} // namespace dxmt::dxil
