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
#include "llvm/IR/MDBuilder.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
// #include "llvm/Analysis/LoopAnalysisManager.h"
// #include "llvm/Analysis/CGSCCPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/IR/Verifier.h"
#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "Metal/MTLHeaderBridge.hpp"
#include "dxbc_converter.hpp"
#include "DXBCParser/d3d12tokenizedprogramformat.hpp"
#include "air_builder.hpp"
#include "air_constants.hpp"
#include "air_operation.hpp"
#include "airconv_public.h"
#include "DXBCParser/BlobContainer.h"
#include "DXBCParser/ShaderBinary.h"
#include "DXBCParser/DXBCUtils.h"
#include "dxbc_utils.h"
#include "metallib.h"
#include "sha256.hpp"

using namespace llvm;
using namespace dxmt::air;

namespace dxmt {

DxbcConverter::DxbcConverter(DxbcAnalyzer &analyzer, AirType &types,
                             llvm::LLVMContext &context, llvm::Module &pModule)
    : analyzer(analyzer), types(types), context_(context), pModule_(pModule),
      pBuilder_(context), airBuilder(types, pBuilder_),
      airOp(types, airBuilder, context, pModule_) {}

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
          ArrayType::get(types.tyInt32V4, analyzer.maxInputRegister);
      shaderContext.inputRegs = pBuilder_.CreateAlloca(
          shaderContext.tyInputRegs, nullptr, "inputRegisters");
    }
    if (analyzer.maxOutputRegister) {
      shaderContext.tyOutputRegs =
          ArrayType::get(types.tyInt32V4, analyzer.maxOutputRegister);
      shaderContext.outputRegs = pBuilder_.CreateAlloca(
          shaderContext.tyOutputRegs, nullptr, "outputRegisters");
    }
    if (analyzer.tempsCount) {
      shaderContext.tyTempRegs =
          ArrayType::get(types.tyInt32V4, analyzer.tempsCount);
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

    switch (Inst.OpCode()) {
    case D3D10_SB_OPCODE_DCL_GLOBAL_FLAGS:
      break;
    case D3D10_SB_OPCODE_MOV: {
      auto mask = Inst.m_Operands[0].m_WriteMask >> 4;

      auto input = LoadOperand(Inst, 1);
      StoreOperand(input, Inst, 0, mask);
      break;
    }
    case D3D10_SB_OPCODE_ADD: {
      // TODO: BINARY TEMPALTE
      const unsigned DstIdx = 0;
      const unsigned SrcIdx1 = 1;
      const unsigned SrcIdx2 = 2;
      auto mask = Inst.m_Operands[DstIdx].m_WriteMask >> 4;

      auto A = LoadOperand(Inst, SrcIdx1);
      auto B = LoadOperand(Inst, SrcIdx2);

      auto R =
          pBuilder_.CreateFAdd(pBuilder_.CreateBitCast(A, types.tyFloatV4),
                               pBuilder_.CreateBitCast(B, types.tyFloatV4));

      StoreOperand(pBuilder_.CreateBitCast(R, types.tyInt32V4), Inst, DstIdx,
                   mask);

      break;
    }
    case D3D10_SB_OPCODE_DP2:
    case D3D10_SB_OPCODE_DP3:
    case D3D10_SB_OPCODE_DP4: {
      auto mask = Inst.m_Operands[0].m_WriteMask >> 4;

      auto A = LoadOperand(Inst, 1);
      auto B = LoadOperand(Inst, 2);

      auto opDP = airOp.GetDotProduct();
      auto R = pBuilder_.CreateCall(
          opDP, {pBuilder_.CreateBitCast(A, types.tyFloatV4),
                 pBuilder_.CreateBitCast(B, types.tyFloatV4)});

      StoreOperand(pBuilder_.CreateBitCast(pBuilder_.CreateVectorSplat(4, R),
                                           types.tyInt32V4),
                   Inst, 0, mask);
      break;
    }
    default:
      // DXASSERT(false, "Unhandled opecode")
      break;
    }
  }
}

llvm::Value *
DxbcConverter::LoadOperand(const D3D10ShaderBinary::CInstruction &Inst,
                           const unsigned OpIdx) {
  const D3D10ShaderBinary::COperandBase &O = Inst.m_Operands[OpIdx];
  switch (O.m_Type) {
  case D3D10_SB_OPERAND_TYPE_IMMEDIATE32: {

    DXASSERT_DXBC(O.m_Modifier == D3D10_SB_OPERAND_MODIFIER_NONE);
    bool bVec4 = O.m_NumComponents == D3D10_SB_OPERAND_4_COMPONENT;
    std::array<uint32_t, 4> constantVal = {
        O.m_Value[0], O.m_Value[bVec4 ? 1 : 0], O.m_Value[bVec4 ? 2 : 0],
        O.m_Value[bVec4 ? 3 : 0]}; // wtf?
    return airBuilder.CreateConstantVector(constantVal);
    break;
  }
  case D3D10_SB_OPERAND_TYPE_INPUT: {
    unsigned Register;     // Starting index of the register range.
    Value *pRowIndexValue; // Row index expression.
    switch (O.m_IndexDimension) {
    case D3D10_SB_OPERAND_INDEX_1D:
      Register = O.m_Index[0].m_RegIndex;
      pRowIndexValue = LoadOperandIndex(O.m_Index[0], O.m_IndexType[0]);
      break;
    default:
      DXASSERT(false, "there should no other index dimensions");
    }
    return airBuilder.CreateRegisterLoad(
        shaderContext.inputRegs, shaderContext.tyInputRegs, pRowIndexValue);
    break;
  }

  case D3D10_SB_OPERAND_TYPE_TEMP: {
    unsigned Register;     // Starting index of the register range.
    Value *pRowIndexValue; // Row index expression.
    switch (O.m_IndexDimension) {
    case D3D10_SB_OPERAND_INDEX_1D:
      Register = O.m_Index[0].m_RegIndex;
      pRowIndexValue = LoadOperandIndex(O.m_Index[0], O.m_IndexType[0]);
      break;
    default:
      DXASSERT(false, "there should no other index dimensions");
    }
    return airBuilder.CreateRegisterLoad(
        shaderContext.tempRegs, shaderContext.tyTempRegs, pRowIndexValue);
    break;
  }
  case D3D10_SB_OPERAND_TYPE_CONSTANT_BUFFER: {
    auto cbIdx = pBuilder_.CreateInBoundsGEP(
        shaderContext.tyBindingTable, shaderContext.bindingTable,
        {airBuilder.CreateConstant((uint32_t)0),
         airBuilder.CreateConstant(
             (uint32_t)0) /** TODO: get from binding table*/});
    auto cbPtr = pBuilder_.CreateLoad(types.tyInt32V4->getPointerTo(2), cbIdx);
    auto arrayIdx = pBuilder_.CreateInBoundsGEP(
        types.tyInt32V4, cbPtr,
        {LoadOperandIndex(O.m_Index[1], O.m_IndexType[1])});
    auto element = pBuilder_.CreateLoad(types.tyInt32V4, arrayIdx);
    return element;
    break;
  }
  default: {
    assert(0 && "to be handled");
    break;
  }
  }
}

llvm::Value *DxbcConverter::LoadOperandIndex(
    const D3D10ShaderBinary::COperandIndex &OpIndex,
    const D3D10_SB_OPERAND_INDEX_REPRESENTATION IndexType) {
  Value *pValue = nullptr;

  switch (IndexType) {
  case D3D10_SB_OPERAND_INDEX_IMMEDIATE32:
    DXASSERT_DXBC(OpIndex.m_RelRegType == D3D10_SB_OPERAND_TYPE_IMMEDIATE32);
    pValue = airBuilder.CreateConstant(OpIndex.m_RegIndex);
    break;

  case D3D10_SB_OPERAND_INDEX_IMMEDIATE64:
    DXASSERT_DXBC(false);
    break;

  case D3D10_SB_OPERAND_INDEX_RELATIVE:
  case D3D10_SB_OPERAND_INDEX_IMMEDIATE32_PLUS_RELATIVE:
  case D3D10_SB_OPERAND_INDEX_IMMEDIATE64_PLUS_RELATIVE:
    DXASSERT_DXBC(false);
    break;

  default:
    DXASSERT_DXBC(false);
    break;
  }

  return pValue;
}

void DxbcConverter::StoreOperand(llvm::Value *value,
                                 const D3D10ShaderBinary::CInstruction &Inst,
                                 const unsigned OpIdx, uint32_t mask) {

  const D3D10ShaderBinary::COperandBase &O = Inst.m_Operands[OpIdx];
  switch (O.m_Type) {
  case D3D10_SB_OPERAND_TYPE_OUTPUT: {
    unsigned Reg = O.m_Index[0].m_RegIndex;
    // Row index expression.
    Value *pRowIndexValue = LoadOperandIndex(O.m_Index[0], O.m_IndexType[0]);
    airBuilder.CreateRegisterStore(shaderContext.outputRegs,
                                   shaderContext.tyOutputRegs, pRowIndexValue,
                                   value, mask, {0, 1, 2, 3});
    break;
  }
  case D3D10_SB_OPERAND_TYPE_TEMP: {
    unsigned Reg = O.m_Index[0].m_RegIndex;
    // Row index expression.
    Value *pRowIndexValue = LoadOperandIndex(O.m_Index[0], O.m_IndexType[0]);
    airBuilder.CreateRegisterStore(shaderContext.tempRegs,
                                   shaderContext.tyTempRegs, pRowIndexValue,
                                   value, mask, {0, 1, 2, 3});
    break;
  }
  case D3D10_SB_OPERAND_TYPE_NULL:
    break;
  default: {
    outs() << (O.m_Type);
    assert(0 && "to be handled");
    break;
  }
  }
}

Value *
DxbcConverter::ApplyOperandModifiers(Value *pValue,
                                     const D3D10ShaderBinary::COperandBase &O) {
  bool bAbsModifier = (O.m_Modifier & D3D10_SB_OPERAND_MODIFIER_ABS) != 0;
  bool bNegModifier = (O.m_Modifier & D3D10_SB_OPERAND_MODIFIER_NEG) != 0;

  if (bAbsModifier) {
    DXASSERT_DXBC(pValue->getType()->isFloatingPointTy());
    // Function *F = m_pOP->GetOpFunc(OP::OpCode::FAbs, pValue->getType());
    // Value *Args[2];
    // Args[0] = m_pOP->GetU32Const((unsigned)OP::OpCode::FAbs);
    // Args[1] = pValue;
    // pValue = m_pBuilder->CreateCall(F, Args);
    // pBuilder_.CreateCall()
    // pModule_.getOrInsertFunction()
  }

  if (bNegModifier) {
    if (pValue->getType()->isFloatingPointTy()) {
      pValue = pBuilder_.CreateFNeg(pValue);
    } else {
      DXASSERT_DXBC(pValue->getType()->isIntegerTy());
      pValue = pBuilder_.CreateNeg(pValue);
    }
  }

  return pValue;
}

void DxbcConverter::Optimize(llvm::OptimizationLevel level) {
  // Create the analysis managers.
  // These must be declared in this order so that they are destroyed in the
  // correct order due to inter-analysis-manager references.
  LoopAnalysisManager LAM;
  FunctionAnalysisManager FAM;
  CGSCCAnalysisManager CGAM;
  ModuleAnalysisManager MAM;

  // Create the new pass manager builder.
  // Take a look at the PassBuilder constructor parameters for more
  // customization, e.g. specifying a TargetMachine or various debugging
  // options.
  PassBuilder PB;

  // Register all the basic analyses with the managers.
  PB.registerModuleAnalyses(MAM);
  PB.registerCGSCCAnalyses(CGAM);
  PB.registerFunctionAnalyses(FAM);
  PB.registerLoopAnalyses(LAM);
  PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

  ModulePassManager MPM = PB.buildPerModuleDefaultPipeline(level);

  FunctionPassManager FPM;
  FPM.addPass(VerifierPass());

  MPM.addPass(createModuleToFunctionPassAdaptor(std::move(FPM)));

  // Optimize the IR!
  MPM.run(pModule_, MAM);
}

void DxbcConverter::SerializeAIR(raw_ostream &OS) {
  std::string funciton_name(function->getName());
  MTLBHeader header;
  header.Magic = MTLB_Magic;

  SmallVector<char, 0> bitcode;
  SmallVector<char, 0> public_metadata;
  SmallVector<char, 0> private_metadata;
  SmallVector<char, 0> function_def;

  raw_svector_ostream bitcode_stream(bitcode);
  ModuleHash module_hash;
  WriteBitcodeToFile(pModule_, bitcode_stream, false, nullptr, true,
                     &module_hash);

  raw_svector_ostream public_metadata_stream(public_metadata);

  raw_svector_ostream private_metadata_stream(private_metadata);

  raw_svector_ostream function_def_stream(function_def);
  function_def_stream.write("NAME", 4);
  uint16_t size = funciton_name.size() + 1;
  function_def_stream.write((const char *)&size, 2);
  function_def_stream.write(funciton_name.c_str(), funciton_name.size() + 1);

  MTLB_TYPE_TAG type_tag{
      .type = analyzer.IsVS()   ? MTLB_FunctionType::MTLB_VertexFunction
              : analyzer.IsPS() ? MTLB_FunctionType::MTLB_FragmentFunction
              : analyzer.IsCS() ? MTLB_KernelFunction
                                : (assert(0), MTLB_KernelFunction)};
  function_def_stream.write((const char *)&type_tag, sizeof(type_tag));

  MTLB_HASH_TAG hash_tag{.hash = {0}};

  hash_tag.hash =
      compute_sha256_hash((const uint8_t *)bitcode.data(), bitcode.size());

  function_def_stream.write((const char *)&hash_tag, sizeof(hash_tag));

  MTLB_MDSZ_TAG size_tag{.bitcodeSize = bitcode.size()};

  function_def_stream.write((const char *)&size_tag, sizeof(size_tag));

  MTLB_OFFT_TAG offset_tag{
      .PublicMetadataOffset = 0,
      .PrivateMetadataOffset = 0,
      .BitcodeOffset = 0,
  };
  function_def_stream.write((const char *)&offset_tag, sizeof(offset_tag));

  MTLB_VERS_TAG version_tag{
      .airVersionMajor = 2,
      .airVersionMinor = 6,
      .languageVersionMajor = 3,
      .languageVersionMinor = 1,
  };
  function_def_stream.write((const char *)&version_tag, sizeof(version_tag));

  function_def_stream.write("ENDT", 4);

  // public metadata
  uint32_t public_metadata_size = 4;

  if (analyzer.IsVS()) {
    public_metadata_size += (4 + 11 + 4 + 5);
  }

  public_metadata_stream.write((const char *)&public_metadata_size, 4);

  if (analyzer.IsVS()) {
    // FIXME: properly handle it via vertex attribute information

    public_metadata_stream.write("VATT", 4);
    uint8_t w[] = {
        0x09, 0x00,                   // tag len
        0x01, 0x00,                   // type
        0x72, 0x65, 0x67, 0x30, 0x00, // name str
        0x00, 0x80                    // wtf?
    };
    public_metadata_stream.write((char *)w, sizeof(w));

    public_metadata_stream.write("VATY", 4);
    char e[] = {
        3, 0, // tag len
        1, 0, // attr type array len?
        6     // type[0]
    };
    public_metadata_stream.write(e, sizeof(e));
  }

  public_metadata_stream.write("ENDT", 4);

  // private metadata
  uint32_t private_metadata_size = 4;
  private_metadata_stream.write((const char *)&private_metadata_size, 4);
  private_metadata_stream.write("ENDT", 4);

  header.FileSize = sizeof(MTLBHeader) + sizeof(uint32_t) + sizeof(uint32_t) +
                    function_def.size() + sizeof(MTLB_EndTag) // extended header
                    + public_metadata.size() + private_metadata.size() +
                    bitcode.size();
  header.FunctionListOffset = sizeof(MTLBHeader);
  header.FunctionListSize = function_def.size() + 4;
  header.PublicMetadataOffset =
      header.FunctionListOffset + header.FunctionListSize +
      sizeof(uint32_t) // extra room for function count
      + sizeof(MTLB_EndTag);
  header.PublicMetadataSize = public_metadata.size();
  header.PrivateMetadataOffset =
      header.PublicMetadataOffset + header.PublicMetadataSize;
  header.PrivateMetadataSize = private_metadata.size();
  header.BitcodeOffset =
      header.PrivateMetadataOffset + header.PrivateMetadataSize;
  header.BitcodeSize = bitcode.size();

  header.Type = MTLBType_Executable; // executable
  header.Platform = MTLBPlatform_macOS;
  header.VersionMajor = 2;
  header.VersionMinor = 7;
  header.OS = MTLBOS_macOS;
  header.OSVersionMajor = 14;
  header.OSVersionMinor = 4;

  // write to stream
  OS.write((const char *)&header, sizeof(MTLBHeader));

  uint32_t fn_count = 1;
  OS.write((const char *)&fn_count, 4);
  uint32_t size_fn = function_def.size() + 4;
  OS.write((const char *)&size_fn, 4);
  OS.write((const char *)function_def.data(), function_def.size());
  OS.write("ENDT", 4); // extend header
  OS.write((const char *)public_metadata.data(), public_metadata.size());
  OS.write((const char *)private_metadata.data(), private_metadata.size());
  OS.write((const char *)bitcode.data(), bitcode.size());
}

} // namespace dxmt