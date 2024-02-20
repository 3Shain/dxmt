
#include "dxbc_converter.h"
#include "airconv_public.h"
#include "DXBCParser/BlobContainer.h"
#include "DXBCParser/ShaderBinary.h"
#include "DXBCParser/DXBCUtils.h"
#include "dxbc_utils.h"
#include "metallib.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/MDBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"

namespace dxmt {

DxbcConverter::DxbcConverter() {}

DxbcConverter::~DxbcConverter() {}

void DxbcConverter::AnalyzeShader(
    D3D10ShaderBinary::CShaderCodeParser &Parser) {

  D3D10_SB_TOKENIZED_PROGRAM_TYPE ShaderType = Parser.ShaderType();
  m_DxbcMajor = Parser.ShaderMajorVersion();
  m_DxbcMinor = Parser.ShaderMinorVersion();

  // Collect:
  //   1. Declarations
  //   2. Labels
  // Declare:
  //   1. Global symbols for resources/samplers.
  //   2. Their types.
  D3D10ShaderBinary::CInstruction Inst;
  while (!Parser.EndOfShader()) {

    Parser.ParseInstruction(&Inst);

    switch (Inst.OpCode()) {
    case D3D10_SB_OPCODE_DCL_CONSTANT_BUFFER: {
      DXASSERT(false, "Unhandled declaration (tbd)");
      break;
    }
    case D3D10_SB_OPCODE_DCL_SAMPLER: {
      DXASSERT(false, "Unhandled declaration (tbd)");
      // pModule_.gettyp
      // StructType::getTypeByName(LLVMContext &C, StringRef Name)
      break;
    }

    case D3D10_SB_OPCODE_DCL_RESOURCE:
    case D3D11_SB_OPCODE_DCL_RESOURCE_RAW:
    case D3D11_SB_OPCODE_DCL_RESOURCE_STRUCTURED: {
      DXASSERT(false, "Unhandled declaration (tbd)");
      break;
    }

    case D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_TYPED:
    case D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_RAW:
    case D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED: {
      DXASSERT(false, "Unhandled declaration (tbd)");
      break;
    }

    case D3D10_SB_OPCODE_DCL_INDEX_RANGE: {
      DXASSERT(false, "Unhandled declaration (tbd)");
      break;
    }

    case D3D10_SB_OPCODE_DCL_GS_INPUT_PRIMITIVE: {
      DXASSERT(false, "Unhandled declaration (tbd)");
      break;
    }

    case D3D10_SB_OPCODE_DCL_GS_OUTPUT_PRIMITIVE_TOPOLOGY: {
      DXASSERT(false, "Unhandled declaration (tbd)");
      break;
    }

    case D3D10_SB_OPCODE_DCL_MAX_OUTPUT_VERTEX_COUNT: {
      DXASSERT(false, "Unhandled declaration (tbd)");
      break;
    }

    case D3D10_SB_OPCODE_DCL_INPUT: {
      DXASSERT(false, "Unhandled declaration (tbd)");
      break;
    }

    case D3D10_SB_OPCODE_DCL_INPUT_SGV: {
      DXASSERT(false, "Unhandled declaration (tbd)");
      break;
    }

    case D3D10_SB_OPCODE_DCL_INPUT_SIV: {
      DXASSERT(false, "Unhandled declaration (tbd)");
      break;
    }

    case D3D10_SB_OPCODE_DCL_INPUT_PS: {
      DXASSERT(false, "Unhandled declaration (tbd)");
      break;
    }

    case D3D10_SB_OPCODE_DCL_INPUT_PS_SGV: {
      DXASSERT(false, "Unhandled declaration (tbd)");
      break;
    }

    case D3D10_SB_OPCODE_DCL_INPUT_PS_SIV: {
      DXASSERT(false, "Unhandled declaration (tbd)");
      break;
    }

    case D3D10_SB_OPCODE_DCL_OUTPUT: {
      DXASSERT(false, "Unhandled declaration (tbd)");
      break;
    }

    case D3D10_SB_OPCODE_DCL_OUTPUT_SGV: {
      DXASSERT(false, "Unhandled declaration (tbd)");
      break;
    }
    case D3D10_SB_OPCODE_DCL_OUTPUT_SIV: {
      DXASSERT(false, "Unhandled declaration (tbd)");
      break;
    }

    case D3D10_SB_OPCODE_DCL_TEMPS: {
      DXASSERT(false, "Unhandled declaration (tbd)");
      break;
    }

    case D3D10_SB_OPCODE_DCL_INDEXABLE_TEMP: {
      DXASSERT(false, "Unhandled declaration (tbd)");
      break;
    }

    case D3D10_SB_OPCODE_DCL_GLOBAL_FLAGS: {
      //
      break;
    }

    case D3D11_SB_OPCODE_DCL_STREAM: {
      DXASSERT(false, "Unhandled declaration (tbd)");
      break;
    }

    case D3D11_SB_OPCODE_HS_DECLS:
      break; // on purpose

    case D3D11_SB_OPCODE_DCL_INPUT_CONTROL_POINT_COUNT: {
      DXASSERT(false, "Unhandled declaration (tbd)");
      break;
    }

    case D3D11_SB_OPCODE_DCL_OUTPUT_CONTROL_POINT_COUNT: {
      DXASSERT(false, "Unhandled declaration (tbd)");
      break;
    }

    case D3D11_SB_OPCODE_DCL_TESS_DOMAIN: {
      DXASSERT(false, "Unhandled declaration (tbd)");
      break;
    }

    case D3D11_SB_OPCODE_DCL_TESS_PARTITIONING: {
      DXASSERT(false, "Unhandled declaration (tbd)");
      break;
    }

    case D3D11_SB_OPCODE_DCL_TESS_OUTPUT_PRIMITIVE: {
      DXASSERT(false, "Unhandled declaration (tbd)");
      break;
    }

    case D3D11_SB_OPCODE_DCL_HS_MAX_TESSFACTOR: {
      DXASSERT(false, "Unhandled declaration (tbd)");
      break;
    }

    case D3D11_SB_OPCODE_HS_CONTROL_POINT_PHASE: {
      DXASSERT(false, "Unhandled declaration (tbd)");
      break;
    }

    case D3D11_SB_OPCODE_HS_FORK_PHASE:
    case D3D11_SB_OPCODE_HS_JOIN_PHASE: {
      DXASSERT(false, "Unhandled declaration (tbd)");
      break;
    }

    case D3D11_SB_OPCODE_DCL_HS_FORK_PHASE_INSTANCE_COUNT: {
      DXASSERT(false, "Unhandled declaration (tbd)");
      break;
    }

    case D3D11_SB_OPCODE_DCL_HS_JOIN_PHASE_INSTANCE_COUNT: {
      DXASSERT(false, "Unhandled declaration (tbd)");
      break;
    }

    case D3D11_SB_OPCODE_DCL_THREAD_GROUP: {
      DXASSERT(false, "Unhandled declaration (tbd)");
      break;
    }

    case D3D11_SB_OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_RAW:
    case D3D11_SB_OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_STRUCTURED: {
      DXASSERT(false, "Unhandled declaration (tbd)");
      break;
    }

    case D3D11_SB_OPCODE_DCL_GS_INSTANCE_COUNT: {
      DXASSERT(false, "Unhandled declaration (tbd)");
      break;
    }

    case D3D10_SB_OPCODE_CUSTOMDATA:
      break; // on purpose

    case D3D11_SB_OPCODE_DCL_FUNCTION_BODY: {
      DXASSERT(false, "Unhandled declaration (tbd)");
      break;
    }

    case D3D11_SB_OPCODE_DCL_FUNCTION_TABLE: {
      DXASSERT(false, "Unhandled declaration (tbd)");
      break;
    }

    case D3D11_SB_OPCODE_DCL_INTERFACE: {
      DXASSERT(false, "Unhandled declaration (tbd)");
      break;
    }

    case D3D10_SB_OPCODE_LABEL: {
      DXASSERT(false, "Unhandled declaration (tbd)");
      break;
    }

    default:
      break;
    };
  }
}

void DxbcConverter::Convert(LPCVOID dxbc, UINT dxbcSize, LPVOID *ppAIR,
                            UINT *pAIRSize) {

  // module metadata
  pModule_ = std::make_unique<Module>("generated.metal", context_);
  pModule_->setSourceFileName("airconv_generated.metal");
  pModule_->setTargetTriple("air64-apple-macosx13.0.0");
  pModule_->setDataLayout(
      "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:"
      "64:64-v16:16:16-v24:32:32-v32:32:32-v48:64:64-v64:64:64-v96:128:128-"
      "v128:128:128-v192:256:256-v256:256:256-v512:512:512-v1024:1024:1024-n8:"
      "16:32");

  CDXBCParser DXBCParser;
  DXASSERT(DXBCParser.ReadDXBC(dxbc, dxbcSize) == S_OK,
           "otherwise invalid dxbc blob");

  UINT codeBlobIdx = DXBCParser.FindNextMatchingBlob(DXBC_GenericShaderEx);
  if (codeBlobIdx == DXBC_BLOB_NOT_FOUND) {
    codeBlobIdx = DXBCParser.FindNextMatchingBlob(DXBC_GenericShader);
  }
  DXASSERT(codeBlobIdx != DXBC_BLOB_NOT_FOUND, "otherwise invalid dxbc blob");
  LPCVOID codeBlob = DXBCParser.GetBlob(codeBlobIdx);

  D3D10ShaderBinary::CShaderCodeParser CodeParser;

  CShaderToken *ShaderCode = (CShaderToken *)(BYTE *)codeBlob;
  // 1. Collect information about the shader.
  CodeParser.SetShader(ShaderCode);
  AnalyzeShader(CodeParser);

  // 2. Parse input signature(s).
  //   DXBCGetInputSignature(const void *pBlobContainer, CSignatureParser
  //   *pParserToUse)

  // 3. Parse output signature(s).

  // 4. Transform DXBC to AIR.
  CodeParser.SetShader(ShaderCode);
  ConvertInstructions(CodeParser);

  // 5. Emit medatada.
  EmitAIRMetadata();

  // 6. Cleanup/Optimize AIR.
  Optimize();

  SmallVector<char, 0> vec;

  raw_svector_ostream OS(vec);

  // Serialize AIR
  SerializeAIR(OS);
  auto ptr = malloc(vec.size_in_bytes());
  memcpy(ptr, vec.data(), vec.size_in_bytes());

  *ppAIR = ptr;
  *pAIRSize = vec.size_in_bytes();

  pModule_.reset();
}

void DxbcConverter::ConvertInstructions(
    D3D10ShaderBinary::CShaderCodeParser &Parser) {

  StructType::get(context_, {FixedVectorType::get(int32Ty, 4)}, false);

  FunctionType *entryFunctionTy = FunctionType::get(
      voidTy, {}, false); // TODO: need proper param and return type definition

  Function *pFunction =
      Function::Create(entryFunctionTy, llvm::GlobalValue::ExternalLinkage,
                       "__main__", pModule_.get());

  auto pBasicBlock = BasicBlock::Create(context_, "entry", pFunction);
  pBuilder_ = std::make_unique<IRBuilder<>>(pBasicBlock);

  // TODO: check fastmath

  if (Parser.EndOfShader()) {
    pBuilder_->CreateRetVoid();
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
      CMask WriteMask = CMask::FromDXBC(Inst.m_Operands[0].m_WriteMask);
      break;
    }
    default:
      DXASSERT(false, "Unhandled opecode")
      break;
    }
  }
}

void DxbcConverter::LoadOperand(int src,
                                const D3D10ShaderBinary::CInstruction &Inst,
                                const unsigned int OpIdx, const CMask &Mask,
                                const int ValueType) {
  const D3D10ShaderBinary::COperandBase &O = Inst.m_Operands[OpIdx];
  switch (O.m_Type) {
  case D3D10_SB_OPERAND_TYPE_IMMEDIATE32: {
    DXASSERT_DXBC(O.m_Modifier == D3D10_SB_OPERAND_MODIFIER_NONE);
    break;
  }
  case D3D10_SB_OPERAND_TYPE_IMMEDIATE64: {
    DXASSERT(false, "64bit value not supported yet");
    break;
  }
  default:
    DXASSERT(false, "unhandled load operand type");
    break;
  }
}

Value *
DxbcConverter::ApplyOperandModifiers(Value *pValue,
                                     const D3D10ShaderBinary::COperandBase &O) {
  bool bAbsModifier = (O.m_Modifier & D3D10_SB_OPERAND_MODIFIER_ABS) != 0;
  bool bNegModifier = (O.m_Modifier & D3D10_SB_OPERAND_MODIFIER_NEG) != 0;

  if (pValue->getType()->isVectorTy()) {
    if (bAbsModifier) {
      DXASSERT_DXBC(pValue->getType()->getScalarType()->isFloatingPointTy());
    }

    if (bNegModifier) {
    }
  } else {

    if (bAbsModifier) {
      DXASSERT_DXBC(pValue->getType()->isFloatingPointTy());
      // Function *F = m_pOP->GetOpFunc(OP::OpCode::FAbs, pValue->getType());
      // Value *Args[2];
      // Args[0] = m_pOP->GetU32Const((unsigned)OP::OpCode::FAbs);
      // Args[1] = pValue;
      // pValue = m_pBuilder->CreateCall(F, Args);
    }

    if (bNegModifier) {
      if (pValue->getType()->isFloatingPointTy()) {
        pValue = pBuilder_->CreateFNeg(pValue);
      } else {
        DXASSERT_DXBC(pValue->getType()->isIntegerTy());
        pValue = pBuilder_->CreateNeg(pValue);
      }
    }
  }

  return pValue;
}

void DxbcConverter::EmitAIRMetadata() {
  MDBuilder mdbuilder(context_);
  auto uintMetadata = [&](uint32_t value) {
    return mdbuilder.createConstant(
        ConstantInt::get(Type::getInt32Ty(context_), value));
  };

  auto stringMetadata = [&](StringRef str) {
    return MDString::get(context_, str);
  };
  auto llvmIdent = pModule_->getOrInsertNamedMetadata("llvm.ident");
  llvmIdent->addOperand(
      MDNode::get(context_, {MDString::get(context_, "airconv")}));

  auto airVersion = pModule_->getOrInsertNamedMetadata("air.version");
  airVersion->addOperand(MDNode::get(context_, {
                                                   uintMetadata(2),
                                                   uintMetadata(5),
                                                   uintMetadata(0),
                                               }));
  auto airLangVersion =
      pModule_->getOrInsertNamedMetadata("air.language_version");
  airLangVersion->addOperand(MDNode::get(context_, {
                                                       stringMetadata("Metal"),
                                                       uintMetadata(3),
                                                       uintMetadata(0),
                                                       uintMetadata(0),
                                                   }));
}

void DxbcConverter::Optimize() {
  // TODO
}

void DxbcConverter::SerializeAIR(raw_ostream &OS) {
  std::string funciton_name("generated_entry");
  MTLBHeader header;
  header.Magic = MTLB_Magic;

  SmallVector<char, 0> bitcode;
  SmallVector<char, 0> public_metadata;
  SmallVector<char, 0> private_metadata;
  SmallVector<char, 0> function_def;

  raw_svector_ostream bitcode_stream(bitcode);
  ModuleHash module_hash;
  WriteBitcodeToFile(*pModule_.get(), bitcode_stream, false, nullptr, true,
                     &module_hash);

  raw_svector_ostream public_metadata_stream(public_metadata);

  raw_svector_ostream private_metadata_stream(private_metadata);

  raw_svector_ostream function_def_stream(function_def);
  function_def_stream.write("NAME", 4);
  uint16_t size = funciton_name.size() + 1;
  function_def_stream.write((const char *)&size, 2);
  function_def_stream.write(funciton_name.c_str(), funciton_name.size() + 1);

  MTLB_TYPE_TAG type_tag{
      .type = 2 // TODO: function type
  };
  function_def_stream.write((const char *)&type_tag, sizeof(type_tag));

  MTLB_HASH_TAG hash_tag{.hash = {0}};

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
      .airVersionMinor = 5,
      .languageVersionMajor = 2,
      .languageVersionMinor = 4,
  };
  function_def_stream.write((const char *)&version_tag, sizeof(version_tag));

  function_def_stream.write("ENDT", 4);

  // public metadata
  uint32_t public_metadata_size = 4;
  public_metadata_stream.write((const char *)&public_metadata_size, 4);
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
  header.OSVersionMajor = 13;
  header.OSVersionMinor = 3;

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

extern "C" void ConvertDXBC(const void *pDXBC, uint32_t DXBCSize,
                            void **ppMetalLib, uint32_t *pMetalLibSize) {
  DxbcConverter converter;
  converter.Convert(pDXBC, DXBCSize, ppMetalLib, pMetalLibSize);
};

} // namespace dxmt