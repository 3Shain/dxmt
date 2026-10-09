#pragma once

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/Error.h"

#include "../dxil_metadata.hpp"
#include "air_builder.hpp"

namespace dxmt::dxbc {
class IOHelper;
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
  Converter(llvm::Function *entry, dxmt::dxbc::context &ctx);
  llvm::Expected<llvm::BasicBlock *> Run(llvm::BasicBlock *ReturnBB);

private:
  dxmt::dxbc::context &ctx;
  llvm::IRBuilder<> &ir;
  llvm::air::AIRBuilder &air;
  llvm::Function *entry = nullptr;

  llvm::DenseMap<const llvm::Value *, llvm::Value *> value_map_;
  llvm::DenseMap<const llvm::BasicBlock *, llvm::BasicBlock *> block_map_;
  llvm::DenseMap<llvm::Type *, llvm::Type *> type_map_;
  llvm::DenseMap<llvm::Constant *, llvm::Constant *> constant_map_;
  llvm::DenseMap<llvm::GlobalVariable *, llvm::GlobalVariable *> global_map_;
  llvm::DenseMap<const llvm::Value *, HandleInfo> handles_;

  llvm::Type *MapType(llvm::Type *Type);
  llvm::Expected<llvm::Constant *> MapConstantExpr(llvm::ConstantExpr *Expr);
  llvm::Expected<llvm::Constant *> MapConstant(llvm::Constant *Constant);
  llvm::Expected<llvm::Value *> Get(llvm::Value *Value);
  llvm::Expected<HandleInfo> ResolveHandleOperand(llvm::Value *Operand);

  llvm::Error TranslateTerminator(llvm::Instruction *Inst, llvm::BasicBlock *ReturnBB);
  llvm::Expected<llvm::Value *> TranslateInstruction(llvm::Instruction *Inst);
  llvm::Expected<llvm::Value *> TranslateCall(llvm::CallInst &Call);

  llvm::Expected<llvm::Value *> EmitDxilOp(llvm::CallInst &Call);
  llvm::Expected<llvm::Value *> EmitCreateHandle(llvm::CallInst &Call);
  llvm::Expected<llvm::Value *> EmitCbufferLoadLegacy(llvm::CallInst &Call);
  llvm::Expected<llvm::Value *> EmitThreadId(llvm::CallInst &Call, ThreadBuiltin Kind);
};

} // namespace dxmt::dxil
