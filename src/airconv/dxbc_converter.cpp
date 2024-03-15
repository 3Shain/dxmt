#include "dxbc_converter.hpp"

#include "llvm/ADT/APInt.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/FMF.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/MDBuilder.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "air_builder.hpp"
#include "air_constants.hpp"
#include "air_operation.hpp"
#include "air_type.hpp"
#include "dxbc_instruction.hpp"
#include "dxbc_utils.h"

using namespace llvm;
using namespace dxmt::air;

namespace dxmt {

class IndexableRegisters {

  IndexableRegisters(uint32_t size, IRBuilder<> &builder, const AirType &types)
      : size(size), builder(builder) {
    builder.CreateAlloca(types._int4,
                         ConstantInt::get(types._int, size));
  };

  Value *Load(uint32_t index) {
    // return Load(ConstantInt::get(types.tyInt32, index));
  }
  Value *Load(Value *index) {}

  void Store(Value *index, const std::function<Value *(Value *)> &reducer) {}

private:
  uint32_t size;
  IRBuilder<> &builder;
};

class SSARegisters {};

class DxbcConverter : public IDxbcConverter {

public:
  DxbcConverter(DxbcAnalyzer &analyzer, AirType &types,
                llvm::LLVMContext &context, llvm::Module &pModule)
      : analyzer(analyzer), types(types), context_(context), pModule_(pModule),
        pBuilder_(context), airBuilder(types, pBuilder_),
        airOp(types, airBuilder, context, pModule_) {}

private:
  DxbcAnalyzer &analyzer;
  air::AirType &types;
  llvm::LLVMContext &context_;
  llvm::Module &pModule_;
  llvm::IRBuilder<> pBuilder_;
  air::AirBuilder airBuilder;
  air::AirOp airOp;

  unsigned m_DxbcMajor;
  unsigned m_DxbcMinor;

  llvm::Function *function;
  llvm::BasicBlock *epilogue;

  __ShaderContext shaderContext;

public:
  llvm::MDNode *Pre(air::AirMetadata &metadata);

  llvm::MDNode *Post(air::AirMetadata &metadata, llvm::MDNode *input);

  void ConvertInstructions(D3D10ShaderBinary::CShaderCodeParser &Parser);

  /* internal helpers */
private:
  llvm::Value *Load(const dxbc::SrcOperand &operand, llvm::Type *expectedType) {

    assert(0 && "Unhandled branch in operand load");
  };

  PerformStore Store(const dxbc::DstOperand &operand) {
    if (operand.null()) {
      return [](auto _) {}; // do nothing
    }
    // auto instructionModifier = [this]() {

    // };
    if (operand.genericOutput()) {
    }

    if (operand.temp()) {
    }

    assert(0 && "Unhandled branch in operand store");
  };

  llvm::Value *LoadIndex(const dxbc::OperandIndex &index) {
    return index.match<llvm::Value *>(
        [&](uint32_t index) { return airBuilder.CreateConstant(index); },
        [&](uint32_t reg, uint8_t component) {
          return airBuilder.CreateConstant(reg); // TODO!
        },
        [&](uint32_t regFile, uint32_t reg, uint8_t component) {
          return airBuilder.CreateConstant(reg); // TODO!
        },
        [&](llvm::Value *val, uint32_t offset) {
          return pBuilder_.CreateAdd(val, airBuilder.CreateConstant(offset));
        });
  };
};

std::unique_ptr<IDxbcConverter> CreateConverterSM50(DxbcAnalyzer &analyzer,
                                                air::AirType &types,
                                                llvm::LLVMContext &context,
                                                llvm::Module &pModule) {
  return std::make_unique<DxbcConverter>(analyzer, types, context, pModule);
};

MDNode *DxbcConverter::Pre(AirMetadata &metadata) {

  std::vector<Type *> argTypes;
  std::vector<Metadata *> argMetadatas;
  std::vector<Type *> retFieldTypes;

  for (auto &input : analyzer.inputs) {
    argTypes.push_back(input.type);
    argMetadatas.push_back(input.indexedMetadata);
  }
  for (auto &outputField : analyzer.outputs) {
    assert(outputField.type);
    retFieldTypes.push_back(outputField.type);
  }
  // binding table
  auto [bindingTableType, bindingTableMetadat] =
      analyzer.GenerateBindingTable();

  if (bindingTableType) {
    // argTypes.push_back(bindingTableType);
    argTypes.push_back(
        PointerType::get(bindingTableType, air::EBufferAddrSpace::constant));
    // argTypes.push_back(types.tyConstantPtr);
    argMetadatas.push_back(metadata.createIndirectBufferBinding(
        argMetadatas.size(),
        pModule_.getDataLayout().getTypeAllocSize(bindingTableType), 30, 1,
        air::EBufferAccess::read, air::EBufferAddrSpace::constant,
        bindingTableMetadat,
        pModule_.getDataLayout().getABITypeAlignment(bindingTableType),
        "BindingTable", "bindingTable"));
  }

  // TODO: insert ResourceBindingTable

  FunctionType *funcType =
      FunctionType::get(analyzer.NoShaderOutput()
                            ? Type::getVoidTy(context_)
                            : StructType::get(context_, retFieldTypes, true),
                        argTypes, false);
  // funcType.

  function = Function::Create(funcType, llvm::GlobalValue::ExternalLinkage,
                              "shader_main", pModule_);

  // prologue
  auto prologue = BasicBlock::Create(context_, "prelogue", function);
  pBuilder_.SetInsertPoint(prologue);
  {
    if (analyzer.maxInputRegister) {
      shaderContext.tyInputRegs =
          ArrayType::get(types._int4, analyzer.maxInputRegister);
      shaderContext.inputRegs = pBuilder_.CreateAlloca(
          shaderContext.tyInputRegs, nullptr, "inputRegisters");
    }
    if (analyzer.maxOutputRegister) {
      shaderContext.tyOutputRegs =
          ArrayType::get(types._int4, analyzer.maxOutputRegister);
      shaderContext.outputRegs = pBuilder_.CreateAlloca(
          shaderContext.tyOutputRegs, nullptr, "outputRegisters");
    }
    if (analyzer.tempsCount) {
      shaderContext.tyTempRegs =
          ArrayType::get(types._int4, analyzer.tempsCount);
      shaderContext.tempRegs = pBuilder_.CreateAlloca(shaderContext.tyTempRegs,
                                                      nullptr, "tempRegisters");
    }

    for (auto [input, arg] : zip(analyzer.inputs, function->args())) {
      arg.setName(input.name);
      input.prologueHook(&arg, shaderContext, airBuilder);
    }
    if (bindingTableType) {
      shaderContext.bindingTable = function->getArg(function->arg_size() - 1);
      shaderContext.tyBindingTable = bindingTableType;
      function->addParamAttr(function->arg_size() - 1,
                             Attribute::get(context_, "air-buffer-no-alias"));
    }
  }

  // epilogue

  epilogue = BasicBlock::Create(context_, "epilogue", function);

  return MDTuple::get(context_, argMetadatas);
}

MDNode *DxbcConverter::Post(AirMetadata &metadata, MDNode *input) {
  pBuilder_.CreateBr(epilogue);
  pBuilder_.SetInsertPoint(epilogue);
  auto functionMD = ConstantAsMetadata::get(function);
  {
    if (analyzer.NoShaderOutput()) {
      pBuilder_.CreateRetVoid();
      return MDTuple::get(context_,
                          {functionMD, MDTuple::get(context_, {}), input});
    }

    std::vector<Metadata *> outputMetadatas;

    // 1. map output registers
    Value *retValue = UndefValue::get(function->getReturnType());
    for (auto &item : enumerate(analyzer.outputs)) {
      outputMetadatas.push_back(item.value().metadata);
      retValue = pBuilder_.CreateInsertValue(
          retValue, item.value().epilogueHook(shaderContext, airBuilder),
          item.index());
    }
    pBuilder_.CreateRet(retValue);

    auto outputMD = MDTuple::get(context_, outputMetadatas);

    return MDTuple::get(context_, {functionMD, outputMD, input});
  }
}

void DxbcConverter::ConvertInstructions(
    D3D10ShaderBinary::CShaderCodeParser &Parser) {

  // TODO: check fastmath

  if (Parser.EndOfShader()) {
    pBuilder_.CreateRetVoid();
    return;
  }

  for (;;) {
    if (Parser.EndOfShader())
      break;
    D3D10ShaderBinary::CInstruction Inst;
    Parser.ParseInstruction(&Inst);
    dxbc::Instruciton inst(Inst);
    // char temp[1024];
    // Inst.Disassemble(temp, 1024);
    // outs() << temp << "\n";

    // Fix up output register masks.
    // DXBC instruction conversion relies on the output mask(s) determining
    // what components need to be written.
    // Some output operand types have write mask that is 0 -- fix this.
    for (unsigned i = 0; i < std::min(Inst.m_NumOperands, (UINT)2); i++) {
      D3D10ShaderBinary::COperandBase &O = Inst.m_Operands[i];
      switch (O.m_Type) {
      case D3D10_SB_OPERAND_TYPE_OUTPUT_DEPTH:
      case D3D11_SB_OPERAND_TYPE_OUTPUT_DEPTH_GREATER_EQUAL:
      case D3D11_SB_OPERAND_TYPE_OUTPUT_DEPTH_LESS_EQUAL:
      case D3D11_SB_OPERAND_TYPE_OUTPUT_STENCIL_REF:
      case D3D10_SB_OPERAND_TYPE_OUTPUT_COVERAGE_MASK:
        DXASSERT_DXBC(O.m_WriteMask == 0);
        O.m_WriteMask = (D3D10_SB_OPERAND_4_COMPONENT_MASK_X);
        break;
      default:
        break;
      }
    }
    using namespace dxmt::dxbc;

    auto CreateComparison =
        [&](const std::function<Value *(Value *, Value *)> op) {
          auto src1 = Load(inst.src(1), nullptr);
          auto src2 = Load(inst.src(2), nullptr);
          auto store = Store(inst.dst(0));
          store(pBuilder_.CreateSExt(op(src1, src2), types._int4));
        };

    auto CreateUnary = [&](const std::function<Value *(Value *)> op) {
      auto src1 = Load(inst.src(1), nullptr); // should pass float?
      auto store = Store(inst.dst(0));
      store(op(src1));
    };

    switch (inst.opCode()) {
    case Op::DCL_GLOBAL_FLAGS:
      break;
    case Op::mov: {
      // TODO
      break;
    }
    case Op::movc: {
      // TODO
      break;
    }
    case Op::swapc: {
      // TODO
      break;
    }
    case Op::DP2:
    case Op::DP3:
    case Op::DP4: {
      // TODO
      break;
    }
      {/* comparison */
       case Op::eq : {CreateComparison([&](auto src1, auto src2) {
         return pBuilder_.CreateFCmp(CmpInst::FCMP_OEQ, src1, src2);
       });
      break;
    }
  case Op::ne: {
    CreateComparison([&](auto src1, auto src2) {
      return pBuilder_.CreateFCmp(CmpInst::FCMP_UNE, src1, src2);
    });
    break;
  }
  case Op::lt: {
    CreateComparison([&](auto src1, auto src2) {
      return pBuilder_.CreateFCmp(CmpInst::FCMP_OLT, src1, src2);
    });
    break;
  }
  case Op::ge: {
    CreateComparison([&](auto src1, auto src2) {
      return pBuilder_.CreateCmp(CmpInst::FCMP_OGE, src1, src2);
    });
    break;
  }

  case Op::ieq: {
    CreateComparison([&](auto src1, auto src2) {
      return pBuilder_.CreateCmp(CmpInst::ICMP_EQ, src1, src2);
    });
    break;
  }
  case Op::ine: {
    CreateComparison([&](auto src1, auto src2) {
      return pBuilder_.CreateCmp(CmpInst::ICMP_NE, src1, src2);
    });
    break;
  }
  case Op::ige: {
    CreateComparison([&](auto src1, auto src2) {
      return pBuilder_.CreateCmp(CmpInst::ICMP_SGE, src1, src2);
    });
    break;
  }
  case Op::ilt: {
    CreateComparison([&](auto src1, auto src2) {
      return pBuilder_.CreateCmp(CmpInst::ICMP_SLT, src1, src2);
    });
    break;
  }
  case Op::ult: {
    CreateComparison([&](auto src1, auto src2) {
      return pBuilder_.CreateCmp(CmpInst::ICMP_ULT, src1, src2);
    });
    break;
  }
  case Op::uge: {
    CreateComparison([&](auto src1, auto src2) {
      return pBuilder_.CreateCmp(CmpInst::ICMP_UGE, src1, src2);
    });
    break;
  }
  }
  {
    /* float arith */
  case Op::EXP: {
    CreateUnary([&](auto src) {
      return airOp.CreateFloatUnaryOp(EFloatUnaryOp::exp2, src);
    });
    break;
  }
  case Op::LOG: {
    CreateUnary([&](auto src) {
      return airOp.CreateFloatUnaryOp(EFloatUnaryOp::log2, src);
    });
    break;
  }
  case Op::FRC: {
    CreateUnary([&](auto src) {
      return airOp.CreateFloatUnaryOp(EFloatUnaryOp::fract, src);
    });
    break;
  }
  case Op::SQRT: {
    CreateUnary([&](auto src) {
      return airOp.CreateFloatUnaryOp(EFloatUnaryOp::sqrt, src);
    });
    break;
  }
  case Op::ROUND_NE: {
    CreateUnary([&](auto src) {
      return airOp.CreateFloatUnaryOp(EFloatUnaryOp::rint, src);
    });
    break;
  }
  case Op::ROUND_NI: {
    CreateUnary([&](auto src) {
      return airOp.CreateFloatUnaryOp(EFloatUnaryOp::floor, src);
    });
    break;
  }
  case Op::ROUND_PI: {
    CreateUnary([&](auto src) {
      return airOp.CreateFloatUnaryOp(EFloatUnaryOp::ceil, src);
    });
    break;
  }
  case Op::ROUND_Z: {
    CreateUnary([&](auto src) {
      return airOp.CreateFloatUnaryOp(EFloatUnaryOp::round, src);
    });
    break;
  }
  case Op::RSQ: {
    CreateUnary([&](auto src) {
      return airOp.CreateFloatUnaryOp(EFloatUnaryOp::rsqrt, src);
    });
    break;
  }
  case Op::RCP: {
    CreateUnary([&](auto src) {
      return airOp.CreateFloatUnaryOp(EFloatUnaryOp::_rcp, src);
    });
    break;
  }
  case Op::NOP:
    break; // do nothing

  case Op::ADD: {
    break;
  }
  case Op::DIV: {
    break;
  }
  case Op::MIN: {
    break;
  }
  case Op::MAX: {
    break;
  }
  case Op::MUL: {
    break;
  }
    // case Op::SINCOS: {
    //   break;
    // }
  }

default:
  DXASSERT(false, "Unhandled opecode")
  break;
}
} // namespace dxmt
}

} // namespace dxmt