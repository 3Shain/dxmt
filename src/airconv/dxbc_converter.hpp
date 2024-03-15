
#pragma once

#include "dxbc_analyzer.hpp"

using PerformStore = std::function<void(llvm::Value *value)>;

namespace dxmt {

class IDxbcConverter {
public:
  virtual llvm::MDNode *Pre(air::AirMetadata &metadata) = 0;

  virtual llvm::MDNode *Post(air::AirMetadata &metadata,
                             llvm::MDNode *input) = 0;

  virtual void
  ConvertInstructions(D3D10ShaderBinary::CShaderCodeParser &Parser) = 0;
};

std::unique_ptr<IDxbcConverter> CreateConverterSM50(DxbcAnalyzer &analyzer,
                                                air::AirType &types,
                                                llvm::LLVMContext &context,
                                                llvm::Module &pModule);

class Scope {};
} // namespace dxmt