#include "dxil_converter_base.hpp"

#include "../airconv_error.hpp"
#include "../dxbc_converter.hpp"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/raw_ostream.h"

#include <string>
#include <utility>

using namespace llvm;
using namespace llvm::air;

namespace dxmt::dxil {

llvm::Error
unsupported(llvm::StringRef what) {
  return llvm::make_error<dxmt::UnsupportedFeature>(what);
}

std::optional<uint32_t>
asU32(Value *value) {
  if (auto *ci = dyn_cast<ConstantInt>(value))
    return (uint32_t)ci->getZExtValue();
  return std::nullopt;
}

Converter::Converter(llvm::Function *entry, dxmt::dxbc::context &ctx) :
    ctx(ctx),
    ir(ctx.builder),
    air(ctx.air),
    entry(entry) {}

llvm::Expected<llvm::BasicBlock *>
Converter::Run(llvm::BasicBlock *ReturnBB) {
  if (!entry)
    return unsupported("dxil: entry point is missing");

  {
    for (auto &BB : *entry) {
      auto *NewBB = llvm::BasicBlock::Create(air.getContext(), "", ctx.function);
      block_map_[&BB] = NewBB;
      ir.SetInsertPoint(NewBB);
      for (auto &Inst : BB) {
        auto *PHI = dyn_cast<PHINode>(&Inst);
        if (!PHI)
          break;
        value_map_[PHI] = ir.CreatePHI(MapType(PHI->getType()), 0);
      }
    }

    for (auto &BB : *entry) {
      ir.SetInsertPoint(block_map_[&BB]);
      for (auto &Inst : BB) {
        if (isa<PHINode>(&Inst))
          continue;
        if (Inst.isTerminator()) {
          if (auto err = TranslateTerminator(&Inst, ReturnBB))
            return err;
          break;
        }
        auto Result = TranslateInstruction(&Inst);
        if (!Result)
          return Result.takeError();
        if (*Result && !(*Result)->getType()->isVoidTy())
          value_map_[&Inst] = *Result;
      }
    }
  }

  return block_map_[&entry->getEntryBlock()];
}

llvm::Type *
Converter::MapType(llvm::Type *Type) {
  auto Found = type_map_.find(Type);
  if (Found != type_map_.end())
    return Found->second;

  llvm::Type *TyMapped = nullptr;
  if (Type->isVoidTy()) {
    TyMapped = llvm::Type::getVoidTy(air.getContext());
  } else if (Type->isIntegerTy()) {
    TyMapped = llvm::IntegerType::get(air.getContext(), Type->getIntegerBitWidth());
  } else if (Type->isHalfTy()) {
    TyMapped = llvm::Type::getHalfTy(air.getContext());
  } else if (Type->isFloatTy()) {
    TyMapped = llvm::Type::getFloatTy(air.getContext());
  } else if (Type->isDoubleTy()) {
    TyMapped = llvm::Type::getDoubleTy(air.getContext());
  } else if (auto *pointer = llvm::dyn_cast<llvm::PointerType>(Type)) {
    TyMapped = MapType(pointer->getNonOpaquePointerElementType())->getPointerTo(pointer->getAddressSpace());
  } else if (auto *array = llvm::dyn_cast<llvm::ArrayType>(Type)) {
    TyMapped = llvm::ArrayType::get(MapType(array->getElementType()), array->getNumElements());
  } else if (auto *vector = llvm::dyn_cast<llvm::FixedVectorType>(Type)) {
    TyMapped = llvm::FixedVectorType::get(MapType(vector->getElementType()), vector->getNumElements());
  } else if (auto *function = llvm::dyn_cast<llvm::FunctionType>(Type)) {
    llvm::SmallVector<llvm::Type *, 8> Params;
    for (auto *param : function->params())
      Params.push_back(MapType(param));
    TyMapped = llvm::FunctionType::get(MapType(function->getReturnType()), Params, function->isVarArg());
  } else if (auto *TyStruct = llvm::dyn_cast<llvm::StructType>(Type)) {
    if (TyStruct->isOpaque()) {
      TyMapped = llvm::StructType::create(air.getContext(), TyStruct->getName());
    } else if (TyStruct->hasName()) {
      auto *TyNamed = llvm::StructType::create(air.getContext(), TyStruct->getName());
      type_map_[Type] = TyNamed;
      llvm::SmallVector<llvm::Type *, 8> Elements;
      for (auto *TyElement : TyStruct->elements())
        Elements.push_back(MapType(TyElement));
      TyNamed->setBody(Elements);
      TyMapped = TyNamed;
    } else {
      llvm::SmallVector<llvm::Type *, 8> Elements;
      for (auto *TyElement : TyStruct->elements())
        Elements.push_back(MapType(TyElement));
      TyMapped = llvm::StructType::get(air.getContext(), Elements);
    }
  } else {
    TyMapped = Type;
  }

  type_map_[Type] = TyMapped;
  return TyMapped;
}

llvm::Expected<llvm::Constant *>
Converter::MapConstantExpr(llvm::ConstantExpr *Expr) {
  switch (Expr->getOpcode()) {
  case llvm::Instruction::GetElementPtr: {
    llvm::SmallVector<llvm::Constant *, 8> Indices;
    for (unsigned i = 1; i < Expr->getNumOperands(); i++) {
      auto Index = MapConstant(llvm::cast<llvm::Constant>(Expr->getOperand(i)));
      if (!Index)
        return Index.takeError();
      Indices.push_back(*Index);
    }
    auto Ptr = MapConstant(llvm::cast<llvm::Constant>(Expr->getOperand(0)));
    if (!Ptr)
      return Ptr.takeError();
    auto *TySrc = MapType(Expr->getOperand(0)->getType()->getNonOpaquePointerElementType());
    return llvm::ConstantExpr::getGetElementPtr(TySrc, *Ptr, Indices);
  }
  case llvm::Instruction::BitCast:
  case llvm::Instruction::Trunc:
  case llvm::Instruction::ZExt:
  case llvm::Instruction::SExt:
  case llvm::Instruction::FPTrunc:
  case llvm::Instruction::FPExt:
  case llvm::Instruction::UIToFP:
  case llvm::Instruction::SIToFP:
  case llvm::Instruction::FPToUI:
  case llvm::Instruction::FPToSI:
  case llvm::Instruction::PtrToInt:
  case llvm::Instruction::IntToPtr:
  case llvm::Instruction::AddrSpaceCast: {
    auto Operand = MapConstant(llvm::cast<llvm::Constant>(Expr->getOperand(0)));
    if (!Operand)
      return Operand.takeError();
    return llvm::ConstantExpr::getCast(Expr->getOpcode(), *Operand, MapType(Expr->getType()));
  }
  default:
    return unsupported(
        std::string("dxil: unsupported constant expression: ") + Instruction::getOpcodeName(Expr->getOpcode())
    );
  }
}

llvm::Expected<llvm::Constant *>
Converter::MapConstant(llvm::Constant *Constant) {
  auto Found = constant_map_.find(Constant);
  if (Found != constant_map_.end())
    return Found->second;

  llvm::Constant *Mapped = nullptr;
  if (auto *Integer = llvm::dyn_cast<llvm::ConstantInt>(Constant)) {
    Mapped = llvm::ConstantInt::get(MapType(Integer->getType()), Integer->getValue());
  } else if (auto *FP = llvm::dyn_cast<llvm::ConstantFP>(Constant)) {
    Mapped = llvm::ConstantFP::get(air.getContext(), FP->getValueAPF());
  } else if (llvm::isa<llvm::ConstantPointerNull>(Constant)) {
    Mapped = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(MapType(Constant->getType())));
  } else if (llvm::isa<llvm::UndefValue>(Constant)) {
    Mapped = llvm::UndefValue::get(MapType(Constant->getType()));
  } else if (llvm::isa<llvm::PoisonValue>(Constant)) {
    Mapped = llvm::PoisonValue::get(MapType(Constant->getType()));
  } else if (llvm::isa<llvm::ConstantAggregateZero>(Constant)) {
    Mapped = llvm::ConstantAggregateZero::get(MapType(Constant->getType()));
  } else if (llvm::isa<llvm::GlobalVariable>(Constant)) {
    return unsupported("dxil: unhandled GlobalVariable");
  } else if (auto *Sequence = llvm::dyn_cast<llvm::ConstantDataSequential>(Constant)) {
    llvm::SmallVector<llvm::Constant *, 16> Elements;
    for (unsigned i = 0; i < Sequence->getNumElements(); i++) {
      auto Element = MapConstant(Sequence->getElementAsConstant(i));
      if (!Element)
        return Element.takeError();
      Elements.push_back(*Element);
    }
    if (Sequence->getType()->isVectorTy())
      Mapped = llvm::ConstantVector::get(Elements);
    else
      Mapped = llvm::ConstantArray::get(llvm::cast<llvm::ArrayType>(MapType(Sequence->getType())), Elements);
  } else if (auto *Vector = llvm::dyn_cast<llvm::ConstantVector>(Constant)) {
    llvm::SmallVector<llvm::Constant *, 16> Elements;
    for (auto &Operand : Vector->operands()) {
      auto Element = MapConstant(llvm::cast<llvm::Constant>(Operand));
      if (!Element)
        return Element.takeError();
      Elements.push_back(*Element);
    }
    Mapped = llvm::ConstantVector::get(Elements);
  } else if (auto *Array = llvm::dyn_cast<llvm::ConstantArray>(Constant)) {
    llvm::SmallVector<llvm::Constant *, 16> Elements;
    for (auto &Operand : Array->operands()) {
      auto Element = MapConstant(llvm::cast<llvm::Constant>(Operand));
      if (!Element)
        return Element.takeError();
      Elements.push_back(*Element);
    }
    Mapped = llvm::ConstantArray::get(llvm::cast<llvm::ArrayType>(MapType(Array->getType())), Elements);
  } else if (auto *Structure = llvm::dyn_cast<llvm::ConstantStruct>(Constant)) {
    llvm::SmallVector<llvm::Constant *, 16> Elements;
    for (auto &Operand : Structure->operands()) {
      auto Element = MapConstant(llvm::cast<llvm::Constant>(Operand));
      if (!Element)
        return Element.takeError();
      Elements.push_back(*Element);
    }
    Mapped = llvm::ConstantStruct::get(llvm::cast<llvm::StructType>(MapType(Structure->getType())), Elements);
  } else if (auto *Expr = llvm::dyn_cast<llvm::ConstantExpr>(Constant)) {
    auto MappedExpr = MapConstantExpr(Expr);
    if (!MappedExpr)
      return MappedExpr.takeError();
    Mapped = *MappedExpr;
  } else {
    return unsupported("dxil: unsupported Constant");
  }

  constant_map_[Constant] = Mapped;
  return Mapped;
}

llvm::Expected<llvm::Value *>
Converter::Get(llvm::Value *Value) {
  auto Found = value_map_.find(Value);
  if (Found != value_map_.end())
    return Found->second;
  if (auto *Constant = llvm::dyn_cast<llvm::Constant>(Value)) {
    auto Mapped = MapConstant(Constant);
    if (!Mapped)
      return Mapped.takeError();
    return *Mapped;
  }
  return unsupported("dxil: reference to untranslated Value");
}

llvm::Error
Converter::TranslateTerminator(llvm::Instruction *Inst, llvm::BasicBlock *ReturnBB) {
  if (auto *Return = dyn_cast<ReturnInst>(Inst)) {
    if (Return->getNumOperands())
      return unsupported("dxil: non-void return in a compute shader");
    ir.CreateBr(ReturnBB);
  } else if (auto *Branch = dyn_cast<BranchInst>(Inst)) {
    if (Branch->isConditional()) {
      auto Cond = Get(Branch->getCondition());
      if (!Cond)
        return Cond.takeError();
      ir.CreateCondBr(*Cond, block_map_[Branch->getSuccessor(0)], block_map_[Branch->getSuccessor(1)]);
    } else {
      ir.CreateBr(block_map_[Branch->getSuccessor(0)]);
    }
  } else if (llvm::isa<llvm::UnreachableInst>(Inst)) {
    ir.CreateUnreachable();
  } else if (auto *Switch = dyn_cast<SwitchInst>(Inst)) {
    auto Cond = Get(Switch->getCondition());
    if (!Cond)
      return Cond.takeError();
    auto *NewSwitch = ir.CreateSwitch(*Cond, block_map_[Switch->getDefaultDest()], Switch->getNumCases());
    for (auto &Case : Switch->cases()) {
      auto CaseValue = MapConstant(Case.getCaseValue());
      if (!CaseValue)
        return CaseValue.takeError();
      NewSwitch->addCase(llvm::cast<llvm::ConstantInt>(*CaseValue), block_map_[Case.getCaseSuccessor()]);
    }
  } else {
    return unsupported("dxil: unsupported terminator");
  }

  auto *NewBB = ir.GetInsertBlock();
  SmallPtrSet<BasicBlock *, 4> Seen;
  for (auto *Succ : successors(Inst->getParent())) {
    auto Found = block_map_.find(Succ);
    if (Found == block_map_.end() || !Seen.insert(Succ).second)
      continue;
    for (auto &InstInSucc : *Succ) {
      auto *PHI = dyn_cast<PHINode>(&InstInSucc);
      if (!PHI)
        break;
      auto MapFound = value_map_.find(PHI);
      if (MapFound == value_map_.end())
        return unsupported("dxil: phi without translation");
      auto Incoming = Get(PHI->getIncomingValueForBlock(Inst->getParent()));
      if (!Incoming)
        return Incoming.takeError();
      cast<PHINode>(MapFound->second)->addIncoming(*Incoming, NewBB);
    }
  }
  return llvm::Error::success();
}

llvm::Expected<llvm::Value *>
Converter::TranslateInstruction(llvm::Instruction *Inst) {
  auto operand = [&](unsigned index) { return Get(Inst->getOperand(index)); };

  switch (Inst->getOpcode()) {
  case llvm::Instruction::Add:
  case llvm::Instruction::Sub:
  case llvm::Instruction::Mul:
  case llvm::Instruction::UDiv:
  case llvm::Instruction::SDiv:
  case llvm::Instruction::URem:
  case llvm::Instruction::SRem:
  case llvm::Instruction::Shl:
  case llvm::Instruction::LShr:
  case llvm::Instruction::AShr:
  case llvm::Instruction::And:
  case llvm::Instruction::Or:
  case llvm::Instruction::Xor:
  case llvm::Instruction::FAdd:
  case llvm::Instruction::FSub:
  case llvm::Instruction::FMul:
  case llvm::Instruction::FDiv:
  case llvm::Instruction::FRem: {
    auto LHS = operand(0);
    if (!LHS)
      return LHS.takeError();
    auto RHS = operand(1);
    if (!RHS)
      return RHS.takeError();
    auto *BinOp = llvm::BinaryOperator::Create((llvm::Instruction::BinaryOps)Inst->getOpcode(), *LHS, *RHS);
    if (llvm::isa<llvm::FPMathOperator>(BinOp))
      BinOp->copyFastMathFlags(Inst);
    ir.Insert(BinOp);
    return BinOp;
  }
  case llvm::Instruction::FNeg: {
    auto Value = operand(0);
    if (!Value)
      return Value.takeError();
    auto *UnOp = llvm::UnaryOperator::Create((llvm::Instruction::UnaryOps)Inst->getOpcode(), *Value);
    if (llvm::isa<llvm::FPMathOperator>(UnOp))
      UnOp->copyFastMathFlags(Inst);
    ir.Insert(UnOp);
    return UnOp;
  }
  case llvm::Instruction::ICmp:
  case llvm::Instruction::FCmp: {
    auto LHS = operand(0);
    if (!LHS)
      return LHS.takeError();
    auto RHS = operand(1);
    if (!RHS)
      return RHS.takeError();
    auto *Cmp = llvm::CmpInst::Create(
        (llvm::Instruction::OtherOps)Inst->getOpcode(), llvm::cast<llvm::CmpInst>(Inst)->getPredicate(), *LHS, *RHS
    );
    if (llvm::isa<llvm::FPMathOperator>(Cmp))
      Cmp->copyFastMathFlags(Inst);
    ir.Insert(Cmp);
    return Cmp;
  }
  case llvm::Instruction::Select: {
    auto Cond = operand(0);
    if (!Cond)
      return Cond.takeError();
    auto True = operand(1);
    if (!True)
      return True.takeError();
    auto False = operand(2);
    if (!False)
      return False.takeError();
    return ir.CreateSelect(*Cond, *True, *False);
  }
  case llvm::Instruction::Trunc:
  case llvm::Instruction::ZExt:
  case llvm::Instruction::SExt:
  case llvm::Instruction::FPTrunc:
  case llvm::Instruction::FPExt:
  case llvm::Instruction::FPToUI:
  case llvm::Instruction::FPToSI:
  case llvm::Instruction::UIToFP:
  case llvm::Instruction::SIToFP:
  case llvm::Instruction::PtrToInt:
  case llvm::Instruction::IntToPtr:
  case llvm::Instruction::BitCast:
  case llvm::Instruction::AddrSpaceCast: {
    auto Value = operand(0);
    if (!Value)
      return Value.takeError();
    auto *Cast =
        llvm::CastInst::Create((llvm::Instruction::CastOps)Inst->getOpcode(), *Value, MapType(Inst->getType()));
    ir.Insert(Cast);
    return Cast;
  }
  case llvm::Instruction::GetElementPtr: {
    auto *OldGEP = llvm::cast<llvm::GetElementPtrInst>(Inst);
    auto Ptr = operand(0);
    if (!Ptr)
      return Ptr.takeError();
    llvm::SmallVector<llvm::Value *, 8> Indices;
    for (unsigned i = 1; i < Inst->getNumOperands(); i++) {
      auto Index = operand(i);
      if (!Index)
        return Index.takeError();
      Indices.push_back(*Index);
    }
    auto *GEP = llvm::GetElementPtrInst::Create(MapType(OldGEP->getSourceElementType()), *Ptr, Indices);
    GEP->setIsInBounds(OldGEP->isInBounds());
    ir.Insert(GEP);
    return GEP;
  }
  case llvm::Instruction::Load: {
    auto *OldLoad = llvm::cast<llvm::LoadInst>(Inst);
    auto Ptr = operand(0);
    if (!Ptr)
      return Ptr.takeError();
    auto *Load = new llvm::LoadInst(MapType(Inst->getType()), *Ptr, "", OldLoad->isVolatile(), OldLoad->getAlign());
    if (OldLoad->isAtomic())
      Load->setOrdering(OldLoad->getOrdering());
    ir.Insert(Load);
    return Load;
  }
  case llvm::Instruction::Store: {
    auto *OldStore = llvm::cast<llvm::StoreInst>(Inst);
    auto Value = operand(0);
    if (!Value)
      return Value.takeError();
    auto Ptr = operand(1);
    if (!Ptr)
      return Ptr.takeError();
    auto *Store = new llvm::StoreInst(*Value, *Ptr, OldStore->isVolatile(), OldStore->getAlign());
    if (OldStore->isAtomic())
      Store->setOrdering(OldStore->getOrdering());
    ir.Insert(Store);
    return (llvm::Value *)nullptr;
  }
  case llvm::Instruction::Alloca: {
    auto *OldAlloca = llvm::cast<llvm::AllocaInst>(Inst);
    llvm::Value *Size = nullptr;
    if (OldAlloca->isArrayAllocation()) {
      auto MappedSize = operand(0);
      if (!MappedSize)
        return MappedSize.takeError();
      Size = *MappedSize;
    }
    auto *Alloca = new llvm::AllocaInst(
        MapType(OldAlloca->getAllocatedType()), OldAlloca->getAddressSpace(), Size, OldAlloca->getAlign()
    );
    ir.Insert(Alloca);
    return Alloca;
  }
  case llvm::Instruction::ExtractElement: {
    auto Vectpr = operand(0);
    if (!Vectpr)
      return Vectpr.takeError();
    auto Index = operand(1);
    if (!Index)
      return Index.takeError();
    auto *ExtractElement = llvm::ExtractElementInst::Create(*Vectpr, *Index);
    ir.Insert(ExtractElement);
    return ExtractElement;
  }
  case llvm::Instruction::InsertElement: {
    auto Vector = operand(0);
    if (!Vector)
      return Vector.takeError();
    auto Element = operand(1);
    if (!Element)
      return Element.takeError();
    auto Index = operand(2);
    if (!Index)
      return Index.takeError();
    auto *InsertElement = llvm::InsertElementInst::Create(*Vector, *Element, *Index);
    ir.Insert(InsertElement);
    return InsertElement;
  }
  case llvm::Instruction::ShuffleVector: {
    auto First = operand(0);
    if (!First)
      return First.takeError();
    auto Second = operand(1);
    if (!Second)
      return Second.takeError();
    auto Mask = operand(2);
    if (!Mask)
      return Mask.takeError();
    return ir.CreateShuffleVector(*First, *Second, *Mask);
  }
  case llvm::Instruction::ExtractValue: {
    auto *OldExtract = llvm::cast<llvm::ExtractValueInst>(Inst);
    auto Aggregate = operand(0);
    if (!Aggregate)
      return Aggregate.takeError();
    auto *ExtractValue = llvm::ExtractValueInst::Create(*Aggregate, OldExtract->getIndices());
    ir.Insert(ExtractValue);
    return ExtractValue;
  }
  case llvm::Instruction::InsertValue: {
    auto *OldInsert = llvm::cast<llvm::InsertValueInst>(Inst);
    auto Aggregate = operand(0);
    if (!Aggregate)
      return Aggregate.takeError();
    auto Value = operand(1);
    if (!Value)
      return Value.takeError();
    auto *InsertValue = llvm::InsertValueInst::Create(*Aggregate, *Value, OldInsert->getIndices());
    ir.Insert(InsertValue);
    return InsertValue;
  }
  case llvm::Instruction::Freeze: {
    auto Value = operand(0);
    if (!Value)
      return Value.takeError();
    return ir.CreateFreeze(*Value);
  }
  case llvm::Instruction::Call:
    return TranslateCall(*llvm::cast<llvm::CallInst>(Inst));
  default:
    return unsupported(std::string("dxil: unsupported instruction: ") + Inst->getOpcodeName());
  }
}

llvm::Expected<HandleInfo>
Converter::ResolveHandleOperand(llvm::Value *Operand) {
  auto Found = handles_.find(Operand);
  if (Found == handles_.end())
    return unsupported("dxil: unresolvable resource handle");
  return Found->second;
}

llvm::Optional<dxmt::dxbc::TextureDescirptor>
Converter::GetTextureDescriptor(const HandleInfo &Handle) {
  return Handle.resource_class == ResourceClass::SRV ? ctx.binding.GetSRVTexture(air, Handle.range_id, Handle.index)
                                                     : ctx.binding.GetUAVTexture(air, Handle.range_id, Handle.index);
}

llvm::Optional<dxmt::dxbc::BufferDescriptor>
Converter::GetBufferDescriptor(const HandleInfo &Handle) {
  return Handle.resource_class == ResourceClass::SRV ? ctx.binding.GetSRVBuffer(air, Handle.range_id, Handle.index)
                                                     : ctx.binding.GetUAVBuffer(air, Handle.range_id, Handle.index);
}

bool
Converter::SupportsMemoryCoherency() const {
  return ctx.metal_version >= SM50_SHADER_METAL_320;
}

llvm::air::Texture
Converter::MakeTexture(const dxmt::dxbc::TextureDescirptor &Descriptor) {
  return llvm::air::Texture{
      .kind = Descriptor.ResourceKind,
      .sample_type = Descriptor.SampleType,
      .memory_access = Descriptor.MemoryAccess,
  };
}

Value *
Converter::BitcastIfNeeded(Value *Value, Type *Type) {
  if (Value->getType() == Type)
    return Value;
  return ir.CreateBitCast(Value, Type);
}

llvm::Expected<llvm::Value *>
Converter::TranslateCall(llvm::CallInst &Call) {
  auto *Callee = Call.getCalledFunction();
  if (!Callee)
    return unsupported("dxil: indirect call");
  if (Callee->getName().startswith("dx.op."))
    return EmitDxilOp(Call);
  return unsupported("dxil: unsupported call to " + Callee->getName().str());
}

llvm::Expected<llvm::Value *>
Converter::EmitDxilOp(CallInst &Call) {
  auto *callee = Call.getCalledFunction();
  StringRef op = callee->getName().drop_front(strlen("dx.op."));
  auto dot = op.rfind('.');
  if (dot != StringRef::npos)
    op = op.take_front(dot);

  if (op == "createHandle")
    return EmitCreateHandle(Call);
  if (op == "cbufferLoadLegacy")
    return EmitCbufferLoadLegacy(Call);
  if (op == "threadId")
    return EmitThreadId(Call, ThreadBuiltin::kGlobalId);
  if (op == "groupId")
    return EmitThreadId(Call, ThreadBuiltin::kGroupId);
  if (op == "threadIdInGroup")
    return EmitThreadId(Call, ThreadBuiltin::kLocalId);
  if (op == "flattenedThreadIdInGroup")
    return EmitThreadId(Call, ThreadBuiltin::kLocalLinearId);
  if (op == "bufferStore")
    return EmitBufferStore(Call);

  return unsupported("dxil: unsupported operation dx.op." + op.str());
}

llvm::Expected<llvm::Value *>
Converter::EmitCreateHandle(CallInst &Call) {
  auto Class = asU32(Call.getArgOperand(1));
  auto Range = asU32(Call.getArgOperand(2));
  if (!Class || !Range)
    return unsupported("dxil: non-constant createHandle arguments");
  auto Index = Get(Call.getArgOperand(3));
  if (!Index)
    return Index.takeError();
  handles_[&Call] = HandleInfo{(ResourceClass)*Class, *Range, *Index};
  return (llvm::Value *)nullptr;
}

llvm::Expected<llvm::Value *>
Converter::EmitCbufferLoadLegacy(CallInst &Call) {
  auto Handle = ResolveHandleOperand(Call.getArgOperand(1));
  if (!Handle)
    return Handle.takeError();
  auto Descriptor = ctx.binding.GetConstantBuffer(air, Handle->range_id, Handle->index);
  if (!Descriptor)
    return unsupported("dxil: no constant buffer descriptor for resource");
  auto Index = Get(Call.getArgOperand(2));
  if (!Index)
    return Index.takeError();

  auto *TyResult = MapType(Call.getType());
  auto *TyElement = TyResult->getStructElementType(0);
  auto *PackedType = FixedVectorType::get(TyElement, TyResult->getStructNumElements());
  auto *PackedPtr = ir.CreateBitCast(
      ir.CreateGEP(air.getIntTy(4), Descriptor->Pointer, {*Index}),
      PackedType->getPointerTo(Descriptor->Pointer->getType()->getPointerAddressSpace())
  );
  auto *PackedLoad = ir.CreateLoad(PackedType, PackedPtr);
  llvm::Value *Result = PoisonValue::get(TyResult);
  for (unsigned i = 0; i < PackedType->getNumElements(); i++)
    Result = ir.CreateInsertValue(Result, ir.CreateExtractElement(PackedLoad, (uint64_t)i), i);
  return Result;
}

llvm::Expected<llvm::Value *>
Converter::EmitThreadId(CallInst &Call, ThreadBuiltin Kind) {
  if (Kind == ThreadBuiltin::kLocalLinearId)
    return air.CreateGetLocalLinearID();
  auto component = asU32(Call.getArgOperand(1));
  if (!component || *component > 2)
    return unsupported("dxil: invalid thread id component");
  switch (Kind) {
  case ThreadBuiltin::kGlobalId:
    return air.CreateGetGlobalID(*component);
  case ThreadBuiltin::kGroupId:
    return air.CreateGetGroupID(*component);
  default:
    return air.CreateGetLocalID(*component);
  }
}

llvm::Expected<llvm::Value *>
Converter::EmitBufferStore(CallInst &Call) {
  auto Handle = ResolveHandleOperand(Call.getArgOperand(1));
  if (!Handle)
    return Handle.takeError();

  auto Index = Get(Call.getArgOperand(2));
  if (!Index)
    return Index.takeError();
  auto Mask = asU32(Call.getArgOperand(8));
  if (!Mask)
    return unsupported("dxil: non-constant bufferStore mask");

  auto Texture = GetTextureDescriptor(*Handle);
  if (Texture) {
    auto TyElement = Texture->SampleType == Texture::sample_float ? (Type *)ir.getFloatTy() : (Type *)ir.getInt32Ty();
    auto *VecType = FixedVectorType::get(TyElement, 4);
    Value *Value = PoisonValue::get(VecType);
    for (unsigned i = 0; i < 4; i++) {
      if (!(*Mask & (1u << i)))
        continue;
      auto component = Get(Call.getArgOperand(4 + i));
      if (!component)
        return component.takeError();
      Value = ir.CreateInsertElement(Value, BitcastIfNeeded(*component, TyElement), (uint64_t)i);
    }
    bool Coherent = Texture->GlobalCoherent && SupportsMemoryCoherency();
    air.CreateWrite(
        MakeTexture(*Texture), Texture->ResourceHandle, *Index, nullptr, nullptr, air.getInt(0), Value, Coherent
    );
    return (llvm::Value *)nullptr;
  }

  auto Buffer = GetBufferDescriptor(*Handle);
  if (!Buffer)
    return unsupported("dxil: no buffer descriptor for resource");

  Value *ByteOffset = *Index;
  if (Buffer->StructureStride != 0) {
    auto ElementOffset = Get(Call.getArgOperand(3));
    if (!ElementOffset)
      return ElementOffset.takeError();
    ByteOffset = ir.CreateMul(ByteOffset, ir.getInt32(Buffer->StructureStride));
    ByteOffset = ir.CreateAdd(ByteOffset, *ElementOffset);
  }
  auto Base = ir.CreateLShr(ByteOffset, ir.getInt32(2));
  bool Coherent = Buffer->GlobalCoherent && SupportsMemoryCoherency();

  for (unsigned i = 0; i < 4; i++) {
    if (!(*Mask & (1u << i)))
      continue;
    auto Component = Get(Call.getArgOperand(4 + i));
    if (!Component)
      return Component.takeError();
    Value *Word = BitcastIfNeeded(*Component, ir.getInt32Ty());
    auto *Ptr = ir.CreateGEP(ir.getInt32Ty(), Buffer->Pointer, {ir.CreateAdd(Base, ir.getInt32(i))});
    if (Coherent)
      air.CreateDeviceCoherentStore(Word, Ptr);
    else
      ir.CreateStore(Word, Ptr);
  }
  return (llvm::Value *)nullptr;
}

} // namespace dxmt::dxil
