#pragma once
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Value.h"
#include <cstdint>
#include <functional>
#include <variant>
#include <vector>

#include "DXBCParser/DXBCUtils.h"
#include "DXBCParser/ShaderBinary.h" // a weird bug: this must be the last one
#include "DXBCParser/d3d12tokenizedprogramformat.hpp"

// #include "dxbc_utils.h"

#include "air_builder.hpp"
#include "air_constants.hpp"
#include "air_metadata.hpp"
#include "air_type.hpp"
#include "dxbc_signature.hpp"

namespace dxmt {

struct ConstantBuffer {
  uint32_t registerIndex;
  uint32_t size;
};

struct __ShaderContext {
  llvm::Function *function = 0;
  llvm::Type *tyBindingTable = 0;
  llvm::Value *bindingTable = 0;
  llvm::Type *tyInputRegs = 0;
  llvm::Value *inputRegs = 0;
  llvm::Type *tyOutputRegs = 0;
  llvm::Value *outputRegs = 0;
  llvm::Type *tyTempRegs = 0;
  llvm::Value *tempRegs = 0;

  /* SIV & SGV */
  std::optional<std::tuple<llvm::Value *, air::EDepthArgument>> oDepth;
  std::optional<llvm::Value *> oMask;
};

using PrologueHook = std::function<void(
    llvm::Value *input, const __ShaderContext &context, air::AirBuilder &builder)>;

struct InputBinding {
  llvm::Type *type;
  llvm::Metadata *indexedMetadata; // explicit index encoded inside (but why?)
  std::string name;
  PrologueHook prologueHook;
};

using EpilogueHook =
    std::function<llvm::Value *(const __ShaderContext &context, air::AirBuilder &)>;

struct OutputBinding {
  llvm::Type *type;
  llvm::Metadata *metadata;
  std::string name;
  EpilogueHook epilogueHook;
};

using SignatureMatcher = std::function<bool(dxbc::Signature)>;
using SignatureFinder = std::function<dxbc::Signature(const SignatureMatcher &)>;

class DxbcAnalyzer {
public:
  DxbcAnalyzer(llvm::LLVMContext &context, air::AirType &types,
               air::AirMetadata &metadata)
      : context(context), types(types), metadata(metadata) {}

  void AnalyzeShader(D3D10ShaderBinary::CShaderCodeParser &parser, const SignatureFinder& input, const SignatureFinder& output);

  // for SM 5.0 : assume no register space
  std::tuple<llvm::Type *, llvm::MDNode *>
  GenerateBindingTable();
  // std::tuple<llvm::Type*, llvm::MDNode*> GenerateBindingTableSM51(AirType&
  // type);

  bool IsSM51Plus() const {
    return m_DxbcMajor > 5 || (m_DxbcMajor == 5 && m_DxbcMinor >= 1);
  }

  bool IsVS() const { return ShaderType == D3D10_SB_VERTEX_SHADER; }
  bool IsPS() const { return ShaderType == D3D10_SB_PIXEL_SHADER; }
  bool IsCS() const { return ShaderType == D3D11_SB_COMPUTE_SHADER; }
  bool NoShaderOutput() const {
    if (IsCS()) {
      return true;
    }
    return outputs.size() == 0;
  }

  std::vector<InputBinding> inputs;
  std::vector<OutputBinding> outputs;
  std::vector<ConstantBuffer> constantBuffers;

  uint32_t maxOutputRegister = 0;
  uint32_t maxInputRegister = 0;
  uint32_t tempsCount = 0;
  std::map<uint32_t, uint32_t> indexableTemps;

private:
  llvm::LLVMContext &context;
  air::AirType &types;
  air::AirMetadata &metadata;
  unsigned m_DxbcMajor;
  unsigned m_DxbcMinor;
  D3D10_SB_TOKENIZED_PROGRAM_TYPE ShaderType;
};
} // namespace dxmt